// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class UWorld;

/**
 * 右クリックインスペクターのロジックをまとめた合成クラス
 * UImGuiSubsystemが所有し、Tick(World)を毎フレーム呼ぶ
 */
struct FImGuiInspector
{
	// 同時に開けるインスペクターウィンドウの最大数
	int32 MaxInspectorWindows = 1;

	void Tick(UWorld* World);

private:

	void TryOpenInspector(AActor* Actor, FVector2D SpawnPos);
	void DrawInspectorWindows();
	// trueを返したときActorはDestroy済み
	bool DrawDefaultInfo(AActor* Actor);
	AActor* LineTraceUnderCursor(UWorld* World) const;

	struct FInspectorEntry
	{
		TWeakObjectPtr<AActor> Actor;
		int32 WindowId = 0;
		bool bOpen = true;
		FVector2D SpawnPos = FVector2D::ZeroVector;

	};

	TArray<FInspectorEntry> InspectorEntries;
	int32 NextWindowId = 0;
	bool bPrevRightMouseDown = false;

};
