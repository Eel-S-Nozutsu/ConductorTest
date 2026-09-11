// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#include "PRJ_TIDE_P0/Integration/ImGui/Gameplay/ContentDirectorWindow.h"
#include "PRJ_TIDE_P0/Director/ContentDirector.h"
#include "PRJ_TIDE_P0/Director/ContentDirectorSubsystem.h"
#include "PRJ_TIDE_P0/Director/DirectorModule.h"
#include "PRJ_TIDE_P0/Director/MapDirector.h"

#include "Engine/World.h"

void ContentDirectorWindow::DrawContents()
{
	UWorld* World = GameInstance ? GameInstance->GetWorld() : nullptr;
	if (!World)
	{
		ImGui::TextDisabled("World unavailable");
		return;
	}

	UContentDirectorSubsystem* Subsystem = World->GetSubsystem<UContentDirectorSubsystem>();
	if (!Subsystem)
	{
		ImGui::TextDisabled("ContentDirectorSubsystem unavailable");
		return;
	}

	const TArray<TObjectPtr<UMapDirector>>& MapDirectors = Subsystem->GetMapDirectors();
	if (MapDirectors.IsEmpty())
	{
		ImGui::TextDisabled("  (起動中のMapDirectorなし)");
		ImGui::TextDisabled("  レベル紐づけDT: プロジェクト設定 > Game > Tide Content Director");
		return;
	}

	for (const TObjectPtr<UMapDirector>& MapDirector : MapDirectors)
	{
		if (!MapDirector) continue;

		ImGui::SeparatorText(TCHAR_TO_UTF8(*MapDirector->GetLevelPackageName().ToString()));

		const TArray<TObjectPtr<UContentDirector>>& Contents = MapDirector->GetContentDirectors();
		if (Contents.IsEmpty())
		{
			ImGui::TextDisabled("  (コンテンツなし)");
			continue;
		}

		for (const TObjectPtr<UContentDirector>& Content : Contents)
		{
			if (!Content) continue;

			const FString Header = FString::Printf(TEXT("%s  [%s]"),
				*Content->GetContentId().ToString(), *Content->GetCurrentPhase().ToString());

			if (!ImGui::TreeNodeEx(TCHAR_TO_UTF8(*Header), ImGuiTreeNodeFlags_DefaultOpen)) continue;

			// --- 現フェーズの出口 ---
			TArray<TTuple<FString, FName, bool>> Transitions;
			Content->GetActiveTransitionsDebug(Transitions);

			if (Transitions.IsEmpty())
			{
				ImGui::TextDisabled("  (出口なし = 終端フェーズ)");
			}
			else
			{
				for (int32 Index = 0; Index < Transitions.Num(); ++Index)
				{
					const TTuple<FString, FName, bool>& Row = Transitions[Index];
					const FString Text = FString::Printf(TEXT("  %d. %s -> %s"),
						Index + 1, *Row.Get<0>(), *Row.Get<1>().ToString());

					const ImVec4 Color = Row.Get<2>()
						? ImVec4(0.2f, 1.0f, 0.2f, 1.0f)  // 成立
						: ImVec4(0.7f, 0.7f, 0.7f, 1.0f);
					ImGui::TextColored(Color, TCHAR_TO_UTF8(*Text));
				}
			}

			// --- Module ---
			for (const TObjectPtr<UContentDirectorModule>& Module : Content->GetModules())
			{
				if (!Module) continue;

				ImGui::TextDisabled("  [Module] %s", TCHAR_TO_UTF8(*Module->GetClass()->GetName()));
			}

			// --- 強制遷移 ---
			const TArray<FName> Phases = Content->GetDefinedPhases();
			for (const FName& Phase : Phases)
			{
				const FString Label = FString::Printf(TEXT("%s##%s_%s"),
					*Phase.ToString(), *Content->GetContentId().ToString(), *Phase.ToString());

				if (ImGui::SmallButton(TCHAR_TO_UTF8(*Label)))
				{
					Content->RequestPhase(Phase);
				}
				ImGui::SameLine();
			}
			ImGui::NewLine();

			ImGui::TreePop();
		}
	}
}
