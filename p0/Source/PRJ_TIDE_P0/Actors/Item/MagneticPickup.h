// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NiagaraSystem.h"
#include "NiagaraComponent.h"
#include "MagneticPickup.generated.h"

/**
 * キラキラ吸着アイテム：接近するとプレイヤーに向かって吸い寄せられ、触れると取得する。
 */
UCLASS()
class PRJ_TIDE_P0_API AMagneticPickup : public AActor
{
	GENERATED_BODY()

public:

	AMagneticPickup();

protected:

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaTime) override;

public:

	UPROPERTY(VisibleAnywhere, Category = "Tide|Pickup")
	TObjectPtr<class USphereComponent> AttractionComp;

	UPROPERTY(VisibleAnywhere, Category = "Tide|Pickup")
	TObjectPtr<class USphereComponent> CollectionComp;

	UPROPERTY(VisibleAnywhere, Category = "Tide|Pickup")
	TObjectPtr<UNiagaraComponent> SparkleComp;

	// 吸着が始まる距離
	UPROPERTY(EditAnywhere, Category = "Tide|Pickup")
	float AttractionRadius = 400.0f;

	// 取得判定距離
	UPROPERTY(EditAnywhere, Category = "Tide|Pickup")
	float CollectionRadius = 60.0f;

	// 吸着開始時の初速
	UPROPERTY(EditAnywhere, Category = "Tide|Pickup")
	float AttractionInitialSpeed = 200.0f;

	// 吸着最大速度
	UPROPERTY(EditAnywhere, Category = "Tide|Pickup")
	float AttractionMaxSpeed = 1500.0f;

	// 吸着加速度
	UPROPERTY(EditAnywhere, Category = "Tide|Pickup")
	float AttractionAcceleration = 2000.0f;

	// 回転速度 ※度毎秒
	UPROPERTY(EditAnywhere, Category = "Tide|Pickup")
	float RotationSpeed = 180.0f;

	// OFFで無重力になる。スポーン時に落下せず、足場が壊れても浮いたままになる
	UPROPERTY(EditAnywhere, Category = "Tide|Pickup|Drop")
	bool bUseGravity = false;

	// 落下開始時の上向き初速。0で真下 ※足場が抜けての再落下では使わない
	UPROPERTY(EditAnywhere, Category = "Tide|Pickup|Drop")
	float DropInitialUpSpeed = 250.0f;

	// 落下加速度
	UPROPERTY(EditAnywhere, Category = "Tide|Pickup|Drop")
	float DropGravity = 1960.0f;

	// 接地面から浮遊の基準位置までの高さ
	UPROPERTY(EditAnywhere, Category = "Tide|Pickup|Drop")
	float GroundOffset = 40.0f;

	// 接地面を見つけられないまま落ち続けるのを打ち切る距離
	// ※高所からのばらまきで足りないと空中で止まるため、穴に落ちた場合の保険として十分大きく取る
	UPROPERTY(EditAnywhere, Category = "Tide|Pickup|Drop")
	float MaxDropDistance = 1000000.0f;

	// 浮遊中に足元を確認する間隔
	UPROPERTY(EditAnywhere, Category = "Tide|Pickup|Drop")
	float GroundCheckInterval = 0.25f;

	// 足元判定の余裕。地面との隙間がこの値を超えたら落下を再開する
	UPROPERTY(EditAnywhere, Category = "Tide|Pickup|Drop")
	float GroundCheckTolerance = 10.0f;

	// 常時再生のキラキラエフェクト
	UPROPERTY(EditAnywhere, Category = "Tide|Pickup|VFX")
	TObjectPtr<UNiagaraSystem> SparkleEffect;

	// 取得時エフェクト
	UPROPERTY(EditAnywhere, Category = "Tide|Pickup|VFX")
	TObjectPtr<UNiagaraSystem> PickupEffect;

	// 取得時にプレイヤーが回復するHP量
	UPROPERTY(EditAnywhere, Category = "Tide|Pickup")
	float HealAmount = 10.0f;

private:

	// プレイヤーが吸着範囲に入ったら飛行を開始する
	UFUNCTION()
	void OnAttractionBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	// プレイヤーが取得範囲に触れたら取得する
	UFUNCTION()
	void OnCollectionBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	// 吸着(飛行)を開始する。オーバーラップと起動時の近接チェックの両方から呼ばれる
	void BeginAttraction();
	void Pickup();
	// 落下中の1フレーム分の積分と接地判定
	void UpdateDrop(float DeltaTime);
	// 接地確定。以降の浮遊はこの位置を基準にする
	void Land(const FVector& Location);
	// 落下開始 ※足場が抜けた場合は初速0で呼ぶ
	void BeginDrop(float InitialUpSpeed);
	// 浮遊中に足元の地面が残っているか調べる。無ければ落下を再開する
	void UpdateGroundCheck(float DeltaTime);
	// 床だけを拾う真下トレース。落下の接地判定と足元判定で共有する
	bool TraceGround(const FVector& Start, const FVector& End, FHitResult& OutHit) const;

	bool bAttracting = false;
	float CurrentAttractionSpeed = 0.0f;

	bool bDropping = false;
	float DropVelocityZ = 0.0f;
	float DropStartZ = 0.0f;
	float GroundCheckTimer = 0.0f;

	FVector SpawnLocation;

};
