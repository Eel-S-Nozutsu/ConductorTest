// Copyright (c) 2026, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.
#include "EnemyHudPlaceholder.h"

#if !UE_BUILD_SHIPPING

#include "imgui.h"

#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

#include "PRJ_TIDE_P0/Integration/ImGui/TideHudColors.h"
#include "PRJ_TIDE_P0/Integration/ImGui/TideHudAnim.h"
#include "PRJ_TIDE_P0/Core/TideGameSettings.h"

namespace EnemyHudPlaceholder
{
	namespace
	{
		// 基準 1080p の解像度スケール。OutViewportSize にビューポート実サイズも返す
		float GetUIScaleFactor( FVector2D& OutViewportSize )
		{
			OutViewportSize = FVector2D( 1920.0, 1080.0 );	// 取得失敗時のフォールバック
			if ( GEngine && GEngine->GameViewport )
			{
				GEngine->GameViewport->GetViewportSize( OutViewportSize );
			}
			constexpr float BaseResolutionY = 1080.0f;
			return FMath::Max( (float)OutViewportSize.Y / BaseResolutionY, 0.1f );
		}

		// 通常ゲージ／ボスゲージ共通の描画。CenterX＝横中心／MidY＝通常太さ時のバー縦中心／
		// FullWidth＝通常太さ時の横幅（いずれも画面座標）、Name＝ゲージ上の名前（空で無し）。
		// in/out アニメ・追従ゲージの状態は引数の参照で呼び出し側が所有する
		void DrawGaugeCommon(
			ImDrawList* DrawList, float ScaleFactor, float DeltaTime,
			float CenterX, float MidY, float FullWidth,
			float CurrentHP, float MaxHP, float& InOutTrailingHP,
			bool bShowHpText, const char* Name, float TipGapThreshold,
			bool bWantVisible, float& InOutShowAlpha, bool bGrayedOut )
		{
			// curve・速さは TideHudAnim に集約（プレイヤー HP／神技ゲージと共通）
			TideHudAnim::UpdateShowAlpha( InOutShowAlpha, bWantVisible );
			if ( InOutShowAlpha <= 0.0f ) return;

			const float WidthPhase = TideHudAnim::WidthPhase( InOutShowAlpha );
			const float ThickPhase = TideHudAnim::ThickPhase( InOutShowAlpha );

			// --- 寸法（横幅は引数で可変。その他は基準値×スケール）---
			constexpr float BaseGaugeHeight = 10.0f;	// 通常太さ時の縦幅
			constexpr float BaseGaugeRounding = 1000.0f;// 最大ピル形状にクランプされる
			constexpr float BaseTextOffsetY = 4.0f;		// ゲージと HP 数値テキストの間隔
			constexpr float BaseNameOffsetY = 6.0f;		// ゲージと名前テキストの間隔
			constexpr float BaseBorderThickness = 3.5f;	// 枠線（左右の丸キャップ）
			constexpr float BaseBorderThicknessV = 2.0f;// 枠線（上下の直線。縦だけ細い）
			constexpr float BaseTipOverhang = 50.0f;	// 先端の縦線がゲージ上に突き出す長さ
			constexpr float BaseTipThickness = 3.0f;
			constexpr float BaseThinHeight = 3.0f;		// 細い線のときの縦幅
			constexpr float NameFontScale = 2.0f;

			const float ScaledGaugeWidth = FullWidth;	// 既にスケール適用済み
			const float ScaledGaugeHeight = BaseGaugeHeight * ScaleFactor;
			const float ScaledGaugeRounding = BaseGaugeRounding * ScaleFactor;
			const float ScaledTextOffsetY = BaseTextOffsetY * ScaleFactor;
			const float ScaledNameOffsetY = BaseNameOffsetY * ScaleFactor;
			const float ScaledBorderThickness = BaseBorderThickness * ScaleFactor;
			const float ScaledBorderThicknessV = BaseBorderThicknessV * ScaleFactor;
			const float ScaledTipOverhang = BaseTipOverhang * ScaleFactor;
			const float ScaledTipThickness = BaseTipThickness * ScaleFactor;
			const float ScaledThinHeight = BaseThinHeight * ScaleFactor;
			const float AnimHeight = FMath::Lerp( ScaledThinHeight, ScaledGaugeHeight, ThickPhase );

			// 遅れて減る残像
			constexpr float TrailingInterpSpeed = 1.0f;
			if ( InOutTrailingHP < 0.0f || InOutTrailingHP < CurrentHP )
			{
				InOutTrailingHP = CurrentHP;
			}
			else if ( InOutTrailingHP > CurrentHP )
			{
				InOutTrailingHP = FMath::FInterpTo( InOutTrailingHP, CurrentHP, DeltaTime, TrailingInterpSpeed );
			}

			const float HpRatio = FMath::Clamp( CurrentHP / MaxHP, 0.0f, 1.0f );
			const float TrailingRatio = FMath::Clamp( InOutTrailingHP / MaxHP, 0.0f, 1.0f );

			// bGrayedOut（帰還中など戦闘外）なら灰色へ差し替えて戦闘対象でないことを示す
			// （モチーフ有無に関わらず灰色）
			const UTideGameSettings* Settings = UTideGameSettings::Get();
			const bool bMotif = Settings && Settings->bUseTideMotifHudColors;

			const ImU32 ColorGaugeBg = TideHudColors::ToU32( bMotif ? TideHudColors::Enemy::GaugeBgMotif : TideHudColors::Common::GaugeBg );
			const ImU32 ColorBorder = TideHudColors::ToU32( bGrayedOut
				? TideHudColors::Enemy::BorderGray
				: ( bMotif ? TideHudColors::Enemy::BorderMotif : TideHudColors::Enemy::Border ) );		// 枠
			const ImU32 ColorTrailing = TideHudColors::ToU32( bMotif ? TideHudColors::Enemy::TrailingMotif : TideHudColors::Common::Trailing );
			const ImU32 ColorCurrentHp = TideHudColors::ToU32( bGrayedOut
				? TideHudColors::Enemy::HpGray
				: ( bMotif ? TideHudColors::Enemy::HpMotif : TideHudColors::Enemy::Hp ) );		// HP ゲージ
			// 被弾縦ブロック：先端線色を半透明で
			const ImU32 ColorTipLine = TideHudColors::ToU32( TideHudColors::WithAlpha( bGrayedOut
				? TideHudColors::Enemy::TipGray
				: ( bMotif ? TideHudColors::Enemy::TipMotif : TideHudColors::Enemy::Tip ), 0.5f ) );
			const ImU32 ColorText = TideHudColors::ToU32( bMotif ? TideHudColors::Enemy::TextMotif : TideHudColors::Common::Text );		// 名前 / HP 数値テキスト

			// 太さアニメは通常時の縦中心を固定して上下対称に育てる
			const float FullMidY = MidY;
			const float FullTop = FullMidY - ScaledGaugeHeight * 0.5f;	// テキスト配置基準
			const float FullBottom = FullMidY + ScaledGaugeHeight * 0.5f;
			const ImVec2 BarMin = ImVec2( CenterX - ScaledGaugeWidth * 0.5f, FullMidY - AnimHeight * 0.5f );
			const ImVec2 BarMax = ImVec2( CenterX + ScaledGaugeWidth * 0.5f, FullMidY + AnimHeight * 0.5f );
			const ImVec2 BorderMin = BarMin;
			const ImVec2 BorderMax = BarMax;

			const float RevealHalfW = ScaledGaugeWidth * 0.5f * WidthPhase;

			// 中心から左右へ開くワイプ。クリップ範囲は名前・先端線・HP テキストも含むよう余裕を持たせる
			const float ScaledNameRoomY = ImGui::GetFontSize() * NameFontScale * ScaleFactor + ScaledNameOffsetY;
			constexpr float ClipMarginTop = 16.0f;
			constexpr float ClipMarginBottom = 28.0f;
			const ImVec2 ClipMin = ImVec2( CenterX - RevealHalfW, FullTop - ScaledTipOverhang - ScaledNameRoomY - ClipMarginTop * ScaleFactor );
			const ImVec2 ClipMax = ImVec2( CenterX + RevealHalfW, FullBottom + ClipMarginBottom * ScaleFactor );
			DrawList->PushClipRect( ClipMin, ClipMax, true );

			// 背景
			DrawList->AddRectFilled( BorderMin, BorderMax, ColorGaugeBg, ScaledGaugeRounding );

			// 追従ゲージ
			if ( TrailingRatio > 0.0f )
			{
				const ImVec2 TrailingMax = ImVec2( BarMin.x + ScaledGaugeWidth * TrailingRatio, BarMax.y );
				DrawList->AddRectFilled( BarMin, TrailingMax, ColorTrailing, ScaledGaugeRounding );
			}

			// 現在 HP
			if ( HpRatio > 0.0f )
			{
				const ImVec2 CurrentMax = ImVec2( BarMin.x + ScaledGaugeWidth * HpRatio, BarMax.y );
				DrawList->AddRectFilled( BarMin, CurrentMax, ColorCurrentHp, ScaledGaugeRounding );
			}

			// 細い太さで全周を一筆描き → 左右の丸キャップだけ太い円弧を重ねる
			DrawList->AddRect( BorderMin, BorderMax, ColorBorder, ScaledGaugeRounding, 0, ScaledBorderThicknessV );
			{
				const float PillRadius = AnimHeight * 0.5f;
				const float CapRadius = FMath::Max( PillRadius - ScaledBorderThickness * 0.5f, 0.5f );
				const float CapMidY = ( BorderMin.y + BorderMax.y ) * 0.5f;
				const float LeftCx = BorderMin.x + PillRadius;
				const float RightCx = BorderMax.x - PillRadius;

				const float HalfPi = (float)PI * 0.5f;
				DrawList->PathArcTo( ImVec2( LeftCx, CapMidY ), CapRadius, HalfPi, HalfPi * 3.0f );
				DrawList->PathStroke( ColorBorder, 0, ScaledBorderThickness );
				DrawList->PathArcTo( ImVec2( RightCx, CapMidY ), CapRadius, -HalfPi, HalfPi );
				DrawList->PathStroke( ColorBorder, 0, ScaledBorderThickness );
			}

			// 追従ゲージと現在HPの差を、その帯の幅ぶんの縦ブロックで強調する
			if ( TrailingRatio - HpRatio > TipGapThreshold )
			{
				const float LeftX = BarMin.x + ScaledGaugeWidth * HpRatio;
				float RightX = BarMin.x + ScaledGaugeWidth * TrailingRatio;
				if ( RightX - LeftX < ScaledTipThickness ) RightX = LeftX + ScaledTipThickness;	// 最低の線幅は確保
				const ImVec2 TipMin = ImVec2( LeftX, BarMin.y - ScaledTipOverhang );
				const ImVec2 TipMax = ImVec2( RightX, BarMin.y );	// 下の輪郭にかぶせない
				DrawList->AddRectFilled( TipMin, TipMax, ColorTipLine );
			}

			// ゲージの上に中央揃え。枠と同じ色で輪郭をつけてから文字を重ねる
			if ( Name && Name[0] )
			{
				ImFont* Font = ImGui::GetFont();
				const float FontPx = ImGui::GetFontSize() * NameFontScale * ScaleFactor;
				const float NameW = ImGui::CalcTextSize( Name ).x * NameFontScale * ScaleFactor;
				const ImVec2 NamePos = ImVec2( CenterX - NameW * 0.5f, FullTop - ScaledNameOffsetY - FontPx );

				// 周囲 8 方向に枠色でずらして描く
				const float OutlinePx = FMath::Max( 1.0f, 1.5f * ScaleFactor );
				const ImVec2 Offsets[8] = {
					ImVec2( -OutlinePx, -OutlinePx ), ImVec2( 0.0f, -OutlinePx ), ImVec2( OutlinePx, -OutlinePx ),
					ImVec2( -OutlinePx,  0.0f ),                                  ImVec2( OutlinePx,  0.0f ),
					ImVec2( -OutlinePx,  OutlinePx ), ImVec2( 0.0f,  OutlinePx ), ImVec2( OutlinePx,  OutlinePx )
				};
				for ( const ImVec2& Off : Offsets )
				{
					DrawList->AddText( Font, FontPx, ImVec2( NamePos.x + Off.x, NamePos.y + Off.y ), ColorBorder, Name );
				}
				DrawList->AddText( Font, FontPx, NamePos, ColorText, Name );
			}

			// 名前があるときは重ならないよう下に出す
			if ( bShowHpText )
			{
				char HpText[32];
				snprintf( HpText, sizeof( HpText ), "%.0f / %.0f", CurrentHP, MaxHP );
				ImFont* Font = ImGui::GetFont();
				const float HpFontPx = ImGui::GetFontSize() * ScaleFactor;
				const float TextW = ImGui::CalcTextSize( HpText ).x * ScaleFactor;
				const float TextH = ImGui::GetFontSize() * ScaleFactor;
				const float TextY = ( Name && Name[0] )
					? ( FullBottom + ScaledTextOffsetY )
					: ( FullTop - TextH - ScaledTextOffsetY );
				const ImVec2 TextPos = ImVec2( CenterX - TextW * 0.5f, TextY );
				DrawList->AddText( TextPos, ColorText, HpText );
			}

			DrawList->PopClipRect();
		}
	} // namespace

	// 通常（頭上追従）HP ゲージ
	void Draw( const UWorld* World, const FVector& WorldAnchorLocation, float ScreenOffsetY, float CurrentHP, float MaxHP, float& InOutTrailingHP, bool bShowHpText, bool bWantVisible, float& InOutShowAlpha, bool bGrayedOut )
	{
		if ( !World || MaxHP <= 0.0f ) return;

		APlayerController* PC = UGameplayStatics::GetPlayerController( World, 0 );
		if ( !PC ) return;

		FVector2D ScreenPos = FVector2D::ZeroVector;
		if ( !UGameplayStatics::ProjectWorldToScreen( PC, WorldAnchorLocation, ScreenPos ) )
		{
			return; // カメラ背後など投影不可
		}

		FVector2D ViewportSize;
		const float ScaleFactor = GetUIScaleFactor( ViewportSize );

		const ImGuiViewport* Viewport = ImGui::GetMainViewport();
		const ImVec2 Origin = Viewport ? Viewport->WorkPos : ImVec2( 0.0f, 0.0f );

		ImDrawList* DrawList = ImGui::GetBackgroundDrawList();
		if ( !DrawList ) return;

		// アンカー（頭の投影点）から画面ピクセルで固定オフセット上に置くので距離でズレない。
		// バー下端をアンカーに合わせるため、縦中心は半分上
		const float ScaledGaugeHeight = 14.0f * ScaleFactor;
		const float ScaledOffsetY = ScreenOffsetY * ScaleFactor;
		const float CenterX = Origin.x + (float)ScreenPos.X;
		const float AnchorBottomY = Origin.y + (float)ScreenPos.Y - ScaledOffsetY;
		const float MidY = AnchorBottomY - ScaledGaugeHeight * 0.5f;
		const float FullWidth = 170.0f * ScaleFactor;

		// 1 発のダメージが HP の大きな割合なので、しきい値は大きめ
		constexpr float TipGapThreshold = 0.05f;
		DrawGaugeCommon( DrawList, ScaleFactor, World->GetDeltaSeconds(),
			CenterX, MidY, FullWidth, CurrentHP, MaxHP, InOutTrailingHP,
			bShowHpText, nullptr, TipGapThreshold, bWantVisible, InOutShowAlpha, bGrayedOut );
	}

	// ボス用 HP ゲージ（画面上部に固定表示・名前付き）
	void DrawBoss( const UWorld* World, const char* BossName, float CurrentHP, float MaxHP, float& InOutTrailingHP, bool bShowHpText, bool bWantVisible, float& InOutShowAlpha )
	{
		if ( !World || MaxHP <= 0.0f ) return;

		FVector2D ViewportSize;
		const float ScaleFactor = GetUIScaleFactor( ViewportSize );

		const ImGuiViewport* Viewport = ImGui::GetMainViewport();
		const ImVec2 Origin = Viewport ? Viewport->WorkPos : ImVec2( 0.0f, 0.0f );

		ImDrawList* DrawList = ImGui::GetBackgroundDrawList();
		if ( !DrawList ) return;

		// 画面上部・横中央に固定配置。名前ぶんの余白を上に確保するため少し下げる
		const float ScaledGaugeHeight = 14.0f * ScaleFactor;
		const float CenterX = Origin.x + (float)ViewportSize.X * 0.5f;
		const float TopMargin = 70.0f * ScaleFactor;	// 画面上端からバー上端までの余白
		const float FullWidth = 700.0f * ScaleFactor;	// ボスは横長
		const float MidY = Origin.y + TopMargin + ScaledGaugeHeight * 0.5f;

		// HP が大きく 1 発の割合が小さいので、小ダメージでも先端線が出るよう小さくする
		constexpr float TipGapThreshold = 0.01f;
		DrawGaugeCommon( DrawList, ScaleFactor, World->GetDeltaSeconds(),
			CenterX, MidY, FullWidth, CurrentHP, MaxHP, InOutTrailingHP,
			bShowHpText, BossName, TipGapThreshold, bWantVisible, InOutShowAlpha, /*bGrayedOut*/ false );
	}
}

#endif // !UE_BUILD_SHIPPING
