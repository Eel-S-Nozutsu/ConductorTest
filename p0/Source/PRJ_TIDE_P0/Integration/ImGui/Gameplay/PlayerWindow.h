// Copyright (c) 2025, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.

#pragma once

#if !UE_BUILD_SHIPPING

#include "PRJ_TIDE_P0/Integration/ImGui/ImGuiWindowBase.h"
#include "PRJ_TIDE_P0/Integration/ImGui/ImGuiLogWindow.h"

/**
 * プレイヤーデバッグImGuiクラス
 */
class PlayerWindow : public ImGuiWindowBase
{
public:
	const char* GetWindowName() const override { return "Player"; }
	EImGuiMenuCategory GetCategory() const override { return EImGuiMenuCategory::Gameplay; }

	void DrawContents() override;

	static FImGuiLogWindow AttackLog;

private:
	void DrawTabGenerals();
	void DrawTabDebugFlags();
	void DrawTabAttackLog();

	void DrawContentsPlayerInfo();
	void DrawContentsAnimInfo();
	void DrawContentsChargeInfo();
	void DrawContentsAttackInfo();
	void DrawContentsDodgeInfo();
	void DrawContentsLockOnInfo();
	void DrawContentsDamageInfo();
	void DrawContentsFallInfo();
	void DrawContentsGodActionInfo();
	void DrawContentsSlidePassiveInfo();
	void DrawContentsSurfaceRideInfo();

private:
	struct FAnimHistoryInfo {
		FString Name;
		float Length;
		float LastRatio;
		float EndTime;
	};

	FString LastMontageName = TEXT( "None" );
	float LastPosition = 0.0f;
	float LastLength = 0.0f;
	float LastPlayRatio = 0.0f;
	float TimeOfEnd = -1.0f; // 終了した時間を記録
};

#endif // !UE_BUILD_SHIPPING
