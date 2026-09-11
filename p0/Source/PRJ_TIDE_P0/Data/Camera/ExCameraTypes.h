// Copyright (c) 2026, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraShakeBase.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/DataTable.h"

#include "ExCameraTypes.generated.h"

// スタックに積まれたモードを一意に識別するハンドル
USTRUCT( BlueprintType )
struct FCameraModeHandle
{
	GENERATED_BODY()

	UPROPERTY( BlueprintReadOnly, Category = "Camera System" )
	int32 ID = 0;	// 0 は無効

	bool IsValid() const { return ID != 0; }
	bool operator==( const FCameraModeHandle& Other ) const { return ID == Other.ID; }
	void Clear() { ID = 0; }
};

// ジャンプ中の注視点の縦“置いていき”状態。空中で別カメラが Push されても高さを引き継ぎ、現在高さへ
// 再アンカーして縦に飛ぶのを防ぎたいので、モードではなく永続コンポーネントが保持する共有ストアにする
struct FJumpFocusVerticalState
{
	bool  bValid = false;            // 無効ならモード側でプレイヤー高さから再アンカーする
	float CurrentFocusZ = 0.0f;      // 実際に使う注視点Z
	float GroundedFocusZ = 0.0f;     // 離陸（接地）時の基準高さ
	bool  bPrevAirborne = false;
	bool  bExceededDeadZone = false; // 今回の滞空で一度でも超えたか
};

// カメラモードの共通パラメータ
USTRUCT( BlueprintType )
struct FExCameraModeCommonParams : public FTableRowBase
{
	GENERATED_BODY()

	// 優先度（数値が大きいほど優先される。スタック内で同値の場合は後に積まれた方が優先される） 
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Camera Mode|Base" )
	int32 Priority = 0;
	// カメラの基本的なFOV（視野角）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Camera Mode|Base" )
	float FOV = 90.0f;

	// カメラの回転（視点移動）に乗算する倍率
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Camera Mode|Control" )
	FVector2D RotationSpeedRate = FVector2D( 1.0f, 1.0f );

	// カメラの上下方向への許容角度
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Camera Mode|Limits" )
	float PitchMin = -60.0f;
	// カメラの上下方向への許容角度
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Camera Mode|Limits" )
	float PitchMax = 60.0f;

	// カメラの左右方向への許容角度（カメラMinとMaxが同値なら制限なし）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Camera Mode|Limits" )
	float YawMin = 0.0f;
	// カメラの左右方向への許容角度（カメラMinとMaxが同値なら制限なし）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Camera Mode|Limits" )
	float YawMax = 0.0f;

	// カメラの揺れアセット
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Camera Mode|Effects" )
	TSubclassOf<UCameraShakeBase> CameraShake;
	// カメラのモディファイアアセット
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Camera Mode|Effects" )
	TSubclassOf<UCameraModifier> CameraModifier;
};

// PlayerController から CameraMode へ渡す入力
USTRUCT( BlueprintType )
struct FCameraControlData
{
	GENERATED_BODY()

	UPROPERTY( BlueprintReadWrite, Category = "Camera Input" )
	float DeltaTime = 0.0f;

	// 操作対象（キャラクター）の現在のワールド座標
	// ※これがあれば、Mode側でPawnを取得しにいく必要がなくなる
	UPROPERTY( BlueprintReadWrite, Category = "Camera Input" )
	FVector TargetLocation = FVector::ZeroVector;

	// コントローラーが現在保持している回転値（プレイヤーがカメラを向けたい方向）
	UPROPERTY( BlueprintReadWrite, Category = "Camera Input" )
	FRotator ControlRotation = FRotator::ZeroRotator;

	// スティックやマウスの生入力（元のデータ・特殊なカメラ操作拡張用）
	UPROPERTY( BlueprintReadWrite, Category = "Camera Input" )
	FVector2D LookInput = FVector2D::ZeroVector;
	// WASDや左スティックの移動入力（元のデータ・特殊なカメラ操作拡張用）
	UPROPERTY( BlueprintReadWrite, Category = "Camera Input" )
	FVector MovementInput = FVector::ZeroVector;
};
