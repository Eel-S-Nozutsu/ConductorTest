// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#include "TideSpawner.h"
#include "TideSpawnPoint.h"
#include "PRJ_TIDE_P0/Actors/Character/Enemy/EnemyCharacter.h"
#include "PRJ_TIDE_P0/Data/Enemy/EnemyDataAsset.h"
#include "PRJ_TIDE_P0/Components/Combat/StatusComponent.h"
#include "PRJ_TIDE_P0/Core/TideGameSettings.h"

#include "AIController.h"
#include "PRJ_TIDE_P0/AI/Controllers/Enemy/EnemyAIController.h"
#include "PRJ_TIDE_P0/AI/Components/Enemy/EnemyThreatComponent.h"
#include "Components/SphereComponent.h"
#include "Kismet/GameplayStatics.h"
#include "PRJ_TIDE_P0/Actors/Character/Player/TidePlayerCharacter.h"
#include "PRJ_TIDE_P0/Subsystems/Signal/TideSignalSubsystem.h"

#if WITH_EDITOR
#include "Editor.h"
#endif

ATideSpawner::ATideSpawner()
{
	PrimaryActorTick.bCanEverTick = true;

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	InnerVolume = CreateDefaultSubobject<USphereComponent>(TEXT("InnerVolume"));
	InnerVolume->SetupAttachment(RootComponent);
	InnerVolume->SetSphereRadius(1000.0f);
	InnerVolume->SetCollisionResponseToAllChannels(ECR_Ignore);
	InnerVolume->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	InnerVolume->ShapeColor = FColor::Green;

	OuterVolume = CreateDefaultSubobject<USphereComponent>(TEXT("OuterVolume"));
	OuterVolume->SetupAttachment(RootComponent);
	OuterVolume->SetSphereRadius(1500.0f);
	OuterVolume->SetCollisionResponseToAllChannels(ECR_Ignore);
	OuterVolume->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	OuterVolume->ShapeColor = FColor::Yellow;

	// カリング範囲の可視化専用。当たり判定は持たない(判定は距離で行う)
	CullVolume = CreateDefaultSubobject<USphereComponent>(TEXT("CullVolume"));
	CullVolume->SetupAttachment(RootComponent);
	CullVolume->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CullVolume->ShapeColor = FColor::Cyan;

	InnerVolume->bHiddenInGame = true;
	OuterVolume->bHiddenInGame = true;
	CullVolume->bHiddenInGame = true;
}

void ATideSpawner::BeginPlay()
{
	Super::BeginPlay();
	SyncDebugVolumeVisibility();

	InnerVolume->OnComponentBeginOverlap.AddDynamic(this, &ATideSpawner::OnInnerVolumeBeginOverlap);
	OuterVolume->OnComponentEndOverlap.AddDynamic(this, &ATideSpawner::OnOuterVolumeEndOverlap);

	OnAllWavesCleared.AddUObject(this, &ATideSpawner::EmitClearedSignal);

	if (CullVolumeRadius > 0.0f)
	{
		// 生成は「プレイヤーが範囲に入ったら」に遅延する(BeginPlayで全部湧かせない)
		// 低頻度ポーリングで在圏を監視し、入ったら生成・出たら破棄する
		GetWorldTimerManager().SetTimer(CullPollHandle, this,
			&ATideSpawner::UpdateCullPresence, 0.3f, true);
		UpdateCullPresence();
	}
	else
	{
		// カリング無効: 従来どおり開始時に生成して常駐する
		Populate();
	}

	// PlayerStart経由でスポーンする場合、BeginPlay時点ではポーンがまだ存在しないため
	// 短いインターバルでポーリングし、InnerVolume内に出現したらActivateする
	// 初期スポーン検出が目的なので3秒で自動停止する
	GetWorldTimerManager().SetTimer(ActivationPollHandle, this,
		&ATideSpawner::PollPlayerInVolume, 0.1f, true);
	GetWorldTimerManager().SetTimer(ActivationPollStopHandle,
		[this]() { GetWorldTimerManager().ClearTimer(ActivationPollHandle); },
		3.0f, false);
}

void ATideSpawner::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	SyncDebugVolumeVisibility();
}

void ATideSpawner::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	// エディタ配置時にカリング範囲を可視化する(0＝無効のときは描かない)
	if (CullVolume)
	{
		CullVolume->SetSphereRadius(CullVolumeRadius);
	}
}

void ATideSpawner::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);

	GetWorldTimerManager().ClearTimer(ActivationPollHandle);
	GetWorldTimerManager().ClearTimer(ActivationPollStopHandle);
	GetWorldTimerManager().ClearTimer(WaveDelayHandle);
	GetWorldTimerManager().ClearTimer(RespawnHandle);
	GetWorldTimerManager().ClearTimer(CullPollHandle);
}

void ATideSpawner::PollPlayerInVolume()
{
	if (bActive)
	{
		GetWorldTimerManager().ClearTimer(ActivationPollHandle);
		return;
	}

	TArray<AActor*> Overlapping;
	InnerVolume->GetOverlappingActors(Overlapping, ATidePlayerCharacter::StaticClass());
	if (!Overlapping.IsEmpty())
	{
		GetWorldTimerManager().ClearTimer(ActivationPollHandle);
		Activate();
	}
}

void ATideSpawner::SyncDebugVolumeVisibility()
{
#if !UE_BUILD_SHIPPING
	const bool bShowVolumes = UTideGameSettings::Get()->bDebugDrawSpawnerVolumes;
#else
	constexpr bool bShowVolumes = false;
#endif

	if (bShowVolumes == bCachedDebugDrawVolumes) return;

	bCachedDebugDrawVolumes = bShowVolumes;
	if (InnerVolume)
	{
		InnerVolume->SetHiddenInGame(!bShowVolumes);
	}
	if (OuterVolume)
	{
		OuterVolume->SetHiddenInGame(!bShowVolumes);
	}
	if (CullVolume)
	{
		CullVolume->SetHiddenInGame(!bShowVolumes);
	}
}

// ------------------------------------------------------------
// トリガー
// ------------------------------------------------------------

void ATideSpawner::OnInnerVolumeBeginOverlap(UPrimitiveComponent*, AActor* OtherActor,
	UPrimitiveComponent*, int32, bool, const FHitResult&)
{
	if (bActive) return;
	if (!Cast<ATidePlayerCharacter>(OtherActor)) return;

	Activate();
}

void ATideSpawner::OnOuterVolumeEndOverlap(UPrimitiveComponent*, AActor* OtherActor,
	UPrimitiveComponent*, int32)
{
	// スポナー未起動(Inner未侵入)でも、検知ゲージで自力交戦している敵がいるため
	// bActiveで早期リターンしない。Deactivateは冪等
	if (!Cast<ATidePlayerCharacter>(OtherActor)) return;

	// 検知の無効化はOnSpawnerDeactivatedを購読する各
	// AEnemyAIControllerが行う
	// (Deactivate内でBroadcast)
	Deactivate();
}

// ------------------------------------------------------------
// カリング(近接で生成 / 離脱で破棄)
// ------------------------------------------------------------

void ATideSpawner::UpdateCullPresence()
{
	const bool bInRange = IsPlayerInCullRange();
	if (bInRange && !bPopulated)
	{
		Populate();
	}
	else if (!bInRange && bPopulated)
	{
		Despawn();
	}
}

bool ATideSpawner::IsPlayerInCullRange() const
{
	const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!Player) return false;

	return FVector::DistSquared(Player->GetActorLocation(), GetActorLocation()) <= FMath::Square(CullVolumeRadius);
}

void ATideSpawner::Populate()
{
	if (bPopulated) return;
	bPopulated = true;

	// 破棄→再侵入でもWave[0]から作り直す(離脱＝エンカウントのリセット)
	CurrentWaveIndex = 0;

	if (Mode == ESpawnerMode::Wave)
	{
		if (Waves.IsEmpty()) return;
		for (const FSpawnEntry& Entry : Waves[0].Enemies)
		{
			if (AEnemyCharacter* Enemy = SpawnEnemy(Entry))
			{
				AliveEnemies.Add(Enemy);
			}
		}
	}
	else
	{
		if (InfinitePool.IsEmpty()) return;
		for (int32 i = 0; i < MaxAliveCount; ++i)
		{
			const int32 Idx = FMath::RandRange(0, InfinitePool.Num() - 1);
			if (AEnemyCharacter* Enemy = SpawnEnemy(InfinitePool[Idx]))
			{
				AliveEnemies.Add(Enemy);
			}
		}
	}

	// 既にエンカウント中(プレイヤーがInner内)に生成された場合、
	// 生成前に飛んだOnSpawnerActivatedを受け取れないので、ここで交戦状態を配り直す
	if (bActive)
	{
		OnSpawnerActivated.Broadcast();
	}
}

void ATideSpawner::Despawn()
{
	if (!bPopulated) return;
	bPopulated = false;

	// 進行中の交戦・ウェーブ・湧きをすべて止めてリセットする
	bActive = false;
	GetWorldTimerManager().ClearTimer(WaveDelayHandle);
	GetWorldTimerManager().ClearTimer(RespawnHandle);
	CurrentWaveIndex = 0;

	// 死亡ではなく破棄なのでOnEnemyDiedは通らない(ウェーブは進めない)
	for (const TWeakObjectPtr<AEnemyCharacter>& Ptr : AliveEnemies)
	{
		if (AEnemyCharacter* Enemy = Ptr.Get())
		{
			Enemy->Destroy();
		}
	}
	AliveEnemies.Reset();
}

// ------------------------------------------------------------
// 起動・停止
// ------------------------------------------------------------

void ATideSpawner::Activate()
{
	// カリングで未生成のままInnerに到達した場合に備え、先に生成しておく
	// (bActiveを立てる前に呼ぶのでPopulate側の二重Broadcastは起きない)
	if (!bPopulated) Populate();

	bActive = true;
	// 検知の有効化はOnSpawnerActivatedを購読する各
	// AEnemyAIControllerが行う
	OnSpawnerActivated.Broadcast();

	// 再侵入時: 離脱中に全員が死亡していた場合の継続処理
	AliveEnemies.RemoveAll([](const TWeakObjectPtr<AEnemyCharacter>& Ptr) { return !Ptr.IsValid(); });

	if (AliveEnemies.IsEmpty())
	{
		if (Mode == ESpawnerMode::Wave)
		{
			const int32 NextWave = CurrentWaveIndex + 1;
			if (Waves.IsValidIndex(NextWave))
			{
				StartWave(NextWave);
			}
			else
			{
				bActive = false;
				OnAllWavesCleared.Broadcast();
			}
		}
		else
		{
			TryRespawnInfinite();
		}
	}
}

void ATideSpawner::Deactivate()
{
	bActive = false;
	GetWorldTimerManager().ClearTimer(WaveDelayHandle);
	GetWorldTimerManager().ClearTimer(RespawnHandle);
	OnSpawnerDeactivated.Broadcast();
}

// ------------------------------------------------------------
// Waveモード
// ------------------------------------------------------------

void ATideSpawner::StartWave(int32 WaveIndex)
{
	if (!Waves.IsValidIndex(WaveIndex)) return;

	CurrentWaveIndex = WaveIndex;

	// Wave[0]はPopulate(カリング侵入時)で生成済み
	if (WaveIndex == 0) return;

	for (const FSpawnEntry& Entry : Waves[WaveIndex].Enemies)
	{
		if (AEnemyCharacter* Enemy = SpawnEnemy(Entry))
		{
			AliveEnemies.Add(Enemy);
		}
	}
}

// ------------------------------------------------------------
// Infiniteモード
// ------------------------------------------------------------

void ATideSpawner::TryRespawnInfinite()
{
	if (!bActive || InfinitePool.IsEmpty()) return;

	AliveEnemies.RemoveAll([](const TWeakObjectPtr<AEnemyCharacter>& Ptr) { return !Ptr.IsValid(); });

	while (AliveEnemies.Num() < MaxAliveCount)
	{
		const int32 PoolIndex = FMath::RandRange(0, InfinitePool.Num() - 1);
		if (AEnemyCharacter* Enemy = SpawnEnemy(InfinitePool[PoolIndex]))
		{
			AliveEnemies.Add(Enemy);
		}
		else
		{
			break;
		}
	}
}

// ------------------------------------------------------------
// 死亡コールバック
// ------------------------------------------------------------

void ATideSpawner::OnEnemyDied()
{
	AliveEnemies.RemoveAll([](const TWeakObjectPtr<AEnemyCharacter>& Ptr)
	{
		if (!Ptr.IsValid()) return true;
		UStatusComponent* Status = Ptr->GetStatusComponent();
		return Status && Status->IsDead();
	});

	if (!bActive) return;

	if (Mode == ESpawnerMode::Wave)
	{
		if (AliveEnemies.IsEmpty())
		{
			const int32 NextWave = CurrentWaveIndex + 1;
			if (Waves.IsValidIndex(NextWave))
			{
				const float Delay = Waves[NextWave].DelayBeforeWave;
				if (Delay > 0.0f)
				{
					GetWorldTimerManager().SetTimer(WaveDelayHandle,
						[this, NextWave]() { StartWave(NextWave); },
						Delay, false);
				}
				else
				{
					StartWave(NextWave);
				}
			}
			else
			{
				bActive = false;
				OnAllWavesCleared.Broadcast();
			}
		}
	}
	else
	{
		GetWorldTimerManager().SetTimer(RespawnHandle,
			this, &ATideSpawner::TryRespawnInfinite,
			RespawnInterval, false);
	}
}

void ATideSpawner::EmitClearedSignal()
{
	if (ClearedSignalName.IsNone()) return;

	if (UTideSignalSubsystem* Signals = GetWorld() ? GetWorld()->GetSubsystem<UTideSignalSubsystem>() : nullptr)
	{
		Signals->BroadcastSignal(ClearedSignalName);
	}
}

// ------------------------------------------------------------
// スポーン共通
// ------------------------------------------------------------

AEnemyCharacter* ATideSpawner::SpawnEnemy(const FSpawnEntry& Entry)
{
	if (!Entry.EnemyClass) return nullptr;

	ATideSpawnPoint* const SpawnPoint = PickSpawnPoint(Entry);
	const FTransform SpawnTransform = SpawnPoint ? SpawnPoint->GetActorTransform() : GetActorTransform();

	AEnemyCharacter* Enemy = GetWorld()->SpawnActorDeferred<AEnemyCharacter>(
		Entry.EnemyClass, SpawnTransform,
		nullptr, nullptr,
		ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);

	if (!Enemy) return nullptr;

	if (SpawnPoint && SpawnPoint->HasWaitSettingsOverride())
	{
		Enemy->ApplyWaitSettings(SpawnPoint->GetWaitSettings());
	}

	// タイプ別DAの注入はOnConstruction/Initializeより前
	// (FinishSpawning前) に行う
	// 未設定なら旧AttackTableOverrideをフォールバックで使う (移行期間中)
	if (Entry.CharacterDataOverride)
	{
		Enemy->SetCharacterData(Entry.CharacterDataOverride);
	}
	else if (Entry.AttackTableOverride)
	{
		Enemy->SetAttackTableOverride(Entry.AttackTableOverride);
	}

	// 視線半径の上書きはOnPossessで参照するためFinishSpawningより前に注入する
	if (Entry.SightRadiusOverride >= 0.0f)
	{
		Enemy->SetSightRadiusOverride(Entry.SightRadiusOverride);
	}

	Enemy->FinishSpawning(SpawnTransform);

	if (!Enemy->GetController())
	{
		Enemy->SpawnDefaultController();
	}

	if (UStatusComponent* Status = Enemy->GetStatusComponent())
	{
		Status->OnDeath.AddDynamic(this, &ATideSpawner::OnEnemyDied);
	}

	if (AEnemyAIController* AIC = Cast<AEnemyAIController>(Enemy->GetController()))
	{
		// テリトリー (BB.PatrolOrigin/PatrolRadius)
		// の書き込みはThreatが単一管理する
		if (UEnemyThreatComponent* Threat = AIC->ThreatComponent)
		{
			// 徘徊圏=Inner / 交戦圏=Outer。交戦の維持もOuterまで(エンカウントの
			// ヒステリシスと帰還判定の境界を揃える)
			Threat->SetTerritory(GetActorLocation(),
				InnerVolume->GetScaledSphereRadius(),
				OuterVolume->GetScaledSphereRadius());

			// 交戦ゲートをスポナーの現状に合わせる (既定trueは手置き敵向けの値)
			// 未エンカウントで生成しないと湧いた瞬間から交戦扱いになり検知ゲージが働かない。
			// 逆に交戦中の増援 (StartWave) は気づかせた状態で出す
			Threat->SetEngaged(bActive);
		}

		// エンカウント起動/停止を購読させる。スポナーは知覚コンポーネントを直接触らない
		// 死亡した敵のbindingは非動的マルチキャストのBroadcast時に自動コンパクトされる
		OnSpawnerActivated.AddUObject(AIC, &AEnemyAIController::HandleEncounterActivated);
		OnSpawnerDeactivated.AddUObject(AIC, &AEnemyAIController::HandleEncounterDeactivated);
	}

	if (Entry.Appearance)
	{
		TArray<FTransform> Points = { SpawnTransform };
		Entry.Appearance->Execute(Enemy, Points);
	}

	return Enemy;
}

ATideSpawnPoint* ATideSpawner::PickSpawnPoint(const FSpawnEntry& Entry) const
{
	TArray<int32> Candidates = Entry.SpawnPointIndices;

	Candidates.RemoveAll([this](int32 Idx) { return !SpawnPoints.IsValidIndex(Idx); });
	if (Candidates.IsEmpty())
	{
		for (int32 i = 0; i < SpawnPoints.Num(); ++i) Candidates.Add(i);
	}

	if (Candidates.IsEmpty()) return nullptr;

	const int32 Chosen = Candidates[FMath::RandRange(0, Candidates.Num() - 1)];
	return SpawnPoints[Chosen].Get();
}
