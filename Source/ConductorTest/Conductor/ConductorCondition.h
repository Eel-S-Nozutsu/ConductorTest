// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Conductor/ConductorObjectBase.h"
#include "ConductorCondition.generated.h"

class UContentConductor;

/**
 * フェーズ遷移の条件 真偽を返すのみで副作用は持たない
 */
UCLASS(Abstract, Blueprintable, BlueprintType)
class CONDUCTORTEST_API UConductorCondition : public UConductorObjectBase
{
	GENERATED_BODY()

public:
	void BeginEvaluation(UContentConductor* InContentConductor);
	void EndEvaluation();

	bool IsEvaluating() const { return bActive; }

	UFUNCTION(BlueprintNativeEvent)
	bool Evaluate();

	UFUNCTION(BlueprintPure)
	UContentConductor* GetContentConductor() const { return Conductor; }

#if !UE_BUILD_SHIPPING
	virtual FString GetDebugText() const { return GetClass()->GetName(); }
#endif

protected:
	// 評価開始イベント
	UFUNCTION(BlueprintNativeEvent)
	void OnEvaluationBegin();

	// 評価終了イベント
	UFUNCTION(BlueprintNativeEvent)
	void OnEvaluationEnd();

	// 評価開始からの経過秒数を返す
	UFUNCTION(BlueprintPure)
	float GetElapsedTime() const;

private:
	UPROPERTY()
	TObjectPtr<UContentConductor> Conductor;

	float PhaseEnterWorldTime = 0.0f;
	bool bActive			  = false;
};

/**
 * 時間経過 フェーズ開始からDelay秒が経過したらtrue
 */
UCLASS(meta = (DisplayName = "時間経過"))
class CONDUCTORTEST_API UConductorCondition_Elapsed : public UConductorCondition
{
	GENERATED_BODY()

public:
	bool Evaluate_Implementation() override;
#if !UE_BUILD_SHIPPING
	FString GetDebugText() const override;
#endif

	UPROPERTY(EditAnywhere, meta = (ClampMin = "0.0"), meta = (DisplayName = "時間"))
	float Delay = 3.0f;
};
