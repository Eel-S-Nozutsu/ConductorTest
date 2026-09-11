// Copyright (c) 2026, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.

#pragma once

#if !UE_BUILD_SHIPPING

#include "CoreMinimal.h"

/**
 * デバッグ用ダメージ数値オーバーレイ（ImGui foreground draw list）。
 *
 * デバッグフラグ（UTideGameSettings::bDebugShowDamageNumbers・既定 OFF）が ON のときだけ、
 * ダメージ確定時（UDamageSystemComponent::ProcessDamage）に数値ポップを積み、毎フレーム画面へ描画する。
 * UDamageSystemComponent を持たない被弾側（壊れ物 ABreakableProp など）は各自 Push を呼ぶ。
 *
 * 文字は常に白。輪郭色で種別を区別する：
 *   - 30 以上のダメージ … 輪郭 黄（最優先）
 *   - プレイヤー被弾     … 輪郭 赤
 *   - 敵など被弾         … 輪郭 黒
 *
 * アニメ：出現時に大きくなってから通常サイズへ戻る（in）、その後は画面固定（張り付き）、
 * 消滅時はフェードしながら上へ移動して消える（out）。
 */
class FDamageNumberDebug
{
public:
	// ダメージ確定時に呼ぶ（フラグ OFF 時・ダメージ 0 以下は何もしない）。WorldLocation は数値を出す位置で、
	// 出現時に一度だけスクリーン投影して以後固定する。bVictimIsPlayer は輪郭色の決定に使う
	static void Push( const FVector& WorldLocation, float Damage, bool bVictimIsPlayer );

	// 毎フレーム、ポップのアニメ更新とオーバーレイ描画を行う（ImGuiDebugSubsystem::Tick から呼ぶ）。
	static void Render( class UWorld* World, float DeltaTime );

	// 全ポップを破棄（PIE 終了時など）。
	static void Clear();
};

#endif // !UE_BUILD_SHIPPING
