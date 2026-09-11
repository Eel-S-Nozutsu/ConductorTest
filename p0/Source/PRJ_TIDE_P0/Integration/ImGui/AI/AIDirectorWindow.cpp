// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#include "PRJ_TIDE_P0/Integration/ImGui/AI/AIDirectorWindow.h"
#include "PRJ_TIDE_P0/AI/Director/AIDirector.h"
#include "PRJ_TIDE_P0/Actors/Character/Enemy/EnemyCharacter.h"

#include "Engine/World.h"

void AIDirectorWindow::DrawContents()
{
	UWorld* World = GameInstance ? GameInstance->GetWorld() : nullptr;
	if (!World)
	{
		ImGui::TextDisabled("World unavailable");
		return;
	}

	UAIDirector* Director = World->GetSubsystem<UAIDirector>();
	if (!Director)
	{
		ImGui::TextDisabled("AIDirector unavailable");
		return;
	}

	// --- AttackToken ---
	ImGui::SeparatorText("Attack Token");

	const int32 Current = Director->GetCurrentAttackerCount();
	const int32 Max = Director->GetMaxSimultaneousAttackers();

	// トークン使用率バー
	{
		const float Progress = (Max > 0) ? static_cast<float>(Current) / Max : 0.0f;
		const FString Label = FString::Printf(TEXT("%d / %d##token"), Current, Max);
		const ImVec4 Color = (Current >= Max)
			? ImVec4(1.0f, 0.3f, 0.2f, 1.0f)   // 満杯: 赤
			: ImVec4(0.2f, 1.0f, 0.2f, 1.0f);   // 空き: 緑
		ImGui::PushStyleColor(ImGuiCol_PlotHistogram, Color);
		ImGui::ProgressBar(Progress, ImVec2(-1.0f, 0.0f), TCHAR_TO_UTF8(*Label));
		ImGui::PopStyleColor();
	}

	TArray<AEnemyCharacter*> Attackers = Director->GetActiveAttackers();
	if (Attackers.IsEmpty())
	{
		ImGui::TextDisabled("  (攻撃中の敵なし)");
	}
	else
	{
		for (AEnemyCharacter* Enemy : Attackers)
		{
			const FVector Loc = Enemy->GetActorLocation();
			const FString Text = FString::Printf(TEXT("  [ATK] (%.0f, %.0f, %.0f)"), Loc.X, Loc.Y, Loc.Z);
			ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.2f, 1.0f), TCHAR_TO_UTF8(*Text));
		}

		// TokenHoldTime経過分は枠を明け渡すため、攻撃中の実数とCurrentがずれる
		if (Current < Attackers.Num())
		{
			ImGui::TextDisabled("  (うち %d体は TokenHoldTime 経過で枠を解放済み)", Attackers.Num() - Current);
		}
	}

	// --- 入札 ---
	ImGui::Spacing();
	ImGui::SeparatorText("Attack Bids");
	ImGui::TextDisabled("  近い順。先頭がトークンを取れる");

	TArray<TPair<AEnemyCharacter*, float>> Bids = Director->GetActiveBids();
	if (Bids.IsEmpty())
	{
		ImGui::TextDisabled("  (入札なし)");
	}
	else
	{
		for (int32 i = 0; i < Bids.Num(); ++i)
		{
			const bool bWinner = Director->IsClosestBidder(Bids[i].Key);
			const FString Text = FString::Printf(TEXT("  %s %.0fcm"),
				bWinner ? TEXT("[WIN]") : TEXT("     "), Bids[i].Value);
			const ImVec4 Color = bWinner
				? ImVec4(0.2f, 1.0f, 0.2f, 1.0f)
				: ImVec4(0.6f, 0.6f, 0.6f, 1.0f);
			ImGui::TextColored(Color, TCHAR_TO_UTF8(*Text));
		}
	}

	// --- スロット占有 ---
	ImGui::Spacing();
	ImGui::SeparatorText("Desired Positions");
	ImGui::TextDisabled("  SlotRadius: %.0fcm", Director->SlotRadius);

	TArray<TPair<AEnemyCharacter*, FVector>> Positions = Director->GetDesiredPositions();
	if (Positions.IsEmpty())
	{
		ImGui::TextDisabled("  (登録なし)");
	}
	else
	{
		for (const auto& [Enemy, Pos] : Positions)
		{
			const FString Text = FString::Printf(TEXT("  (%.0f, %.0f, %.0f)"), Pos.X, Pos.Y, Pos.Z);
			ImGui::Text(TCHAR_TO_UTF8(*Text));
		}
	}
}
