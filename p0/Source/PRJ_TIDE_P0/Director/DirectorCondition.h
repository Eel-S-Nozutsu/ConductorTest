// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PRJ_TIDE_P0/Director/DirectorObjectBase.h"
#include "DirectorCondition.generated.h"

class UContentDirector;

/**
 * フェーズ遷移の条件。真偽を返すだけで、副作用は持たない。
 *
 * 評価される期間はフェーズにいる間だけ。
 * コリジョン接触のようなイベント条件はOnEvaluationBeginで購読してフラグを立て、Evaluateでそれを返す。
 */
UCLASS(Abstract, Blueprintable, BlueprintType)
class PRJ_TIDE_P0_API UDirectorCondition : public UDirectorObjectBase
{
	GENERATED_BODY()

public:

	// UContentDirectorから呼ぶ
	void BeginEvaluation(UContentDirector* InDirector);
	void EndEvaluation();

	bool IsEvaluating() const { return bEvaluating; }

	// 評価。評価期間中だけUContentDirectorから呼ばれる
	UFUNCTION(BlueprintNativeEvent, Category = "Tide|Director")
	bool Evaluate();

	UFUNCTION(BlueprintPure, Category = "Tide|Director")
	UContentDirector* GetContentDirector() const { return Director; }

	// デバッグ表示用の一行説明
	virtual FString GetDebugText() const { return GetClass()->GetName(); }

protected:

	// イベントの購読はここで行う
	UFUNCTION(BlueprintNativeEvent, Category = "Tide|Director")
	void OnEvaluationBegin();

	UFUNCTION(BlueprintNativeEvent, Category = "Tide|Director")
	void OnEvaluationEnd();

	// 評価が始まってからの経過秒数
	UFUNCTION(BlueprintPure, Category = "Tide|Director")
	float GetElapsedTime() const;

private:

	UPROPERTY()
	TObjectPtr<UContentDirector> Director;

	float BeginWorldTime = 0.0f;
	bool  bEvaluating    = false;

};

/**
 * 評価開始からDelay秒が経過したらtrue
 */
UCLASS(meta = (DisplayName = "時間経過"))
class PRJ_TIDE_P0_API UDirectorCondition_Elapsed : public UDirectorCondition
{
	GENERATED_BODY()

public:

	virtual bool Evaluate_Implementation() override;
	virtual FString GetDebugText() const override;

	UPROPERTY(EditAnywhere, Category = "Director", meta = (ClampMin = "0.0"))
	float Delay = 3.0f;

};

/**
 * 指定シグナル(UTideSignalSubsystem)が発火していたらtrue
 *
 * 発火はラッチ保持されるので、評価が始まる前に撃たれたシグナルでも拾える。
 * 同じ名前を再利用するフェーズでは評価開始時にリセットする(bResetSignalOnBegin)。
 */
UCLASS(meta = (DisplayName = "シグナル発火"))
class PRJ_TIDE_P0_API UDirectorCondition_Signal : public UDirectorCondition
{
	GENERATED_BODY()

public:

	virtual bool Evaluate_Implementation() override;
	virtual FString GetDebugText() const override;

	UPROPERTY(EditAnywhere, Category = "Director")
	FName SignalName;

	UPROPERTY(EditAnywhere, Category = "Director")
	bool bResetSignalOnBegin = true;

protected:

	virtual void OnEvaluationBegin_Implementation() override;

};

/**
 * グループ(DT_Actors_*のGroupId)の生存数がAliveThreshold以下になったらtrue
 *
 * 生成前は0体なので、一度でも生存を見るまでは判定しない
 * (評価開始直後に「全滅済み」と誤判定して素通りするのを防ぐ)。
 */
UCLASS(meta = (DisplayName = "グループ撃破"))
class PRJ_TIDE_P0_API UDirectorCondition_GroupDefeated : public UDirectorCondition
{
	GENERATED_BODY()

public:

	virtual bool Evaluate_Implementation() override;
	virtual FString GetDebugText() const override;

	// DT_Actors_* のGroupId
	UPROPERTY(EditAnywhere, Category = "Director")
	FName GroupId;

	UPROPERTY(EditAnywhere, Category = "Director", meta = (ClampMin = "0"))
	int32 AliveThreshold = 0;

protected:

	virtual void OnEvaluationBegin_Implementation() override;

private:

	bool bSeenAlive = false;

};
