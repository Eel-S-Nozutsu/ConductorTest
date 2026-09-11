// Copyright (c) 2026, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.

#include "InputWindow.h"

#include <format>
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"

#include "PRJ_TIDE_P0/Subsystems/Camera/ExCameraSubsystem.h"
#include "PRJ_TIDE_P0/Components/Camera/ExCameraModeComponent.h"
#include "PRJ_TIDE_P0/Composites/Camera/CameraMode/ExCameraMode.h"

namespace GamepadUIConfig
{
	constexpr float MinScale = 0.5f;
	constexpr float MaxScale = 3.0f;

	constexpr float BaseCanvasWidth = 400.0f;
	constexpr float BaseCanvasHeight = 240.0f;

	constexpr float BgOffsetX = 30.0f;
	constexpr float BgOffsetY = 40.0f;
	constexpr float BgWidth = 340.0f;
	constexpr float BgHeight = 180.0f;
	constexpr float BgRounding = 50.0f;
	constexpr float BgThickness = 3.0f;

	constexpr float TriggerWidth = 60.0f;
	constexpr float TriggerHeight = 12.0f;
	constexpr float TriggerRounding = 4.0f;
	constexpr float TriggerLeftX = 60.0f;
	constexpr float TriggerRightX = 280.0f;
	constexpr float TriggerY = 20.0f;
	constexpr float BumperHeight = 14.0f;
	constexpr float BumperY = 38.0f;

	constexpr float DPadCenterX = 100.0f;
	constexpr float DPadCenterY = 110.0f;
	constexpr float DPadWidth = 12.0f;
	constexpr float DPadHeight = 16.0f;
	constexpr float DPadOffset = 13.0f;
	constexpr float DPadRounding = 2.0f;

	constexpr float FaceBtnCenterX = 300.0f;
	constexpr float FaceBtnCenterY = 110.0f;
	constexpr float FaceBtnRadius = 9.0f;
	constexpr float FaceBtnOffset = 18.0f;

	constexpr float StickLeftX = 150.0f;
	constexpr float StickRightX = 250.0f;
	constexpr float StickY = 160.0f;
	constexpr float StickBgRadius = 24.0f;
	constexpr float StickFgRadius = 12.0f;
	constexpr float StickThickness = 2.0f;

	constexpr float DeadZoneValue = 0.01f;

	const ImU32 ColBg = IM_COL32( 40, 40, 40, 255 );
	const ImU32 ColOutline = IM_COL32( 100, 100, 100, 150 );
	const ImU32 ColPressed = IM_COL32( 50, 255, 50, 255 );
	const ImU32 ColReleased = IM_COL32( 100, 100, 100, 255 );
	const ImU32 ColDarkBg = IM_COL32( 60, 60, 60, 255 );
	const ImU32 ColStickPressed = IM_COL32( 255, 100, 100, 255 );
	const ImU32 ColStickReleased = IM_COL32( 150, 150, 150, 255 );
	constexpr ImVec4 ErrorColor( 1.0f, 0.0f, 0.0f, 1.0f );
}

namespace KeyboardUIConfig
{
	constexpr float BaseCanvasWidth = 420.0f; // キーが増えたため拡張
	constexpr float BaseCanvasHeight = 180.0f;

	constexpr float KeyBaseSize = 32.0f;
	constexpr float KeyHeight = 32.0f;
	constexpr float KeyRounding = 4.0f;
	constexpr float KeyThickness = 2.0f;

	// 行のY座標
	constexpr float Row0_Y = 20.0f;
	constexpr float Row1_Y = 56.0f;
	constexpr float Row2_Y = 92.0f;
	constexpr float Row3_Y = 128.0f;

	// X座標 (キーピッチ36.0fで計算)
	constexpr float Col_Q_X = 40.0f;
	constexpr float Col_W_X = 76.0f;
	constexpr float Col_E_X = 112.0f;
	constexpr float Col_R_X = 148.0f;
	constexpr float Col_T_X = 184.0f;
	constexpr float Col_Y_X = 220.0f;
	constexpr float Col_U_X = 256.0f;
	constexpr float Col_I_X = 292.0f;
	constexpr float Col_O_X = 328.0f;
	constexpr float Col_P_X = 364.0f;

	constexpr float Col_A_X = 46.0f;
	constexpr float Col_S_X = 82.0f;
	constexpr float Col_D_X = 118.0f;
	constexpr float Col_F_X = 154.0f;
	constexpr float Col_G_X = 190.0f;
	constexpr float Col_H_X = 226.0f;
	constexpr float Col_J_X = 262.0f;
	constexpr float Col_K_X = 298.0f;
	constexpr float Col_L_X = 334.0f;

	constexpr float Col_Shift_X = 10.0f;
	constexpr float Shift_Width = 54.0f;
	constexpr float Col_Z_X = 68.0f;
	constexpr float Col_X_X = 104.0f;
	constexpr float Col_C_X = 140.0f;
	constexpr float Col_V_X = 176.0f;
	constexpr float Col_B_X = 212.0f;
	constexpr float Col_N_X = 248.0f;
	constexpr float Col_M_X = 284.0f;

	constexpr float Col_Ctrl_X = 10.0f;
	constexpr float Ctrl_Width = 44.0f;
	constexpr float Col_Space_X = 58.0f;
	constexpr float Space_Width = 150.0f;
	constexpr float Col_Alt_X = 212.0f;
	constexpr float Alt_Width = 44.0f;

	const ImU32 ColBg = IM_COL32( 60, 60, 60, 255 );
	const ImU32 ColPressed = IM_COL32( 50, 255, 50, 255 );
	const ImU32 ColReleased = IM_COL32( 100, 100, 100, 255 );
	const ImU32 ColText = IM_COL32( 255, 255, 255, 255 );
}

namespace MouseUIConfig
{
	constexpr float BaseCanvasWidth = 200.0f;
	constexpr float BaseCanvasHeight = 220.0f;

	constexpr float BodyOffsetX = 50.0f;
	constexpr float BodyOffsetY = 30.0f;
	constexpr float BodyWidth = 80.0f;
	constexpr float BodyHeight = 130.0f;
	constexpr float BodyRounding = 35.0f;
	constexpr float BodyThickness = 3.0f;

	constexpr float ButtonHeight = 45.0f;
	constexpr float ButtonSpacing = 2.0f;

	constexpr float WheelWidth = 12.0f;
	constexpr float WheelHeight = 24.0f;
	constexpr float WheelOffsetY = 10.0f;
	constexpr float WheelRounding = 6.0f;

	constexpr float SideBtnWidth = 8.0f;
	constexpr float SideBtnHeight = 20.0f;
	constexpr float SideBtnOffset = -4.0f;
	constexpr float SideBtn1Y = 50.0f;
	constexpr float SideBtn2Y = 75.0f;
	constexpr float SideBtnRounding = 2.0f;

	const ImU32 ColBg = IM_COL32( 40, 40, 40, 255 );
	const ImU32 ColOutline = IM_COL32( 100, 100, 100, 150 );
	const ImU32 ColPressed = IM_COL32( 50, 255, 50, 255 );
	const ImU32 ColReleased = IM_COL32( 100, 100, 100, 255 );
}

#if !UE_BUILD_SHIPPING

void InputWindow::DrawContents()
{
	if ( !GameInstance ) return;

	ULocalPlayer* LocalPlayer = GameInstance->GetFirstGamePlayer();
	if ( !LocalPlayer ) return;

	APlayerController* PC = LocalPlayer->GetPlayerController( GameInstance->GetWorld() );
	if ( !PC )
	{
		ImGui::TextColored( GamepadUIConfig::ErrorColor, "PlayerControllerが取得できません" );
		return;
	}

	ImGui::SliderFloat( "UI Scale", &UIScale, GamepadUIConfig::MinScale, GamepadUIConfig::MaxScale, "%.2f" );

	// --- ゲームパッド入力状態 ---
	if ( ImGui::CollapsingHeader( "パッド入力状態", ImGuiTreeNodeFlags_DefaultOpen ) )
	{
		const ImVec2 P = ImGui::GetCursorScreenPos();
		ImDrawList* DrawList = ImGui::GetWindowDrawList();

		const float ScaledCanvasWidth = GamepadUIConfig::BaseCanvasWidth * UIScale;
		const float ScaledCanvasHeight = GamepadUIConfig::BaseCanvasHeight * UIScale;
		ImGui::Dummy( ImVec2( ScaledCanvasWidth, ScaledCanvasHeight ) );

		auto GetCol = []( bool bPressed ) { return bPressed ? GamepadUIConfig::ColPressed : GamepadUIConfig::ColReleased; };

		const ImVec2 BgMin( P.x + ( GamepadUIConfig::BgOffsetX * UIScale ), P.y + ( GamepadUIConfig::BgOffsetY * UIScale ) );
		const ImVec2 BgMax( BgMin.x + ( GamepadUIConfig::BgWidth * UIScale ), BgMin.y + ( GamepadUIConfig::BgHeight * UIScale ) );
		const float BgRounding = GamepadUIConfig::BgRounding * UIScale;
		const float BgThickness = GamepadUIConfig::BgThickness * UIScale;

		DrawList->AddRectFilled( BgMin, BgMax, GamepadUIConfig::ColBg, BgRounding );
		DrawList->AddRect( BgMin, BgMax, GamepadUIConfig::ColOutline, BgRounding, 0, BgThickness );

		auto DrawTrigger = [&]( const ImVec2& BasePos, float Value )
			{
				const ImVec2 TopLeft( P.x + ( BasePos.x * UIScale ), P.y + ( BasePos.y * UIScale ) );
				const float W = GamepadUIConfig::TriggerWidth * UIScale;
				const float H = GamepadUIConfig::TriggerHeight * UIScale;
				const float R = GamepadUIConfig::TriggerRounding * UIScale;

				DrawList->AddRectFilled( TopLeft, ImVec2( TopLeft.x + W, TopLeft.y + H ), GamepadUIConfig::ColDarkBg, R );
				if ( Value > GamepadUIConfig::DeadZoneValue )
				{
					DrawList->AddRectFilled( TopLeft, ImVec2( TopLeft.x + ( W * Value ), TopLeft.y + H ), GamepadUIConfig::ColPressed, R );
				}
			};

		auto DrawBumper = [&]( const ImVec2& BasePos, bool bPressed )
			{
				const ImVec2 TopLeft( P.x + ( BasePos.x * UIScale ), P.y + ( BasePos.y * UIScale ) );
				const float W = GamepadUIConfig::TriggerWidth * UIScale;
				const float H = GamepadUIConfig::BumperHeight * UIScale;
				const float R = GamepadUIConfig::TriggerRounding * UIScale;

				DrawList->AddRectFilled( TopLeft, ImVec2( TopLeft.x + W, TopLeft.y + H ), GetCol( bPressed ), R );
			};

		auto DrawDPad = [&]( const ImVec2& BaseCenter, bool U, bool D, bool L, bool R )
			{
				const ImVec2 Center( P.x + ( BaseCenter.x * UIScale ), P.y + ( BaseCenter.y * UIScale ) );
				const float W = GamepadUIConfig::DPadWidth * UIScale;
				const float H = GamepadUIConfig::DPadHeight * UIScale;
				const float Offset = GamepadUIConfig::DPadOffset * UIScale;
				const float Rounding = GamepadUIConfig::DPadRounding * UIScale;
				const float HalfW = W * 0.5f;

				DrawList->AddRectFilled( ImVec2( Center.x - HalfW, Center.y - Offset - H ), ImVec2( Center.x + HalfW, Center.y - Offset ), GetCol( U ), Rounding );
				DrawList->AddRectFilled( ImVec2( Center.x - HalfW, Center.y + Offset ), ImVec2( Center.x + HalfW, Center.y + Offset + H ), GetCol( D ), Rounding );
				DrawList->AddRectFilled( ImVec2( Center.x - Offset - H, Center.y - HalfW ), ImVec2( Center.x - Offset, Center.y + HalfW ), GetCol( L ), Rounding );
				DrawList->AddRectFilled( ImVec2( Center.x + Offset, Center.y - HalfW ), ImVec2( Center.x + Offset + H, Center.y + HalfW ), GetCol( R ), Rounding );
				DrawList->AddRectFilled( ImVec2( Center.x - HalfW, Center.y - HalfW ), ImVec2( Center.x + HalfW, Center.y + HalfW ), GamepadUIConfig::ColReleased );
			};

		auto DrawFaceButtons = [&]( const ImVec2& BaseCenter, bool Top, bool Bot, bool Left, bool Right )
			{
				const ImVec2 Center( P.x + ( BaseCenter.x * UIScale ), P.y + ( BaseCenter.y * UIScale ) );
				const float Radius = GamepadUIConfig::FaceBtnRadius * UIScale;
				const float Offset = GamepadUIConfig::FaceBtnOffset * UIScale;

				DrawList->AddCircleFilled( ImVec2( Center.x, Center.y - Offset ), Radius, GetCol( Top ) );
				DrawList->AddCircleFilled( ImVec2( Center.x, Center.y + Offset ), Radius, GetCol( Bot ) );
				DrawList->AddCircleFilled( ImVec2( Center.x - Offset, Center.y ), Radius, GetCol( Left ) );
				DrawList->AddCircleFilled( ImVec2( Center.x + Offset, Center.y ), Radius, GetCol( Right ) );
			};

		auto DrawStick = [&]( const ImVec2& BaseCenter, float X, float Y, bool bClicked )
			{
				const ImVec2 Center( P.x + ( BaseCenter.x * UIScale ), P.y + ( BaseCenter.y * UIScale ) );
				const float BgRadius = GamepadUIConfig::StickBgRadius * UIScale;
				const float FgRadius = GamepadUIConfig::StickFgRadius * UIScale;
				const float Thickness = GamepadUIConfig::StickThickness * UIScale;

				DrawList->AddCircleFilled( Center, BgRadius, GamepadUIConfig::ColDarkBg );
				DrawList->AddCircle( Center, BgRadius, GamepadUIConfig::ColReleased, 0, Thickness );

				const ImVec2 Marker( Center.x + ( X * BgRadius ), Center.y - ( Y * BgRadius ) );
				const ImU32 StickColor = bClicked ? GamepadUIConfig::ColStickPressed : GamepadUIConfig::ColStickReleased;

				DrawList->AddCircleFilled( Marker, FgRadius, StickColor );
			};

		const float LTrigger = PC->GetInputAnalogKeyState( EKeys::Gamepad_LeftTrigger );
		const float RTrigger = PC->GetInputAnalogKeyState( EKeys::Gamepad_RightTrigger );
		DrawTrigger( ImVec2( GamepadUIConfig::TriggerLeftX, GamepadUIConfig::TriggerY ), LTrigger );
		DrawTrigger( ImVec2( GamepadUIConfig::TriggerRightX, GamepadUIConfig::TriggerY ), RTrigger );

		DrawBumper( ImVec2( GamepadUIConfig::TriggerLeftX, GamepadUIConfig::BumperY ), PC->IsInputKeyDown( EKeys::Gamepad_LeftShoulder ) );
		DrawBumper( ImVec2( GamepadUIConfig::TriggerRightX, GamepadUIConfig::BumperY ), PC->IsInputKeyDown( EKeys::Gamepad_RightShoulder ) );

		DrawDPad( ImVec2( GamepadUIConfig::DPadCenterX, GamepadUIConfig::DPadCenterY ),
			PC->IsInputKeyDown( EKeys::Gamepad_DPad_Up ),
			PC->IsInputKeyDown( EKeys::Gamepad_DPad_Down ),
			PC->IsInputKeyDown( EKeys::Gamepad_DPad_Left ),
			PC->IsInputKeyDown( EKeys::Gamepad_DPad_Right ) );

		DrawFaceButtons( ImVec2( GamepadUIConfig::FaceBtnCenterX, GamepadUIConfig::FaceBtnCenterY ),
			PC->IsInputKeyDown( EKeys::Gamepad_FaceButton_Top ),
			PC->IsInputKeyDown( EKeys::Gamepad_FaceButton_Bottom ),
			PC->IsInputKeyDown( EKeys::Gamepad_FaceButton_Left ),
			PC->IsInputKeyDown( EKeys::Gamepad_FaceButton_Right ) );

		const float L_X = PC->GetInputAnalogKeyState( EKeys::Gamepad_LeftX );
		const float L_Y = PC->GetInputAnalogKeyState( EKeys::Gamepad_LeftY );
		const float R_X = PC->GetInputAnalogKeyState( EKeys::Gamepad_RightX );
		const float R_Y = PC->GetInputAnalogKeyState( EKeys::Gamepad_RightY ) * -1.0f;

		DrawStick( ImVec2( GamepadUIConfig::StickLeftX, GamepadUIConfig::StickY ), L_X, L_Y, PC->IsInputKeyDown( EKeys::Gamepad_LeftThumbstick ) );
		DrawStick( ImVec2( GamepadUIConfig::StickRightX, GamepadUIConfig::StickY ), R_X, R_Y, PC->IsInputKeyDown( EKeys::Gamepad_RightThumbstick ) );
	}

	// --- キーボード＆マウス入力状態 ---
	if ( ImGui::CollapsingHeader( "キーボード＆マウス入力状態", ImGuiTreeNodeFlags_DefaultOpen ) )
	{
		const ImVec2 P = ImGui::GetCursorScreenPos();
		ImDrawList* DrawList = ImGui::GetWindowDrawList();

		// キャンバスサイズを合算 (幅はキーボード＋マウス、高さは高い方に合わせる)
		const float CombinedCanvasWidth = ( KeyboardUIConfig::BaseCanvasWidth + MouseUIConfig::BaseCanvasWidth ) * UIScale;
		const float CombinedCanvasHeight = ( MouseUIConfig::BaseCanvasHeight > KeyboardUIConfig::BaseCanvasHeight ? MouseUIConfig::BaseCanvasHeight : KeyboardUIConfig::BaseCanvasHeight ) * UIScale;
		ImGui::Dummy( ImVec2( CombinedCanvasWidth, CombinedCanvasHeight ) );

		// --- キーボード描画 ---
		auto DrawKey = [&]( float BaseX, float BaseY, float BaseWidth, const char* Label, const FKey& Key )
			{
				const bool bPressed = PC->IsInputKeyDown( Key );

				const ImVec2 TopLeft( P.x + ( BaseX * UIScale ), P.y + ( BaseY * UIScale ) );
				const float ScaledW = BaseWidth * UIScale;
				const float ScaledH = KeyboardUIConfig::KeyHeight * UIScale;
				const float Rounding = KeyboardUIConfig::KeyRounding * UIScale;
				const float Thickness = KeyboardUIConfig::KeyThickness * UIScale;

				const ImU32 FillColor = bPressed ? KeyboardUIConfig::ColPressed : KeyboardUIConfig::ColBg;

				DrawList->AddRectFilled( TopLeft, ImVec2( TopLeft.x + ScaledW, TopLeft.y + ScaledH ), FillColor, Rounding );
				DrawList->AddRect( TopLeft, ImVec2( TopLeft.x + ScaledW, TopLeft.y + ScaledH ), KeyboardUIConfig::ColReleased, Rounding, 0, Thickness );

				const ImVec2 TextSize = ImGui::CalcTextSize( Label );
				const ImVec2 TextPos(
					TopLeft.x + ( ScaledW - TextSize.x ) * 0.5f,
					TopLeft.y + ( ScaledH - TextSize.y ) * 0.5f
				);
				DrawList->AddText( TextPos, KeyboardUIConfig::ColText, Label );
			};

		// Row 0
		DrawKey( KeyboardUIConfig::Col_Q_X, KeyboardUIConfig::Row0_Y, KeyboardUIConfig::KeyBaseSize, "Q", EKeys::Q );
		DrawKey( KeyboardUIConfig::Col_W_X, KeyboardUIConfig::Row0_Y, KeyboardUIConfig::KeyBaseSize, "W", EKeys::W );
		DrawKey( KeyboardUIConfig::Col_E_X, KeyboardUIConfig::Row0_Y, KeyboardUIConfig::KeyBaseSize, "E", EKeys::E );
		DrawKey( KeyboardUIConfig::Col_R_X, KeyboardUIConfig::Row0_Y, KeyboardUIConfig::KeyBaseSize, "R", EKeys::R );
		DrawKey( KeyboardUIConfig::Col_T_X, KeyboardUIConfig::Row0_Y, KeyboardUIConfig::KeyBaseSize, "T", EKeys::T );
		DrawKey( KeyboardUIConfig::Col_Y_X, KeyboardUIConfig::Row0_Y, KeyboardUIConfig::KeyBaseSize, "Y", EKeys::Y );
		DrawKey( KeyboardUIConfig::Col_U_X, KeyboardUIConfig::Row0_Y, KeyboardUIConfig::KeyBaseSize, "U", EKeys::U );
		DrawKey( KeyboardUIConfig::Col_I_X, KeyboardUIConfig::Row0_Y, KeyboardUIConfig::KeyBaseSize, "I", EKeys::I );
		DrawKey( KeyboardUIConfig::Col_O_X, KeyboardUIConfig::Row0_Y, KeyboardUIConfig::KeyBaseSize, "O", EKeys::O );
		DrawKey( KeyboardUIConfig::Col_P_X, KeyboardUIConfig::Row0_Y, KeyboardUIConfig::KeyBaseSize, "P", EKeys::P );

		// Row 1
		DrawKey( KeyboardUIConfig::Col_A_X, KeyboardUIConfig::Row1_Y, KeyboardUIConfig::KeyBaseSize, "A", EKeys::A );
		DrawKey( KeyboardUIConfig::Col_S_X, KeyboardUIConfig::Row1_Y, KeyboardUIConfig::KeyBaseSize, "S", EKeys::S );
		DrawKey( KeyboardUIConfig::Col_D_X, KeyboardUIConfig::Row1_Y, KeyboardUIConfig::KeyBaseSize, "D", EKeys::D );
		DrawKey( KeyboardUIConfig::Col_F_X, KeyboardUIConfig::Row1_Y, KeyboardUIConfig::KeyBaseSize, "F", EKeys::F );
		DrawKey( KeyboardUIConfig::Col_G_X, KeyboardUIConfig::Row1_Y, KeyboardUIConfig::KeyBaseSize, "G", EKeys::G );
		DrawKey( KeyboardUIConfig::Col_H_X, KeyboardUIConfig::Row1_Y, KeyboardUIConfig::KeyBaseSize, "H", EKeys::H );
		DrawKey( KeyboardUIConfig::Col_J_X, KeyboardUIConfig::Row1_Y, KeyboardUIConfig::KeyBaseSize, "J", EKeys::J );
		DrawKey( KeyboardUIConfig::Col_K_X, KeyboardUIConfig::Row1_Y, KeyboardUIConfig::KeyBaseSize, "K", EKeys::K );
		DrawKey( KeyboardUIConfig::Col_L_X, KeyboardUIConfig::Row1_Y, KeyboardUIConfig::KeyBaseSize, "L", EKeys::L );

		// Row 2
		DrawKey( KeyboardUIConfig::Col_Shift_X, KeyboardUIConfig::Row2_Y, KeyboardUIConfig::Shift_Width, "Shift", EKeys::LeftShift );
		DrawKey( KeyboardUIConfig::Col_Z_X, KeyboardUIConfig::Row2_Y, KeyboardUIConfig::KeyBaseSize, "Z", EKeys::Z );
		DrawKey( KeyboardUIConfig::Col_X_X, KeyboardUIConfig::Row2_Y, KeyboardUIConfig::KeyBaseSize, "X", EKeys::X );
		DrawKey( KeyboardUIConfig::Col_C_X, KeyboardUIConfig::Row2_Y, KeyboardUIConfig::KeyBaseSize, "C", EKeys::C );
		DrawKey( KeyboardUIConfig::Col_V_X, KeyboardUIConfig::Row2_Y, KeyboardUIConfig::KeyBaseSize, "V", EKeys::V );
		DrawKey( KeyboardUIConfig::Col_B_X, KeyboardUIConfig::Row2_Y, KeyboardUIConfig::KeyBaseSize, "B", EKeys::B );
		DrawKey( KeyboardUIConfig::Col_N_X, KeyboardUIConfig::Row2_Y, KeyboardUIConfig::KeyBaseSize, "N", EKeys::N );
		DrawKey( KeyboardUIConfig::Col_M_X, KeyboardUIConfig::Row2_Y, KeyboardUIConfig::KeyBaseSize, "M", EKeys::M );

		// Row 3
		DrawKey( KeyboardUIConfig::Col_Ctrl_X, KeyboardUIConfig::Row3_Y, KeyboardUIConfig::Ctrl_Width, "Ctrl", EKeys::LeftControl );
		DrawKey( KeyboardUIConfig::Col_Space_X, KeyboardUIConfig::Row3_Y, KeyboardUIConfig::Space_Width, "Space", EKeys::SpaceBar );
		DrawKey( KeyboardUIConfig::Col_Alt_X, KeyboardUIConfig::Row3_Y, KeyboardUIConfig::Alt_Width, "Alt", EKeys::LeftAlt );

		// --- マウス描画（キーボードの右側にオフセット）---
		const float MouseBaseX = P.x + ( KeyboardUIConfig::BaseCanvasWidth * UIScale );

		auto GetMouseCol = []( bool bPressed ) { return bPressed ? MouseUIConfig::ColPressed : MouseUIConfig::ColReleased; };

		const float BodyX = MouseBaseX + ( MouseUIConfig::BodyOffsetX * UIScale );
		const float BodyY = P.y + ( MouseUIConfig::BodyOffsetY * UIScale );
		const float BodyW = MouseUIConfig::BodyWidth * UIScale;
		const float BodyH = MouseUIConfig::BodyHeight * UIScale;
		const float BodyR = MouseUIConfig::BodyRounding * UIScale;
		const float Thick = MouseUIConfig::BodyThickness * UIScale;

		// マウス本体 (背景とアウトライン)
		DrawList->AddRectFilled( ImVec2( BodyX, BodyY ), ImVec2( BodyX + BodyW, BodyY + BodyH ), MouseUIConfig::ColBg, BodyR );
		DrawList->AddRect( ImVec2( BodyX, BodyY ), ImVec2( BodyX + BodyW, BodyY + BodyH ), MouseUIConfig::ColOutline, BodyR, 0, Thick );

		const float Space = MouseUIConfig::ButtonSpacing * UIScale;
		const float BtnW = ( BodyW * 0.5f ) - Space * 1.5f;
		const float BtnH = MouseUIConfig::ButtonHeight * UIScale;
		const float BtnY = BodyY + Space;
		const float BtnR = BodyR * 0.5f;

		// 左ボタン
		const float LBtnX = BodyX + Space;
		const bool  bLeft = PC->IsInputKeyDown( EKeys::LeftMouseButton );
		DrawList->AddRectFilled( ImVec2( LBtnX, BtnY ), ImVec2( LBtnX + BtnW, BtnY + BtnH ), GetMouseCol( bLeft ), BtnR );

		// 右ボタン
		const float RBtnX = BodyX + ( BodyW * 0.5f ) + Space * 0.5f;
		const bool  bRight = PC->IsInputKeyDown( EKeys::RightMouseButton );
		DrawList->AddRectFilled( ImVec2( RBtnX, BtnY ), ImVec2( RBtnX + BtnW, BtnY + BtnH ), GetMouseCol( bRight ), BtnR );

		// ホイール (中ボタン)
		const float WheelW = MouseUIConfig::WheelWidth * UIScale;
		const float WheelH = MouseUIConfig::WheelHeight * UIScale;
		const float WheelX = BodyX + ( BodyW * 0.5f ) - ( WheelW * 0.5f );
		const float WheelY = BodyY + ( MouseUIConfig::WheelOffsetY * UIScale );
		const float WheelR = MouseUIConfig::WheelRounding * UIScale;
		const bool  bMiddle = PC->IsInputKeyDown( EKeys::MiddleMouseButton );
		DrawList->AddRectFilled( ImVec2( WheelX, WheelY ), ImVec2( WheelX + WheelW, WheelY + WheelH ), GetMouseCol( bMiddle ), WheelR );

		// サイドボタン
		const float SideW = MouseUIConfig::SideBtnWidth * UIScale;
		const float SideH = MouseUIConfig::SideBtnHeight * UIScale;
		const float SideX = BodyX + ( MouseUIConfig::SideBtnOffset * UIScale );
		const float Side1Y = BodyY + ( MouseUIConfig::SideBtn1Y * UIScale );
		const float Side2Y = BodyY + ( MouseUIConfig::SideBtn2Y * UIScale );
		const float SideR = MouseUIConfig::SideBtnRounding * UIScale;
		const bool  bThumb2 = PC->IsInputKeyDown( EKeys::ThumbMouseButton2 );
		const bool  bThumb1 = PC->IsInputKeyDown( EKeys::ThumbMouseButton );

		DrawList->AddRectFilled( ImVec2( SideX, Side1Y ), ImVec2( SideX + SideW, Side1Y + SideH ), GetMouseCol( bThumb2 ), SideR );
		DrawList->AddRectFilled( ImVec2( SideX, Side2Y ), ImVec2( SideX + SideW, Side2Y + SideH ), GetMouseCol( bThumb1 ), SideR );

		// 座標テキスト出力
		float MousePosX = 0.0f;
		float MousePosY = 0.0f;
		PC->GetMousePosition( MousePosX, MousePosY );

		ImGui::Separator();
		ImGui::Text( "Mouse Position: X = %7.2f, Y = %7.2f", MousePosX, MousePosY );
	}
}

#endif // !UE_BUILD_SHIPPING
