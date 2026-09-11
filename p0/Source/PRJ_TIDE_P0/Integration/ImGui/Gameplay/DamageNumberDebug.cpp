// Copyright (c) 2026, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.

#include "PRJ_TIDE_P0/Integration/ImGui/Gameplay/DamageNumberDebug.h"

#if !UE_BUILD_SHIPPING

#include "PRJ_TIDE_P0/Integration/ImGui/ImGuiCommon.h"
#include "PRJ_TIDE_P0/Integration/ImGui/TideHudColors.h"
#include "PRJ_TIDE_P0/Core/TideGameSettings.h"

#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

namespace
{
	// --- アニメ・見た目の調整値 ---
	constexpr float BigDamageThreshold = 30.0f;	// この値以上は輪郭を黄にする（被弾者が敵のときのみ。プレイヤーは常に紅）
	constexpr float BaseFontPx         = 39.0f;	// 1080p 基準の通常時フォントサイズ（px）
	constexpr float OutlineThickness   = 2.0f;	// 輪郭の太さ（px・スケール連動）
	constexpr float BigDamageSizeScale = 1.2f;	// 黄（大ダメージ）のときだけ少し大きくする倍率

	constexpr float InDuration    = 0.18f;		// 出現アニメ（大きくなって戻る）の尺（秒）
	constexpr float HoldDuration  = 0.45f;		// 画面固定で保持する尺（秒）
	constexpr float OutDuration   = 0.45f;		// 消滅アニメ（フェード＋上移動）の尺（秒）
	constexpr float TotalLife     = InDuration + HoldDuration + OutDuration;

	constexpr float InStartScale  = 0.3f;		// 出現直後のサイズ
	constexpr float InPeakScale   = 1.6f;		// 出現途中で一番大きくなるサイズ
	constexpr float InGrowPortion = 0.4f;		// InDuration のうち「大きくなる」までの割合（残りで通常へ戻す）

	constexpr float RiseDistance  = 70.0f;		// out アニメで上へ移動する量（px・1080p 基準）
	constexpr int32 MaxPopups     = 64;			// 積みすぎ防止の上限

	struct FDamagePopup
	{
		FVector WorldLocation = FVector::ZeroVector;
		float   Damage = 0.0f;
		bool    bVictimIsPlayer = false;

		float   Age = 0.0f;
		bool    bScreenValid = false;	// スクリーン座標が確定済みか（出現後は画面張り付き）
		FVector2D ScreenPos = FVector2D::ZeroVector;	// 投影済みのビューポート px（原点補正前）
	};

	TArray<FDamagePopup> GPopups;

	// 1080p を基準にした UI スケール（他の ImGui HUD と同方式）
	float GetUIScaleFactor()
	{
		FVector2D ViewportSize( 1920.0, 1080.0 );
		if ( GEngine && GEngine->GameViewport )
		{
			GEngine->GameViewport->GetViewportSize( ViewportSize );
		}
		constexpr float BaseResolutionY = 1080.0f;
		return FMath::Max( (float)ViewportSize.Y / BaseResolutionY, 0.1f );
	}
}

void FDamageNumberDebug::Push( const FVector& WorldLocation, float Damage, bool bVictimIsPlayer )
{
	const UTideGameSettings* Settings = UTideGameSettings::Get();
	if ( !Settings || !Settings->bDebugShowDamageNumbers ) return;
	if ( Damage <= 0.0f ) return;

	FDamagePopup Popup;
	Popup.WorldLocation = WorldLocation;
	Popup.Damage = Damage;
	Popup.bVictimIsPlayer = bVictimIsPlayer;
	GPopups.Add( Popup );

	// 古いものから捨てる
	if ( GPopups.Num() > MaxPopups )
	{
		GPopups.RemoveAt( 0, GPopups.Num() - MaxPopups );
	}
}

void FDamageNumberDebug::Clear()
{
	GPopups.Reset();
}

void FDamageNumberDebug::Render( UWorld* World, float DeltaTime )
{
#if UE_WITH_IMGUI
	const UTideGameSettings* Settings = UTideGameSettings::Get();
	const bool bEnabled = Settings && Settings->bDebugShowDamageNumbers;

	// OFF のときは残ポップも片付ける
	if ( !bEnabled )
	{
		if ( GPopups.Num() > 0 ) GPopups.Reset();
		return;
	}

	if ( GPopups.Num() == 0 ) return;

	APlayerController* PC = World ? UGameplayStatics::GetPlayerController( World, 0 ) : nullptr;

	const float ScaleFactor = GetUIScaleFactor();

	// 投影座標 → ImGui 座標の原点補正（他の HUD と同方式）
	const ImGuiViewport* Viewport = ImGui::GetMainViewport();
	const ImVec2 Origin = Viewport ? Viewport->WorkPos : ImVec2( 0.0f, 0.0f );

	ImDrawList* DrawList = ImGui::GetForegroundDrawList();
	if ( !DrawList ) return;

	ImFont* Font = ImGui::GetFont();

	for ( int32 i = GPopups.Num() - 1; i >= 0; --i )
	{
		FDamagePopup& Popup = GPopups[i];
		Popup.Age += DeltaTime;

		// 出現時に一度だけ投影して以後は固定＝画面張り付き
		if ( !Popup.bScreenValid )
		{
			if ( PC && UGameplayStatics::ProjectWorldToScreen( PC, Popup.WorldLocation, Popup.ScreenPos ) )
			{
				Popup.bScreenValid = true;
			}
			else
			{
				GPopups.RemoveAt( i );	// 画面外・背後などで投影できないものは出さない
				continue;
			}
		}

		if ( Popup.Age >= TotalLife )
		{
			GPopups.RemoveAt( i );
			continue;
		}

		// --- フェーズ別のスケール・アルファ・上移動 ---
		float Scale = 1.0f;
		float Alpha = 1.0f;
		float RiseY = 0.0f;

		if ( Popup.Age < InDuration )
		{
			// in：小さく出て → 大きくなって → 通常サイズへ戻す
			const float t = Popup.Age / InDuration;
			if ( t < InGrowPortion )
			{
				const float u = t / InGrowPortion;
				Scale = FMath::Lerp( InStartScale, InPeakScale, u );
			}
			else
			{
				const float u = ( t - InGrowPortion ) / ( 1.0f - InGrowPortion );
				const float e = 1.0f - ( 1.0f - u ) * ( 1.0f - u );	// ease-out
				Scale = FMath::Lerp( InPeakScale, 1.0f, e );
			}
		}
		else if ( Popup.Age < InDuration + HoldDuration )
		{
			Scale = 1.0f;	// hold
		}
		else
		{
			// out：フェードしながら上へ移動
			const float t = ( Popup.Age - InDuration - HoldDuration ) / OutDuration;
			Alpha = FMath::Clamp( 1.0f - t, 0.0f, 1.0f );
			RiseY = -RiseDistance * ScaleFactor * t;
		}

		const int32 A = FMath::RoundToInt( 255.0f * Alpha );

		// 基調色へ指定アルファを載せて ImU32 化する
		auto ColWithA = []( const ImVec4& C, int32 InA )
		{
			return IM_COL32( (int)( C.x * 255.0f + 0.5f ), (int)( C.y * 255.0f + 0.5f ), (int)( C.z * 255.0f + 0.5f ), InA );
		};

		// 輪郭色で被弾者を区別する（敵は BigDamageThreshold 以上で黄、プレイヤーは常に紅）。
		// 文字色はモチーフカラー ON なら生成り、OFF なら白（他 HUD と同方針）
		const bool bBigDamage = ( !Popup.bVictimIsPlayer && Popup.Damage >= BigDamageThreshold );
		ImU32 OutlineCol;
		if ( Popup.bVictimIsPlayer ) OutlineCol = ColWithA( TideHudColors::DamageNumber::PlayerOutline, A );
		else if ( bBigDamage )       OutlineCol = ColWithA( TideHudColors::DamageNumber::BigOutline, A );
		else                         OutlineCol = ColWithA( TideHudColors::DamageNumber::EnemyOutline, A );
		const ImVec4& TextBase = ( Settings && Settings->bUseTideMotifHudColors )
			? TideHudColors::DamageNumber::TextMotif
			: TideHudColors::DamageNumber::Text;
		const ImU32 TextCol = ColWithA( TextBase, A );

		char Buf[32];
		snprintf( Buf, sizeof( Buf ), "%d", FMath::RoundToInt( Popup.Damage ) );

		const float SizeMul = bBigDamage ? BigDamageSizeScale : 1.0f;
		const float FontSize = BaseFontPx * ScaleFactor * Scale * SizeMul;
		const ImVec2 TextSize = Font->CalcTextSizeA( FontSize, FLT_MAX, 0.0f, Buf );

		const float CenterX = Origin.x + (float)Popup.ScreenPos.X;
		const float CenterY = Origin.y + (float)Popup.ScreenPos.Y + RiseY;
		const ImVec2 Pos( CenterX - TextSize.x * 0.5f, CenterY - TextSize.y * 0.5f );

		// 輪郭は 8 方向へオフセットして描き、その上に文字を載せる
		const float O = FMath::Max( 1.0f, OutlineThickness * ScaleFactor * Scale * SizeMul );
		const ImVec2 Offsets[8] =
		{
			ImVec2( -O, -O ), ImVec2( 0, -O ), ImVec2( O, -O ),
			ImVec2( -O,  0 ),                  ImVec2( O,  0 ),
			ImVec2( -O,  O ), ImVec2( 0,  O ), ImVec2( O,  O ),
		};
		for ( const ImVec2& Off : Offsets )
		{
			DrawList->AddText( Font, FontSize, ImVec2( Pos.x + Off.x, Pos.y + Off.y ), OutlineCol, Buf );
		}
		DrawList->AddText( Font, FontSize, Pos, TextCol, Buf );
	}
#endif // UE_WITH_IMGUI
}

#endif // !UE_BUILD_SHIPPING
