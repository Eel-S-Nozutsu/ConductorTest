// Copyright (c) 2026, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"

#include "PRJ_TIDE_P0/Data/Camera/ExCameraTypes.h"

#include "ThirdPersonExCameraModeParamRow.generated.h"

// 三人称カメラ用のパラメータテーブル行。CameraMode クラス自体は Subsystem 側で決定し、この行は調整値だけを持つ
USTRUCT( BlueprintType )
struct FThirdPersonExCameraModeParamRow : public FExCameraModeCommonParams
{
	GENERATED_BODY()

	// コメント
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Camera Mode|Base" )
	FString Comment;

	// カメラとキャラクターの距離（エイム時などは短くする）
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "Camera Mode|TPS" )
	float TargetArmLength = 400.0f;

	// キャラクターから見たローカル空間でのオフセット（右肩越しにする場合などに使用）。
	// Z が「回転軸（カメラが周回する中心＝画面中心に留まる点）」の高さを兼ねる（腰→頭へ軸を上げるなら Z を上げる）
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "Camera Mode|TPS" )
	FVector TargetOffset = FVector( 0.0f, 0.0f, 60.0f );

	// 【注視点オフセット】カメラが向く点（画面中心に来る点）を回転軸から上下にずらす相対高さ（cm・ワールド垂直）。
	// 0 なら注視点＝回転軸（その点を軸に周回・画面中心）。正で軸より上・負で軸より下を画面中心に。既定 0（従来挙動）。
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "Camera Mode|TPS" )
	float FocusHeightOffset = 0.0f;

	// 壁へのめり込みを防ぐための判定球の半径（細い隙間を通す時は小さくする等）
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "Camera Mode|Collision" )
	float CollisionRadius = 15.0f;

	// 【壁避けの寄り】地形に当たって手前へ寄せる量の追従速度（既定 0＝補間なし・従来のスナップ）。
	// 下げるとゆっくり寄るが、寄り切るまでカメラが地形に入る
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "Camera Mode|Collision", meta = ( ClampMin = "0.0" ) )
	float CollisionPullInInterpSpeed = 0.0f;

	// 【壁避けの戻り】地形から離れて理想位置へ戻る速度（既定 0＝補間なし）。寄りより遅めにすると飛び出しが穏やかになる
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "Camera Mode|Collision", meta = ( ClampMin = "0.0" ) )
	float CollisionPullOutInterpSpeed = 0.0f;

	// XY軸（水平）のカメララグ
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "Camera Mode|Lag" )
	bool bEnableCameraLagXY = false;

	//  XY軸（水平）のカメララグ追従速度（値が大きいほどターゲットに素早く追従する）
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "Camera Mode|Lag", meta = ( EditCondition = "bEnableCameraLagXY" ) )
	float CameraLagSpeedXY = 10.0f;

	// Z軸（垂直）のカメララグ
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "Camera Mode|Lag" )
	bool bEnableCameraLagZ = false;

	// Z軸（垂直）のカメララグ追従速度（値が大きいほどターゲットに素早く追従する）
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "Camera Mode|Lag", meta = ( EditCondition = "bEnableCameraLagZ" ) )
	float CameraLagSpeedZ = 20.0f;

	// カメラがターゲットから離れることができる最大距離（0.0以下で無制限）
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "Camera Mode|Lag", meta = ( EditCondition = "bEnableCameraLag" ) )
	float CameraLagMaxDistance = 0.0f;

	// カメラの回転（視点移動）に遅延（ラグ）を持たせるかどうか
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "Camera Mode|Lag" )
	bool bEnableCameraRotationLag = false;

	// カメラ回転の追従速度（値が大きいほど入力に対して素早く追従する）
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "Camera Mode|Lag", meta = ( EditCondition = "bEnableCameraRotationLag" ) )
	float CameraRotationLagSpeed = 10.0f;

	// ジャンプ中の注視点の縦追従（縦デッドゾーン式）のパラメータは、機能トグル・置いていく距離・
	// 追従速度などすべて TidePlayerParamDataAsset（全カメラ共通）へ集約した。この行には持たない。
};
