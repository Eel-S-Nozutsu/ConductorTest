// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#include "TideAttackExecution_EM0020.h"

#include "PRJ_TIDE_P0/Actors/Character/Enemy/EnemyCharacter.h"
#include "PRJ_TIDE_P0/Actors/Projectile/EnemyProjectile.h"
#include "PRJ_TIDE_P0/Actors/Projectile/ProjectilePoolSubsystem.h"
#include "PRJ_TIDE_P0/Actors/Projectile/ProjectileProfile.h"
#include "PRJ_TIDE_P0/Actors/Projectile/Behaviors/DeployHomingBehavior.h"
#include "PRJ_TIDE_P0/Actors/Projectile/Behaviors/BezierArcBehavior.h"
#include "PRJ_TIDE_P0/Utilities/TideCombatUtils.h"
#include "PRJ_TIDE_P0/Data/Tags/TideGameplayTags.h"
#include "PRJ_TIDE_P0/Core/TideGameSettings.h"

#include "AIController.h"
#include "DrawDebugHelpers.h"
#include "BrainComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "NavigationSystem.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"

// ------------------------------------------------------------
// UHaloThrowAttackExecution
// ------------------------------------------------------------

void UHaloThrowAttackExecution::OnAttackEvent(AEnemyCharacter* Enemy, FGameplayTag EventTag)
{
	if (EventTag == TAG_AttackEvent_HaloThrowStart)
	{
		// 光輪はAEnemyCharacterがDAから構成した実体を使う (旧BP配置の名前検索は廃止)
		UStaticMeshComponent* Halo = Enemy->GetHaloMeshComponent();
		HaloComp = Halo;
		if (Halo) Halo->SetVisibility(false, true);

		if (!HaloProfile || !HaloComp.IsValid()) return;

		UWorld* World = Enemy->GetWorld();
		UProjectilePoolSubsystem* Pool = World ? World->GetSubsystem<UProjectilePoolSubsystem>() : nullptr;
		if (!Pool) return;

		// 位置は光輪の位置、回転はキャラクターの前方向きを使う
		FTransform SpawnTransform(Enemy->GetActorRotation(), HaloComp->GetComponentLocation());
		AEnemyProjectile* Projectile = Pool->Acquire(HaloProfile, SpawnTransform, Enemy, Enemy->GetInstigator());
		if (Projectile)
		{
			TideCombatUtil::InjectAttackDamage(Enemy, Projectile->Damage);
			Projectile->ActivateProjectile();
			SpawnedProjectile = Projectile;
		}

		// 光輪が手元を離れている間は無防備。光輪攻撃の再抽選を止め、肉質を本体扱いにする
		Enemy->SetHaloThrown(true);
	}
	else if (EventTag == TAG_AttackEvent_HaloReturn)
	{
		if (SpawnedProjectile.IsValid())
		{
			SpawnedProjectile->Despawn();
			SpawnedProjectile.Reset();
		}

		if (HaloComp.IsValid())
		{
			HaloComp->SetVisibility(true, true);
		}

		Enemy->RestoreHaloToStateSocket(HaloComponentName);
		Enemy->SetHaloThrown(false);
	}
}

void UHaloThrowAttackExecution::OnAttackEnd(AEnemyCharacter* Enemy)
{
	if (SpawnedProjectile.IsValid())
	{
		SpawnedProjectile->Despawn();
		SpawnedProjectile.Reset();
	}

	// HaloReturnを経ていれば光輪は既に手元へ戻っている
	// 中断された場合はここで即座に戻さず、技クールを消化してから戻す (その間は無防備のまま)
	Enemy->StartHaloThrowRegenIfAway();
}

// ------------------------------------------------------------
// UGuardActivateAttackExecution
// ------------------------------------------------------------

void UGuardActivateAttackExecution::OnAttackEvent(AEnemyCharacter* Enemy, FGameplayTag EventTag)
{
	/*
	* AM_guard_onのNotify構成メモ
	*  0F: TAG_AttackEvent_GuardOn
	* 64F: HaloReadyEvent
	*  0F～64F: SetStateTag.SA付与 (展開中。怯まず光輪も割れない猶予)
	* 64F～終端: SetStateTag.SA.HaloOpen付与 (怯まないが光輪は割れる)
	*/

	if (EventTag == TAG_AttackEvent_GuardOn)
	{
		Enemy->BeginGuard(HaloComponentName);
		if (ChargeDelay > 0.0f)
		{
			StartCharge(Enemy);
		}
		return;
	}

	if (HaloReadyEvent.IsValid() && EventTag == HaloReadyEvent)
	{
		Enemy->SetHaloDeployed(true);
	}
}

void UGuardActivateAttackExecution::StartCharge(AEnemyCharacter* Enemy)
{
	if (!Enemy) return;

	CachedEnemy = Enemy;
	ChargeElapsed = 0.0f;
	FlickerPhase = 0.0f;

	// ExecutionはOnAttackEnd後 (ガードモンタージュ終了時) にGCされうるが、
	// チャージはそれを跨いで継続する。Enemyにハードリファレンスで保持させて延命する
	Enemy->HoldChargeExecution(this);

	FTimerManager& TM = Enemy->GetWorldTimerManager();
	// 発光フリッカー (高頻度更新)
	TM.SetTimer(FlickerTimerHandle, this, &UGuardActivateAttackExecution::TickFlicker, 0.03f, true);
	// ChargeDelay秒後に暴発
	TM.SetTimer(DetonateTimerHandle, this, &UGuardActivateAttackExecution::Detonate,
		FMath::Max(0.01f, ChargeDelay), false);
}

void UGuardActivateAttackExecution::TickFlicker()
{
	AEnemyCharacter* Enemy = CachedEnemy.Get();

	// ガードを割られた / 消滅したらチャージ中断 (発光も戻す)
	if (!Enemy || !Enemy->HasStateTag(TAG_State_Enemy_Guard))
	{
		CancelCharge();
		return;
	}

	constexpr float Dt = 0.03f;
	ChargeElapsed += Dt;
	const float Delay = FMath::Max(0.01f, ChargeDelay);
	const float T = FMath::Clamp(ChargeElapsed / Delay, 0.0f, 1.0f);

	// 点滅周波数をStart -> Endへ
	// 位相を積分するのでStart<Endなら確実に徐々に速くなる
	// (cos(elapsed*Hz(elapsed))
	// 式だと周波数変動で見かけ速度が崩れるため位相積分にする)イーズイン (T*T)
	// で序盤はゆっくり溜め、終盤に一気に速くする
	const float Hz = FMath::Lerp(FlickerStartHz, FlickerEndHz, T * T);
	FlickerPhase += 2.0f * PI * Hz * Dt;

	// 0..1のパルス。終盤ほど振幅 (T) も上げて徐々に強く明滅させる
	const float Pulse = 0.5f * (1.0f - FMath::Cos(FlickerPhase));
	Enemy->SetHaloGlowIntensity(Pulse * GlowMaxIntensity * T);
}

void UGuardActivateAttackExecution::Detonate()
{
	AEnemyCharacter* Enemy = CachedEnemy.Get();

	if (Enemy)
	{
		Enemy->GetWorldTimerManager().ClearTimer(FlickerTimerHandle);
	}

	// ガードを割られていたら暴発しない (割った側へのご褒美)
	if (!Enemy || !Enemy->HasStateTag(TAG_State_Enemy_Guard))
	{
		if (Enemy)
		{
			Enemy->SetHaloGlowIntensity(0.0f);
			Enemy->ReleaseChargeExecution();
		}
		return;
	}

	UWorld* World = Enemy->GetWorld();
	if (!World)
	{
		Enemy->SetHaloGlowIntensity(0.0f);
		Enemy->EndGuardNow();
		Enemy->ReleaseChargeExecution();
		return;
	}

	const FVector Origin = GetChargeOrigin(Enemy);

	// 2倍スケールVFX
	if (BurstVFX)
	{
		if (UNiagaraComponent* NC = UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, BurstVFX, Origin))
		{
			NC->SetWorldScale3D(FVector(BurstVFXScale));
		}
	}

	// 広範囲ダメージ (敵中心スフィア。敵対するIDamageableに当てる)
	if (BlastRadius > 0.0f)
	{
#if !UE_BUILD_SHIPPING
		// 暴発した範囲ダメージエリアを可視化する。
		// 判定はこの場で完結する (以降は当たらない) ので、
		// 実際のダメージ発生期間に合わせて表示も1Fだけにする
		if (UTideGameSettings::Get()->bDebugDrawAttackHitbox)
		{
			DrawDebugSphere(World, Origin, BlastRadius, 16, FColor::Red, false, -1.0f, 0, 1.5f);
		}
#endif

		FCollisionObjectQueryParams ObjQuery(ECC_Pawn);
		FCollisionQueryParams Params(SCENE_QUERY_STAT(GuardBurstBlast), false, Enemy);
		Params.AddIgnoredActor(Enemy);

		const FCollisionShape Sphere = FCollisionShape::MakeSphere(BlastRadius);
		TArray<FHitResult> Hits;
		World->SweepMultiByObjectType(
			Hits, Origin - FVector(0.0f, 0.0f, 1.0f), Origin + FVector(0.0f, 0.0f, 1.0f),
			FQuat::Identity, ObjQuery, Sphere, Params);

		TSet<AActor*> Processed;
		for (const FHitResult& Hit : Hits)
		{
			AActor* Other = Hit.GetActor();
			if (!Other || Other == Enemy || Processed.Contains(Other)) continue;
			if (!TideCombatUtil::IsHostileTo(Enemy, Other)) continue;

			IDamageable* Damageable = Cast<IDamageable>(Other);
			if (!Damageable) continue;

			Processed.Add(Other);

			FDamageInfo Info;
			Info.BaseDamage = BlastDamage;
			Info.Instigator = Enemy;
			Info.HitReactionTag = BlastReactionTag;
			Info.KnockbackDirectionOverride = (Other->GetActorLocation() - Origin).GetSafeNormal2D();
			Info.HitResult = Hit;
			Damageable->ReceiveDamage(Info);
		}
	}

	// 水平方向へ360/Nで放射状発射
	UProjectilePoolSubsystem* Pool = World ? World->GetSubsystem<UProjectilePoolSubsystem>() : nullptr;
	if (ProjectileProfile && Pool && ProjectileCount > 0)
	{
		const float Step = 360.0f / static_cast<float>(ProjectileCount);
		const float BaseYaw = Enemy->GetActorRotation().Yaw;
		for (int32 i = 0; i < ProjectileCount; ++i)
		{
			const FRotator Rotation(0.0f, BaseYaw + Step * i, 0.0f);
			const FTransform SpawnXf(Rotation, Origin);

			AEnemyProjectile* Projectile = Pool->Acquire(ProjectileProfile, SpawnXf, Enemy, Enemy->GetInstigator());
			if (!Projectile) continue;

			// ProjectileDamageが負ならプロファイル既定
			// (InitFromProfileが設定済み)を尊重する
			if (ProjectileDamage >= 0.0f) Projectile->Damage = ProjectileDamage;
			Projectile->ActivateProjectile();
		}
	}

	// 暴発と同時にガードを終了する (タイマー0 = ガード終了 = 暴発)
	Enemy->SetHaloGlowIntensity(0.0f);
	Enemy->EndGuardNow();
	Enemy->ReleaseChargeExecution();
}

void UGuardActivateAttackExecution::CancelCharge()
{
	AEnemyCharacter* Enemy = CachedEnemy.Get();
	if (!Enemy) return;

	Enemy->GetWorldTimerManager().ClearTimer(FlickerTimerHandle);
	Enemy->GetWorldTimerManager().ClearTimer(DetonateTimerHandle);
	Enemy->SetHaloGlowIntensity(0.0f);
	Enemy->ReleaseChargeExecution();
}

FVector UGuardActivateAttackExecution::GetChargeOrigin(const AEnemyCharacter* Enemy) const
{
	if (!Enemy) return FVector::ZeroVector;

	const USkeletalMeshComponent* MeshComp = Enemy->GetMesh();
	if (!MuzzleSocketName.IsNone() && MeshComp && MeshComp->DoesSocketExist(MuzzleSocketName))
	{
		return MeshComp->GetSocketLocation(MuzzleSocketName);
	}
	return Enemy->GetActorLocation() + FVector(0.0f, 0.0f, OriginHeight);
}

void UGuardActivateAttackExecution::OnAttackEnd(AEnemyCharacter* Enemy)
{
	if (!Enemy->HasStateTag(TAG_State_Enemy_Guard)) return;

	// 中断時はガードを成立させず畳む。AIに触らないのは優先度の再評価が自分で次の状態を選ぶため
	if (bAttackInterrupted)
	{
		// 被弾由来の中断 (ひび等のリアクションがモンタージュを潰しただけ) はガード継続
		// 停止はStopLogic("Hit") が済ませており、リアクション後の再開もガードタグが抑止する
		if (Enemy->IsProcessingDamage())
		{
			if (ChargeDelay <= 0.0f)
			{
				Enemy->StartGuardAutoRelease();
			}
			return;
		}

		CancelCharge();
		Enemy->EndGuardNow(/*bRestartAI=*/false);
		return;
	}

	// ガード成立中は行動しない。解除はEndGuardNow /
	// ClearGuardReactionと対になる
	Enemy->SetReacting(true, TEXT("Guard"));
	if (AAIController* AIC = Cast<AAIController>(Enemy->GetController()))
	{
		AIC->StopMovement();
	}

	// チャージ中はガードの寿命をDetonate (ChargeDelay) が握る
	// 自動解除/CheckGuardReleaseを起動するとChargeDelayより前にガードが終わり
	// 暴発しないため、チャージ時はStartGuardAutoReleaseを呼ばない
	// (早期終了する経路はプレイヤーのガード破壊のみになる)
	if (ChargeDelay <= 0.0f)
	{
		Enemy->StartGuardAutoRelease();
	}
}

// ------------------------------------------------------------
// UHaloStepAttackExecution
// ------------------------------------------------------------

void UHaloStepAttackExecution::OnAttackBegin(AEnemyCharacter* Enemy)
{
	bStepRight = FMath::RandBool();

	// PCへの方向から垂直方向を計算(敵の現在の向きに依存しない)
	{
		const AActor* Target = Enemy->GetTargetActor();
		const FVector ToPlayer = Target
			? (Target->GetActorLocation() - Enemy->GetActorLocation()).GetSafeNormal2D()
			: Enemy->GetActorForwardVector().GetSafeNormal2D();
		// CrossProduct(Up, ToPlayer) = プレイヤー方向に対して垂直右方向
		const FVector PerpRight = FVector::CrossProduct(FVector::UpVector, ToPlayer).GetSafeNormal();
		CachedStepDirection = PerpRight * (bStepRight ? 1.0f : -1.0f);
	}

	UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(Enemy->GetWorld());
	if (NavSys)
	{
		const FVector TargetPos = Enemy->GetActorLocation() + CachedStepDirection * StepNavCheckDistance;
		FNavLocation NavLoc;
		if (!NavSys->ProjectPointToNavigation(TargetPos, NavLoc, FVector(50.0f, 50.0f, 200.0f)))
		{
			if (FinishDelegate) FinishDelegate(false);
			return;
		}
	}

	UAnimMontage* Montage = bStepRight ? RightMontage.Get() : LeftMontage.Get();
	if (!Montage) { if (FinishDelegate) FinishDelegate(false); return; }

	UAnimInstance* AnimInst = Enemy->GetMesh()->GetAnimInstance();
	if (!AnimInst) { if (FinishDelegate) FinishDelegate(false); return; }

	CachedAnimInstance = AnimInst;
	ActiveMontage = Montage;

	PreStepMaxWalkSpeed = Enemy->GetCharacterMovement()->MaxWalkSpeed;
	Enemy->GetCharacterMovement()->MaxWalkSpeed = StepSpeed;

	// 回転補正をモンタージュ再生直後に適用する
	// 右:ステップ方向を向く / 左:モーションを流用してプレイヤーと逆方向を向く
	UCharacterMovementComponent* Move = Enemy->GetCharacterMovement();
	bSavedUseControllerDesiredRotation = Move->bUseControllerDesiredRotation;
	Move->bUseControllerDesiredRotation = false;
	bRotationOverridden = true;

	// 右: PCに対して垂直右方向 / 左: PCに対して垂直左方向
	Enemy->SetActorRotation(CachedStepDirection.Rotation());

	AnimInst->OnMontageEnded.AddDynamic(this, &UHaloStepAttackExecution::OnStepMontageEnded);
	AnimInst->Montage_Play(Montage);
	// 移動はHaloStepStart AnimNotifyで開始する
}

void UHaloStepAttackExecution::OnAttackEvent(AEnemyCharacter* Enemy, FGameplayTag EventTag)
{
	if (EventTag == TAG_AttackEvent_HaloStepStart)
	{
		bStepping = true;
	}
	else if (EventTag == TAG_AttackEvent_HaloStepEnd)
	{
		bStepping = false;
		RestoreRotation(Enemy);
	}
}

void UHaloStepAttackExecution::OnAttackTick(AEnemyCharacter* Enemy, float DeltaTime)
{
	if (!bStepping) return;

	Enemy->AddMovementInput(CachedStepDirection, 1.0f);
}

void UHaloStepAttackExecution::OnAttackEnd(AEnemyCharacter* Enemy)
{
	bStepping = false;

	if (PreStepMaxWalkSpeed > 0.0f)
	{
		Enemy->GetCharacterMovement()->MaxWalkSpeed = PreStepMaxWalkSpeed;
		PreStepMaxWalkSpeed = 0.0f;
	}

	RestoreRotation(Enemy);

	if (CachedAnimInstance.IsValid())
	{
		CachedAnimInstance->OnMontageEnded.RemoveDynamic(
			this, &UHaloStepAttackExecution::OnStepMontageEnded);
	}
}

void UHaloStepAttackExecution::RestoreRotation(AEnemyCharacter* Enemy)
{
	if (!bRotationOverridden) return;

	Enemy->GetCharacterMovement()->bUseControllerDesiredRotation = bSavedUseControllerDesiredRotation;
	bRotationOverridden = false;
}

void UHaloStepAttackExecution::OnStepMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	if (Montage != ActiveMontage.Get()) return;

	if (CachedAnimInstance.IsValid())
	{
		CachedAnimInstance->OnMontageEnded.RemoveDynamic(
			this, &UHaloStepAttackExecution::OnStepMontageEnded);
	}

	if (FinishDelegate) FinishDelegate(!bInterrupted);
}

// ------------------------------------------------------------
// UHaloRotationAttackExecution
// ------------------------------------------------------------

void UHaloRotationAttackExecution::OnAttackEvent(AEnemyCharacter* Enemy, FGameplayTag EventTag)
{
	if (EventTag == TAG_AttackEvent_HaloAttackStart)
	{
		// 攻撃開始時に折り返し区間の回数を [Min, Max] の乱数で確定する
		// (Min/Max逆転も安全に丸める)。以降の折り返しはTurnIntervalごとのループタイマーが
		// 駆動する(HaloAttackStartは1回のみ)
		if (!bCycleInitialized)
		{
			const int32 MinCount = FMath::Max(1, FMath::Min(RepeatCountMin, RepeatCountMax));
			const int32 MaxCount = FMath::Max(MinCount, FMath::Max(RepeatCountMin, RepeatCountMax));
			RepeatsRemaining = FMath::RandRange(MinCount, MaxCount);
			bCycleInitialized = true;
		}

		CachedEnemy = Enemy;
		bMoving = true;
		ChargeElapsed = 0.0f;
		SegmentElapsed = 0.0f;
		AbortTimer = 0.0f;
		bWindingUp = false;
		WindupElapsed = 0.0f;
		IgnoredEnemies.Reset();

		// 攻撃中の常駐VFXを生成(OnAttackEndで破棄)
		StartLoopVFX(Enemy);

		// 初回は現在の向きへ突進する (以降の切り返しは追い越し検知or TurnIntervalが駆動)
		ChargeDirection = Enemy->GetActorForwardVector();

		// CMCによる向き上書きを止め、ChargeDirectionにアクター向きを固定する
		UCharacterMovementComponent* CMCLocal = Enemy->GetCharacterMovement();
		bSavedUseControllerDesiredRotation = CMCLocal->bUseControllerDesiredRotation;
		CMCLocal->bUseControllerDesiredRotation = false;
		bRotationOverridden = true;
		Enemy->SetActorRotation(ChargeDirection.Rotation());

		// バンク用にメッシュの初期相対回転を控え、以降ロールを上乗せする
		CurrentBankRoll = 0.0f;
		BankVelocity = 0.0f;
		PrevChargeYaw = ChargeDirection.Rotation().Yaw;
		if (USkeletalMeshComponent* Mesh = Enemy->GetMesh())
		{
			MeshBaseRelRot = Mesh->GetRelativeRotation();
			bMeshBankActive = true;
		}

		if (MoveSpeed > 0.0f)
		{
			PreMoveMaxWalkSpeed = Enemy->GetCharacterMovement()->MaxWalkSpeed;
			Enemy->GetCharacterMovement()->MaxWalkSpeed = MoveSpeed;
		}

		// ルートモーション系は全フレームVelocity=0を強制するため使用不可
		// SafeMoveUpdatedComponentで毎Tick直接位置を動かす方式に切り替え
		// lp中のアニメーション平行移動ルートモーションを無効化し、ed遷移時に復元する
		PreAnimRMTranslationScale = Enemy->GetAnimRootMotionTranslationScale();
		Enemy->SetAnimRootMotionTranslationScale(0.0f);
	}
}

void UHaloRotationAttackExecution::OnAttackTick(AEnemyCharacter* Enemy, float DeltaTime)
{
	if (!bMoving) return;

	// --- 切り返しの溜め中: 移動せずその場で徐々に旋回する。光輪(モンタージュ)は回り続ける ---
	if (bWindingUp)
	{
		WindupElapsed += DeltaTime;
		const float RawAlpha = (CutbackWindupTime > 0.0f)
			? FMath::Clamp(WindupElapsed / CutbackWindupTime, 0.0f, 1.0f) : 1.0f;
		// ease-in-outでグググっと溜めてから向き直す
		const float Alpha = FMath::SmoothStep(0.0f, 1.0f, RawAlpha);

		// 溜め中もPCへ動的追従: 目標方向を毎フレーム更新する
		// ただし近距離 (HomingMinDistance以下) では追従を切り、
		// 溜め開始時の狙いにコミットする
		if (GetTargetDistance2D(Enemy) > HomingMinDistance)
		{
			const FVector NewTarget = GetPredictedTargetDirection(Enemy);
			if (!NewTarget.IsNearlyZero())
			{
				WindupTargetDir = NewTarget;
			}
		}

		ChargeDirection = FQuat::Slerp(
			WindupStartDir.ToOrientationQuat(),
			WindupTargetDir.ToOrientationQuat(), Alpha).GetForwardVector();
		Enemy->SetActorRotation(ChargeDirection.Rotation());

		// その場停止 (残留速度で滑らないよう明示的に0)
		Enemy->GetCharacterMovement()->Velocity = FVector::ZeroVector;

		// 旋回に応じて傾ける
		UpdateBanking(Enemy, DeltaTime);

		if (RawAlpha >= 1.0f)
		{
			// 溜め完了: 新方向へ突進再開
			bWindingUp = false;
			ChargeDirection = WindupTargetDir;
			SegmentElapsed = 0.0f;
			AbortTimer = 0.0f;
		}
		return; // 溜め中は移動しない
	}

	ChargeElapsed += DeltaTime;
	SegmentElapsed += DeltaTime;

	// --- ホーミング: 各区間の開始からHomingDuration秒だけ、
	// PC予測方向へ旋回上限つきで寄せる ---
	// (SegmentElapsed基準なので初回だけでなく切り返し後の各突進にも効く)近距離
	// (HomingMinDistance以下) では理不尽になるためホーミングを切る
	if (SegmentElapsed < HomingDuration && HomingTurnRate > 0.0f
		&& GetTargetDistance2D(Enemy) > HomingMinDistance)
	{
		const FVector Desired = GetPredictedTargetDirection(Enemy);
		if (!Desired.IsNearlyZero())
		{
			ChargeDirection = FMath::VInterpNormalRotationTo(
				ChargeDirection, Desired, DeltaTime, HomingTurnRate);
			Enemy->SetActorRotation(ChargeDirection.Rotation());
		}
	}

	// --- 切り返し判定 ---PCを追い越した(角度超過がAbortHoldTime続いた) か、
	// 区間がTurnIntervalに達したら切り返す
	// AdvanceSegmentが残り回数を見て「再照準して継続」か「edで終了」かを決める
	bool bCutback = (SegmentElapsed >= TurnInterval);
	if (!bCutback && SegmentElapsed >= MinCommitTime)
	{
		const FVector ActualDir = GetTargetDirection(Enemy);
		if (!ActualDir.IsNearlyZero())
		{
			const float Dot = FMath::Clamp(FVector::DotProduct(ChargeDirection, ActualDir), -1.0f, 1.0f);
			const float AngleDeg = FMath::RadiansToDegrees(FMath::Acos(Dot));
			if (AngleDeg > AbortAngle)
			{
				AbortTimer += DeltaTime;
				if (AbortTimer >= AbortHoldTime) bCutback = true;
			}
			else
			{
				AbortTimer = 0.0f;
			}
		}
	}
	if (bCutback)
	{
		AdvanceSegment(Enemy);
		// edへ遷移(bMoving=false) or溜めに入った場合はこのフレームの移動をスキップ
		if (!bMoving || bWindingUp) return;
	}

	NavCheckTimer -= DeltaTime;
	if (NavCheckTimer <= 0.0f)
	{
		NavCheckTimer = NavCheckInterval;
		const FVector CheckPos = Enemy->GetActorLocation() + ChargeDirection * NavCheckDistance;
		UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(Enemy->GetWorld());
		FNavLocation NavLoc;
		// XYを50cmに絞ることで、穴の上から隣接地面のNavMeshへスナップするのを防ぐ
		const bool bNavFound = NavSys && NavSys->ProjectPointToNavigation(
			CheckPos, NavLoc, FVector(50.0f, 50.0f, 200.0f));
		bool bDangerous = !bNavFound;
		if (!bDangerous)
		{
			// 投影先が足元よりNavCheckMaxFallHeight cm以上低ければ崖/穴と判定
			const float FeetZ = Enemy->GetActorLocation().Z
				- Enemy->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
			bDangerous = (FeetZ - NavLoc.Location.Z) > NavCheckMaxFallHeight;
		}
		if (bDangerous)
		{
			ChargeDirection = -ChargeDirection;
			Enemy->SetActorRotation(ChargeDirection.Rotation());
		}
	}

	UCharacterMovementComponent* CMC = Enemy->GetCharacterMovement();
	FVector DeltaMove = ChargeDirection * CMC->MaxWalkSpeed * DeltaTime;

	// 坂道でも水平速度を維持する: 接地面の法線に沿ってZを補正し、
	// 斜面を上り下りする(CMCのランプ移動ComputeGroundMovementDeltaと同等
	// 歩行可能な床のみ対象)
	if (CMC->CurrentFloor.IsWalkableFloor())
	{
		const FVector FloorNormal = CMC->CurrentFloor.HitResult.Normal;
		if (FloorNormal.Z > KINDA_SMALL_NUMBER)
		{
			const FVector HorizDelta(DeltaMove.X, DeltaMove.Y, 0.0f);
			DeltaMove.Z = -FVector::DotProduct(FloorNormal, HorizDelta) / FloorNormal.Z;
		}
	}

	FHitResult HitResult;
	CMC->SafeMoveUpdatedComponent(DeltaMove, Enemy->GetActorQuat(), true, HitResult);

	// 他の敵(EM)に当たっても止まらない: ぶつかった敵を移動無視に追加し、残りを詰めてすり抜ける
	if (HitResult.bBlockingHit)
	{
		if (AEnemyCharacter* OtherEM = Cast<AEnemyCharacter>(HitResult.GetActor()))
		{
			if (UCapsuleComponent* Cap = Enemy->GetCapsuleComponent())
			{
				Cap->IgnoreActorWhenMoving(OtherEM, true);
			}
			IgnoredEnemies.Add(OtherEM);

			const FVector Remaining = DeltaMove * (1.0f - HitResult.Time);
			FHitResult Hit2;
			CMC->SafeMoveUpdatedComponent(Remaining, Enemy->GetActorQuat(), true, Hit2);
		}
	}

	// 進行方向の旋回に応じてメッシュをバンクさせる(向きが確定した後に適用)
	UpdateBanking(Enemy, DeltaTime);
}

void UHaloRotationAttackExecution::OnAttackEnd(AEnemyCharacter* Enemy)
{
	bMoving = false;

	RestoreMeshBank(Enemy);
	StopLoopVFX();

	// すり抜け用に無視していた敵の移動無視を解除する
	if (UCapsuleComponent* Cap = Enemy->GetCapsuleComponent())
	{
		for (const TWeakObjectPtr<AActor>& Ignored : IgnoredEnemies)
		{
			if (Ignored.IsValid())
			{
				Cap->IgnoreActorWhenMoving(Ignored.Get(), false);
			}
		}
	}
	IgnoredEnemies.Reset();

	if (bRotationOverridden)
	{
		Enemy->GetCharacterMovement()->bUseControllerDesiredRotation = bSavedUseControllerDesiredRotation;
		bRotationOverridden = false;
	}

	if (PreAnimRMTranslationScale >= 0.0f)
	{
		Enemy->SetAnimRootMotionTranslationScale(PreAnimRMTranslationScale);
		PreAnimRMTranslationScale = -1.0f;
	}

	if (PreMoveMaxWalkSpeed > 0.0f)
	{
		Enemy->GetCharacterMovement()->MaxWalkSpeed = PreMoveMaxWalkSpeed;
		PreMoveMaxWalkSpeed = 0.0f;
	}
}

void UHaloRotationAttackExecution::JumpToEnd()
{
	if (!CachedEnemy.IsValid()) return;

	bMoving = false;

	StopLoopVFX();
	RestoreMeshBank(CachedEnemy.Get());

	// edセクションのルートモーション(着地)が正常に動くようスケールを復元する
	if (PreAnimRMTranslationScale >= 0.0f)
	{
		CachedEnemy->SetAnimRootMotionTranslationScale(PreAnimRMTranslationScale);
		PreAnimRMTranslationScale = -1.0f;
	}

	if (PreMoveMaxWalkSpeed > 0.0f)
	{
		CachedEnemy->GetCharacterMovement()->MaxWalkSpeed = PreMoveMaxWalkSpeed;
		PreMoveMaxWalkSpeed = 0.0f;
	}

	if (bRotationOverridden)
	{
		CachedEnemy->GetCharacterMovement()->bUseControllerDesiredRotation = bSavedUseControllerDesiredRotation;
		bRotationOverridden = false;
	}

	// 折り返しを終え、ed(着地)へ遷移して攻撃を終える
	if (UAnimInstance* Anim = CachedEnemy->GetMesh()->GetAnimInstance())
		Anim->Montage_JumpToSection(FName("ed"));
}

void UHaloRotationAttackExecution::AdvanceSegment(AEnemyCharacter* Enemy)
{
	// 残り回数があれば切り返し、無ければedで終了
	if (--RepeatsRemaining > 0)
	{
		const FVector Dir = GetPredictedTargetDirection(Enemy);
		if (CutbackWindupTime > 0.0f && !Dir.IsNearlyZero())
		{
			// 溜めフェーズへ: その場で停止し、傾きながら徐々に新方向へ旋回する (実回転はTick)
			bWindingUp = true;
			WindupElapsed = 0.0f;
			WindupStartDir = ChargeDirection;
			WindupTargetDir = Dir;
		}
		else
		{
			// 溜めなし: 即座に向き直して次の突進へ
			if (!Dir.IsNearlyZero())
			{
				ChargeDirection = Dir;
				Enemy->SetActorRotation(Dir.Rotation());
			}
			SegmentElapsed = 0.0f;
			AbortTimer = 0.0f;
		}
	}
	else
	{
		JumpToEnd();
	}
}

void UHaloRotationAttackExecution::UpdateBanking(AEnemyCharacter* Enemy, float DeltaTime)
{
	if (!bMeshBankActive || DeltaTime <= 0.0f) return;

	USkeletalMeshComponent* Mesh = Enemy->GetMesh();
	if (!Mesh) return;

	// 進行方向の旋回速度(deg/s)に応じて目標バンク角を決め、なめらかに追従させる
	const float CurYaw = ChargeDirection.Rotation().Yaw;
	const float DeltaYaw = FMath::FindDeltaAngleDegrees(PrevChargeYaw, CurYaw);
	PrevChargeYaw = CurYaw;

	const float YawRate = DeltaYaw / DeltaTime;
	const float Norm = FMath::Clamp(YawRate / FMath::Max(1.0f, BankSensitivity), -1.0f, 1.0f);
	float TargetRoll = Norm * MaxBankAngle;
	if (bInvertBank) TargetRoll = -TargetRoll;

	// バネ・ダンパで慣性をもって目標へ寄せる (行きも戻りもじわっと加減速)
	// BankInterpSpeed = 固有振動数Omega、
	// BankDamping = 減衰比 (1で臨界減衰)
	const float Omega = FMath::Max(0.0f, BankInterpSpeed);
	const float Accel = Omega * Omega * (TargetRoll - CurrentBankRoll) - 2.0f * BankDamping * Omega * BankVelocity;
	BankVelocity += Accel * DeltaTime;
	CurrentBankRoll += BankVelocity * DeltaTime;

	// アクター前方(X)軸まわりのロールをメッシュ初期姿勢に上乗せする
	const FQuat BankQ(FVector::ForwardVector, FMath::DegreesToRadians(CurrentBankRoll));
	Mesh->SetRelativeRotation(BankQ * MeshBaseRelRot.Quaternion());
}

void UHaloRotationAttackExecution::RestoreMeshBank(AEnemyCharacter* Enemy)
{
	if (!bMeshBankActive) return;

	CurrentBankRoll = 0.0f;
	BankVelocity = 0.0f;
	if (Enemy)
	{
		if (USkeletalMeshComponent* Mesh = Enemy->GetMesh())
		{
			Mesh->SetRelativeRotation(MeshBaseRelRot);
		}
	}
	bMeshBankActive = false;
}

void UHaloRotationAttackExecution::StartLoopVFX(AEnemyCharacter* Enemy)
{
	if (!LoopVFX || !Enemy || LoopVFXComp.IsValid()) return;

	// 骨ではなくRootにアタッチ。アクターの向き＝進行方向なので、
	// 相対オフセット/回転は自動的に進行方向基準になる
	USceneComponent* Root = Enemy->GetRootComponent();
	if (!Root) return;

	UNiagaraComponent* NC = UNiagaraFunctionLibrary::SpawnSystemAttached(
		LoopVFX, Root, NAME_None,
		LoopVFXOffset, LoopVFXRotation,
		EAttachLocation::KeepRelativeOffset, /*bAutoDestroy=*/true);
	if (NC)
	{
		// TransformのスケールではなくNiagaraのUser floatパラメータ "Scale"
		// に流す
		NC->SetVariableFloat(FName("Scale"), LoopVFXScale);
		LoopVFXComp = NC;
	}
}

void UHaloRotationAttackExecution::StopLoopVFX()
{
	if (LoopVFXComp.IsValid())
	{
		// Deactivateで新規発生だけ止め、残存パーティクルは寿命で自然に消えるのに任せる
		// (Niagara側でディアクティベート時の消え方が調整済み)
		// bAutoDestroyで消滅後に自動破棄される
		LoopVFXComp->Deactivate();
	}
	LoopVFXComp.Reset();
}

FVector UHaloRotationAttackExecution::GetTargetDirection(AEnemyCharacter* Enemy) const
{
	AActor* Target = Enemy->GetTargetActor();

	if (!Target) return Enemy->GetActorForwardVector();

	FVector Dir = Target->GetActorLocation() - Enemy->GetActorLocation();
	Dir.Z = 0.0f;
	return Dir.GetSafeNormal();
}

FVector UHaloRotationAttackExecution::GetPredictedTargetDirection(AEnemyCharacter* Enemy) const
{
	AActor* Target = Enemy->GetTargetActor();

	if (!Target) return Enemy->GetActorForwardVector();

	// PCの現在速度からPredictLeadTime秒先の位置を線形予測して狙う
	const FVector PredictedPos = Target->GetActorLocation() + Target->GetVelocity() * PredictLeadTime;

	FVector Dir = PredictedPos - Enemy->GetActorLocation();
	Dir.Z = 0.0f;
	return Dir.GetSafeNormal();
}

float UHaloRotationAttackExecution::GetTargetDistance2D(AEnemyCharacter* Enemy) const
{
	AActor* Target = Enemy->GetTargetActor();

	if (!Target) return -1.0f;

	return FVector::Dist2D(Target->GetActorLocation(), Enemy->GetActorLocation());
}
