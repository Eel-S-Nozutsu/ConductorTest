// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WaterSurfaceGimmick.generated.h"

/**
 * チャージダッシュ攻撃中のみプレイヤーが歩けるようになる海面ギミック。
 * WaterMeshに水マテリアルを適用した板ポリをセットして使う。
 * IsPlayingChargeAction()がtrueの間だけコリジョンを有効化する。
 */
UCLASS()
class PRJ_TIDE_P0_API AWaterSurfaceGimmick : public AActor
{
	GENERATED_BODY()

public:

	AWaterSurfaceGimmick();

	virtual void Tick(float DeltaTime) override;

public:

	// 水マテリアルを適用した板ポリをセット。チャージダッシュ中のみコリジョンが有効になる
	UPROPERTY(VisibleAnywhere, Category = "Tide|Gimmick")
	TObjectPtr<class UStaticMeshComponent> WaterMesh;

private:

	TWeakObjectPtr<class ATidePlayerCharacter> CachedPlayer;

};
