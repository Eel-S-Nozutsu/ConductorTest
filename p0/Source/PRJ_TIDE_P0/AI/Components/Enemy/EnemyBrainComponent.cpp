// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#include "PRJ_TIDE_P0/AI/Components/Enemy/EnemyBrainComponent.h"
#include "PRJ_TIDE_P0/AI/Components/Enemy/EnemyThreatComponent.h"
#include "PRJ_TIDE_P0/AI/Controllers/Enemy/EnemyAIController.h"
#include "PRJ_TIDE_P0/AI/Director/AIDirector.h"
#include "PRJ_TIDE_P0/AI/States/EnemyAIStateBase.h"
#include "PRJ_TIDE_P0/AI/States/EnemyAIState_Approach.h"
#include "PRJ_TIDE_P0/AI/States/EnemyAIState_Attack.h"
#include "PRJ_TIDE_P0/AI/States/EnemyAIState_BossIdle.h"
#include "PRJ_TIDE_P0/AI/States/EnemyAIState_Patrol.h"
#include "PRJ_TIDE_P0/AI/States/EnemyAIState_React.h"
#include "PRJ_TIDE_P0/AI/States/EnemyAIState_Retreat.h"
#include "PRJ_TIDE_P0/AI/States/EnemyAIState_ReturnToOrigin.h"
#include "PRJ_TIDE_P0/AI/States/EnemyAIState_Strafe.h"
#include "PRJ_TIDE_P0/AI/States/EnemyAIState_Suspended.h"
#include "PRJ_TIDE_P0/AI/States/EnemyAIState_Wait.h"
#include "PRJ_TIDE_P0/Actors/Character/Enemy/BossCharacter.h"
#include "PRJ_TIDE_P0/Actors/Character/Enemy/EnemyCharacter.h"
#include "PRJ_TIDE_P0/Components/AI/EnemyBattleComponent.h"
#include "PRJ_TIDE_P0/Components/Combat/HitReactionComponent.h"
#include "PRJ_TIDE_P0/Components/Combat/StatusComponent.h"
#include "PRJ_TIDE_P0/Data/Enemy/EnemyDataAsset.h"
#include "PRJ_TIDE_P0/Core/TideGameSettings.h"
#include "PRJ_TIDE_P0/Core/TidePlayerController.h"

#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Navigation/PathFollowingComponent.h"

UEnemyBrainComponent::UEnemyBrainComponent()
{
	PrimaryComponentTick.bCanEverTick = true;

	// StartLogicまで停めておく。BT運用のままの敵ではティックさせない
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UEnemyBrainComponent::StartLogic()
{
	AEnemyCharacter* Enemy = GetEnemy();
	if (!Enemy) return;

	bIsBoss = Enemy->IsA<ABossCharacter>();
	HitReaction = Enemy->FindComponentByClass<UHitReactionComponent>();
	Status = Enemy->GetStatusComponent();

	// 状態は起動時に1インスタンスずつ生成して使い回す
	// ザコとボスで使う状態の集合は違うが、選ばれない状態を持っていても害はないので生成は分けない
	// (どちらが選ばれるかはAI設定が決める
	// 例: KeepDistance < 0ならRetreatは選ばれない)待機はザコとボスで中身が違う
	// ボスは待機中に振り向き・後退ステップを差し込む
	if (bIsBoss)
	{
		States.Add(EEnemyAIState::Idle,       NewObject<UEnemyAIState_BossIdle>(this));
		States.Add(EEnemyAIState::Hold,       NewObject<UEnemyAIState_BossIdle>(this));
	}
	else
	{
		States.Add(EEnemyAIState::Idle,       NewObject<UEnemyAIState_Wait>(this));
		States.Add(EEnemyAIState::Hold,       NewObject<UEnemyAIState_Wait>(this));
	}
	States.Add(EEnemyAIState::Patrol,         NewObject<UEnemyAIState_Patrol>(this));
	States.Add(EEnemyAIState::ReturnToOrigin, NewObject<UEnemyAIState_ReturnToOrigin>(this));
	States.Add(EEnemyAIState::Approach,       NewObject<UEnemyAIState_Approach>(this));
	States.Add(EEnemyAIState::Retreat,        NewObject<UEnemyAIState_Retreat>(this));
	States.Add(EEnemyAIState::Strafe,         NewObject<UEnemyAIState_Strafe>(this));
	States.Add(EEnemyAIState::Attack,         NewObject<UEnemyAIState_Attack>(this));
	States.Add(EEnemyAIState::React,          NewObject<UEnemyAIState_React>(this));
	States.Add(EEnemyAIState::Suspended,      NewObject<UEnemyAIState_Suspended>(this));

	// シネマモード問い合わせ先をキャッシュ(実プレイヤーはATidePlayerController派生)
	CachedPlayerController = Cast<ATidePlayerController>(UGameplayStatics::GetPlayerController(this, 0));

	SetComponentTickEnabled(true);
}

void UEnemyBrainComponent::StopLogicForDeath()
{
	ClearAllAttackRequests();
	TransitionTo(EEnemyAIState::None);
	LeaveCombatantRegistry();
	SetComponentTickEnabled(false);
}

void UEnemyBrainComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 死亡を経ずに消える経路 (レベル遷移・デスポーン) でも参加者リストに残さない
	LeaveCombatantRegistry();

	Super::EndPlay(EndPlayReason);
}

void UEnemyBrainComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	SyncCombatantRegistration();
	TickPickAttack(DeltaTime);

	const EEnemyAIState Desired = SelectDesiredState();

	// Attack終了(IsBusy=false)後も攻撃予約が残り、
	// Desiredがそのままなら同一ステートで再入し消化
	// しないと再遷移が塞がれEnter未呼びで棒立ち
	const bool bAttackNeedsRestart = Desired == EEnemyAIState::Attack
		&& CurrentStateId == EEnemyAIState::Attack
		&& CurrentState && !CurrentState->IsBusy();

	if ((Desired != CurrentStateId || bAttackNeedsRestart) && CanInterruptCurrentState(Desired))
	{
		TransitionTo(Desired);
	}

	if (CurrentState)
	{
		CurrentState->Tick(*this, DeltaTime);
	}
}

EEnemyAIState UEnemyBrainComponent::SelectDesiredState()
{
	if (Status && Status->IsDead())     return EEnemyAIState::Dead;
	// カットシーンは死亡を除く全てに優先する
	// 攻撃中でも固める (割り込み許可はCanInterruptCurrentState側)
	if (IsCinematicSuspended())         return EEnemyAIState::Suspended;
	if (IsReacting())                   return EEnemyAIState::React;
	if (ForceAttackIndex >= 0)          return EEnemyAIState::Attack;
	if (IsOutsidePatrolArea())          return EEnemyAIState::ReturnToOrigin;

	if (!GetTargetActor())
	{
		return bIsBoss ? EEnemyAIState::Idle : EEnemyAIState::Patrol;
	}

	if (CounterAttackIndex >= 0)        return EEnemyAIState::Attack;
	if (PendingAttackIndex >= 0)        return EEnemyAIState::Attack;

	// 判定順はBT_CombatのSelectorと揃える
	// IsTooClose/IsInEngageRangeはKeepDistanceの有効/無効で排他にな
	// るので、ザコとボスで枝が分かれる
	if (IsTooClose())                   return EEnemyAIState::Retreat;
	if (IsInEngageRange())
	{
		return bIsBoss ? EEnemyAIState::Hold : EEnemyAIState::Strafe;
	}
	if (IsTooFar())                     return EEnemyAIState::Approach;

	return bIsBoss ? EEnemyAIState::Idle : EEnemyAIState::Hold;
}

bool UEnemyBrainComponent::CanInterruptCurrentState(EEnemyAIState Desired) const
{
	// 仕事を終えた状態は割り込み規則の対象外。ここを見ないと、攻撃が終わっているのに
	// 「Attackは割り込み不可」の規則が次の状態への遷移まで拒み続けて棒立ちになる
	if (CurrentState && !CurrentState->IsBusy()) return true;

	switch (CurrentStateId)
	{
	case EEnemyAIState::Attack:
		// 強制攻撃 (協力攻撃の徴集・デバッグ) は実行中の攻撃より優先する
		// カットシーン (Suspended) は攻撃モンタージュ中でもハードに止める
		return Desired == EEnemyAIState::React
			|| Desired == EEnemyAIState::Dead
			|| Desired == EEnemyAIState::Suspended
			|| ForceAttackIndex >= 0;

	case EEnemyAIState::Dead:
		return false;

	default:
		// 移動・待機系は毎ティックの優先度再評価に従ってよい
		// Reactも含む: Reactは優先度が最上位なので、リアクション中は他が選ばれず、
		// 終われば抜けてよい (規則で縛るとAttackと同じ固まり方をする)
		return true;
	}
}

void UEnemyBrainComponent::TransitionTo(EEnemyAIState Next)
{
	if (CurrentState)
	{
		CurrentState->Exit(*this);
	}

	CurrentStateId = Next;
	CurrentState = FindState(Next);

	if (CurrentState)
	{
		CurrentState->Enter(*this);
	}
}

UEnemyAIStateBase* UEnemyBrainComponent::FindState(EEnemyAIState Id) const
{
	const TObjectPtr<UEnemyAIStateBase>* Found = States.Find(Id);
	return Found ? Found->Get() : nullptr;
}

void UEnemyBrainComponent::TickPickAttack(float DeltaSeconds)
{
	// BTService_PickAttackと同じ間隔
	// BTではBT_Combat直下のサービスだったので、
	// ストレイフ・接近・後退のどれを実行中でも回っていた。ここでも状態を問わず回す
	constexpr float Interval = 0.2f;

	// のけぞり中は抽選しない。ReactはAttackより優先されるので、ここで当選すると復帰まで
	// (1〜2秒) 保留され、凍結された古い距離・角度のまま発動してしまう。被弾直前に積んでいた
	// 予約も同じ理由で捨て、復帰後に新しい条件で選び直させる
	// 蓄積は満たしたままにしておき、復帰した最初のティックで即抽選する。TickPickAttackは
	// SelectDesiredStateより先に走るので、被弾直後に攻撃を返す積極性は落ちない
	if (IsReacting())
	{
		PendingAttackIndex = -1;
		PickAttackAccum = Interval;
		return;
	}

	PickAttackAccum += DeltaSeconds;
	if (PickAttackAccum < Interval) return;
	PickAttackAccum = 0.0f;

	// 攻撃実行中 (通常/強制/好機のいずれか) は新たな抽選を積まない
	// PendingAttackIndexだけを見ると、強制攻撃 (ForceAttackIndex)
	// の実行中に通常抽選がPendingAttackIndexを積んでしまい、
	// 強制攻撃終了でForceAttackIndexだけ消費された後もPendingが残ってAttackス
	// テートへ張り付く (再EnterされずbActive=falseのまま棒立ち)
	if (GetActiveAttackIndex() >= 0) return;
	if (!GetTargetActor()) return;

	AEnemyCharacter* Enemy = GetEnemy();
	if (!Enemy || Enemy->IsDodging()) return;

	const float DistToTarget = Enemy->GetDistToTarget();
	if (DistToTarget < 0.0f) return;

	UEnemyBattleComponent* Battle = GetBattleComponent();
	if (!Battle) return;

	int32 SelectedIndex = -1;
	if (Battle->TryPickAttack(DistToTarget, Enemy->GetAngleToTarget(), SelectedIndex))
	{
		PendingAttackIndex = SelectedIndex;
	}
}

void UEnemyBrainComponent::SyncCombatantRegistration()
{
	AEnemyCharacter* Enemy = GetEnemy();
	UAIDirector* Director = Enemy ? Enemy->GetWorld()->GetSubsystem<UAIDirector>() : nullptr;
	if (!Director) return;

	// 参加条件はターゲットを持っていること。巡回中・帰還中の敵まで数えると
	// EQSの混雑判定が遠くの無関係な個体で埋まる
	const bool bInCombat = GetTargetActor() != nullptr && !(Status && Status->IsDead());
	if (bInCombat) Director->RegisterCombatant(Enemy);
	else           Director->UnregisterCombatant(Enemy);
}

void UEnemyBrainComponent::LeaveCombatantRegistry()
{
	AEnemyCharacter* Enemy = GetEnemy();
	UWorld* World = Enemy ? Enemy->GetWorld() : nullptr;
	if (!World) return;

	if (UAIDirector* Director = World->GetSubsystem<UAIDirector>())
	{
		Director->UnregisterCombatant(Enemy);
	}
}

// ---- 条件判定 ----

bool UEnemyBrainComponent::IsReacting() const
{
#if !UE_BUILD_SHIPPING
	// デバッグのAI停止。BTではStopLogic("DebugAIStop") だったもの
	if (UTideGameSettings::Get()->bDebugEnemyAIStop) return true;
#endif

	// 被弾・ガード・崖落下すべての「行動できない」はAEnemyCharacterが唯一の真実を持つ
	const AEnemyCharacter* Enemy = GetEnemy();
	return Enemy && Enemy->IsReacting();
}

bool UEnemyBrainComponent::IsCinematicSuspended() const
{
	ATidePlayerController* PC = CachedPlayerController.Get();
	if (!PC)
	{
		// StartLogic時にPC未生成だった場合の引き直し
		PC = Cast<ATidePlayerController>(UGameplayStatics::GetPlayerController(this, 0));
		CachedPlayerController = PC;
	}
	return PC && PC->IsInCinematicMode();
}

bool UEnemyBrainComponent::IsOutsidePatrolArea() const
{
	const AEnemyAIController* AIController = GetAIController();
	const UEnemyThreatComponent* Threat = AIController ?
		AIController->ThreatComponent : nullptr;
	if (!Threat) return false;

	// 交戦中の許容範囲は交戦圏(OuterVolume)。徘徊圏(Inner)で判定すると
	// スポナーがまだ交戦中(Inner侵入で起動しOuter離脱まで継続)なのに
	// Innerを出た瞬間に帰還してしまい、ヒステリシスが無くなる
	// 非交戦時は徘徊圏まで詰めて、追跡をやめた敵をスポーン地点へ戻す
	const float BoundRadius = Threat->IsEngaged()
		? Threat->GetCombatRadius()
		: Threat->GetPatrolRadius();
	if (BoundRadius <= 0.0f) return false;

	const AEnemyCharacter* Enemy = GetEnemy();
	if (!Enemy) return false;

	const FVector& PatrolOrigin = Threat->GetPatrolOrigin();
	if (FVector::Dist2D(Enemy->GetActorLocation(), PatrolOrigin) > BoundRadius) return true;

	// ターゲットが圏外へ出た時点でも帰還させる
	const AActor* Target = Threat->GetTargetActor();
	return Target && FVector::Dist2D(Target->GetActorLocation(), PatrolOrigin) > BoundRadius;
}

bool UEnemyBrainComponent::IsTooClose()
{
	const AEnemyCharacter* Enemy = GetEnemy();
	const AActor* Target = GetTargetActor();
	const UEnemyDataAsset* Data = GetEnemyData();
	if (!Enemy || !Target || !Data)
	{
		bWasTooClose = false;
		return false;
	}

	// -1 (無効) の敵は後退しない
	const float KeepDistance = Data->AISettings.KeepDistance;
	if (KeepDistance < 0.0f)
	{
		bWasTooClose = false;
		return false;
	}

	// 発動はKeepDistance、発動中は解除ラインまで離れるまで維持する
	const float Threshold = bWasTooClose ?
		Data->AISettings.GetKeepReleaseDistance() : KeepDistance;

	const float DistSq = FVector::DistSquared2D(
		Enemy->GetActorLocation(), Target->GetActorLocation());
	bWasTooClose = DistSq < FMath::Square(Threshold);

	return bWasTooClose;
}

bool UEnemyBrainComponent::IsInEngageRange()
{
	const AEnemyCharacter* Enemy = GetEnemy();
	const AActor* Target = GetTargetActor();
	const UEnemyDataAsset* Data = GetEnemyData();
	if (!Enemy || !Target || !Data)
	{
		bWasInEngageRange = false;
		return false;
	}

	// 距離維持が有効な敵はストレイフしない。PC中心グリッドEQSがデッドゾーンに届かず
	// 失敗して接近へ抜け、間合いと接近を往復するのを防ぐ
	if (Data->AISettings.KeepDistance >= 0.0f)
	{
		bWasInEngageRange = false;
		return false;
	}

	// 接敵中は解除側の閾値まで維持する
	const float Threshold = bWasInEngageRange
		? Data->AISettings.EngageRangeThreshold + FMath::Max(
			Data->AISettings.EngageRangeExitMargin, 0.0f)
		: Data->AISettings.EngageRangeThreshold;

	const float DistSq = FVector::DistSquared2D(
		Enemy->GetActorLocation(), Target->GetActorLocation());
	bWasInEngageRange = DistSq <= FMath::Square(Threshold);

	return bWasInEngageRange;
}

bool UEnemyBrainComponent::IsTooFar()
{
	const AEnemyCharacter* Enemy = GetEnemy();
	const AActor* Target = GetTargetActor();
	const UEnemyDataAsset* Data = GetEnemyData();
	if (!Enemy || !Target || !Data)
	{
		bWasTooFar = false;
		return false;
	}

	// 距離維持を使わない敵 (KeepDistance < 0) は従来どおり無条件で接近させる
	const float FarThreshold = Data->AISettings.GetKeepFarThreshold();
	if (FarThreshold < 0.0f)
	{
		bWasTooFar = true;
		return true;
	}

	// 接近中はキープ帯へ入るまで維持するヒス幅がKeepBandを超えてもKeepDistanceを下回ら
	// ないよう守る
	const float ReleaseThreshold = FMath::Max(
		FarThreshold - Data->AISettings.GetKeepHysteresis(), Data->AISettings.KeepDistance);
	const float Threshold = bWasTooFar ? ReleaseThreshold : FarThreshold;

	const float DistSq = FVector::DistSquared2D(
		Enemy->GetActorLocation(), Target->GetActorLocation());
	bWasTooFar = DistSq > FMath::Square(Threshold);

	return bWasTooFar;
}

// ---- 攻撃要求 ----

void UEnemyBrainComponent::RequestForceAttack(int32 AttackIndex)
{
	ForceAttackIndex = AttackIndex;
}

void UEnemyBrainComponent::RequestCounterAttack(int32 AttackIndex)
{
	CounterAttackIndex = AttackIndex;
}

void UEnemyBrainComponent::ConsumeActiveAttackRequest()
{
	// GetActiveAttackIndexと同じ優先度で1つだけ取り下げる
	// まとめて消すと、強制攻撃の裏で抽選済みだった通常攻撃まで捨ててしまう
	if (ForceAttackIndex >= 0)        ForceAttackIndex = -1;
	else if (CounterAttackIndex >= 0) CounterAttackIndex = -1;
	else                              PendingAttackIndex = -1;
}

void UEnemyBrainComponent::ClearAllAttackRequests()
{
	ForceAttackIndex = -1;
	CounterAttackIndex = -1;
	PendingAttackIndex = -1;
}

int32 UEnemyBrainComponent::GetActiveAttackIndex() const
{
	// SelectDesiredStateの評価順と揃える
	if (ForceAttackIndex >= 0)   return ForceAttackIndex;
	if (CounterAttackIndex >= 0) return CounterAttackIndex;
	return PendingAttackIndex;
}

// ---- アクセサ ----

AEnemyAIController* UEnemyBrainComponent::GetAIController() const
{
	return Cast<AEnemyAIController>(GetOwner());
}

AEnemyCharacter* UEnemyBrainComponent::GetEnemy() const
{
	const AEnemyAIController* AIController = GetAIController();
	return AIController ? Cast<AEnemyCharacter>(AIController->GetPawn()) : nullptr;
}

AActor* UEnemyBrainComponent::GetTargetActor() const
{
	const AEnemyAIController* AIController = GetAIController();
	return AIController && AIController->ThreatComponent
		? AIController->ThreatComponent->GetTargetActor()
		: nullptr;
}

FVector UEnemyBrainComponent::GetPatrolOrigin() const
{
	const AEnemyAIController* AIController = GetAIController();
	return AIController && AIController->ThreatComponent
		? AIController->ThreatComponent->GetPatrolOrigin()
		: FVector::ZeroVector;
}

float UEnemyBrainComponent::GetPatrolRadius() const
{
	const AEnemyAIController* AIController = GetAIController();
	return AIController && AIController->ThreatComponent
		? AIController->ThreatComponent->GetPatrolRadius()
		: 0.0f;
}

float UEnemyBrainComponent::GetCombatRadius() const
{
	const AEnemyAIController* AIController = GetAIController();
	return AIController && AIController->ThreatComponent
		? AIController->ThreatComponent->GetCombatRadius()
		: 0.0f;
}

const UEnemyDataAsset* UEnemyBrainComponent::GetEnemyData() const
{
	const AEnemyCharacter* Enemy = GetEnemy();
	return Enemy ? Cast<UEnemyDataAsset>(Enemy->GetCharacterData()) : nullptr;
}

UEnemyBattleComponent* UEnemyBrainComponent::GetBattleComponent() const
{
	const AEnemyCharacter* Enemy = GetEnemy();
	return Enemy ? Enemy->GetBattleComponent() : nullptr;
}

// ---- 移動 ----

bool UEnemyBrainComponent::RequestMoveTo(const FVector& Goal, float AcceptanceRadius,
	bool bCanStrafe, bool bStrictAcceptance)
{
	AEnemyAIController* AIController = GetAIController();
	if (!AIController) return false;

	FAIMoveRequest MoveReq(Goal);
	MoveReq.SetAcceptanceRadius(AcceptanceRadius);
	MoveReq.SetUsePathfinding(true);
	MoveReq.SetCanStrafe(bCanStrafe);
	MoveReq.SetReachTestIncludesAgentRadius(!bStrictAcceptance);

	const FPathFollowingRequestResult Result = AIController->MoveTo(MoveReq);
	return Result.Code == EPathFollowingRequestResult::RequestSuccessful
		|| Result.Code == EPathFollowingRequestResult::AlreadyAtGoal;
}

bool UEnemyBrainComponent::RequestMoveToActor(AActor* Goal, float AcceptanceRadius, bool bCanStrafe)
{
	AEnemyAIController* AIController = GetAIController();
	if (!AIController || !Goal) return false;

	FAIMoveRequest MoveReq(Goal);
	MoveReq.SetAcceptanceRadius(AcceptanceRadius);
	MoveReq.SetUsePathfinding(true);
	MoveReq.SetCanStrafe(bCanStrafe);

	const FPathFollowingRequestResult Result = AIController->MoveTo(MoveReq);
	return Result.Code == EPathFollowingRequestResult::RequestSuccessful
		|| Result.Code == EPathFollowingRequestResult::AlreadyAtGoal;
}

void UEnemyBrainComponent::StopMovement()
{
	if (AEnemyAIController* AIController = GetAIController())
	{
		AIController->StopMovement();
	}
}

bool UEnemyBrainComponent::IsMoveInProgress() const
{
	const AEnemyAIController* AIController = GetAIController();
	return AIController && AIController->GetMoveStatus() != EPathFollowingStatus::Idle;
}

void UEnemyBrainComponent::ApplyMoveSpeed(EEnemySpeedType Speed)
{
	AEnemyCharacter* Enemy = GetEnemy();
	const UEnemyDataAsset* Data = GetEnemyData();
	if (!Enemy || !Data) return;

	UCharacterMovementComponent* MoveComp = Enemy->GetCharacterMovement();
	if (!MoveComp) return;

	const FEnemyAISettings& AI = Data->AISettings;
	switch (Speed)
	{
	case EEnemySpeedType::Strafe: MoveComp->MaxWalkSpeed = AI.StrafeSpeed; break;
	case EEnemySpeedType::Run:    MoveComp->MaxWalkSpeed = AI.RunSpeed;    break;
	default:                      MoveComp->MaxWalkSpeed = AI.WalkSpeed;   break;
	}
}

FString UEnemyBrainComponent::GetCurrentStateDebugText() const
{
	const FString StateName = StaticEnum<EEnemyAIState>()->GetNameStringByValue(
		static_cast<int64>(CurrentStateId));
	const FString Detail = CurrentState ? CurrentState->GetDebugText() : FString();

	return Detail.IsEmpty() ? StateName : FString::Printf(TEXT("%s (%s)"), *StateName, *Detail);
}
