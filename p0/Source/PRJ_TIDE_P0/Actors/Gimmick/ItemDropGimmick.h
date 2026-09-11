// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ItemDropGimmick.generated.h"

class USceneComponent;
class UBoxComponent;
class AMagneticPickup;
class ATidePlayerCharacter;

USTRUCT(BlueprintType)
struct FItemDropEntry
{
	GENERATED_BODY()

	// ばらまくアイテムのBPクラス
	UPROPERTY(EditAnywhere)
	TSubclassOf<AMagneticPickup> ItemClass;
	// ばらまく個数
	UPROPERTY(EditAnywhere, meta = (ClampMin = "0"))
	int32 Count = 10;

};

/**
 * アイテムばらまきギミック(コリジョン侵入で1度だけ起動する)。
 *
 * プレイヤーがTriggerBoxに入ると、DropAreaBoxの内部からランダムに位置を抽選して
 * Itemsの各エントリをCount個ずつ生成する。起動は1度きり(bTriggered)。
 * 混合比はCountの比で表す(A=60/B=40なら計100個でその比)。
 *
 * 生成はSpawnPerBatch個ずつ複数フレームに分けて行う。UEのアクター生成はゲームスレッド固定で
 * 非同期化できないため、100個規模のNiagara初期化を1フレームに固めないための分散である。
 * SpawnPerBatchをItemCount以上にすれば従来の一括生成に戻る。
 *
 * 落下演出はAMagneticPickup側のbUseGravityに委ねる。無重力のBPを指定した場合は
 * 抽選位置に浮いたまま留まる(その代わり個体のTickがゼロになる)。
 */
UCLASS()
class PRJ_TIDE_P0_API AItemDropGimmick : public AActor
{
	GENERATED_BODY()

public:

	AItemDropGimmick();

protected:

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION()
	void OnTriggerOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
		bool bFromSweep, const FHitResult& SweepResult);

	// ばらまきを起動
	void Trigger();
	// Itemsを個数ぶん平らに展開してシャッフルしPendingItemsを作成
	void BuildPendingItems();
	// 1バッチ分を生成
	void SpawnBatch();
	// 生成位置を1つ決める
	FVector PickSpawnLocation() const;
	// DropAreaBoxの内部を一様にサンプル (回転・スケール込み)
	FVector RandomPointInDropArea() const;

public:

	UPROPERTY(VisibleAnywhere, Category = "Tide|ItemDropGimmick")
	TObjectPtr<USceneComponent> SceneRoot;

	// 起動判定用のオーバーラップ箱
	UPROPERTY(VisibleAnywhere, Category = "Tide|ItemDropGimmick")
	TObjectPtr<UBoxComponent> TriggerBox;

	// アイテムの抽選範囲。当たり判定は持たない
	UPROPERTY(VisibleAnywhere, Category = "Tide|ItemDropGimmick")
	TObjectPtr<UBoxComponent> DropAreaBox;

	// ONならATidePlayerCharacterが入った時のみ起動する
	UPROPERTY(EditAnywhere, Category = "Tide|ItemDropGimmick")
	bool bTriggerByPlayerOnly = true;

	// ばらまくアイテムの設定
	UPROPERTY(EditAnywhere, Category = "Tide|ItemDropGimmick|Drop")
	TArray<FItemDropEntry> Items;

	// 1バッチで生成する数 ※総数以上にすると一括生成になる
	UPROPERTY(EditAnywhere, Category = "Tide|ItemDropGimmick|Batch", meta = (ClampMin = "1"))
	int32 SpawnPerBatch = 4;

	// バッチ間隔・秒 ※既定は約1フレーム
	UPROPERTY(EditAnywhere, Category = "Tide|ItemDropGimmick|Batch", meta = (ClampMin = "0.0"))
	float SpawnBatchInterval = 1.0f / 60.0f;

private:

	static constexpr float MinSeparation = 60.0f; // 個体間の最小水平距離
	static constexpr int32 MaxSampleAttempts = 8; // 位置抽選のリトライ上限

	bool bTriggered = false; // 起動済み(1度きり)

	TArray<TSubclassOf<AMagneticPickup>> PendingItems; // 未生成の残り
	TArray<FVector> PlacedPoints; // MinSeparationの判定用

	FTimerHandle SpawnBatchHandle;

};
