// Copyright (c) 2026, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.

#include "CameraWindow.h"

#include <format>
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"

#include "PRJ_TIDE_P0/Subsystems/Camera/ExCameraSubsystem.h"
#include "PRJ_TIDE_P0/Components/Camera/ExCameraModeComponent.h"
#include "PRJ_TIDE_P0/Composites/Camera/CameraMode/ExCameraMode.h"

#if !UE_BUILD_SHIPPING

void CameraWindow::DrawContents()
{
	if ( !GameInstance ) return;

	// LocalPlayer の取得
	ULocalPlayer* LocalPlayer = GameInstance->GetFirstGamePlayer();
	if ( !LocalPlayer ) return;

	UExCameraSubsystem* CameraSubsystem = LocalPlayer->GetSubsystem<UExCameraSubsystem>();
	if ( !CameraSubsystem ) return;

	UExCameraModeComponent* CameraComp = CameraSubsystem->GetCameraModeComponent();
	if ( !CameraComp )
	{
		ImGui::TextColored( ImVec4( 1.0f, 0.0f, 0.0f, 1.0f ), "カメラコンポーネントが登録されていません。" );
		return;
	}

	// --- 1. カメラスタックの状態表示 ---
	if ( ImGui::CollapsingHeader( "モードスタック", ImGuiTreeNodeFlags_DefaultOpen ) )
	{
		const TArray<TObjectPtr<UExCameraMode>>& ModeStack = CameraComp->GetModeStack();

		ImGui::Text( "スタック数: %d", ModeStack.Num() );
		ImGui::Separator();

		// アクティブは最上位固定ではなく Priority で決まる（同値は後積み優先）
		const int32 ActiveIndex = CameraComp->GetActiveModeIndex();

		for ( int32 i = ModeStack.Num() - 1; i >= 0; --i )
		{
			if ( UExCameraMode* Mode = ModeStack[i] )
			{
				const bool bIsActive = ( i == ActiveIndex );
				const ImVec4 TextColor = bIsActive ? ImVec4( 0.2f, 1.0f, 0.2f, 1.0f ) : ImVec4( 0.6f, 0.6f, 0.6f, 1.0f );

				// 修正: UEのTCHARをImGui用にUTF-8へ変換
				ImGui::TextColored( TextColor, "[%d]%s %s (ID: %d, Pri: %d)",
					i,
					bIsActive ? " *" : "  ",
					TCHAR_TO_UTF8( *Mode->GetName() ),
					Mode->InstanceID,
					Mode->GetPriority() );
			}
		}
	}

	// --- 2. ブレンド（補間）の状態表示 ---
	if ( ImGui::CollapsingHeader( "ブレンド状態", ImGuiTreeNodeFlags_DefaultOpen ) )
	{
		if ( CameraComp->IsBlending() )
		{
			const float Elapsed = CameraComp->GetBlendTimeElapsed();
			const float Total = CameraComp->GetCurrentBlendTime();
			const float Ratio = ( Total > 0.0f ) ? ( Elapsed / Total ) : 1.0f;

			ImGui::TextColored( ImVec4( 1.0f, 0.6f, 0.0f, 1.0f ), "ブレンド実行中... (%.2f / %.2f)", Elapsed, Total );
			ImGui::ProgressBar( Ratio, ImVec2( -1.0f, 0.0f ) );
		}
		else
		{
			ImGui::TextColored( ImVec4( 0.5f, 0.5f, 0.5f, 1.0f ), "ブレンド待機中" );
		}
	}

	// --- 3. 最終出力（ViewInfo）の確認 ---
	if ( ImGui::CollapsingHeader( "最終出力 (ViewInfo)", ImGuiTreeNodeFlags_DefaultOpen ) )
	{
		const FMinimalViewInfo& ViewInfo = CameraComp->GetLastFrameViewInfo();

		ImGui::Text( "位置: X: %7.1f  Y: %7.1f  Z: %7.1f", ViewInfo.Location.X, ViewInfo.Location.Y, ViewInfo.Location.Z );
		ImGui::Text( "回転: P: %7.1f  Y: %7.1f  R: %7.1f", ViewInfo.Rotation.Pitch, ViewInfo.Rotation.Yaw, ViewInfo.Rotation.Roll );
		ImGui::Text( "画角: %7.1f", ViewInfo.FOV );
	}

	// --- 4. デバッグ操作（手動での Push/Pop）---
	if ( ImGui::CollapsingHeader( "デバッグ操作", ImGuiTreeNodeFlags_DefaultOpen ) )
	{
		// 文字列の入力フィールド
		ImGui::InputText( "追加行名", InputRowName, IM_ARRAYSIZE( InputRowName ) );
		ImGui::InputText( "ブレンド行名", InputBlendRowName, IM_ARRAYSIZE( InputBlendRowName ) );

		// Pushボタン
		if ( ImGui::Button( "カメラモードをPush" ) )
		{
			// ImGuiのUTF-8文字列をUEのFNameに変換
			const FName RowName( UTF8_TO_TCHAR( InputRowName ) );
			const FName BlendRowName( UTF8_TO_TCHAR( InputBlendRowName ) );

			FCameraModeHandle NewHandle = CameraSubsystem->PushThirdPersonCameraByKey( RowName, BlendRowName );
			if ( NewHandle.IsValid() )
			{
				DebugPushedHandles.Add( NewHandle );
			}
		}

		ImGui::Separator();
		ImGui::Text( "Push済みハンドル:" );

		// Pushしたモードのリストと、個別のPopボタンを表示
		for ( int32 i = 0; i < DebugPushedHandles.Num(); ++i )
		{
			// ImGuiで同じ名前のボタンを複数並べる場合はIDのプッシュが必要
			ImGui::PushID( i );

			ImGui::Text( "ハンドルID: %d", DebugPushedHandles[i].ID );
			ImGui::SameLine();

			if ( ImGui::Button( "Pop（削除）" ) )
			{
				// Pop時のブレンドにも、現在入力されている BlendRowName を使用する
				const FName BlendRowName( UTF8_TO_TCHAR( InputBlendRowName ) );

				CameraSubsystem->PopCameraMode( DebugPushedHandles[i], BlendRowName );
				DebugPushedHandles.RemoveAt( i );
				--i; // 要素を削除したのでインデックスを調整
			}

			ImGui::PopID();
		}
	}
}

#endif // !UE_BUILD_SHIPPING
