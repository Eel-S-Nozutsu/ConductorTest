// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SignalReceiverComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSignalReceived);

/**
 * シグナルバス(UTideSignalSubsystem)の受信側コンポーネント。
 * 扉などのギミックBPに付けてListenSignalNameを設定し、
 * OnSignalReceivedイベントに開扉などの演出を繋ぐだけで疎結合に連動できる。
 *
 * 発火済みシグナルはラッチされるため、本コンポーネントの登録が発火より後でも受信できる。
 */
UCLASS(ClassGroup = (Tide), meta = (BlueprintSpawnableComponent))
class PRJ_TIDE_P0_API USignalReceiverComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	USignalReceiverComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// 受信するシグナル名。発火側(スポナーのClearedSignalName等)と一致させる
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tide|Signal")
	FName ListenSignalName;

	// 登録時点で既に発火済みなら即座にOnSignalReceivedを呼ぶか
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tide|Signal")
	bool bFireIfAlreadyBroadcast = true;

	// シグナル受信時に発火する。BPからギミック演出を繋ぐ
	UPROPERTY(BlueprintAssignable, Category = "Tide|Signal")
	FOnSignalReceived OnSignalReceived;

private:

	void HandleSignal();

	FDelegateHandle ListenHandle;

};
