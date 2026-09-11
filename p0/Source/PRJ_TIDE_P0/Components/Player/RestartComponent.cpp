// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#include "PRJ_TIDE_P0/Components/Player/RestartComponent.h"
#include "PRJ_TIDE_P0/Actors/Character/Player/TidePlayerCharacter.h"
#include "PRJ_TIDE_P0/Components/Combat/StatusComponent.h"

#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"

URestartComponent::URestartComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void URestartComponent::BeginPlay()
{
	Super::BeginPlay();

	// トリガー未通過時のフォールバックとしてスポーン地点を初期リスタート地点にする
	if (const AActor* Owner = GetOwner())
	{
		RestartLocation = Owner->GetActorLocation();
		RestartRotation = Owner->GetActorRotation();
	}
}

void URestartComponent::SetRestartPoint(const FVector& InLocation, const FRotator& InRotation, int32 InOrder)
{
	// 前進のみ更新する ※古い進行順のトリガーへ戻っても巻き戻さない
	if (InOrder < CurrentOrder) return;

	RestartLocation = InLocation;
	RestartRotation = InRotation;
	CurrentOrder    = InOrder;
}

void URestartComponent::PushRestartOverride(const FVector& InLocation, const FRotator& InRotation, const AActor* InScope)
{
	if (!InScope) return;

	OverrideLocation = InLocation;
	OverrideRotation = InRotation;
	OverrideScope    = InScope;
}

void URestartComponent::ClearRestartOverride(const AActor* InScope)
{
	// 現在の持ち主からの解除だけ通す ※入れ違いで他スコープの上書きを消さない
	if (OverrideScope.Get() != InScope) return;

	OverrideScope.Reset();
}

void URestartComponent::RequestRestart()
{
	if (bIsRestarting) return;
	bIsRestarting = true;

	const ATidePlayerCharacter* Player = Cast<ATidePlayerCharacter>(GetOwner());
	if (!Player)
	{
		bIsRestarting = false;
		return;
	}

	// 暗転。完了後にExecuteRestartでテレポート＋復帰
	if (const APlayerController* PC = Cast<APlayerController>(Player->GetController()))
	{
		if (PC->PlayerCameraManager)
		{
			PC->PlayerCameraManager->StartCameraFade(0.0f, 1.0f, FadeOutDuration,
				FLinearColor::Black, false, true);
		}
	}

	GetWorld()->GetTimerManager().SetTimer(RestartTimerHandle, this,
		&URestartComponent::ExecuteRestart, FadeOutDuration, false);
}

void URestartComponent::ExecuteRestart()
{
	ATidePlayerCharacter* Player = Cast<ATidePlayerCharacter>(GetOwner());
	if (!Player)
	{
		bIsRestarting = false;
		return;
	}

	// ギミックルーム等の一時的な上書きがあればそちらを優先する
	// テレポートで在室判定が外れて上書きが解除されるので、先に地点を確定させる
	const bool bUseOverride = OverrideScope.IsValid();
	const FVector  TargetLocation = bUseOverride ? OverrideLocation : RestartLocation;
	const FRotator TargetRotation = bUseOverride ? OverrideRotation : RestartRotation;

	// リスタート地点へテレポートし、向きと移動を整える
	Player->SetActorLocationAndRotation(TargetLocation, TargetRotation,
		false, nullptr, ETeleportType::TeleportPhysics);

	if (UCharacterMovementComponent* Movement = Player->GetCharacterMovement())
	{
		Movement->StopMovementImmediately();
	}

	Player->CancelAllActions();

	// カメラ(操作回転)も復帰向きへ合わせる
	if (APlayerController* PC = Cast<APlayerController>(Player->GetController()))
	{
		PC->SetControlRotation(TargetRotation);
	}

	// HP全回復 ※死亡状態ならReviveが解除も行う
	if (UStatusComponent* Status = Player->GetStatusComponent())
	{
		Status->Revive(1.0f);
	}

	// フェードイン
	if (const APlayerController* PC = Cast<APlayerController>(Player->GetController()))
	{
		if (PC->PlayerCameraManager)
		{
			PC->PlayerCameraManager->StartCameraFade(1.0f, 0.0f, FadeInDuration,
				FLinearColor::Black, false, false);
		}
	}

	bIsRestarting = false;
}
