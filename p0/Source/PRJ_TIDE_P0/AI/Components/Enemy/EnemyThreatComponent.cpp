// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#include "PRJ_TIDE_P0/AI/Components/Enemy/EnemyThreatComponent.h"
#include "PRJ_TIDE_P0/AI/Components/Enemy/EnemyPerceptionComponent.h"
#include "PRJ_TIDE_P0/Core/TideGameSettings.h"

#include "AIController.h"
#include "Perception/AISense_Sight.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

UEnemyThreatComponent::UEnemyThreatComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UEnemyThreatComponent::BeginPlay()
{
	Super::BeginPlay();

	// 同じAIControllerに載っている感知コンポーネントを購読する
	const AAIController* Controller = Cast<AAIController>(GetOwner());
	Perception = Controller ? Controller->FindComponentByClass<UEnemyPerceptionComponent>() : nullptr;
	if (Perception)
	{
		Perception->OnTargetPerceptionUpdated.AddDynamic(this, &UEnemyThreatComponent::OnPerceptionUpdated);
	}

	// 検知ゲージ更新と取得の定期ポーリング。視野に入りっぱなしでテリトリー境界を越える
	// ケースの補完もここが担う (イベントが出ないため)
	// ターゲット所持中はTryAcquireTargetが即リターンするので実コストはほぼ発生しない
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(AcquireTimerHandle, this,
			&UEnemyThreatComponent::PollDetection, PollInterval, true);
	}
}

void UEnemyThreatComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (Perception)
	{
		Perception->OnTargetPerceptionUpdated.RemoveDynamic(this, &UEnemyThreatComponent::OnPerceptionUpdated);
	}
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(AcquireTimerHandle);
	}

	Super::EndPlay(EndPlayReason);
}

void UEnemyThreatComponent::OnPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus)
{
	if (!Actor) return;

	// エンカウント型: 視覚で取得したら、視覚喪失(物陰・扇外れ)ではクリアしない
	// TargetActorの解除はテリトリー離脱(帰還BT)またはエンカウント停止
	// (SetEngaged(false))のみが行う
	// そのため感知「失敗」イベントはここでは扱わない(＝一度戦闘に入ったらエリア内で継続)
	if (!Stimulus.WasSuccessfullySensed()) return;

#if !UE_BUILD_SHIPPING
	if (UTideGameSettings::Get()->bDebugEnemyNoDetect) return;
#endif
	if (!bEngaged) return;

	// 交戦圏外の候補は視野内でも取得しない(領域外PCの検知↔帰還の高速往復を防ぐ)
	if (!IsWithinTerritory(Actor)) return;

	SetTargetActor(Actor);
}

void UEnemyThreatComponent::PollDetection()
{
	UpdateDetectionGauge(PollInterval);
	TryAcquireTarget();
}

void UEnemyThreatComponent::TryAcquireTarget()
{
	if (!bEngaged) return;

	// 既にターゲットを持っている場合は何もしない(イベント経路とエンジンのMaxAge記憶を妨げない)
	if (TargetActor != nullptr) return;

	if (AActor* Candidate = FindDetectionCandidate())
	{
		SetTargetActor(Candidate);
	}
}

void UEnemyThreatComponent::UpdateDetectionGauge(float DeltaSeconds)
{
	// 交戦中は満タン固定。降りるのはエンカウント停止=交戦圏離脱のSetEngaged(false)だけ
	if (bEngaged) return;

	if (FindDetectionCandidate())
	{
		// 0秒指定はゲージ無効=検知即戦闘
		DetectionGauge = DetectionFillSeconds > 0.0f
			? FMath::Min(1.0f, DetectionGauge + DeltaSeconds / DetectionFillSeconds)
			: 1.0f;

		// ターゲットはSetEngagedが現在の検知状態から再導出する
		if (DetectionGauge >= 1.0f)
		{
			SetEngaged(true);
		}
		return;
	}

	// 検知が切れている間は減衰する (0で減衰しない=溜まった分を維持)
	if (DetectionDecaySeconds > 0.0f)
	{
		DetectionGauge = FMath::Max(0.0f, DetectionGauge - DeltaSeconds / DetectionDecaySeconds);
	}
}

AActor* UEnemyThreatComponent::FindDetectionCandidate() const
{
	// スポナー配下は交戦圏(OuterVolume)の在圏だけで判定する。視野・LOSは条件にしない
	// (物陰や背後の配置で開戦しないのを避ける。徘徊圏侵入の問答無用開戦も、
	//  スポナーのSetEngaged(true)からこの経路でターゲットが埋まることで成立する)
	return HasTerritory()
		? FindPlayerWithinRadius(CombatRadius)
		: FindPerceivedActorInTerritory();
}

AActor* UEnemyThreatComponent::FindPlayerWithinRadius(float Radius) const
{
#if !UE_BUILD_SHIPPING
	if (UTideGameSettings::Get()->bDebugEnemyNoDetect) return nullptr;
#endif

	if (Radius <= 0.0f) return nullptr;

	APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!Player) return nullptr;

	// UEnemyBrainComponent::IsOutsidePatrolAreaと同じ水平距離に揃える
	return FVector::Dist2D(Player->GetActorLocation(), HomeOrigin) <= Radius ? Player : nullptr;
}

AActor* UEnemyThreatComponent::FindPerceivedActorInTerritory() const
{
	if (!Perception) return nullptr;

#if !UE_BUILD_SHIPPING
	if (UTideGameSettings::Get()->bDebugEnemyNoDetect) return nullptr;
#endif

	TArray<AActor*> CurrentlyPerceivedActors;
	Perception->GetCurrentlyPerceivedActors(UAISense_Sight::StaticClass(), CurrentlyPerceivedActors);

	for (AActor* PerceivedActor : CurrentlyPerceivedActors)
	{
		if (PerceivedActor && IsWithinTerritory(PerceivedActor))
		{
			return PerceivedActor;
		}
	}
	return nullptr;
}

void UEnemyThreatComponent::SetTargetActor(AActor* InTarget)
{
	TargetActor = InTarget;
}

int32 UEnemyThreatComponent::GetPerceivedActorCount() const
{
	if (!Perception) return -1;

	TArray<AActor*> CurrentlyPerceivedActors;
	Perception->GetCurrentlyPerceivedActors(UAISense_Sight::StaticClass(), CurrentlyPerceivedActors);
	return CurrentlyPerceivedActors.Num();
}

void UEnemyThreatComponent::SetEngaged(bool bInEngaged)
{
	bEngaged = bInEngaged;
	// ゲージは交戦状態のミラーとして扱う (交戦中=満タン / 非交戦=振り出し)
	DetectionGauge = bEngaged ? 1.0f : 0.0f;

	if (bEngaged)
	{
		RefreshTargetActor();
	}
	else
	{
		ClearTarget();
	}
}

void UEnemyThreatComponent::ApplyDetectionSettings(float InFillSeconds, float InDecaySeconds)
{
	DetectionFillSeconds = InFillSeconds;
	DetectionDecaySeconds = InDecaySeconds;
}

void UEnemyThreatComponent::SetTerritory(const FVector& Origin, float InPatrolRadius, float InCombatRadius)
{
	HomeOrigin = Origin;
	TerritoryRadius = InPatrolRadius;
	// OuterがInnerより小さく置かれた場合でもヒステリシスが逆転しないよう下限を張る
	CombatRadius = FMath::Max(InPatrolRadius, InCombatRadius);
}

void UEnemyThreatComponent::RefreshTargetActor()
{
	SetTargetActor(bEngaged ? FindDetectionCandidate() : nullptr);
}

void UEnemyThreatComponent::ClearTarget()
{
	SetTargetActor(nullptr);
}

bool UEnemyThreatComponent::IsWithinTerritory(const AActor* Actor) const
{
	if (!Actor) return false;
	// テリトリー未設定 (半径0=スポナー非所属) は無制限に検知する
	if (CombatRadius <= 0.0f) return true;
	// UEnemyBrainComponent::IsOutsidePatrolAreaと同じ水平距離・同じ半径に揃える
	return FVector::Dist2D(Actor->GetActorLocation(), HomeOrigin) <= CombatRadius;
}
