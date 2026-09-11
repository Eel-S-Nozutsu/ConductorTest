// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "PRJ_TIDE_P0/Integration/ImGui/ImGuiWindowBase.h"

/**
 * DataTable (行型: FSituationJumpEntryRow) に登録したレベル+座標にワンクリックで飛ぶデバッグウィンドウ
 * AssetRegistry経由でFSituationJumpEntryRowを行型に持つDTを自動収集する
 */
class SituationJumpWindow : public ImGuiWindowBase
{
public:

	const char* GetWindowName() const override { return "SituationJump"; }
	EImGuiMenuCategory GetCategory() const override { return EImGuiMenuCategory::Tools; }

	void DrawContents() override;
	void OnOpen() override;

private:

	struct FJumpEntry
	{
		FString  DisplayName;
		FName    LevelName;
		FVector Location = FVector::ZeroVector;
		float   Yaw     = 0.0f;

	};

	struct FGroup
	{
		FString            GroupName;
		TArray<FJumpEntry> Entries;

	};

	void ScanDataTables();
	void Jump(const FJumpEntry& Entry);

	TArray<FGroup> Groups;

};
