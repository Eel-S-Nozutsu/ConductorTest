// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#include "PRJ_TIDE_P0/Components/Combat/HitReactionComponent.h"
#include "PRJ_TIDE_P0/Interfaces/Combat/IDamageable.h"
#include "PRJ_TIDE_P0/Components/Combat/DamageSystemComponent.h"
#include "PRJ_TIDE_P0/Components/Combat/StatusComponent.h"
#include "PRJ_TIDE_P0/Data/Combat/DamageInfo.h"
#include "PRJ_TIDE_P0/Data/Enemy/EnemyDataAsset.h"
#include "PRJ_TIDE_P0/Actors/Character/TideCharacter.h"
#include "PRJ_TIDE_P0/Core/TideGameSettings.h"
#include "PRJ_TIDE_P0/Data/Tags/TideGameplayTags.h"

#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SphereComponent.h"
#include "NavigationSystem.h"
#include "NiagaraFunctionLibrary.h"

// ---------------------------------------------------------------------------
// static helpers
// ---------------------------------------------------------------------------


UAnimMontage* UHitReactionComponent::PickRecoveryMontage(
	const TArray<FBlowbackRecoveryEntry>& Recoveries, int32& LastIndex,
	const ACharacter* Enemy, float NavCheckDistance)
{
	if (Recoveries.IsEmpty()) return nullptr;

	UNavigationSystemV1* NavSys = (NavCheckDistance > 0.0f)
		? UNavigationSystemV1::GetCurrent(Enemy->GetWorld()) : nullptr;

	const FVector FootPos = Enemy->GetActorLocation()
		- FVector(0.0f, 0.0f, Enemy->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
	const FQuat ActorQuat = Enemy->GetActorQuat();

	TArray<int32> Valid;
	Valid.Reserve(Recoveries.Num());
	for (int32 i = 0; i < Recoveries.Num(); ++i)
	{
		const FBlowbackRecoveryEntry& Entry = Recoveries[i];
		if (!Entry.Montage) continue;

		if (NavSys && !Entry.LocalMoveDir.IsNearlyZero())
		{
			const FVector WorldDir = ActorQuat.RotateVector(
				FVector(Entry.LocalMoveDir.X, Entry.LocalMoveDir.Y, 0.0f)).GetSafeNormal();
			const FVector CheckPos = FootPos + WorldDir * NavCheckDistance;
			FNavLocation NavLoc;
			if (!NavSys->ProjectPointToNavigation(CheckPos, NavLoc, FVector(50.0f, 50.0f, 200.0f)))
				continue;
		}
		Valid.Add(i);
	}
	if (Valid.IsEmpty()) return nullptr;

	TArray<int32> Candidates;
	for (int32 i : Valid)
	{
		if (i != LastIndex) Candidates.Add(i);
	}
	if (Candidates.IsEmpty()) Candidates = Valid;

	const int32 Picked = Candidates[FMath::RandRange(0, Candidates.Num() - 1)];
	LastIndex = Picked;
	return Recoveries[Picked].Montage.Get();
}

// ---------------------------------------------------------------------------
// Initialize
// ---------------------------------------------------------------------------

void UHitReactionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// 吹き飛び中の速度をキャッシュ(壁バウンド時の反射計算に使用)
	if (BlowbackPhase == EBlowbackPhase::Start || BlowbackPhase == EBlowbackPhase::Loop)
	{
		if (ACharacter* Character = Cast<ACharacter>(GetOwner()))
		{
			if (UCharacterMovementComponent* Move = Character->GetCharacterMovement())
			{
				BlowbackVelocityCache = Move->Velocity;
			}
		}
	}

	// 巻き上げ中の空中アニメ (Airborne/Falling) を実速度で切り替える
	UpdateWindAerialAnim();

	// 遷移イベント取りこぼしでWindPhaseが畳まれない棒立ちを監視・強制終了する
	UpdateWindReactionWatchdog(DeltaTime);
}

void UHitReactionComponent::InitializeFromData(const FHitReactionSettings& Settings)
{
	HitReactionSettings = Settings;
}

void UHitReactionComponent::InitializeReactionTable(const UDataTable* InTable)
{
	TableSingleSettings.Reactions.Reset();
	TableSequences.Reset();

	if (!InTable) return;
	if (InTable->GetRowStruct() != FHitReactionRow::StaticStruct()) return;

	// 起き上がりセット行を引いてRecoveries / NavCheckDistanceを取り出す
	auto ResolveRecoverySet = [](const FDataTableRowHandle& Handle,
		TArray<FBlowbackRecoveryEntry>& OutRecoveries, float& OutNavDistance)
	{
		if (Handle.IsNull()) return false;
		const FRecoverySetRow* Set = Handle.GetRow<FRecoverySetRow>(TEXT("HitReactionTable"));
		if (!Set) return false;

		OutRecoveries  = Set->Recoveries;
		OutNavDistance = Set->NavCheckDistance;
		return true;
	};

	TArray<FHitReactionRow*> Rows;
	InTable->GetAllRows<FHitReactionRow>(TEXT("HitReactionTable"), Rows);

	for (const FHitReactionRow* Row : Rows)
	{
		if (!Row || !Row->ReactionTag.IsValid()) continue;

		// 単発・多段を問わずエントリを作る。HitEffect
		// (SuperArmor時のVFX)とLaunchForce/LaunchUpForce
		// (多段の開始処理がFindEntryByTagで引く吹き飛ばし力) はKindによらず必要なため、
		// Sequence行も必ずここへ登録する
		// Sequence行はMontagesが空なので、単発として誤再生されることはない
		{
			FHitReactionEntry Entry;
			Entry.ReactionTag   = Row->ReactionTag;
			Entry.Montage       = Row->Montage;
			Entry.HitEffect     = Row->HitEffect;
			Entry.LaunchForce   = Row->LaunchForce;
			Entry.LaunchUpForce = Row->LaunchUpForce;
			Entry.bFaceAttacker = Row->bFaceAttacker;
			TableSingleSettings.Reactions.Add(MoveTemp(Entry));
		}

		if (Row->Kind == EHitReactionKind::Single) continue;

		// 多段: 既存ドライバがそのまま食えるFBlowbackAnimSequenceへ組み立てる
		// 空中ループが1種類のドライバ (吹き飛び) はLoopだけを見るのでRiseLoopを入れる
		FBlowbackAnimSequence Seq;
		Seq.Start = Row->Start;
		Seq.Loop  = Row->RiseLoop;
		Seq.Land  = Row->Land;
		Seq.WallHitStart = Row->WallHitStart;
		Seq.WallHitLoop  = Row->WallHitLoop;
		Seq.WallHitLand  = Row->WallHitLand;
		Seq.WallHitBounceRestitution     = Row->WallHitBounceRestitution;
		Seq.bSuppressAirborneRootMotion  = Row->bSuppressAirborneRootMotion;
		Seq.CollisionDamage              = Row->CollisionDamage;
		Seq.CollisionHitReactionTag      = Row->CollisionHitReactionTag;

		ResolveRecoverySet(Row->RecoverySet, Seq.Recoveries, Seq.RecoveryNavCheckDistance);

		// 壁衝突用が未設定なら通常の起き上がりへフォールバック (DA直設定と同じ規約)
		float WallHitNavDistance = Seq.RecoveryNavCheckDistance;
		if (!ResolveRecoverySet(Row->WallHitRecoverySet, Seq.WallHitRecoveries, WallHitNavDistance))
		{
			Seq.WallHitRecoveries.Reset();
		}

		TableSequences.Emplace(Row->ReactionTag, MoveTemp(Seq));
	}
}

void UHitReactionComponent::InitializeWindReaction()
{
	// 巻き上げもHitReaction.Wind行から解決する
	// 行型は吹き飛びと共通なのでRiseLoop/FallLoop/RecoverySetから読み替える
	const FBlowbackAnimSequence* Seq = FindTableSequence(TAG_HitReaction_Wind);
	if (!Seq) return;

	WindStartMontage    = Seq->Start;
	WindAirLoopMontage  = Seq->Loop;
	// 落下ループ未設定なら上昇ループを流用する(行型の規約)
	WindFallLoopMontage = Seq->WallHitLoop ? Seq->WallHitLoop : Seq->Loop;
	WindLandMontage     = Seq->Land;

	// 起き上がりはセットの先頭を使う(巻き上げは単発Recoveryのため)
	WindRecoveryMontage = Seq->Recoveries.IsEmpty() ? nullptr : Seq->Recoveries[0].Montage;

	// 打ち上げ力は行のCommon列(吹き飛びと同じLaunchUpForce)から取る
	if (const FHitReactionEntry* Entry = FindEntryByTag(TAG_HitReaction_Wind))
	{
		WindLaunchUpForce = Entry->LaunchUpForce;
	}
}

float UHitReactionComponent::BeginWindReaction()
{
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (!Character) return 0.0f;

	// 初回捕捉時のみ開始する(既に巻き上げ中なら何もしない)
	if (WindPhase != EWindReactionPhase::None) return 0.0f;

	// 空中の移動は竜巻のvelocity override / 打ち上げ物理で駆動するため、
	// モンタージュのroot motion移動を抑制する(着地で復帰)
	if (!bWindRMSuppressed)
	{
		PreWindRMScale = Character->GetAnimRootMotionTranslationScale();
		Character->SetAnimRootMotionTranslationScale(0.0f);
		bWindRMSuppressed = true;
	}

	LastWindAnimSwitchTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;

	// 物理打ち上げモード: 竜巻の持続揚力ではなく、上向きインパルスで撃ち出して重力で放物線を描く
	bWindPhysicsLaunch = (WindLaunchUpForce > 0.0f);
	bWindLaunchPeaked  = false;
	if (bWindPhysicsLaunch)
	{
		// 撃ち出しは所有側が行う(戻り値)。着地検出はここで張る
		Character->LandedDelegate.AddUniqueDynamic(this, &UHitReactionComponent::OnBlowbackLanded);
	}

	if (WindStartMontage)
	{
		// 打ち上げ開始を1回再生。ブレンドアウトでAirborne(AirLoop) へ繋ぐ
		WindPhase = EWindReactionPhase::Launching;
		PlayWindMontage(WindStartMontage);
	}
	else
	{
		WindPhase = EWindReactionPhase::Airborne;
		PlayWindMontage(WindAirLoopMontage);
	}

	return bWindPhysicsLaunch ? WindLaunchUpForce : 0.0f;
}

void UHitReactionComponent::OnWindReleased()
{
	if (WindPhase == EWindReactionPhase::None) return;

	// 死亡・吹き飛びに奪われた場合は落下リアクションを行わない(各処理側に委ねる)
	if (StatusComponent && StatusComponent->IsDead()) { AbortWindReaction(); return; }
	if (IsInBlowback()) { AbortWindReaction(); return; }

	// 風から外れただけ。空中アニメの切り替えはUpdateWindAerialAnimが実速度で行う
	// ここでは着地待ちのバインドだけ行う(バウンドで複数回呼ばれてもAddUniqueで二重束縛しない)
	if (ACharacter* Character = Cast<ACharacter>(GetOwner()))
	{
		Character->LandedDelegate.AddUniqueDynamic(this, &UHitReactionComponent::OnBlowbackLanded);
	}
}

void UHitReactionComponent::AbortWindReaction()
{
	RestoreWindRootMotion();
	WindPhase = EWindReactionPhase::None;
	bWindPhysicsLaunch = false;
	bWindLaunchPeaked  = false;
}

void UHitReactionComponent::EndWindReaction()
{
	// 通常フローでは着地時に復帰済みだが、着地イベント取りこぼしで
	// ここへ来た場合に備えて冪等に戻す
	RestoreWindRootMotion();
	WindPhase = EWindReactionPhase::None;
	bWindPhysicsLaunch = false;
	bWindLaunchPeaked  = false;
	OnWindReactionEnded.Broadcast();
}

void UHitReactionComponent::PlayWindMontage(UAnimMontage* Montage)
{
	if (!Montage || !AnimInstance) return;
	AnimInstance->Montage_Play(Montage);
}

void UHitReactionComponent::RestoreWindRootMotion()
{
	if (!bWindRMSuppressed) return;
	if (ACharacter* Character = Cast<ACharacter>(GetOwner()))
	{
		Character->SetAnimRootMotionTranslationScale(PreWindRMScale);
	}
	bWindRMSuppressed = false;
}

void UHitReactionComponent::UpdateWindAerialAnim()
{
	// 空中フェーズ (Airborne / Falling) のみ動きに追従させる
	// Landing/Recoveryはモンタージュ駆動
	if (WindPhase != EWindReactionPhase::Airborne && WindPhase != EWindReactionPhase::Falling) return;

	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (!Character) return;

	EWindReactionPhase Desired = WindPhase;

	if (bWindCaptured)
	{
		// 風に捕捉中 (上昇・滞空) は常にAirLoop
		Desired = EWindReactionPhase::Airborne;
	}
	else
	{
		// 風の外: 実際の上下速度で判定。両しきい値の間 (apex付近) は現状維持してチラつきを防ぐ
		const float VelZ = Character->GetCharacterMovement() ? Character->GetCharacterMovement()->Velocity.Z : 0.0f;
		if (VelZ < -WindFallVelThreshold)     Desired = EWindReactionPhase::Falling;
		else if (VelZ > WindRiseVelThreshold) Desired = EWindReactionPhase::Airborne;
	}

	if (Desired == WindPhase) return;

	// 最小ドウェル: 直近の切り替えから一定時間は切り替えない(境界での高速トグル抑制)
	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	if (Now - LastWindAnimSwitchTime < WindAnimMinDwell) return;
	LastWindAnimSwitchTime = Now;

	WindPhase = Desired;
	PlayWindMontage(Desired == EWindReactionPhase::Airborne ? WindAirLoopMontage : WindFallLoopMontage);
}

void UHitReactionComponent::UpdateWindReactionWatchdog(float DeltaTime)
{
	if (WindPhase == EWindReactionPhase::None)
	{
		WindGroundedStuckTime = 0.0f;
		return;
	}

	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	const UCharacterMovementComponent* Move = Character ? Character->GetCharacterMovement() : nullptr;
	if (!Move) return;

	// 空中にいる間は正常。接地して初めて「畳まれるべき」区間に入る
	if (!Move->IsMovingOnGround())
	{
		WindGroundedStuckTime = 0.0f;
		return;
	}

	// 接地後の正規の着地/起き上がりモンタージュ再生中は順当な進行なので猶予する。
	// 空中フェーズ (Launching/Airborne/Falling) なのに接地しているのは着地イベントの
	// 取りこぼしなので、モンタージュ再生に関わらず異常として計時する
	if (WindPhase == EWindReactionPhase::Landing || WindPhase == EWindReactionPhase::Recovery)
	{
		UAnimMontage* PhaseMontage = (WindPhase == EWindReactionPhase::Landing)
			? WindLandMontage : WindRecoveryMontage;
		if (AnimInstance && PhaseMontage && AnimInstance->Montage_IsPlaying(PhaseMontage))
		{
			WindGroundedStuckTime = 0.0f;
			return;
		}
	}

	// 接地したままWindPhaseが終了しない = 着地/ブレンドアウトイベントの取りこぼし。
	// React(Wind)への居座り (棒立ち) を断つため強制終了してAI再開を通知する
	WindGroundedStuckTime += DeltaTime;
	if (WindGroundedStuckTime >= WindStuckTimeout)
	{
		WindGroundedStuckTime = 0.0f;
		EndWindReaction();
	}
}

void UHitReactionComponent::OnWindLanded()
{
	// 接地したので空中フェーズのRM抑制を解除する(Land/RecoveryのRMを活かす)
	RestoreWindRootMotion();

	// 空中リアクション中 (打ち上げ開始/上昇/落下いずれでも) の接地で着地モーションへ
	if (WindPhase != EWindReactionPhase::Launching
		&& WindPhase != EWindReactionPhase::Airborne
		&& WindPhase != EWindReactionPhase::Falling) return;

	if (StatusComponent && StatusComponent->IsDead()) { AbortWindReaction(); return; }
	if (IsInBlowback()) { AbortWindReaction(); return; }

	if (WindLandMontage)
	{
		WindPhase = EWindReactionPhase::Landing;
		PlayWindMontage(WindLandMontage);
		return;
	}

	// 着地モーションが無ければ起き上がりへ直行、それも無ければ終了
	if (WindRecoveryMontage)
	{
		WindPhase = EWindReactionPhase::Recovery;
		PlayWindMontage(WindRecoveryMontage);
		return;
	}
	EndWindReaction();
}

void UHitReactionComponent::AdvanceWindPhase(UAnimMontage* Montage)
{
	switch (WindPhase)
	{
	case EWindReactionPhase::Launching:
		// 打ち上げ開始のブレンドアウトで滞空ループへ
		// 以降はUpdateWindAerialAnimが実速度で切替
		if (Montage == WindStartMontage)
		{
			WindPhase = EWindReactionPhase::Airborne;
			LastWindAnimSwitchTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
			PlayWindMontage(WindAirLoopMontage);
		}
		break;

	case EWindReactionPhase::Airborne:
		// 終了前に再生し直して隙間なくループ
		if (Montage == WindAirLoopMontage) PlayWindMontage(WindAirLoopMontage);
		break;

	case EWindReactionPhase::Falling:
		if (Montage == WindFallLoopMontage) PlayWindMontage(WindFallLoopMontage);
		break;

	case EWindReactionPhase::Landing:
		if (Montage == WindLandMontage)
		{
			if (WindRecoveryMontage)
			{
				WindPhase = EWindReactionPhase::Recovery;
				PlayWindMontage(WindRecoveryMontage);
			}
			else
			{
				EndWindReaction();
			}
		}
		break;

	case EWindReactionPhase::Recovery:
		if (Montage == WindRecoveryMontage) EndWindReaction();
		break;

	default:
		break;
	}
}

const FBlowbackAnimSequence* UHitReactionComponent::FindTableSequence(FGameplayTag Tag) const
{
	if (TableSequences.IsEmpty() || !Tag.IsValid()) return nullptr;

	// 完全一致を優先し、無ければTagの親に当たる行のうち最も具体的なものを採用する
	// (FindEntryByTagと同じ照合規約)
	const FBlowbackAnimSequence* Best = nullptr;
	int32 BestDepth = -1;

	for (const TPair<FGameplayTag, FBlowbackAnimSequence>& Pair : TableSequences)
	{
		if (Pair.Key == Tag) return &Pair.Value;

		if (Tag.MatchesTag(Pair.Key))
		{
			const int32 Depth = Pair.Key.GetGameplayTagParents().Num();
			if (Depth > BestDepth)
			{
				BestDepth = Depth;
				Best = &Pair.Value;
			}
		}
	}
	return Best;
}

void UHitReactionComponent::Initialize(UDamageSystemComponent* InDamageSystem, UStatusComponent* InStatusComponent)
{
	StatusComponent = InStatusComponent;
	CachedDamageSystem = InDamageSystem;

	// ストリーミング再入で残留束縛と衝突して二重束縛のensureが出るのを防ぐため
	// AddUniqueDynamicで束縛する(EndPlay側で確実に解除もする)
	InDamageSystem->OnDamageReceived.AddUniqueDynamic(this, &UHitReactionComponent::OnDamageReceived);

	if (ACharacter* Character = Cast<ACharacter>(GetOwner()))
	{
		AnimInstance = Character->GetMesh()->GetAnimInstance();

		Character->GetCapsuleComponent()->OnComponentHit.AddUniqueDynamic(
			this, &UHitReactionComponent::OnBlowbackWallHit);

		if (AnimInstance)
		{
			AnimInstance->OnMontageBlendingOut.AddUniqueDynamic(
				this, &UHitReactionComponent::OnBlowbackMontageBlendingOut);
		}
	}
}

void UHitReactionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// ストリームアウト・破棄のたびにInitializeで張った束縛を確実に外す
	// GC任せだとOnComponentHit等のスパースデリゲート束縛が残留し、再入時に
	// 二重束縛のensure(ScriptDelegates.h)を招く
	if (CachedDamageSystem)
	{
		CachedDamageSystem->OnDamageReceived.RemoveDynamic(this, &UHitReactionComponent::OnDamageReceived);
	}

	if (ACharacter* Character = Cast<ACharacter>(GetOwner()))
	{
		if (UCapsuleComponent* Capsule = Character->GetCapsuleComponent())
		{
			Capsule->OnComponentHit.RemoveDynamic(this, &UHitReactionComponent::OnBlowbackWallHit);
		}
		Character->LandedDelegate.RemoveDynamic(this, &UHitReactionComponent::OnBlowbackLanded);
	}

	if (USphereComponent* Collider = RagdollDamageCollider.Get())
	{
		Collider->OnComponentBeginOverlap.RemoveDynamic(this, &UHitReactionComponent::OnRagdollSphereOverlap);
	}

	if (AnimInstance)
	{
		AnimInstance->OnMontageBlendingOut.RemoveDynamic(this, &UHitReactionComponent::OnBlowbackMontageBlendingOut);
	}

	Super::EndPlay(EndPlayReason);
}

// ---------------------------------------------------------------------------
// Knockback系
// ---------------------------------------------------------------------------

FVector UHitReactionComponent::GetLaunchVelocity(const FDamageInfo& DamageInfo) const
{
#if !UE_BUILD_SHIPPING
	if (UTideGameSettings::Get()->bDebugDisableHitReaction) return FVector::ZeroVector;
#endif

	const FHitReactionEntry* Entry = FindEntryByTag(DamageInfo.HitReactionTag);
	if (!Entry || (Entry->LaunchForce == 0.0f && Entry->LaunchUpForce == 0.0f)) return FVector::ZeroVector;

	AActor* Owner = GetOwner();
	if (!Owner) return FVector::ZeroVector;

	FVector LaunchDir = FVector::BackwardVector;
	if (!DamageInfo.KnockbackDirectionOverride.IsNearlyZero())
	{
		LaunchDir = DamageInfo.KnockbackDirectionOverride.GetSafeNormal2D();
	}
	else if (DamageInfo.Instigator.IsValid())
	{
		LaunchDir = (Owner->GetActorLocation() - DamageInfo.Instigator->GetActorLocation()).GetSafeNormal2D();
	}
	return LaunchDir * Entry->LaunchForce + FVector(0.0f, 0.0f, Entry->LaunchUpForce);
}

void UHitReactionComponent::OnDamageReceived(const FDamageInfo& DamageInfo)
{
	#if !UE_BUILD_SHIPPING
	if (UTideGameSettings::Get()->bDebugDisableHitReaction) return;
	#endif

	// 死亡している場合はリアクション(モンタージュ・吹き飛び)は行わないが、致死ヒットの
	// インパクトVFXだけは通常ヒットと同様に出す (トドメの一撃で被弾表現が消えないように)
	if (StatusComponent && StatusComponent->IsDead())
	{
		if (const AActor* Owner = GetOwner())
		{
			const FHitReactionEntry* Entry = FindEntryByTag(DamageInfo.HitReactionTag);
			if (Entry && Entry->HitEffect)
			{
				UNiagaraFunctionLibrary::SpawnSystemAtLocation(
					Owner->GetWorld(), Entry->HitEffect,
					DamageInfo.HitResult.ImpactPoint,
					DamageInfo.HitResult.ImpactNormal.Rotation());
			}
		}
		return;
	}

	// SuperArmor中はモンタージュをスキップし、HitEffect VFXのみ再生
	if (const ATideCharacter* Owner = Cast<ATideCharacter>(GetOwner()))
	{
		if (Owner->HasStateTag(TAG_State_Common_SuperArmor))
		{
			const FHitReactionEntry* Entry = FindEntryByTag(DamageInfo.HitReactionTag);
			if (Entry && Entry->HitEffect)
			{
				UNiagaraFunctionLibrary::SpawnSystemAtLocation(
					Owner->GetWorld(), Entry->HitEffect,
					DamageInfo.HitResult.ImpactPoint,
					DamageInfo.HitResult.ImpactNormal.Rotation());
			}
			return;
		}
	}

	// 多段シーケンスはテーブル行から解決する
	const FBlowbackAnimSequence* TableSeq = FindTableSequence(DamageInfo.HitReactionTag);

	if (DamageInfo.HitReactionTag.MatchesTag(FGameplayTag::RequestGameplayTag("HitReaction.Blowoff")))
	{
		if (TableSeq && StatusComponent && !StatusComponent->IsDead())
		{
			BlowbackSeq = TableSeq;
			StartBlowbackSequence(DamageInfo);
		}
		return;
	}

	if (DamageInfo.HitReactionTag == TAG_HitReaction_SmashDown)
	{
		if (TableSeq && StatusComponent && !StatusComponent->IsDead())
		{
			BlowbackSeq = TableSeq;
			StartBlowbackSequence(DamageInfo);
		}
		return;
	}

	// 打ち上げ: Blowoffと同じst-lp-ed着地シーケンスで駆動する
	// (配下タグも拾えるよう階層マッチ)
	if (DamageInfo.HitReactionTag.MatchesTag(TAG_HitReaction_Blowup))
	{
		if (TableSeq && StatusComponent && !StatusComponent->IsDead())
		{
			BlowbackSeq = TableSeq;
			StartBlowbackSequence(DamageInfo);
		}
		return;
	}

	const FHitReactionEntry* Entry = FindEntryByTag(DamageInfo.HitReactionTag);
	if (!Entry) return;

	PlayEntry(*Entry, DamageInfo);
}

bool UHitReactionComponent::WillReact(FGameplayTag Tag) const
{
	#if !UE_BUILD_SHIPPING
	if (UTideGameSettings::Get()->bDebugDisableHitReaction) return false;
	#endif

	if (StatusComponent && StatusComponent->IsDead()) return false;

	// SuperArmor中はVFXのみでmontageを再生しない
	if (const ATideCharacter* Owner = Cast<ATideCharacter>(GetOwner()))
	{
		if (Owner->HasStateTag(TAG_State_Common_SuperArmor)) return false;
	}

	// Blowoff / SmashDownは専用シーケンスへ分岐する。シーケンスが設定済みなら
	// 再開はOnBlowbackEndedが担うので「リアクションする」とみなす
	// (OnDamageReceivedと同じ解決経路で判定を揃える)
	const FBlowbackAnimSequence* TableSeq = FindTableSequence(Tag);

	if (Tag.MatchesTag(FGameplayTag::RequestGameplayTag("HitReaction.Blowoff")))
	{
		return TableSeq != nullptr;
	}
	if (Tag == TAG_HitReaction_SmashDown)
	{
		return TableSeq != nullptr;
	}
	if (Tag.MatchesTag(TAG_HitReaction_Blowup))
	{
		return TableSeq != nullptr;
	}

	// 通常Knockback: Entryがあり、かつ再生可能なmontageを持つこと
	// montageが無いとOnReactionMontageEndedが来ずAIが再開しないため、
	// ここで弾く
	const FHitReactionEntry* Entry = FindEntryByTag(Tag);
	return Entry && Entry->Montage != nullptr;
}

const FHitReactionEntry* UHitReactionComponent::FindEntryByTag(FGameplayTag Tag) const
{
	// 敵はテーブル (HitReactionTable) 由来、
	// プレイヤーは基底DAのHitReactionSettings由来
	// HitReactionTableはUEnemyDataAssetにあるためプレイヤーは常に後者を使う
	// (移行の名残ではない)
	const TArray<FHitReactionEntry>& Entries = TableSingleSettings.Reactions.IsEmpty()
		? HitReactionSettings.Reactions
		: TableSingleSettings.Reactions;

	// 1. 完全一致を優先する
	for (const FHitReactionEntry& Entry : Entries)
	{
		if (Entry.ReactionTag == Tag) return &Entry;
	}

	// 2. 完全一致が無ければ、Tagの親に当たるエントリのうち最も具体的なものへフォールバックする
	// ルーティング(OnDamageReceived)は階層マッチなのに、力ルックアップが完全一致だと
	// 子タグ(例: HaloBackBreakReactionTag =
	// HitReaction.Blowoff.Back)で取りこぼすため照合基準を揃える
	const FHitReactionEntry* Best = nullptr;
	int32 BestDepth = -1;
	for (const FHitReactionEntry& Entry : Entries)
	{
		if (!Entry.ReactionTag.IsValid()) continue;
		// Entry.ReactionTagがTag自身またはTagの親なら候補
		if (Tag.MatchesTag(Entry.ReactionTag))
		{
			const int32 Depth = Entry.ReactionTag.GetGameplayTagParents().Num();
			if (Depth > BestDepth)
			{
				BestDepth = Depth;
				Best = &Entry;
			}
		}
	}
	return Best;
}

void UHitReactionComponent::PlayEntry(const FHitReactionEntry& Entry, const FDamageInfo& DamageInfo)
{
	AActor* Owner = GetOwner();
	if (!Owner) return;

	if (Entry.bFaceAttacker && DamageInfo.Instigator.IsValid())
	{
		FVector Dir = DamageInfo.Instigator->GetActorLocation() - Owner->GetActorLocation();
		Dir.Z = 0.0f;
		if (!Dir.IsNearlyZero())
		{
			Owner->SetActorRotation(Dir.GetSafeNormal().Rotation());
		}
	}

	if (Entry.HitEffect)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			Owner->GetWorld(), Entry.HitEffect,
			DamageInfo.HitResult.ImpactPoint,
			DamageInfo.HitResult.ImpactNormal.Rotation());
	}

	if (Entry.Montage && AnimInstance)
	{
		AnimInstance->Montage_Play(Entry.Montage);
	}

	if (Entry.LaunchForce > 0.0f)
	{
		if (ACharacter* Character = Cast<ACharacter>(Owner))
		{
			FVector LaunchDir = FVector::BackwardVector;
			if (!DamageInfo.KnockbackDirectionOverride.IsNearlyZero())
			{
				LaunchDir = DamageInfo.KnockbackDirectionOverride.GetSafeNormal2D();
			}
			else if (DamageInfo.Instigator.IsValid())
			{
				LaunchDir = (Owner->GetActorLocation() - DamageInfo.Instigator->GetActorLocation()).GetSafeNormal2D();
			}
			Character->LaunchCharacter(
				LaunchDir * Entry.LaunchForce + FVector(0.0f, 0.0f, Entry.LaunchUpForce), true, true);
		}
	}
}

// ---------------------------------------------------------------------------
// Blowoff系 ステートマシン
// ---------------------------------------------------------------------------

void UHitReactionComponent::SetBlowbackPhase(EBlowbackPhase NewPhase)
{
	BlowbackPhase = NewPhase;
	OnBlowbackPhaseChanged.Broadcast(NewPhase);
}

void UHitReactionComponent::EndBlowback()
{
	// 着地フェーズを経ずに終了した場合の保険(Landmontage無しで即Recovery/Endなど)
	RestoreAirborneRootMotion();

	// 着地せずに終了した場合、OnBlowbackLanded側の解除が走らないためここでも外す
	if (ACharacter* Character = Cast<ACharacter>(GetOwner()))
	{
		Character->LandedDelegate.RemoveDynamic(this, &UHitReactionComponent::OnBlowbackLanded);
	}

	BlowbackPhase = EBlowbackPhase::None;
	BlowbackVelocityCache = FVector::ZeroVector;
	CachedBlowbackInstigator.Reset();
	OnBlowbackEnded.Broadcast();
}

void UHitReactionComponent::RestoreAirborneRootMotion()
{
	if (!bAirborneRMSuppressed) return;
	if (ACharacter* Character = Cast<ACharacter>(GetOwner()))
	{
		Character->SetAnimRootMotionTranslationScale(PreAirborneRMScale);
	}
	bAirborneRMSuppressed = false;
}

void UHitReactionComponent::StartBlowbackSequence(const FDamageInfo& DamageInfo)
{
	if (!BlowbackSeq) return;
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (!Character || !AnimInstance) return;

	bBlowbackLanded = false;
	bWallHitOccurred = false;
	LastWallHitRecoveryIndex = -1;
	CachedBlowbackInstigator = DamageInfo.Instigator;
	SetBlowbackPhase(EBlowbackPhase::Start);

	// 空中フェーズは物理 (LaunchCharacter+重力) に任せる
	// モンタージュに垂直移動のrootmotionが入っていると打ち上げ
	// /落下と競合し着地検出が発火しないため、ここでRM移動を抑制する(着地で復帰)
	bAirborneRMSuppressed = false;
	if (BlowbackSeq->bSuppressAirborneRootMotion)
	{
		PreAirborneRMScale = Character->GetAnimRootMotionTranslationScale();
		Character->SetAnimRootMotionTranslationScale(0.0f);
		bAirborneRMSuppressed = true;
	}

	// 着地前の再被弾で二重束縛にならないようUniqueで張る(解除は着地時とEndBlowback /
	// EndPlay)
	Character->LandedDelegate.AddUniqueDynamic(this, &UHitReactionComponent::OnBlowbackLanded);

	if (BlowbackSeq->Start)
	{
		AnimInstance->Montage_Play(BlowbackSeq->Start);
	}
	else
	{
		SetBlowbackPhase(EBlowbackPhase::Loop);
		if (BlowbackSeq->Loop) AnimInstance->Montage_Play(BlowbackSeq->Loop);
	}

	// HitReactionTagに対応するReactionsエントリから発射力を取得して適用
	// (Blowoff_S/M/LごとにLaunchForce
	// /LaunchUpForceをReactionsに設定する)
	const FHitReactionEntry* LaunchEntry = FindEntryByTag(DamageInfo.HitReactionTag);
	const float BlowbackMult = UTideGameSettings::Get()->bDebugEnemyBlowbackX3 ? 3.0f : 1.0f;
	const float Force   = (LaunchEntry ? LaunchEntry->LaunchForce   : 0.0f) * BlowbackMult;
	const float UpForce = (LaunchEntry ? LaunchEntry->LaunchUpForce : 0.0f) * BlowbackMult;
	if (Force != 0.0f || UpForce != 0.0f)
	{
		FVector LaunchDir = FVector::BackwardVector;
		if (!DamageInfo.KnockbackDirectionOverride.IsNearlyZero())
		{
			LaunchDir = DamageInfo.KnockbackDirectionOverride.GetSafeNormal2D();
		}
		else if (DamageInfo.Instigator.IsValid())
		{
			LaunchDir = (Character->GetActorLocation() - DamageInfo.Instigator->GetActorLocation()).GetSafeNormal2D();
		}

		// 攻撃者を向いてから発射する。吹き飛びアニメは「正面被弾→後方へ飛ぶ」想定のため、
		// 背後から飛ばされても攻撃者へ向き直すと、移動方向
		// (=攻撃者から離れる=後方)とアニメが一致する
		// LaunchDirはワールド空間なので回転しても発射方向は変わらない
		if (Force != 0.0f && DamageInfo.Instigator.IsValid())
		{
			FVector FaceDir = DamageInfo.Instigator->GetActorLocation() - Character->GetActorLocation();
			FaceDir.Z = 0.0f;
			if (!FaceDir.IsNearlyZero())
			{
				Character->SetActorRotation(FaceDir.GetSafeNormal().Rotation());
			}
		}

		Character->LaunchCharacter(
			LaunchDir * Force + FVector(0.0f, 0.0f, UpForce),
			true, true);
	}
}

void UHitReactionComponent::StartWallHitSequence()
{
	if (!BlowbackSeq || !AnimInstance) return;

	bWallHitOccurred = true;
	SetBlowbackPhase(EBlowbackPhase::WallHitStart);

	UAnimMontage* StartMontage = BlowbackSeq->WallHitStart ? BlowbackSeq->WallHitStart.Get() : BlowbackSeq->Start.Get();
	if (StartMontage)
	{
		AnimInstance->Montage_Play(StartMontage);
	}
	else
	{
		SetBlowbackPhase(EBlowbackPhase::WallHitLoop);
		UAnimMontage* LoopMontage = BlowbackSeq->WallHitLoop ? BlowbackSeq->WallHitLoop.Get() : BlowbackSeq->Loop.Get();
		if (LoopMontage) AnimInstance->Montage_Play(LoopMontage);
	}
}

void UHitReactionComponent::AdvanceBlowbackPhase()
{
	if (!BlowbackSeq || !AnimInstance) return;
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (!Character) return;

	switch (BlowbackPhase)
	{
	case EBlowbackPhase::Start:
	case EBlowbackPhase::Loop:
		if (bBlowbackLanded)
		{
			// 着地したので空中フェーズのRM抑制を解除する(Land
			// /Recoveryのrootmotionを活かす)
			RestoreAirborneRootMotion();
			SetBlowbackPhase(EBlowbackPhase::Land);
			if (BlowbackSeq->Land)
			{
				AnimInstance->Montage_Play(BlowbackSeq->Land);
			}
			else
			{
				SetBlowbackPhase(EBlowbackPhase::Recovery);
				if (UAnimMontage* Recovery = PickRecoveryMontage(BlowbackSeq->Recoveries, LastRecoveryIndex, Character, BlowbackSeq->RecoveryNavCheckDistance))
					AnimInstance->Montage_Play(Recovery);
				else
					EndBlowback();
			}
		}
		else
		{
			SetBlowbackPhase(EBlowbackPhase::Loop);
			if (BlowbackSeq->Loop) AnimInstance->Montage_Play(BlowbackSeq->Loop);
		}
		break;

	case EBlowbackPhase::WallHitStart:
	case EBlowbackPhase::WallHitLoop:
		if (bBlowbackLanded)
		{
			// 着地したので空中フェーズのRM抑制を解除する(Land
			// /Recoveryのrootmotionを活かす)
			RestoreAirborneRootMotion();
			SetBlowbackPhase(EBlowbackPhase::Land);
			UAnimMontage* LandMontage = BlowbackSeq->WallHitLand ? BlowbackSeq->WallHitLand.Get() : BlowbackSeq->Land.Get();
			if (LandMontage)
			{
				AnimInstance->Montage_Play(LandMontage);
			}
			else
			{
				SetBlowbackPhase(EBlowbackPhase::Recovery);
				const bool bUseWall = !BlowbackSeq->WallHitRecoveries.IsEmpty();
				if (UAnimMontage* Recovery = PickRecoveryMontage(
						bUseWall ? BlowbackSeq->WallHitRecoveries : BlowbackSeq->Recoveries,
						bUseWall ? LastWallHitRecoveryIndex : LastRecoveryIndex,
						Character, BlowbackSeq->RecoveryNavCheckDistance))
					AnimInstance->Montage_Play(Recovery);
				else
					EndBlowback();
			}
		}
		else
		{
			SetBlowbackPhase(EBlowbackPhase::WallHitLoop);
			UAnimMontage* LoopMontage = BlowbackSeq->WallHitLoop ? BlowbackSeq->WallHitLoop.Get() : BlowbackSeq->Loop.Get();
			if (LoopMontage) AnimInstance->Montage_Play(LoopMontage);
		}
		break;

	case EBlowbackPhase::Land:
	{
		SetBlowbackPhase(EBlowbackPhase::Recovery);
		const bool bUseWall = bWallHitOccurred && !BlowbackSeq->WallHitRecoveries.IsEmpty();
		if (UAnimMontage* Recovery = PickRecoveryMontage(
				bUseWall ? BlowbackSeq->WallHitRecoveries : BlowbackSeq->Recoveries,
				bUseWall ? LastWallHitRecoveryIndex : LastRecoveryIndex,
				Character, BlowbackSeq->RecoveryNavCheckDistance))
		{
			AnimInstance->Montage_Play(Recovery);
		}
		else
		{
			EndBlowback();
		}
		break;
	}

	case EBlowbackPhase::Recovery:
		EndBlowback();
		break;

	default:
		break;
	}
}

void UHitReactionComponent::OnBlowbackMontageBlendingOut(UAnimMontage* Montage, bool bInterrupted)
{
	// 多段リアクションは吹き飛びと巻き上げの2系統。同じデリゲートを共有するのでここで振り分ける
	if (BlowbackPhase != EBlowbackPhase::None)
	{
		// 切り替え/被弾などによる中断は無視 (自然なブレンドアウトのときだけ次へ繋ぐ)
		if (bInterrupted) return;
		AdvanceBlowbackPhase();
		return;
	}

	if (WindPhase != EWindReactionPhase::None)
	{
		if (StatusComponent && StatusComponent->IsDead()) return;

		if (bInterrupted)
		{
			// 終端 (Landing/Recovery) が被弾等の別モンタージュに中断されると、以降
			// AdvanceWindPhaseが二度と来ずWindPhaseが取り残されてReact(Wind)へ居座る (棒立ち)。
			// ここで巻き上げを畳んでおき、reactingの寿命は割り込んだリアクション側へ引き継がせる
			// (AbortはOnWindReactionEndedを撃たないので、被弾側が立てたSetReactingを潰さない)。
			// 空中フェーズの中断はアニメが飛ぶだけなので従来どおり無視する
			if (WindPhase == EWindReactionPhase::Landing || WindPhase == EWindReactionPhase::Recovery)
			{
				AbortWindReaction();
			}
			return;
		}

		AdvanceWindPhase(Montage);
	}
}

void UHitReactionComponent::OnBlowbackLanded(const FHitResult& Hit)
{
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (Character)
	{
		Character->LandedDelegate.RemoveDynamic(this, &UHitReactionComponent::OnBlowbackLanded);
	}

	// 着地デリゲートも吹き飛びと巻き上げで共有する。吹き飛びを優先し、
	// 吹き飛び中でなければ巻き上げの着地として扱う
	if (BlowbackPhase != EBlowbackPhase::None)
	{
		bBlowbackLanded = true;
		if (BlowbackPhase == EBlowbackPhase::Loop || BlowbackPhase == EBlowbackPhase::WallHitLoop)
		{
			AdvanceBlowbackPhase();
		}
		return;
	}

	if (WindPhase != EWindReactionPhase::None)
	{
		OnWindLanded();
	}
}

void UHitReactionComponent::BeginRagdollDamage(USphereComponent* Collider, float Damage,
	FGameplayTag HitReactionTag, AActor* Instigator)
{
	if (!Collider || Damage <= 0.0f) return;

	RagdollCollisionDamage         = Damage;
	RagdollCollisionHitReactionTag = HitReactionTag;
	CachedRagdollInstigator        = Instigator;
	RagdollDamageCollider          = Collider;

	Collider->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Collider->SetCollisionResponseToAllChannels(ECR_Ignore);
	Collider->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
	Collider->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Collider->SetGenerateOverlapEvents(true);
	Collider->OnComponentBeginOverlap.AddUniqueDynamic(this, &UHitReactionComponent::OnRagdollSphereOverlap);
}

void UHitReactionComponent::OnRagdollSphereOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!OtherActor || OtherActor == GetOwner()) return;
	if (APawn* OtherPawn = Cast<APawn>(OtherActor) ; OtherPawn && OtherPawn->IsPlayerControlled()) return;

	IDamageable* Damageable = Cast<IDamageable>(OtherActor);
	if (!Damageable || !Damageable->CanBeDamaged()) return;

	// bFromSweep=falseの静的オーバーラップではSweepResultが空になるため、
	// 衝突点・法線をスフィア中心→相手アクター方向から手動で構築する
	FHitResult BuiltHit = SweepResult;
	if (!bFromSweep || BuiltHit.ImpactPoint.IsNearlyZero())
	{
		const FVector OtherPos = OtherComp ? OtherComp->GetComponentLocation() : OtherActor->GetActorLocation();
		const FVector SpherePos = OverlappedComp->GetComponentLocation();
		BuiltHit.ImpactPoint  = OtherPos;
		BuiltHit.Location     = OtherPos;
		BuiltHit.ImpactNormal = (OtherPos - SpherePos).GetSafeNormal();
		BuiltHit.Normal       = BuiltHit.ImpactNormal;
	}

	FDamageInfo Info;
	Info.BaseDamage     = RagdollCollisionDamage;
	Info.Instigator     = CachedRagdollInstigator;
	Info.HitReactionTag = RagdollCollisionHitReactionTag;
	Info.HitResult      = BuiltHit;
	Damageable->ReceiveDamage(Info);

	// 1回ダメージを与えたらコライダーを無効化
	if (USphereComponent* Col = RagdollDamageCollider.Get())
	{
		Col->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Col->SetGenerateOverlapEvents(false);
	}
}

void UHitReactionComponent::OnBlowbackWallHit(UPrimitiveComponent* HitComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	if (BlowbackPhase != EBlowbackPhase::Start && BlowbackPhase != EBlowbackPhase::Loop) return;
	if (bBlowbackLanded) return;

	// 敵・壊れ物への衝突ダメージ
	const float CollisionDmg = BlowbackSeq ? BlowbackSeq->CollisionDamage : 0.0f;
#if !UE_BUILD_SHIPPING
	const bool bCollisionDamageEnabled = UTideGameSettings::Get()->bDebugEnableBlowbackCollisionDamage;
#else
	constexpr bool bCollisionDamageEnabled = true;
#endif
	if (bCollisionDamageEnabled && OtherActor && OtherActor != GetOwner() && CollisionDmg > 0.0f)
	{
		if (IDamageable* Damageable = Cast<IDamageable>(OtherActor))
		{
			if (Damageable->CanBeDamaged())
			{
				FDamageInfo Info;
				Info.BaseDamage     = CollisionDmg;
				Info.Instigator     = CachedBlowbackInstigator;
				Info.HitResult      = Hit;
				Info.HitReactionTag = BlowbackSeq->CollisionHitReactionTag;
				Damageable->ReceiveDamage(Info);
			}
		}
	}

	if (Hit.ImpactNormal.Z > 0.7f) return;

	ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (!Character) return;

	if (UCharacterMovementComponent* Move = Character->GetCharacterMovement(); Move && BlowbackSeq)
	{
		Move->Velocity = BlowbackVelocityCache.MirrorByVector(Hit.ImpactNormal) * BlowbackSeq->WallHitBounceRestitution;
	}

	StartWallHitSequence();
}
