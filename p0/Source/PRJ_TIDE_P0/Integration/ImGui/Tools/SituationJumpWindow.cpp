// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#include "PRJ_TIDE_P0/Integration/ImGui/Tools/SituationJumpWindow.h"
#include "PRJ_TIDE_P0/Integration/ImGui/ImGuiCommon.h"
#include "PRJ_TIDE_P0/Data/Debug/SituationJumpEntryRow.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/DataTable.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "PRJ_TIDE_P0/Core/TideGameInstance.h"

void SituationJumpWindow::OnOpen()
{
	ScanDataTables();
}

void SituationJumpWindow::DrawContents()
{
	if (ImGui::Button("Rescan"))
	{
		ScanDataTables();
	}

	ImGui::Separator();

	if (Groups.IsEmpty())
	{
		ImGui::TextDisabled("No DataTable found.");
		return;
	}

	for (const FGroup& Group : Groups)
	{
		if (ImGui::CollapsingHeader(TCHAR_TO_UTF8(*Group.GroupName)))
		{
			for (const FJumpEntry& Entry : Group.Entries)
			{
				FString BtnLabel = FString::Printf(TEXT("Jump##%s_%s"), *Group.GroupName, *Entry.DisplayName);
				if (ImGui::Button(TCHAR_TO_UTF8(*BtnLabel)))
				{
					Jump(Entry);
				}
				ImGui::SameLine();
				ImGui::TextUnformatted(TCHAR_TO_UTF8(*Entry.DisplayName));
			}
		}
	}
}

void SituationJumpWindow::ScanDataTables()
{
	Groups.Reset();

	TMap<FName, int32> LevelToGroupIndex;

	FAssetRegistryModule& AR = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");

	TArray<FAssetData> AssetDataList;
	AR.Get().GetAssetsByClass(UDataTable::StaticClass()->GetClassPathName(), AssetDataList);

	for (const FAssetData& AssetData : AssetDataList)
	{
		FString RowStructure;
		AssetData.GetTagValue(TEXT("RowStructure"), RowStructure);
		if (!RowStructure.Contains(TEXT("SituationJumpEntryRow")))
		{
			continue;
		}

		UDataTable* Table = Cast<UDataTable>(AssetData.GetAsset());
		if (!Table)
		{
			continue;
		}

		for (const FName& RowName : Table->GetRowNames())
		{
			const FSituationJumpEntryRow* Row = Table->FindRow<FSituationJumpEntryRow>(RowName, TEXT(""));
			if (!Row || Row->LevelName.IsNone())
			{
				continue;
			}

			int32* GroupIndex = LevelToGroupIndex.Find(Row->LevelName);
			if (!GroupIndex)
			{
				int32 NewIndex = Groups.AddDefaulted();
				Groups[NewIndex].GroupName = Row->LevelName.ToString();
				GroupIndex = &LevelToGroupIndex.Add(Row->LevelName, NewIndex);
			}

			FJumpEntry& Entry = Groups[*GroupIndex].Entries.AddDefaulted_GetRef();
			Entry.LevelName   = Row->LevelName;
			Entry.Location    = Row->Location;
			Entry.Yaw         = Row->Yaw;
			Entry.DisplayName = Row->DisplayName.IsEmpty() ? RowName.ToString() : Row->DisplayName;
		}
	}
}

void SituationJumpWindow::Jump(const FJumpEntry& Entry)
{
	if (!GameInstance) return;
	UWorld* World = GameInstance->GetWorld();
	if (!World) return;

	FString CurrentLevel = UGameplayStatics::GetCurrentLevelName(World, true);

	if (CurrentLevel == Entry.LevelName.ToString())
	{
		APlayerController* PC = World->GetFirstPlayerController();
		if (PC && PC->GetPawn())
		{
			const FRotator NewRot(0.0f, Entry.Yaw, 0.0f);
			PC->GetPawn()->SetActorLocation(Entry.Location);
			PC->GetPawn()->SetActorRotation(NewRot);
			PC->SetControlRotation(NewRot);
		}
	}
	else
	{
		if (UTideGameInstance* TideGI = Cast<UTideGameInstance>(GameInstance))
		{
			TideGI->SetPendingJump(Entry.Location, Entry.Yaw);
		}
		UGameplayStatics::OpenLevel(World, Entry.LevelName);
	}
}
