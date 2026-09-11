// Copyright (c) 2026, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Curves/CurveFloat.h"

#include "ExCameraBlendTypes.generated.h"

// 拡張性を持たせた独自のブレンド計算式Enum
UENUM( BlueprintType )
enum class EExCameraBlendFunction : uint8
{
	Linear			UMETA( DisplayName = "線形補間 (一定速度)" ),
	SmoothStep		UMETA( DisplayName = "SmoothStep (滑らかな加減速)" ),
	EaseIn			UMETA( DisplayName = "EaseIn (徐々に加速)" ),
	EaseOut			UMETA( DisplayName = "EaseOut (徐々に減速)" ),
	EaseInOut		UMETA( DisplayName = "EaseInOut (指数を使った加減速)" ),
	CustomCurve		UMETA( DisplayName = "CustomCurve (カーブアセット)" )
};

// ブレンド設定テーブルの行データ
USTRUCT( BlueprintType )
struct FExCameraBlendTableRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "Blend Settings" )
	float BlendTime = 0.5f;

	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "Blend Settings" )
	EExCameraBlendFunction BlendFunction = EExCameraBlendFunction::SmoothStep;

	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "Blend Settings", meta = ( EditCondition = "BlendFunction == EExCameraBlendFunction::EaseIn || BlendFunction == EExCameraBlendFunction::EaseOut || BlendFunction == EExCameraBlendFunction::EaseInOut" ) )
	float BlendExp = 2.0f;

	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "Blend Settings", meta = ( EditCondition = "BlendFunction == EExCameraBlendFunction::CustomCurve" ) )
	TObjectPtr<UCurveFloat> BlendCurve = nullptr;

	// ブレンド開始時のカメラを固定するか
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "Blend Settings" )
	bool bUseFixedBlendStart = false;
};
