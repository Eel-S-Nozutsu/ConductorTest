// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#include "TideAttackExecution_EM0010.h"

#include "PRJ_TIDE_P0/Actors/Character/Enemy/EnemyCharacter.h"
#include "PRJ_TIDE_P0/Actors/Projectile/EnemyProjectile.h"
#include "PRJ_TIDE_P0/Actors/Projectile/ProjectilePoolSubsystem.h"
#include "PRJ_TIDE_P0/Actors/Projectile/ProjectileProfile.h"
#include "PRJ_TIDE_P0/Actors/Projectile/Behaviors/DeployHomingBehavior.h"
#include "PRJ_TIDE_P0/Actors/Projectile/Behaviors/BallisticBehavior.h"
#include "PRJ_TIDE_P0/Actors/Projectile/Behaviors/SplashDamageBehavior.h"
#include "PRJ_TIDE_P0/Actors/Projectile/Behaviors/GuidanceBehavior.h"
#include "PRJ_TIDE_P0/Actors/Projectile/Behaviors/ArcPathBehavior.h"
#include "PRJ_TIDE_P0/Actors/Projectile/Behaviors/BezierArcBehavior.h"
#include "PRJ_TIDE_P0/Actors/Projectile/BossShockwaveActor.h"
#include "PRJ_TIDE_P0/Actors/Hazard/BossMineHazard.h"
#include "PRJ_TIDE_P0/Actors/Hazard/HaloHazard.h"
#include "PRJ_TIDE_P0/Actors/Hazard/CircusRainConductor.h"
#include "PRJ_TIDE_P0/Components/Combat/PartDestructionComponent.h"
#include "PRJ_TIDE_P0/Core/TideGameSettings.h"
#include "PRJ_TIDE_P0/Utilities/TideCombatUtils.h"

#include "AIController.h"
#include "Animation/AnimInstance.h"
#include "Components/CapsuleComponent.h"
#include "Components/DecalComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "NavigationSystem.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"

FTransform UTurnSweepAttackExecution::ReadBoneComp(const AEnemyCharacter* Enemy) const
{
	const USkeletalMeshComponent* Mesh = Enemy ? Enemy->GetMesh() : nullptr;
	if (!Mesh || Mesh->GetBoneIndex(SourceBoneName) == INDEX_NONE) return FTransform::Identity;

	return Mesh->GetSocketTransform(SourceBoneName, RTS_Component);
}

void UTurnSweepAttackExecution::OnAttackBegin(AEnemyCharacter* Enemy)
{
	if (!Enemy) return;

	CachedEnemy = Enemy;

	// OnAttackEndはブレンドアウト完了後 (= ポーズが消えた後) に来るため差分が取れない
	// 最終姿勢はブレンドアウト開始で確定させる
	CachedAnimInstance = Enemy->GetMesh() ? Enemy->GetMesh()->GetAnimInstance() : nullptr;
	if (CachedAnimInstance.IsValid())
	{
		CachedAnimInstance->OnMontageBlendingOut.AddUniqueDynamic(
			this, &UTurnSweepAttackExecution::HandleMontageBlendingOut);
	}
}

void UTurnSweepAttackExecution::HandleMontageBlendingOut(UAnimMontage* InMontage, bool bInterrupted)
{
	if (InMontage != Montage || bInterrupted || bCommitting) return;

	AEnemyCharacter* Enemy = CachedEnemy.Get();
	if (!Enemy) return;

	CommitBoneComp = ReadBoneComp(Enemy);
	BaseLocation   = Enemy->GetActorLocation();
	BaseYaw        = Enemy->GetActorRotation().Yaw;
	bCommitting    = true;

	// ルートモーション無しではPhysicsRotationが生きており、
	// bUseControllerDesiredRotationで毎フレーム古いコントロール回転へ引き戻さ
	// れてカプセルの置き直しと競合する。注視点で合わせる手もあるがAIの向き制御と干渉するので、
	// commit中だけ駆動を止める
	if (UCharacterMovementComponent* Movement = Enemy->GetCharacterMovement())
	{
		Movement->StopMovementImmediately();
		bSavedUseControllerDesiredRotation = Movement->bUseControllerDesiredRotation;
		Movement->bUseControllerDesiredRotation = false;
	}
}

void UTurnSweepAttackExecution::ApplyCommit(AEnemyCharacter* Enemy) const
{
	const USkeletalMeshComponent* Mesh = Enemy ? Enemy->GetMesh() : nullptr;
	if (!Mesh) return;

	// 見た目のボーン姿勢 = ポーズ * メッシュ相対 * カプセル
	// これをブレンド開始時の値に固定するカプセル変換を逆算する (Current * C =
	// Frozen)。位置と向きを別々に足すと、
	// カプセル原点から離れたメッシュが振り回されて弧を描くため、
	// 必ずこの形でまとめて解く
	const FTransform MeshRel = Mesh->GetRelativeTransform();
	const FTransform Base(FRotator(0.0f, BaseYaw, 0.0f), BaseLocation);

	const FTransform Current = ReadBoneComp(Enemy) * MeshRel;
	const FTransform Frozen  = CommitBoneComp * MeshRel * Base;
	const FTransform Target  = Current.Inverse() * Frozen;

	// Zは触らない。接地はCharacterMovementに任せる
	const FVector TargetLoc = Target.GetLocation();
	const FVector NewLoc(TargetLoc.X, TargetLoc.Y, Enemy->GetActorLocation().Z);

	Enemy->SetActorLocationAndRotation(NewLoc, FRotator(0.0f, Target.Rotator().Yaw, 0.0f));
}

void UTurnSweepAttackExecution::OnAttackTick(AEnemyCharacter* Enemy, float DeltaTime)
{
	if (bCommitting) ApplyCommit(Enemy);
}

void UTurnSweepAttackExecution::OnAttackEnd(AEnemyCharacter* Enemy)
{
	if (CachedAnimInstance.IsValid())
	{
		CachedAnimInstance->OnMontageBlendingOut.RemoveDynamic(
			this, &UTurnSweepAttackExecution::HandleMontageBlendingOut);
	}

	// ブレンドアウト前に終わった場合 (早期遷移) は基準が無いので何も触っていない
	if (!bCommitting || !Enemy) return;
	bCommitting = false;

	// 中断時は割り込み側 (リアクション等) が姿勢を握るので最終適用はしない
	if (!bAttackInterrupted)
	{
		// ポーズは抜け切っているので、ここで残量ぶんを入れて締める
		ApplyCommit(Enemy);
	}

	// 退避した回転駆動は中断経路でも必ず戻す (戻し漏れるとAIが向きを制御できなくなる)
	if (UCharacterMovementComponent* Movement = Enemy->GetCharacterMovement())
	{
		Movement->bUseControllerDesiredRotation = bSavedUseControllerDesiredRotation;
	}
}

namespace
{
	// 着弾点がEnemyPosからMinDist未満なら押し出す
	FVector ClampBombardLandingPos(const FVector& LandingPos, const FVector& EnemyPos,
		float MinDist, const FVector& FallbackDir)
	{
		FVector Clamped = LandingPos;
		const float Dist2D = FVector::Dist2D(Clamped, EnemyPos);
		if (Dist2D < MinDist)
		{
			FVector Dir2D = (Clamped - EnemyPos).GetSafeNormal2D();
			if (Dir2D.IsNearlyZero())
			{
				Dir2D = FallbackDir.GetSafeNormal2D();
			}
			Clamped.X = EnemyPos.X + Dir2D.X * MinDist;
			Clamped.Y = EnemyPos.Y + Dir2D.Y * MinDist;
		}
		return Clamped;
	}

	// "End" セクションへジャンプして攻撃終了を通知する
	void JumpMontageToEnd(AEnemyCharacter* Enemy)
	{
		if (UAnimInstance* AnimInst = Enemy->GetMesh()->GetAnimInstance())
		{
			AnimInst->Montage_JumpToSection(FName("End"), AnimInst->GetCurrentActiveMontage());
		}
	}
}

// ------------------------------------------------------------
// UMissileAttackExecution
// ------------------------------------------------------------

void UMissileAttackExecution::OnAttackBegin(AEnemyCharacter* Enemy)
{
	MissileFiredCount = 0;
	Enemy->GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
}

void UMissileAttackExecution::OnAttackEnd(AEnemyCharacter* Enemy)
{
	Enemy->GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
}

void UMissileAttackExecution::OnAttackEvent(AEnemyCharacter* Enemy, FGameplayTag EventTag)
{
	if (EventTag == TAG_AttackEvent_MissileStart)
	{
		MissileFiredCount = 0;
	}
	else if (EventTag == TAG_AttackEvent_MissileShot)
	{
		SpawnMissilePair(Enemy);
	}
}

void UMissileAttackExecution::SpawnMissilePair(AEnemyCharacter* Enemy)
{
	if (!MissileProfile) return;

	UWorld* World = Enemy->GetWorld();
	UProjectilePoolSubsystem* Pool = World ? World->GetSubsystem<UProjectilePoolSubsystem>() : nullptr;
	if (!Pool) return;

	const FVector BackDir = -Enemy->GetActorForwardVector();
	const FVector RightDir = Enemy->GetActorRightVector();
	const FVector SpawnBase = Enemy->GetActorLocation()
		+ BackDir * MissileSpawnBackOffset
		+ FVector(0.0f, 0.0f, MissileSpawnHeightOffset);

	// MissileCount全体で中心対称に均等分散 (例: Count=4 → -1.5S,
	// -0.5S, +0.5S, +1.5S)
	const float NormalizedIndex = (MissileCount > 1) ? MissileFiredCount - (MissileCount - 1) * 0.5f : 0.0f;
	const float HeightStep = NormalizedIndex * MissileDeployHeightStagger;
	const FVector DeployBase = Enemy->GetActorLocation() + FVector(0.0f, 0.0f, MissileDeployHeightOffset);
	const FVector LeftTarget = DeployBase + (-RightDir) * MissileSideOffset + FVector(0.0f, 0.0f, HeightStep);
	const FVector RightTarget = DeployBase + RightDir * MissileSideOffset + FVector(0.0f, 0.0f, HeightStep);

	const FRotator SpawnRot = Enemy->GetActorRotation();

	auto SpawnOneSide = [&](const FVector& DeployTarget)
		{
			AEnemyProjectile* Missile = Pool->Acquire(
				MissileProfile, FTransform(SpawnRot, SpawnBase), Enemy, Enemy->GetInstigator());
			if (!Missile) return;

			TideCombatUtil::InjectAttackDamage(Enemy, Missile->Damage);
			// 展開先を1発ごとにビヘイビアへ注入 ※発射前
			if (UDeployHomingBehavior* Homing = Missile->FindBehavior<UDeployHomingBehavior>())
			{
				Homing->SetDeployTarget(DeployTarget);
			}
			// 誘導弾の場合はターゲット(プレイヤー)と初期方位(左右展開方向)を注入する
			// プロファイルのMovementを「誘導弾 (追尾)」にすると外へ出てから回り込んで追尾する
			if (UGuidanceBehavior* Guidance = Missile->FindBehavior<UGuidanceBehavior>())
			{
				Guidance->SetTarget(TideCombatUtil::GetTarget(Enemy));
				Guidance->SetInitialHeadingDirection((DeployTarget - SpawnBase).GetSafeNormal());
			}
			Missile->ActivateProjectile();
		};

	SpawnOneSide(LeftTarget);
	SpawnOneSide(RightTarget);

	++MissileFiredCount;
	if (MissileFiredCount >= MissileCount)
	{
		JumpMontageToEnd(Enemy);
	}
}

// ------------------------------------------------------------
// UShockwallAttackExecution
// ------------------------------------------------------------

void UShockwallAttackExecution::OnAttackEvent(AEnemyCharacter* Enemy, FGameplayTag EventTag)
{
	if (EventTag == TAG_AttackEvent_ShockwallStart)
	{
		CachedEnemy = Enemy;

		// プレイヤー周囲リングから着地点を抽選して放物線跳躍する
		// 有効な着地点が取れない場合は従来どおりその場ジャンプにフォールバックする
		FVector LeapTarget;
		if (ComputeLeapTarget(Enemy, LeapTarget))
		{
			LeapStartLocation = Enemy->GetActorLocation();
			LeapTargetLocation = LeapTarget;
			LeapElapsed = 0.0f;
			bLeaping = true;

			// 重力でアークが崩れないよう移動を手動制御に切り替える
			if (UCharacterMovementComponent* Move = Enemy->GetCharacterMovement())
			{
				Move->StopMovementImmediately();
				Move->SetMovementMode(MOVE_Flying);
			}
		}
		else
		{
			Enemy->Jump();
			Enemy->LandedDelegate.AddDynamic(this, &UShockwallAttackExecution::OnShockwallLanded);
		}

		// 攻撃開始: 全部位の光輪を発光させ、攻撃中の部位無敵を開始する (頭などは無敵対象外)
		if (UPartDestructionComponent* PartComp = Enemy->FindComponentByClass<UPartDestructionComponent>())
		{
			PartComp->BeginPartHaloAttackGlow();
		}
	}
	else if (ShockwallMineBurstEvent.IsValid() && EventTag == ShockwallMineBurstEvent)
	{
		BeginMineBurst(Enemy);
	}
	else if (EventTag == TAG_AttackEvent_ShockwallLand)
	{
		// 叩きつけで対象部位の光輪をディザフェードで消す (無敵は継続)
		if (UPartDestructionComponent* PartComp = Enemy->FindComponentByClass<UPartDestructionComponent>())
		{
			PartComp->HidePartHalosForAttack();
		}

		if (ShockwaveActorClass)
		{
			FVector SpawnLocation = Enemy->GetActorLocation();
			SpawnLocation.Z -= Enemy->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
			FActorSpawnParameters SpawnParams;
			SpawnParams.Owner = Enemy;
			ABossShockwaveActor* Shockwave = Enemy->GetWorld()->SpawnActor<ABossShockwaveActor>(
				ShockwaveActorClass,
				FTransform(Enemy->GetActorRotation(), SpawnLocation),
				SpawnParams);
			if (Shockwave)
			{
				const float AttackDamage = Enemy->GetCurrentAttackDamage();
				if (AttackDamage >= 0.0f) Shockwave->SetDamage(AttackDamage);
				const FGameplayTag ReactionTag = Enemy->GetCurrentAttackHitReactionTag();
				if (ReactionTag.IsValid()) Shockwave->SetHitReactionTag(ReactionTag);
			}
		}

		TWeakObjectPtr<AEnemyCharacter> WeakEnemy = Enemy;
		Enemy->GetWorldTimerManager().SetTimer(ShockwallLandTimerHandle, [WeakEnemy]()
			{
				if (!WeakEnemy.IsValid()) return;
				if (UAnimInstance* Anim = WeakEnemy->GetMesh()->GetAnimInstance())
				{
					Anim->Montage_JumpToSection(FName("Ed"));
				}
			}, ShockwallLandDuration, false);
	}
}

void UShockwallAttackExecution::OnAttackTick(AEnemyCharacter* Enemy, float DeltaTime)
{
	if (!bLeaping) return;

	LeapElapsed += DeltaTime;
	const float Duration = FMath::Max(0.05f, ShockwallAirDuration);
	const float Alpha = FMath::Clamp(LeapElapsed / Duration, 0.0f, 1.0f);

	// XYは始点→着地点を線形補間、Zは始点Z→着地点Zの線形に放物線アークを重ねる
	// アーク項4h*a*(1-a) はa=0,1で0、a=0.5で最大hとなる
	FVector NewLoc = FMath::Lerp(LeapStartLocation, LeapTargetLocation, Alpha);
	const float Arc = 4.0f * ShockwallArcHeight * Alpha * (1.0f - Alpha);
	NewLoc.Z += Arc;

	// カプセルスウィープで移動し、ジオメトリへのめり込みを防ぐ
	Enemy->SetActorLocation(NewLoc, true);

	if (Alpha >= 1.0f)
	{
		bLeaping = false;
		if (UCharacterMovementComponent* Move = Enemy->GetCharacterMovement())
		{
			Move->SetMovementMode(MOVE_Walking);
		}
		JumpToLandSection();
	}
}

void UShockwallAttackExecution::OnAttackEnd(AEnemyCharacter* Enemy)
{
	Enemy->GetWorldTimerManager().ClearTimer(ShockwallLandTimerHandle);
	Enemy->GetWorldTimerManager().ClearTimer(ShockwallMineBurstTimerHandle);
	RemainingShockwallMineCount = 0;
	Enemy->LandedDelegate.RemoveDynamic(this, &UShockwallAttackExecution::OnShockwallLanded);

	// 跳躍が途中で中断された場合に備えて移動モードを歩行へ戻す
	if (bLeaping)
	{
		bLeaping = false;
		if (UCharacterMovementComponent* Move = Enemy->GetCharacterMovement())
		{
			Move->SetMovementMode(MOVE_Walking);
		}
	}

	// 攻撃終了で発光を消し、部位光輪をディザフェードで再表示し、無敵を解除する
	if (UPartDestructionComponent* PartComp = Enemy->FindComponentByClass<UPartDestructionComponent>())
	{
		PartComp->RestorePartHalosAfterAttack();
	}
}

void UShockwallAttackExecution::OnShockwallLanded(const FHitResult& Hit)
{
	if (!CachedEnemy.IsValid()) return;
	CachedEnemy->LandedDelegate.RemoveDynamic(this, &UShockwallAttackExecution::OnShockwallLanded);
	JumpToLandSection();
}

void UShockwallAttackExecution::JumpToLandSection()
{
	if (!CachedEnemy.IsValid()) return;
	if (UAnimInstance* Anim = CachedEnemy->GetMesh()->GetAnimInstance())
	{
		Anim->Montage_JumpToSection(FName("Land"));
	}
}

bool UShockwallAttackExecution::ComputeLeapTarget(AEnemyCharacter* Enemy, FVector& OutTarget) const
{
	if (!Enemy) return false;

	const AActor* Target = Enemy->GetTargetActor();
	if (!Target) return false;

	UWorld* World = Enemy->GetWorld();
	if (!World) return false;

	const FVector PlayerPos = Target->GetActorLocation();
	UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(World);

	const float CapsuleRadius = Enemy->GetCapsuleComponent()
		? Enemy->GetCapsuleComponent()->GetScaledCapsuleRadius()
		: 50.0f;
	const float HalfHeight = Enemy->GetCapsuleComponent()
		? Enemy->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()
		: 100.0f;

	const int32 DirCount = FMath::Max(1, ShockwallRingDirectionCount);
	const float StepDeg = 360.0f / static_cast<float>(DirCount);
	// 開始角を毎回ランダムに回し、固定方位の学習を防ぐ
	const float BaseAngleDeg = FMath::FRandRange(0.0f, 360.0f);

	const FCollisionShape Capsule = FCollisionShape::MakeCapsule(CapsuleRadius, HalfHeight);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(ShockwallLandPoint), false, Enemy);

	const int32 MaxTries = FMath::Max(1, ShockwallLandPointMaxTries);
	for (int32 i = 0; i < MaxTries; ++i)
	{
		// 方位: 均等分割のいずれかをランダム選択し、開始角オフセットを足す
		const int32 DirIndex = FMath::RandRange(0, DirCount - 1);
		const float AngleRad = FMath::DegreesToRadians(BaseAngleDeg + StepDeg * static_cast<float>(DirIndex));

		// 距離帯: 候補配列からランダム選択(空ならフォールバック半径)
		const float Radius = (ShockwallRingRadii.Num() > 0)
			? ShockwallRingRadii[FMath::RandRange(0, ShockwallRingRadii.Num() - 1)]
			: ShockwallRingFallbackRadius;

		const FVector Offset = FVector(FMath::Cos(AngleRad), FMath::Sin(AngleRad), 0.0f) * Radius;
		FVector GroundPoint = PlayerPos + Offset;

		// Nav投影: ナビメッシュ上へ補正、範囲外なら棄却
		if (NavSys)
		{
			FNavLocation NavLoc;
			if (!NavSys->ProjectPointToNavigation(GroundPoint, NavLoc, FVector(100.0f, 100.0f, 300.0f)))
			{
				if (bDebugDrawLandPoint)
				{
					DrawDebugSphere(World, GroundPoint, 60.0f, 12, FColor::Red, false, 2.0f);
				}
				continue;
			}
			GroundPoint = NavLoc.Location;
		}

		// 着地カプセル中心 = 地面 + HalfHeight。ジオメトリへめり込まないかOverlapで確認
		const FVector CapsuleCenter = GroundPoint + FVector(0.0f, 0.0f, HalfHeight);
		const bool bBlocked = World->OverlapBlockingTestByChannel(
			CapsuleCenter, FQuat::Identity, ECC_Pawn, Capsule, Params);
		if (bBlocked)
		{
			if (bDebugDrawLandPoint)
			{
				DrawDebugCapsule(World, CapsuleCenter, HalfHeight, CapsuleRadius,
					FQuat::Identity, FColor::Red, false, 2.0f);
			}
			continue;
		}

		if (bDebugDrawLandPoint)
		{
			DrawDebugCapsule(World, CapsuleCenter, HalfHeight, CapsuleRadius,
				FQuat::Identity, FColor::Green, false, 2.0f);
		}

		OutTarget = CapsuleCenter;
		return true;
	}

	return false;
}

void UShockwallAttackExecution::BeginMineBurst(AEnemyCharacter* Enemy)
{
	if (!Enemy || !ShockwallMineClass) return;

	// 発動者HPが閾値以下のときだけ地雷を投射する (攻撃本体は通常どおり進行し、ここで投射のみ抑止)
	if (const UStatusComponent* Status = Enemy->GetStatusComponent())
	{
		const float MaxHP = Status->GetMaxHP();
		if (MaxHP > 0.0f && (Status->GetCurrentHP() / MaxHP) > ShockwallMineHPThreshold)
		{
			return;
		}
	}

	Enemy->GetWorldTimerManager().ClearTimer(ShockwallMineBurstTimerHandle);
	RemainingShockwallMineCount = FMath::Max(0, ShockwallMineCount);
	ShockwallMineSpawnedCount = 0;
	if (RemainingShockwallMineCount <= 0) return;

	CachedEnemy = Enemy;

	// Notifyフレームで1個目を即時投射し、その後intervalで継続する
	SpawnShockwallMine();
	if (RemainingShockwallMineCount <= 0) return;

	const float Interval = FMath::Max(0.001f, ShockwallMineInterval);
	Enemy->GetWorldTimerManager().SetTimer(
		ShockwallMineBurstTimerHandle,
		this,
		&UShockwallAttackExecution::SpawnShockwallMine,
		Interval,
		true);
}

void UShockwallAttackExecution::SpawnShockwallMine()
{
	if (!CachedEnemy.IsValid() || !ShockwallMineClass)
	{
		if (CachedEnemy.IsValid())
		{
			CachedEnemy->GetWorldTimerManager().ClearTimer(ShockwallMineBurstTimerHandle);
		}
		RemainingShockwallMineCount = 0;
		ShockwallMineSpawnedCount = 0;
		return;
	}

	AEnemyCharacter* Enemy = CachedEnemy.Get();
	if (!Enemy) return;

	if (RemainingShockwallMineCount <= 0)
	{
		Enemy->GetWorldTimerManager().ClearTimer(ShockwallMineBurstTimerHandle);
		ShockwallMineSpawnedCount = 0;
		return;
	}

	const float HalfHeight = Enemy->GetCapsuleComponent()
		? Enemy->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()
		: 0.0f;
	const float SpreadRadius = FMath::Max(0.0f, ShockwallMineSpawnSpreadRadius);
	const float SpawnAngleRad = FMath::DegreesToRadians(FMath::Fmod(137.5f * static_cast<float>(ShockwallMineSpawnedCount), 360.0f));
	const FVector SpawnOffset = FVector(FMath::Cos(SpawnAngleRad), FMath::Sin(SpawnAngleRad), 0.0f) * SpreadRadius;
	const FVector SpawnLoc = Enemy->GetActorLocation()
		+ SpawnOffset
		+ FVector(0.0f, 0.0f, HalfHeight + ShockwallMineSpawnHeightOffset);

	const float YawDeg = FMath::FRandRange(0.0f, 360.0f);
	const float PitchMin = FMath::Min(ShockwallMineLaunchPitchMinDeg, ShockwallMineLaunchPitchMaxDeg);
	const float PitchMax = FMath::Max(ShockwallMineLaunchPitchMinDeg, ShockwallMineLaunchPitchMaxDeg);
	const float PitchDeg = FMath::FRandRange(PitchMin, PitchMax);
	const FVector LaunchDir = FRotator(PitchDeg, YawDeg, 0.0f).Vector();

	const float SpeedMin = FMath::Min(ShockwallMineLaunchSpeedMin, ShockwallMineLaunchSpeedMax);
	const float SpeedMax = FMath::Max(ShockwallMineLaunchSpeedMin, ShockwallMineLaunchSpeedMax);
	const float LaunchSpeed = FMath::FRandRange(SpeedMin, SpeedMax);
	const FVector LaunchVelocity = LaunchDir * LaunchSpeed;

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = Enemy;

	ABossMineHazard* Mine = Enemy->GetWorld()->SpawnActor<ABossMineHazard>(
		ShockwallMineClass,
		FTransform(FRotationMatrix::MakeFromX(LaunchDir).Rotator(), SpawnLoc),
		SpawnParams);
	if (Mine)
	{
		Mine->SetLaunchVelocity(LaunchVelocity);
		Mine->SetImmuneThroughShockwaveWaveId(ABossShockwaveActor::PeekNextWaveId());
		const float AttackDamage = Enemy->GetCurrentAttackDamage();
		if (AttackDamage >= 0.0f)
		{
			Mine->SetExplosionDamage(AttackDamage);
		}
		TideCombatUtil::InjectAttackHitReactionTag(Enemy, Mine->HitReactionTag);
	}

	--RemainingShockwallMineCount;
	++ShockwallMineSpawnedCount;
	if (RemainingShockwallMineCount <= 0)
	{
		Enemy->GetWorldTimerManager().ClearTimer(ShockwallMineBurstTimerHandle);
		ShockwallMineSpawnedCount = 0;
	}
}

// ------------------------------------------------------------
// UChargeAttackExecution
// ------------------------------------------------------------

void UChargeAttackExecution::OnAttackEvent(AEnemyCharacter* Enemy, FGameplayTag EventTag)
{
	if (EventTag == TAG_AttackEvent_ChargeStart)
	{
		StartChargeVFX(Enemy);
	}
	else if (EventTag == TAG_AttackEvent_ChargeEnd)
	{
		StopChargeVFX();
	}
}

void UChargeAttackExecution::OnAttackEnd(AEnemyCharacter* Enemy)
{
	// ChargeEndが来ずに終了/中断した場合の保険
	StopChargeVFX();
}

void UChargeAttackExecution::StartChargeVFX(AEnemyCharacter* Enemy)
{
	if (!Enemy) return;
	if (!WindVFXComp.IsValid()) WindVFXComp = SpawnChargeVFX(Enemy, WindVFX);
	if (!DustVFXComp.IsValid()) DustVFXComp = SpawnChargeVFX(Enemy, DustVFX);
}

void UChargeAttackExecution::StopChargeVFX()
{
	// Deactivateで新規発生だけ止め、残存パーティクルは寿命で自然に消えるのに任せる
	// (Niagara側でディアクティベート時の消え方が調整済み)
	// bAutoDestroyで消滅後に自動破棄される
	if (WindVFXComp.IsValid()) WindVFXComp->Deactivate();
	if (DustVFXComp.IsValid()) DustVFXComp->Deactivate();
	WindVFXComp.Reset();
	DustVFXComp.Reset();
}

UNiagaraComponent* UChargeAttackExecution::SpawnChargeVFX(AEnemyCharacter* Enemy, const FChargeVFXEntry& Entry) const
{
	if (!Entry.System || !Enemy) return nullptr;

	// SocketName未指定はRoot (アクター向き＝進行方向基準)、
	// 指定時はメッシュのボーン/ソケットにアタッチ
	USceneComponent* Attach = Enemy->GetRootComponent();
	if (!Entry.SocketName.IsNone())
	{
		Attach = Enemy->GetMesh();
	}
	if (!Attach) return nullptr;

	UNiagaraComponent* NC = UNiagaraFunctionLibrary::SpawnSystemAttached(
		Entry.System, Attach, Entry.SocketName,
		Entry.Offset, Entry.Rotation,
		EAttachLocation::KeepRelativeOffset, /*bAutoDestroy=*/true);
	if (NC)
	{
		// TransformのスケールではなくNiagaraのUser floatパラメータ "Scale"
		// に流す
		NC->SetVariableFloat(FName("Scale"), Entry.Scale);
	}
	return NC;
}

bool UShockwallAttackExecution::CanActivate(const AEnemyCharacter* Enemy) const
{
	if (!Enemy) return true;

	// 両足とも足光輪が破壊されていたら発動しない (片足でも残っていれば発動する)
	if (const UPartDestructionComponent* PartComp = Enemy->FindComponentByClass<UPartDestructionComponent>())
	{
		if (PartComp->IsPartDestroyed(LeftFootPartTag) && PartComp->IsPartDestroyed(RightFootPartTag))
		{
			return false;
		}
	}
	return true;
}

// ------------------------------------------------------------
// UStompAttackExecution
// ------------------------------------------------------------

void UStompAttackExecution::OnAttackBegin(AEnemyCharacter* Enemy)
{
	CachedAnimInstance = Enemy->GetMesh()->GetAnimInstance();
	StompsPlayed = 0;
	bReplaying = false;

	if (!CachedAnimInstance.IsValid() || !RightStompMontage || !LeftStompMontage)
	{
		if (FinishDelegate) FinishDelegate(false);
		return;
	}

	// 足の光輪状態を確定する。踏みつけ自体は光輪が無くても行うので、これは衝撃波の可否だけに使う
	bLeftFootHaloIntact  = IsFootHaloIntact(Enemy, /*bRight=*/false);
	bRightFootHaloIntact = IsFootHaloIntact(Enemy, /*bRight=*/true);

	// 踏みつけ回数を抽選 (Min/Maxが逆転していても安全に丸める)
	const int32 MinCount = FMath::Max(1, FMath::Min(StompCountMin, StompCountMax));
	const int32 MaxCount = FMath::Max(MinCount, FMath::Max(StompCountMin, StompCountMax));
	TotalStomps = FMath::RandRange(MinCount, MaxCount);

	// モンタージュ終了通知は一度だけ登録し、最後の足が終わった時に攻撃完了とする
	CachedAnimInstance->OnMontageEnded.AddDynamic(this, &UStompAttackExecution::OnStompMontageEnded);

	// 開始足はランダム。先頭は頭出し再生 (StartTime = 0)
	StompsPlayed = 1;
	PlayStomp(FMath::RandBool(), 0.0f);
}

void UStompAttackExecution::OnAttackEvent(AEnemyCharacter* Enemy, FGameplayTag EventTag)
{
	// 踏み下ろし着地: 踏んだ足の光輪が残っていれば足元から衝撃波を発生させる
	if (EventTag == TAG_AttackEvent_StompImpact)
	{
		SpawnStompShockwave(Enemy);
		return;
	}

	if (EventTag != TAG_AttackEvent_StompConnect) return;

	// まだ踏む回数が残っていれば反対足へ連結する。残っていなければ現モンタージュを最後まで流す
	if (StompsPlayed >= TotalStomps) return;

	// 光輪の有無に関わらず反対足へ交互に連結する (壊れた足の番は衝撃波が出ないだけ)
	const bool bNextRight = !bCurrentFootRight;

	const float ConnectSec = (bNextRight ? RightConnectInFrame : LeftConnectInFrame)
		/ FMath::Max(1.0f, FrameRate);

	// 連結再生では旧モンタージュが中断される。その通知を自己中断として扱うためマーカーを立てる
	bReplaying = true;
	PlayStomp(bNextRight, ConnectSec);
	++StompsPlayed;
}

void UStompAttackExecution::OnAttackEnd(AEnemyCharacter* Enemy)
{
	if (CachedAnimInstance.IsValid())
	{
		CachedAnimInstance->OnMontageEnded.RemoveDynamic(this, &UStompAttackExecution::OnStompMontageEnded);
	}
}

void UStompAttackExecution::PlayStomp(bool bRight, float StartTime)
{
	UAnimMontage* Montage = bRight ? RightStompMontage : LeftStompMontage;
	if (!Montage || !CachedAnimInstance.IsValid()) return;

	bCurrentFootRight = bRight;

	// ActiveMontageをMontage_Playより先に更新する
	// 連結再生は同スロットの旧モンタージュを停止させOnStompMontageEnded
	// (旧,interrupted) を発火させる
	// これは連結側で立てたbReplayingマーカーにより「自己中断」として無視する
	// (同一モンタージュ連続再生・中断の非同期発火の双方に対応)
	ActiveMontage = Montage;
	CachedAnimInstance->Montage_Play(
		Montage, 1.0f, EMontagePlayReturnType::MontageLength, StartTime);
}

void UStompAttackExecution::SpawnStompShockwave(AEnemyCharacter* Enemy)
{
	if (!Enemy || !ShockwaveActorClass) return;

	// 踏み下ろした足に対応するソケットを引く
	// (bCurrentFootRightはPlayStompで更新済み)
	const FName FootSocket = bCurrentFootRight ? RightFootSocket : LeftFootSocket;

	// 踏みつけ自体は行うが、足の光輪が壊れていたら衝撃波は出さない
	// 技開始時に確定した状態を使い、途中で光輪の状態が変わっても技の中で分岐がぶれないようにする
	if (!(bCurrentFootRight ? bRightFootHaloIntact : bLeftFootHaloIntact)) return;

	const FVector SpawnLocation = Enemy->GetMesh()->GetSocketLocation(FootSocket);

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = Enemy;
	ABossShockwaveActor* Shockwave = Enemy->GetWorld()->SpawnActor<ABossShockwaveActor>(
		ShockwaveActorClass,
		FTransform(Enemy->GetActorRotation(), SpawnLocation),
		SpawnParams);
	if (Shockwave)
	{
		const float AttackDamage = Enemy->GetCurrentAttackDamage();
		if (AttackDamage >= 0.0f) Shockwave->SetDamage(AttackDamage);
		const FGameplayTag ReactionTag = Enemy->GetCurrentAttackHitReactionTag();
		if (ReactionTag.IsValid()) Shockwave->SetHitReactionTag(ReactionTag);
	}
}

void UStompAttackExecution::OnStompMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	if (bInterrupted)
	{
		// 連結再生に伴う自己中断は完了扱いにしない (次の足の再生が既に走っている)
		// 同一モンタージュ連続再生でもマーカーで判定できるので取りこぼさない
		if (bReplaying)
		{
			bReplaying = false;
			return;
		}
		// それ以外の中断 (外部からの停止) は現在の足の通知のみ失敗完了とする
		if (Montage == ActiveMontage.Get() && FinishDelegate) FinishDelegate(false);
		return;
	}

	// 自然終了: 現在の足のモンタージュが最後まで再生されたら攻撃完了
	if (Montage != ActiveMontage.Get()) return;

	if (CachedAnimInstance.IsValid())
	{
		CachedAnimInstance->OnMontageEnded.RemoveDynamic(this, &UStompAttackExecution::OnStompMontageEnded);
	}

	if (FinishDelegate) FinishDelegate(true);
}

bool UStompAttackExecution::IsFootHaloIntact(const AEnemyCharacter* Enemy, bool bRight) const
{
	if (!Enemy) return true;

	const FName FootTag = bRight ? RightFootPartTag : LeftFootPartTag;
	if (const UPartDestructionComponent* PartComp = Enemy->FindComponentByClass<UPartDestructionComponent>())
	{
		return !PartComp->IsPartDestroyed(FootTag);
	}
	return true; // 部位管理が無ければ常に光輪ありとして扱う
}

// ------------------------------------------------------------
// UScatterBombardAttackExecution
// ------------------------------------------------------------

void UScatterBombardAttackExecution::OnAttackBegin(AEnemyCharacter* Enemy)
{
	ShotFiredCount = 0;
	Enemy->GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
}

void UScatterBombardAttackExecution::OnAttackEnd(AEnemyCharacter* Enemy)
{
	Enemy->GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
}

void UScatterBombardAttackExecution::OnAttackEvent(AEnemyCharacter* Enemy, FGameplayTag EventTag)
{
	if (EventTag == TAG_AttackEvent_MissileStart)
	{
		ShotFiredCount = 0;
	}
	else if (EventTag == TAG_AttackEvent_MissileShot)
	{
		// 1発目の発射時点でプレイヤー位置とグリッド基準方向を確定する
		if (ShotFiredCount == 0)
		{
			AActor* Target = TideCombatUtil::GetTarget(Enemy);
			if (!Target) return;

			CachedPlayerPos = Target->GetActorLocation();
			CachedForward2D = (CachedPlayerPos - Enemy->GetActorLocation()).GetSafeNormal2D();
			if (CachedForward2D.IsNearlyZero())
			{
				CachedForward2D = Enemy->GetActorForwardVector().GetSafeNormal2D();
			}
			CachedRight2D = FVector::CrossProduct(FVector::UpVector, CachedForward2D).GetSafeNormal();
		}
		SpawnBombardPair(Enemy);
	}
}

void UScatterBombardAttackExecution::SpawnBombardPair(AEnemyCharacter* Enemy)
{
	if (!BombardProfile) return;

	UProjectilePoolSubsystem* Pool = Enemy->GetWorld()
		? Enemy->GetWorld()->GetSubsystem<UProjectilePoolSubsystem>() : nullptr;
	if (!Pool) return;

	const FVector EnemyPos = Enemy->GetActorLocation();
	const FVector SpawnBase = EnemyPos - Enemy->GetActorForwardVector() * SpawnBackOffset +
		FVector(0.0f, 0.0f, SpawnHeightOffset);
	const FRotator SpawnRot = Enemy->GetActorRotation();

	// 一番手前 (Shot 0) の着弾をPC位置に合わせ、以降は奥 (敵から離れる方向)
	// へ均等展開する。ただしPCが敵の足元回避半径 (MinDistFromEnemy) 内にいる場合は、
	// 基準点を半径外周へ押し出してから展開し、敵足元への着弾と隊形の潰れを防ぐ
	const FVector Anchor = ClampBombardLandingPos(CachedPlayerPos, EnemyPos, MinDistFromEnemy, CachedForward2D);

	const float NormalizedIndex = static_cast<float>(ShotFiredCount);
	const FVector RowOffset = CachedForward2D * NormalizedIndex * RowSpacing;

	const FVector LeftTarget = ClampBombardLandingPos(
		Anchor + RowOffset - CachedRight2D * SideOffset, EnemyPos, MinDistFromEnemy, CachedForward2D);
	const FVector RightTarget = ClampBombardLandingPos(
		Anchor + RowOffset + CachedRight2D * SideOffset, EnemyPos, MinDistFromEnemy, CachedForward2D);

	const FVector EnemyForward = Enemy->GetActorForwardVector().GetSafeNormal();
	FVector Back2D = (-EnemyForward).GetSafeNormal2D();
	if (Back2D.IsNearlyZero())
		{
		Back2D = (-CachedForward2D).GetSafeNormal2D();
			}

	const float PitchMin = FMath::Min(LaunchPitchMinDeg, LaunchPitchMaxDeg);
	const float PitchMax = FMath::Max(LaunchPitchMinDeg, LaunchPitchMaxDeg);
	const float PairPitch = bRandomizeLaunchDirection
		? FMath::FRandRange(PitchMin, PitchMax)
		: PitchMin;
	const float PairYawJitter = bRandomizeLaunchDirection
		? FMath::FRandRange(-FMath::Abs(LaunchYawJitterDeg), FMath::Abs(LaunchYawJitterDeg))
		: 0.0f;
	const float OutwardYawMag = bLaunchPerfectMirror
		? FMath::Max(0.0f, LaunchOutwardYawDeg)
		: FMath::Max(0.0f, LaunchOutwardYawDeg + PairYawJitter);

	auto SpawnOne = [&](const FVector& Target, float SideSign)
		{
			AEnemyProjectile* Proj = Pool->Acquire(
				BombardProfile, FTransform(SpawnRot, SpawnBase), Enemy, Enemy->GetInstigator());
			if (!Proj) return;

			UBezierArcBehavior* Bombard = Proj->FindBehavior<UBezierArcBehavior>();

			// ダメージはAttackExecution側の値を常に適用する
			TideCombatUtil::InjectAttackDamage(Enemy, Proj->Damage);
			if (Bombard) TideCombatUtil::InjectAttackDamage(Enemy, Bombard->SplashDamage);

			// 左右ペアで外側へ開くV字射出(左右対称)
			// 射出の左右は着弾側 (SideSign * Right2D) に合わせる
			// Right2Dで明示合成して交差を防ぐ
			FVector EnemyRight2D = FVector::CrossProduct(FVector::UpVector, EnemyForward).GetSafeNormal2D();
			if (EnemyRight2D.IsNearlyZero())
			{
				EnemyRight2D = CachedRight2D;
			}
			const float OutwardRad = FMath::DegreesToRadians(OutwardYawMag);
			FVector LaunchDir = Back2D * FMath::Cos(OutwardRad) + EnemyRight2D * SideSign * FMath::Sin(OutwardRad);
			LaunchDir.Z = FMath::Tan(FMath::DegreesToRadians(PairPitch));
			LaunchDir = LaunchDir.GetSafeNormal();

			const float KickDistance = FMath::Max(0.0f, LaunchKickDistance);
			FVector ControlPoint1 = SpawnBase + LaunchDir * KickDistance;

			const float BackComp = FVector::DotProduct(ControlPoint1 - EnemyPos, -EnemyForward);
			const float MinBackClearance = FMath::Max(0.0f, LaunchBackClearance);
			if (BackComp < MinBackClearance)
			{
				ControlPoint1 += (-EnemyForward) * (MinBackClearance - BackComp);
			}

			const FVector PathDelta = Target - SpawnBase;
			const FVector ControlPoint2 = SpawnBase + PathDelta * 0.60f;

			// 着弾点・初期方位・制御点を1発ごとに注入 ※飛行パラメータはプロファイル側
			if (Bombard)
			{
				Bombard->SetTargetLocation(Target);
				Bombard->SetInitialHeadingDirection(LaunchDir);
				Bombard->SetControlPoints(ControlPoint1, ControlPoint2);
			}

			Proj->ActivateProjectile();
		};

	SpawnOne(LeftTarget, -1.0f);
	SpawnOne(RightTarget, 1.0f);

	// 4way: 内側2列に加えて、外側へ扇状に広がる2列を追加する
	FVector LeftOuterTarget = FVector::ZeroVector;
	FVector RightOuterTarget = FVector::ZeroVector;
	if (bFourWay)
	{
		const float Rad = FMath::DegreesToRadians(FMath::Clamp(OuterFanAngleDeg, 0.0f, 89.0f));
		// 奥行きが増すほど外へ開く扇方向。左外側は -Right側、右外側は +Right側へ傾ける
		const FVector LeftOuterDir = (CachedForward2D * FMath::Cos(Rad) - CachedRight2D * FMath::Sin(Rad)).GetSafeNormal();
		const FVector RightOuterDir = (CachedForward2D * FMath::Cos(Rad) + CachedRight2D * FMath::Sin(Rad)).GetSafeNormal();
		const float OuterRowDist = NormalizedIndex * RowSpacing;

		LeftOuterTarget = ClampBombardLandingPos(
			Anchor - CachedRight2D * OuterSideOffset + LeftOuterDir * OuterRowDist, EnemyPos, MinDistFromEnemy, CachedForward2D);
		RightOuterTarget = ClampBombardLandingPos(
			Anchor + CachedRight2D * OuterSideOffset + RightOuterDir * OuterRowDist, EnemyPos, MinDistFromEnemy, CachedForward2D);

		SpawnOne(LeftOuterTarget, -1.0f);
		SpawnOne(RightOuterTarget, 1.0f);
	}

#if !UE_BUILD_SHIPPING
	if (UTideGameSettings::Get()->bDebugDrawBombardLanding)
	{
		const float Duration = 3.0f;
		const int32 ShotIndex = ShotFiredCount + 1;
		DrawDebugSphere(Enemy->GetWorld(), LeftTarget, 50.0f, 12, FColor::Yellow, false, Duration);
		DrawDebugSphere(Enemy->GetWorld(), RightTarget, 50.0f, 12, FColor::Orange, false, Duration);
		DrawDebugString(Enemy->GetWorld(), LeftTarget + FVector(0.0f, 0.0f, 70.0f),
			FString::Printf(TEXT("L%d"), ShotIndex), nullptr, FColor::Yellow, Duration);
		DrawDebugString(Enemy->GetWorld(), RightTarget + FVector(0.0f, 0.0f, 70.0f),
			FString::Printf(TEXT("R%d"), ShotIndex), nullptr, FColor::Orange, Duration);
		if (bFourWay)
		{
			DrawDebugSphere(Enemy->GetWorld(), LeftOuterTarget, 50.0f, 12, FColor::Red, false, Duration);
			DrawDebugSphere(Enemy->GetWorld(), RightOuterTarget, 50.0f, 12, FColor::Magenta, false, Duration);
			DrawDebugString(Enemy->GetWorld(), LeftOuterTarget + FVector(0.0f, 0.0f, 70.0f),
				FString::Printf(TEXT("LO%d"), ShotIndex), nullptr, FColor::Red, Duration);
			DrawDebugString(Enemy->GetWorld(), RightOuterTarget + FVector(0.0f, 0.0f, 70.0f),
				FString::Printf(TEXT("RO%d"), ShotIndex), nullptr, FColor::Magenta, Duration);
		}
	}
#endif

	++ShotFiredCount;
	if (ShotFiredCount >= ShotCount)
	{
		JumpMontageToEnd(Enemy);
	}
}

// ------------------------------------------------------------
// UArcBombardAttackExecution
// ------------------------------------------------------------

void UArcBombardAttackExecution::OnAttackBegin(AEnemyCharacter* Enemy)
{
	ShotFiredCount = 0;
	Enemy->GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
}

void UArcBombardAttackExecution::OnAttackEnd(AEnemyCharacter* Enemy)
{
	Enemy->GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
}

void UArcBombardAttackExecution::OnAttackEvent(AEnemyCharacter* Enemy, FGameplayTag EventTag)
{
	if (EventTag == TAG_AttackEvent_MissileStart)
	{
		ShotFiredCount = 0;
	}
	else if (EventTag == TAG_AttackEvent_MissileShot)
	{
		if (ShotFiredCount == 0)
		{
			AActor* Target = TideCombatUtil::GetTarget(Enemy);
			if (!Target) return;

			CachedPlayerPos = Target->GetActorLocation();
			CachedForward2D = (CachedPlayerPos - Enemy->GetActorLocation()).GetSafeNormal2D();
			if (CachedForward2D.IsNearlyZero())
			{
				CachedForward2D = Enemy->GetActorForwardVector().GetSafeNormal2D();
			}

			// 着弾半径 = 敵→PCの2D距離。MinDistFromEnemyを下限としてクランプ
			const float DistToPlayer = FVector::Dist2D(CachedPlayerPos, Enemy->GetActorLocation());
			CachedEffectiveRadius = FMath::Max(DistToPlayer, MinDistFromEnemy);

			// 初段発射時にPCの移動方向(velocity)から掃引の左右を確定する
			// PCが向かう側とは逆へ弧を掃引する(PCが右へ動くなら左から弧を描く)
			// 停止中(移動量ほぼ0)は右へ掃引
			const FVector PCVel2D = Target->GetVelocity().GetSafeNormal2D();
			const FVector ForwardRight2D = FVector::CrossProduct(FVector::UpVector, CachedForward2D).GetSafeNormal();
			if (PCVel2D.IsNearlyZero())
			{
				DynamicStepAngle = FMath::Abs(ArcStepAngle);
			}
			else
			{
				const float SideSign = FVector::DotProduct(PCVel2D, ForwardRight2D) >= 0.0f ? -1.0f : 1.0f;
				DynamicStepAngle = FMath::Abs(ArcStepAngle) * SideSign;
			}
		}
		SpawnBombard(Enemy);
	}
}

void UArcBombardAttackExecution::SpawnBombard(AEnemyCharacter* Enemy)
{
	if (!BombardProfile) return;

	UProjectilePoolSubsystem* Pool = Enemy->GetWorld()
		? Enemy->GetWorld()->GetSubsystem<UProjectilePoolSubsystem>() : nullptr;
	if (!Pool) return;

	const FVector EnemyPos = Enemy->GetActorLocation();
	const FVector SpawnBase = EnemyPos
		- Enemy->GetActorForwardVector() * SpawnBackOffset
		+ FVector(0.0f, 0.0f, SpawnHeightOffset);
	const FRotator SpawnRot = Enemy->GetActorRotation();

	// EM中心の円弧: 敵→PC方向を基準にShot i *
	// DynamicStepAngleずつ回転した方向へCachedEffectiveRadius
	// 着弾の広がりは弧のふくらみ(BulgeSign)とは逆向きに掃引するため符号を反転する
	const FVector ShotDir = CachedForward2D.RotateAngleAxis(
		ShotFiredCount * -DynamicStepAngle, FVector::UpVector);

	// 着弾Zは着弾XYの真下にある実際の床へ合わせる
	// CachedPlayerPos.ZはPCカプセル中心(地面より約半身高い)なので、
	// そのまま使うと着弾点が空中になり距離に関係なく空中で爆発してしまう。床トレースで地面Zを取る
	auto ProjectToGroundZ = [&](FVector& Pos)
		{
			FHitResult FloorHit;
			FCollisionQueryParams FloorParams;
			FloorParams.AddIgnoredActor(Enemy);
			const FVector TraceFrom = FVector(Pos.X, Pos.Y, CachedPlayerPos.Z + 300.0f);
			const FVector TraceTo = FVector(Pos.X, Pos.Y, CachedPlayerPos.Z - 3000.0f);
			if (Enemy->GetWorld()->LineTraceSingleByChannel(FloorHit, TraceFrom, TraceTo, ECC_WorldStatic, FloorParams))
			{
				Pos.Z = FloorHit.ImpactPoint.Z;
			}
			else
			{
				Pos.Z = CachedPlayerPos.Z;
			}
		};

	FVector InnerTarget = ClampBombardLandingPos(
		EnemyPos + ShotDir * CachedEffectiveRadius, EnemyPos, MinDistFromEnemy, CachedForward2D);
	ProjectToGroundZ(InnerTarget);

	FVector OuterTarget = ClampBombardLandingPos(
		EnemyPos + ShotDir * (CachedEffectiveRadius + FMath::Max(0.0f, OuterLandingOffset)),
		EnemyPos, MinDistFromEnemy, CachedForward2D);
	ProjectToGroundZ(OuterTarget);

	// 展開ルーチン用: 展開位置(EM周囲の円周上、
	// 1発ごとにDeployAngleStepDegずつ回す)と追尾対象
	// inner/outer両弾とも同じ展開位置へ集まってから各着弾点へ発射する
	// 展開の回転向き(+/-)は初段で確定した掃引方向DynamicStepAngleの符号に従う
	// これでDeployStartAngleDegの符号がPCの移動量から自動補正され、
	// 2発目以降の時計オフセット(DeployAngleStepDeg)も同じ向きへ揃う
	const float   DeploySideSign = (DynamicStepAngle >= 0.0f) ? 1.0f : -1.0f;
	const float   DeployAngle = DeploySideSign * (DeployStartAngleDeg + ShotFiredCount * DeployAngleStepDeg);
	const FVector DeployDir = CachedForward2D.RotateAngleAxis(DeployAngle, FVector::UpVector);
	float DeployFootZ = EnemyPos.Z;
	if (Enemy->GetCapsuleComponent())
	{
		DeployFootZ -= Enemy->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	}
	const FVector DeployPos = FVector(EnemyPos.X, EnemyPos.Y, DeployFootZ)
		+ DeployDir * DeployRadius + FVector(0.0f, 0.0f, DeployHeight);
	AActor* HomingTargetActor = TideCombatUtil::GetTarget(Enemy);

	auto SpawnProjectileAtTarget = [&](const FVector& Target)
		{
			AEnemyProjectile* Proj = Pool->Acquire(
				BombardProfile, FTransform(SpawnRot, SpawnBase), Enemy, Enemy->GetInstigator());
			if (!Proj) return;

			UArcPathBehavior* Bombard = Proj->FindBehavior<UArcPathBehavior>();

			TideCombatUtil::InjectAttackDamage(Enemy, Proj->Damage);
			if (Bombard) TideCombatUtil::InjectAttackDamage(Enemy, Bombard->SplashDamage);
			if (Bombard) Bombard->SetTargetLocation(Target);

			if (bUseDeployRoutine)
			{
				// 展開→待機→着弾点へ誘導→途中からホーミング
				if (Bombard)
				{
					Bombard->SetDeploy(DeployPos, DeployHoverDuration);
					if (HomingTargetActor)
					{
						Bombard->SetHomingHandoff(HomingTargetActor, HomingHandoffRatio, HomingTurnRateDegPerSec);
					}
				}

				// 展開位置を起点に、横へふくらむ山なり弧の制御点を与える(弧を描いて飛ばすため)
				const FVector DPathDelta = Target - DeployPos;
				const FVector DPathRight2D = FVector::CrossProduct(FVector::UpVector, DPathDelta.GetSafeNormal2D()).GetSafeNormal();
				const float   DBulgeSign = (DynamicStepAngle >= 0.0f) ? 1.0f : -1.0f;
				const FVector DSide = DPathRight2D * BombardArcSideOffset * DBulgeSign;
				const FVector DUp(0.0f, 0.0f, 1.0f);
				const FVector DCP1 = DeployPos + DPathDelta * (1.0f / 3.0f) + DSide + DUp * BombardArcHeight;
				const FVector DCP2 = DeployPos + DPathDelta * (2.0f / 3.0f) + DSide * 0.35f + DUp * (BombardArcHeight * 0.35f);
				if (Bombard) Bombard->SetControlPoints(DCP1, DCP2);
			}
			else
			{
				// 上方向ではなく左右(水平)にふくらむ山なり弧
				// 掃引方向側(DynamicStepAngle符号)へふくらませる
				const FVector PathDelta = Target - SpawnBase;
				const FVector PathRight2D = FVector::CrossProduct(FVector::UpVector, PathDelta.GetSafeNormal2D()).GetSafeNormal();
				const float   BulgeSign = (DynamicStepAngle >= 0.0f) ? 1.0f : -1.0f;
				const FVector Side = PathRight2D * BombardArcSideOffset * BulgeSign;
				const FVector Up(0.0f, 0.0f, 1.0f);
				const FVector ControlPoint1 = SpawnBase - PathDelta * (1.0f / 3.0f) + Side + Up * BombardArcHeight;
				const FVector ControlPoint2 = SpawnBase + PathDelta * (2.0f / 3.0f) + Side * 0.35f + Up * (BombardArcHeight * 0.35f);
				if (Bombard) Bombard->SetControlPoints(ControlPoint1, ControlPoint2);
			}

			Proj->ActivateProjectile();
		};

	SpawnProjectileAtTarget(InnerTarget);
	SpawnProjectileAtTarget(OuterTarget);

#if !UE_BUILD_SHIPPING
	if (UTideGameSettings::Get()->bDebugDrawBombardLanding)
	{
		const float Duration = 3.0f;
		DrawDebugSphere(Enemy->GetWorld(), InnerTarget, 50.0f, 12, FColor::Cyan, false, Duration);
		DrawDebugSphere(Enemy->GetWorld(), OuterTarget, 50.0f, 12, FColor::Orange, false, Duration);
		DrawDebugString(Enemy->GetWorld(), InnerTarget + FVector(0.0f, 0.0f, 70.0f),
			FString::Printf(TEXT("%d"), ShotFiredCount + 1), nullptr, FColor::Cyan, Duration);
		DrawDebugString(Enemy->GetWorld(), OuterTarget + FVector(0.0f, 0.0f, 70.0f),
			FString::Printf(TEXT("%d-Outer"), ShotFiredCount + 1), nullptr, FColor::Orange, Duration);
	}
#endif

	++ShotFiredCount;
	if (ShotFiredCount >= ShotCount)
	{
		JumpMontageToEnd(Enemy);
	}
}

// ------------------------------------------------------------
// UHaloLineAttackExecution
// ------------------------------------------------------------

void UHaloLineAttackExecution::OnAttackEvent(AEnemyCharacter* Enemy, FGameplayTag EventTag)
{
	if (EventTag != TAG_AttackEvent_HaloPlace) return;
	if (!HaloHazardClass) return;

	UWorld* World = Enemy->GetWorld();
	if (!World) return;

	// 腕の光輪を非表示にし、発射元位置を取得する (見つからなければボス位置を使う)
	FVector StartLoc = Enemy->GetActorLocation();
	TArray<USceneComponent*> Components;
	Enemy->GetComponents<USceneComponent>(Components);
	for (USceneComponent* Comp : Components)
	{
		if (Comp->GetFName() != HaloComponentName) continue;
		HaloComp = Comp;
		StartLoc = Comp->GetComponentLocation();
		Comp->SetVisibility(false, true);
		break;
	}

	const FVector Forward = Enemy->GetActorForwardVector().GetSafeNormal2D();
	const FVector Right = FVector::CrossProduct(FVector::UpVector, Forward).GetSafeNormal();

	// 着地面の高さ = ボスの足元
	float FootZ = Enemy->GetActorLocation().Z;
	if (Enemy->GetCapsuleComponent())
	{
		FootZ -= Enemy->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	}
	const FVector Center = FVector(Enemy->GetActorLocation().X, Enemy->GetActorLocation().Y, FootZ);

	// 左右2ラインのオフセット
	const float LineOffsets[2] = { -LineGap * 0.5f, LineGap * 0.5f };

	for (int32 Line = 0; Line < 2; ++Line)
	{
		for (int32 i = 0; i < CountPerLine; ++i)
		{
			FVector Target = Center
				+ Forward * (StartDistance + Spacing * i)
				+ Right * LineOffsets[Line];

			// 着地点を地形に合わせる。XY上空から下方向トレースして地面Zを採用する
			{
				const FVector TraceStart(Target.X, Target.Y, Target.Z + GroundTraceUpDistance);
				const FVector TraceEnd(Target.X, Target.Y, Target.Z - GroundTraceDownDistance);
				// bTraceComplex = true: 地面がコンプレックスコリジョン (UseComplex
				// as Simple) でも拾えるようにする
				FCollisionQueryParams TraceParams(FName(TEXT("HaloLineGround")), true, Enemy);
				FHitResult GroundHit;
				const bool bHit = World->LineTraceSingleByChannel(GroundHit, TraceStart, TraceEnd, ECC_Visibility, TraceParams);
				if (bHit)
				{
					Target.Z = GroundHit.ImpactPoint.Z + GroundOffset;
				}

				if (bDebugDrawGroundTrace)
				{
					DrawDebugLine(World, TraceStart, TraceEnd, bHit ? FColor::Green : FColor::Red, false, 8.0f, 0, 4.0f);
					if (bHit)
					{
						DrawDebugSphere(World, GroundHit.ImpactPoint, 40.0f, 12, FColor::Yellow, false, 8.0f);
					}
				}
			}

			// 穴の軸 (ローカル +X) をライン方向へ向け、リングを縦に立てる
			// PCはライン方向に進んで穴をくぐる
			const FRotator SpawnRot = FRotationMatrix::MakeFromX(Forward).Rotator();
			const FTransform SpawnTM(SpawnRot, StartLoc);
			AHaloHazard* Hazard = World->SpawnActorDeferred<AHaloHazard>(
				HaloHazardClass, SpawnTM, Enemy, Enemy->GetInstigator(),
				ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
			if (!Hazard) continue;

			TideCombatUtil::InjectAttackDamage(Enemy, Hazard->Damage);
			TideCombatUtil::InjectAttackHitReactionTag(Enemy, Hazard->HitReactionTag);

			// 手前から順番に落とす。同じ距離 (i) の左右ラインは同時に落ちる
			const int32 Order = bReverseDropOrder ? (CountPerLine - 1 - i) : i;
			const float StartDelay = static_cast<float>(Order) * DropInterval;

			Hazard->FinishSpawning(SpawnTM);
			Hazard->Deploy(StartLoc, Target, StartDelay);
		}
	}
}

void UHaloLineAttackExecution::OnAttackEnd(AEnemyCharacter* Enemy)
{
	// 配置した光輪はステージに残す。腕の光輪だけ復帰させる
	// 光輪が壊れている間は再生を破壊クール側に任せ、ここで表示を戻さない
	if (HaloComp.IsValid() && !Enemy->IsHaloAway())
	{
		HaloComp->SetVisibility(true, true);
		Enemy->RestoreHaloToStateSocket(HaloComponentName);
	}
}

// ------------------------------------------------------------
// UFlameDanceAttackExecution
// ------------------------------------------------------------

void UFlameDanceAttackExecution::OnAttackBegin(AEnemyCharacter* Enemy)
{
	CachedEnemy = Enemy;
	CachedAnimInstance = Enemy ? Enemy->GetMesh()->GetAnimInstance() : nullptr;
	CurrentPhase = EFlameDancePhase::None;
	SpawnedFireballs.Empty();

	PhaseTimer = 0.0f;
	ShootTimer = 0.0f;
	RotationToggleTimer = 0.0f;
	ShootingToggleTimer = 0.0f;
	bShootingActive = true;
	CurrentOrbitAngle = 0.0f;
	LeapStartLocation = FVector::ZeroVector;
	LeapTargetLocation = FVector::ZeroVector;
	LeapElapsed = 0.0f;
	bIsRotating = true;
	bHasSpawned = false;
	bLeaping = false;
	ActiveMontage = nullptr;
	FireballPlacements.Reset();

	if (!Enemy)
	{
		if (FinishDelegate) FinishDelegate(false);
		return;
	}

	PlaySequenceMontage(Enemy, Montage);
}

void UFlameDanceAttackExecution::OnAttackEvent(AEnemyCharacter* Enemy, FGameplayTag EventTag)
{
	if (EventTag == TAG_AttackEvent_ShockwallStart)
	{
		CachedEnemy = Enemy;
		LeapStartLocation = Enemy->GetActorLocation();
		// スポナーエリア内・Nav到達可能・PCから一定距離の着地点を抽選。失敗時はその場着地
		FVector LeapTarget;
		LeapTargetLocation = ComputeLeapTarget(Enemy, LeapTarget) ? LeapTarget : LeapStartLocation;
		LeapElapsed = 0.0f;
		bLeaping = true;

		if (UCharacterMovementComponent* Move = Enemy->GetCharacterMovement())
		{
			Move->StopMovementImmediately();
			Move->SetMovementMode(MOVE_Flying);
		}
		return;
	}

	if (EventTag == TAG_AttackEvent_ShockwallLand)
	{
		SpawnLandingShockwave(Enemy);
		SpawnLandBurst(Enemy);
		BeginSpawning(Enemy);
		Enemy->BeginArmamentSoftWindow();
		return;
	}

	if (EventTag == TAG_AttackEvent_MissileShot && !bHasSpawned)
	{
		BeginSpawning(Enemy);
	}
}

void UFlameDanceAttackExecution::OnAttackTick(AEnemyCharacter* Enemy, float DeltaTime)
{
	if (bLeaping)
	{
		LeapElapsed += DeltaTime;
		const float Duration = FMath::Max(0.05f, LeapAirDuration);
		const float Alpha = FMath::Clamp(LeapElapsed / Duration, 0.0f, 1.0f);

		FVector NewLoc = FMath::Lerp(LeapStartLocation, LeapTargetLocation, Alpha);
		const float Arc = 4.0f * LeapArcHeight * Alpha * (1.0f - Alpha);
		NewLoc.Z += Arc;

		Enemy->SetActorLocation(NewLoc, true);

		if (Alpha >= 1.0f)
		{
			bLeaping = false;
			if (UCharacterMovementComponent* Move = Enemy->GetCharacterMovement())
			{
				Move->SetMovementMode(MOVE_Walking);
			}
			JumpToLandSection();
		}
		return;
	}

	if (CurrentPhase == EFlameDancePhase::None) return;

	PhaseTimer += DeltaTime;

	if (CurrentPhase == EFlameDancePhase::Spawning)
	{
		// SpawnDurationは生成が開始されるまでの待機時間。経過後に固定秒数で一斉に拡大する
		const float ExpandDuration = 0.2f;
		const float Alpha = FMath::Clamp((PhaseTimer - SpawnDuration) / ExpandDuration, 0.0f, 1.0f);

		UpdateFireballTransforms(Alpha);

		// 待機 + 拡大が完了したら回転フェーズへ移行
		if (PhaseTimer >= SpawnDuration + ExpandDuration)
		{
			CurrentPhase = EFlameDancePhase::Orbiting;
			PhaseTimer = 0.0f;
		}
	}
	else if (CurrentPhase == EFlameDancePhase::Orbiting)
	{
		UpdateOrbit(DeltaTime);

		// 指定秒数経過で自動的に連射フェーズへ移行
		if (PhaseTimer >= OrbitDuration)
		{
			CurrentPhase = EFlameDancePhase::Shooting;
			PhaseTimer = 0.0f;
			ShootTimer = 0.0f;
			RotationToggleTimer = 0.0f;
			ShootingToggleTimer = 0.0f;
			bShootingActive = true; // 連射フェーズは発射から始める
			bIsRotating = !bStopRotationDuringShoot;
		}
	}
	else if (CurrentPhase == EFlameDancePhase::Shooting)
	{
		// 回転/停止のトグル制御
		if (bToggleRotation)
		{
			RotationToggleTimer += DeltaTime;
			if (RotationToggleTimer >= RotationToggleInterval)
			{
				bIsRotating = !bIsRotating;
				RotationToggleTimer = 0.0f;
			}
		}

		if (bIsRotating)
		{
			UpdateOrbit(DeltaTime);
		}

		// 発射/停止のトグル制御 (バババッ発射 -> 停止 -> 発射 ... を作る)
		if (bToggleShooting)
		{
			ShootingToggleTimer += DeltaTime;
			if (ShootingToggleTimer >= ShootingToggleInterval)
			{
				bShootingActive = !bShootingActive;
				ShootingToggleTimer = 0.0f;
			}
		}

		// 弾の発射 (発射状態のときだけ)
		if (bShootingActive)
		{
			ShootTimer += DeltaTime;
			if (ShootTimer >= ShootInterval)
			{
				ShootProjectiles(Enemy);
				ShootTimer = 0.0f;
			}
		}

		// 連射全体の指定時間経過で攻撃終了処理
		if (PhaseTimer >= ShootingDuration)
		{
			if (UAnimInstance* Anim = Enemy->GetMesh()->GetAnimInstance())
			{
				Anim->Montage_JumpToSection(FName("Ed"), Anim->GetCurrentActiveMontage());
			}
			// 火球は即消しせず縮小フェーズへ移行する
			CurrentPhase = EFlameDancePhase::Despawning;
			PhaseTimer = 0.0f;
		}
	}
	else if (CurrentPhase == EFlameDancePhase::Despawning)
	{
		// 回転を続けながら火球を縮小し、縮みきったら破棄する
		CurrentOrbitAngle += OrbitRotationSpeed * DeltaTime;
		CurrentOrbitAngle = FMath::Fmod(CurrentOrbitAngle, 360.0f);

		const float Alpha = FMath::Clamp(PhaseTimer / FMath::Max(0.001f, FireballDespawnDuration), 0.0f, 1.0f);
		UpdateFireballTransforms(1.0f - Alpha);

		if (PhaseTimer >= FireballDespawnDuration)
		{
			for (TWeakObjectPtr<AActor> Fireball : SpawnedFireballs)
			{
				if (Fireball.IsValid()) Fireball->Destroy();
			}
			SpawnedFireballs.Empty();
			FireballPlacements.Reset();
			CurrentPhase = EFlameDancePhase::None;
		}
	}
}

void UFlameDanceAttackExecution::OnFlameDanceMontageEnded(UAnimMontage* EndedMontage, bool bInterrupted)
{
	if (EndedMontage != ActiveMontage.Get()) return;

	if (CachedAnimInstance.IsValid())
	{
		CachedAnimInstance->OnMontageEnded.RemoveDynamic(this, &UFlameDanceAttackExecution::OnFlameDanceMontageEnded);
	}

	if (bInterrupted)
	{
		if (FinishDelegate) FinishDelegate(false);
		return;
	}

	if (FinishDelegate) FinishDelegate(true);
}

void UFlameDanceAttackExecution::PlaySequenceMontage(AEnemyCharacter* Enemy, UAnimMontage* InMontage)
{
	if (!Enemy || !InMontage)
	{
		if (FinishDelegate) FinishDelegate(false);
		return;
	}

	UAnimInstance* AnimInst = Enemy->GetMesh() ? Enemy->GetMesh()->GetAnimInstance() : nullptr;
	if (!AnimInst)
	{
		if (FinishDelegate) FinishDelegate(false);
		return;
	}

	CachedEnemy = Enemy;
	CachedAnimInstance = AnimInst;
	ActiveMontage = InMontage;

	AnimInst->OnMontageEnded.RemoveDynamic(this, &UFlameDanceAttackExecution::OnFlameDanceMontageEnded);
	AnimInst->OnMontageEnded.AddDynamic(this, &UFlameDanceAttackExecution::OnFlameDanceMontageEnded);
	AnimInst->Montage_Play(InMontage);
}

void UFlameDanceAttackExecution::JumpToLandSection()
{
	if (!CachedEnemy.IsValid()) return;
	if (UAnimInstance* Anim = CachedEnemy->GetMesh()->GetAnimInstance())
	{
		Anim->Montage_JumpToSection(FName("Land"));
	}
}

bool UFlameDanceAttackExecution::ComputeLeapTarget(AEnemyCharacter* Enemy, FVector& OutTarget) const
{
	if (!Enemy) return false;

	UWorld* World = Enemy->GetWorld();
	if (!World) return false;

	// スポナーが設定したエリア (InnerVolume中心/半径)
	// スポナー管理外なら半径0でその場着地へ
	const float AreaRadius = Enemy->GetPatrolRadius();
	if (AreaRadius <= 0.0f) return false;
	const FVector AreaCenter = Enemy->GetPatrolOrigin();

	// 外縁マージン分だけ内側に縮めた実効半径。きわきわ着地を防ぐ
	// マージンが半径以上ならエリア中心のみが候補になる (0クランプ)
	const float EffectiveRadius = FMath::Max(0.0f, AreaRadius - LeapAreaEdgeMargin);

	const AActor* Target = Enemy->GetTargetActor();
	const FVector PlayerPos = Target ? Target->GetActorLocation() : Enemy->GetActorLocation();

	UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(World);

	const float CapsuleRadius = Enemy->GetCapsuleComponent()
		? Enemy->GetCapsuleComponent()->GetScaledCapsuleRadius() : 50.0f;
	const float HalfHeight = Enemy->GetCapsuleComponent()
		? Enemy->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 100.0f;

	const FCollisionShape Capsule = FCollisionShape::MakeCapsule(CapsuleRadius, HalfHeight);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(FlameDanceLeapPoint), false, Enemy);

	// PCから離す目標距離の候補。PC中心のリング半径として使う。降順に試し (遠い距離を優先し、
	// エリアに収まらなければ近い距離へ緩める)。空または0以下は距離指定なし (エリア内一様)
	TArray<float> DistCandidates = MinLeapDistanceCandidates;
	if (DistCandidates.Num() == 0)
	{
		DistCandidates.Add(0.0f);
	}
	DistCandidates.Sort([](const float& A, const float& B) { return A > B; });

	const int32 DirTries = FMath::Max(1, LeapLandPointMaxTries);
	for (const float TargetDist : DistCandidates)
	{
		// 全周をまんべんなく試すため、均等分割＋ランダム開始角で方位を回す
		const float BaseAngleDeg = FMath::FRandRange(0.0f, 360.0f);
		for (int32 i = 0; i < DirTries; ++i)
		{
			const float AngleRad = FMath::DegreesToRadians(BaseAngleDeg + (360.0f / DirTries) * i);
			const FVector Dir(FMath::Cos(AngleRad), FMath::Sin(AngleRad), 0.0f);

			// 着地候補: 目標距離ありならPC中心のリング上 (着地距離 ≒ TargetDist)、
			// 0以下なら距離指定なしでエリア内一様
			FVector GroundPoint = (TargetDist > 0.0f)
				? PlayerPos + Dir * TargetDist
				: AreaCenter + Dir * (EffectiveRadius * FMath::Sqrt(FMath::FRand()));

			// スポナーエリア外の方位は棄却 (リングがエリアからはみ出す向きを弾く)
			if (FVector::Dist2D(GroundPoint, AreaCenter) > EffectiveRadius)
			{
				if (bDebugDrawLeapPoint) DrawDebugSphere(World, GroundPoint, 60.0f, 12, FColor::Orange, false, 2.0f);
				continue;
			}

			// Nav投影: ナビメッシュ上へ補正、外れたら棄却
			if (NavSys)
			{
				FNavLocation NavLoc;
				if (!NavSys->ProjectPointToNavigation(GroundPoint, NavLoc, FVector(150.0f, 150.0f, 400.0f)))
				{
					if (bDebugDrawLeapPoint) DrawDebugSphere(World, GroundPoint, 60.0f, 12, FColor::Red, false, 2.0f);
					continue;
				}
				GroundPoint = NavLoc.Location;
			}

			// 投影でエリア外へはみ出したら棄却
			if (FVector::Dist2D(GroundPoint, AreaCenter) > EffectiveRadius)
			{
				if (bDebugDrawLeapPoint) DrawDebugSphere(World, GroundPoint, 60.0f, 12, FColor::Red, false, 2.0f);
				continue;
			}

			// 着地カプセルがジオメトリへめり込まないか
			const FVector CapsuleCenter = GroundPoint + FVector(0.0f, 0.0f, HalfHeight);
			if (World->OverlapBlockingTestByChannel(CapsuleCenter, FQuat::Identity, ECC_Pawn, Capsule, Params))
			{
				if (bDebugDrawLeapPoint)
				{
					DrawDebugCapsule(World, CapsuleCenter, HalfHeight, CapsuleRadius, FQuat::Identity, FColor::Red, false, 2.0f);
				}
				continue;
			}

			if (bDebugDrawLeapPoint)
			{
				DrawDebugCapsule(World, CapsuleCenter, HalfHeight, CapsuleRadius, FQuat::Identity, FColor::Green, false, 2.0f);
			}
			OutTarget = CapsuleCenter;
			return true;
		}
	}

	return false;
}

void UFlameDanceAttackExecution::BeginSpawning(AEnemyCharacter* Enemy)
{
	if (!Enemy || bHasSpawned) return;

	CurrentPhase = EFlameDancePhase::Spawning;
	PhaseTimer = 0.0f;
	bHasSpawned = true;

	if (FireballClass && Enemy->GetWorld())
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = Enemy;

		// バリアントに応じた配置を構築し、その数だけ火球を生成する
		BuildFireballPlacements();
		for (int32 i = 0; i < FireballPlacements.Num(); ++i)
		{
			AActor* Fireball = Enemy->GetWorld()->SpawnActor<AActor>(
				FireballClass, Enemy->GetActorLocation(), FRotator::ZeroRotator, SpawnParams);
			if (Fireball)
			{
				Fireball->SetActorScale3D(FVector::ZeroVector);
				SpawnedFireballs.Add(Fireball);
			}
		}
	}
}

void UFlameDanceAttackExecution::SpawnLandingShockwave(AEnemyCharacter* Enemy) const
{
	if (!Enemy || !ShockwaveActorClass) return;

	FVector SpawnLocation = Enemy->GetActorLocation();
	SpawnLocation.Z -= Enemy->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = Enemy;
	ABossShockwaveActor* Shockwave = Enemy->GetWorld()->SpawnActor<ABossShockwaveActor>(
		ShockwaveActorClass,
		FTransform(Enemy->GetActorRotation(), SpawnLocation),
		SpawnParams);
	if (Shockwave)
	{
		const float AttackDamage = Enemy->GetCurrentAttackDamage();
		if (AttackDamage >= 0.0f) Shockwave->SetDamage(AttackDamage);
		const FGameplayTag ReactionTag = Enemy->GetCurrentAttackHitReactionTag();
		if (ReactionTag.IsValid()) Shockwave->SetHitReactionTag(ReactionTag);
	}
}

void UFlameDanceAttackExecution::SpawnLandBurst(AEnemyCharacter* Enemy)
{
	if (!LandBurstProfile || LandBurstCount <= 0 || !Enemy || !Enemy->GetWorld()) return;

	UProjectilePoolSubsystem* Pool = Enemy->GetWorld()->GetSubsystem<UProjectilePoolSubsystem>();
	if (!Pool) return;

	UWorld* World = Enemy->GetWorld();

	// ボス足元を基準に高さオフセットを足したスポーン中心 (層0の高さ。着弾点の基準にも使う)
	const float HalfHeight = Enemy->GetCapsuleComponent()
		? Enemy->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 0.0f;
	FVector GroundBase = Enemy->GetActorLocation();
	GroundBase.Z += -HalfHeight + LandBurstHeightOffset;

	// 着弾点トレース用の上端Z (ボス頭上)
	const float TraceTopZ = Enemy->GetActorLocation().Z + 1000.0f;

	// 360度を等分したリングを、層数ぶん高さ違いで縦に積む (着弾距離は全層共通)
	const int32 Layers = FMath::Max(1, LandBurstLayerCount);
	for (int32 LayerIndex = 0; LayerIndex < Layers; ++LayerIndex)
	{
		const float LayerZ = LayerIndex * LandBurstLayerHeightSpacing;

		for (int32 i = 0; i < LandBurstCount; ++i)
		{
			const float AngleRad = FMath::DegreesToRadians((360.0f / LandBurstCount) * i);
			const FVector Dir(FMath::Cos(AngleRad), FMath::Sin(AngleRad), 0.0f);

			const FVector SpawnLoc = GroundBase + Dir * LandBurstRadius + FVector(0.0f, 0.0f, LayerZ);
			const FRotator SpawnRot = Dir.Rotation(); // 外向き

			AEnemyProjectile* Proj = Pool->Acquire(
				LandBurstProfile, FTransform(SpawnRot, SpawnLoc), Enemy, Enemy->GetInstigator());
			if (!Proj) continue;

			TideCombatUtil::InjectAttackDamage(Enemy, Proj->Damage);

			// 山なり弾 (Ballistic) の場合は着弾点を注入する
			// 着弾点 = 外向きLandBurstDistance地点を地面へ下方向トレースした位置
			// (層によらず地面)。Ballisticが無ければMovement任せ (直進等)
			if (UBallisticBehavior* Ballistic = Proj->FindBehavior<UBallisticBehavior>())
			{
				FVector Target = GroundBase + Dir * LandBurstDistance;
				FHitResult FloorHit;
				FCollisionQueryParams FloorParams;
				FloorParams.AddIgnoredActor(Enemy);
				const FVector TraceFrom(Target.X, Target.Y, TraceTopZ);
				const FVector TraceTo(Target.X, Target.Y, GroundBase.Z - 3000.0f);
				if (World->LineTraceSingleByChannel(FloorHit, TraceFrom, TraceTo, ECC_WorldStatic, FloorParams))
				{
					Target.Z = FloorHit.ImpactPoint.Z;
				}

				// 姿勢は速度方向追従 (behavior側) のため初期方位の注入は不要
				Ballistic->SetTargetLocation(Target);
			}

			// 範囲ダメージはEffect (USplashDamageBehavior) が担当
			// 合成されていれば与ダメを注入する
			if (USplashDamageBehavior* Splash = Proj->FindBehavior<USplashDamageBehavior>())
			{
				TideCombatUtil::InjectAttackDamage(Enemy, Splash->SplashDamage);
			}

			Proj->ActivateProjectile();
		}
	}
}

void UFlameDanceAttackExecution::UpdateOrbit(float DeltaTime)
{
	if (!CachedEnemy.IsValid()) return;

	// 回転向きはOrbitRotationSpeedの符号で決まる (マイナスで逆回転)
	CurrentOrbitAngle += OrbitRotationSpeed * DeltaTime;
	CurrentOrbitAngle = FMath::Fmod(CurrentOrbitAngle, 360.0f);

	UpdateFireballTransforms(1.0f);
}

void UFlameDanceAttackExecution::BuildFireballPlacements()
{
	FireballPlacements.Reset();
	if (FireballCount <= 0) return;

	const int32 Layers = FMath::Max(1, LayerCount);
	const int32 Stride = FMath::Max(1, UpperLayerSlotStride);
	for (int32 Layer = 0; Layer < Layers; ++Layer)
	{
		for (int32 Slot = 0; Slot < FireballCount; ++Slot)
		{
			// 下段(Layer 0)は常にフルリング。上段はStride間隔で間引き、層ごとに1スロットずらす
			// 例Stride=2で 列が1層,2層,1層,2層 / Stride=1で全層フルリング
			if (Layer >= 1 && ((Slot + Layer) % Stride) != 0)
			{
				continue;
			}
			FireballPlacements.Add(FIntPoint(Slot, Layer));
		}
	}
}

void UFlameDanceAttackExecution::UpdateFireballTransforms(float SpawnScale)
{
	if (!CachedEnemy.IsValid() || FireballCount <= 0) return;

	const FVector Base = CachedEnemy->GetActorLocation();

	const int32 Num = FMath::Min(SpawnedFireballs.Num(), FireballPlacements.Num());
	for (int32 i = 0; i < Num; ++i)
	{
		AActor* Fireball = SpawnedFireballs[i].Get();
		if (!Fireball) continue;

		const int32 Slot = FireballPlacements[i].X;
		const int32 Layer = FireballPlacements[i].Y;

		const float AngleDeg = CurrentOrbitAngle + (360.0f / FireballCount) * Slot;
		const float AngleRad = FMath::DegreesToRadians(AngleDeg);

		const FVector Offset = FVector(FMath::Cos(AngleRad), FMath::Sin(AngleRad), 0.0f) * OrbitRadius;
		const FVector Pos = Base + Offset + FVector(0.0f, 0.0f, OrbitHeightOffset + Layer * LayerHeightSpacing);

		Fireball->SetActorLocation(Pos);
		Fireball->SetActorScale3D(FVector(SpawnScale));
	}
}

void UFlameDanceAttackExecution::ShootProjectiles(AEnemyCharacter* Enemy)
{
	if (!Profile || !Enemy->GetWorld()) return;

	UProjectilePoolSubsystem* Pool = Enemy->GetWorld()->GetSubsystem<UProjectilePoolSubsystem>();
	if (!Pool) return;

	for (int32 i = 0; i < SpawnedFireballs.Num(); ++i)
	{
		if (AActor* Fireball = SpawnedFireballs[i].Get())
		{
			const FVector SpawnLoc = Fireball->GetActorLocation();
			const FVector Dir = (SpawnLoc - Enemy->GetActorLocation()).GetSafeNormal2D();
			const FRotator SpawnRot = Dir.Rotation();

			AEnemyProjectile* Proj = Pool->Acquire(
				Profile, FTransform(SpawnRot, SpawnLoc), Enemy, Enemy->GetInstigator());

			DrawDebugSphere(GetWorld(), SpawnLoc, 20.0f, 12, FColor::Red);

			if (Proj)
			{
				TideCombatUtil::InjectAttackDamage(Enemy, Proj->Damage);
				Proj->ActivateProjectile();
			}
		}
	}
}

void UFlameDanceAttackExecution::OnAttackEnd(AEnemyCharacter* Enemy)
{
	if (Enemy)
	{
		// 殴れる窓を閉じる (再硬化)。リアクション再生中なら終了まで遅延される
		Enemy->EndArmamentSoftWindow();
	}

	if (CachedAnimInstance.IsValid())
	{
		CachedAnimInstance->OnMontageEnded.RemoveDynamic(this, &UFlameDanceAttackExecution::OnFlameDanceMontageEnded);
	}

	if (bLeaping)
	{
		bLeaping = false;
		if (Enemy)
		{
			if (UCharacterMovementComponent* Move = Enemy->GetCharacterMovement())
			{
				Move->SetMovementMode(MOVE_Walking);
			}
		}
	}

	for (TWeakObjectPtr<AActor> Fireball : SpawnedFireballs)
	{
		if (Fireball.IsValid())
		{
			Fireball->Destroy();
		}
	}
	SpawnedFireballs.Empty();
	FireballPlacements.Reset();
	CurrentPhase = EFlameDancePhase::None;
}

// ------------------------------------------------------------
// UIronRainAttackExecution
// ------------------------------------------------------------

void UIronRainAttackExecution::OnAttackBegin(AEnemyCharacter* Enemy)
{
	CachedEnemy = Enemy;
	CurrentPhase = EIronRainPhase::None;
	PhaseTimer = 0.0f;
	TimeSinceLastDrop = 0.0f;
	DroppedCount = 0;
	DroppedLayers = 0;
	TotalDropCount = 0;
	LaunchedCount = 0;
	DropBaseAngleDeg = FMath::FRandRange(0.0f, 360.0f);
	TimeSinceLastLaunch = 0.0f;
	bHasStarted = false;
	PendingDrops.Empty();
	DropCountsPerLayer.Reset();

	const int32 LayerCount = FMath::Max(1, DropLayerCount);
	const float TargetSpacing = FMath::Max(1.0f, DropTargetSpacing);
	DropCountsPerLayer.Reserve(LayerCount);
	for (int32 LayerIndex = 0; LayerIndex < LayerCount; ++LayerIndex)
	{
		const float LayerAlpha = (LayerCount > 1)
			? static_cast<float>(LayerIndex) / static_cast<float>(LayerCount - 1)
			: 0.5f;
		const float Radius = FMath::Lerp(MinDropRadius, MaxDropRadius, LayerAlpha);
		const float Circumference = 2.0f * PI * FMath::Max(0.0f, Radius);
		const int32 CountInLayer = FMath::Max(1, FMath::RoundToInt(Circumference / TargetSpacing));

		DropCountsPerLayer.Add(CountInLayer);
		TotalDropCount += CountInLayer;
	}

	LaunchInterval = (LaunchMissileCount > 0) ? (LaunchDuration / LaunchMissileCount) : 0.0f;

	// 落下の刻みは進行単位で決める外側/内側は層単位(層数で割る) ランダムは1点単位(総数で割る)
	const int32 RainSteps = (DropPattern == 2)
		? TotalDropCount
		: FMath::Max(1, DropCountsPerLayer.Num());
	RainInterval = (RainSteps > 0) ? (RainDuration / static_cast<float>(RainSteps)) : 0.0f;
}

void UIronRainAttackExecution::OnAttackEvent(AEnemyCharacter* Enemy, FGameplayTag EventTag)
{
	if (EventTag == TAG_AttackEvent_MissileShot && !bHasStarted)
	{
		CurrentPhase = EIronRainPhase::Spawning;
		PhaseTimer = 0.0f;
		bHasStarted = true;

		// 殴れる窓を開く (硬化解除)閉じるのはOnAttackEnd
		if (Enemy)
		{
			Enemy->BeginArmamentSoftWindow();
		}
	}
}

void UIronRainAttackExecution::OnAttackTick(AEnemyCharacter* Enemy, float DeltaTime)
{
	for (int32 i = PendingDrops.Num() - 1; i >= 0; --i)
	{
		PendingDrops[i].RemainingTime -= DeltaTime;

		// 着弾までの進行度0->1をデカールマテリアルへ流す
		// 縦長ミサイルの先端が地面より手前で接触して見えるので
		// Progress=1を補正距離ぶん早める
		float EffectiveTotal = PendingDrops[i].TotalTime;
		if (FallProfile && FallProfile->InitialSpeed > 0.0f)
		{
			const float LeadTime = DropDecalImpactLeadDistance / FallProfile->InitialSpeed;
			EffectiveTotal = FMath::Max(0.0001f, PendingDrops[i].TotalTime - LeadTime);
		}
		const float Progress = FMath::Clamp(
			(PendingDrops[i].TotalTime - PendingDrops[i].RemainingTime) / EffectiveTotal, 0.0f, 1.0f);
		if (UMaterialInstanceDynamic* MID = PendingDrops[i].DecalMID.Get())
		{
			MID->SetScalarParameterValue(DropDecalProgressParam, Progress);
		}

		if (PendingDrops[i].RemainingTime <= 0.0f)
		{
			// 着弾デカールはLifeSpanでも消えるが即時破棄する
			if (UDecalComponent* Decal = PendingDrops[i].Decal.Get())
			{
				Decal->DestroyComponent();
			}
			PendingDrops.RemoveAt(i);
		}
	}

	if (CurrentPhase == EIronRainPhase::None) return;
	PhaseTimer += DeltaTime;

	if (CurrentPhase == EIronRainPhase::Spawning)
	{
		if (PhaseTimer >= SpawnDuration)
		{
			CurrentPhase = EIronRainPhase::Launching;
			PhaseTimer = 0.0f;
			TimeSinceLastLaunch = LaunchInterval;
		}
	}
	else if (CurrentPhase == EIronRainPhase::Launching)
	{
		ProcessLaunching(Enemy, DeltaTime);

		if (LaunchedCount >= LaunchMissileCount)
		{
			CurrentPhase = EIronRainPhase::Raining;
			PhaseTimer = 0.0f;
			TimeSinceLastDrop = RainInterval;
		}
	}
	else if (CurrentPhase == EIronRainPhase::Raining)
	{
		ProcessRaining(Enemy, DeltaTime);

		if (DroppedCount >= TotalDropCount && PendingDrops.IsEmpty())
		{
			JumpMontageToEnd(Enemy);
			CurrentPhase = EIronRainPhase::None;
		}
	}
}

void UIronRainAttackExecution::SpawnLaunchProjectiles(AEnemyCharacter* Enemy)
{
	if (!LaunchProfile || !Enemy->GetWorld()) return;

	UProjectilePoolSubsystem* Pool = Enemy->GetWorld()->GetSubsystem<UProjectilePoolSubsystem>();
	if (Pool)
	{
		const FTransform SpawnTM(FRotator(90.0f, 0.0f, 0.0f),
			Enemy->GetActorLocation() + FVector(0.0f, 0.0f, 100.0f));
		if (AEnemyProjectile* Proj = Pool->Acquire(
			LaunchProfile, SpawnTM, Enemy, Enemy->GetInstigator()))
		{
			Proj->ActivateProjectile();
		}
	}
}

void UIronRainAttackExecution::ProcessLaunching(AEnemyCharacter* Enemy, float DeltaTime)
{
	if (LaunchedCount >= LaunchMissileCount) return;

	TimeSinceLastLaunch += DeltaTime;
	while (TimeSinceLastLaunch >= LaunchInterval && LaunchedCount < LaunchMissileCount)
	{
		TimeSinceLastLaunch -= LaunchInterval;

		if (LaunchProfile && Enemy->GetWorld())
		{
			if (UProjectilePoolSubsystem* Pool =
				Enemy->GetWorld()->GetSubsystem<UProjectilePoolSubsystem>())
			{
				// 上方向を基準としたコーン内に連射するイメージ
				FVector LaunchDir = FMath::VRandCone(
					FVector::UpVector, FMath::DegreesToRadians(LaunchConeHalfAngle));

				FVector SpawnLoc = Enemy->GetActorLocation() + FVector(0.0f, 0.0f, 100.0f);
				FRotator SpawnRot = LaunchDir.Rotation();

				if (AEnemyProjectile* Proj = Pool->Acquire(
					LaunchProfile, FTransform(SpawnRot, SpawnLoc), Enemy, Enemy->GetInstigator()))
				{
					Proj->ActivateProjectile();
				}
			}
		}
		LaunchedCount++;
	}
}

void UIronRainAttackExecution::ProcessRaining(AEnemyCharacter* Enemy, float DeltaTime)
{
	if (DroppedCount >= TotalDropCount) return;

	TimeSinceLastDrop += DeltaTime;

	if (DropPattern == 2)
	{
		// ランダム: 1点ずつ落とす
		while (TimeSinceLastDrop >= RainInterval && DroppedCount < TotalDropCount)
		{
			TimeSinceLastDrop -= RainInterval;

			const float Radius = FMath::Sqrt(FMath::FRand() * (FMath::Square(MaxDropRadius) -
				FMath::Square(MinDropRadius)) + FMath::Square(MinDropRadius));
			const float Angle = FMath::FRandRange(0.0f, 360.0f);
			DropAt(Enemy, Angle, Radius);
		}
		return;
	}

	// 外側(0)/内側(1): 1層(同心円リング)をまとめて落としてから次の層へ進む
	const int32 LayerNum = DropCountsPerLayer.Num();
	while (TimeSinceLastDrop >= RainInterval && DroppedLayers < LayerNum)
	{
		TimeSinceLastDrop -= RainInterval;

		// 外側へ広がる=0..N-1 / 内側へ狭まる=N-1..0
		const int32 LayerIndex = (DropPattern == 1) ? (LayerNum - 1 - DroppedLayers) : DroppedLayers;
		DropLayer(Enemy, LayerIndex);
		DroppedLayers++;
	}
}

void UIronRainAttackExecution::DropLayer(AEnemyCharacter* Enemy, int32 LayerIndex)
{
	if (!DropCountsPerLayer.IsValidIndex(LayerIndex)) return;

	const int32 LayerNum = FMath::Max(1, DropCountsPerLayer.Num());
	const float LayerAlpha = (LayerNum > 1)
		? static_cast<float>(LayerIndex) / static_cast<float>(LayerNum - 1)
		: 0.5f;
	const float Radius = FMath::Lerp(MinDropRadius, MaxDropRadius, LayerAlpha);

	const int32 CountInLayer = DropCountsPerLayer[LayerIndex];
	const float AngleStep = (CountInLayer > 0) ? 360.0f / static_cast<float>(CountInLayer) : 360.0f;
	// 隣接層で落下点が重ならないよう奇数層は半ステップずらす
	const float LayerOffset = (LayerIndex % 2 == 0) ? 0.0f : AngleStep * 0.5f;

	for (int32 i = 0; i < CountInLayer; ++i)
	{
		const float Angle = DropBaseAngleDeg + LayerOffset + AngleStep * static_cast<float>(i);
		DropAt(Enemy, Angle, Radius);
	}
}

void UIronRainAttackExecution::DropAt(AEnemyCharacter* Enemy, float AngleDeg, float Radius)
{
	const FVector Center = Enemy->GetActorLocation();

	FVector DropLoc = Center + FVector(FMath::Cos(FMath::DegreesToRadians(AngleDeg)),
		FMath::Sin(FMath::DegreesToRadians(AngleDeg)), 0.0f) * Radius;

	FHitResult FloorHit;
	FCollisionQueryParams FloorParams;
	FloorParams.AddIgnoredActor(Enemy);

	const FVector TraceFrom(DropLoc.X, DropLoc.Y, Center.Z + 1000.0f);
	const FVector TraceTo(DropLoc.X, DropLoc.Y, Center.Z - 3000.0f);

	if (Enemy->GetWorld()->LineTraceSingleByChannel(
		FloorHit, TraceFrom, TraceTo, ECC_WorldStatic, FloorParams))
	{
		DropLoc.Z = FloorHit.ImpactPoint.Z;
	}
	else
	{
		DropLoc.Z = Center.Z;
	}

	ScheduleDrop(Enemy, DropLoc);
	DroppedCount++;
}

void UIronRainAttackExecution::ScheduleDrop(AEnemyCharacter* Enemy, const FVector& DropLoc)
{
	float CalculatedDelay = DropDelay;

	if (FallProfile && FallProfile->InitialSpeed > 0.0f)
	{
		const float SpawnZ = Enemy->GetActorLocation().Z + DropHeight;
		const float FallDistance = FMath::Max(0.0f, SpawnZ - DropLoc.Z);
		CalculatedDelay = FallDistance / FallProfile->InitialSpeed;
	}

	FPendingDrop Pending;
	Pending.Location = DropLoc;
	Pending.RemainingTime = CalculatedDelay;
	Pending.TotalTime = FMath::Max(0.0001f, CalculatedDelay);

	// 落下予告デカールを生成Material側で進行度スカラー(0->1)で明度を変化
	if (DropDecalMaterial)
	{
		if (UWorld* World = Enemy->GetWorld())
		{
			if (UDecalComponent* Decal = UGameplayStatics::SpawnDecalAtLocation(
				World, DropDecalMaterial, DropDecalSize, DropLoc,
				FRotator(-90.0f, 0.0f, 0.0f), CalculatedDelay))
			{
				if (UMaterialInstanceDynamic* MID = Decal->CreateDynamicMaterialInstance())
				{
					MID->SetScalarParameterValue(DropDecalProgressParam, 0.0f);
					Pending.DecalMID = MID;
				}
				Pending.Decal = Decal;
			}
		}
	}

	PendingDrops.Add(Pending);
	ExecuteDrop(DropLoc);
}

void UIronRainAttackExecution::ExecuteDrop(FVector DropLoc)
{
	if (!CachedEnemy.IsValid() || !FallProfile) return;
	AEnemyCharacter* Enemy = CachedEnemy.Get();
	UWorld* World = Enemy->GetWorld();
	if (!World) return;

	UProjectilePoolSubsystem* Pool = World->GetSubsystem<UProjectilePoolSubsystem>();
	if (!Pool) return;

	FVector SpawnLoc = DropLoc + FVector(0.0f, 0.0f, DropHeight);
	FRotator SpawnRot = FRotator(-90.0f, 0.0f, 0.0f);

	if (AEnemyProjectile* Proj = Pool->Acquire(
		FallProfile, FTransform(SpawnRot, SpawnLoc), Enemy, Enemy->GetInstigator()))
	{
		TideCombatUtil::InjectAttackDamage(Enemy, Proj->Damage);
		Proj->ActivateProjectile();
	}
}

void UIronRainAttackExecution::OnAttackEnd(AEnemyCharacter* Enemy)
{
	if (Enemy)
	{
		// 再硬化
		Enemy->EndArmamentSoftWindow();
	}

	// 攻撃中断時に残っている予告デカールを破棄する
	for (FPendingDrop& Pending : PendingDrops)
	{
		if (UDecalComponent* Decal = Pending.Decal.Get())
		{
			Decal->DestroyComponent();
		}
	}

	CurrentPhase = EIronRainPhase::None;
	PendingDrops.Empty();
}

// ------------------------------------------------------------
// UCircusBarrageAttackExecution
// ------------------------------------------------------------

void UCircusBarrageAttackExecution::OnAttackBegin(AEnemyCharacter* Enemy)
{
	FiredWaveCount = 0;
	ClearImpactDecals();

	BaseAngleDeg = bRandomizeBaseAngle
		? FMath::FRandRange(0.0f, 360.0f)
		: (Enemy ? Enemy->GetActorRotation().Yaw : 0.0f);

	Enemy->GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);

	// ダウン明けの反撃用: 復帰中の光輪発光＋接触ダメージ窓を開く
	// (ミニオンのSetRecoveryArmor相当)
	if (bEnableRecoveryTouchDamage)
	{
		if (UPartDestructionComponent* PartComp = Enemy->FindComponentByClass<UPartDestructionComponent>())
		{
			PartComp->BeginPartHaloRecoveryDamage();
		}
	}
}

void UCircusBarrageAttackExecution::OnAttackEvent(AEnemyCharacter* Enemy, FGameplayTag EventTag)
{
	if (!Enemy) return;

	if (EventTag == TAG_AttackEvent_MissileStart)
	{
		FiredWaveCount = 0;

		if (bOpenArmamentSoftWindow)
		{
			// 殴れる窓を開く (閉じるのはOnAttackEnd)
			Enemy->BeginArmamentSoftWindow();
		}
	}
	else if (EventTag == TAG_AttackEvent_MissileShot)
	{
		FireWave(Enemy, FiredWaveCount);

		++FiredWaveCount;
		if (FiredWaveCount >= WaveCount)
		{
			JumpMontageToEnd(Enemy);
		}
	}
}

void UCircusBarrageAttackExecution::OnAttackTick(AEnemyCharacter* Enemy, float DeltaTime)
{
	UpdateImpactDecals(DeltaTime);
}

float UCircusBarrageAttackExecution::GetWaveRadius(int32 WaveIndex) const
{
	if (WaveRadii.IsEmpty())
	{
		return FMath::Max(0.0f, WaveRadiusStep) * (WaveIndex + 1);
	}
	if (WaveRadii.IsValidIndex(WaveIndex))
	{
		return WaveRadii[WaveIndex];
	}

	// 波数に対して半径の指定が足りないぶんは最後の値から等差で伸ばす
	const int32 Overflow = WaveIndex - (WaveRadii.Num() - 1);
	return WaveRadii.Last() + FMath::Max(0.0f, WaveRadiusStep) * Overflow;
}

int32 UCircusBarrageAttackExecution::GetWaveShotCount(int32 WaveIndex) const
{
	if (ShotsPerWave.IsEmpty())
	{
		return 8;
	}
	// 指定が足りない波は最後の弾数を使い続ける (半径と違い等差で増やす根拠がないため)
	const int32 Index = FMath::Min(WaveIndex, ShotsPerWave.Num() - 1);
	return FMath::Max(1, ShotsPerWave[Index]);
}

FVector UCircusShotAttackExecutionBase::ProjectToGround(const AEnemyCharacter* Enemy, const FVector& Location) const
{
	// 着弾点を空中に置くと地形に当たらず、炸裂が寿命切れの保険までずれ込む
	FVector Grounded = Location;

	FHitResult FloorHit;
	FCollisionQueryParams FloorParams;
	FloorParams.AddIgnoredActor(Enemy);

	const FVector TraceFrom(Location.X, Location.Y, Location.Z + 1000.0f);
	const FVector TraceTo(Location.X, Location.Y, Location.Z - 4000.0f);
	if (Enemy->GetWorld()->LineTraceSingleByChannel(FloorHit, TraceFrom, TraceTo, ECC_WorldStatic, FloorParams))
	{
		Grounded.Z = FloorHit.ImpactPoint.Z;
	}
	return Grounded;
}

void UCircusBarrageAttackExecution::FireWave(AEnemyCharacter* Enemy, int32 WaveIndex)
{
	if (!CircusProfile || !Enemy || !Enemy->GetWorld()) return;

	const int32 Count = GetWaveShotCount(WaveIndex);
	const float AngleStep = 360.0f / static_cast<float>(Count);
	const float WaveAngle = BaseAngleDeg + WaveAngleStepDeg * WaveIndex;
	const float Radius = GetWaveRadius(WaveIndex);

	const FVector EnemyPos = Enemy->GetActorLocation();
	const FVector SpawnLoc = GetMuzzleLocation(Enemy);

	for (int32 i = 0; i < Count; ++i)
	{
		const float ShotAngle = WaveAngle + AngleStep * static_cast<float>(i);
		const FVector ShotDir = FVector::ForwardVector.RotateAngleAxis(ShotAngle, FVector::UpVector);

		const FVector Target = ProjectToGround(Enemy, EnemyPos + ShotDir * Radius);

		const float LeadTime = SpawnCircusShot(Enemy, SpawnLoc, Target);
		AddImpactDecal(Enemy, Target, LeadTime);
		DebugDrawImpact(Enemy, Target);
	}
}

// ------------------------------------------------------------
// UCircusShotAttackExecutionBase
// ------------------------------------------------------------

FVector UCircusShotAttackExecutionBase::GetMuzzleLocation(const AEnemyCharacter* Enemy) const
{
	if (!MuzzleSocketName.IsNone() && Enemy->GetMesh() && Enemy->GetMesh()->DoesSocketExist(MuzzleSocketName))
	{
		return Enemy->GetMesh()->GetSocketLocation(MuzzleSocketName);
	}
	return Enemy->GetActorLocation()
		- Enemy->GetActorForwardVector() * SpawnBackOffset
		+ FVector(0.0f, 0.0f, SpawnHeightOffset);
}

float UCircusShotAttackExecutionBase::SpawnCircusShot(AEnemyCharacter* Enemy, const FVector& SpawnLoc,
	const FVector& Target)
{
	UProjectilePoolSubsystem* Pool = Enemy->GetWorld()->GetSubsystem<UProjectilePoolSubsystem>();
	if (!Pool) return FlightDuration;

	// 発射位置から着弾点への水平方向を基準に、その直交方向を横ずらしの軸にする
	// 真下に落とす場合は水平成分が消えるので、そのときはアクターの右方向で代用する
	FVector ShotDir = (Target - SpawnLoc).GetSafeNormal2D();
	if (ShotDir.IsNearlyZero())
	{
		ShotDir = Enemy->GetActorForwardVector().GetSafeNormal2D();
	}
	const FVector Tangent = FVector::CrossProduct(FVector::UpVector, ShotDir).GetSafeNormal();

	// 弾ごとに制御点の距離をばらす。着弾点 (P3) と飛行時間は触らないので一斉着弾は崩れない
	auto Jitter = [this]() { return 1.0f + FMath::FRandRange(-ShotShapeJitter, ShotShapeJitter); };

	// 横ずらしの方位も振る。P1とP2で独立に引くので、登りと降りで回り込む向きが食い違い、
	// ひねりの形そのものが1発ごとに変わる (距離だけ振っても同じ形の拡大縮小にしかならない)
	auto JitterDir = [this, &Tangent]()
		{
			return Tangent.RotateAngleAxis(
				FMath::FRandRange(-ControlPointAngleJitterDeg, ControlPointAngleJitterDeg), FVector::UpVector);
		};

	// 頂点は発射位置と着弾点の中間の上空に置き、P2/P3を同じ高さで水平に並べて平らにする
	// この2点の間隔が「切り返しに使える水平距離」そのものになる
	const float   ApexZ = SpawnLoc.Z + ApexHeight * Jitter();
	const FVector ApexIn = FVector(SpawnLoc.X, SpawnLoc.Y, ApexZ)
		+ JitterDir() * (ApexSpreadDistance * Jitter());
	const FVector ApexOut = FVector(Target.X, Target.Y, ApexZ)
		+ JitterDir() * (ApexSpreadDistance * Jitter());

	TArray<FVector> MidPoints;
	MidPoints.Reserve(4);
	// P1: 登りの向きを固定する
	MidPoints.Add(SpawnLoc
		+ FVector(0.0f, 0.0f, LaunchUpDistance * Jitter()) + JitterDir() * (LaunchSpreadDistance * Jitter()));
	// P2/P3: 頂点を渡る区間
	MidPoints.Add(ApexIn);
	MidPoints.Add(ApexOut);
	// P4: 降りの向きを固定する
	MidPoints.Add(Target + FVector(0.0f, 0.0f, ImpactApproachHeight * Jitter()));

	const FRotator SpawnRot = (MidPoints[0] - SpawnLoc).GetSafeNormal().Rotation();

	AEnemyProjectile* Proj = Pool->Acquire(
		CircusProfile, FTransform(SpawnRot, SpawnLoc), Enemy, Enemy->GetInstigator());
	if (!Proj) return FlightDuration;

	UArcPathBehavior* Arc = Proj->FindBehavior<UArcPathBehavior>();
	USplashDamageBehavior* Splash = Proj->FindBehavior<USplashDamageBehavior>();

	TideCombatUtil::InjectAttackDamage(Enemy, Proj->Damage);
	if (Splash) TideCombatUtil::InjectAttackDamage(Enemy, Splash->SplashDamage);

	// ビヘイビアは弾ごとに複製されたインスタンスなので直接書いてよい
	if (Splash && SplashRadius > 0.0f)
	{
		Splash->SplashRadius = SplashRadius;
	}

	float LeadTime = FlightDuration;
	if (Arc)
	{
		// 炸裂は範囲ダメージ (Effect) に一本化する。弧側の空中炸裂を残すと二重に当たる
		Arc->SplashRadius = 0.0f;

		// 螺旋も弾ごとにばらす。プールで使い回される複製インスタンスへ書くので、前回値に掛けると
		// 発射のたびに増減が積み上がる。必ずプロファイルの元値を基準に代入し直す
		if (const UArcPathBehavior* Template = Cast<UArcPathBehavior>(CircusProfile->Movement))
		{
			Arc->SpiralTurns = Template->SpiralTurns * Jitter();
			Arc->SpiralAmplitude = Template->SpiralAmplitude * Jitter();
		}

		if (FlightDuration > 0.0f)
		{
			Arc->FlightDuration = FlightDuration;
		}
		LeadTime = Arc->FlightDuration;

		Arc->SetTargetLocation(Target);
		Arc->SetCurveMidPoints(MidPoints);
		Arc->SetInitialHeadingDirection((MidPoints[0] - SpawnLoc).GetSafeNormal());
	}

	Proj->ActivateProjectile();
	return LeadTime;
}

void UCircusShotAttackExecutionBase::AddImpactDecal(AEnemyCharacter* Enemy, const FVector& Target, float LeadTime)
{
	if (!ImpactDecalMaterial || LeadTime <= 0.0f) return;

	UWorld* World = Enemy->GetWorld();
	if (!World) return;

	// 炸裂半径0 = プロファイル任せ。デカールだけは潰れないよう最低値を持たせる
	const float DecalRadius = FMath::Max(50.0f, SplashRadius * ImpactDecalRadiusRatio);
	const FVector DecalSize(ImpactDecalDepth, DecalRadius, DecalRadius);

	UDecalComponent* Decal = UGameplayStatics::SpawnDecalAtLocation(
		World, ImpactDecalMaterial, DecalSize, Target, FRotator(-90.0f, 0.0f, 0.0f), LeadTime);
	if (!Decal) return;

	FCircusDecal Pending;
	Pending.RemainingTime = LeadTime;
	Pending.TotalTime = FMath::Max(0.0001f, LeadTime);
	Pending.Decal = Decal;
	if (UMaterialInstanceDynamic* MID = Decal->CreateDynamicMaterialInstance())
	{
		MID->SetScalarParameterValue(ImpactDecalProgressParam, 0.0f);
		Pending.DecalMID = MID;
	}

	PendingDecals.Add(Pending);
}

void UCircusShotAttackExecutionBase::UpdateImpactDecals(float DeltaTime)
{
	for (int32 i = PendingDecals.Num() - 1; i >= 0; --i)
	{
		FCircusDecal& Pending = PendingDecals[i];
		Pending.RemainingTime -= DeltaTime;

		const float Progress = FMath::Clamp(
			(Pending.TotalTime - Pending.RemainingTime) / Pending.TotalTime, 0.0f, 1.0f);
		if (UMaterialInstanceDynamic* MID = Pending.DecalMID.Get())
		{
			MID->SetScalarParameterValue(ImpactDecalProgressParam, Progress);
		}

		if (Pending.RemainingTime <= 0.0f)
		{
			if (UDecalComponent* Decal = Pending.Decal.Get())
			{
				Decal->DestroyComponent();
			}
			PendingDecals.RemoveAt(i);
		}
	}
}

void UCircusShotAttackExecutionBase::ClearImpactDecals()
{
	// 技終了後は進行度を回せず表示が固まるので残しても意味がない
	// 最後の弾が着弾しきる長さは "End" セクション側で確保しておくこと
	for (FCircusDecal& Pending : PendingDecals)
	{
		if (UDecalComponent* Decal = Pending.Decal.Get())
		{
			Decal->DestroyComponent();
		}
	}
	PendingDecals.Empty();
}

void UCircusShotAttackExecutionBase::DebugDrawImpact(const AEnemyCharacter* Enemy, const FVector& Target) const
{
	if (!bDebugDrawImpacts || !Enemy->GetWorld()) return;

	const float Duration = 5.0f;
	DrawDebugSphere(Enemy->GetWorld(), Target, 60.0f, 12, FColor::Yellow, false, Duration);
	DrawDebugCircle(Enemy->GetWorld(), Target + FVector(0.0f, 0.0f, 5.0f),
		FMath::Max(50.0f, SplashRadius), 24, FColor::Red, false, Duration, 0, 3.0f,
		FVector::ForwardVector, FVector::RightVector, false);
}

void UCircusBarrageAttackExecution::OnAttackEnd(AEnemyCharacter* Enemy)
{
	if (Enemy)
	{
		Enemy->GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);

		if (bOpenArmamentSoftWindow)
		{
			// 再硬化
			Enemy->EndArmamentSoftWindow();
		}

		// 復帰中の光輪発光＋接触ダメージ窓を閉じる (中断終了でも必ず対称に閉じる)
		if (bEnableRecoveryTouchDamage)
		{
			if (UPartDestructionComponent* PartComp = Enemy->FindComponentByClass<UPartDestructionComponent>())
			{
				PartComp->EndPartHaloRecoveryDamage();
			}
		}
	}

	ClearImpactDecals();
}

// ------------------------------------------------------------
// UCircusRainAttackExecution
// ------------------------------------------------------------

void UCircusRainAttackExecution::OnAttackBegin(AEnemyCharacter* Enemy)
{
	bFired = false;
	Enemy->GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
}

void UCircusRainAttackExecution::OnAttackEvent(AEnemyCharacter* Enemy, FGameplayTag EventTag)
{
	if (!Enemy || EventTag != TAG_AttackEvent_MissileShot || bFired) return;

	// 最初のShotで全弾を上空へ打ち上げ、その場でEndへ飛んで攻撃を終える
	// 以降の6秒はコンダクターが各弾の降下を刻むだけで、この攻撃は次の抽選を妨げない
	bFired = true;
	LaunchVolley(Enemy);
	JumpMontageToEnd(Enemy);
}

void UCircusRainAttackExecution::LaunchVolley(AEnemyCharacter* Enemy)
{
	UWorld* World = Enemy ? Enemy->GetWorld() : nullptr;
	if (!CircusProfile || !World) return;

	// 総弾数: 旧・時間駆動ループが6秒で撃っていた本数と同じだけ一斉に打ち上げる
	const float Interval = FMath::Max(0.02f, FireInterval);
	const int32 BurstCount = FMath::Max(1, FMath::CeilToInt(FireDuration / Interval));
	const int32 TotalShots = BurstCount * FMath::Max(1, ShotsPerFire);

	// 各弾はホバー中に寿命切れしないよう、最も遅い降下 + 降下飛行 + 余白まで生かす
	const float MissileLifeSpan = DescentStartDelay + FireDuration + DescentFlightDuration + 1.0f;

	const FVector EnemyPos = Enemy->GetActorLocation();
	const FVector SpawnLoc = GetMuzzleLocation(Enemy);

	// 6秒間の降下スケジュールと予兆デカール進行を担うコンダクターを1体立てる
	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = Enemy;
	ACircusRainConductor* Conductor = World->SpawnActor<ACircusRainConductor>(
		ACircusRainConductor::StaticClass(), FTransform::Identity, SpawnParams);
	if (!Conductor) return;

	ACircusRainConductor::FConfig Config;
	Config.ScatterRadius = ScatterRadius;
	Config.PredictLeadTime = PredictLeadTime;
	Config.DescentFlightDuration = DescentFlightDuration;
	Config.DescentSwoopHeight = DescentSwoopHeight;
	Config.ImpactApproachHeight = DescentApproachHeight;
	Config.DecalMaterial = ImpactDecalMaterial;
	Config.DecalProgressParam = ImpactDecalProgressParam;
	Config.DecalRadius = FMath::Max(50.0f, SplashRadius * ImpactDecalRadiusRatio);
	Config.DecalDepth = ImpactDecalDepth;
	Config.bDebugDraw = bDebugDrawImpacts;
	Conductor->Initialize(Enemy, Config, MissileLifeSpan + 1.0f);

	for (int32 k = 0; k < TotalShots; ++k)
	{
		// 降下開始時刻を [DescentStartDelay, DescentStartDelay +
		// FireDuration] に分布させる
		const float Frac = (TotalShots > 1) ? static_cast<float>(k) / static_cast<float>(TotalShots - 1) : 0.0f;
		float DescendAt = DescentStartDelay + Frac * FireDuration;
		if (DescentTimeJitter > 0.0f && TotalShots > 1)
		{
			const float Slot = FireDuration / static_cast<float>(TotalShots - 1);
			DescendAt += FMath::FRandRange(-DescentTimeJitter, DescentTimeJitter) * Slot;
		}
		DescendAt = FMath::Max(DescentStartDelay, DescendAt);

		// ホバー位置: ボス頭上の水平ディスク内に散らす (円内一様は半径に平方根)
		const float HRadius = HoverSpreadRadius * FMath::Sqrt(FMath::FRand());
		const float HAngle = FMath::FRandRange(0.0f, 360.0f);
		const FVector HOffset = FVector::ForwardVector.RotateAngleAxis(HAngle, FVector::UpVector) * HRadius;
		const FVector HoverPos = EnemyPos + HOffset + FVector(0.0f, 0.0f, HoverHeight);

		// 上昇は降下開始のわずか手前でちょうど頂点へ着くよう時間を合わせる。これで静止ホバーが
		// 実質消え、各弾は「上りきったその瞬間に降り始める」噴水のような流れになる
		const float RiseDuration = FMath::Max(0.2f, DescendAt - 0.1f);
		AEnemyProjectile* Missile = SpawnHoverMissile(Enemy, SpawnLoc, HoverPos, MissileLifeSpan, RiseDuration);
		if (!Missile) continue;

		Conductor->AddDescent(Missile, DescendAt);
	}
}

AEnemyProjectile* UCircusRainAttackExecution::SpawnHoverMissile(AEnemyCharacter* Enemy, const FVector& SpawnLoc,
	const FVector& HoverPos, float MissileLifeSpan, float RiseDuration) const
{
	UProjectilePoolSubsystem* Pool = Enemy->GetWorld()
		? Enemy->GetWorld()->GetSubsystem<UProjectilePoolSubsystem>() : nullptr;
	if (!Pool) return nullptr;

	const FRotator SpawnRot = (HoverPos - SpawnLoc).GetSafeNormal().Rotation();
	AEnemyProjectile* Proj = Pool->Acquire(
		CircusProfile, FTransform(SpawnRot, SpawnLoc), Enemy, Enemy->GetInstigator());
	if (!Proj) return nullptr;

	// ダメージはAttackExecution側の値を注入する
	// (全弾t=0スポーンなので攻撃キャッシュはまだ有効)
	TideCombatUtil::InjectAttackDamage(Enemy, Proj->Damage);
	if (USplashDamageBehavior* Splash = Proj->FindBehavior<USplashDamageBehavior>())
	{
		TideCombatUtil::InjectAttackDamage(Enemy, Splash->SplashDamage);
	}

	// ホバー中に寿命切れしないよう寿命を上書きする
	Proj->MaxLifeTime = MissileLifeSpan;

	if (UArcPathBehavior* Arc = Proj->FindBehavior<UArcPathBehavior>())
	{
		// 空中炸裂は使わない (寿命切れの保険で誤爆させない)。降下弧はコンダクターが後から張り直す
		Arc->SplashRadius = 0.0f;
		// SpawnLoc → HoverPosへ上昇し、指令が来るまで実質無限にホバー保持する
		Arc->SetTargetLocation(HoverPos);
		Arc->SetDeploy(HoverPos, /*HoverDuration=*/1.0e9f);
		// RiseDuration秒でちょうどHoverPosへ着くよう上昇速度を合わせる
		// (静止ホバーを実質無くす)
		const float RiseDist = (HoverPos - SpawnLoc).Size();
		Arc->DeploySpeed = RiseDist / FMath::Max(0.05f, RiseDuration);
	}

	Proj->ActivateProjectile();
	return Proj;
}

void UCircusRainAttackExecution::OnAttackEnd(AEnemyCharacter* Enemy)
{
	if (Enemy)
	{
		Enemy->GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	}

	// 弾とデカールはコンダクターが所有・後始末する。ここでは何も破棄しない (消すと降雨が止まる)
	bFired = false;
}

// ------------------------------------------------------------
// USpeedMissileAttackExecution
// ------------------------------------------------------------
void USpeedMissileAttackExecution::OnAttackBegin(AEnemyCharacter* Enemy)
{
	ShotFiredCount = 0;
	NextHeldMissileIndex = 0;
	HeldMissiles.Reset();
	Enemy->GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
}

void USpeedMissileAttackExecution::OnAttackEnd(AEnemyCharacter* Enemy)
{
	Enemy->GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);

	// 撃ち切れず頭上に残った待機弾は攻撃終了 (中断含む) 時に後始末する
	for (int32 i = NextHeldMissileIndex; i < HeldMissiles.Num(); ++i)
	{
		if (AEnemyProjectile* Missile = HeldMissiles[i].Get())
		{
			Missile->Despawn();
		}
	}
	HeldMissiles.Reset();
	NextHeldMissileIndex = 0;
}

void USpeedMissileAttackExecution::OnAttackEvent(AEnemyCharacter* Enemy, FGameplayTag EventTag)
{
	if (EventTag == TAG_AttackEvent_MissileStart)
	{
		ShotFiredCount = 0;
		SpawnAllMissiles(Enemy);
	}
	else if (EventTag == TAG_AttackEvent_MissileShot)
	{
		// 発射のたびにプレイヤー位置と着弾列の基準方向をターゲット方向へ補正し直す
		AActor* Target = TideCombatUtil::GetTarget(Enemy);
		if (!Target) return;

		CachedPlayerPos = Target->GetActorLocation();
		CachedForward2D = (CachedPlayerPos - Enemy->GetActorLocation()).GetSafeNormal2D();
		if (CachedForward2D.IsNearlyZero())
		{
			CachedForward2D = Enemy->GetActorForwardVector().GetSafeNormal2D();
		}

		FireMissileLine(Enemy);
	}
}

void USpeedMissileAttackExecution::SpawnAllMissiles(AEnemyCharacter* Enemy)
{
	if (!BombardProfile || !Enemy) return;

	UProjectilePoolSubsystem* Pool = Enemy->GetWorld()
		? Enemy->GetWorld()->GetSubsystem<UProjectilePoolSubsystem>() : nullptr;
	if (!Pool) return;

	HeldMissiles.Reset();
	NextHeldMissileIndex = 0;

	const int32 LineCount = LineBulletCounts.Num();
	if (LineCount <= 0) return;

	int32 TotalCount = 0;
	for (int32 Line = 0; Line < LineCount; ++Line)
	{
		TotalCount += GetLineBulletCountForRow(Line);
	}
	if (TotalCount <= 0) return;

	// MissileShotで狙い撃って解放するまでの待機時間。攻撃の実尺より十分長い固定値を入れておき、
	// 実際の発射タイミングはFireMissileLineからのReleaseHold()の明示呼び出しに委ねる
	constexpr float HeldMissileHoldDuration = 999.0f;

	const FVector HeadCenter = Enemy->GetActorLocation() + FVector(0.0f, 0.0f, SpawnHeightOffset);
	// CachedForward2DはMissileStart時点ではまだ確定していない (最初のMissileShotで確定する) ため、
	// 頭上フォーメーションの扇の軸には敵の現在の正面方向を使う。この軸を中心に回すことで、
	// 水平面ではなく敵の正面を向いた円 (Up/Right平面) 上に扇を広げる
	const FVector FanPivotAxis = Enemy->GetActorForwardVector().GetSafeNormal();
	const FRotator SpawnRot = Enemy->GetActorRotation();

	HeldMissiles.Reserve(TotalCount);
	for (int32 Line = 0; Line < LineCount; ++Line)
	{
		// 行ごとの弾数 (LineBulletCountsで行ごとに指定)
		const int32 BulletsInLine = GetLineBulletCountForRow(Line);

		// 中心 (HeadCenter = SpawnHeightOffsetの位置) はどの行も共通。
		// 行が進むごとにHeadLineRadiusStepぶん半径を広げ、同心円状に重ねる
		const float LineRadius = HeadLineFanRadius + static_cast<float>(Line) * HeadLineRadiusStep;

		// 着弾側と同じ左右判定 (中央より前が左、後ろが右) で頭上待機弾を2グループに分け、
		// 敵の左側/右側 (Forward軸まわりで±90度の方向) を中心にした小さな扇としてそれぞれ配置する
		const int32 LeftCount = BulletsInLine / 2;
		const int32 RightCount = BulletsInLine - LeftCount;

		for (int32 SlotInLine = 0; SlotInLine < BulletsInLine; ++SlotInLine)
		{
			const bool bLeftGroup = SlotInLine < LeftCount;
			const int32 GroupCount = bLeftGroup ? LeftCount : RightCount;
			const int32 IndexInGroup = bLeftGroup ? SlotInLine : (SlotInLine - LeftCount);
			const float LocalIndex = (GroupCount > 1) ? IndexInGroup - (GroupCount - 1) * 0.5f : 0.0f;

			// 敵の正面 (Up方向) を基準に、左グループは-90度側、右グループは+90度側を中心とする
			const float BaseAngleDeg = bLeftGroup ? -90.0f : 90.0f;
			const float AngleDeg = BaseAngleDeg + LocalIndex * HeadLineFanAngleStep;

			// Forward軸まわりの回転はUp軸まわりの回転と左右が逆になる (Axis×Vの向きが反転するため)。
			// 着弾側 (FireMissileLineのFanDir) と同じ並び順で左右が対応するよう符号を反転する
			const FVector FanDir = FVector::UpVector.RotateAngleAxis(-AngleDeg, FanPivotAxis);

			const FVector SpawnLoc = HeadCenter + FanDir * LineRadius;

			AEnemyProjectile* Missile = Pool->Acquire(
				BombardProfile, FTransform(SpawnRot, SpawnLoc), Enemy, Enemy->GetInstigator());
			if (!Missile) continue;

			// 見た目だけ出して待機させる (移動/着弾はMissileShotでのReleaseHold()まで起きない)
			Missile->SpawnHoldDuration = HeldMissileHoldDuration;
			Missile->ActivateProjectile();

			HeldMissiles.Add(Missile);
		}
	}
}

int32 USpeedMissileAttackExecution::GetLineBulletCountForRow(int32 RowIndex) const
{
	return LineBulletCounts.IsValidIndex(RowIndex) ? FMath::Max(1, LineBulletCounts[RowIndex]) : 1;
}

void USpeedMissileAttackExecution::FireMissileLine(AEnemyCharacter* Enemy)
{
	if (!Enemy) return;

	const int32 BulletsPerLine = GetLineBulletCountForRow(ShotFiredCount);
	const FVector EnemyPos = Enemy->GetActorLocation();

	// この行の着弾半径。敵からターゲット方向へRowSpacing×(行番号+1)進めた距離を使う
	const float RowRadius = static_cast<float>(ShotFiredCount + 1) * RowSpacing;

#if !UE_BUILD_SHIPPING
	const bool bDebugDraw = UTideGameSettings::Get()->bDebugDrawBombardLanding;
	const float DebugDuration = 3.0f;
#endif

	for (int32 i = 0; i < BulletsPerLine; ++i)
	{
		if (NextHeldMissileIndex >= HeldMissiles.Num()) break;

		AEnemyProjectile* Missile = HeldMissiles[NextHeldMissileIndex].Get();
		++NextHeldMissileIndex;
		if (!Missile) continue;

		// 中心方向(敵→ターゲット)を基準に、LineFanAngleStepずつ角度をずらして扇状に広げる
		// (弾数が多い行ほど扇全体の開き角も広がる)
		const float NormalizedIndex = (BulletsPerLine > 1) ? i - (BulletsPerLine - 1) * 0.5f : 0.0f;
		const FVector FanDir = CachedForward2D.RotateAngleAxis(NormalizedIndex * LineFanAngleStep, FVector::UpVector);
		const FVector Target = ClampBombardLandingPos(
			EnemyPos + FanDir * RowRadius,
			EnemyPos, MinDistFromEnemy, CachedForward2D);

		TideCombatUtil::InjectAttackDamage(Enemy, Missile->Damage);
		if (UBezierArcBehavior* Bombard = Missile->FindBehavior<UBezierArcBehavior>())
		{
			TideCombatUtil::InjectAttackDamage(Enemy, Bombard->SplashDamage);
			Bombard->FlightDuration = BombardFlightDuration;
			Bombard->SetTargetLocation(Target);
			Bombard->SetInitialHeadingDirection((Target - Missile->GetActorLocation()).GetSafeNormal());
		}

		Missile->ReleaseHold();

#if !UE_BUILD_SHIPPING
		if (bDebugDraw)
		{
			DrawDebugSphere(Enemy->GetWorld(), Target, 50.0f, 12, FColor::Orange, false, DebugDuration);
			DrawDebugString(Enemy->GetWorld(), Target + FVector(0.0f, 0.0f, 70.0f),
				FString::Printf(TEXT("R%dL%d"), ShotFiredCount + 1, i + 1), nullptr, FColor::Orange, DebugDuration);
		}
#endif
	}

	++ShotFiredCount;
	if (ShotFiredCount >= LineBulletCounts.Num())
	{
		JumpMontageToEnd(Enemy);
	}
}

// ------------------------------------------------------------
// UHomingShotAttackExecution
// ------------------------------------------------------------
void UHomingShotAttackExecution::OnAttackBegin(AEnemyCharacter* Enemy)
{
	ShotFiredCount = 0;
	Enemy->GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
}

void UHomingShotAttackExecution::OnAttackEnd(AEnemyCharacter* Enemy)
{
	Enemy->GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
}

void UHomingShotAttackExecution::OnAttackEvent(AEnemyCharacter* Enemy, FGameplayTag EventTag)
{
	if (EventTag == TAG_AttackEvent_MissileStart)
	{
		ShotFiredCount = 0;
	}
	else if (EventTag == TAG_AttackEvent_MissileShot)
	{
		if (ShotFiredCount == 0)
		{
			AActor* Target = TideCombatUtil::GetTarget(Enemy);
			if (!Target) return;

			CachedForward2D = (Target->GetActorLocation() - Enemy->GetActorLocation()).GetSafeNormal2D();
			if (CachedForward2D.IsNearlyZero())
			{
				CachedForward2D = Enemy->GetActorForwardVector().GetSafeNormal2D();
			}

			// 初段発射時にPCの移動方向(velocity)から弧を描く左右を確定する。以降のショットも同じ向きを使う
			// PCが向かう側とは逆へ弧を描く(PCが右へ動くなら左から弧を描く)。停止中(移動量ほぼ0)は右へ
			const FVector PCVel2D = Target->GetVelocity().GetSafeNormal2D();
			const FVector ForwardRight2D = FVector::CrossProduct(FVector::UpVector, CachedForward2D).GetSafeNormal();
			if (PCVel2D.IsNearlyZero())
			{
				CurveSideSign = 1.0f;
			}
			else
			{
				CurveSideSign = FVector::DotProduct(PCVel2D, ForwardRight2D) >= 0.0f ? -1.0f : 1.0f;
			}
		}
		SpawnHomingShot(Enemy);
	}
}

void UHomingShotAttackExecution::SpawnHomingShot(AEnemyCharacter* Enemy)
{
	if (!HomingProfile) return;

	UProjectilePoolSubsystem* Pool = Enemy->GetWorld()
		? Enemy->GetWorld()->GetSubsystem<UProjectilePoolSubsystem>() : nullptr;
	if (!Pool) return;

	AActor* Target = TideCombatUtil::GetTarget(Enemy);
	if (!Target) return;

	const FVector EnemyPos = Enemy->GetActorLocation();
	const FVector SpawnBase = EnemyPos
		- Enemy->GetActorForwardVector() * SpawnBackOffset
		+ FVector(0.0f, 0.0f, SpawnHeightOffset);

	// ターゲット方向からCurveSideSignの側へInitialLaunchAngleDegだけ逸らした向きで撃ち出す。
	// UHomingProjectileBehaviorは追従対象をプレイヤーPawnから自動取得し、旋回速度制限つきで
	// 徐々に向き直していくため、この初速の逸らしが弧を描くように見える
	const FVector ToTargetDir2D = (Target->GetActorLocation() - SpawnBase).GetSafeNormal2D();
	const FVector LaunchDir = ToTargetDir2D.RotateAngleAxis(InitialLaunchAngleDeg * CurveSideSign, FVector::UpVector);
	const FRotator SpawnRot = LaunchDir.Rotation();

	AEnemyProjectile* Proj = Pool->Acquire(
		HomingProfile, FTransform(SpawnRot, SpawnBase), Enemy, Enemy->GetInstigator());
	if (!Proj) return;

	// MaxLifeTime/bDetonateOnExpire等はProjectileProfile側の値をそのまま使い、ここでは上書きしない
	TideCombatUtil::InjectAttackDamage(Enemy, Proj->Damage);
	if (USplashDamageBehavior* Splash = Proj->FindBehavior<USplashDamageBehavior>())
	{
		TideCombatUtil::InjectAttackDamage(Enemy, Splash->SplashDamage);
	}

	Proj->ActivateProjectile();

#if !UE_BUILD_SHIPPING
	if (UTideGameSettings::Get()->bDebugDrawBombardLanding)
	{
		DrawDebugDirectionalArrow(Enemy->GetWorld(), SpawnBase, SpawnBase + LaunchDir * 400.0f, 40.0f, FColor::Cyan, false, 2.0f);
	}
#endif

	++ShotFiredCount;
	if (ShotFiredCount >= ShotCount)
	{
		JumpMontageToEnd(Enemy);
	}
}
