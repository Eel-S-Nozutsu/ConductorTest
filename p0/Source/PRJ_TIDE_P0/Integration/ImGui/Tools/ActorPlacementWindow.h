// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "PRJ_TIDE_P0/Integration/ImGui/ImGuiWindowBase.h"

/**
 * DataTableに登録したActorBPをワンクリックでPC前方に配置するツールウィンドウ
 * カテゴリごとに行型の異なるDataTableを用意し、Asset Registry経由で自動収集する
 */
class ActorPlacementWindow : public ImGuiWindowBase
{
public:

	ActorPlacementWindow();

	const char* GetWindowName() const override { return "ActorPlacement"; }
	EImGuiMenuCategory GetCategory() const override { return EImGuiMenuCategory::Tools; }

	void DrawContents() override;
	void OnOpen() override;

private:

	struct FSpawnEntry
	{
		FString             DisplayName;
		TSubclassOf<AActor> ActorClass;

	};

	struct FSpawnCategory
	{
		const char*         TabName      = nullptr;
		FString             RowStructName;
		TArray<FSpawnEntry> Entries;

	};

	// FEnemySpawnTableRow / FGimmickSpawnTableRow /
	// FNPCSpawnTableRowを行型に持つDataTableを
	// AssetRegistry経由で検索してEntriesに積む
	void ScanDataTables();

	void PlaceActor(TSubclassOf<AActor> ActorClass);

	TArray<FSpawnCategory> Categories;

	float ForwardDistance = 300.0f;
	float OffsetXYZ[3]   = {};

};
