// Copyright (c) 2026, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WindSource.generated.h"

class USphereComponent;

/**
 * 風の発生源（点）。AWindZone::WindDirectionMode を FromSource にして本アクターを参照させると、
 * 風向きが「アクター前方の固定方向」ではなく「この位置から対象への放射状の方向」になる。
 * 単体では何もしない（AWindZone から参照されて初めて効く）。位置のみ意味を持つ軽量アクター。
 */
UCLASS()
class PRJ_TIDE_P0_API AWindSource : public AActor
{
	GENERATED_BODY()

public:
	AWindSource();

protected:
	virtual void Tick( float DeltaTime ) override;
	// VisualRadius をシェイプへ同期する
	virtual void OnConstruction( const FTransform& Transform ) override;

public:
	UPROPERTY( VisibleAnywhere, Category = "Tide|WindSource" )
	TObjectPtr<USceneComponent> SceneRoot;

	// エディタで常時表示する可視化シェイプ（見た目専用・コリジョン無し）
	UPROPERTY( VisibleAnywhere, Category = "Tide|WindSource" )
	TObjectPtr<USphereComponent> SphereVis;

	// 可視化シェイプの半径（cm）
	UPROPERTY( EditAnywhere, Category = "Tide|WindSource" )
	float VisualRadius = 30.0f;

	// --- デバッグ ---
	// ON で放射状の矢印を毎フレーム描画し、風が全方向へ出ることを示す
	UPROPERTY( EditAnywhere, Category = "Tide|WindSource|Debug" )
	bool bDebugDraw = true;

	// 放射矢印の本数
	UPROPERTY( EditAnywhere, Category = "Tide|WindSource|Debug", meta = ( EditCondition = "bDebugDraw", EditConditionHides, ClampMin = "0" ) )
	int32 DebugArrowCount = 8;

	// 放射矢印の長さ（cm）
	UPROPERTY( EditAnywhere, Category = "Tide|WindSource|Debug", meta = ( EditCondition = "bDebugDraw", EditConditionHides, ClampMin = "1.0" ) )
	float DebugArrowLength = 150.0f;
};
