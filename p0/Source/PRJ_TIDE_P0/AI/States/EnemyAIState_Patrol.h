// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PRJ_TIDE_P0/AI/States/EnemyAIStateBase.h"
#include "EnemyAIState_Patrol.generated.h"

/**
 * ターゲットを持たないザコの徘徊。
 *
 * 2モードある。どちらを使うかはAEnemyCharacterに注入されたFEnemyWaitSettingsが決める。
 *   RandomPatrol … 自身の周囲をランダムに歩く(テリトリー外へは出ない)
 *   RouteMove    … 配置されたAEnemyRoutePathのポイントを順に巡る
 *
 * ルート移動の進行状態はこのクラスが持つ。状態オブジェクトはbrainの生存期間中ずっと
 * 生きているので、戦闘でPatrolを抜けて戻ってきても進行が保たれる。
 *
 * 到達判定は「MoveToが終わったか」ただ1つ。移動を出す側が到着も判断するので、
 * 停止位置と到達判定がズレて固まることが構造的に起きない。
 */
UCLASS()
class PRJ_TIDE_P0_API UEnemyAIState_Patrol : public UEnemyAIStateBase
{
	GENERATED_BODY()

public:

	virtual void Enter(UEnemyBrainComponent& Brain) override;
	virtual void Tick(UEnemyBrainComponent& Brain, float DeltaSeconds) override;
	virtual void Exit(UEnemyBrainComponent& Brain) override;

	virtual FString GetDebugText() const override;

private:

	bool IsRouteMode(const UEnemyBrainComponent& Brain) const;

	// 次の目的地を決めて移動を開始する。決められなければfalse ※待機して再挑戦
	bool PickNextDestination(UEnemyBrainComponent& Brain);

	// --- ランダム徘徊 ---

	bool PickRandomDestination(UEnemyBrainComponent& Brain);

	// --- ルート移動 ---

	// 次のポイントへの移動を開始する。待機中・終着済みならfalse
	bool AdvanceRoute(UEnemyBrainComponent& Brain);

	// 移動完了時に呼ぶ。到達ポイントを確定して待機へ入る
	void OnReachedRoutePoint(UEnemyBrainComponent& Brain);

	// ルートポイントの位置をNavへ投影して返す
	bool ResolveRoutePointLocation(const UEnemyBrainComponent& Brain, int32 PointIndex,
		FVector& OutLocation) const;

	int32 ComputeNextRoutePointIndex(const UEnemyBrainComponent& Brain, int32 PointCount);

	// 終着型で終点に着いた後の待機モンタージュ
	void SetTerminalIdleActive(UEnemyBrainComponent& Brain, bool bActive);

#if !UE_BUILD_SHIPPING
	void DrawRouteDebug(const UEnemyBrainComponent& Brain) const;
#endif

	// ランダム徘徊の到着後の待機時間。ルート移動はWaitAtRoutePointTimeを使う
	static constexpr float WaitDuration = 2.0f;

	// 目的地を決められなかったときの再挑戦間隔。ナビが張られていない場所で
	// 毎ティック探索を叩き続けないようにする
	static constexpr float RetryInterval = 1.0f;

	// 終着待機中のポーリング間隔
	static constexpr float RoutePollInterval = 0.5f;

	// 1回の徘徊で移動する距離の上限。常に自身の周囲から選ぶ ※テリトリー全体からではない
	static constexpr float WanderRadius = 500.0f;

	// テリトリー外を引いたときの再抽選回数。境界際だと外側ばかり出るため上限を設ける
	static constexpr int32 MaxWanderAttempts = 4;

	float WaitRemaining = 0.0f;
	bool bWaiting = false;

	// ルート移動の進行状態
	int32 RouteCurrentPointIndex = INDEX_NONE;
	int32 RouteTargetPointIndex = INDEX_NONE;
	int32 RouteDirection = 1;
	bool bRouteInitialized = false;
	bool bRouteTerminalReached = false;
	bool bTerminalIdleActive = false;

};
