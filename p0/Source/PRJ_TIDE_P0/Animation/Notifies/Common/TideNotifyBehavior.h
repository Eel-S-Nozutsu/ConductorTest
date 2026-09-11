// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Kismet/KismetMathLibrary.h"
#include "UObject/Object.h"
#include "TideNotifyBehavior.generated.h"

class ATideCharacter;

/**
 * 
 */
UCLASS(Abstract, EditInlineNew, DefaultToInstanced)
class PRJ_TIDE_P0_API UTideNotifyBehavior : public UObject
{
	GENERATED_BODY()

public:

	virtual void OnBegin(ATideCharacter* Character, float TotalDuration) {}
	virtual void OnTick(ATideCharacter* Character, float DeltaTime) {}
	virtual void OnEnd(ATideCharacter* Character) {}

};

/**
 * ターゲット方向への回転
 * ※AEnemyCharacter専用
 */
UCLASS(meta = (DisplayName = "ターゲット方向へ回転"))
class PRJ_TIDE_P0_API UEnemyNotifyBehavior_RotateToTarget : public UTideNotifyBehavior
{
	GENERATED_BODY()

public:

	// Notify開始時の向きから回転できる最大角度 ※0で無制限
	UPROPERTY(EditAnywhere, meta = (ClampMin = "0.0"))
	float MaxRotationAngle = 0.0f;

	// ターゲット方向に加えるYawオフセットdeg※背後攻撃モーションなら180でターゲットに背中を向ける
	UPROPERTY(EditAnywhere, meta = (ClampMin = "-180.0", ClampMax = "180.0"))
	float YawOffset = 0.0f;

	// 回転速度に掛けるイージング ※Notify区間を0->1で進む
	UPROPERTY(EditAnywhere)
	TEnumAsByte<EEasingFunc::Type> EasingFunction = EEasingFunc::Linear;

	UPROPERTY(EditAnywhere, meta = (ClampMin = "1.0"))
	float BlendExp = 2.0f;

	// 基本回転速度deg/s
	UPROPERTY(EditAnywhere, meta = (ClampMin = "0.0"))
	float RotationRate = 360.0f;

	virtual void OnBegin(ATideCharacter* Character, float TotalDuration) override;
	virtual void OnTick(ATideCharacter* Character, float DeltaTime) override;
	virtual void OnEnd(ATideCharacter* Character) override;

private:

	struct FRotateState
	{
		float StartYaw    = 0.0f;
		float ElapsedTime = 0.0f;
		float Duration    = 0.0f;

	};

	// 同一アセットが複数キャラで同時再生された場合でも状態が混ざらないようキャラごとに管理
	TMap<ATideCharacter*, FRotateState> States;

};
