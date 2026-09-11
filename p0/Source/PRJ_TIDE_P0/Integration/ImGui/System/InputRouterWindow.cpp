// Copyright (c) 2026, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.

#include "InputRouterWindow.h"

#if !UE_BUILD_SHIPPING

#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"
#include "InputMappingContext.h"

#include "PRJ_TIDE_P0/Components/Input/InputRouterComponent.h"
#include "PRJ_TIDE_P0/Data/Input/MappingContextData.h"
#include "PRJ_TIDE_P0/Data/Input/InputActionListDataAsset.h"

namespace InputRouterWindowColors
{
	// UE の ::Error / ::Warn 等と衝突しないよう Col プレフィックスを付ける
	constexpr ImVec4 ColOk    ( 0.40f, 1.00f, 0.40f, 1.0f ); // 緑：正常/Active
	constexpr ImVec4 ColWarn  ( 1.00f, 0.80f, 0.20f, 1.0f ); // 黄：非Active（設定ミスの可能性）
	constexpr ImVec4 ColError ( 1.00f, 0.35f, 0.35f, 1.0f ); // 赤：無効/未設定
	constexpr ImVec4 ColDim   ( 0.65f, 0.65f, 0.65f, 1.0f ); // 灰：補足
}

void InputRouterWindow::DrawContents()
{
	using namespace InputRouterWindowColors;

	if ( !GameInstance )
	{
		ImGui::TextColored( ColError, "GameInstance が取得できません" );
		return;
	}

	ULocalPlayer* LocalPlayer = GameInstance->GetFirstGamePlayer();
	APlayerController* PC = LocalPlayer ? LocalPlayer->GetPlayerController( GameInstance->GetWorld() ) : nullptr;
	if ( !PC )
	{
		ImGui::TextColored( ColError, "PlayerController が取得できません" );
		return;
	}

	UInputRouterComponent* Router = PC->FindComponentByClass<UInputRouterComponent>();
	if ( !Router )
	{
		ImGui::TextColored( ColError, "InputRouterComponent が見つかりません" );
		return;
	}

	const TArray<FGameplayTag>& ActiveTags = Router->GetActiveLayerTags();

	// --- アクティブレイヤー ---
	if ( ImGui::CollapsingHeader( "アクティブレイヤー (ActiveLayerTags)", ImGuiTreeNodeFlags_DefaultOpen ) )
	{
		if ( ActiveTags.Num() == 0 )
		{
			ImGui::TextColored( ColWarn, "アクティブなレイヤーがありません（どのIMCも追加されていない状態）" );
		}
		else
		{
			for ( const FGameplayTag& Tag : ActiveTags )
			{
				ImGui::BulletText( "%s", TCHAR_TO_UTF8( *Tag.ToString() ) );
			}
		}
	}

	// --- MappingContextList（登録IMCと現在の適用状況）---
	if ( ImGui::CollapsingHeader( "MappingContextList", ImGuiTreeNodeFlags_DefaultOpen ) )
	{
		if ( Router->MappingContextList.Num() == 0 )
		{
			ImGui::TextColored( ColError, "MappingContextList が空です" );
		}

		for ( int32 i = 0; i < Router->MappingContextList.Num(); ++i )
		{
			const FMappingContextData& Entry = Router->MappingContextList[ i ];

			const bool bHasMC     = ( Entry.MappingContext != nullptr );
			const bool bTagValid  = Entry.LayerTag.IsValid();
			const bool bTagActive = bTagValid && ActiveTags.Contains( Entry.LayerTag );
			const bool bContributes = bHasMC && bTagValid && bTagActive; // UpdateMappingContexts と同条件

			const FString MCName  = bHasMC ? Entry.MappingContext->GetName() : FString( TEXT( "<None>" ) );
			const FString ALName  = Entry.ActionList ? Entry.ActionList->GetName() : FString( TEXT( "<None>" ) );
			const FString TagName = bTagValid ? Entry.LayerTag.ToString() : FString( TEXT( "<None>" ) );

			// 見出し（左に状態バッジ）
			const ImVec4 StateCol = bContributes ? ColOk : ( bHasMC && bTagValid ? ColWarn : ColError );
			const char* StateStr  = bContributes ? "[ACTIVE]" : ( bHasMC && bTagValid ? "[inactive]" : "[invalid]" );

			ImGui::PushID( i );
			ImGui::TextColored( StateCol, "%s", StateStr );
			ImGui::SameLine();
			const bool bOpen = ImGui::TreeNodeEx( "node", ImGuiTreeNodeFlags_SpanAvailWidth,
				"[%d] IMC=%s  Layer=%s  Pri=%d  AutoActivate=%s",
				i,
				TCHAR_TO_UTF8( *MCName ),
				TCHAR_TO_UTF8( *TagName ),
				Entry.Priority,
				Entry.bAutoActivate ? "true" : "false" );

			if ( bOpen )
			{
				ImGui::Text( "ActionList : %s", TCHAR_TO_UTF8( *ALName ) );

				// 非Active/無効の理由をヒント表示
				if ( !bHasMC )    ImGui::TextColored( ColError, "→ MappingContext 未設定" );
				if ( !bTagValid ) ImGui::TextColored( ColError, "→ LayerTag が無効（PushLayer が早期returnし、絶対にActiveにならない）" );
				if ( bHasMC && bTagValid && !bTagActive )
				{
					ImGui::TextColored( ColWarn, "→ このレイヤーは未Active。bAutoActivate=true にするか PushLayerByTag で有効化が必要" );
				}

				// ActionList 内の IA 一覧（ここに入っていない IA は Bind されない）
				if ( Entry.ActionList )
				{
					if ( ImGui::TreeNode( "Actions (ActionList内)" ) )
					{
						if ( Entry.ActionList->Actions.Num() == 0 )
						{
							ImGui::TextColored( ColWarn, "（空）" );
						}
						for ( const TObjectPtr<UInputAction>& IA : Entry.ActionList->Actions )
						{
							if ( IA )
							{
								ImGui::BulletText( "%s", TCHAR_TO_UTF8( *IA->GetName() ) );
							}
							else
							{
								ImGui::TextColored( ColError, "・<null IA>" );
							}
						}
						ImGui::TreePop();
					}
				}
				ImGui::TreePop();
			}
			ImGui::PopID();
		}
	}

	// --- Bind済みIA（BeginPlay で全コンテキストの ActionList から収集した固有アクション）---
	if ( ImGui::CollapsingHeader( "Bind済みIA (UniqueActions)", ImGuiTreeNodeFlags_DefaultOpen ) )
	{
		ImGui::TextColored( ColDim, "ここに無いIAは、どのActionListにも入っていない＝Bindされず絶対に発火しません" );
		ImGui::InputText( "絞り込み", ActionFilter, IM_ARRAYSIZE( ActionFilter ) );

		const FString Filter = UTF8_TO_TCHAR( ActionFilter );

		int32 Shown = 0;
		for ( const TObjectPtr<UInputAction>& IA : Router->GetUniqueActionsForDebug() )
		{
			if ( !IA ) { continue; }
			const FString Name = IA->GetName();
			if ( !Filter.IsEmpty() && !Name.Contains( Filter ) ) { continue; }
			ImGui::BulletText( "%s", TCHAR_TO_UTF8( *Name ) );
			++Shown;
		}

		if ( Shown == 0 )
		{
			ImGui::TextColored( ColWarn, "該当なし（絞り込み条件、または未Bind）" );
		}
	}
}

#endif // !UE_BUILD_SHIPPING
