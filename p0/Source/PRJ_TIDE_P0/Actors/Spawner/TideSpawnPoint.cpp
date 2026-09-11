// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#include "TideSpawnPoint.h"

#include "Components/BillboardComponent.h"
#if WITH_EDITORONLY_DATA
#include "Components/ArrowComponent.h"
#endif

ATideSpawnPoint::ATideSpawnPoint()
{
	PrimaryActorTick.bCanEverTick = false;

	// ルートは必ず実行時にも生成する
	// CreateEditorOnlyDefaultSubobjectで作ると-gameで生成されず、
	// SCSが原点のDefaultSceneRootを自動生成して配置座標が失われる
	// (standaloneで敵が全員ワールド原点にスポーンした実害あり)
	// 名前 "Billboard" を維持することで、
	// 既存配置のシリアライズ済みトランスフォームがそのまま載る
	Billboard = CreateDefaultSubobject<UBillboardComponent>(TEXT("Billboard"));
	RootComponent = Billboard;

#if WITH_EDITORONLY_DATA
	Arrow = CreateEditorOnlyDefaultSubobject<UArrowComponent>(TEXT("Arrow"));
	if (Arrow)
	{
		Arrow->SetupAttachment(RootComponent);
		Arrow->ArrowSize = 1.5f;
	}
#endif
}
