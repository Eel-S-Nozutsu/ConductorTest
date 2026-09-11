// Copyright (c) 2026, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.
#include "GodActionHudPlaceholder.h"

#if !UE_BUILD_SHIPPING

#include "imgui.h"

#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Misc/App.h"

#include "PRJ_TIDE_P0/Integration/ImGui/TideHudColors.h"
#include "PRJ_TIDE_P0/Integration/ImGui/TideHudAnim.h"
#include "PRJ_TIDE_P0/Core/TideGameSettings.h"

namespace GodActionHudPlaceholder
{
	namespace
	{
		// 表示 in/out アニメの進行 [0,1]。神技 HUD は 1 つしか無いのでここで持つ
		float GShowAlpha = 0.0f;

		// ForegroundDrawList への直描きには ImGuiStyleVar_Alpha が効かないため、
		// この HUD の色はすべてこれを通す（＝素の ToU32 は使わない）
		ImU32 ShowCol( const ImVec4& C ) { return TideHudColors::ToU32( TideHudColors::WithAlpha( C, C.w * GShowAlpha ) ); }

		float GTrailingGaugeRate = -1.0f;	// 遅れて変化する残像バー。-1 は未初期化

		// ゲージ増加フラッシュ。GFlashAge<0 は非アクティブ、GPrevGaugeRate は増加検出用の前フレーム値
		float GFlashAge = -1.0f;
		float GPrevGaugeRate = -1.0f;

		float GArtSel01[3] = { 1.0f, 0.0f, 0.0f };	// 3択の選択度。大きさは変えず色・グローだけで示す
		float GStanceVis = 0.0f;					// 構えの表示率。拡大・山なりの持ち上げ・ボタン記号に使う
		float GArrowVis = 0.0f;						// 左右 ◀▶ の表示率（カーソルモードの構え中のみ）

		// 右端 RightX に右揃えでテキストを描く（HUD が画面右下なので基準は右端になる）
		void AddTextScaledRight( ImDrawList* DrawList, float RightX, float TopY, ImU32 Color, float FontSize, const char* Text )
		{
			ImFont* Font = ImGui::GetFont();
			const float Width = Font->CalcTextSizeA( FontSize, FLT_MAX, 0.0f, Text ).x;
			DrawList->AddText( Font, FontSize, ImVec2( RightX - Width, TopY ), Color, Text );
		}

		// 横長レティクル（四隅ブラケット＋中央リング）を描画する
		void DrawReticle( ImDrawList* DrawList, const ImVec2& Center, float HalfW, float HalfH, ImU32 Color, float Thickness, int32 Index )
		{
			const float Bracket = FMath::Min( HalfW, HalfH ) * 0.6f;	// 角ブラケットの長さ

			const ImVec2 TL = ImVec2( Center.x - HalfW, Center.y - HalfH );
			const ImVec2 TR = ImVec2( Center.x + HalfW, Center.y - HalfH );
			const ImVec2 BL = ImVec2( Center.x - HalfW, Center.y + HalfH );
			const ImVec2 BR = ImVec2( Center.x + HalfW, Center.y + HalfH );

			DrawList->AddLine( TL, ImVec2( TL.x + Bracket, TL.y ), Color, Thickness );
			DrawList->AddLine( TL, ImVec2( TL.x, TL.y + Bracket ), Color, Thickness );
			DrawList->AddLine( TR, ImVec2( TR.x - Bracket, TR.y ), Color, Thickness );
			DrawList->AddLine( TR, ImVec2( TR.x, TR.y + Bracket ), Color, Thickness );
			DrawList->AddLine( BL, ImVec2( BL.x + Bracket, BL.y ), Color, Thickness );
			DrawList->AddLine( BL, ImVec2( BL.x, BL.y - Bracket ), Color, Thickness );
			DrawList->AddLine( BR, ImVec2( BR.x - Bracket, BR.y ), Color, Thickness );
			DrawList->AddLine( BR, ImVec2( BR.x, BR.y - Bracket ), Color, Thickness );

			// 中央のリング・番号は各対象の小レティクルのみ。大型（Index < 0）は四隅ブラケットだけ
			if ( Index >= 0 )
			{
				DrawList->AddCircle( Center, FMath::Min( HalfW, HalfH ) * 0.45f, Color, 32, Thickness );

				char NumText[8];
				snprintf( NumText, sizeof( NumText ), "%d", Index + 1 );
				const float NumFontSize = FMath::Min( HalfW, HalfH ) * 0.9f;
				ImFont* Font = ImGui::GetFont();
				const float NumWidth = Font->CalcTextSizeA( NumFontSize, FLT_MAX, 0.0f, NumText ).x;
				DrawList->AddText( Font, NumFontSize, ImVec2( Center.x - NumWidth * 0.5f, Center.y - NumFontSize * 0.5f ), Color, NumText );
			}
		}
	}

	void Draw( const UWorld* World, APlayerController* PC, const FGodActionHudData& Data, bool bWantVisible )
	{
		const ImGuiViewport* Viewport = ImGui::GetMainViewport();
		if ( !Viewport ) return;

		// リング状セレクタで「幅→太さ」の形が合わないため、フェード＋下方向スライドで表現する
		TideHudAnim::UpdateShowAlpha( GShowAlpha, bWantVisible );
		if ( GShowAlpha <= 0.0f ) return;

		// --- 解像度スケール（HP ゲージと同じ流儀）---
		FVector2D GameViewportSize = FVector2D( 1920.0, 1080.0 );	// 取得失敗時のフォールバック
		if ( GEngine && GEngine->GameViewport )
		{
			GEngine->GameViewport->GetViewportSize( GameViewportSize );
		}

		constexpr float BaseResolutionY = 1080.0f;
		const float ScaleFactor = FMath::Max( GameViewportSize.Y / BaseResolutionY, 0.1f );

		// --- 神技セレクタ（画面右下）の基本サイズ ---
		constexpr float BasePaddingX = 80.0f;		// 画面右端からの余白
		constexpr float BasePaddingY = 100.0f;		// 画面下端からの余白（少し上げてある）
		constexpr float BaseInfoFontSize = 20.0f;	// 補助テキスト
		constexpr float BaseLabelGap = 6.0f;		// 補助テキストの隙間

		const float InfoFontSize = BaseInfoFontSize * ScaleFactor;
		const float LabelGap = BaseLabelGap * ScaleFactor;

		const float Time = World ? World->GetRealTimeSeconds() : (float)ImGui::GetTime();
		const float Pulse = 0.5f + 0.5f * FMath::Sin( Time * 6.0f );	// 0〜1

		// モチーフカラー切替（トグルは DebugFlags ウィンドウの「UI」→ HUDモチーフカラー）
		const UTideGameSettings* Settings = UTideGameSettings::Get();
		const bool bMotif = Settings && Settings->bUseTideMotifHudColors;

		// --- 色（TideHudColors に集約。色味を変えるときはそちらを編集する）---
		const ImU32 ColorGaugeBg = ShowCol( bMotif ? TideHudColors::GodAction::GaugeBgMotif : TideHudColors::GodAction::GaugeBg );
		const ImU32 ColorTrailing = ShowCol( bMotif ? TideHudColors::GodAction::TrailingMotif : TideHudColors::GodAction::Trailing );

		// 蓄積中は単色（ゴールド/ラスト）、満タン時はパルスで明滅
		const ImU32 ColorFill = Data.bGaugeFull
			? ShowCol( TideHudColors::Lerp(
				bMotif ? TideHudColors::GodAction::FillPulseLoMotif : TideHudColors::GodAction::FillPulseLo,
				bMotif ? TideHudColors::GodAction::FillPulseHiMotif : TideHudColors::GodAction::FillPulseHi, Pulse ) )
			: ShowCol( bMotif ? TideHudColors::GodAction::GaugeFillMotif : TideHudColors::GodAction::GaugeFill );

		ImDrawList* DrawList = ImGui::GetForegroundDrawList();

		// --- 神技セレクタの配置（右下）---
		// 右端・下端は ImGui の WorkSize（OS ウィンドウ全体になりがち）ではなく実際のゲームビューポート基準で
		// 求める（ウィンドウモードでの見切れ対策）
		const float RightX = Viewport->WorkPos.x + (float)GameViewportSize.X - BasePaddingX * ScaleFactor;
		// out アニメでは画面下へ沈めながら消す（セレクタ／ゲージ／エネルギー玉の収束先がすべてここ基準）。
		// ワールド投影のレティクルはこの基準を使わないのでスライドせず、アルファだけで消える
		constexpr float BaseShowSlideY = 60.0f;
		const float ShowSlideY = BaseShowSlideY * ScaleFactor * ( 1.0f - TideHudAnim::Smooth01( GShowAlpha ) );
		const float BottomY = Viewport->WorkPos.y + (float)GameViewportSize.Y - BasePaddingY * ScaleFactor + ShowSlideY;

		// 増加は即時、減少はゆっくり追従
		if ( GTrailingGaugeRate < 0.0f || GTrailingGaugeRate < Data.GaugeRate )
		{
			GTrailingGaugeRate = Data.GaugeRate;
		}
		else if ( GTrailingGaugeRate > Data.GaugeRate )
		{
			const float DeltaTime = FApp::GetDeltaTime();
			GTrailingGaugeRate = FMath::FInterpTo( GTrailingGaugeRate, Data.GaugeRate, DeltaTime, 3.0f );
		}

		// ゲージはアイコンの周囲リングに融合。3 択共通なので 3 つとも同じ量を描き、
		// エネルギー玉とフラッシュは真ん中のアイコンへ集める
		const float TrailingRate = FMath::Clamp( GTrailingGaugeRate, 0.0f, 1.0f );
		const float FillRate     = FMath::Clamp( Data.GaugeRate, 0.0f, 1.0f );

		const float AnimDt = FApp::GetDeltaTime();

		// 3 つとも同じ大きさで並べ、構えるとまとめて一回り大きくなり真ん中だけ持ち上がる。
		// 選択は大きさではなく色・グローで示す（＝X/Y/B の配置がそのまま並びになる）
		GStanceVis = FMath::FInterpTo( GStanceVis, Data.bGodArtSelecting ? 1.0f : 0.0f, AnimDt, 16.0f );
		const float StanceE = TideHudAnim::Smooth01( GStanceVis );

		// 見た目の調整はこの 4 つで足りる（px@1080p。解像度は ScaleFactor が掛かる）
		constexpr float BaseIconR       = 26.0f;	// 非構え時の半径（3 つとも同じ小サイズ）
		constexpr float StanceIconScale = 1.25f;	// 構え中の拡大率（若干大きく）
		constexpr float BaseIconGap     = 20.0f;	// 縁と縁の隙間
		constexpr float BaseArchLift    = 20.0f;	// 構え中に真ん中を持ち上げる量（山なり）

		const float IconR    = BaseIconR * ScaleFactor * FMath::Lerp( 1.0f, StanceIconScale, StanceE );
		const float IconGap  = BaseIconGap * ScaleFactor;
		const float ArchLift = BaseArchLift * ScaleFactor * StanceE;

		// 右端固定で右から左へ 依／導／戯 の順に並べる（描画・索引は 0=戯 が左のまま）
		float CenterX[3], CenterY[3];
		for ( int32 i = 0; i < 3; ++i )
		{
			CenterX[i] = RightX - IconR - (float)( 2 - i ) * ( IconR * 2.0f + IconGap );
			CenterY[i] = BottomY - IconR - ( i == 1 ? ArchLift : 0.0f );
		}

		for ( int32 i = 0; i < 3; ++i )
		{
			GArtSel01[i] = FMath::FInterpTo( GArtSel01[i], ( i == Data.SelectedArt ) ? 1.0f : 0.0f, AnimDt, 18.0f );
		}

		// 吸着先は真ん中のアイコン（ゲージは 3 択共通なので選択中には寄せない）
		const ImVec2 AnchorCenter( CenterX[1], CenterY[1] );

		// 増えた瞬間に広がるリング。ゲージは共通なので 3 つとも同時に光らせる
		{
			const float FlashDelta = FApp::GetDeltaTime();
			const float FlashTime = FMath::Max( 0.05f, Data.OrbFlashTime );

			if ( GPrevGaugeRate >= 0.0f && Data.GaugeRate > GPrevGaugeRate + 0.0005f )
			{
				GFlashAge = 0.0f;
			}
			GPrevGaugeRate = Data.GaugeRate;

			if ( GFlashAge >= 0.0f )
			{
				GFlashAge += FlashDelta;
				if ( GFlashAge >= FlashTime )
				{
					GFlashAge = -1.0f;
				}
				else
				{
					const float fa = GFlashAge / FlashTime;
					const float a = ( 1.0f - fa ) * ( 1.0f - fa );
					const float FlashR = IconR + ( 8.0f + 24.0f * fa ) * ScaleFactor;
					const float FThick = FMath::Max( 1.0f, 3.0f * ScaleFactor ) * ( 1.0f - 0.4f * fa );
					const ImVec4 GlowCol = bMotif ? TideHudColors::GodAction::OrbGlowMotif : TideHudColors::GodAction::OrbGlow;
					for ( int32 i = 0; i < 3; ++i )
					{
						const ImVec2 Center( CenterX[i], CenterY[i] );
						DrawList->AddCircle( Center, FlashR, ShowCol( TideHudColors::WithAlpha( GlowCol, a * 0.6f ) ), 40, FThick * 2.0f );
						DrawList->AddCircle( Center, FlashR, ShowCol( TideHudColors::WithAlpha( TideHudColors::White, a * 0.85f ) ), 40, FThick );
					}
				}
			}
		}

		// --- エネルギー玉（被弾箇所→ゲージへカーブ移動。収納＝モジュール側でゲージ加算）---
		if ( PC && Data.Orbs.Num() > 0 )
		{
			const ImVec2 GaugeAnchor = AnchorCenter;
			const ImVec4 OrbCoreCol = bMotif ? TideHudColors::GodAction::OrbCoreMotif : TideHudColors::GodAction::OrbCore;
			const ImVec4 GlowBase = bMotif ? TideHudColors::GodAction::OrbGlowMotif : TideHudColors::GodAction::OrbGlow;
			const float CoreR = FMath::Max( 2.0f, Data.OrbRadius * ScaleFactor );

			auto Bezier3 = []( const ImVec2& P0, const ImVec2& P1, const ImVec2& P2, const ImVec2& P3, float u )
			{
				const float v = 1.0f - u;
				const float a = v * v * v, b = 3.0f * v * v * u, c = 3.0f * v * u * u, d = u * u * u;
				return ImVec2( a * P0.x + b * P1.x + c * P2.x + d * P3.x, a * P0.y + b * P1.y + c * P2.y + d * P3.y );
			};
			const float OrbA = FMath::Clamp( Data.OrbAlpha, 0.0f, 1.0f );	// グロー・尾・コア全体に掛ける

			for ( const FGodActionHudData::FOrbView& Orb : Data.Orbs )
			{
				FVector2D ScreenPos;
				if ( !PC->ProjectWorldLocationToScreen( Orb.StartWorldLoc, ScreenPos, false ) )
				{
					continue;	// カメラ背後など投影不可。ゲージ加算はモジュール側で進むので見た目だけ省略
				}
				const ImVec2 Start( Viewport->Pos.x + (float)ScreenPos.X, Viewport->Pos.y + (float)ScreenPos.Y );

				// 終盤に吸い込まれる感じの smoothstep
				const float t = FMath::Clamp( Orb.Progress, 0.0f, 1.0f );
				const float et = t * t * ( 3.0f - 2.0f * t );

				const float dx = GaugeAnchor.x - Start.x;
				const float dy = GaugeAnchor.y - Start.y;
				const float Dist = FMath::Max( 1.0f, FMath::Sqrt( dx * dx + dy * dy ) );
				const float Curve = FMath::Max( 0.0f, Data.OrbCurveStrength );

				// 発生点→ゲージの直線を中心軸とし、垂直方向へ Lateral 符号で膨らませる＝玉ごとに左右へ分かれる。
				// 序盤（25%地点）を大きく張り出し、終盤（70%）は控えめにして収束させる
				ImVec2 Perp( -dy / Dist, dx / Dist );
				const float Bow1 = Orb.Lateral * Dist * Curve * 1.4f;
				const float Bow2 = Orb.Lateral * Dist * Curve * 0.5f;
				const ImVec2 A1( Start.x + dx * 0.25f, Start.y + dy * 0.25f );
				const ImVec2 A2( Start.x + dx * 0.70f, Start.y + dy * 0.70f );
				const ImVec2 P1( A1.x + Perp.x * Bow1, A1.y + Perp.y * Bow1 );
				const ImVec2 P2( A2.x + Perp.x * Bow2, A2.y + Perp.y * Bow2 );

				const ImVec2 Pos = Bezier3( Start, P1, P2, GaugeAnchor, et );
				// 玉ごとの見た目倍率（BoostGimmick 経路などを目立たせる）
				const float R = CoreR * Orb.RadiusScale * ( 1.0f - 0.35f * et );	// 収納に向けて少し縮む
				const float ThisOrbA = FMath::Clamp( OrbA * Orb.AlphaScale, 0.0f, 1.0f );

				// グロー（外周）
				DrawList->AddCircleFilled( Pos, R * 1.8f, ShowCol( TideHudColors::WithAlpha( GlowBase, 0.10f * ThisOrbA ) ), 20 );
				DrawList->AddCircleFilled( Pos, R * 1.2f, ShowCol( TideHudColors::WithAlpha( GlowBase, 0.26f * ThisOrbA ) ), 20 );

				// 短い尾（手前の進捗を数点、さらに薄く重ねる）
				for ( int32 s = 1; s <= 2; ++s )
				{
					const float tt = FMath::Max( 0.0f, et - 0.06f * (float)s );
					const ImVec2 TrailPos = Bezier3( Start, P1, P2, GaugeAnchor, tt );
					DrawList->AddCircleFilled( TrailPos, R * ( 0.65f - 0.18f * (float)s ), ShowCol( TideHudColors::WithAlpha( GlowBase, ( 0.09f - 0.03f * (float)s ) * ThisOrbA ) ), 16 );
				}
				// コア
				DrawList->AddCircleFilled( Pos, R, ShowCol( TideHudColors::WithAlpha( OrbCoreCol, ThisOrbA ) ), 20 );
			}
		}

		// ロックオン中の操作ヒント＋カウント
		if ( Data.bLockingOn )
		{
			char CountText[48];
			snprintf( CountText, sizeof( CountText ), "LOCK ON  %d / %d", Data.LockedCount, Data.MaxLockOnCount );
			AddTextScaledRight( DrawList, RightX, BottomY + LabelGap, ShowCol( bMotif ? TideHudColors::GodAction::LockCountMotif : TideHudColors::GodAction::LockCount ), InfoFontSize, CountText );

			// 3 択構え中は操作系統が違う（十字←→で選択・R2 発動・L2 解除）ため、この従来ヒントは出さない
			if ( !Data.bGodArtSelecting )
			{
				const char* HintText = Data.bGaugeFull
					? "Y: \xe7\x99\xba\xe5\x8b\x95 / B: \xe3\x82\xad\xe3\x83\xa3\xe3\x83\xb3\xe3\x82\xbb\xe3\x83\xab"	// Y:発動 / B:キャンセル
					: "\xe3\x82\xb2\xe3\x83\xbc\xe3\x82\xb8\xe4\xb8\x8d\xe8\xb6\xb3  Y/B: \xe3\x82\xad\xe3\x83\xa3\xe3\x83\xb3\xe3\x82\xbb\xe3\x83\xab"; // ゲージ不足  Y/B:キャンセル
				AddTextScaledRight( DrawList, RightX, BottomY + LabelGap + InfoFontSize + LabelGap,
					ShowCol( bMotif ? TideHudColors::GodAction::HintMotif : TideHudColors::GodAction::Hint ), InfoFontSize, HintText );
			}
		}

		// --- 大型レティクル（ロックオン中。画面中央に画面半分ほどの大きさ）---
		if ( Data.bLockingOn )
		{
			const ImVec2 ScreenCenter = ImVec2(
				Viewport->WorkPos.x + (float)GameViewportSize.X * 0.5f,
				Viewport->WorkPos.y + (float)GameViewportSize.Y * 0.5f );

			const float BigHalfW = (float)GameViewportSize.X * 0.40f;
			const float BigHalfH = (float)GameViewportSize.Y * 0.35f;
			const float BigThickness = FMath::Max( 3.0f, 4.5f * ScaleFactor );

			// 蓄積中はグレー、発動可能はゴールドでパルス
			const ImU32 BigColor = Data.bGaugeFull
				? ShowCol( TideHudColors::WithAlpha( bMotif ? TideHudColors::GodAction::ReticleGoldMotif : TideHudColors::GodAction::ReticleGold, FMath::Lerp( 160.0f, 255.0f, Pulse ) / 255.0f ) )
				: ShowCol( bMotif ? TideHudColors::GodAction::ReticleNeutralMotif : TideHudColors::GodAction::ReticleNeutral );

			DrawReticle( DrawList, ScreenCenter, BigHalfW, BigHalfH, BigColor, BigThickness, -1 );
		}

		// --- ロックオンレティクル（ワールド→スクリーン投影）---
		if ( PC && ( Data.bLockingOn || Data.bSlashing ) && Data.LockedTargetWorldLocations.Num() > 0 )
		{
			const float ReticleHalfW = 38.0f * ScaleFactor;
			const float ReticleHalfH = 38.0f * ScaleFactor;
			const float ReticleThickness = FMath::Max( 1.5f, 3.5f * ScaleFactor );

			// ロックオン中は黄、発動中はパルス
			const ImU32 ReticleColor = Data.bSlashing
				? ShowCol( TideHudColors::Lerp(
					bMotif ? TideHudColors::GodAction::ReticleSlashLoMotif : TideHudColors::GodAction::ReticleSlashLo,
					bMotif ? TideHudColors::GodAction::ReticleSlashHiMotif : TideHudColors::GodAction::ReticleSlashHi, Pulse ) )
				: ShowCol( bMotif ? TideHudColors::GodAction::ReticleGoldMotif : TideHudColors::GodAction::ReticleGold );

			// マーカー背後に薄く敷いて浮き立たせる
			const ImVec4 ReticleGlowVec = bMotif ? TideHudColors::GodAction::ReticleGlowMotif : TideHudColors::GodAction::ReticleGlow;
			const float GlowAlpha = FMath::Lerp( 0.12f, 0.26f, Pulse );
			const float GlowRadius = ReticleHalfW * 1.15f;

			for ( int32 i = 0; i < Data.LockedTargetWorldLocations.Num(); ++i )
			{
				FVector2D ScreenPos;
				if ( PC->ProjectWorldLocationToScreen( Data.LockedTargetWorldLocations[i], ScreenPos, false ) )
				{
					const ImVec2 Center = ImVec2( Viewport->Pos.x + (float)ScreenPos.X, Viewport->Pos.y + (float)ScreenPos.Y );
					DrawList->AddCircleFilled( Center, GlowRadius, ShowCol( TideHudColors::WithAlpha( ReticleGlowVec, GlowAlpha * 0.5f ) ), 24 );
					DrawList->AddCircleFilled( Center, GlowRadius * 0.6f, ShowCol( TideHudColors::WithAlpha( ReticleGlowVec, GlowAlpha ) ), 24 );
					DrawReticle( DrawList, Center, ReticleHalfW, ReticleHalfH, ReticleColor, ReticleThickness, i );
				}
			}
		}

		// --- 3択神技セレクタ（画面右下に丸アイコンで常時表示。構えると拡大＋山なりになる）---
		{
			// 索引順＝戯/導/依（Frolic/Guidance/Possession）で、そのまま X（左）／Y（上）／B（右）の配置に対応する
			const char* ArtLabels[3] = { "戯", "導", "依" };

			ImFont* ArtFont = ImGui::GetFont();

			// アクセント色（クラシック＝金／モチーフ＝ミント）。選択度で補間する
			const ImVec4 GoldVec    = bMotif ? TideHudColors::GodAction::ReticleGoldMotif    : TideHudColors::GodAction::ReticleGold;
			const ImVec4 NeutralVec = bMotif ? TideHudColors::GodAction::ReticleNeutralMotif : TideHudColors::GodAction::ReticleNeutral;
			const ImVec4 GlowVec    = bMotif ? TideHudColors::GodAction::ReticleGlowMotif    : TideHudColors::GodAction::ReticleGlow;
			const ImVec4 HintVec    = bMotif ? TideHudColors::GodAction::HintMotif           : TideHudColors::GodAction::Hint;
			const ImVec4 ShadowVec  = ImVec4( 0.0f, 0.0f, 0.0f, 1.0f );

			for ( int32 i = 0; i < 3; ++i )
			{
				const float R = IconR;
				const ImVec2 Center( CenterX[i], CenterY[i] );
				const float Sel01 = FMath::Clamp( GArtSel01[i], 0.0f, 1.0f );
				// 鳥が出払っている間は、その鳥を使う神技（戯=0／導=1）を選択不可としてグレーで示す
				const bool bArtDisabled = Data.bBirdAway && i != 2;
				const ImVec4 Accent = bArtDisabled ? ImVec4( 0.5f, 0.5f, 0.52f, 1.0f ) : TideHudColors::Lerp( NeutralVec, GoldVec, Sel01 );

				// 1) 外側グロー（ゲージ満タン時のみ）
				if ( Sel01 > 0.01f && Data.bGaugeFull && !bArtDisabled )
				{
					const float GlowA = ( 0.10f + 0.14f * Pulse ) * Sel01;
					DrawList->AddCircleFilled( Center, R * 1.45f, ShowCol( TideHudColors::WithAlpha( GlowVec, GlowA * 0.5f ) ), 40 );
					DrawList->AddCircleFilled( Center, R * 1.18f, ShowCol( TideHudColors::WithAlpha( GlowVec, GlowA ) ), 40 );
				}

				// 2) 下地ディスク＋中心アクセント
				DrawList->AddCircleFilled( Center, R, ShowCol( TideHudColors::WithAlpha( TideHudColors::DarkPanel, FMath::Lerp( 0.55f, 0.92f, Sel01 ) ) ), 40 );
				DrawList->AddCircleFilled( Center, R * 0.70f, ShowCol( TideHudColors::WithAlpha( Accent, 0.06f + 0.12f * Sel01 ) ), 40 );

				// 3) 外リング＋内側白ハイライト
				const float RingThick = FMath::Max( 1.5f, ( 2.0f + 1.8f * Sel01 ) * ScaleFactor );
				const float RingPulse = Data.bGaugeFull ? FMath::Lerp( 0.7f, 1.0f, Pulse ) : 0.9f;
				const float RingAlpha = FMath::Lerp( 0.85f, RingPulse, Sel01 );
				DrawList->AddCircle( Center, R, ShowCol( TideHudColors::WithAlpha( Accent, RingAlpha ) ), 48, RingThick );
				DrawList->AddCircle( Center, R * 0.82f, ShowCol( TideHudColors::WithAlpha( TideHudColors::White, 0.08f + 0.16f * Sel01 ) ), 40, FMath::Max( 1.0f, 1.0f * ScaleFactor ) );

				// 3b) 周囲リング（トラック＋追従＋現在値）。ゲージは共通なので 3 つとも同じ
				{
					constexpr float RingPi = 3.14159265f;
					const float Rg = R + 6.0f * ScaleFactor;
					const float GaugeThick = FMath::Max( 3.0f, 5.0f * ScaleFactor );
					const float aTop = -0.5f * RingPi;
					DrawList->AddCircle( Center, Rg, ColorGaugeBg, 48, GaugeThick * 0.7f );
					if ( TrailingRate > 0.001f )
					{
						DrawList->PathArcTo( Center, Rg, aTop, aTop + TrailingRate * 2.0f * RingPi, FMath::Max( 6, (int)( 64.0f * TrailingRate ) ) );
						DrawList->PathStroke( ColorTrailing, 0, GaugeThick );
					}
					if ( FillRate > 0.001f )
					{
						DrawList->PathArcTo( Center, Rg, aTop, aTop + FillRate * 2.0f * RingPi, FMath::Max( 6, (int)( 64.0f * FillRate ) ) );
						DrawList->PathStroke( ColorFill, 0, GaugeThick );
					}
				}

				// 4) 上部ハイライト弧（満タン時）
				if ( Sel01 > 0.15f && Data.bGaugeFull && !bArtDisabled )
				{
					constexpr float ArtPi = 3.14159265f;
					DrawList->PathArcTo( Center, R * 0.86f, ArtPi * 1.15f, ArtPi * 1.85f, 18 );
					DrawList->PathStroke( ShowCol( TideHudColors::WithAlpha( TideHudColors::White, 0.28f * Sel01 ) ), 0, FMath::Max( 1.0f, 1.5f * ScaleFactor ) );
				}

				// 5) ラベル（影→本体）
				const float IconFontSize = R * 1.15f;
				const float TextW = ArtFont->CalcTextSizeA( IconFontSize, FLT_MAX, 0.0f, ArtLabels[i] ).x;
				const ImVec2 TextPos( Center.x - TextW * 0.5f, Center.y - IconFontSize * 0.5f );
				DrawList->AddText( ArtFont, IconFontSize, ImVec2( TextPos.x + 1.5f * ScaleFactor, TextPos.y + 1.5f * ScaleFactor ), ShowCol( TideHudColors::WithAlpha( ShadowVec, 0.5f ) ), ArtLabels[i] );
				const ImVec4 TextVec = bArtDisabled ? Accent : TideHudColors::Lerp( HintVec, GoldVec, Sel01 );
				DrawList->AddText( ArtFont, IconFontSize, TextPos, ShowCol( TextVec ), ArtLabels[i] );

				// 6) 選択不可は薄い暗幕で覆って沈める
				if ( bArtDisabled )
				{
					DrawList->AddCircleFilled( Center, R, ShowCol( TideHudColors::WithAlpha( TideHudColors::DarkPanel, 0.4f ) ), 40 );
				}
			}

			// X/Y/B 直接選択モード：各アイコンの上へ対応ボタンの記号を出す
			if ( Data.bFaceButtonSelect && StanceE > 0.01f )
			{
				const char* FaceLabels[3] = { "X", "Y", "B" };
				const ImVec4 FaceColors[3] = {
					TideHudColors::GodAction::FaceButtonX,
					TideHudColors::GodAction::FaceButtonY,
					TideHudColors::GodAction::FaceButtonB };

				const float BadgeR   = IconR * 0.40f;
				const float BadgeGap = 8.0f * ScaleFactor;
				for ( int32 i = 0; i < 3; ++i )
				{
					const ImVec2 c( CenterX[i], CenterY[i] - IconR - BadgeGap - BadgeR );
					DrawList->AddCircleFilled( c, BadgeR, ShowCol( TideHudColors::WithAlpha( TideHudColors::DarkPanel, 0.8f * StanceE ) ), 24 );
					DrawList->AddCircle( c, BadgeR, ShowCol( TideHudColors::WithAlpha( FaceColors[i], StanceE ) ), 24, FMath::Max( 1.5f, 2.0f * ScaleFactor ) );

					const float FontSize = BadgeR * 1.5f;
					const float TextW = ArtFont->CalcTextSizeA( FontSize, FLT_MAX, 0.0f, FaceLabels[i] ).x;
					DrawList->AddText( ArtFont, FontSize, ImVec2( c.x - TextW * 0.5f, c.y - FontSize * 0.5f ),
						ShowCol( TideHudColors::WithAlpha( FaceColors[i], StanceE ) ), FaceLabels[i] );
				}
			}

			// カーソル移動を示す ◀▶ 丸アイコン（カーソルモードの構え中のみ）
			GArrowVis = FMath::FInterpTo( GArrowVis, ( Data.bGodArtSelecting && !Data.bFaceButtonSelect ) ? 1.0f : 0.0f, AnimDt, 16.0f );
			if ( GArrowVis > 0.01f )
			{
				const float e = GArrowVis * GArrowVis * ( 3.0f - 2.0f * GArrowVis );	// smoothstep
				const float ArrowR   = IconR * 0.55f;
				const float ArrowGap = 14.0f * ScaleFactor;
				const float SlideDist = 16.0f * ScaleFactor;	// 出現時に外側へスライドする量
				const float LeftCx   = ( CenterX[0] - IconR ) - ArrowGap - ArrowR + ( 1.0f - e ) * SlideDist;
				const float RightCx  = ( CenterX[2] + IconR ) + ArrowGap + ArrowR - ( 1.0f - e ) * SlideDist;
				auto DrawArrowChip = [&]( float cx, bool bLeft )
				{
					const float dr = ArrowR * FMath::Lerp( 0.4f, 1.0f, e );	// スケールイン
					const ImVec2 c( cx, BottomY - dr );	// 下端合わせ
					DrawList->AddCircleFilled( c, dr, ShowCol( TideHudColors::WithAlpha( TideHudColors::DarkPanel, 0.55f * e ) ), 32 );
					DrawList->AddCircle( c, dr, ShowCol( TideHudColors::WithAlpha( NeutralVec, 0.9f * e ) ), 32, FMath::Max( 1.5f, 2.0f * ScaleFactor ) );
					const float s = dr * 0.5f;
					const ImU32 Tri = ShowCol( TideHudColors::WithAlpha( HintVec, e ) );
					if ( bLeft )
						DrawList->AddTriangleFilled( ImVec2( c.x - s, c.y ), ImVec2( c.x + s * 0.6f, c.y - s ), ImVec2( c.x + s * 0.6f, c.y + s ), Tri );
					else
						DrawList->AddTriangleFilled( ImVec2( c.x + s, c.y ), ImVec2( c.x - s * 0.6f, c.y - s ), ImVec2( c.x - s * 0.6f, c.y + s ), Tri );
				};
				DrawArrowChip( LeftCx, true );
				DrawArrowChip( RightCx, false );
			}
		}
	}
}

#endif // !UE_BUILD_SHIPPING
