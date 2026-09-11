// Copyright (c) 2026, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"

#include "PlayerAnimInstance.generated.h"

UCLASS()
class PRJ_TIDE_P0_API UPlayerAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:

	virtual void NativeUpdateAnimation( float DeltaSeconds ) override;

	// ロックオン中BSを適応するか
	UPROPERTY( BlueprintReadOnly )
	bool bIsLockOnMove = false;

	// チャージBS再生中かどうかのマスターフラグ
	UPROPERTY( BlueprintReadOnly )
	bool	bIsChargeBSPlaying = false;
	// 現在のチャージコンボ段階 (1〜4)
	UPROPERTY( BlueprintReadOnly )
	int32	ChargeComboIndex = 1;
	// チャージ用方向（-90～90）
	UPROPERTY( BlueprintReadOnly )
	float ChargeDirection = 0.0f;
	// チャージのSlide~LpのState(0~1)
	UPROPERTY( BlueprintReadOnly )
	float ChargeState = 0.0f;

	// ダッシュのBS再生フラグ
	UPROPERTY( BlueprintReadOnly )
	bool bIsDashBSPlaying = false;
	// ダッシュ用方向（-90～90）
	UPROPERTY( BlueprintReadOnly )
	float DashDirection = 0.0f;

	// チャージダッシュLoopのBS再生フラグ
	UPROPERTY( BlueprintReadOnly )
	bool bIsChargeDashBSPlaying = false;
	// チャージダッシュLoop用方向（-90～90）
	UPROPERTY( BlueprintReadOnly )
	float ChargeDashDirection = 0.0f;
	// チャージダッシュのギア段階（1〜3）。AnimBP側でギアごとのBS選択に使う
	UPROPERTY( BlueprintReadOnly )
	int32 ChargeDashGearIndex = 1;

	// ターンモーション再生中かどうかのフラグ
	UPROPERTY( BlueprintReadOnly )
	bool bIsTurning = false;
};
