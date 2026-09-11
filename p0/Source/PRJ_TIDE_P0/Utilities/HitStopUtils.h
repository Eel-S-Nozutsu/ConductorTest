// Copyright (c) 2025, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

namespace HitStopUtil
{
	/**
	 * アクターへヒットストップを適用する。
	 *
	 * - 同一アクターへの多重適用は無視される(既存のヒットストップが優先)
	 * - タイマーはWorldTime計測(WorldTimeDilationの影響を受ける)
	 *
	 * @param Actor対象アクター(nullptr / 破棄済みは無視)
	 * @param DurationSec持続秒数
	 * @param Dilation適用するCustomTimeDilation値(0.0f〜1.0fにクランプされる)
	 */
	void ApplyHitStop(AActor* Actor, float DurationSec, float Dilation);

	/**
	 * 指定アクターのヒットストップを即時解除する。
	 *
	 * @param Actor対象アクター
	 */
	void CancelHitStop(AActor* Actor);

}
