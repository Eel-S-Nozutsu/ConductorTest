// Copyright (c) 2026, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Camera/CameraTypes.h"

#include "PRJ_TIDE_P0/Data/Camera/ExCameraTypes.h"

#include "ExCameraMode.generated.h"

// カメラモードの基底クラス
UCLASS( Abstract, Blueprintable )
class UExCameraMode : public UObject
{
	GENERATED_BODY()

public:
	virtual void InitializeMode( const FExCameraModeCommonParams& Params );

	virtual void UpdateCamera( const FCameraControlData& ControlData, FMinimalViewInfo& OutViewInfo );

	// モードがアクティブ（スタック最上位）になった際にコンポーネントから呼ばれる
	virtual void OnActivated( const FMinimalViewInfo& LastViewInfo ) {}

	virtual FVector2D GetRotationSpeedRate() const
	{
		return FVector2D( 1.0f, 1.0f );
	}

	// スタック内での優先度。数値が大きいほど優先してアクティブになる。
	// 同値のときは後に積まれた（スタック上位の）モードが優先される。
	int32 GetPriority() const { return Priority; }

public:
	int32 InstanceID = 0;

protected:
	// 優先度。InitializeMode / InitializeFromRow で CommonParams.Priority から設定する。
	int32 Priority = 0;

};
