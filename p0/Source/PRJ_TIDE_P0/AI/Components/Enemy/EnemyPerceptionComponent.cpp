// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#include "PRJ_TIDE_P0/AI/Components/Enemy/EnemyPerceptionComponent.h"
#include "PRJ_TIDE_P0/Core/TideGameSettings.h"
#include "PRJ_TIDE_P0/Actors/Character/Enemy/EnemyCharacter.h"

#include "Perception/AISenseConfig_Sight.h"
#include "Perception/AISenseConfig_Damage.h"

#include "AIController.h"
#include "GameFramework/Pawn.h"
#include "Components/DecalComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "DrawDebugHelpers.h"

UEnemyPerceptionComponent::UEnemyPerceptionComponent()
{
	PrimaryComponentTick.bCanEverTick = true;

	// 視覚
	SightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("SightConfig"));
	SightConfig->SightRadius = 1600.0f;
	SightConfig->LoseSightRadius = 1600.0f; // ヒステリシス不使用 (エンカウント型)。ApplySightSettingsで上書き
	SightConfig->PeripheralVisionAngleDegrees = 120.0f;
	SightConfig->SetMaxAge(5.0f);
	SightConfig->AutoSuccessRangeFromLastSeenLocation = -1.0f;

	SightConfig->DetectionByAffiliation.bDetectEnemies = true;
	SightConfig->DetectionByAffiliation.bDetectNeutrals = false;
	SightConfig->DetectionByAffiliation.bDetectFriendlies = false;

	SensesConfig.Add(SightConfig);
	SetDominantSense(SightConfig->GetSenseImplementation());

	// 触覚
	DamageSenseConfig = CreateDefaultSubobject<UAISenseConfig_Damage>(TEXT("DamageConfig"));
	SensesConfig.Add(DamageSenseConfig);
}

void UEnemyPerceptionComponent::ApplySightSettings(
	float InSightRadius, float InPeripheralVisionAngle)
{
	if (!SightConfig) return;
	SightConfig->SightRadius = InSightRadius;
	// エンカウント型ではロスト半径のヒステリシスを使わないためLoseSightRadius =
	// SightRadiusに揃える
	SightConfig->LoseSightRadius = InSightRadius;
	SightConfig->PeripheralVisionAngleDegrees = InPeripheralVisionAngle;
	RequestStimuliListenerUpdate();
}

void UEnemyPerceptionComponent::TickComponent(
	float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

#if !UE_BUILD_SHIPPING
	UpdateDebugFOVDecal();
#endif
}

void UEnemyPerceptionComponent::UpdateDebugFOVDecal()
{
	if (!UTideGameSettings::Get()->bDebugDrawEnemyFOV)
	{
		if (DebugFOVDecal && DebugFOVDecal->IsVisible())
		{
			DebugFOVDecal->SetVisibility(false);
		}
		return;
	}

	// デカールは所有Pawnのルートへ空間的にぶら下げる ロジックは知覚コンポーネントが握る
	const AAIController* Controller = Cast<AAIController>(GetOwner());
	APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
	if (!Pawn) return;

	// 検知マーカー: 検知中のみEM足元(黄球) / ターゲット足元(赤球) / 両者を結ぶ(赤線)
	// を描く。扇デカールとは独立 (マテリアル未設定でも出す)
#if ENABLE_DRAW_DEBUG
	const AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(Pawn);
	AActor* TargetActor = Enemy ? Enemy->GetTargetActor() : nullptr;
	if (TargetActor)
	{
		if (UWorld* World = GetWorld())
		{
			// アクターの簡易衝突半径ぶん下げて足元を求める
			auto FeetOf = [](const AActor* A)
			{
				FVector Loc = A->GetActorLocation();
				Loc.Z -= A->GetSimpleCollisionHalfHeight();
				return Loc;
			};
			const FVector EnemyFeet = FeetOf(Pawn);
			const FVector TargetFeet = FeetOf(TargetActor);
			DrawDebugSphere(World, EnemyFeet, 40.0f, 12, FColor::Yellow, false, -1.0f, 0, 2.0f);
			DrawDebugSphere(World, TargetFeet, 40.0f, 12, FColor::Red, false, -1.0f, 0, 2.0f);
			DrawDebugLine(World, EnemyFeet, TargetFeet, FColor::Red, false, -1.0f, 0, 2.0f);
		}
	}
#endif

	// 扇デカール (マテリアル未設定なら非表示にして終了)
	if (!DebugFOVMaterial || !SightConfig)
	{
		if (DebugFOVDecal && DebugFOVDecal->IsVisible())
		{
			DebugFOVDecal->SetVisibility(false);
		}
		return;
	}

	const float SightRadius = SightConfig->SightRadius;
	const float LoseSightRadius = SightConfig->LoseSightRadius;
	const float HalfAngleDeg = SightConfig->PeripheralVisionAngleDegrees;
	const float MaxRadius = FMath::Max(SightRadius, LoseSightRadius) + 100.0f;

	if (!IsValid(DebugFOVDecal))
	{
		DebugFOVDecal = NewObject<UDecalComponent>(Pawn, TEXT("DebugFOVDecal"));
		DebugFOVDecal->SetupAttachment(Pawn->GetRootComponent());
		DebugFOVDecal->SetUsingAbsoluteScale(true); // 敵に影響されずワールドcm実寸で扱う
		DebugFOVDecal->RegisterComponent();
		DebugFOVDecal->SetWorldScale3D(FVector::OneVector);
		DebugFOVDecal->SetDecalMaterial(DebugFOVMaterial);
		DebugFOVMID = DebugFOVDecal->CreateDynamicMaterialInstance();
	}

	// 真下へ投影する向き 扇の向きはマテリアルへ前方ベクトルで渡すので
	// デカール自体のヨーは固定でよい 投影深さは坂でも地面に届くよう大きめに
	const FVector Center = Pawn->GetActorLocation();
	const float Depth = 2000.0f;

	DebugFOVDecal->SetVisibility(true);
	DebugFOVDecal->SetWorldLocationAndRotation(Center, FRotator(-90.0f, 0.0f, 0.0f));
	DebugFOVDecal->DecalSize = FVector(Depth, MaxRadius * 2.0f, MaxRadius * 2.0f);
	DebugFOVDecal->MarkRenderStateDirty();

	if (DebugFOVMID)
	{
		const FVector Fwd = Pawn->GetActorForwardVector().GetSafeNormal2D();

		DebugFOVMID->SetVectorParameterValue(TEXT("Forward"), FLinearColor(Fwd.X, Fwd.Y, 0.0f));
		DebugFOVMID->SetVectorParameterValue(TEXT("Center"), FLinearColor(Center.X, Center.Y, Center.Z));
		DebugFOVMID->SetScalarParameterValue(TEXT("Radius"), SightRadius);
		DebugFOVMID->SetScalarParameterValue(TEXT("LoseRadius"), LoseSightRadius);
		DebugFOVMID->SetScalarParameterValue(TEXT("HalfAngleDeg"), HalfAngleDeg);
		DebugFOVMID->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.0f, 0.05f, 0.3f));
	}
}
