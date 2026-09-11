// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "FallRecoveryComponent.generated.h"

class ATidePlayerCharacter;

/**
 * 奈落落下時の復帰地点を管理するコンポーネント
 */
UCLASS()
class PRJ_TIDE_P0_API UFallRecoveryComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	UFallRecoveryComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	void RequestRecovery();

private:

	void TryRecordSafeLocation();
	void ExecuteTeleport();

	// 候補地点が崖の縁から十分離れた平坦地かを、周囲の放射状トレースで判定する
	bool IsRoomySafeLocation(const FVector& Location) const;

	// 安全地点を更新するために必要な最低移動距離
	UPROPERTY(EditAnywhere, Category = "FallRecovery", meta = (ClampMin = "50.0"))
	float SafeLocationRecordDistance = 200.0f;

	// 復帰地点として認めるための、周囲に地面が続いていてほしい半径
	// 崖の縁からこの距離以上離れた点のみ記録する = スティック入力の余裕になる
	UPROPERTY(EditAnywhere, Category = "FallRecovery|EdgeMargin", meta = (ClampMin = "0.0"))
	float EdgeMarginRadius = 200.0f;

	// 周囲をサンプリングする方向数
	UPROPERTY(EditAnywhere, Category = "FallRecovery|EdgeMargin", meta = (ClampMin = "3"))
	int32 EdgeSampleDirections = 8;

	// 記録を認めるのに地面が必要な最低サンプル数 ※全方向必須にせず細い足場を許容
	UPROPERTY(EditAnywhere, Category = "FallRecovery|EdgeMargin", meta = (ClampMin = "1"))
	int32 MinValidSamples = 6;

	// 平坦とみなす、中心足元とサンプル地面との高さ許容差
	UPROPERTY(EditAnywhere, Category = "FallRecovery|EdgeMargin", meta = (ClampMin = "0.0"))
	float FlatnessHeightTolerance = 60.0f;

	// 歩行可能とみなす地面法線Zの下限 (1.0=水平, 0.7≒約45度)
	UPROPERTY(EditAnywhere, Category = "FallRecovery|EdgeMargin", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MinGroundNormalZ = 0.7f;

	// 記録条件にNavMesh上にあることも加える ※補強・既定は無効
	UPROPERTY(EditAnywhere, Category = "FallRecovery|EdgeMargin")
	bool bRequireOnNavMesh = false;

	// 縁マージン判定を可視化する (緑=地面あり/赤=空or段差, 黄球=採用中の復帰地点)
	UPROPERTY(EditAnywhere, Category = "FallRecovery|Debug")
	bool bDrawDebugEdgeMargin = false;

	// 復帰地点へのテレポート前のフェードアウト秒数
	UPROPERTY(EditAnywhere, Category = "FallRecovery", meta = (ClampMin = "0.0"))
	float FadeOutDuration = 0.3f;

	// 復帰地点へのテレポート後のフェードイン秒数
	UPROPERTY(EditAnywhere, Category = "FallRecovery", meta = (ClampMin = "0.0"))
	float FadeInDuration = 0.3f;

	FVector LastSafeLocation = FVector::ZeroVector;
	bool bIsRecovering = false;
	FTimerHandle RecoveryTimerHandle;

};
