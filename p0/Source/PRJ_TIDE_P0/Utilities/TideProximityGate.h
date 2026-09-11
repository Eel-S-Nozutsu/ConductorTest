// Copyright (c) 2026, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class APawn;

/**
 * 遠距離ギミックの作動ゲート（プレイヤー距離カリング）の共通ヘルパー。
 *
 * レベル上に多数配置されるギミック（ハザード・風・アリジゴク等）は、毎フレームの
 * オーバーラップ走査・トレース・全 Pawn 走査といった重い処理を持つ。プレイヤーから
 * 十分離れているギミックはそれらを走らせる必要がないため、各アクターの Tick 冒頭で
 * IsPlayerWithinDistance を呼び、範囲外なら重い処理をスキップする運用にする。
 *
 * プレイヤー参照は各アクターが保持する TWeakObjectPtr<APawn> にキャッシュし、
 * 無効になったときだけ取り直す。
 */
namespace TideProximityGate
{
	// WorldContext からローカルプレイヤー（index 0）のポーンを解決する。
	// CachedPlayer が有効ならそれを返し、無効なときだけ取り直してキャッシュする。
	APawn* ResolvePlayerPawn( const UObject* WorldContext, TWeakObjectPtr<APawn>& CachedPlayer );

	// プレイヤーが Center から ActivationDistance 以内にいれば true（＝ギミックを作動させる）。プレイヤーを取得できない
	// 場合は bActivateWhenNoPlayer、距離が 0 以下ならゲート無効で常に true。距離は 3D で、平方根を避けて二乗で比較する
	bool IsPlayerWithinDistance( const UObject* WorldContext, const FVector& Center,
		float ActivationDistance, TWeakObjectPtr<APawn>& CachedPlayer, bool bActivateWhenNoPlayer = true );

#if !UE_BUILD_SHIPPING
	// 作動範囲（Center 中心・半径 ActivationDistance の球）をデバッグ描画する。bWithinRange で色分けし作動中は緑・
	// スキップ中は赤（ゲート無効なら描画しない）
	void DrawActivationRange( const UObject* WorldContext, const FVector& Center,
		float ActivationDistance, bool bWithinRange );
#endif
}
