// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RootMotionModifier_SkewWarp.h"
#include "TideRootMotionModifier_WarpToTarget.generated.h"

/**
 * ターゲット(PC)へ吸着するSkewWarp
 *
 * 標準の歪みワープはワープターゲットを外部から供給してもらう必要があって不便だったので
 * 自動で解決するようにした ※AEnemyCharacter専用
 */
UCLASS(meta = (DisplayName = "ターゲット吸着ワープ(自動)"))
class PRJ_TIDE_P0_API UTideRootMotionModifier_WarpToTarget : public URootMotionModifier_SkewWarp
{
	GENERATED_BODY()

public:

	UTideRootMotionModifier_WarpToTarget(const FObjectInitializer& ObjectInitializer);

	// ターゲット中心から手前に残す距離
	UPROPERTY(EditAnywhere, Category = "Tide", meta = (ClampMin = "0.0"))
	float StopDistance = 100.0f;

	// ONでワープ先を正面線上に射影し前後の伸縮だけにする
	UPROPERTY(EditAnywhere, Category = "Tide")
	bool bProjectToForward = true;

	// 正面からこの角度以上ターゲットがズレていたらワープしない
	UPROPERTY(EditAnywhere, Category = "Tide", meta = (ClampMin = "0.0", ClampMax = "180.0"))
	float MaxWarpAngle = 0.0f;

	// ターゲット方向に加えるYawオフセット
	UPROPERTY(EditAnywhere, Category = "Tide", meta = (ClampMin = "-180.0", ClampMax = "180.0"))
	float YawOffset = 0.0f;

	// 検証用 ※ワープ先と供給状態を描画
	UPROPERTY(EditAnywhere, Category = "Tide")
	bool bDrawDebug = false;

	virtual void Update(const FMotionWarpingUpdateContext& Context) override;

private:

	bool ResolveWarpTarget();

};
