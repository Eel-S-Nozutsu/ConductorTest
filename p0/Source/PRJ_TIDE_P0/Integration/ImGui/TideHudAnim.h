// Copyright (c) 2026, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.
#pragma once

#if !UE_BUILD_SHIPPING

#include "CoreMinimal.h"
#include "Misc/App.h"

// ImGui 暫定 HUD 共通の「表示 in/out アニメ」と非表示条件。元は敵ゲージの表示アニメで、プレイヤー HP／神技ゲージからも
// 同じ curve で動かすために切り出した（速さ・イージングを変えるときはここだけ触る）。
// 使い方：呼び出し側が表示率 [0,1] を 1 つ持ち、毎フレーム UpdateShowAlpha() を通して 0 に落ち切るまで描画を続ける
namespace TideHudAnim
{
	inline constexpr float ShowInSpeed = 5.0f;	// in の速さ
	inline constexpr float ShowOutSpeed = 8.0f;	// out の速さ
	inline constexpr float SplitPoint = 0.5f;	// 幅フェーズと太さフェーズの境目

	// smoothstep（0..1）
	inline float Smooth01( float T ) { T = FMath::Clamp( T, 0.0f, 1.0f ); return T * T * ( 3.0f - 2.0f * T ); }

	// 表示率を bWantVisible へ向けて補間して返す。DeltaTime は World ではなく FApp（実時間）を使う——ヒットストップや
	// 神技のワールドスロー中に HUD の出入りまで一緒に間延びしないようにするため
	inline float UpdateShowAlpha( float& InOutShowAlpha, bool bWantVisible )
	{
		const float DeltaTime = (float)FApp::GetDeltaTime();
		const float Target = bWantVisible ? 1.0f : 0.0f;
		const float Speed = bWantVisible ? ShowInSpeed : ShowOutSpeed;
		InOutShowAlpha = FMath::FInterpTo( InOutShowAlpha, Target, DeltaTime, Speed );
		if ( FMath::Abs( InOutShowAlpha - Target ) < 0.01f ) InOutShowAlpha = Target;
		return InOutShowAlpha;
	}

	// バー系ゲージの 2 フェーズ。前半＝細い線が伸びて全幅になる／後半＝全幅のまま細→太に育つ。
	// out は表示率が 1→0 に戻るので自動で逆再生になる。
	inline float WidthPhase( float ShowAlpha ) { return Smooth01( ShowAlpha / SplitPoint ); }
	inline float ThickPhase( float ShowAlpha ) { return Smooth01( ( ShowAlpha - SplitPoint ) / ( 1.0f - SplitPoint ) ); }

	// HUD を隠したいか（全 HUD 共通の非表示条件）。カットシーン中（敵 AI の停止条件と同じ信号）と DebugFlags の
	// 「UI非表示」で true。呼び出し側はこれを bWantVisible の否定条件に入れる（＝即消しではなく out アニメで消える）
	bool IsHudSuppressed( const UWorld* World );
}

#endif // !UE_BUILD_SHIPPING
