// Copyright (c) 2026, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.
#include "LockOnComponent.h"

#include "Kismet/KismetSystemLibrary.h"
#include "GameFramework/Actor.h"

#include "LockOnTargetComponent.h"

#if !UE_BUILD_SHIPPING
#include "imgui.h"
#endif

ULockOnComponent::ULockOnComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	// デフォルトでPawnを検索対象とする
	TargetObjectTypes.Add( UEngineTypes::ConvertToObjectType( ECollisionChannel::ECC_Pawn ) );
}

void ULockOnComponent::ToggleLockOn()
{
	if ( HasTarget() )
	{
		// 既にターゲットがいれば解除
		CurrentTarget = nullptr;
	}
	else
	{
		// ターゲットがいなければ検索して設定
		CurrentTarget = FindBestTarget();
	}
}

void ULockOnComponent::SetTarget( ULockOnTargetComponent* NewTarget )
{
	CurrentTarget = NewTarget;
}

void ULockOnComponent::ClearLockOn()
{
	CurrentTarget = nullptr;
}

bool ULockOnComponent::HasTarget() const
{
	return CurrentTarget.IsValid();
}

ULockOnTargetComponent* ULockOnComponent::GetTarget() const
{
	return CurrentTarget.Get();
}

ULockOnTargetComponent* ULockOnComponent::FindBestTarget() const
{
	APawn* OwnerPawn = Cast<APawn>( GetOwner() );
	if ( !OwnerPawn ) return nullptr;

	APlayerController* PC = Cast<APlayerController>( OwnerPawn->GetController() );
	if ( !PC || !PC->PlayerCameraManager ) return nullptr;

	TArray<AActor*> OverlappedActors;
	TArray<AActor*> ActorsToIgnore;
	ActorsToIgnore.Add( OwnerPawn );

	// 検索の中心はプレイヤーキャラクターのままとする
	UKismetSystemLibrary::SphereOverlapActors(
		this,
		OwnerPawn->GetActorLocation(),
		SearchRadius,
		TargetObjectTypes,
		AActor::StaticClass(),
		ActorsToIgnore,
		OverlappedActors
	);

	// カメラの位置と正面方向を取得
	const FVector CameraLocation = PC->PlayerCameraManager->GetCameraLocation();
	const FVector CameraForward = PC->PlayerCameraManager->GetActorForwardVector();

	ULockOnTargetComponent* BestTarget = nullptr;
	float BestScore = -MAX_flt;

	for ( AActor* Actor : OverlappedActors )
	{
		if ( !Actor ) continue;

		TArray<ULockOnTargetComponent*> TargetComps;
		Actor->GetComponents<ULockOnTargetComponent>( TargetComps );

		for ( ULockOnTargetComponent* TargetComp : TargetComps )
		{
			if ( !TargetComp->bIsTargetable ) continue;

			const FVector TargetLocation = TargetComp->GetTargetLocation();
			const FVector DirectionToTarget = ( TargetLocation - CameraLocation ).GetSafeNormal();

			const float Dot = FVector::DotProduct( CameraForward, DirectionToTarget );
			if ( Dot < MinDotProductThreshold ) continue;

			const float Distance = FVector::Distance( CameraLocation, TargetLocation );
			const float DistancePenalty = ( Distance / SearchRadius );

			// コンポーネント固有の優先度(TargetPriorityScore)もスコアに加味できる
			const float Score = ( Dot * AngleWeight ) - ( DistancePenalty * DistanceWeight ) + TargetComp->TargetPriorityScore;

			if ( Score > BestScore )
			{
				BestScore = Score;
				BestTarget = TargetComp;
			}
		}
	}

	return BestTarget;
}

#if !UE_BUILD_SHIPPING
void ULockOnComponent::DrawImGuiDebug()
{
	ImGui::Indent();

	// 1. 現在のターゲット状態の表示
	ImGui::Text( "ステータス:" );
	ImGui::SameLine();
	if ( HasTarget() )
	{
		ImGui::TextColored( ImVec4( 0.2f, 1.0f, 0.2f, 1.0f ), "ロックオン中" ); // 緑色で強調

		ImGui::Spacing();
		ImGui::Text( "【ターゲット情報】" );

		ULockOnTargetComponent* Target = CurrentTarget.Get();
		ImGui::BulletText( "名前: %s", TCHAR_TO_UTF8( *Target->GetName() ) );

		if ( AActor* OwnerActor = GetOwner() )
		{
			const float Distance = FVector::Distance( OwnerActor->GetActorLocation(), Target->GetTargetLocation() );
			ImGui::BulletText( "距離: %.1f", Distance );
		}
	}
	else
	{
		ImGui::TextColored( ImVec4( 0.6f, 0.6f, 0.6f, 1.0f ), "検索中..." ); // グレーで目立たせない
	}

	ImGui::Spacing();
	ImGui::Separator();
	ImGui::Spacing();

	// 2. 評価パラメータのリアルタイム調整スライダー
	ImGui::Text( "【スコア計算パラメータ】" );

	// スライダー用の定数（UI表示の上下限）
	constexpr float MinRadius = 500.0f;
	constexpr float MaxRadius = 5000.0f;
	constexpr float MinWeight = 0.0f;
	constexpr float MaxWeight = 5.0f;
	constexpr float MinDotThreshold = -1.0f;
	constexpr float MaxDotThreshold = 1.0f;

	ImGui::SliderFloat( "検索半径", &SearchRadius, MinRadius, MaxRadius );
	ImGui::SliderFloat( "角度ウェイト", &AngleWeight, MinWeight, MaxWeight );
	ImGui::SliderFloat( "距離ウェイト", &DistanceWeight, MinWeight, MaxWeight );
	ImGui::SliderFloat( "最小内積(角度)のしきい値", &MinDotProductThreshold, MinDotThreshold, MaxDotThreshold );

	ImGui::Unindent();
}
#endif
