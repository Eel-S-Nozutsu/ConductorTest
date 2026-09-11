// Copyright (c) 2026, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.
#pragma once

#if !UE_BUILD_SHIPPING

#include "CoreMinimal.h"

/**
 * プレイヤー HUD の暫定（プレースホルダ）表示。
 * 正式 UI が未実装の間、ImGui で HP ゲージ等を画面に描画しておくためのもの。
 *
 * 【移行手順】正式 UI を実装したら、
 *   1. このファイル（.h / .cpp）を削除する
 *   2. TidePlayerCharacter::UpdateUI() からの Draw() 呼び出し 1 行を外す
 * これだけでプレースホルダ HUD の撤去が完了する。
 */
namespace PlayerHudPlaceholder
{
	// 画面左上に暫定 HUD（Lv / HP ゲージ）を描画する（World は追従ゲージ・表示アニメの補間に使う DeltaSeconds 取得用）。
	// bWantVisible が false のときは out アニメを再生しながら消えるので、消え切るまで毎フレーム呼び続けること
	void Draw( const UWorld* World, int32 CurrentLevel, float CurrentHP, float MaxHP, bool bWantVisible );
}

#endif // !UE_BUILD_SHIPPING
