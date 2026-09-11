// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PRJ_TIDE_P0/Data/Enemy/EnemyWaitSettings.h"
#include "TideSpawnPoint.generated.h"

/**
 * スポーン位置マーカー。ATideSpawnerから参照し、エディタ上で位置・向きを視認できる。
 */
UCLASS()
class PRJ_TIDE_P0_API ATideSpawnPoint : public AActor
{
	GENERATED_BODY()

public:

	ATideSpawnPoint();
	bool HasWaitSettingsOverride() const { return bOverrideWaitSettings; }
	const FEnemyWaitSettings& GetWaitSettings() const { return WaitSettings; }

protected:

	UPROPERTY(EditAnywhere, Category = "Tide|Enemy")
	bool bOverrideWaitSettings = false;

	UPROPERTY(EditAnywhere, Category = "Tide|Enemy", meta = (EditCondition = "bOverrideWaitSettings", EditConditionHides))
	FEnemyWaitSettings WaitSettings;

private:

	// ルート。editor-onlyにすると -gameでは生成されず、
	// SCSが原点のDefaultSceneRootを自動生成して配置トランスフォームが失われる
	// (standaloneで全スポーンが原点に集まる)
	// そのため実行時も生成する(スプライトはゲームでは描画されない)
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<class UBillboardComponent> Billboard;

#if WITH_EDITORONLY_DATA
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<class UArrowComponent> Arrow;
#endif

};
