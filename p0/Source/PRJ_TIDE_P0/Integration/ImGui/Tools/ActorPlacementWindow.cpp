// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#include "PRJ_TIDE_P0/Integration/ImGui/Tools/ActorPlacementWindow.h"
#include "PRJ_TIDE_P0/Integration/ImGui/ImGuiCommon.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/DataTable.h"
#include "UObject/UnrealType.h"

ActorPlacementWindow::ActorPlacementWindow()
{
	auto AddCategory = [&](const char* Name, const TCHAR* StructName)
	{
		FSpawnCategory& Cat = Categories.AddDefaulted_GetRef();
		Cat.TabName       = Name;
		Cat.RowStructName = StructName;
	};

	AddCategory("Enemy",   TEXT("EnemySpawnTableRow"));
	AddCategory("Gimmick", TEXT("GimmickSpawnTableRow"));
	AddCategory("NPC",     TEXT("NPCSpawnTableRow"));
}

void ActorPlacementWindow::OnOpen()
{
	ScanDataTables();
}

void ActorPlacementWindow::ScanDataTables()
{
	for (FSpawnCategory& Cat : Categories)
	{
		Cat.Entries.Reset();
	}

	FAssetRegistryModule& AR = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");

	TArray<FAssetData> AssetDataList;
	AR.Get().GetAssetsByClass(UDataTable::StaticClass()->GetClassPathName(), AssetDataList);

	for (const FAssetData& AssetData : AssetDataList)
	{
		FString RowStructure;
		AssetData.GetTagValue(TEXT("RowStructure"), RowStructure);

		FSpawnCategory* MatchedCat = Categories.FindByPredicate(
			[&](const FSpawnCategory& Cat) { return RowStructure.Contains(Cat.RowStructName); });
		if (!MatchedCat)
		{
			continue;
		}

		UDataTable* Table = Cast<UDataTable>(AssetData.GetAsset());
		if (!Table)
		{
			continue;
		}

		// 全カテゴリのTableRowが "ActorClass" / "DisplayName"
		// という共通フィールド名を持つことを前提にする
		FClassProperty* ClassProp = FindFProperty<FClassProperty>(Table->GetRowStruct(), TEXT("ActorClass"));
		FStrProperty*   NameProp  = FindFProperty<FStrProperty>(Table->GetRowStruct(), TEXT("DisplayName"));
		if (!ClassProp)
		{
			continue;
		}

		for (const auto& Pair : Table->GetRowMap())
		{
			const uint8* RowData = Pair.Value;

			UClass* Class = Cast<UClass>(ClassProp->GetObjectPropertyValue(
				ClassProp->ContainerPtrToValuePtr<void>(RowData)));
			if (!Class)
			{
				continue;
			}

			FSpawnEntry Entry;
			Entry.ActorClass  = Class;
			Entry.DisplayName = NameProp
				? NameProp->GetPropertyValue(NameProp->ContainerPtrToValuePtr<void>(RowData))
				: FString{};
			if (Entry.DisplayName.IsEmpty())
			{
				Entry.DisplayName = Pair.Key.ToString();
			}
			MatchedCat->Entries.Add(MoveTemp(Entry));
		}
	}
}

void ActorPlacementWindow::DrawContents()
{
	ImGui::DragFloat("Forward Distance (cm)", &ForwardDistance, 10.0f, 0.0f, 5000.0f);
	ImGui::DragFloat3("Offset (cm)", OffsetXYZ, 10.0f);

	ImGui::Separator();

	if (ImGui::Button("Rescan"))
	{
		ScanDataTables();
	}

	ImGui::Separator();

	if (ImGui::BeginTabBar("Categories"))
	{
		for (FSpawnCategory& Cat : Categories)
		{
			if (ImGui::BeginTabItem(Cat.TabName))
			{
				if (Cat.Entries.IsEmpty())
				{
					ImGui::TextDisabled("No DataTable found.");
				}
				else
				{
					for (const FSpawnEntry& Entry : Cat.Entries)
					{
						// タブ名と行名を組み合わせてImGui内部IDを一意にする
						FString BtnLabel = FString::Printf(TEXT("Spawn##%hs_%s"), Cat.TabName, *Entry.DisplayName);
						if (ImGui::Button(TCHAR_TO_UTF8(*BtnLabel)))
						{
							PlaceActor(Entry.ActorClass);
						}
						ImGui::SameLine();
						ImGui::Text("%s", TCHAR_TO_UTF8(*Entry.DisplayName));
					}
				}
				ImGui::EndTabItem();
			}
		}
		ImGui::EndTabBar();
	}
}

void ActorPlacementWindow::PlaceActor(TSubclassOf<AActor> ActorClass)
{
	if (!ActorClass || !GameInstance)
	{
		return;
	}

	UWorld* World = GameInstance->GetWorld();
	if (!World)
	{
		return;
	}

	APlayerController* PC = World->GetFirstPlayerController();
	if (!PC || !PC->GetPawn())
	{
		return;
	}

	APawn* Pawn      = PC->GetPawn();
	FVector SpawnLoc = Pawn->GetActorLocation()
	                 + Pawn->GetActorForwardVector() * ForwardDistance
	                 + FVector(OffsetXYZ[0], OffsetXYZ[1], OffsetXYZ[2]);
	FRotator SpawnRot = FRotator(0.0f, Pawn->GetActorRotation().Yaw, 0.0f);

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	World->SpawnActor<AActor>(ActorClass, SpawnLoc, SpawnRot, Params);
}
