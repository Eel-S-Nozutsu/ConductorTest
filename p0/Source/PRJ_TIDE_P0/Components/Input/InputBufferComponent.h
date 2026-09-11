// Copyright (c) 2026, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"

#include "InputBufferComponent.generated.h"

USTRUCT( BlueprintType )
struct FInputBufferData
{
	GENERATED_BODY()

	UPROPERTY()
	FGameplayTag CommandTag;

	UPROPERTY()
	float Timestamp = 0.0f;

	UPROPERTY()
	bool bIsConsumed = false;
};

// 入力バッファコンポーネント（先行入力や同時入力の解決）
UCLASS( ClassGroup = ( Custom ), meta = ( BlueprintSpawnableComponent ) )
class PRJ_TIDE_P0_API UInputBufferComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UInputBufferComponent();

	virtual void TickComponent( float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction ) override;

	void PushCommand( FGameplayTag CommandTag );
	bool ConsumeCommand( FGameplayTag CommandTag, float ValidDuration );
	void ClearCommand( FGameplayTag CommandTag );
	bool HasCommand( FGameplayTag CommandTag, float ValidDuration ) const;

protected:
	// 古いコマンドを破棄するまでの絶対時間
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Tide|InputBuffer" )
	float MaxBufferLifespan = 1.0f;

private:
	UPROPERTY()
	TArray<FInputBufferData> CommandBuffer;
};
