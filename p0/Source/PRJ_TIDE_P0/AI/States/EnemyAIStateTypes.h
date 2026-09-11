// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EnemyAIStateTypes.generated.h"

/**
 * 敵AIの状態。UEnemyBrainComponentが毎ティック優先度順に評価して1つを選ぶ。
 * 値の並びは優先度ではない(優先度はSelectDesiredStateのif連鎖が唯一の定義)。
 */
UENUM()
enum class EEnemyAIState : uint8
{
	// 未Possess・死亡後。何も動かさない
	None,

	Dead,

	// 被弾リアクション中。BrainComponent::StopLogicの置き換え
	React,

	// カットシーン中の一時停止。PCのシネマモードを見て最上位優先で入る
	// Reactと同じく「行動できない」を表す状態
	Suspended,

	Attack,

	// パトロール圏外からの原点復帰
	ReturnToOrigin,

	// ターゲットなし: ザコはランダム徘徊、ボスはその場待機
	Patrol,
	Idle,

	// 近すぎる (KeepDistance有効時のみ)
	Retreat,

	// 交戦距離内: ザコはEQSで回り込み、ボスはその場待機
	Strafe,
	Hold,

	Approach,
};
