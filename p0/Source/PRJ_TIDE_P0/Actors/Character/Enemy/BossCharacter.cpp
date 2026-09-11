// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#include "PRJ_TIDE_P0/Actors/Character/Enemy/BossCharacter.h"
#include "PRJ_TIDE_P0/Components/AI/BossPhaseComponent.h"
#include "PRJ_TIDE_P0/Components/AI/EnemyBattleComponent.h"
#include "PRJ_TIDE_P0/Components/Combat/DamageSystemComponent.h"
#include "PRJ_TIDE_P0/Components/Combat/PartDestructionComponent.h"
#include "PRJ_TIDE_P0/Components/Combat/HaloSystemComponent.h"
#include "PRJ_TIDE_P0/Components/Targeting/LockOnTargetComponent.h"
#include "PRJ_TIDE_P0/Animation/Instances/Character/Enemy/EnemyAnimInstance.h"
#include "PRJ_TIDE_P0/Data/Enemy/BossDataAsset.h"
#include "PRJ_TIDE_P0/Data/Tags/TideGameplayTags.h"
#include "PRJ_TIDE_P0/Actors/Character/Player/TidePlayerCharacter.h"

#include "Kismet/GameplayStatics.h"

#include "AIController.h"
#include "BrainComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "DrawDebugHelpers.h"
#include "NiagaraFunctionLibrary.h"
#include "imgui.h"

ABossCharacter::ABossCharacter()
{
	BossPhaseComponent       = CreateDefaultSubobject<UBossPhaseComponent>(TEXT("BossPhaseComponent"));
	PartDestructionComponent = CreateDefaultSubobject<UPartDestructionComponent>(TEXT("PartDestructionComponent"));

	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
}

void ABossCharacter::BeginPlay()
{
	Super::BeginPlay();

	// 常時SAは付与しない。ボスはHitReaction未設定で怯みが無く常時SAの意味が薄いうえ、
	// 常時SAだと片足やられのSAゲートが常に効いてしまう
	// SAを効かせたい攻撃はモンタージュにAnimNotifyState_SetStateTag
	// (State.Common.SuperArmor) を置いて区間付与する

	const UBossDataAsset* BossData = Cast<UBossDataAsset>(GetCharacterData());
	if (!BossData) return;

	PartDestructionComponent->Initialize(BossData->Parts, BossData->PartComboReactions, BossData->RecoveryTouch);

	if (DamageSystem)
	{
		DamageSystem->OnDamageReceived.AddDynamic(this, &ABossCharacter::OnDamageReceivedForStep);
	}

	if (UEnemyAnimInstance* AnimInst = Cast<UEnemyAnimInstance>(GetMesh()->GetAnimInstance()))
	{
		AnimInst->InitHitShake(BossData->Parts.Num());
	}

	// BossPhaseはPartDestructionのOnPartDestroyedをBindするため
	// PartDestruction初期化より後に呼ぶ必要がある
	BossPhaseComponent->Initialize(BossData);
	BossPhaseComponent->OnPhaseChanged.AddUObject(this, &ABossCharacter::OnPhaseChanged);

	// BattleComponentのAttackEventをHandleAttackEventへ配線
	// ※virtual dispatchで派生クラスへ届く
	GetBattleComponent()->OnAttackEvent.AddUObject(this, &ABossCharacter::HandleAttackEvent);

	PartDestructionComponent->OnPartDestroyed.AddUObject(this, &ABossCharacter::OnPartDestroyedResponse);
	PartDestructionComponent->OnPartComboDestroyed.AddUObject(this, &ABossCharacter::OnPartComboDestroyedResponse);

	// 部位光輪を共通光輪システムへユニット登録し、破壊/復活で状態をミラーする
	// 肉質(無防備＝本体ヒットで多くダメージ)が状態に応じて効く
	if (HaloSystemComponent)
	{
		TArray<FHaloUnitConfig> Units;
		for (const FPartEntry& Part : BossData->Parts)
		{
			if (!Part.HaloConfig.HaloMesh) continue;	// 光輪なし部位は対象外

			FHaloUnitConfig Unit;
			Unit.PartTag = Part.PartTag;
			Unit.HaloHitDamageMultiplier = Part.HaloConfig.HaloHitDamageMultiplier;
			Unit.BodyHitDamageMultiplier = Part.HaloConfig.BodyHitDamageMultiplier;
			Unit.BreakRegenCooldown = Part.HaloConfig.BreakRegenCooldown;
			Units.Add(Unit);
		}
		HaloSystemComponent->Initialize(Units);

		PartDestructionComponent->OnPartDestroyed.AddWeakLambda(this, [this](FName PartTag)
		{
			HaloSystemComponent->SetUnitState(PartTag, EHaloUnitState::Broken);
		});
		PartDestructionComponent->OnPartRevived.AddWeakLambda(this, [this](FName PartTag)
		{
			HaloSystemComponent->SetUnitState(PartTag, EHaloUnitState::Deployed);
		});
	}

	if (UAnimInstance* AnimInst = GetMesh()->GetAnimInstance())
	{
		AnimInst->OnMontageEnded.AddDynamic(this, &ABossCharacter::OnPartReactionMontageEnded);
	}

	// FPartEntry::bRequiresDownOrGodLockが立つ部位のPartTagを集め
	// る
	TSet<FName> ConditionalPartTags;
	for (const FPartEntry& Part : BossData->Parts)
	{
		if (Part.bRequiresDownOrGodLock)
		{
			ConditionalPartTags.Add(Part.PartTag);
		}
	}

	// 条件付き部位配下のLockOnTargetを収集し、初期状態は対象外にする
	// 部位の特定は、親 (PartTagタグ付きコリジョン) のタグから逆引きする
	// 以降はTickのUpdateConditionalPartTargetableがダウン/神技ロックオ
	// ン中だけtrueに駆動する
	if (ConditionalPartTags.Num() > 0)
	{
		TArray<ULockOnTargetComponent*> TargetComps;
		GetComponents<ULockOnTargetComponent>(TargetComps);
		for (ULockOnTargetComponent* Comp : TargetComps)
		{
			if (!Comp) continue;

			bool bConditional = false;
			for (USceneComponent* Parent = Comp->GetAttachParent(); Parent && !bConditional; Parent = Parent->GetAttachParent())
			{
				for (const FName& PartTag : ConditionalPartTags)
				{
					if (Parent->ComponentHasTag(PartTag))
					{
						bConditional = true;
						break;
					}
				}
			}

			if (bConditional)
			{
				Comp->bIsTargetable = false;
				ConditionalLockOnTargets.Add(Comp);
			}
		}
	}
}

void ABossCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// 条件付き部位のロックオン可否を毎フレーム更新する
	UpdateConditionalPartTargetable();

	// 足場ジャンプ移動: 放物線で着地先へ移動 ※旋回より先に更新して同フレームの位置を確定
	if (PlatformJumpPhase != EPlatformJumpPhase::None)
	{
		TickPlatformJump(DeltaSeconds);
	}

	// 旋回ロジック: モンタージュが終了するか目標角度に達したら停止
	if (bIsTurning)
	{
		const UAnimInstance* AnimInst = GetMesh()->GetAnimInstance();
		if (!AnimInst || !AnimInst->Montage_IsPlaying(ActiveTurnMontage))
		{
			bIsTurning        = false;
			ActiveTurnMontage = nullptr;
		}
		else
		{
			const float CurYaw = GetActorRotation().Yaw;
			const float NewYaw = FMath::FixedTurn(CurYaw, TurnTargetYaw, TurnRotationRate * DeltaSeconds);
			SetActorRotation(FRotator(0.0f, NewYaw, 0.0f));
			if (FMath::Abs(FRotator::NormalizeAxis(NewYaw - TurnTargetYaw)) < 1.0f)
			{
				bIsTurning        = false;
				ActiveTurnMontage = nullptr;
			}
		}
	}
}

void ABossCharacter::DrawImGuiInspector()
{
	Super::DrawImGuiInspector();

	if (ImGui::CollapsingHeader("Platform Jump"))
	{
		const TCHAR* PhaseStr =
			PlatformJumpPhase == EPlatformJumpPhase::WindUp   ? TEXT("WindUp (踏ん張り/離陸待ち)") :
			PlatformJumpPhase == EPlatformJumpPhase::Airborne ? TEXT("Airborne (滞空)") :
			                                                    TEXT("None");
		ImGui::Text("Phase:");
		ImGui::SameLine(140.0f);
		ImGui::Text("%s", TCHAR_TO_UTF8(PhaseStr));

		ImGui::Text("Queue:");
		ImGui::SameLine(140.0f);
		ImGui::Text("%d", PendingJumpTargets.Num());

		if (PlatformJumpPhase == EPlatformJumpPhase::Airborne)
		{
			const UBossDataAsset* BossData = Cast<UBossDataAsset>(GetCharacterData());
			const float Duration = BossData ? FMath::Max(BossData->PlatformJump.JumpDuration, 0.05f) : 0.8f;
			const float Alpha = FMath::Clamp(PlatformJumpElapsed / Duration, 0.0f, 1.0f);
			ImGui::Text("Arc progress:");
			ImGui::SameLine(140.0f);
			ImGui::ProgressBar(Alpha, ImVec2(160.0f, 0.0f));
		}

		const UBossDataAsset* BossData = Cast<UBossDataAsset>(GetCharacterData());
		const bool bHasMontage = BossData && BossData->PlatformJump.JumpMontage != nullptr;
		if (!bHasMontage)
		{
			ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.2f, 1.0f),
				"JumpMontage 未設定 (DataAsset > PlatformJump)");
		}
	}
}

void ABossCharacter::HandleAttackEvent(FGameplayTag Tag)
{
	// 足場ジャンプの離陸合図: 踏ん張りモンタージュのNotifyから届く
	if (PlatformJumpPhase == EPlatformJumpPhase::WindUp)
	{
		const UBossDataAsset* BossData = Cast<UBossDataAsset>(GetCharacterData());
		const FGameplayTag LaunchTag = BossData ? BossData->PlatformJump.LaunchEventTag : FGameplayTag();
		if (LaunchTag.IsValid() && Tag == LaunchTag)
		{
			LaunchPlatformJump();
			return;
		}
	}

	BossPhaseComponent->HandleAttackEvent(Tag);
}

void ABossCharacter::SetExecutingAttack(bool bInExecuting)
{
	Super::SetExecutingAttack(bInExecuting);

	// 攻撃中に光輪が生え直すと、技が開始時に確定させた部位状態
	// (UStompAttackExecutionの足選択など) と食い違って混乱のもとになる
	// UEnemyAIState_Attackが開始/終了・中断で必ず対称に呼ぶので、
	// 攻撃の種類によらず全攻撃が対象になる。攻撃終了時の解除はExecutionのOnAttackEndよ
	// り後に走るため、保留していた復活ディザは光輪の再表示フェードと同じタイミングで始まる
	PartDestructionComponent->SetRegenBlocked(EPartRegenBlockReason::Attack, bInExecuting);
}

void ABossCharacter::OnPartDestroyedResponse(FName PartTag)
{
	const UBossDataAsset* BossData = Cast<UBossDataAsset>(GetCharacterData());
	if (!BossData) return;

	const FPartEntry* Part = BossData->Parts.FindByPredicate(
		[&](const FPartEntry& P) { return P.PartTag == PartTag; });
	if (!Part) return;

	// 部位コンポーネントの位置にVFXをスポーン
	if (Part->DestroyVFX)
	{
		FVector VFXLocation = GetActorLocation();
		TArray<UPrimitiveComponent*> PrimComps;
		GetComponents<UPrimitiveComponent>(PrimComps);
		for (UPrimitiveComponent* Prim : PrimComps)
		{
			if (Prim->ComponentHasTag(PartTag))
			{
				VFXLocation = Prim->GetComponentLocation();
				break;
			}
		}
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, Part->DestroyVFX, VFXLocation);
	}

	// 連動グループが成立した部位は個別モンタージュを再生せず、ComboMontageに委ねる
	// (再生はOnPartComboDestroyedResponseが担当する)
	const int32 CompletedComboIndex = PartDestructionComponent->FindCompletedComboForPart(PartTag);
	const bool bComboTriggered = CompletedComboIndex != INDEX_NONE;
	// 攻撃実行中の片足やられ (bSkipWhenExecutingAttack)
	// は攻撃を上書きしないよう丸ごとスキップする
	const bool bSkipCombo  = bComboTriggered && ShouldSkipComboReaction(CompletedComboIndex);
	const bool bPlayDestroy = !bComboTriggered && Part->DestroyMontage != nullptr;

	// StopLogicは攻撃タスクを同期的に中断しOnAttackEnd(殴れる窓の再硬化)を走らせる
	// その判定が「リアクション再生中」になるよう、StopLogicより前にリアクションを確定させておく
	// 連動成立時は個別通知 (この関数) が連動通知より先に走るため、
	// ここでComboMontageを先取りする
	// (実際の再生は直後のOnPartComboDestroyedResponseが担当する)
	if (bPlayDestroy)
	{
		ActivePartReactionMontage = Part->DestroyMontage;
	}
	else if (bComboTriggered && !bSkipCombo && BossData->PartComboReactions.IsValidIndex(CompletedComboIndex))
	{
		ActivePartReactionMontage = BossData->PartComboReactions[CompletedComboIndex].ComboMontage;
	}

	// リアクションのモンタージュが実際に流れるときだけAIを止める
	// (モンタージュ無しで止めると終了コールバックが来ずreactingのまま固まる
	//  スキップされた片足やられや、モンタージュ未設定の単体破壊はここを通さない)
	if (ActivePartReactionMontage)
	{
		// ひざまずき/ダウン中に別部位の光輪が生え直すと反撃機会が潰れるので、復活を保留させる
		// (解除はOnPartReactionMontageEnded)
		PartDestructionComponent->SetRegenBlocked(EPartRegenBlockReason::Reaction, true);

		// AI停止 (復帰はOnPartReactionMontageEnded +
		// AEnemyCharacter::OnReactionMontageEndedが担う)
		if (AAIController* AIC = Cast<AAIController>(GetController()))
		{
			AIC->StopMovement();
			AIC->ClearFocus(EAIFocusPriority::Gameplay);

			// ステートマシン運用ではBrainComponentが無い。行動抑止はフラグ側が担う
			SetReacting(true, TEXT("PartDestroyed"));
		}
	}

	if (bPlayDestroy)
	{
		if (UAnimInstance* AnimInst = GetMesh()->GetAnimInstance())
		{
			AnimInst->Montage_Play(Part->DestroyMontage);
		}
	}
}

void ABossCharacter::OnPartComboDestroyedResponse(int32 ComboIndex)
{
	const UBossDataAsset* BossData = Cast<UBossDataAsset>(GetCharacterData());
	if (!BossData || !BossData->PartComboReactions.IsValidIndex(ComboIndex)) return;

	const FPartComboReaction& Combo = BossData->PartComboReactions[ComboIndex];

	// SA中の片足やられ (bSkipWhenSuperArmor) は攻撃を上書きしないよう再生しない
	// 破壊状態・光輪演出・復活タイマーはPartDestructionComponent側で維持される
	if (ShouldSkipComboReaction(ComboIndex)) return;

	// StopLogicは攻撃タスクを同期的に中断しOnAttackEnd (殴れる窓の再硬化)
	// を走らせる。その判定が「リアクション再生中」になるよう、
	// StopLogicより前にリアクションを確定させておく
	if (Combo.ComboMontage)
	{
		ActivePartReactionMontage = Combo.ComboMontage;
		// 複数部位破壊によるダウン中。条件付き部位 (bRequiresDownOrGodLock)
		// のロックオンを解禁する。終了時にOnPartReactionMontageEndedで除去する
		AddStateTag(TAG_State_Enemy_PartComboDown);

		// ダウン中に光輪が生え直さないよう復活を保留させる
		// (解除はOnPartReactionMontageEnded)
		PartDestructionComponent->SetRegenBlocked(EPartRegenBlockReason::Reaction, true);

		// ダウン明けに強制発動する攻撃 (反撃バラージ) を予約する (-1なら発動しない)
		// 消費はOnPartReactionMontageEnded
		PendingForceAttackOnReactionEnd = Combo.ForceAttackIndexOnEnd;
	}

	// AI停止 (個別破壊と同様。復帰はOnReactionMontageEndedが担う)
	if (AAIController* AIC = Cast<AAIController>(GetController()))
	{
		AIC->StopMovement();
		AIC->ClearFocus(EAIFocusPriority::Gameplay);

		// ステートマシン運用ではBrainComponentが無い。行動抑止はフラグ側が担う
		SetReacting(true, TEXT("PartComboDestroyed"));
	}

	if (Combo.ComboMontage)
	{
		if (UAnimInstance* AnimInst = GetMesh()->GetAnimInstance())
		{
			AnimInst->Montage_Play(Combo.ComboMontage);
		}
	}
	// 復活は連動では行わず、各部位がBreakRegenCooldown秒後に個別復活する
	// (PartDestructionComponent)
}

bool ABossCharacter::ShouldSkipComboReaction(int32 ComboIndex) const
{
	const UBossDataAsset* BossData = Cast<UBossDataAsset>(GetCharacterData());
	if (!BossData || !BossData->PartComboReactions.IsValidIndex(ComboIndex)) return false;

	// SA区間 (攻撃モンタージュのNotifyで付与) 中だけ片足やられを抑止して攻撃を上書きさせない
	return BossData->PartComboReactions[ComboIndex].bSkipWhenSuperArmor
		&& HasStateTag(TAG_State_Common_SuperArmor);
}

void ABossCharacter::OnPartReactionMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	if (Montage != ActivePartReactionMontage) return;

	ActivePartReactionMontage = nullptr;

	// 複数部位破壊ダウンの終了 (個別破壊モンタージュ終了時は付いていないのでno-op)
	RemoveStateTag(TAG_State_Enemy_PartComboDown);

	// リアクション中に溜まっていた部位復活を解禁する (攻撃中ならその解除まで更に保留される)
	PartDestructionComponent->SetRegenBlocked(EPartRegenBlockReason::Reaction, false);

	// 殴れる窓の最中にリアクションへ入っていた場合、ここで遅延していた再硬化を実行する
	NotifyPartReactionEnded();

	// ダウン明けの強制攻撃 (反撃バラージ) を消費する
	// 中断終了では反撃させない (死亡等で不自然になるため)
	const int32 ForceIdx = PendingForceAttackOnReactionEnd;
	PendingForceAttackOnReactionEnd = -1;
	if (!bInterrupted && ForceIdx >= 0)
	{
		// ダウン明けは全光輪を即時復活させてから反撃を発動する。クールダウン復活と攻撃中の
		// 復活保留がバッティングして反撃中に光輪が出ない問題を避け、復活済みの全光輪を
		// 発光＋接触ダメージの対象にする
		PartDestructionComponent->ReviveAllPartsImmediate();
		RequestForceAttack(ForceIdx);
	}

	// RestartLogicはOnReactionMontageEndedが担う
}

void ABossCharacter::UpdateConditionalPartTargetable()
{
	if (ConditionalLockOnTargets.Num() == 0) return;

	// 死亡後はAEnemyCharacter側でbIsTargetableをfalse固定にするため、
	// ここでは触らない
	if (HasStateTag(TAG_State_Common_Dead)) return;

	if (!CachedPlayer.IsValid())
	{
		CachedPlayer = Cast<ATidePlayerCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0));
	}

	// 複数部位破壊ダウン中 もしくは プレイヤー神技ロックオン中 のみ解禁する
	const bool bDownReaction = HasStateTag(TAG_State_Enemy_PartComboDown);
	const bool bPlayerGodLock = CachedPlayer.IsValid() && CachedPlayer->IsGodActionLockingOn();
	const bool bTargetable = bDownReaction || bPlayerGodLock;

	for (ULockOnTargetComponent* Comp : ConditionalLockOnTargets)
	{
		if (Comp)
		{
			Comp->bIsTargetable = bTargetable;
		}
	}
}

void ABossCharacter::OnPhaseChanged(int32 OldPhase, int32 NewPhase)
{
	// フェーズ番号の反映先はUEnemyBattleComponent::SetCurrentPhase
	// (攻撃抽選のフィルタ)。UBossPhaseComponentが直接呼ぶのでここでは何もしない

	// 遷移モンタージュを再生
	const UBossDataAsset* BossData = Cast<UBossDataAsset>(GetCharacterData());
	if (!BossData || !BossData->Phases.IsValidIndex(NewPhase)) return;

	if (UAnimMontage* Montage = BossData->Phases[NewPhase].TransitionMontage)
	{
		if (UAnimInstance* Anim = GetMesh()->GetAnimInstance())
		{
			Anim->Montage_Play(Montage);
		}
	}
}

void ABossCharacter::BeginTurn(UAnimMontage* TurnMontage, float TargetYaw, float InRotationRate)
{
	ActiveTurnMontage = TurnMontage;
	TurnTargetYaw     = TargetYaw;
	TurnRotationRate  = InRotationRate;
	bIsTurning        = true;
}

void ABossCharacter::EnqueuePlatformJump(const FVector& TargetLocation)
{
	PendingJumpTargets.Add(TargetLocation);

	// ジャンプ中でなければ先頭を即開始する (ジャンプ中なら着地時に次が起動される)
	if (PlatformJumpPhase == EPlatformJumpPhase::None)
	{
		const FVector Next = PendingJumpTargets[0];
		PendingJumpTargets.RemoveAt(0);
		BeginPlatformJump(Next);
	}
}

void ABossCharacter::BeginPlatformJump(const FVector& Target)
{
	const UBossDataAsset* BossData = Cast<UBossDataAsset>(GetCharacterData());
	const FBossPlatformJumpSettings& Settings =
		BossData ? BossData->PlatformJump : FBossPlatformJumpSettings();

	// 着地点 (カプセル中心) を先に確定する。アンカーXY上空から下方向へカプセルスウィープして
	// 足場上面を採り、その上にカプセル底を載せる (LaunchCharacterは使わない方針)
	const float HalfHeight = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const float Radius     = GetCapsuleComponent()->GetScaledCapsuleRadius();

	FVector LandCenter = Target + FVector(0.0f, 0.0f, HalfHeight); // フォールバック: アンカーを足元とみなす

	const FVector TraceStart(Target.X, Target.Y, Target.Z + Settings.LandTraceUp);
	const FVector TraceEnd  (Target.X, Target.Y, Target.Z - Settings.LandTraceDown);

	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(BossPlatformLand), false, this);
	const FCollisionShape Capsule = FCollisionShape::MakeCapsule(Radius, HalfHeight);
	if (GetWorld()->SweepSingleByChannel(Hit, TraceStart, TraceEnd, FQuat::Identity,
		ECC_WorldStatic, Capsule, Params))
	{
		LandCenter = Hit.Location; // スウィープ停止位置 = カプセル中心が床に載る位置
	}

	PlatformJumpStart   = GetActorLocation();
	PlatformJumpTarget  = LandCenter;
	PlatformJumpElapsed = 0.0f;
	PlatformJumpWindUp  = 0.0f;

	// AIとステップ/攻撃の割り込みを抑止する
	AddStateTag(TAG_State_Enemy_PlatformJumping);
	if (AAIController* AIC = Cast<AAIController>(GetController()))
	{
		AIC->StopMovement();
	}

	// 踏ん張り中は接地したまま (移動だけ止める)
	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->StopMovementImmediately();
	}

	// 着地先へ向く (任意)。踏ん張りの段階で向きを合わせる
	if (Settings.bFaceTarget)
	{
		const FVector Flat = (PlatformJumpTarget - PlatformJumpStart).GetSafeNormal2D();
		if (!Flat.IsNearlyZero())
		{
			SetActorRotation(FRotator(0.0f, Flat.Rotation().Yaw, 0.0f));
		}
	}

	// 見た目用ジャンプモンタージュ (踏ん張り → 滞空 → 落下 → 着地)
	// 踏ん張り終わりのNotify(LaunchEventTag) で離陸する
	// モンタージュ/合図が無ければ即離陸
	const bool bHasLaunchCue = Settings.JumpMontage && Settings.LaunchEventTag.IsValid();
	if (Settings.JumpMontage)
	{
		if (UAnimInstance* Anim = GetMesh()->GetAnimInstance())
		{
			Anim->Montage_Play(Settings.JumpMontage);
		}
	}

	if (bHasLaunchCue)
	{
		PlatformJumpPhase = EPlatformJumpPhase::WindUp;
	}
	else
	{
		// 合図が無い構成では踏ん張りを挟まず即離陸する
		PlatformJumpPhase = EPlatformJumpPhase::WindUp;
		LaunchPlatformJump();
	}
}

void ABossCharacter::LaunchPlatformJump()
{
	if (PlatformJumpPhase != EPlatformJumpPhase::WindUp) return;

	// 踏ん張り中に微動していても着地点はそのまま、開始点だけ現在位置で取り直す
	PlatformJumpStart   = GetActorLocation();
	PlatformJumpElapsed = 0.0f;
	PlatformJumpPhase   = EPlatformJumpPhase::Airborne;

	// SetActorLocation駆動を重力/接地拘束に邪魔させない
	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->SetMovementMode(MOVE_Flying);
	}
}

void ABossCharacter::TickPlatformJump(float DeltaSeconds)
{
	const UBossDataAsset* BossData = Cast<UBossDataAsset>(GetCharacterData());
	const FBossPlatformJumpSettings& Settings =
		BossData ? BossData->PlatformJump : FBossPlatformJumpSettings();

	// 踏ん張り中: 離陸合図を待つ。来なければフェイルセーフで強制離陸する
	if (PlatformJumpPhase == EPlatformJumpPhase::WindUp)
	{
		PlatformJumpWindUp += DeltaSeconds;
		if (PlatformJumpWindUp >= Settings.WindUpTimeout)
		{
			LaunchPlatformJump();
		}
		return;
	}

	// 滞空中: 放物線で着地点へ移動する
	const float Duration = FMath::Max(Settings.JumpDuration, 0.05f);
	PlatformJumpElapsed += DeltaSeconds;
	const float Alpha = FMath::Clamp(PlatformJumpElapsed / Duration, 0.0f, 1.0f);

	// 水平は線形、垂直は線形 + 放物線アーチ (頂点 = ArcHeight)
	FVector Pos = FMath::Lerp(PlatformJumpStart, PlatformJumpTarget, Alpha);
	Pos.Z += Settings.ArcHeight * 4.0f * Alpha * (1.0f - Alpha);

	SetActorLocation(Pos, /*bSweep=*/false);

	if (Alpha >= 1.0f)
	{
		FinishPlatformJump();
	}
}

void ABossCharacter::FinishPlatformJump()
{
	const UBossDataAsset* BossData = Cast<UBossDataAsset>(GetCharacterData());
	const FBossPlatformJumpSettings& Settings =
		BossData ? BossData->PlatformJump : FBossPlatformJumpSettings();

	// 確定済みの着地点へスナップして通常移動へ戻す
	SetActorLocation(PlatformJumpTarget, /*bSweep=*/false);

	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->SetMovementMode(MOVE_Walking);
	}

	// 着地モンタージュセクションへジャンプして落下→着地を同期させる
	if (Settings.LandSectionName != NAME_None && Settings.JumpMontage)
	{
		if (UAnimInstance* Anim = GetMesh()->GetAnimInstance())
		{
			if (Anim->Montage_IsPlaying(Settings.JumpMontage))
			{
				Anim->Montage_JumpToSection(Settings.LandSectionName, Settings.JumpMontage);
			}
		}
	}

	RemoveStateTag(TAG_State_Enemy_PlatformJumping);
	PlatformJumpPhase = EPlatformJumpPhase::None;

	// キューに残りがあれば次の足場へ (1足場ずつ消化)
	if (PendingJumpTargets.Num() > 0)
	{
		const FVector Next = PendingJumpTargets[0];
		PendingJumpTargets.RemoveAt(0);
		BeginPlatformJump(Next);
	}
}

void ABossCharacter::OnDamageReceivedForStep(const FDamageInfo& DamageInfo)
{
	if (StatusComponent && StatusComponent->IsDead()) return;
	++StepPressure;
}
