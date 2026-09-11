// Copyright (c) 2026, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CheatManager.h"

#include "TideCheatManager.generated.h"

UCLASS()
class UTideCheatManager : public UCheatManager
{
	GENERATED_BODY()

public:
	UTideCheatManager();

	// ゲームプレイ側コントローラからデバッグカメラを起動するための public ラッパ（EnableDebugCamera が protected なため）。
	// 終了はデバッグカメラ側が自前で行う（DisableDebugCamera は Outer がデバッグカメラのときしか動かないため）
	void EnterDebugCamera();

protected:
	virtual void EnableDebugCamera() override;
//	virtual void DisableDebugCamera() override;	// なぜか呼ばれない

	virtual void Teleport() override;
};
