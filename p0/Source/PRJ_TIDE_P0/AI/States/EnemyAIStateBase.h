// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "EnemyAIStateBase.generated.h"

class UEnemyBrainComponent;

/**
 * 敵AIの各状態の基底。BTのタスク/サブツリー1本に相当する。
 *
 * UEnemyBrainComponentが起動時に状態ごと1インスタンスだけ生成して使い回す。
 * 状態固有のデータ(移動先・EQSリクエスト・実行中の攻撃など)は各派生クラスが持ち、
 * BrainComponent側には持ち込まない。UTideAttackExecutionと同じ流儀。
 *
 * 状態は「自分が選ばれる条件を無効化する」ことで終わる(例: Attackが完了したら
 * PendingAttackIndexを -1に戻す)。個別の完了通知は持たない。
 */
UCLASS(Abstract)
class PRJ_TIDE_P0_API UEnemyAIStateBase : public UObject
{
	GENERATED_BODY()

public:

	virtual void Enter(UEnemyBrainComponent& Brain) {}
	virtual void Tick(UEnemyBrainComponent& Brain, float DeltaSeconds) {}
	virtual void Exit(UEnemyBrainComponent& Brain) {}

	// まだ中断されたくない仕事が進行中か (攻撃モンタージュ再生中など)
	// falseを返す状態は割り込み規則に関係なく遷移してよい
	// 「終わったのに割り込み規則が遷移を拒み続けて固まる」のを防ぐための唯一の出口
	virtual bool IsBusy() const { return false; }

	// ImGuiデバッグ表示用の補足文字列 (移動先・残り待機時間など)
	virtual FString GetDebugText() const { return FString(); }

};
