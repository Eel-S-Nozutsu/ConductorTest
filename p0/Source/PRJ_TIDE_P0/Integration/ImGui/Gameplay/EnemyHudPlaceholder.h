// Copyright (c) 2026, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.
#pragma once

#if !UE_BUILD_SHIPPING

#include "CoreMinimal.h"

/**
 * 敵 HUD の暫定（プレースホルダ）表示。
 * 正式 UI が未実装の間、ImGui で敵の頭上に HP ゲージ（追従ゲージ付き）を描画しておくためのもの。
 * PlayerHudPlaceholder の敵版で、こちらはワールド座標を画面に投影して敵に追従する。
 *
 * 【移行手順】正式 UI を実装したら、
 *   1. このファイル（.h / .cpp）を削除する
 *   2. AEnemyCharacter::Tick() からの Draw() 呼び出し 1 行を外す
 * これだけでプレースホルダ HUD の撤去が完了する。
 */
namespace EnemyHudPlaceholder
{
	// 敵 1 体分の頭上 HP ゲージを描画する。WorldAnchorLocation（敵の頭）を画面へ投影し、そこから ScreenOffsetY ぶん
	// 画面ピクセルで上へずらす（ワールド基準ではないので距離が変わってもゲージが上下にズレない）。
	// InOutTrailingHP（追従ゲージの値。初期値は負）と InOutShowAlpha（表示アニメ [0,1]。初期値 0）は呼び出し側が所有し、
	// ShowAlpha が 0 でない間は呼び続けること（out アニメの再生に必要）。bGrayedOut で戦闘外の灰色表示にする
	void Draw( const UWorld* World, const FVector& WorldAnchorLocation, float ScreenOffsetY, float CurrentHP, float MaxHP, float& InOutTrailingHP, bool bShowHpText, bool bWantVisible, float& InOutShowAlpha, bool bGrayedOut = false );

	// ボス用 HP ゲージ。仕様は通常ゲージと同じだが画面上部・横中央に固定で、ゲージの上に名前（UTF-8。空なら無し）を
	// 表示し横幅も広い。その他の引数は Draw と同じ（画面固定なので WorldAnchorLocation/ScreenOffsetY は不要）
	void DrawBoss( const UWorld* World, const char* BossName, float CurrentHP, float MaxHP, float& InOutTrailingHP, bool bShowHpText, bool bWantVisible, float& InOutShowAlpha );
}

#endif // !UE_BUILD_SHIPPING
