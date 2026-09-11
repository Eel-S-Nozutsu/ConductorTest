// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#include "TideAttackExecution_EM0000.h"

#include "PRJ_TIDE_P0/Actors/Character/Enemy/EnemyCharacter.h"
#include "PRJ_TIDE_P0/Actors/Character/TideCharacter.h"
#include "PRJ_TIDE_P0/Actors/Projectile/EnemyProjectile.h"
#include "PRJ_TIDE_P0/Actors/Projectile/ProjectilePoolSubsystem.h"
#include "PRJ_TIDE_P0/Actors/Projectile/ProjectileProfile.h"
#include "PRJ_TIDE_P0/Actors/Projectile/Behaviors/BallisticBehavior.h"
#include "PRJ_TIDE_P0/Actors/Projectile/Behaviors/SplashDamageBehavior.h"
#include "PRJ_TIDE_P0/Core/TideGameSettings.h"
#include "PRJ_TIDE_P0/Utilities/TideCombatUtils.h"
#include "PRJ_TIDE_P0/Animation/Instances/Character/Enemy/EnemyAnimInstance.h"
#include "PRJ_TIDE_P0/Actors/Hazard/FlameStreamHazard.h"
#include "PRJ_TIDE_P0/AI/Director/AIDirector.h"
#include "PRJ_TIDE_P0/Components/AI/EnemyBattleComponent.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "AIController.h"
#include "Components/CapsuleComponent.h"
#include "Components/DecalComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Math/RotationMatrix.h"
#include "NavigationSystem.h"
#include "TimerManager.h"

// ------------------------------------------------------------
// UProjectileAttackExecution
// ------------------------------------------------------------

void UProjectileAttackExecution::OnAttackBegin(AEnemyCharacter* Enemy)
{
	if (!Enemy)
	{
		if (FinishDelegate) FinishDelegate(false);
		return;
	}

	CachedEnemy = Enemy;
	CachedAnimInstance = Enemy->GetMesh() ? Enemy->GetMesh()->GetAnimInstance() : nullptr;
	FireLoopElapsed = 0.0f;
	FireLoopDuration = 0.0f;

	UAnimInstance* AnimInst = CachedAnimInstance.Get();
	if (AnimInst)
	{
		// 開始→ループの繋ぎ + 撃ちきるまでのループ再トリガをブレンドアウトで拾う
		AnimInst->OnMontageBlendingOut.AddDynamic(this, &UProjectileAttackExecution::OnMontageBlendingOut);
	}

	// st再生開始と同時に弾を展開＋発射する(Startモーション中に隊形が出る)
	// 撃ちきり時間もこの時点から計測する(OnAttackTick参照)
	{
		const int32 Count = FMath::Max(1, ProjectileCount);
		const float Interval = FMath::Max(0.0f, FireInterval);
		const float FireTime = bLineUpBeforeFire
			? FMath::Max(0.0f, LineUpHoldDuration) + (Count - 1) * Interval
			: (Count - 1) * Interval;
		FireLoopDuration = FireTime + FMath::Max(0.0f, PostFireHold);
	}
	if (bLineUpBeforeFire) BeginLineUp(Enemy);
	else                   BeginFireBurst(Enemy);

	if (StartMontage && AnimInst)
	{
		Phase = EProjectilePhase::Starting;
		AnimInst->Montage_Play(StartMontage);
	}
	else
	{
		// 開始モンタージュ無し: 即ループへ(展開は上で済み)
		BeginLoop();
	}
}

void UProjectileAttackExecution::OnAttackTick(AEnemyCharacter* Enemy, float DeltaTime)
{
	// 発射はst開始から走っているためStarting/Loopingの両方で計測する
	// 撃ちきり(+PostFireHold)に達し、かつループ入り済みなら終了へ移る
	if (Phase != EProjectilePhase::Starting && Phase != EProjectilePhase::Looping) return;

	if (bLineUpSweeping) UpdateLineUpSweep(Enemy, DeltaTime);

	FireLoopElapsed += DeltaTime;
	if (FireLoopElapsed >= FireLoopDuration && Phase == EProjectilePhase::Looping)
	{
		BeginEnd();
	}
}

void UProjectileAttackExecution::OnAttackEnd(AEnemyCharacter* Enemy)
{
	if (Enemy)
	{
		Enemy->GetWorldTimerManager().ClearTimer(FireTimerHandle);
	}
	RemainingShots = 0;
	bLineUpSweeping = false;
	LineUpSweepEntries.Reset();

	// 中断・完了いずれでも呼ばれる。自分が再生したモンタージュを片付ける
	// (asyncモードは呼び出し側がモンタージュを止めないため、ここで停止する)
	if (UAnimInstance* AnimInst = CachedAnimInstance.Get())
	{
		AnimInst->OnMontageBlendingOut.RemoveDynamic(this, &UProjectileAttackExecution::OnMontageBlendingOut);
		AnimInst->OnMontageEnded.RemoveDynamic(this, &UProjectileAttackExecution::OnEndMontageEnded);
		if (StartMontage) AnimInst->Montage_Stop(0.15f, StartMontage);
		if (LoopMontage)  AnimInst->Montage_Stop(0.15f, LoopMontage);
		if (EndMontage)   AnimInst->Montage_Stop(0.15f, EndMontage);
	}

	Phase = EProjectilePhase::None;
}

void UProjectileAttackExecution::BeginLoop()
{
	Phase = EProjectilePhase::Looping;

	// 展開・発射・撃ちきり時間の計測はOnAttackBegin(st開始)で済ませている
	// ここではループモーションの再生のみ(撃ちきるまでブレンドアウトで再トリガしてループする)
	if (UAnimInstance* AnimInst = CachedAnimInstance.Get())
	{
		if (LoopMontage) AnimInst->Montage_Play(LoopMontage);
	}
}

void UProjectileAttackExecution::BeginEnd()
{
	Phase = EProjectilePhase::Ending;

	UAnimInstance* AnimInst = CachedAnimInstance.Get();
	if (EndMontage && AnimInst)
	{
		AnimInst->Montage_Play(EndMontage);
		AnimInst->OnMontageEnded.AddDynamic(this, &UProjectileAttackExecution::OnEndMontageEnded);
	}
	else
	{
		// 終了モンタージュ無し: 即完了
		Phase = EProjectilePhase::None;
		if (FinishDelegate) FinishDelegate(true);
	}
}

void UProjectileAttackExecution::OnMontageBlendingOut(UAnimMontage* Montage, bool bInterrupted)
{
	// 中断(別モンタージュ再生や被弾での上書き)は次へ繋がない
	if (bInterrupted) return;

	if (Phase == EProjectilePhase::Starting && Montage == StartMontage)
	{
		// 開始モーション終了 → 発射開始＋ループへ(ブレンドアウトとループのフェードインをクロス)
		BeginLoop();
	}
	else if (Phase == EProjectilePhase::Looping && Montage == LoopMontage)
	{
		// 撃ちきるまでシームレスにループ再生し直す
		if (UAnimInstance* AnimInst = CachedAnimInstance.Get())
		{
			AnimInst->Montage_Play(LoopMontage);
		}
	}
}

void UProjectileAttackExecution::OnEndMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	if (Montage != EndMontage) return;

	if (UAnimInstance* AnimInst = CachedAnimInstance.Get())
	{
		AnimInst->OnMontageEnded.RemoveDynamic(this, &UProjectileAttackExecution::OnEndMontageEnded);
	}

	if (Phase != EProjectilePhase::Ending) return;

	Phase = EProjectilePhase::None;
	if (FinishDelegate) FinishDelegate(!bInterrupted);
}

void UProjectileAttackExecution::BeginFireBurst(AEnemyCharacter* Enemy)
{
	if (!Enemy || !Profile) return;

	Enemy->GetWorldTimerManager().ClearTimer(FireTimerHandle);
	CachedEnemy = Enemy;
	RemainingShots = FMath::Max(1, ProjectileCount);

	// 1発目を即時発射し、残りをFireInterval間隔で継続する
	FireOne();
	if (RemainingShots <= 0) return;

	const float Interval = FMath::Max(0.001f, FireInterval);
	Enemy->GetWorldTimerManager().SetTimer(
		FireTimerHandle, this, &UProjectileAttackExecution::FireOne, Interval, true);
}

void UProjectileAttackExecution::FireOne()
{
	AEnemyCharacter* Enemy = CachedEnemy.Get();
	if (!Enemy || !Profile || RemainingShots <= 0)
	{
		if (Enemy) Enemy->GetWorldTimerManager().ClearTimer(FireTimerHandle);
		RemainingShots = 0;
		return;
	}

	USkeletalMeshComponent* MeshComp = Enemy->GetMesh();
	if (!MeshComp) return;

	const FVector Location = MeshComp->GetSocketTransform(MuzzleSocketName, RTS_World).GetLocation();
	SpawnProjectileAt(Enemy, Location, 0.0f);

	--RemainingShots;
	if (RemainingShots <= 0)
	{
		Enemy->GetWorldTimerManager().ClearTimer(FireTimerHandle);
	}
}

FRotator UProjectileAttackExecution::ComputeAimRotation(AEnemyCharacter* Enemy, const FVector& FromLocation, bool& bOutAimAtPlayer) const
{
	bOutAimAtPlayer = true; // 拡散抽選の結果(デバッグ表示の色分けにも使う)。ホーミング時は常にtrue扱い

	FRotator Rotation(0.0f, Enemy->GetActorRotation().Yaw, 0.0f);
	ACharacter* Player = Cast<ACharacter>(UGameplayStatics::GetPlayerPawn(Enemy, 0));
	if (!Player)
	{
		return Rotation;
	}

	const FVector AimPoint = Player->GetActorLocation()
		+ FVector(0.0f, 0.0f, Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());

	const FVector ToTarget = AimPoint - FromLocation;
	const float HorizontalDist = FVector2D(ToTarget.X, ToTarget.Y).Size();
	if (HorizontalDist > KINDA_SMALL_NUMBER)
	{
		Rotation.Pitch = FMath::RadiansToDegrees(FMath::Atan2(ToTarget.Z, HorizontalDist));

		const float ToTargetYaw = FMath::RadiansToDegrees(FMath::Atan2(ToTarget.Y, ToTarget.X));
		const float YawDiff = FMath::Abs(FRotator::NormalizeAxis(ToTargetYaw - Rotation.Yaw));
		if (YawDiff <= ProjectileYawCorrectionAngle)
		{
			Rotation.Yaw = ToTargetYaw;
		}
	}

	if (bUseSpread)
	{
		// 弾ごとに「PCを狙う弾」かどうかを抽選する。外れた弾は狙い方向へ
		// 上下左右ランダムを加えて散らす(棒立ち対策で一定割合は素直に狙う)
		bOutAimAtPlayer = FMath::FRand() < AimAtPlayerChance;
		if (!bOutAimAtPlayer)
		{
			Rotation.Yaw += FMath::FRandRange(-SpreadYawRangeDeg, SpreadYawRangeDeg);
			Rotation.Pitch += FMath::FRandRange(-SpreadPitchRangeDeg, SpreadPitchRangeDeg);
		}
	}

#if !UE_BUILD_SHIPPING
	// 狙う基準点(迎撃点)の球と、実際の発射方向の線。狙い弾=緑 / 散らし弾=橙
	if (bDebugDrawAim)
	{
		const float Duration = 2.0f;
		const float DrawDist = FMath::Max(ToTarget.Size(), 100.0f);
		const FVector ShotEnd = FromLocation + Rotation.Vector() * DrawDist;
		DrawDebugSphere(Enemy->GetWorld(), AimPoint, 40.0f, 12, FColor::Red, false, Duration);
		DrawDebugLine(Enemy->GetWorld(), FromLocation, ShotEnd,
			bOutAimAtPlayer ? FColor::Green : FColor::Orange, false, Duration, 0, 2.0f);
	}
#endif

	return Rotation;
}

AEnemyProjectile* UProjectileAttackExecution::SpawnProjectileAt(AEnemyCharacter* Enemy, const FVector& Location, float HoldDuration, const FRotator* OverrideRotation)
{
	if (!Enemy || !Profile) return nullptr;

	UWorld* World = Enemy->GetWorld();
	if (!World) return nullptr;

	UProjectilePoolSubsystem* Pool = World->GetSubsystem<UProjectilePoolSubsystem>();
	if (!Pool) return nullptr;

	// OverrideRotationがあればそれで撃つ(横一列の平行発射)。無ければPC狙いの照準計算
	FRotator Rotation;
	if (OverrideRotation)
	{
		Rotation = *OverrideRotation;
	}
	else
	{
		bool bAimAtPlayer = true;
		Rotation = ComputeAimRotation(Enemy, Location, bAimAtPlayer);
	}
	const FTransform SpawnTransform(Rotation, Location);

	// Acquireがプロファイルを適用(パラメータ + 飛び方/効果ビヘイビアの複製)して返す
	// 追尾するかはプロファイルのMovementビヘイビアが決める
	AEnemyProjectile* Projectile = Pool->Acquire(Profile, SpawnTransform, Enemy, Enemy->GetInstigator());
	if (!Projectile) return nullptr;

	// 実行中攻撃のダメージで上書きする(負値ならプロファイル既定を尊重)
	// InitFromProfileが毎回プロファイル値へ戻すため、プール再利用でも前回値は残らない
	TideCombatUtil::InjectAttackDamage(Enemy, Projectile->Damage);
	Projectile->SpawnHoldDuration = FMath::Max(0.0f, HoldDuration);

	Projectile->ActivateProjectile();

	return Projectile;
}

void UProjectileAttackExecution::BeginLineUp(AEnemyCharacter* Enemy)
{
	if (!Enemy || !Profile) return;

	bLineUpSweeping = false;
	LineUpSweepEntries.Reset();

	const int32 Count = FMath::Max(1, ProjectileCount);
	const bool bSweep = bLineUpParallel && bLineUpSweepToTarget;

	const ACharacter* Player = Cast<ACharacter>(UGameplayStatics::GetPlayerPawn(Enemy, 0));

	// 隊列の基準向き: 敵の現在の向きではなく、敵→PCの水平方向を基準にする(PC不在時は敵の正面)
	// スイープ有効時もこれを初期値にし、以降はPCが動いた場合のみ追従修正する
	// (敵の現在の向きからの大きな振り向き演出はしない)
	FVector BaseDir = Enemy->GetActorForwardVector();
	if (Player)
	{
		FVector Delta = Player->GetActorLocation() - Enemy->GetActorLocation();
		Delta.Z = 0.0f;
		if (!Delta.IsNearlyZero())
		{
			BaseDir = Delta.GetSafeNormal();
		}
	}
	const FRotator BaseYawRot(0.0f, FMath::RadiansToDegrees(FMath::Atan2(BaseDir.Y, BaseDir.X)), 0.0f);
	const FVector Forward = BaseYawRot.Vector();
	const FVector Right = FRotationMatrix(BaseYawRot).GetUnitAxis(EAxis::Y);

	const FVector Base = Enemy->GetActorLocation()
		+ Forward * LineUpForwardOffset
		+ FVector(0.0f, 0.0f, LineUpHeightOffset);
	const float BaseHold = FMath::Max(0.0f, LineUpHoldDuration);
	const float Interval = FMath::Max(0.0f, FireInterval);
	const float CenterIndex = (Count - 1) * 0.5f;

	// 平行発射: 全弾を同一方向で撃つ(各弾がPCへ収束せず横幅を保ったまま直進＝弾幕の壁)
	FRotator ParallelRot = BaseYawRot;
	if (bLineUpParallel && Player)
	{
		const FVector AimPoint = Player->GetActorLocation()
			+ FVector(0.0f, 0.0f, Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
		const FVector ToTarget = AimPoint - Base;
		const float Horiz = FVector2D(ToTarget.X, ToTarget.Y).Size();
		if (Horiz > KINDA_SMALL_NUMBER)
		{
			// 照準は生成時(st開始時)のPC位置で1回だけ決まり、
			// 弾はLineUpHoldDuration後に飛ぶ
			// ピッチを向けるとその瞬間のPCの高さ(ジャンプ中なら跳躍頂点)が全弾に焼き付くため、
			// 水平発射では0のまま据え置き、生成高さを保って直進させる
			if (!bLineUpLevelPitch)
			{
				ParallelRot.Pitch = FMath::RadiansToDegrees(FMath::Atan2(ToTarget.Z, Horiz));
			}
			ParallelRot.Yaw = FMath::RadiansToDegrees(FMath::Atan2(ToTarget.Y, ToTarget.X));
		}
	}

	// 中心(Base)を挟んで左右対称・等間隔の一直線に並べる
	// (例: Count=4なら-1.5,-0.5,+0.5,+1.5段)
	// 全弾を並べて待機させたうえで、中心に近い弾から外側へFireInterval間隔で順次発射する
	// (左右対称ペアは同時発射。中心からの距離ランクをホールド時間へ反映する)
	for (int32 i = 0; i < Count; ++i)
	{
		const float IndexOffset = i - CenterIndex;
		const FVector Loc = Base + Right * (IndexOffset * LineUpSpacing);

		const int32 Rank = FMath::FloorToInt(FMath::Abs(IndexOffset));
		const float Hold = BaseHold + Rank * Interval;

		AEnemyProjectile* Projectile = SpawnProjectileAt(Enemy, Loc, Hold, bLineUpParallel ? &ParallelRot : nullptr);

		if (bSweep && Projectile)
		{
			LineUpSweepEntries.Add(FLineUpSweepEntry{ Projectile, IndexOffset });
		}
	}

	if (bSweep && !LineUpSweepEntries.IsEmpty())
	{
		bLineUpSweeping = true;
		LineUpSweepElapsed = 0.0f;
		// 初期値はParallelRot(=生成時点のPC方向)と一致させ、位置とのズレ(瞬間移動)を防ぐ
		LineUpSweepCurrentYaw = ParallelRot.Yaw;
		LineUpSweepCurrentPitch = ParallelRot.Pitch;
	}
}

void UProjectileAttackExecution::UpdateLineUpSweep(AEnemyCharacter* Enemy, float DeltaTime)
{
	if (!bLineUpSweeping || !Enemy) return;

	LineUpSweepElapsed += DeltaTime;

	// 毎フレームPCの現在位置へ向く目標Yaw/Pitchを再計算する(狙いは追従し続ける)
	float TargetYaw = LineUpSweepCurrentYaw;
	float TargetPitch = 0.0f;
	const FVector PivotLocation = Enemy->GetActorLocation() + FVector(0.0f, 0.0f, LineUpHeightOffset);
	if (const ACharacter* Player = Cast<ACharacter>(UGameplayStatics::GetPlayerPawn(Enemy, 0)))
	{
		const FVector AimPoint = Player->GetActorLocation()
			+ FVector(0.0f, 0.0f, Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
		const FVector ToTarget = AimPoint - PivotLocation;
		const float Horiz = FVector2D(ToTarget.X, ToTarget.Y).Size();
		if (Horiz > KINDA_SMALL_NUMBER)
		{
			TargetYaw = FMath::RadiansToDegrees(FMath::Atan2(ToTarget.Y, ToTarget.X));
			if (!bLineUpLevelPitch)
			{
				TargetPitch = FMath::RadiansToDegrees(FMath::Atan2(ToTarget.Z, Horiz));
			}
		}
	}

	// 現在角度を目標角度へ、最大回転速度でクランプしながら近づける(急な回転を防ぐ)
	const float MaxDelta = FMath::Max(0.0f, MaxSweepTurnRateDegPerSec) * DeltaTime;
	LineUpSweepCurrentYaw = FMath::FixedTurn(LineUpSweepCurrentYaw, TargetYaw, MaxDelta);
	LineUpSweepCurrentPitch = FMath::FixedTurn(LineUpSweepCurrentPitch, TargetPitch, MaxDelta);

	const FRotator CurrentRot(LineUpSweepCurrentPitch, LineUpSweepCurrentYaw, 0.0f);

	const FRotator YawRot(0.0f, LineUpSweepCurrentYaw, 0.0f);
	const FVector CurrentRight = FRotationMatrix(YawRot).GetUnitAxis(EAxis::Y);
	const FVector CurrentBase = Enemy->GetActorLocation()
		+ YawRot.Vector() * LineUpForwardOffset
		+ FVector(0.0f, 0.0f, LineUpHeightOffset);

	// 直線の隊列形状(IndexOffset)を保ったまま、
	// 敵を中心に位置・向きともにPCへ向けて回転させる
	for (const FLineUpSweepEntry& Entry : LineUpSweepEntries)
	{
		if (AEnemyProjectile* Projectile = Entry.Projectile.Get())
		{
			const FVector Loc = CurrentBase + CurrentRight * (Entry.IndexOffset * LineUpSpacing);
			Projectile->SetActorLocationAndRotation(Loc, CurrentRot);
		}
	}

	if (LineUpSweepElapsed >= FMath::Max(0.0001f, LineUpHoldDuration))
	{
		bLineUpSweeping = false;
		LineUpSweepEntries.Reset();
	}
}

// ------------------------------------------------------------
// UFanBombardAttackExecution
// ------------------------------------------------------------

void UFanBombardAttackExecution::OnAttackBegin(AEnemyCharacter* Enemy)
{
	if (!Enemy)
	{
		if (FinishDelegate) FinishDelegate(false);
		return;
	}

	CachedEnemy = Enemy;
	CachedAnimInstance = Enemy->GetMesh() ? Enemy->GetMesh()->GetAnimInstance() : nullptr;
	FireLoopElapsed = 0.0f;
	FireLoopDuration = 0.0f;

	UAnimInstance* AnimInst = CachedAnimInstance.Get();
	if (AnimInst)
	{
		// 開始→ループの繋ぎ + 撃ちきるまでのループ再トリガをブレンドアウトで拾う
		AnimInst->OnMontageBlendingOut.AddDynamic(this, &UFanBombardAttackExecution::OnMontageBlendingOut);
	}

	// st再生開始と同時に全弾を展開＋段発射する(Startモーション中に隊形が出る)
	// 撃ちきり時間もこの時点から計測する(OnAttackTick参照)
	{
		const int32 Rows = FMath::Max(1, RowCount);
		const float BaseHold = FMath::Max(0.0f, SpawnHoldDuration);
		const float Interval = FMath::Max(0.0f, RowInterval);
		FireLoopDuration = BaseHold + (Rows - 1) * Interval + FMath::Max(0.0f, PostFireHold);
	}
	BeginVolley(Enemy);

	if (StartMontage && AnimInst)
	{
		Phase = EBombardPhase::Starting;
		AnimInst->Montage_Play(StartMontage);
	}
	else
	{
		// 開始モンタージュ無し: 即ループへ(展開は上で済み)
		BeginLoop();
	}
}

void UFanBombardAttackExecution::OnAttackTick(AEnemyCharacter* Enemy, float DeltaTime)
{
	// 発射はst開始から走っているためStarting/Loopingの両方で計測する
	// 撃ちきり(+PostFireHold)に達し、かつループ入り済みなら終了へ移る
	if (Phase != EBombardPhase::Starting && Phase != EBombardPhase::Looping) return;

	FireLoopElapsed += DeltaTime;
	if (FireLoopElapsed >= FireLoopDuration && Phase == EBombardPhase::Looping)
	{
		BeginEnd();
	}
}

void UFanBombardAttackExecution::OnAttackEnd(AEnemyCharacter* Enemy)
{
	// 中断・完了いずれでも呼ばれる。自分が再生したモンタージュを片付ける
	// (asyncモードは呼び出し側がモンタージュを止めないため、ここで停止する)
	if (UAnimInstance* AnimInst = CachedAnimInstance.Get())
	{
		AnimInst->OnMontageBlendingOut.RemoveDynamic(this, &UFanBombardAttackExecution::OnMontageBlendingOut);
		AnimInst->OnMontageEnded.RemoveDynamic(this, &UFanBombardAttackExecution::OnEndMontageEnded);
		if (StartMontage) AnimInst->Montage_Stop(0.15f, StartMontage);
		if (LoopMontage)  AnimInst->Montage_Stop(0.15f, LoopMontage);
		if (EndMontage)   AnimInst->Montage_Stop(0.15f, EndMontage);
	}

	Phase = EBombardPhase::None;
}

void UFanBombardAttackExecution::BeginLoop()
{
	Phase = EBombardPhase::Looping;

	// 展開・段発射・撃ちきり時間の計測はOnAttackBegin(st開始)で済ませている
	// ここではループモーションの再生のみ(撃ちきるまでブレンドアウトで再トリガしてループする)
	if (UAnimInstance* AnimInst = CachedAnimInstance.Get())
	{
		if (LoopMontage) AnimInst->Montage_Play(LoopMontage);
	}
}

void UFanBombardAttackExecution::BeginEnd()
{
	Phase = EBombardPhase::Ending;

	UAnimInstance* AnimInst = CachedAnimInstance.Get();
	if (EndMontage && AnimInst)
	{
		AnimInst->Montage_Play(EndMontage);
		AnimInst->OnMontageEnded.AddDynamic(this, &UFanBombardAttackExecution::OnEndMontageEnded);
	}
	else
	{
		// 終了モンタージュ無し: 即完了
		Phase = EBombardPhase::None;
		if (FinishDelegate) FinishDelegate(true);
	}
}

void UFanBombardAttackExecution::OnMontageBlendingOut(UAnimMontage* Montage, bool bInterrupted)
{
	// 中断(別モンタージュ再生や被弾での上書き)は次へ繋がない
	if (bInterrupted) return;

	if (Phase == EBombardPhase::Starting && Montage == StartMontage)
	{
		// 開始モーション終了 → 展開＋ループへ(ブレンドアウトとループのフェードインをクロス)
		BeginLoop();
	}
	else if (Phase == EBombardPhase::Looping && Montage == LoopMontage)
	{
		// 撃ちきるまでシームレスにループ再生し直す
		if (UAnimInstance* AnimInst = CachedAnimInstance.Get())
		{
			AnimInst->Montage_Play(LoopMontage);
		}
	}
}

void UFanBombardAttackExecution::OnEndMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	if (Montage != EndMontage) return;

	if (UAnimInstance* AnimInst = CachedAnimInstance.Get())
	{
		AnimInst->OnMontageEnded.RemoveDynamic(this, &UFanBombardAttackExecution::OnEndMontageEnded);
	}

	if (Phase != EBombardPhase::Ending) return;

	Phase = EBombardPhase::None;
	if (FinishDelegate) FinishDelegate(!bInterrupted);
}

void UFanBombardAttackExecution::BeginVolley(AEnemyCharacter* Enemy)
{
	if (!Enemy || !BombardProfile) return;

	// 扇の中心 = EM→PCの水平方向。ターゲットが無ければEMの正面
	const FVector EnemyLoc = Enemy->GetActorLocation();
	const AActor* Target = TideCombatUtil::GetTarget(Enemy);
	FVector Forward2D = Target
		? (Target->GetActorLocation() - EnemyLoc).GetSafeNormal2D()
		: Enemy->GetActorForwardVector().GetSafeNormal2D();
	if (Forward2D.IsNearlyZero())
	{
		Forward2D = Enemy->GetActorForwardVector().GetSafeNormal2D();
	}
	const FVector Up(0.0f, 0.0f, 1.0f);
	const FVector Right2D = FVector::CrossProduct(Up, Forward2D).GetSafeNormal();

	// 着弾の高さ基準。PCの足元 (取得できなければEMの足元) に合わせる
	float GroundZ = EnemyLoc.Z;
	if (Enemy->GetCapsuleComponent())
	{
		GroundZ -= Enemy->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	}
	if (Target)
	{
		GroundZ = Target->GetActorLocation().Z;
	}
	const FVector Center = FVector(EnemyLoc.X, EnemyLoc.Y, GroundZ);

	const int32 Rows = FMath::Max(1, RowCount);
	const int32 Cols = FMath::Max(1, ColumnCount);
	const float NearR = FMath::Min(NearRadius, FarRadius);
	const float FarR = FMath::Max(NearRadius, FarRadius);
	const int32 Total = Rows * Cols;

	// 着弾XYの真下にある実際の床へ着弾Zを合わせる
	// Center.ZはPCカプセル中心(地面より高い)なので、そのまま使うと着弾点が空中になり、
	// そこで爆発＝空中爆発になる。床トレースで地面Zを取る
	UWorld* World = Enemy->GetWorld();
	auto ProjectToGroundZ = [&](FVector& Pos)
		{
			FHitResult FloorHit;
			FCollisionQueryParams FloorParams;
			FloorParams.AddIgnoredActor(Enemy);
			const FVector TraceFrom = FVector(Pos.X, Pos.Y, Center.Z + 300.0f);
			const FVector TraceTo = FVector(Pos.X, Pos.Y, Center.Z - 3000.0f);
			if (World->LineTraceSingleByChannel(FloorHit, TraceFrom, TraceTo, ECC_WorldStatic, FloorParams))
			{
				Pos.Z = FloorHit.ImpactPoint.Z;
			}
			else
			{
				Pos.Z = Center.Z;
			}
		};

	// 左右2円(前方を向く縦ディスク)の生成基準
	// 面内軸 = 水平右(Right2D) と 鉛直上(Up)
	const FVector CircleBase = EnemyLoc + Forward2D * SpawnCircleForwardOffset + Up * SpawnHeightOffset;
	const int32   LeftTotal = (Total + 1) / 2;
	const int32   RightTotal = Total - LeftTotal;
	const float   GoldenAngle = PI * (3.0f - FMath::Sqrt(5.0f)); // 約137.5度(ひまわり螺旋)

	const float BaseHold = FMath::Max(0.0f, SpawnHoldDuration);
	const float Interval = FMath::Max(0.0f, RowInterval);

	// 奥(row=Rows-1) → 手前(row=0) の順に発射されるよう、
	// 奥の段ほど発射遅延を小さくする。全弾を左右2円へ螺旋配置で一斉生成し、
	// 各弾の待機時間に段の発射遅延を仕込む
	int32 k = 0;
	for (int32 row = Rows - 1; row >= 0; --row)
	{
		const float RadiusT = (Rows > 1) ? static_cast<float>(row) / static_cast<float>(Rows - 1) : 1.0f;
		const float LandRadius = FMath::Lerp(NearR, FarR, RadiusT);
		const int32 RowOrder = (Rows - 1) - row; // 奥 = 0が最初に発射
		const float HoldDur = BaseHold + RowOrder * Interval;

		for (int32 c = 0; c < Cols; ++c)
		{
			const float ColT = (Cols > 1) ? (static_cast<float>(c) / static_cast<float>(Cols - 1) - 0.5f) : 0.0f;
			const float AngleDeg = ColT * FanAngleDeg;
			const FVector Dir = Forward2D.RotateAngleAxis(AngleDeg, FVector::UpVector);
			FVector Landing = Center + Dir * LandRadius;
			ProjectToGroundZ(Landing);

			// 生成位置: 左右どちらかの円に黄金角の螺旋(なると)で配置する
			const bool  bLeft = (k % 2 == 0);
			const int32 Local = k / 2;
			const int32 CTotal = FMath::Max(1, bLeft ? LeftTotal : RightTotal);
			const float Rr = SpawnCircleRadius * FMath::Sqrt((static_cast<float>(Local) + 0.5f) / static_cast<float>(CTotal));
			const float Theta = static_cast<float>(Local) * GoldenAngle;
			const FVector Disk = Right2D * (Rr * FMath::Cos(Theta)) + Up * (Rr * FMath::Sin(Theta));
			const FVector CCenter = CircleBase + Right2D * (bLeft ? -SpawnCircleSideSeparation : SpawnCircleSideSeparation);
			const FVector SpawnPos = CCenter + Disk;

			SpawnBombardHeld(Enemy, SpawnPos, Landing, HoldDur);
			++k;
		}
	}
}

void UFanBombardAttackExecution::SpawnBombardHeld(AEnemyCharacter* Enemy, const FVector& SpawnPos, const FVector& Landing, float HoldDuration)
{
	if (!Enemy || !BombardProfile) return;

	UProjectilePoolSubsystem* Pool = Enemy->GetWorld()
		? Enemy->GetWorld()->GetSubsystem<UProjectilePoolSubsystem>() : nullptr;
	if (!Pool) return;

	const FRotator SpawnRot = Enemy->GetActorRotation();
	AEnemyProjectile* Proj = Pool->Acquire(
		BombardProfile, FTransform(SpawnRot, SpawnPos), Enemy, Enemy->GetInstigator());
	if (!Proj) return;

	// 溜め (その場待機) は弾フレームワークのSpawnHoldDurationに任せる
	// LaunchSequenceがOnSpawnでVFX表示→SpawnHoldDuration待機
	// →OnLaunch(弾道発射) を担う
	Proj->SpawnHoldDuration = HoldDuration;

	UBallisticBehavior* Bombard = Proj->FindBehavior<UBallisticBehavior>();
	USplashDamageBehavior* Splash = Proj->FindBehavior<USplashDamageBehavior>();

	TideCombatUtil::InjectAttackDamage(Enemy, Proj->Damage);
	if (Splash) TideCombatUtil::InjectAttackDamage(Enemy, Splash->SplashDamage);
	// 着弾点だけ渡す。山なりの高さはFlightDuration /
	// GravityScaleから物理的に創発する
	if (Bombard) Bombard->SetTargetLocation(Landing);

	Proj->ActivateProjectile();

#if !UE_BUILD_SHIPPING
	if (UTideGameSettings::Get()->bDebugDrawBombardLanding)
	{
		DrawDebugSphere(Enemy->GetWorld(), Landing, 50.0f, 12, FColor::Cyan, false, 3.0f);
	}
#endif
}

// ------------------------------------------------------------
// UFlamethrowerAttackExecution
// ------------------------------------------------------------

namespace
{
	// 照準がAimBlendMaxYawを超えてから、追うのをやめて正面へ戻しきるまでの角度(度)
	// 急に切ると火炎が飛ぶので、この幅でなだらかに0へ落とす
	constexpr float AimFalloffRangeDeg = 45.0f;
}

int32 UFlamethrowerAttackExecution::GetRequiredCoopCount() const
{
	switch (FormationType)
	{
	case EFlameFormationType::Trio: return 2;
	case EFlameFormationType::Duo:  return 1;
	default:                        return 0;
	}
}

TArray<float> UFlamethrowerAttackExecution::GetSlotAngles() const
{
	switch (FormationType)
	{
	// 3体: リーダーを中央にして左右対称に開く
	case EFlameFormationType::Trio: return { -TrioSpreadAngleDeg, TrioSpreadAngleDeg };
	// 2体: 真後ろ。背中合わせでPCの逆側を塞ぐ
	case EFlameFormationType::Duo:  return { 180.0f };
	default:                        return {};
	}
}

bool UFlamethrowerAttackExecution::CanActivate(const AEnemyCharacter* Enemy) const
{
	// 隊形を組まない行は常に発動可
	const int32 NeedCoop = GetRequiredCoopCount();
	if (NeedCoop <= 0) return true;

	// 人数がそろわない隊形は抽選の時点で落とす。ここで弾いておけば、人数違いの行が
	// 「多い順」に自然と選ばれる (3体版が落ちれば2体版、それも落ちれば単体版が残る)
	return GatherCandidates(Enemy).Num() >= NeedCoop;
}

TArray<AEnemyCharacter*> UFlamethrowerAttackExecution::GatherCandidates(const AEnemyCharacter* Leader) const
{
	TArray<AEnemyCharacter*> Candidates;

	UWorld* World = Leader ? Leader->GetWorld() : nullptr;
	UAIDirector* Director = World ? World->GetSubsystem<UAIDirector>() : nullptr;
	if (!Director) return Candidates;

	// 同種・戦闘中・未実行・未予約の候補をリーダー近傍で集める
	// 同種判定はCharacterData(DataAsset) のポインタ一致で行う
	// 戦闘参加はAIDirectorの明示登録 (ターゲット保持) を見るので、
	// 相手がストレイフ中か直近に攻撃抽選を回したかには左右されない
	const UTideCharacterDataAsset* MyData = Leader->GetCharacterData();
	const FVector LeaderLoc = Leader->GetActorLocation();
	const float RadiusSq = RecruitRadius * RecruitRadius;

	for (AEnemyCharacter* E : Director->GetCombatants(Leader))
	{
		if (!E) continue;
		if (E->GetCharacterData() != MyData) continue;             // 同種のみ
		if (E->IsExecutingAttack()) continue;                      // 既に何かの攻撃中
		if (Director->IsCoordinationReserved(E)) continue;         // 他リーダーが予約済み
		if (FVector::DistSquared(E->GetActorLocation(), LeaderLoc) > RadiusSq) continue;
		Candidates.Add(E);
	}
	return Candidates;
}

void UFlamethrowerAttackExecution::OnAttackBegin(AEnemyCharacter* Enemy)
{
	if (!Enemy)
	{
		if (FinishDelegate) FinishDelegate(false);
		return;
	}

	CachedEnemy = Enemy;
	CachedAnimInstance = Enemy->GetMesh() ? Enemy->GetMesh()->GetAnimInstance() : nullptr;
	LoopElapsed = 0.0f;
	CurrentAimBlend = 0.0f;
	bIsCoordinator = false;
	bFormationActive = false;
	StandbyJoinElapsed = 0.0f;
	StandbyElapsed = 0.0f;
	bAllCoordinatorsJoined = false;
	FireDelayRemaining = -1.0f;
	bFireOrderAssigned = false;
	RecoveryElapsed = 0.0f;
	RecruitedCoops.Reset();
	RecruitedSlotAngles.Reset();

	// 自分がリーダーに指名(予約)されている協力者なら、まずスロットへ飛び込む
	// 着地したらスタンバイに入り、リーダーが配る発射順を待つ
	UAIDirector* Director = Enemy->GetWorld() ? Enemy->GetWorld()->GetSubsystem<UAIDirector>() : nullptr;
	FVector Slot;
	if (Director && Director->GetCoordinationSlot(Enemy, Slot))
	{
		bIsCoordinator = true;
		// 着地後の向きはリーダーの采配で決まる。CMCにPCの方へ引き戻させない
		LockFacing(Enemy);
		BeginJumpToSlot(Enemy, Slot);
		return;
	}

	// リーダー: 必要人数がそろえば隊形、そろわなければ単体噴射へ落ちる
	// 抽選側 (CanActivate) で人数は確認済みなので、
	// ここで落ちるのは直前に横取りされた場合だけ
	if (GetRequiredCoopCount() > 0)
	{
		bFormationActive = TryFormFormation(Enemy);
	}

	if (bFormationActive)
	{
		// 隊形の間はリーダーも向きを固定する。扇状の配置が個々にPCを向いて崩れないように
		LockFacing(Enemy);

		// 協力者の着地を待つ間、リーダーはPCを向いて構える
		if (const AActor* Target = TideCombatUtil::GetTarget(Enemy))
		{
			const FVector Forward = (Target->GetActorLocation() - Enemy->GetActorLocation()).GetSafeNormal2D();
			if (!Forward.IsNearlyZero())
			{
				Enemy->SetActorRotation(FRotator(0.0f, Forward.Rotation().Yaw, 0.0f));
			}
		}
		Phase = EFlamePhase::Standby;
		return;
	}

	BeginFlameSequence(Enemy);
}

void UFlamethrowerAttackExecution::BeginFlameSequence(AEnemyCharacter* Enemy)
{
	if (!Enemy) return;

	// ループ噴射の有効化はBeginLoop()(開始モンタージュ終了時)まで遅延する
	// 開始と同時に有効化すると、開始モンタージュのブレンドイン/アウト中に下地のループ
	// BlendSpaceが透けて開始モーションの途中でループへ遷移して見えてしまうため
	if (UEnemyAnimInstance* Anim = GetEnemyAnimInstance())
	{
		Anim->bFlamethrowerActive = false;
		Anim->FlamethrowerAimBlend = 0.0f;
		// 攻撃全体で視線IKを抑制する(頭部追従が本体の照準ブレンドと競合するため)
		Anim->bSuppressLookAtIK = true;
	}

	// 火炎ハザードを先に生成してソケットへアタッチしておく(判定はループ開始まで非アクティブ)
	// FlameHazardClass未設定ならSpawnFlame自体が何もしない
	// (モーションのみの役に使える)
	SpawnFlame(Enemy);

	UAnimInstance* AnimInst = CachedAnimInstance.Get();
	if (StartMontage && AnimInst)
	{
		Phase = EFlamePhase::Starting;
		AnimInst->Montage_Play(StartMontage);
		// ブレンドアウト開始でループへ切り替える(OnMontageEndedだと完全終了まで待って
		// 開始のブレンドアウト中に下地アイドルが透けて一瞬棒立ちになるため)
		AnimInst->OnMontageBlendingOut.AddDynamic(this, &UFlamethrowerAttackExecution::OnStartMontageBlendingOut);
	}
	else
	{
		// 開始モンタージュ無し: 即ループへ
		BeginLoop();
	}
}

void UFlamethrowerAttackExecution::OnAttackTick(AEnemyCharacter* Enemy, float DeltaTime)
{
	if (!Enemy) return;

	// 飛び込み中(協力者)は放物線補間を進める。到達でスタンバイへ移る
	if (Phase == EFlamePhase::Jumping)
	{
		TickJump(Enemy, DeltaTime);
		return;
	}

	// スタンバイ: リーダーは全員の着地を待って発射順を配り、協力者は自分の番を待つ
	if (Phase == EFlamePhase::Standby)
	{
		if (bIsCoordinator) TickStandbyAsCoordinator(Enemy, DeltaTime);
		else                TickStandbyAsLeader(Enemy, DeltaTime);
		return;
	}

	// 未開始は照準/計測をしない
	if (Phase == EFlamePhase::None) return;

	// 後隙: モーションは終わっているが、この間はまだ攻撃を完了させず隙として残す
	if (Phase == EFlamePhase::Recovering)
	{
		RecoveryElapsed += DeltaTime;
		if (RecoveryElapsed >= RecoveryHold)
		{
			Phase = EFlamePhase::None;
			if (FinishDelegate) FinishDelegate(true);
		}
		return;
	}

	// 開始/ループ中は毎フレームPC位置から照準ブレンドを更新する
	UpdateAimBlend(Enemy, DeltaTime);

	if (Phase == EFlamePhase::Looping)
	{
		LoopElapsed += DeltaTime;

		// 予兆: 発生源から徐々に到達距離を伸ばす。判定/デバッグ描画ともRangeを毎tick参照するため
		// ここを書き換えるだけで当たりも一緒に伸びる(見た目は伸びきった長さのまま)
		if (ActiveFlame && FlameGrowDuration > 0.0f)
		{
			const float GrowAlpha = FMath::Clamp(LoopElapsed / FlameGrowDuration, 0.0f, 1.0f);
			ActiveFlame->SetRange(
				FMath::Lerp(FMath::Min(FlameStartRange, FlameFullRange), FlameFullRange, GrowAlpha));
		}

		if (LoopElapsed >= LoopDuration)
		{
			BeginEnd();
		}
	}
}

void UFlamethrowerAttackExecution::OnAttackEnd(AEnemyCharacter* Enemy)
{
	// 中断・完了いずれでも呼ばれる。自分が再生したモンタージュを片付ける
	// (asyncモードは呼び出し側がモンタージュを止めないため、ここで停止する)
	if (UAnimInstance* AnimInst = CachedAnimInstance.Get())
	{
		AnimInst->OnMontageBlendingOut.RemoveDynamic(this, &UFlamethrowerAttackExecution::OnStartMontageBlendingOut);
		AnimInst->OnMontageEnded.RemoveDynamic(this, &UFlamethrowerAttackExecution::OnEndMontageEnded);
		if (StartMontage) AnimInst->Montage_Stop(0.15f, StartMontage);
		if (EndMontage)   AnimInst->Montage_Stop(0.15f, EndMontage);
		if (JumpMontage)  AnimInst->Montage_Stop(0.15f, JumpMontage);
	}

	// 飛び込みで変更した移動モード/スーパーアーマーを戻す (飛び込み中に中断された場合の保険)
	EndJump(Enemy);

	// CMCの向き上書きを戻す。中断経路でも必ず通す (戻し漏れると以後ずっとPCを向かなくなる)
	UnlockFacing(Enemy);

	// 予約の解放。中断・完了いずれでも確実に外す (放置すると再募集の対象外になる)
	// 協力者は自分の分、リーダーは指名した全員分を外す (リーダーが先に中断されても
	// 協力者が誰も来ないスタンバイで固まらないように)
	if (Enemy)
	{
		if (UAIDirector* Director = Enemy->GetWorld() ? Enemy->GetWorld()->GetSubsystem<UAIDirector>() : nullptr)
		{
			if (bIsCoordinator)
			{
				Director->ReleaseCoordinator(Enemy);
			}
			// 発射順を配る前に中断された場合だけ協力者を解放する。配った後に解放すると、
			// まだ自分の番を待っている協力者がリーダー消失とみなして暴発する
			if (!bFireOrderAssigned)
			{
				for (const TWeakObjectPtr<AEnemyCharacter>& Coop : RecruitedCoops)
				{
					if (AEnemyCharacter* C = Coop.Get()) Director->ReleaseCoordinator(C);
				}
			}
		}
	}
	RecruitedCoops.Reset();
	RecruitedSlotAngles.Reset();

	if (UEnemyAnimInstance* Anim = GetEnemyAnimInstance())
	{
		Anim->bFlamethrowerActive = false;
		Anim->FlamethrowerAimBlend = 0.0f;
		// 視線IK抑制を解除する(LookAtAlphaは通常どおり追従へフェードで戻る)
		Anim->bSuppressLookAtIK = false;
	}

	// 火炎ハザードを破棄する(ループ未到達で中断された場合も確実に片付ける)
	DestroyFlame();

	Phase = EFlamePhase::None;
}

void UFlamethrowerAttackExecution::BeginLoop()
{
	// 開始モンタージュを完全に再生し終えたこの時点でループ噴射を有効化する
	// (開始と同時に有効化すると開始モーションの途中でループBlendSpaceが現れてしまうため)
	if (UEnemyAnimInstance* Anim = GetEnemyAnimInstance())
	{
		Anim->bFlamethrowerActive = true;
	}

	// ループ噴射と同時に火炎判定を有効化する
	if (ActiveFlame)
	{
		ActiveFlame->Activate();
	}

	Phase = EFlamePhase::Looping;
	LoopElapsed = 0.0f;

	// LockOnFire用に、噴射に入った時点のワールド照準を控える
	// 以降UpdateAimBlendはこの角度を撃ち続け、EM本体が動いても火炎の向きは変わらない
	if (const AEnemyCharacter* Enemy = CachedEnemy.Get())
	{
		LatchedFlameYaw = Enemy->GetActorRotation().Yaw + CurrentAimBlend * AimBlendMaxYaw;
	}
}

void UFlamethrowerAttackExecution::BeginRecovery()
{
	// 後隙。ここを挟まないと終了モーションのブレンドアウトと同時に次の行動へ移れてしまう
	if (RecoveryHold <= 0.0f)
	{
		Phase = EFlamePhase::None;
		if (FinishDelegate) FinishDelegate(true);
		return;
	}

	Phase = EFlamePhase::Recovering;
	RecoveryElapsed = 0.0f;
}

void UFlamethrowerAttackExecution::BeginEnd()
{
	Phase = EFlamePhase::Ending;

	// ループBlendSpaceを止め、終了モンタージュで下地を覆う
	if (UEnemyAnimInstance* Anim = GetEnemyAnimInstance())
	{
		Anim->bFlamethrowerActive = false;
	}

	// 噴射停止と同時に火炎判定を止める(終了モーションでは炎を出さない)
	if (ActiveFlame)
	{
		ActiveFlame->Deactivate();
	}

	UAnimInstance* AnimInst = CachedAnimInstance.Get();
	if (EndMontage && AnimInst)
	{
		AnimInst->Montage_Play(EndMontage);
		AnimInst->OnMontageEnded.AddDynamic(this, &UFlamethrowerAttackExecution::OnEndMontageEnded);
	}
	else
	{
		// 終了モンタージュ無し: 後隙へ
		BeginRecovery();
	}
}

void UFlamethrowerAttackExecution::UpdateAimBlend(AEnemyCharacter* Enemy, float DeltaTime)
{
	// 正面のみ: 首振りモーション(BlendSpace)を一切使わず、体の正面へ真っ直ぐ噴射する
	// ブレンド値を0に張り付かせるので、左右モーションが混ざらない
	if (AimMode == EFlameAimMode::ForwardOnly)
	{
		CurrentAimBlend = 0.0f;
		if (UEnemyAnimInstance* Anim = GetEnemyAnimInstance())
		{
			Anim->FlamethrowerAimBlend = 0.0f;
		}
		if (ActiveFlame)
		{
			ActiveFlame->SetExternalYaw(Enemy->GetActorRotation().Yaw);
		}
		return;
	}

	// 噴射開始で固定: 控えたワールド照準をそのまま撃ち続ける
	// ブレンド値も更新しないので、モーションの左右振りも噴射開始の姿勢で止まる
	if (AimMode == EFlameAimMode::LockOnFire && Phase == EFlamePhase::Looping)
	{
		if (ActiveFlame)
		{
			ActiveFlame->SetExternalYaw(LatchedFlameYaw);
		}
		return;
	}

	// PC方向のヨーとEMの向きのヨー差を [-AimBlendMaxYaw,
	// +AimBlendMaxYaw] で正規化して-1(左)..0(正面)..+1(右)
	// のブレンド値にする
	float TargetBlend = 0.0f;
	if (const AActor* Target = TideCombatUtil::GetTarget(Enemy))
	{
		const FVector ToTarget = Target->GetActorLocation() - Enemy->GetActorLocation();
		if (!ToTarget.IsNearlyZero())
		{
			const float ToTargetYaw = FMath::RadiansToDegrees(FMath::Atan2(ToTarget.Y, ToTarget.X));
			const float YawDiff = FRotator::NormalizeAxis(ToTargetYaw - Enemy->GetActorRotation().Yaw);

			// 担当方向から離れすぎた相手は追わず、正面(=担当方向)へ戻す
			// 追い続けると真後ろでヨー差の符号が反転して火炎が左右に暴れる
			// 背中合わせでは「相方が見ている側」まで追ってしまい、背中を守る役目も崩れる
			// AimBlendMaxYawを超えてからAimFalloffRangeDegかけて0へ落とし、
			// 境界で飛ばさない
			const float Excess = FMath::Abs(YawDiff) - AimBlendMaxYaw;
			const float Falloff = 1.0f - FMath::Clamp(Excess / AimFalloffRangeDeg, 0.0f, 1.0f);

			TargetBlend = FMath::Clamp(YawDiff / FMath::Max(1.0f, AimBlendMaxYaw), -1.0f, 1.0f) * Falloff;
		}
	}

	if (AimBlendInterpSpeed > 0.0f)
	{
		CurrentAimBlend = FMath::FInterpTo(CurrentAimBlend, TargetBlend, DeltaTime, AimBlendInterpSpeed);
	}
	else
	{
		CurrentAimBlend = TargetBlend;
	}

	if (UEnemyAnimInstance* Anim = GetEnemyAnimInstance())
	{
		Anim->FlamethrowerAimBlend = CurrentAimBlend;
	}

	// 火炎の実方向はプログラム制御(位置はソケット追従・方向は本制御)
	// ブレンド値 × AimBlendMaxYawがそのまま実照準角になる(ブレンドモーションの見た目が
	// ±60度程度しか振れなくても、判定/VFXはAimBlendMaxYawまで振り切れる)
	if (ActiveFlame)
	{
		ActiveFlame->SetExternalYaw(Enemy->GetActorRotation().Yaw + CurrentAimBlend * AimBlendMaxYaw);
	}
}

void UFlamethrowerAttackExecution::SpawnFlame(AEnemyCharacter* Enemy)
{
	if (!FlameHazardClass || !Enemy) return;

	UWorld* World = Enemy->GetWorld();
	if (!World) return;

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = Enemy; // 敵味方判定のソースを引き継ぐ
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AFlameStreamHazard* Flame = World->SpawnActor<AFlameStreamHazard>(
		FlameHazardClass, FTransform::Identity, SpawnParams);
	if (!Flame) return;

	// 現在の攻撃データからダメージ・ヒットリアクションを注入する(弾攻撃と同じ流儀)
	// 攻撃データ未指定ならBP(FlameHazardClass) の既定値を維持する
	TideCombatUtil::InjectAttackDamage(Enemy, Flame->Damage);
	TideCombatUtil::InjectAttackHitReactionTag(Enemy, Flame->HitReactionTag);

	// マズルソケットへアタッチ。以降は円錐の原点/向きがソケット(=アニメ照準)に追従する
	if (USkeletalMeshComponent* Mesh = Enemy->GetMesh())
	{
		Flame->AttachToComponent(
			Mesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale, FlameSocketName);
		Flame->SetActorRelativeLocation(FlameRelativeOffset);
		Flame->SetActorRelativeRotation(FlameRelativeRotation);

		// メッシュのポーズ更新後に火炎のTickを走らせる。ソケット追従を1フレーム遅らせず、
		// yaw平坦化を今フレームの最終的な向きとして確定させる(VFX/判定/デバッグ描画が揃う)
		Flame->AddTickPrerequisiteComponent(Mesh);
	}

	// 予兆用に本来の到達距離を控え、開始距離まで縮めておく(伸ばすのはOnAttackTick)
	// 行側で上書きできる(1つのHazard BPを隊形違いで使い回して間合いだけ変える)
	// 先にSetFinalRangeで伸びきりを確定させると、見た目は最初から伸びきった長さで出る
	FlameFullRange = (FlameRange > 0.0f) ? FlameRange : Flame->GetRange();
	Flame->SetFinalRange(FlameFullRange);
	if (FlameGrowDuration > 0.0f)
	{
		Flame->SetRange(FMath::Min(FlameStartRange, FlameFullRange));
	}

	ActiveFlame = Flame;
}

void UFlamethrowerAttackExecution::DestroyFlame()
{
	if (ActiveFlame)
	{
		ActiveFlame->Destroy();
		ActiveFlame = nullptr;
	}
}

UEnemyAnimInstance* UFlamethrowerAttackExecution::GetEnemyAnimInstance() const
{
	if (AEnemyCharacter* Enemy = CachedEnemy.Get())
	{
		if (USkeletalMeshComponent* Mesh = Enemy->GetMesh())
		{
			return Cast<UEnemyAnimInstance>(Mesh->GetAnimInstance());
		}
	}
	return nullptr;
}

void UFlamethrowerAttackExecution::OnStartMontageBlendingOut(UAnimMontage* Montage, bool bInterrupted)
{
	if (Montage != StartMontage) return;

	if (UAnimInstance* AnimInst = CachedAnimInstance.Get())
	{
		AnimInst->OnMontageBlendingOut.RemoveDynamic(this, &UFlamethrowerAttackExecution::OnStartMontageBlendingOut);
	}

	// 中断された場合はOnAttackEnd側で片付くのでループへは進めない
	if (bInterrupted || Phase != EFlamePhase::Starting) return;

	// 開始モンタージュのブレンドアウトとループBlendSpaceのフェードインをクロスさせる
	BeginLoop();
}

void UFlamethrowerAttackExecution::OnEndMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	if (Montage != EndMontage) return;

	if (UAnimInstance* AnimInst = CachedAnimInstance.Get())
	{
		AnimInst->OnMontageEnded.RemoveDynamic(this, &UFlamethrowerAttackExecution::OnEndMontageEnded);
	}

	if (Phase != EFlamePhase::Ending) return;

	if (bInterrupted)
	{
		Phase = EFlamePhase::None;
		if (FinishDelegate) FinishDelegate(false);
		return;
	}

	BeginRecovery();
}

void UFlamethrowerAttackExecution::BeginJumpToSlot(AEnemyCharacter* Enemy, const FVector& Slot)
{
	Phase = EFlamePhase::Jumping;

	// 飛び込み中は照準/噴射はオフにしておく
	if (UEnemyAnimInstance* Anim = GetEnemyAnimInstance())
	{
		Anim->bFlamethrowerActive = false;
		Anim->FlamethrowerAimBlend = 0.0f;
	}

	if (UAnimInstance* AnimInst = CachedAnimInstance.Get())
	{
		if (JumpMontage) AnimInst->Montage_Play(JumpMontage);
	}

	// 物理ではなくSetActorLocationの曲線補間で飛ぶ
	// (BossCharacterのPlatformJumpと同方式)
	// 物理LaunchはDetourCrowd/重力に打ち消されて手前に落ちるため、
	// 確実に着地点へ届くこの方式にする。Slotは足元(Nav)座標なので、
	// カプセル中心がその上へ来るよう半分の高さを足す
	float HalfHeight = 0.0f;
	if (const UCapsuleComponent* Cap = Enemy->GetCapsuleComponent())
	{
		HalfHeight = Cap->GetScaledCapsuleHalfHeight();
	}
	JumpStart = Enemy->GetActorLocation();
	JumpTarget = Slot + FVector(0.0f, 0.0f, HalfHeight);
	JumpElapsed = 0.0f;

	// SetActorLocationを重力/接地拘束・経路追従に邪魔させない。着地で戻す
	if (AAIController* AIC = Cast<AAIController>(Enemy->GetController()))
	{
		AIC->StopMovement();
	}
	if (UCharacterMovementComponent* Move = Enemy->GetCharacterMovement())
	{
		Move->SetMovementMode(MOVE_Flying);
	}

	// 飛び込みは軌道が固定で回避も防御もできない。着地までは怯まないようにする
	// ダメージが通るSuperArmorを使う(Invincibleだと飛び込みを狩る手が無くなる)
	Enemy->AddStateTag(TAG_State_Common_SuperArmor);
	bJumpSuperArmorApplied = true;

#if !UE_BUILD_SHIPPING
	// 協力者が実際に飛び込みを開始した瞬間。指名(シアン)は出るのに飛ばない個体を切り分ける用
	if (bDrawFormationDebug)
	{
		if (UWorld* World = Enemy->GetWorld())
		{
			DrawDebugLine(World, JumpStart, JumpTarget, FColor::Magenta, false, FormationDebugDuration, 0, 3.0f);
			DrawDebugString(World, JumpStart + FVector(0, 0, 220), TEXT("JUMP"), nullptr, FColor::Magenta, FormationDebugDuration);
		}
	}
#endif
}

void UFlamethrowerAttackExecution::TickJump(AEnemyCharacter* Enemy, float DeltaTime)
{
	JumpElapsed += DeltaTime;
	const float Duration = FMath::Max(JumpDuration, 0.05f);
	const float Alpha = FMath::Clamp(JumpElapsed / Duration, 0.0f, 1.0f);

	// 水平/垂直は線形、そこへ放物線アーチ(頂点 = JumpArcHeight)を足す
	FVector Pos = FMath::Lerp(JumpStart, JumpTarget, Alpha);
	Pos.Z += JumpArcHeight * 4.0f * Alpha * (1.0f - Alpha);
	Enemy->SetActorLocation(Pos, /*bSweep=*/false);

	if (Alpha >= 1.0f)
	{
		// 着地点へスナップして通常移動へ戻し、スタンバイでリーダーの合図を待つ
		Enemy->SetActorLocation(JumpTarget, /*bSweep=*/false);
		EndJump(Enemy);

		if (UAIDirector* Director = Enemy->GetWorld() ? Enemy->GetWorld()->GetSubsystem<UAIDirector>() : nullptr)
		{
			// リーダーが決めた向きを着地時に一度だけ受け取る
			// (背中合わせのようにPCを向かない配置があるため)
			float SlotYaw = 0.0f;
			if (Director->GetCoordinationSlotYaw(Enemy, SlotYaw))
			{
				Enemy->SetActorRotation(FRotator(0.0f, SlotYaw, 0.0f));
			}
			Director->MarkCoordinatorArrived(Enemy);
		}

		Phase = EFlamePhase::Standby;
		FireDelayRemaining = -1.0f;
	}
}

void UFlamethrowerAttackExecution::EndJump(AEnemyCharacter* Enemy)
{
	if (!Enemy) return;

	if (UCharacterMovementComponent* Move = Enemy->GetCharacterMovement())
	{
		if (Move->MovementMode == MOVE_Flying)
		{
			Move->SetMovementMode(MOVE_Walking);
		}
	}

	// 着地・中断のどちらを通ってもここでスーパーアーマーを外す
	// 外し漏れると以後ずっと怯まない個体になるので、付けた時だけ確実に落とす
	if (bJumpSuperArmorApplied)
	{
		Enemy->RemoveStateTag(TAG_State_Common_SuperArmor);
		bJumpSuperArmorApplied = false;
	}
}

void UFlamethrowerAttackExecution::LockFacing(AEnemyCharacter* Enemy)
{
	if (bFacingLocked || !Enemy) return;

	// 向きを書きに来るものが2系統あるので両方止める
	// 1) CMC: 敵は既定でbUseControllerDesiredRotation=true
	// 直前の状態でPCへ向けたままの   コントローラ回転へ毎tick引き戻される
	if (UCharacterMovementComponent* Move = Enemy->GetCharacterMovement())
	{
		bSavedUseControllerDesiredRotation = Move->bUseControllerDesiredRotation;
		Move->bUseControllerDesiredRotation = false;
	}

	// 2) モンタージュの向き補正通知
	// (UEnemyNotifyBehavior_RotateToTarget)
	// SetActorRotationで直接書くのでCMCを切っただけでは止まらない
	// リーダーと協力者が同じモンタージュを共有する以上、実行側から抑止するしかない
	Enemy->SetActionFacingLocked(true);

	bFacingLocked = true;
}

void UFlamethrowerAttackExecution::UnlockFacing(AEnemyCharacter* Enemy)
{
	if (!bFacingLocked || !Enemy) return;

	if (UCharacterMovementComponent* Move = Enemy->GetCharacterMovement())
	{
		Move->bUseControllerDesiredRotation = bSavedUseControllerDesiredRotation;
	}
	Enemy->SetActionFacingLocked(false);

	bFacingLocked = false;
}

bool UFlamethrowerAttackExecution::ResolveSlotGround(const AEnemyCharacter* Leader, FVector& InOutSlot) const
{
	UWorld* World = Leader ? Leader->GetWorld() : nullptr;
	if (!World) return false;

	float HalfHeight = 0.0f;
	if (const UCapsuleComponent* Cap = Leader->GetCapsuleComponent())
	{
		HalfHeight = Cap->GetScaledCapsuleHalfHeight();
	}
	const float LeaderFootZ = Leader->GetActorLocation().Z - HalfHeight;

	// Navには歩けるかどうかだけを聞く。カプセル中心のまま投げると足元より1m近く下を
	// 探すことになり、薄い地形の下に敷いてある床を拾う。足元の高さで、縦幅も段差程度に絞る
	FVector Query = InOutSlot;
	Query.Z = LeaderFootZ;

	if (UNavigationSystemV1* Nav = UNavigationSystemV1::GetCurrent(World))
	{
		FNavLocation Projected;
		if (!Nav->ProjectPointToNavigation(Query, Projected, FVector(150.0f, 150.0f, MaxSlotHeightDiff)))
		{
			return false;
		}
		Query = Projected.Location;
	}

	// 高さは実ジオメトリから取り直す。ここが着地点のZになる
	// 他の敵のカプセルを床と誤認しないよう、
	// チャンネルではなくWorldStaticのオブジェクトだけを見る
	FCollisionObjectQueryParams ObjectParams;
	ObjectParams.AddObjectTypesToQuery(ECC_WorldStatic);

	// 単純コリジョンを持たない床(エンジンのPlane等)にすり抜けないよう複雑コリジョンも見る
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(Leader);
	Params.bTraceComplex = true;

	const FVector TraceFrom(Query.X, Query.Y, LeaderFootZ + MaxSlotHeightDiff + 100.0f);
	const FVector TraceTo(Query.X, Query.Y, LeaderFootZ - MaxSlotHeightDiff);

	FHitResult FloorHit;
	if (!World->LineTraceSingleByObjectType(FloorHit, TraceFrom, TraceTo, ObjectParams, Params))
	{
		// 幾何トレースが外れてもNavが通っている(歩ける床がある)なら、その投影Zで成立させる
		// Navに聞けない場合(Nav無し)はQuery.Z = リーダー足元のまま使う
		InOutSlot = Query;
		return true;
	}

	// リーダーと違う床(地形の下の地面など)を拾っていたら隊形を組まない
	if (FMath::Abs(FloorHit.ImpactPoint.Z - LeaderFootZ) > MaxSlotHeightDiff) return false;

	InOutSlot = FloorHit.ImpactPoint;
	return true;
}

bool UFlamethrowerAttackExecution::TryFormFormation(AEnemyCharacter* Leader)
{
	UWorld* World = Leader ? Leader->GetWorld() : nullptr;
	UAIDirector* Director = World ? World->GetSubsystem<UAIDirector>() : nullptr;
	if (!Director) return false;

	const FVector LeaderLoc = Leader->GetActorLocation();
	TArray<AEnemyCharacter*> Candidates = GatherCandidates(Leader);

#if !UE_BUILD_SHIPPING
	if (bDrawFormationDebug)
	{
		const float Dur = FormationDebugDuration;
		const UTideCharacterDataAsset* MyData = Leader->GetCharacterData();
		const float RadiusSq = RecruitRadius * RecruitRadius;
		const TArray<AEnemyCharacter*> Combatants = Director->GetCombatants(Leader);

		DrawDebugSphere(World, LeaderLoc, RecruitRadius, 24, FColor::Yellow, false, Dur, 0, 1.5f);
		DrawDebugSphere(World, LeaderLoc, 45.0f, 8, FColor::White, false, Dur, 0, 3.0f);

		// 近傍の同種を全走査し、なぜ候補外かを色で示す
		TArray<AActor*> AllEnemies;
		UGameplayStatics::GetAllActorsOfClass(World, AEnemyCharacter::StaticClass(), AllEnemies);
		for (AActor* A : AllEnemies)
		{
			AEnemyCharacter* E = Cast<AEnemyCharacter>(A);
			if (!E || E == Leader) continue;
			if (E->GetCharacterData() != MyData) continue; // 別種は表示しない
			if (FVector::DistSquared(E->GetActorLocation(), LeaderLoc) > RadiusSq) continue; // 半径外は表示しない

			FColor C; FString Reason;
			if (Candidates.Contains(E))                     { C = FColor::Green;  Reason = TEXT("OK"); }
			else if (!Combatants.Contains(E))               { C = FColor::White;  Reason = TEXT("not in combat (no target)"); }
			else if (E->IsExecutingAttack())                { C = FColor::Orange; Reason = TEXT("executing"); }
			else if (Director->IsCoordinationReserved(E))   { C = FColor::Purple; Reason = TEXT("reserved"); }
			else                                            { C = FColor::Red;    Reason = TEXT("rejected"); }

			DrawDebugSphere(World, E->GetActorLocation() + FVector(0, 0, 60), 45.0f, 10, C, false, Dur, 0, 2.0f);
			DrawDebugString(World, E->GetActorLocation() + FVector(0, 0, 150), Reason, nullptr, C, Dur);
		}
	}
#endif

	const TArray<float> SlotAngles = GetSlotAngles();
	const int32 NeedCoop = GetRequiredCoopCount();
	if (Candidates.Num() < NeedCoop) return false;

	// リーダー→PCを前方にした弧上にスロットを作る
	const AActor* Target = TideCombatUtil::GetTarget(Leader);
	FVector Forward = Target
		? (Target->GetActorLocation() - LeaderLoc).GetSafeNormal2D()
		: Leader->GetActorForwardVector().GetSafeNormal2D();
	if (Forward.IsNearlyZero()) Forward = Leader->GetActorForwardVector().GetSafeNormal2D();

	// 第1段: スロットを検証して予約だけ取る。ここで1つでも欠けたら丸ごと巻き戻せる
	// (RequestForceAttackを先に出すと、巻き戻しても相手は既に飛び始めている)
	struct FPendingSlot
	{
		AEnemyCharacter* Coop = nullptr;
		float AngleDeg = 0.0f;
		int32 CoopAttackIndex = -1;

	};
	TArray<FPendingSlot> Pending;

	for (int32 i = 0; i < NeedCoop; ++i)
	{
		const float AngleDeg = SlotAngles.IsValidIndex(i) ? SlotAngles[i] : 0.0f;
		const FVector Dir = Forward.RotateAngleAxis(AngleDeg, FVector::UpVector);
		FVector SlotPos = LeaderLoc + Dir * FormationArcRadius;

		// 足場の検証(落下・埋まり防止)。1つでも置けない位置があれば隊形は組まない
		if (!ResolveSlotGround(Leader, SlotPos))
		{
#if !UE_BUILD_SHIPPING
			if (bDrawFormationDebug)
			{
				DrawDebugSphere(World, SlotPos, 50.0f, 10, FColor::Red, false, FormationDebugDuration, 0, 2.0f);
				DrawDebugString(World, SlotPos + FVector(0, 0, 60), TEXT("slot ground fail"), nullptr, FColor::Red, FormationDebugDuration);
			}
#endif
			break;
		}

		// スロットへ最も近い候補を割り当てる (移動距離最小化)
		int32 BestIdx = INDEX_NONE;
		float BestDistSq = TNumericLimits<float>::Max();
		for (int32 c = 0; c < Candidates.Num(); ++c)
		{
			const float D = FVector::DistSquared(Candidates[c]->GetActorLocation(), SlotPos);
			if (D < BestDistSq) { BestDistSq = D; BestIdx = c; }
		}
		if (BestIdx == INDEX_NONE) break;

		AEnemyCharacter* Coop = Candidates[BestIdx];

		// 協力者エントリのindexをRowNameから引く (同種=同テーブルなので必ず一致)
		const int32 CoopIdx = Coop->GetBattleComponent()
			? Coop->GetBattleComponent()->FindAttackIndexByRowName(CoopAttackRowName) : -1;
		if (CoopIdx < 0)
		{
#if !UE_BUILD_SHIPPING
			if (bDrawFormationDebug)
			{
				DrawDebugString(World, Coop->GetActorLocation() + FVector(0, 0, 200),
					FString::Printf(TEXT("RowName '%s' not found"), *CoopAttackRowName.ToString()),
					nullptr, FColor::Red, FormationDebugDuration);
			}
#endif
			break;
		}

		// 向きはリーダーが決めて予約に載せる(協力者は同じBPなので自分では判断できない)
		// 背中合わせはスロット方向(=PCの逆)。3体連続はPC方向から外側へ開いて扇状にする
		// (立ち位置のAngleDegとは別の値。並びの広さと向きの開きは別々に詰めたい)
		float SlotYaw = (IsBackToBack() ? Dir : Forward).Rotation().Yaw;
		if (FormationType == EFlameFormationType::Trio)
		{
			SlotYaw += FMath::Sign(AngleDeg) * TrioOutwardYawDeg;
		}
		if (!Director->ReserveCoordinator(Coop, Leader, SlotPos, SlotYaw)) break;

		Candidates.RemoveAtSwap(BestIdx);
		Pending.Add(FPendingSlot{ Coop, AngleDeg, CoopIdx });

#if !UE_BUILD_SHIPPING
		if (bDrawFormationDebug)
		{
			DrawDebugSphere(World, SlotPos, 50.0f, 12, FColor::Cyan, false, FormationDebugDuration, 0, 2.0f);
			DrawDebugLine(World, Coop->GetActorLocation(), SlotPos, FColor::Cyan, false, FormationDebugDuration, 0, 2.0f);
		}
#endif
	}

	// 人数がそろわなければ全部巻き戻す。中途半端な人数で撃つと隊形の意味が崩れる
	if (Pending.Num() < NeedCoop)
	{
		for (const FPendingSlot& P : Pending)
		{
			Director->ReleaseCoordinator(P.Coop);
		}
		return false;
	}

	// 第2段: 指名を出す。RequestForceAttackはその場で相手を攻撃状態へ入れ、
	// 相手のOnAttackBeginが予約を読んで飛び込みを始める
	// ここで弾かれた個体は予約を外して落とす。発射順は生き残った人数で詰められるので、
	// 1人減っても連続攻撃としては成立する
	for (const FPendingSlot& P : Pending)
	{
		if (!P.Coop->RequestForceAttack(P.CoopAttackIndex))
		{
			Director->ReleaseCoordinator(P.Coop);
			continue;
		}
		RecruitedCoops.Add(P.Coop);
		RecruitedSlotAngles.Add(P.AngleDeg);
	}

	return !RecruitedCoops.IsEmpty();
}

void UFlamethrowerAttackExecution::TickStandbyAsLeader(AEnemyCharacter* Leader, float DeltaTime)
{
	UAIDirector* Director = Leader->GetWorld() ? Leader->GetWorld()->GetSubsystem<UAIDirector>() : nullptr;

	// まず全員の着地を待つ。地形や経路で飛び込めない個体が出ても隊形全体が固まらないよう、
	// StandbyJoinTimeoutで見切って残ったメンバーだけで進める
	if (!bAllCoordinatorsJoined)
	{
		StandbyJoinElapsed += DeltaTime;

		const bool bAllArrived = !Director || Director->AreAllCoordinatorsArrived(Leader);
		if (!bAllArrived && StandbyJoinElapsed < StandbyJoinTimeout) return;

		bAllCoordinatorsJoined = true;
		StandbyElapsed = 0.0f;
	}

	// 全員そろってからの静止。ここが「構える」見せ場になる
	StandbyElapsed += DeltaTime;
	if (StandbyElapsed < StandbyDuration) return;

	AssignFireOrder(Leader);

	// リーダーは隊形の中央。常に一番手として撃つ
	BeginFlameSequence(Leader);
}

void UFlamethrowerAttackExecution::TickStandbyAsCoordinator(AEnemyCharacter* Enemy, float DeltaTime)
{
	UAIDirector* Director = Enemy->GetWorld() ? Enemy->GetWorld()->GetSubsystem<UAIDirector>() : nullptr;

	// リーダーが発射順を配るまで待つ
	if (FireDelayRemaining < 0.0f)
	{
		// リーダーが中断・死亡して指揮が消えたら、待ち続けず単独で噴射して攻撃を終わらせる
		if (!Director || !Director->GetCoordinationLeader(Enemy))
		{
			BeginFlameSequence(Enemy);
			return;
		}

		float Delay = 0.0f;
		if (!Director->GetCoordinatorFireDelay(Enemy, Delay)) return;
		FireDelayRemaining = Delay;
	}

	FireDelayRemaining -= DeltaTime;
	if (FireDelayRemaining <= 0.0f)
	{
		BeginFlameSequence(Enemy);
	}
}

void UFlamethrowerAttackExecution::AssignFireOrder(AEnemyCharacter* Leader)
{
	bFireOrderAssigned = true;

	UAIDirector* Director = Leader->GetWorld() ? Leader->GetWorld()->GetSubsystem<UAIDirector>() : nullptr;
	if (!Director) return;

	// 先に撃つ側の符号。スロット角と同じ座標系で + が右、- が左
	float SideSign = (TrioFireOrder == EFlameTrioFireOrder::LeftFirst) ? -1.0f : 1.0f;

	// PCがどちら側にいるかはこの瞬間に1回だけ決める。毎フレーム見ると順番が揺れる
	// ほぼ正面(不感帯内)なら初期値のまま右側を先に撃たせる
	if (TrioFireOrder == EFlameTrioFireOrder::PlayerSide)
	{
		if (const AActor* Target = TideCombatUtil::GetTarget(Leader))
		{
			const FVector ToTarget = Target->GetActorLocation() - Leader->GetActorLocation();
			if (!ToTarget.IsNearlyZero())
			{
				const float ToTargetYaw = FMath::RadiansToDegrees(FMath::Atan2(ToTarget.Y, ToTarget.X));
				const float YawDiff = FRotator::NormalizeAxis(ToTargetYaw - Leader->GetActorRotation().Yaw);
				if (FMath::Abs(YawDiff) > SideDecisionDeadzoneDeg)
				{
					SideSign = FMath::Sign(YawDiff);
				}
			}
		}
	}

	// 先の側のスロットを1番に、反対側を後に。着地しなかった個体はここで落ちるので、
	// 残ったメンバーで順番が自動的に詰まる
	TArray<AEnemyCharacter*> FirstSide;
	TArray<AEnemyCharacter*> SecondSide;
	for (int32 i = 0; i < RecruitedCoops.Num(); ++i)
	{
		AEnemyCharacter* Coop = RecruitedCoops[i].Get();
		if (!Coop || !Director->IsCoordinatorArrived(Coop)) continue;

		const float AngleDeg = RecruitedSlotAngles.IsValidIndex(i) ? RecruitedSlotAngles[i] : 0.0f;
		if (FMath::Sign(AngleDeg) == SideSign) FirstSide.Add(Coop);
		else                                   SecondSide.Add(Coop);
	}
	FirstSide.Append(SecondSide);

	// リーダーが0番手なので、協力者は1番手からFireInterval刻みで続く
	for (int32 i = 0; i < FirstSide.Num(); ++i)
	{
		Director->SetCoordinatorFireDelay(FirstSide[i], (i + 1) * FireInterval);
	}
}
//-----------------------------------------------------------------------------
//! UProjectileAttackExecution2
//-----------------------------------------------------------------------------
void UGravityShotAttackExecution::OnAttackBegin(AEnemyCharacter* Enemy)
{
	if (!Enemy)
	{
		if (FinishDelegate) FinishDelegate(false);
		return;
	}

	CachedEnemy = Enemy;
	CachedAnimInstance = Enemy->GetMesh() ? Enemy->GetMesh()->GetAnimInstance() : nullptr;
	FireLoopElapsed = 0.0f;
	FireLoopDuration = 0.0f;
	PostStartElapsed = 0.0f;
	bHasFired = false;
	FrozenTelegraphLocation = FVector::ZeroVector;

	UAnimInstance* AnimInst = CachedAnimInstance.Get();
	if (AnimInst)
	{
		// 開始→ループの繋ぎ + 撃ちきるまでのループ再トリガをブレンドアウトで拾う
		AnimInst->OnMontageBlendingOut.AddDynamic(this, &UGravityShotAttackExecution::OnMontageBlendingOut);
		AnimInst->OnMontageEnded.AddDynamic(this, &UGravityShotAttackExecution::OnMontageEnded);
	}

	// 撃ちきり時間(発射開始からの経過で判定。OnAttackTick参照)
	{
		FireLoopDuration =  PostFireHold;
	}

	if (StartMontage && AnimInst)
	{
		Phase = EProjectilePhase::Starting;
		AnimInst->Montage_Play(StartMontage);
		BeginTelegraph(Enemy);
		BeginIndicatorProjectile(Enemy);
	}
	else
	{
		// 開始モンタージュ無し: 即ループへ(見た目とロジックを同時に開始)
		PlayLoopMontageVisual();
		BeginLoop();
	}
}

void UGravityShotAttackExecution::OnAttackTick(AEnemyCharacter* Enemy, float DeltaTime)
{
	if (Phase == EProjectilePhase::Starting)
	{
		UpdateTelegraph(Enemy, DeltaTime);
		return;
	}

	if (Phase != EProjectilePhase::Looping) return;

	// 予兆が完全にフェード
	// (Progress=1.0)してからPostStartFireDelay秒待って発射開始する
	if (!bHasFired)
	{
		const bool bFadeComplete = (TelegraphDuration <= 0.0f) || (TelegraphElapsed >= TelegraphDuration);
		if (!bFadeComplete)
		{
			// フェード完了までプレイヤー追従を継続する
			// (StartMontageより長いTelegraphFadeDurationが設定されている場合、
			// ここで継続する)
			UpdateTelegraph(Enemy, DeltaTime);

			if (TelegraphElapsed >= TelegraphDuration)
			{
				// ちょうど完了した瞬間: ここで初めて位置を固定する
				FreezeTelegraphPosition();
			}
			return;
		}

		PostStartElapsed += DeltaTime;
		if (PostStartElapsed >= PostStartFireDelay)
		{
			bHasFired = true;
			EndTelegraph();
			BeginFireBurst(Enemy);
		}
		return;
	}

	FireLoopElapsed += DeltaTime;
	if (FireLoopElapsed >= FireLoopDuration)
	{
		BeginEnd();
	}
}

void UGravityShotAttackExecution::OnAttackEnd(AEnemyCharacter* Enemy)
{
	EndTelegraph();
	// 中断・完了いずれでも呼ばれる。自分が再生したモンタージュを片付ける
	// (asyncモードは呼び出し側がモンタージュを止めないため、ここで停止する)
	if (UAnimInstance* AnimInst = CachedAnimInstance.Get())
	{
		AnimInst->OnMontageBlendingOut.RemoveDynamic(this, &UGravityShotAttackExecution::OnMontageBlendingOut);
		AnimInst->OnMontageEnded.RemoveDynamic(this, &UGravityShotAttackExecution::OnMontageEnded);
		if (StartMontage) AnimInst->Montage_Stop(0.15f, StartMontage);
		if (LoopMontage)  AnimInst->Montage_Stop(0.15f, LoopMontage);
		if (EndMontage)   AnimInst->Montage_Stop(0.15f, EndMontage);
	}

	Phase = EProjectilePhase::None;
}

void UGravityShotAttackExecution::PlayLoopMontageVisual()
{
	// 見た目の切り替えのみ。BlendOut時点(従来通りのタイミング)で呼ぶ
	if (UAnimInstance* AnimInst = CachedAnimInstance.Get())
	{
		if (LoopMontage) AnimInst->Montage_Play(LoopMontage);
	}
}

void UGravityShotAttackExecution::BeginLoop()
{
	// StartMontageが終わった時点で、フェードが既に完了していればここで位置を固定する
	// (TelegraphFadeDurationがStartMontageの長さ以下の場合の従来動作)
	// まだフェード中なら固定せず、
	// OnAttackTickのLooping分岐が完了までプレイヤー追従を継続し、
	// 完了した瞬間に位置を固定する(FreezeTelegraphPosition)
	const bool bFadeComplete = (TelegraphDuration <= 0.0f) || (TelegraphElapsed >= TelegraphDuration);
	if (bFadeComplete)
	{
		FreezeTelegraphPosition();
	}

	PostStartElapsed = 0.0f;
	bHasFired = false;

	Phase = EProjectilePhase::Looping;

	// 発射開始
	// (PostStartFireDelay経過後)と撃ちきり判定はOnAttackTickで行う
}

void UGravityShotAttackExecution::FreezeTelegraphPosition()
{
	// TelegraphFollowLocationはデカールの有無に関わらず常に追従済みのため、そのままロックする
	FrozenTelegraphLocation = TelegraphFollowLocation;
}

void UGravityShotAttackExecution::BeginEnd()
{
	Phase = EProjectilePhase::Ending;

	UAnimInstance* AnimInst = CachedAnimInstance.Get();
	if (EndMontage && AnimInst)
	{
		// OnMontageEndedはOnAttackBeginで既にバインド済み
		// (EndMontage分岐も同じハンドラで処理する)
		AnimInst->Montage_Play(EndMontage);
	}
	else
	{
		// 終了モンタージュ無し: 即完了
		Phase = EProjectilePhase::None;
		if (FinishDelegate) FinishDelegate(true);
	}
}

void UGravityShotAttackExecution::OnMontageBlendingOut(UAnimMontage* Montage, bool bInterrupted)
{
	// 中断(別モンタージュ再生や被弾での上書き)は次へ繋がない
	if (bInterrupted) return;

	if (Phase == EProjectilePhase::Starting && Montage == StartMontage)
	{
		// 開始モーション終了 → 見た目だけループへ切り替える(従来通りBlendOutのタイミング)
		// ロジック側(予兆位置の固定・発射開始カウント)はOnMontageEndedまで据え置く
		PlayLoopMontageVisual();
	}
	else if (Phase == EProjectilePhase::Looping && Montage == LoopMontage)
	{
		// 撃ちきるまでシームレスにループ再生し直す
		if (UAnimInstance* AnimInst = CachedAnimInstance.Get())
		{
			AnimInst->Montage_Play(LoopMontage);
		}
	}
}

void UGravityShotAttackExecution::OnMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	if (Montage == StartMontage)
	{
		// StartMontageが完全に終わった時点
		// (予兆のProgressが1.0に到達済み)でロジックを進める
		if (Phase == EProjectilePhase::Starting)
		{
			BeginLoop();
		}
	}
	else if (Montage == EndMontage)
	{
		if (Phase != EProjectilePhase::Ending) return;

		Phase = EProjectilePhase::None;
		if (FinishDelegate) FinishDelegate(!bInterrupted);
	}
}

void UGravityShotAttackExecution::BeginFireBurst(AEnemyCharacter* Enemy)
{
	if (!Enemy || !Profile) return;

	CachedEnemy = Enemy;
	// 1発目を即時発射し、残りをFireInterval間隔で継続する
	FireOne();
}

void UGravityShotAttackExecution::FireOne()
{
	AEnemyCharacter* Enemy = CachedEnemy.Get();
	// 発射位置は予兆でロックした地点(FrozenTelegraphLocation)に固定
	SpawnProjectileAt(Enemy, FrozenTelegraphLocation);
}

void UGravityShotAttackExecution::SpawnProjectileAt(AEnemyCharacter* Enemy, const FVector& Location)
{
	if (!Enemy || !Profile) return;

	UWorld* World = Enemy->GetWorld();
	if (!World) return;

	UProjectilePoolSubsystem* Pool = World->GetSubsystem<UProjectilePoolSubsystem>();
	if (!Pool) return;

	// WorldUp軸まわりのランダムな向きで生成する
	const FRotator Rotation(0.0f, FMath::FRandRange(0.0f, 360.0f), 0.0f);
	const FTransform SpawnTransform(Rotation, Location);

	// Acquireがプロファイルを適用(パラメータ + 飛び方/効果ビヘイビアの複製)して返す
	// 追尾するかはプロファイルのMovementビヘイビアが決める
	AEnemyProjectile* Projectile = Pool->Acquire(Profile, SpawnTransform, Enemy, Enemy->GetInstigator());
	if (!Projectile) return;

	// 実行中攻撃のダメージで上書きする(負値ならプロファイル既定を尊重)
	// InitFromProfileが毎回プロファイル値へ戻すため、プール再利用でも前回値は残らない
	TideCombatUtil::InjectAttackDamage(Enemy, Projectile->Damage);

	Projectile->ActivateProjectile();
}

void UGravityShotAttackExecution::BeginTelegraph(AEnemyCharacter* Enemy)
{
	if (!Enemy) return;

	ACharacter* Player = Cast<ACharacter>(UGameplayStatics::GetPlayerPawn(Enemy, 0));
	if (!Player) return;

	TelegraphElapsed = 0.0f;
	TelegraphDuration = TelegraphFadeDuration > 0.0f
		? TelegraphFadeDuration
		: (StartMontage ? StartMontage->GetPlayLength() : 0.0f);

	const FVector FootLoc = ComputeGroundLocationBelowPlayer(Enemy, Player);
	TelegraphFollowLocation = FootLoc;

	// デカール素材が未設定なら見た目の予兆デカールは出さない
	// (追従先の計算・インジケーター弾/本弾の着弾位置には影響しない)
	if (!TelegraphDecalMaterial) return;

	UWorld* World = Enemy->GetWorld();
	if (!World) return;

	// LifeSpan=0(無期限)で生成し、EndTelegraphで明示的に破棄する
	if (UDecalComponent* Decal = UGameplayStatics::SpawnDecalAtLocation(
		World, TelegraphDecalMaterial, TelegraphDecalSize, FootLoc, FRotator(-90.0f, 0.0f, 0.0f), 0.0f))
	{
		if (UMaterialInstanceDynamic* MID = Decal->CreateDynamicMaterialInstance())
		{
			MID->SetScalarParameterValue(TelegraphDecalProgressParam, 0.0f);
			TelegraphDecalMID = MID;
		}
		TelegraphDecal = Decal;
	}
}

void UGravityShotAttackExecution::UpdateTelegraph(AEnemyCharacter* Enemy, float DeltaTime)
{
	// 追従先を更新する(Starting中のみ)。デカール未生成でも、
	// インジケーター弾・本弾の着弾位置計算のため常に追従させる
	if (ACharacter* Player = Cast<ACharacter>(UGameplayStatics::GetPlayerPawn(Enemy, 0)))
	{
		// トレース失敗時は前フレームのTelegraphFollowLocationに留まる(プレイヤー位置は参照しない)
		TelegraphFollowLocation = ComputeGroundLocationBelowPlayer(Enemy, Player, &TelegraphFollowLocation);
	}

	if (UDecalComponent* Decal = TelegraphDecal.Get())
	{
		Decal->SetWorldLocation(TelegraphFollowLocation);
	}

	UpdateTelegraphFade(DeltaTime);

	// 予兆と同じProgress(TelegraphElapsed/TelegraphDuration)・同じ追従先を使って、
	// インジケーター弾を発射位置→現在の追従先へ放物線移動させる
	UpdateIndicatorProjectile();
}

void UGravityShotAttackExecution::UpdateTelegraphFade(float DeltaTime)
{
	// フェード進行度(Progress)を進める。デカール未生成でもインジケーター弾のProgress計算に
	// 使うため常に進行させ、マテリアルへの反映のみデカールが存在する場合に限る。
	// TelegraphFadeDurationがStartMontageより長い場合、Starting終了後の
	// PostStartFireDelay待機中もここが呼ばれ続け、フェードが完了するまで進行する
	TelegraphElapsed += DeltaTime;
	const float Progress = FMath::Clamp(
		TelegraphDuration > 0.0f ? TelegraphElapsed / TelegraphDuration : 1.0f, 0.0f, 1.0f);
	if (UMaterialInstanceDynamic* MID = TelegraphDecalMID.Get())
	{
		MID->SetScalarParameterValue(TelegraphDecalProgressParam, Progress);
	}
}

void UGravityShotAttackExecution::EndTelegraph()
{
	if (UDecalComponent* Decal = TelegraphDecal.Get())
	{
		Decal->DestroyComponent();
	}
	TelegraphDecal.Reset();
	TelegraphDecalMID.Reset();
	TelegraphElapsed = 0.0f;
	TelegraphDuration = 0.0f;

	EndIndicatorProjectile();
}

void UGravityShotAttackExecution::BeginIndicatorProjectile(AEnemyCharacter* Enemy)
{
	if (!Enemy || !IndicatorProjectileProfile) return;

	UWorld* World = Enemy->GetWorld();
	if (!World) return;

	UProjectilePoolSubsystem* Pool = World->GetSubsystem<UProjectilePoolSubsystem>();
	if (!Pool) return;

	IndicatorLaunchLocation = Enemy->GetActorLocation() + FVector(0.0f, 0.0f, IndicatorLaunchHeight);

	AEnemyProjectile* Projectile = Pool->Acquire(
		IndicatorProjectileProfile, FTransform(FRotator::ZeroRotator, IndicatorLaunchLocation),
		Enemy, Enemy->GetInstigator());
	if (!Projectile) return;

	// 見た目専用: ダメージ・衝突・自前の弾道移動は起こさせず、位置は毎フレームこちらで直接動かす。
	// ActivateProjectile()が内部でコリジョンをQueryOnlyへ戻すため、無効化は呼び出し後に行う
	Projectile->ActivateProjectile();
	if (Projectile->CollisionComp)
	{
		Projectile->CollisionComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	if (Projectile->ProjectileMovement)
	{
		Projectile->ProjectileMovement->Deactivate();
	}

	IndicatorProjectile = Projectile;
}

void UGravityShotAttackExecution::UpdateIndicatorProjectile()
{
	AEnemyProjectile* Projectile = IndicatorProjectile.Get();
	if (!Projectile) return;

	// 予兆と共通のTelegraphFollowLocationへ、
	// 予兆と同じProgressで放物線移動する
	const float Progress = FMath::Clamp(
		TelegraphDuration > 0.0f ? TelegraphElapsed / TelegraphDuration : 1.0f, 0.0f, 1.0f);

	const FVector BaseLoc = FMath::Lerp(IndicatorLaunchLocation, TelegraphFollowLocation, Progress);
	const float ArcOffset = IndicatorArcHeight * 4.0f * Progress * (1.0f - Progress);
	const FVector NewLoc = BaseLoc + FVector(0.0f, 0.0f, ArcOffset);

	const FVector Delta = NewLoc - Projectile->GetActorLocation();
	const FRotator NewRot = Delta.IsNearlyZero() ? Projectile->GetActorRotation() : Delta.Rotation();

	Projectile->SetActorLocationAndRotation(NewLoc, NewRot);
}

void UGravityShotAttackExecution::EndIndicatorProjectile()
{
	if (AEnemyProjectile* Projectile = IndicatorProjectile.Get())
	{
		Projectile->Despawn();
	}
	IndicatorProjectile.Reset();
}

FVector UGravityShotAttackExecution::ComputeGroundLocationBelowPlayer(AEnemyCharacter* Enemy, ACharacter* Player, const FVector* PreviousLocation) const
{
	const FVector PlayerLoc = Player->GetActorLocation();

	float HalfHeight = 0.0f;
	if (const UCapsuleComponent* Capsule = Player->GetCapsuleComponent())
	{
		HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
	}

	// フォールバック: トレースが外れた場合、プレイヤーの位置は参照しない。
	// PreviousLocation指定時はそこに留まり(前の地点で足踏み)、
	// 未指定(初回)なら敵の足元を地面とみなす
	FVector GroundLoc;
	if (PreviousLocation)
	{
		GroundLoc = *PreviousLocation;
	}
	else
	{
		GroundLoc = Enemy ? Enemy->GetActorLocation() : PlayerLoc;
		if (Enemy)
		{
			if (const UCapsuleComponent* EnemyCapsule = Enemy->GetCapsuleComponent())
			{
				GroundLoc.Z -= EnemyCapsule->GetScaledCapsuleHalfHeight();
			}
		}
	}

	UWorld* World = Enemy ? Enemy->GetWorld() : nullptr;
	if (!World) return GroundLoc;

	FCollisionQueryParams Params;
	Params.AddIgnoredActor(Player);
	if (Enemy) Params.AddIgnoredActor(Enemy);

	const FVector TraceFrom = PlayerLoc;
	const FVector TraceTo = PlayerLoc - FVector(0.0f, 0.0f, HalfHeight + GroundTraceDistance);

	FHitResult FloorHit;
	if (World->LineTraceSingleByChannel(FloorHit, TraceFrom, TraceTo, ECC_WorldStatic, Params))
	{
		GroundLoc = FloorHit.ImpactPoint;
	}

	return GroundLoc;
}
//-----------------------------------------------------------------------------
// UTornadoShotAttackExecution
//-----------------------------------------------------------------------------
void UTornadoShotAttackExecution::OnAttackBegin(AEnemyCharacter* Enemy)
{
	if (!Enemy)
	{
		if (FinishDelegate) FinishDelegate(false);
		return;
	}

	CachedEnemy = Enemy;
	CachedAnimInstance = Enemy->GetMesh() ? Enemy->GetMesh()->GetAnimInstance() : nullptr;
	CurrentStageIndex = 0;
	CurrentStageTelegraphs.Reset();
	PlacedPositions.Reset();

	UAnimInstance* AnimInst = CachedAnimInstance.Get();
	if (!AnimInst || !StartMontage)
	{
		if (FinishDelegate) FinishDelegate(false);
		return;
	}

	// 開始→段階生成ループの繋ぎ (OnMontageEnded)、シームレスループ用
	// (OnMontageBlendingOut)
	AnimInst->OnMontageBlendingOut.AddDynamic(this, &UTornadoShotAttackExecution::OnMontageBlendingOut);
	AnimInst->OnMontageEnded.AddDynamic(this, &UTornadoShotAttackExecution::OnMontageEnded);

	Phase = ETornadoPhase::Starting;
	AnimInst->Montage_Play(StartMontage);
	BeginStageTelegraph(Enemy);
}

void UTornadoShotAttackExecution::OnAttackTick(AEnemyCharacter* Enemy, float DeltaTime)
{
	if (Phase != ETornadoPhase::Starting && Phase != ETornadoPhase::Looping) return;

	UpdateStageTelegraphs(DeltaTime);
}

void UTornadoShotAttackExecution::OnAttackEnd(AEnemyCharacter* Enemy)
{
	EndStageTelegraphs();
	PlacedPositions.Reset();

	if (UAnimInstance* AnimInst = CachedAnimInstance.Get())
	{
		AnimInst->OnMontageBlendingOut.RemoveDynamic(this, &UTornadoShotAttackExecution::OnMontageBlendingOut);
		AnimInst->OnMontageEnded.RemoveDynamic(this, &UTornadoShotAttackExecution::OnMontageEnded);
		if (StartMontage) AnimInst->Montage_Stop(0.15f, StartMontage);
		if (LoopMontage)  AnimInst->Montage_Stop(0.15f, LoopMontage);
		if (EndMontage)   AnimInst->Montage_Stop(0.15f, EndMontage);
	}

	Phase = ETornadoPhase::None;
}

void UTornadoShotAttackExecution::OnMontageBlendingOut(UAnimMontage* Montage, bool bInterrupted)
{
	// 中断(別モンタージュ再生や被弾での上書き)は次へ繋がない
	if (bInterrupted) return;

	if (Phase == ETornadoPhase::Looping && Montage == LoopMontage)
	{
		// 撃ちきる(＝全段生成完了)まで見た目上シームレスに再生し直す
		// 実際に次段へ進む/終了するかどうかの判断はOnMontageEnded
		// (AdvanceStage) が行う
		if (UAnimInstance* AnimInst = CachedAnimInstance.Get())
		{
			AnimInst->Montage_Play(LoopMontage);
		}
	}
}

void UTornadoShotAttackExecution::OnMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	// LoopMontageへの再生し直しで旧インスタンスはbInterrupted=trueで呼ばれるた
	// め、Start/Loopの段階進行はbInterruptedを見ずPhaseだけで判定する
	// (攻撃自体の外部中断はOnAttackEndが先にRemoveDynamicするのでこの分岐には来な
	// い)
	if (Montage == StartMontage)
	{
		if (Phase == ETornadoPhase::Starting)
		{
			AdvanceStage(CachedEnemy.Get());
		}
	}
	else if (Montage == LoopMontage)
	{
		if (Phase == ETornadoPhase::Looping)
		{
			AdvanceStage(CachedEnemy.Get());
		}
	}
	else if (Montage == EndMontage)
	{
		if (Phase != ETornadoPhase::Ending) return;

		Phase = ETornadoPhase::None;
		if (FinishDelegate) FinishDelegate(!bInterrupted);
	}
}

void UTornadoShotAttackExecution::AdvanceStage(AEnemyCharacter* Enemy)
{
	if (!Enemy) return;

	// 現在の段(予兆を出していた段)の弾を生成してから予兆を片付ける
	SpawnCurrentStageProjectiles(Enemy);
	EndStageTelegraphs();

	++CurrentStageIndex;
	if (CurrentStageIndex >= StageProjectileCounts.Num())
	{
		BeginEnd();
		return;
	}

	// 残り段あり:次段の予兆を出す
	// LoopMontageの再生継続はOnMontageBlendingOutの
	// 自己再トリガが既に担っているため
	// ここでMontage_Playを呼ぶのは最初のStarting→Looping遷移時のみでよい
	// 2回目以降も呼ぶと、直前に自己再トリガされたばかりのインスタンスを即座に打ち切ってしまい、
	// 次段の予兆表示時間が実質ゼロになる(最終段だけ異常に速く進む不具合の原因だった)
	const bool bNeedsInitialLoopPlay = (Phase != ETornadoPhase::Looping);
	Phase = ETornadoPhase::Looping;
	BeginStageTelegraph(Enemy);

	if (bNeedsInitialLoopPlay)
	{
		if (UAnimInstance* AnimInst = CachedAnimInstance.Get())
		{
			if (LoopMontage) AnimInst->Montage_Play(LoopMontage);
		}
	}
}

void UTornadoShotAttackExecution::BeginEnd()
{
	Phase = ETornadoPhase::Ending;

	UAnimInstance* AnimInst = CachedAnimInstance.Get();
	if (EndMontage && AnimInst)
	{
		AnimInst->Montage_Play(EndMontage);
	}
	else
	{
		// 終了モンタージュ無し: 即完了
		Phase = ETornadoPhase::None;
		if (FinishDelegate) FinishDelegate(true);
	}
}

void UTornadoShotAttackExecution::BeginStageTelegraph(AEnemyCharacter* Enemy)
{
	EndStageTelegraphs();

	// フェード時間: Stage0はStartMontage、
	// 以降はLoopMontageの長さを基準にする
	StageTelegraphElapsed = 0.0f;
	StageTelegraphDuration = (CurrentStageIndex == 0)
		? (StartMontage ? StartMontage->GetPlayLength() : 0.0f)
		: (LoopMontage ? LoopMontage->GetPlayLength() : 0.0f);

	if (!Enemy || !TelegraphDecalMaterial) return;

	// 段階数はStageProjectileCountsの要素数で決まる。空なら生成する段が無い
	const int32 Stages = StageProjectileCounts.Num();
	if (Stages <= 0) return;

	UWorld* World = Enemy->GetWorld();
	if (!World) return;

	// 外側(Stage0)から内側へ、半径帯を段階数で等分する
	const int32 Stage = FMath::Clamp(CurrentStageIndex, 0, Stages - 1);
	const float StageOuter = FMath::Lerp(OuterRadius, InnerRadius, static_cast<float>(Stage) / Stages);
	const float StageInner = FMath::Lerp(OuterRadius, InnerRadius, static_cast<float>(Stage + 1) / Stages);

	const FVector Center = Enemy->GetActorLocation();
	const int32 Count = FMath::Max(0, StageProjectileCounts[Stage]);

	const float MinSepSq = FMath::Square(FMath::Max(0.0f, MinProjectileSeparation));

	for (int32 i = 0; i < Count; ++i)
	{
		// 角度・半径ともランダムに抽選し、リング上ではなく帯の中でばらけて配置する
		// MinProjectileSeparation未満に既存の弾がいる候補は、上限回数まで引き直す
		// (全段通して累積したPlacedPositionsと比較するため、前段の弾からも一定距離離れる)
		// 上限まで条件を満たす候補が見つからなかった場合は、既存点との最小距離が
		// 一番大きかった(＝一番離れていた)候補を採用する(最後に試した候補をそのまま使うと
		// 密集条件下でMinProjectileSeparationを平気で下回ることがあったため)
		FVector HorizontalPoint = Center;
		float BestMinDistSq = -1.0f;
		const int32 MaxAttempts = Count * 30;
		for (int32 Attempt = 0; Attempt < MaxAttempts; ++Attempt)
		{
			const float Angle = FMath::FRandRange(0.0f, 360.0f);
			const float Radius = FMath::FRandRange(FMath::Min(StageInner, StageOuter), FMath::Max(StageInner, StageOuter));
			const FVector Candidate = Center + FVector(
				FMath::Cos(FMath::DegreesToRadians(Angle)) * Radius,
				FMath::Sin(FMath::DegreesToRadians(Angle)) * Radius,
				0.0f);

			float NearestDistSq = TNumericLimits<float>::Max();
			for (const FVector& Placed : PlacedPositions)
			{
				NearestDistSq = FMath::Min(NearestDistSq, FVector::DistSquared2D(Candidate, Placed));
			}

			if (NearestDistSq >= MinSepSq)
			{
				HorizontalPoint = Candidate;
				BestMinDistSq = NearestDistSq;
				break;
			}

			if (NearestDistSq > BestMinDistSq)
			{
				BestMinDistSq = NearestDistSq;
				HorizontalPoint = Candidate;
			}
		}
		PlacedPositions.Add(HorizontalPoint);

		FStageTelegraphEntry Entry;
		Entry.GroundLocation = ComputeGroundLocationAt(Enemy, HorizontalPoint);

		// LifeSpan=0(無期限)で生成し、EndStageTelegraphsで明示的に破棄する
		if (UDecalComponent* Decal = UGameplayStatics::SpawnDecalAtLocation(
			World, TelegraphDecalMaterial, TelegraphDecalSize, Entry.GroundLocation, FRotator(-90.0f, 0.0f, 0.0f), 0.0f))
		{
			if (UMaterialInstanceDynamic* MID = Decal->CreateDynamicMaterialInstance())
			{
				MID->SetScalarParameterValue(TelegraphDecalProgressParam, 0.0f);
				Entry.DecalMID = MID;
			}
			Entry.Decal = Decal;
		}

		CurrentStageTelegraphs.Add(Entry);
	}
}

void UTornadoShotAttackExecution::UpdateStageTelegraphs(float DeltaTime)
{
	if (CurrentStageTelegraphs.IsEmpty()) return;

	StageTelegraphElapsed += DeltaTime;
	const float Progress = FMath::Clamp(
		StageTelegraphDuration > 0.0f ? StageTelegraphElapsed / StageTelegraphDuration : 1.0f, 0.0f, 1.0f);

	for (FStageTelegraphEntry& Entry : CurrentStageTelegraphs)
	{
		if (UMaterialInstanceDynamic* MID = Entry.DecalMID.Get())
		{
			MID->SetScalarParameterValue(TelegraphDecalProgressParam, Progress);
		}
	}
}

void UTornadoShotAttackExecution::SpawnCurrentStageProjectiles(AEnemyCharacter* Enemy)
{
	if (!Enemy || !Profile) return;

	UWorld* World = Enemy->GetWorld();
	if (!World) return;

	UProjectilePoolSubsystem* Pool = World->GetSubsystem<UProjectilePoolSubsystem>();
	if (!Pool) return;

	for (const FStageTelegraphEntry& Entry : CurrentStageTelegraphs)
	{
		// 常駐弾なのでプレイヤーは狙わず、WorldUp軸まわりのランダムな向きで生成する
		const FRotator Rotation(0.0f, FMath::FRandRange(0.0f, 360.0f), 0.0f);
		const FTransform SpawnTransform(Rotation, Entry.GroundLocation);

		AEnemyProjectile* Projectile = Pool->Acquire(Profile, SpawnTransform, Enemy, Enemy->GetInstigator());
		if (!Projectile) continue;

		TideCombatUtil::InjectAttackDamage(Enemy, Projectile->Damage);
		Projectile->ActivateProjectile();
	}
}

void UTornadoShotAttackExecution::EndStageTelegraphs()
{
	for (FStageTelegraphEntry& Entry : CurrentStageTelegraphs)
	{
		if (UDecalComponent* Decal = Entry.Decal.Get())
		{
			Decal->DestroyComponent();
		}
	}
	CurrentStageTelegraphs.Reset();
}

FVector UTornadoShotAttackExecution::ComputeGroundLocationAt(AEnemyCharacter* Enemy, const FVector& HorizontalPoint) const
{
	FVector GroundLoc = HorizontalPoint;
	GroundLoc.Z = Enemy->GetActorLocation().Z;

	UWorld* World = Enemy->GetWorld();
	if (!World) return GroundLoc;

	FHitResult FloorHit;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(Enemy);

	const FVector TraceFrom(HorizontalPoint.X, HorizontalPoint.Y, Enemy->GetActorLocation().Z + 1000.0f);
	const FVector TraceTo(HorizontalPoint.X, HorizontalPoint.Y, Enemy->GetActorLocation().Z - 3000.0f);

	if (World->LineTraceSingleByChannel(FloorHit, TraceFrom, TraceTo, ECC_WorldStatic, Params))
	{
		GroundLoc = FloorHit.ImpactPoint;
	}

	return GroundLoc;
}
//-----------------------------------------------------------------------------
// USpecialAttackExecution
//-----------------------------------------------------------------------------
void USpecialAttackExecution::OnAttackBegin(AEnemyCharacter* Enemy)
{
	if (!Enemy)
	{
		if (FinishDelegate)
		{
			FinishDelegate(false);
		}
		return;
	}

	CachedEnemy = Enemy;
	CachedAnimInstance = Enemy->GetMesh() ? Enemy->GetMesh()->GetAnimInstance() : nullptr;

	UAnimInstance* AnimInst = CachedAnimInstance.Get();
	if (PlayMontage && AnimInst)
	{
		AnimInst->Montage_Play(PlayMontage);
		AnimInst->OnMontageBlendingOut.AddDynamic(this, &USpecialAttackExecution::OnMontageBlendingOut);
		AnimInst->OnMontageBlendingOut.AddDynamic(this, &USpecialAttackExecution::OnMontageEnded);
	}

	EmitCount = 0;

	if (FireInterval <= 0.0f)
	{
		// 0以下はタイマーが正しく機能しないため、タイマーを使わず全エミッターを同時に生成する
		while (EmitCount < Projectiles.Num())
		{
			SpawnProjectiles();
		}
	}
	else
	{
		SpawnProjectiles();
		Enemy->GetWorld()->GetTimerManager().SetTimer(
			EmitTimerHandle,
			this,
			&USpecialAttackExecution::SpawnProjectiles,
			FireInterval,
			true);
	}
}

void USpecialAttackExecution::OnAttackTick(AEnemyCharacter* Enemy, float DeltaTime)
{
	if (!Enemy) return;
}

void USpecialAttackExecution::OnAttackEnd(AEnemyCharacter* Enemy)
{
	// 中断・完了いずれでも呼ばれる。自分が再生したモンタージュを片付ける
	// (asyncモードは呼び出し側がモンタージュを止めないため、ここで停止する)
	if (UAnimInstance* AnimInst = CachedAnimInstance.Get())
	{
		if (PlayMontage)
		{
			AnimInst->Montage_Stop(0.15f, PlayMontage);
			AnimInst->OnMontageBlendingOut.RemoveDynamic(this, &USpecialAttackExecution::OnMontageBlendingOut);
			AnimInst->OnMontageEnded.RemoveDynamic(this, &USpecialAttackExecution::OnMontageEnded);
		}
	}
}

void USpecialAttackExecution::OnMontageBlendingOut(UAnimMontage* Montage, bool bInterrupted)
{
	if (FinishDelegate)
	{
		FinishDelegate(true);
	}
}

void USpecialAttackExecution::OnMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	if (FinishDelegate)
	{
		FinishDelegate(true);
	}
}

UEnemyAnimInstance* USpecialAttackExecution::GetEnemyAnimInstance() const
{
	if (AEnemyCharacter* Enemy = CachedEnemy.Get())
	{
		if (USkeletalMeshComponent* Mesh = Enemy->GetMesh())
		{
			return Cast<UEnemyAnimInstance>(Mesh->GetAnimInstance());
		}
	}
	return nullptr;
}

void USpecialAttackExecution::SpawnProjectiles()
{
	if (AEnemyCharacter* pEnemy = CachedEnemy.Get())
	{
		if (UWorld* World = pEnemy->GetWorld())
		{
			if (UProjectilePoolSubsystem* Pool = World->GetSubsystem<UProjectilePoolSubsystem>())
			{
				if (Projectiles.IsValidIndex(EmitCount))
				{
					const FVector EnemyLoc = pEnemy->GetActorLocation();
					const AActor* Target = TideCombatUtil::GetTarget(pEnemy);

					FVector Forward2D = Target
						? (Target->GetActorLocation() - EnemyLoc).GetSafeNormal2D()
						: pEnemy->GetActorForwardVector().GetSafeNormal2D();

					if (Forward2D.IsNearlyZero())
					{
						Forward2D = pEnemy->GetActorForwardVector().GetSafeNormal2D();
					}

					const FVector Center = EnemyLoc + Forward2D * SpawnForwardOffset;
					const FVector Right = pEnemy->GetActorRightVector().GetSafeNormal();
					const FVector Up = FVector::UpVector;

					auto SpawnOne = [&](UProjectileProfile* Profile, const FVector& SpawnPos, const FRotator& SpawnRot, float Hold)
						{
							AEnemyProjectile* Proj = Pool->Acquire(
								Profile,
								FTransform(SpawnRot, SpawnPos),
								pEnemy,
								pEnemy->GetInstigator());

							if (!Proj) return;

							TideCombatUtil::InjectAttackDamage(pEnemy, Proj->Damage);
							Proj->SpawnHoldDuration = Hold;
							Proj->ActivateProjectile();
						};

					FSpecialAttackProjectileEmitter emitter = Projectiles[EmitCount];
					// このエミッターの円の中心
					// ターゲット方向基準の中心線からCenterSideOffset分だけ左右にずらす
					const FVector EmitterCenter = Center
						+ Right * emitter.CenterSideOffset
						+ FVector::UpVector * emitter.CenterHeightOffset;

					for (int32 i = 0; i < emitter.Count; ++i)
					{
						if (!emitter.Profile || emitter.Count <= 0)
						{
							continue;
						}
						const float T = (emitter.Count > 1)
							? static_cast<float>(i) / static_cast<float>(emitter.Count - 1)
							: 0.5f;
						const float AngleDeg = FMath::Lerp(emitter.ArcDeg, 0.0f, T);
						const float AngleRad = FMath::DegreesToRadians(AngleDeg);
						const FVector PlaneDir = Right * FMath::Cos(AngleRad) + Up * FMath::Sin(AngleRad);
						const FVector SpawnPos = EmitterCenter + PlaneDir * emitter.Radius;
						// 外向きに放射
						const FVector Outward = (SpawnPos - EmitterCenter).GetSafeNormal();
						SpawnOne(emitter.Profile, SpawnPos, Outward.Rotation(), emitter.HoldDuration);
					}
					++EmitCount;
					if (EmitCount >= Projectiles.Num())
					{
						pEnemy->GetWorld()->GetTimerManager().ClearTimer(EmitTimerHandle);
					}
				}		
			}
		}
	}
}
