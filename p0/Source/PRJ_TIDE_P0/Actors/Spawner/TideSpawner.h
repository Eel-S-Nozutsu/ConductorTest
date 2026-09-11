// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PRJ_TIDE_P0/Data/Spawner/SpawnAppearance.h"
#include "TideSpawner.generated.h"

class AEnemyCharacter;
class ATideSpawnPoint;
class USphereComponent;
class UDataTable;
class UEnemyDataAsset;

UENUM(BlueprintType)
enum class ESpawnerMode : uint8
{
	Wave,
	Infinite,
};

USTRUCT(BlueprintType)
struct FSpawnEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	TSubclassOf<AEnemyCharacter> EnemyClass;

	// 候補とするSpawnPointsのインデックス ※空なら全候補からランダム選択
	UPROPERTY(EditAnywhere)
	TArray<int32> SpawnPointIndices;

	// 登場演出 ※未設定で即時スポーン
	UPROPERTY(EditAnywhere, Instanced)
	TObjectPtr<USpawnAppearance> Appearance;

	// この敵のタイプ別DA ※未設定ならEnemyClassのBP既定DAを使う
	// タイプごとに攻撃・レンジ・視野まで別にするため、素体BPを増やさずここでDAを差し替える
	UPROPERTY(EditAnywhere)
	TObjectPtr<UEnemyDataAsset> CharacterDataOverride = nullptr;

	// [旧/移行用] タイプ別DAへ移す前の攻撃テーブル上書き
	// ※移し終えたら削除予定既存マップの値を消さないため残置し、
	// CharacterDataOverride未設定のときだけフォールバックで使う
	UPROPERTY(EditAnywhere, meta = (DisplayName = "[旧] 攻撃テーブル(移行用)", RowType = "/Script/PRJ_TIDE_P0.AttackEntry"))
	TObjectPtr<UDataTable> AttackTableOverride = nullptr;

	// AI視線半径を上書き ※-1で無効(EnemyClassのDAのSightRadiusを使う)
	UPROPERTY(EditAnywhere, meta = (ClampMin = "-1.0"))
	float SightRadiusOverride = -1.0f;

};

USTRUCT(BlueprintType)
struct FWaveEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	TArray<FSpawnEntry> Enemies;

	// 前のウェーブ完了からこのウェーブ開始までの待機秒数
	UPROPERTY(EditAnywhere)
	float DelayBeforeWave = 0.0f;

};

DECLARE_MULTICAST_DELEGATE(FOnSpawnerEvent);

/**
 * エリアトリガーと連動して敵をスポーン・管理するアクター。
 * InnerVolume侵入で起動、OuterVolume離脱で停止(ヒステリシス)。
 */
UCLASS()
class PRJ_TIDE_P0_API ATideSpawner : public AActor
{
	GENERATED_BODY()

public:

	ATideSpawner();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void OnConstruction(const FTransform& Transform) override;

	FOnSpawnerEvent OnAllWavesCleared;
	FOnSpawnerEvent OnSpawnerActivated;
	FOnSpawnerEvent OnSpawnerDeactivated;

protected:

	UPROPERTY(VisibleAnywhere, Category = "Tide|Spawner")
	TObjectPtr<USphereComponent> InnerVolume;

	UPROPERTY(VisibleAnywhere, Category = "Tide|Spawner")
	TObjectPtr<USphereComponent> OuterVolume;

	// カリング範囲の可視化用 ※当たり判定は持たず、判定はスポナー中心からの距離で行う
	UPROPERTY(VisibleAnywhere, Category = "Tide|Spawner|Cull")
	TObjectPtr<USphereComponent> CullVolume;

	UPROPERTY(EditAnywhere, Category = "Tide|Spawner")
	ESpawnerMode Mode = ESpawnerMode::Wave;

	// Waveモード
	UPROPERTY(EditAnywhere, Category = "Tide|Spawner", meta = (EditCondition = "Mode == ESpawnerMode::Wave"))
	TArray<FWaveEntry> Waves;

	// Infiniteモード
	UPROPERTY(EditAnywhere, Category = "Tide|Spawner", meta = (EditCondition = "Mode == ESpawnerMode::Infinite"))
	TArray<FSpawnEntry> InfinitePool;

	UPROPERTY(EditAnywhere, Category = "Tide|Spawner", meta = (EditCondition = "Mode == ESpawnerMode::Infinite", ClampMin = "1"))
	int32 MaxAliveCount = 3;

	UPROPERTY(EditAnywhere, Category = "Tide|Spawner", meta = (EditCondition = "Mode == ESpawnerMode::Infinite", ClampMin = "0.0"))
	float RespawnInterval = 5.0f;

	UPROPERTY(EditAnywhere, Category = "Tide|Spawner")
	TArray<TSoftObjectPtr<ATideSpawnPoint>> SpawnPoints;

	// プレイヤーがこの半径(スポナー中心)に入ると敵を生成し、出ると破棄
	// ※遠方の敵を常時TickさせないためInner/Outerより十分大きくすること
	// ※近距離で破棄されないように破棄はエンカウントのリセットを伴う
	// ※再侵入で作り直し0でカリング無効＝従来どおり開始時に生成して常駐
	UPROPERTY(EditAnywhere, Category = "Tide|Spawner|Cull", meta = (ClampMin = "0.0"))
	float CullVolumeRadius = 20000.0f;

	// 全ウェーブクリア時に発火するシグナル名
	// ※扉などのSignalReceiverComponentと一致させるNoneで発火しない
	UPROPERTY(EditAnywhere, Category = "Tide|Spawner|Signal")
	FName ClearedSignalName;

private:

	void Activate();
	void Deactivate();

	// カリング: プレイヤーが範囲に入ったら生成、出たら破棄する
	void UpdateCullPresence();
	bool IsPlayerInCullRange() const;
	void Populate();
	void Despawn();

	void StartWave(int32 WaveIndex);
	AEnemyCharacter* SpawnEnemy(const FSpawnEntry& Entry);
	ATideSpawnPoint* PickSpawnPoint(const FSpawnEntry& Entry) const;

	void TryRespawnInfinite();
	void PollPlayerInVolume();
	void SyncDebugVolumeVisibility();

	UFUNCTION()
	void OnInnerVolumeBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnOuterVolumeEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	UFUNCTION()
	void OnEnemyDied();

	// OnAllWavesClearedに自己バインドし、
	// ClearedSignalNameをシグナルバスへ発火する
	void EmitClearedSignal();

	bool bActive = false;
	bool bPopulated = false;
	int32 CurrentWaveIndex = 0;
	TArray<TWeakObjectPtr<AEnemyCharacter>> AliveEnemies;

	FTimerHandle WaveDelayHandle;
	FTimerHandle RespawnHandle;
	FTimerHandle ActivationPollHandle;
	FTimerHandle ActivationPollStopHandle;
	FTimerHandle CullPollHandle;
	bool bCachedDebugDrawVolumes = false;

};
