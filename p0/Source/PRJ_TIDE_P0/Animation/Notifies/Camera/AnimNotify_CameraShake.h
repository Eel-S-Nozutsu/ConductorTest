// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_CameraShake.generated.h"

class UCameraShakeBase;

/**
 * カメラシェイク通知(汎用)
 *
 * 発生源とPCの距離に応じて
 * Near以内で最大 / Far以遠で0となるSmoothStepフォールオフで強度を決める
 * (ABossBombardProjectileの着弾シェイクと同じ方式)。
 *
 * 発生源はOriginSocket指定時はそのソケット/ボーン位置、未指定ならメッシュ所有アクターの位置。
 * どのアニメにも置けるよう、特定の攻撃ロジックには依存しない。
 */
UCLASS(meta = (DisplayName = "CameraShake", Category = "Camera"))
class PRJ_TIDE_P0_API UAnimNotify_CameraShake : public UAnimNotify
{
	GENERATED_BODY()

public:

	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;

	// 再生するカメラシェイク ※未設定なら何もしない
	UPROPERTY(EditAnywhere, Category = "CameraShake")
	TSubclassOf<UCameraShakeBase> CameraShake;

	// 発生源ソケット/ボーン名 ※Noneなら所有アクター位置
	UPROPERTY(EditAnywhere, Category = "CameraShake")
	FName OriginSocket = NAME_None;

	// この距離より近いと強度が最大になる
	UPROPERTY(EditAnywhere, Category = "CameraShake")
	float ShakeNearDistance = 1500.0f;

	// この距離より遠いと強度が0になる
	UPROPERTY(EditAnywhere, Category = "CameraShake")
	float ShakeFarDistance = 5000.0f;

	// Near(赤)/Far(黄)のリングをデバッグ描画
	UPROPERTY(EditAnywhere, Category = "CameraShake")
	bool bDebugDrawShakeArea = false;

};
