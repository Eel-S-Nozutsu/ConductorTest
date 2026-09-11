// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Coin.generated.h"

/**
 * マリオ風コイン：浮遊回転し、接触で取得アニメ後に消える。PCが離れた後リポップする。
 */
UCLASS()
class PRJ_TIDE_P0_API ACoin : public AActor
{
	GENERATED_BODY()

public:

	ACoin();

protected:

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	UFUNCTION()
	void OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
		bool bFromSweep, const FHitResult& SweepResult);

public:

	UPROPERTY(VisibleAnywhere, Category = "Tide|Coin")
	TObjectPtr<class USphereComponent> CollisionComp;

	UPROPERTY(VisibleAnywhere, Category = "Tide|Coin")
	TObjectPtr<class UStaticMeshComponent> MeshComp;

	// 浮き上下の振幅
	UPROPERTY(EditAnywhere, Category = "Tide|Coin")
	float FloatAmplitude = 20.0f;

	// 浮き上下の速度
	UPROPERTY(EditAnywhere, Category = "Tide|Coin")
	float FloatSpeed = 2.0f;

	// 回転速度(度/秒)
	UPROPERTY(EditAnywhere, Category = "Tide|Coin")
	float RotationSpeed = 180.0f;

	// 取得時に上昇する高さ
	UPROPERTY(EditAnywhere, Category = "Tide|Coin")
	float PickupRiseHeight = 80.0f;

	// 取得アニメーション時間
	UPROPERTY(EditAnywhere, Category = "Tide|Coin")
	float PickupDuration = 0.4f;

	// 取得時の回転速度(度/秒)
	UPROPERTY(EditAnywhere, Category = "Tide|Coin")
	float PickupRotationSpeed = 720.0f;

	// リポップまでの最短秒数
	UPROPERTY(EditAnywhere, Category = "Tide|Coin")
	float RespawnDelay = 5.0f;

	// リポップに必要なPCとの最小距離
	UPROPERTY(EditAnywhere, Category = "Tide|Coin")
	float RespawnDistance = 600.0f;

private:

	void Pickup();
	void CheckRespawn();

	bool bPickedUp = false;
	float PickupElapsed = 0.0f;
	float PickupTime = 0.0f;

	FVector SpawnLocation;
	FVector PickupStartLocation;

	FTimerHandle RespawnCheckTimerHandle;

};
