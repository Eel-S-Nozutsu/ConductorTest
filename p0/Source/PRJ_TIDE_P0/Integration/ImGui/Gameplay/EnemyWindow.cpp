// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#include "PRJ_TIDE_P0/Integration/ImGui/Gameplay/EnemyWindow.h"

#if !UE_BUILD_SHIPPING

#include "PRJ_TIDE_P0/Actors/Character/Enemy/EnemyCharacter.h"
#include "PRJ_TIDE_P0/Components/Combat/PartDestructionComponent.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

namespace
{
	const ImVec4 ColorAlive   (0.30f, 1.00f, 0.40f, 1.0f);
	const ImVec4 ColorBroken  (1.00f, 0.40f, 0.30f, 1.0f);
	const ImVec4 ColorPending (1.00f, 0.80f, 0.20f, 1.0f);

	// 1体ぶんの部位クールダウン表
	void DrawPartTable(const AActor& Owner, const UPartDestructionComponent& PartComp)
	{
		const int32 PartCount = PartComp.GetPartCount();

		ImGui::SeparatorText(TCHAR_TO_UTF8(*Owner.GetName()));

		constexpr ImGuiTableFlags TableFlags =
			ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp;
		if (!ImGui::BeginTable("PartRegen", 3, TableFlags)) return;

		ImGui::TableSetupColumn("部位", ImGuiTableColumnFlags_WidthStretch, 0.35f);
		ImGui::TableSetupColumn("状態", ImGuiTableColumnFlags_WidthStretch, 0.20f);
		ImGui::TableSetupColumn("復活まで", ImGuiTableColumnFlags_WidthStretch, 0.45f);
		ImGui::TableHeadersRow();

		for (int32 i = 0; i < PartCount; ++i)
		{
			ImGui::TableNextRow();

			ImGui::TableSetColumnIndex(0);
			ImGui::TextUnformatted(TCHAR_TO_UTF8(*PartComp.GetPartTag(i).ToString()));

			const bool bDestroyed = PartComp.IsPartDestroyedByIndex(i);

			ImGui::TableSetColumnIndex(1);
			if (bDestroyed) ImGui::TextColored(ColorBroken, "破壊");
			else            ImGui::TextColored(ColorAlive, "健在");

			ImGui::TableSetColumnIndex(2);
			if (!bDestroyed)
			{
				ImGui::TextDisabled("-");
				continue;
			}

			// クールダウンは進むが、攻撃中/ダウン中は復活だけが保留される
			if (PartComp.IsPartRevivePending(i))
			{
				ImGui::TextColored(ColorPending, "保留中 (攻撃/ダウン)");
				continue;
			}

			const float Remaining = PartComp.GetPartRegenRemaining(i);
			const float Cooldown  = PartComp.GetPartRegenCooldown(i);
			if (Remaining < 0.0f || Cooldown <= 0.0f)
			{
				// BreakRegenCooldownが0以下なら自動復活しない
				ImGui::TextDisabled("復活なし");
				continue;
			}

			const float Progress = 1.0f - FMath::Clamp(Remaining / Cooldown, 0.0f, 1.0f);
			const FString Label  = FString::Printf(TEXT("%.1f / %.1f s"), Remaining, Cooldown);
			ImGui::ProgressBar(Progress, ImVec2(-1.0f, 0.0f), TCHAR_TO_UTF8(*Label));
		}

		ImGui::EndTable();
	}
}

void EnemyWindow::DrawContents()
{
	UWorld* World = GameInstance ? GameInstance->GetWorld() : nullptr;
	if (!World)
	{
		ImGui::TextDisabled("World unavailable");
		return;
	}

	ImGui::SeparatorText("Part Halo Regen");
	ImGui::TextDisabled("  破壊部位の復活クールダウン。攻撃中/ダウン中は復活が保留される");

	TArray<AActor*> Enemies;
	UGameplayStatics::GetAllActorsOfClass(World, AEnemyCharacter::StaticClass(), Enemies);

	bool bAnyPartOwner = false;
	for (const AActor* Enemy : Enemies)
	{
		const UPartDestructionComponent* PartComp =
			Enemy ? Enemy->FindComponentByClass<UPartDestructionComponent>() : nullptr;
		// 部位を持たない通常敵は表示しない
		if (!PartComp || PartComp->GetPartCount() == 0) continue;

		bAnyPartOwner = true;
		DrawPartTable(*Enemy, *PartComp);
	}

	if (!bAnyPartOwner)
	{
		ImGui::TextDisabled("  (部位破壊を持つ敵がいません)");
	}
}

void EnemyWindow::OnOpen()
{
}

#endif // !UE_BUILD_SHIPPING
