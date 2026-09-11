// Copyright (c) 2026, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.
#include "PlayerHudPlaceholder.h"

#if !UE_BUILD_SHIPPING

#include "imgui.h"

#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"

#include "PRJ_TIDE_P0/Integration/ImGui/TideHudColors.h"
#include "PRJ_TIDE_P0/Integration/ImGui/TideHudAnim.h"
#include "PRJ_TIDE_P0/Core/TideGameSettings.h"

namespace PlayerHudPlaceholder
{
	namespace
	{
		// 被弾なら遅れて減り、回復なら遅れて増える残像バー。-1 は未初期化
		float GTrailingHP = -1.0f;

		// 表示 in/out アニメの進行 [0,1]。プレイヤー HUD は 1 つしか無いのでここで持つ
		float GShowAlpha = 0.0f;
	}

	void Draw( const UWorld* World, int32 CurrentLevel, float CurrentHP, float MaxHP, bool bWantVisible )
	{
		const ImGuiViewport* Viewport = ImGui::GetMainViewport();
		if ( !Viewport ) return;

		// in/out アニメ（敵ゲージと同じ TideHudAnim の curve）。
		// 前半＝細い線が左端から右へ伸びて全幅になる、後半＝全幅のまま細→太に育つ
		TideHudAnim::UpdateShowAlpha( GShowAlpha, bWantVisible );
		if ( GShowAlpha <= 0.0f ) return;

		const float WidthPhase = TideHudAnim::WidthPhase( GShowAlpha );
		const float ThickPhase = TideHudAnim::ThickPhase( GShowAlpha );

		FVector2D GameViewportSize = FVector2D( 1920.0, 1080.0 );	// 取得失敗時のフォールバック
		if ( GEngine && GEngine->GameViewport )
		{
			GEngine->GameViewport->GetViewportSize( GameViewportSize );
		}

		constexpr float BaseResolutionY = 1080.0f;
		const float ScaleFactor = FMath::Max( GameViewportSize.Y / BaseResolutionY, 0.1f );

		constexpr float BasePaddingX = 80.0f;		// 画面左上からの余白（ウィンドウの描画開始位置）
		constexpr float BasePaddingY = 45.0f;

		constexpr float BaseGaugeWidth = 350.0f;
		constexpr float BaseGaugeHeight = 16.0f;
		constexpr float BaseGaugeRounding = 3.0f;	// 0 で直角
		constexpr float BaseTextScale = 1.6f;		// 1.0 が標準サイズ

		constexpr float BaseTipOverhang = 60.0f;	// 先端の縦線がゲージの上に突き出す長さ
		constexpr float BaseTipThickness = 4.0f;
		constexpr float TipGapThreshold = 0.02f;	// 先端線を表示する追従ゲージとの差のしきい値
		constexpr float HealSnapRatio = 0.5f;		// 一度にこれ以上増えたら蘇生扱いで演出せず即反映

		constexpr float BaseWindowPadding = 18.0f;	// 枠線と内部コンテンツの間の余白
		constexpr float BaseItemSpacingX = 16.0f;	// 同一行のアイテム間（Lv表示とHP数値の間など）
		constexpr float BaseItemSpacingY = 8.0f;	// 改行されたアイテム間（テキストとゲージの間など）

		const float ScaledPaddingX = BasePaddingX * ScaleFactor;
		const float ScaledPaddingY = BasePaddingY * ScaleFactor;
		const float ScaledGaugeWidth = BaseGaugeWidth * ScaleFactor;
		const float ScaledGaugeHeight = BaseGaugeHeight * ScaleFactor;
		const float ScaledGaugeRounding = BaseGaugeRounding * ScaleFactor;
		const float ScaledTextScale = BaseTextScale * ScaleFactor;

		const float ScaledWindowPadding = BaseWindowPadding * ScaleFactor;
		const float ScaledItemSpacingX = BaseItemSpacingX * ScaleFactor;
		const float ScaledItemSpacingY = BaseItemSpacingY * ScaleFactor;
		const float ScaledTipOverhang = BaseTipOverhang * ScaleFactor;
		const float ScaledTipThickness = BaseTipThickness * ScaleFactor;

		// アニメーション速度は解像度に依存しないため固定
		constexpr float TrailingInterpSpeed = 1.0f;	// 被弾：残像が遅れて減る速さ
		constexpr float HealInterpSpeed = 2.0f;		// 回復：帯が現在HPへ追いつく速さ

		// モチーフカラー切替（トグルは DebugFlags ウィンドウの「UI」→ HUDモチーフカラー）
		const UTideGameSettings* Settings = UTideGameSettings::Get();
		const bool bMotif = Settings && Settings->bUseTideMotifHudColors;

		// DrawList の直描きには ImGuiStyleVar_Alpha が効かないので、表示率は各色のアルファへ明示的に掛ける
		// （ウィンドウ背景とテキストは StyleVar 側で効く）
		auto ShowCol = []( const ImVec4& C ) { return TideHudColors::ToU32( TideHudColors::WithAlpha( C, C.w * GShowAlpha ) ); };

		const ImVec4 BgColor      = bMotif ? TideHudColors::Player::WindowBgMotif : TideHudColors::Player::WindowBg;
		const ImU32 ColorGaugeBg  = ShowCol( bMotif ? TideHudColors::Player::GaugeBgMotif : TideHudColors::Common::GaugeBg );
		const ImU32 ColorTrailing = ShowCol( bMotif ? TideHudColors::Player::TrailingMotif : TideHudColors::Common::Trailing );
		const ImVec4 HpColorHigh  = bMotif ? TideHudColors::Player::HpHighMotif : TideHudColors::Player::HpHigh;
		const ImVec4 HpColorLow   = bMotif ? TideHudColors::Player::HpLowMotif : TideHudColors::Player::HpLow;
		const ImVec4 HealColor    = bMotif ? TideHudColors::Player::HealMotif : TideHudColors::Player::Heal;
		const ImU32 ColorHeal     = ShowCol( HealColor );
		const ImU32 ColorHealTip  = ShowCol( TideHudColors::WithAlpha( HealColor, 0.5f ) );	// 回復縦ブロック
		const ImVec4 TextColor    = bMotif ? TideHudColors::Player::TextMotif : TideHudColors::Common::Text;
		const ImU32 ColorBorder   = ShowCol( TideHudColors::Player::BorderMotif );	// モチーフ時のみ描画

		// 蘇生・リスタートの一括全回復まで演出すると全幅が染まるので、大きすぎる増加は即反映する
		if ( GTrailingHP < 0.0f || CurrentHP - GTrailingHP > MaxHP * HealSnapRatio )
		{
			GTrailingHP = CurrentHP;
		}
		else if ( !FMath::IsNearlyEqual( GTrailingHP, CurrentHP ) )
		{
			const float DeltaTime = World ? World->GetDeltaSeconds() : 0.016f;
			const float InterpSpeed = ( GTrailingHP > CurrentHP ) ? TrailingInterpSpeed : HealInterpSpeed;
			GTrailingHP = FMath::FInterpTo( GTrailingHP, CurrentHP, DeltaTime, InterpSpeed );
		}

		const ImVec2 WindowPos = ImVec2( Viewport->WorkPos.x + ScaledPaddingX, Viewport->WorkPos.y + ScaledPaddingY );
		ImGui::SetNextWindowPos( WindowPos, ImGuiCond_Always );

		const ImGuiWindowFlags WindowFlags =
			ImGuiWindowFlags_NoDecoration |
			ImGuiWindowFlags_AlwaysAutoResize |
			ImGuiWindowFlags_NoSavedSettings |
			ImGuiWindowFlags_NoFocusOnAppearing |
			ImGuiWindowFlags_NoNav |
			ImGuiWindowFlags_NoMove;

		ImGui::PushStyleColor( ImGuiCol_WindowBg, BgColor );
		ImGui::PushStyleVar( ImGuiStyleVar_WindowPadding, ImVec2( ScaledWindowPadding, ScaledWindowPadding ) );
		ImGui::PushStyleVar( ImGuiStyleVar_ItemSpacing, ImVec2( ScaledItemSpacingX, ScaledItemSpacingY ) );

		// テキストは後半のみ出して先にバーを見せる
		ImGui::PushStyleVar( ImGuiStyleVar_Alpha, ThickPhase );

		if ( ImGui::Begin( "PlayerHUD_Overlay", nullptr, WindowFlags ) )
		{
			ImGui::SetWindowFontScale( ScaledTextScale );

			const float HpRatio = FMath::Clamp( CurrentHP / MaxHP, 0.0f, 1.0f );
			const float TrailingRatio = FMath::Clamp( GTrailingHP / MaxHP, 0.0f, 1.0f );

			const float ColorRatio = FMath::Clamp( HpRatio * 2.0f, 0.0f, 1.0f );
			const ImVec4 CurrentHpImColor = ImVec4(
				FMath::Lerp( HpColorLow.x, HpColorHigh.x, ColorRatio ),
				FMath::Lerp( HpColorLow.y, HpColorHigh.y, ColorRatio ),
				FMath::Lerp( HpColorLow.z, HpColorHigh.z, ColorRatio ),
				1.0f
			);
			const ImU32 ColorCurrentHp = ShowCol( CurrentHpImColor );
			const ImU32 ColorTipLine = ShowCol( TideHudColors::WithAlpha( CurrentHpImColor, 0.5f ) );	// 被弾縦ブロック

			// 先端の強調縦線をテキストの背面に描くため 2 チャンネルに分割する
			// （背面(0)にゲージ＋先端線、前面(1)にテキストを描いて Merge）
			ImDrawList* DrawList = ImGui::GetWindowDrawList();
			DrawList->ChannelsSplit( 2 );

			DrawList->ChannelsSetCurrent( 1 );
			ImGui::TextColored( TextColor, "Lv. %d", CurrentLevel );

			char HpOverlayText[32];
			snprintf( HpOverlayText, sizeof( HpOverlayText ), "%.0f / %.0f", CurrentHP, MaxHP );
			const float TextWidth = ImGui::CalcTextSize( HpOverlayText ).x;

			ImGui::SameLine( ScaledGaugeWidth - TextWidth );
			ImGui::TextColored( TextColor, "%s", HpOverlayText );

			DrawList->ChannelsSetCurrent( 0 );
			const ImVec2 P = ImGui::GetCursorScreenPos();

			// レイアウト（下の Dummy）は通常寸法のまま確保し、バーだけをその枠内で縦中央に置く
			// ＝アニメ中もウィンドウサイズが動かない
			constexpr float BaseThinHeight = 3.0f;	// 細い線のときの縦幅（敵ゲージと同値）
			const float AnimGaugeWidth = ScaledGaugeWidth * WidthPhase;
			const float AnimGaugeHeight = FMath::Lerp( BaseThinHeight * ScaleFactor, ScaledGaugeHeight, ThickPhase );
			const float AnimRounding = FMath::Min( ScaledGaugeRounding, AnimGaugeHeight * 0.5f );
			const float BarTopY = P.y + ( ScaledGaugeHeight - AnimGaugeHeight ) * 0.5f;

			const ImVec2 BarMin = ImVec2( P.x, BarTopY );
			const ImVec2 BarMax = ImVec2( P.x + AnimGaugeWidth, BarTopY + AnimGaugeHeight );
			DrawList->AddRectFilled( BarMin, BarMax, ColorGaugeBg, AnimRounding );

			// 広いほう→狭いほうの順に塗ると、差の帯だけが上塗りされずに残る。
			// 被弾（追従が上）なら残像色、回復（現在HPが上）なら回復色がその帯になる
			const bool bHealing = HpRatio > TrailingRatio;
			const float HighRatio = FMath::Max( HpRatio, TrailingRatio );
			const float LowRatio = FMath::Min( HpRatio, TrailingRatio );

			if ( HighRatio > 0.0f )
			{
				const ImVec2 HighMax = ImVec2( P.x + AnimGaugeWidth * HighRatio, BarMax.y );
				DrawList->AddRectFilled( BarMin, HighMax, bHealing ? ColorHeal : ColorTrailing, AnimRounding );
			}

			if ( LowRatio > 0.0f )
			{
				const ImVec2 LowMax = ImVec2( P.x + AnimGaugeWidth * LowRatio, BarMax.y );
				DrawList->AddRectFilled( BarMin, LowMax, ColorCurrentHp, AnimRounding );
			}

			// モチーフ時はゲージ全体に輪郭線を足して締める
			if ( bMotif )
			{
				const float ScaledBorderThickness = FMath::Max( 1.0f, 2.0f * ScaleFactor );
				DrawList->AddRect( BarMin, BarMax, ColorBorder, AnimRounding, 0, ScaledBorderThickness );
			}

			// 追従ゲージと現在HPの差を、その帯の幅ぶんの縦ブロックで強調する。ゲージ／輪郭に被せないよう
			// ブロックはゲージ上端で止め、in/out アニメ中（細い線の段階）は出さない
			if ( ThickPhase > 0.0f && HighRatio - LowRatio > TipGapThreshold )
			{
				const float LeftX = BarMin.x + AnimGaugeWidth * LowRatio;
				float RightX = BarMin.x + AnimGaugeWidth * HighRatio;
				if ( RightX - LeftX < ScaledTipThickness ) RightX = LeftX + ScaledTipThickness;	// 最低の線幅は確保

				const ImVec2 TipMin = ImVec2( LeftX, BarMin.y - ScaledTipOverhang * ThickPhase );
				const ImVec2 TipMax = ImVec2( RightX, BarMin.y );

				// ウィンドウ枠で上側の描画が切り取られないようクリッピングを上書きする
				DrawList->PushClipRect( ImVec2( TipMin.x, TipMin.y ), ImVec2( TipMax.x, TipMax.y ), false );
				DrawList->AddRectFilled( TipMin, TipMax, bHealing ? ColorHealTip : ColorTipLine );
				DrawList->PopClipRect();
			}

			DrawList->ChannelsMerge();	// 背面→前面の順に確定描画

			ImGui::Dummy( ImVec2( ScaledGaugeWidth, ScaledGaugeHeight ) );
			ImGui::SetWindowFontScale( 1.0f );
		}
		ImGui::End();

		ImGui::PopStyleVar( 3 );
		ImGui::PopStyleColor();
	}
}

#endif // !UE_BUILD_SHIPPING
