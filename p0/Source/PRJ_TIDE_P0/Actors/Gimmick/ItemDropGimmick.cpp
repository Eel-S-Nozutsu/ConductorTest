// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#include "ItemDropGimmick.h"

#include "Components/SceneComponent.h"
#include "Components/BoxComponent.h"

#include "PRJ_TIDE_P0/Actors/Character/Player/TidePlayerCharacter.h"
#include "PRJ_TIDE_P0/Actors/Item/MagneticPickup.h"

AItemDropGimmick::AItemDropGimmick()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));
	TriggerBox->SetupAttachment(SceneRoot);
	TriggerBox->SetBoxExtent(FVector(200.0f, 200.0f, 200.0f));
	TriggerBox->ShapeColor = FColor::Green;
	TriggerBox->bHiddenInGame = true;

	DropAreaBox = CreateDefaultSubobject<UBoxComponent>(TEXT("DropAreaBox"));
	DropAreaBox->SetupAttachment(SceneRoot);
	DropAreaBox->SetBoxExtent(FVector(500.0f, 500.0f, 200.0f));
	DropAreaBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	DropAreaBox->ShapeColor = FColor::Cyan;
	DropAreaBox->bHiddenInGame = true;
}

void AItemDropGimmick::BeginPlay()
{
	Super::BeginPlay();

	// デリゲートはランタイムで束縛する(コンストラクタ束縛は古い配置個体で剥がれる)。
	// ストリーミング再入の二重束縛を避けるためAddUnique、EndPlayで解除
	TriggerBox->OnComponentBeginOverlap.AddUniqueDynamic(this, &AItemDropGimmick::OnTriggerOverlap);

	// BPに古いコリジョン設定が焼き付いていても上書きするためランタイムで張り直す。
	// Visibilityを含む全チャンネルをIgnoreするのが重要で、落下中のAMagneticPickupは
	// ECC_Visibilityで床を探すため、Blockしているとこの箱の天面に着地して浮いてしまう
	TriggerBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	TriggerBox->SetCollisionObjectType(ECC_WorldDynamic);
	TriggerBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	TriggerBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	TriggerBox->SetGenerateOverlapEvents(true);
}

void AItemDropGimmick::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// BeginPlayで張ったオーバーラップデリゲートを解除する
	TriggerBox->OnComponentBeginOverlap.RemoveDynamic(this, &AItemDropGimmick::OnTriggerOverlap);

	GetWorldTimerManager().ClearTimer(SpawnBatchHandle);

	Super::EndPlay(EndPlayReason);
}

void AItemDropGimmick::OnTriggerOverlap(UPrimitiveComponent* /*OverlappedComp*/, AActor* OtherActor,
	UPrimitiveComponent* /*OtherComp*/, int32 /*OtherBodyIndex*/,
	bool /*bFromSweep*/, const FHitResult& /*SweepResult*/)
{
	if (bTriggered) return;

	if (bTriggerByPlayerOnly && !Cast<ATidePlayerCharacter>(OtherActor)) return;

	Trigger();
}

void AItemDropGimmick::Trigger()
{
	if (bTriggered) return;
	bTriggered = true;

	// 以降は起動しないよう判定を切る(1度きり)
	if (TriggerBox)
	{
		TriggerBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	BuildPendingItems();
	if (PendingItems.IsEmpty()) return;

	PlacedPoints.Reset();
	PlacedPoints.Reserve(PendingItems.Num());

	// 初回分は待たずに出し、残りを反復タイマーで消化する
	SpawnBatch();
	if (!PendingItems.IsEmpty())
	{
		GetWorldTimerManager().SetTimer(SpawnBatchHandle, this,
			&AItemDropGimmick::SpawnBatch, FMath::Max(KINDA_SMALL_NUMBER, SpawnBatchInterval), true);
	}
}

void AItemDropGimmick::BuildPendingItems()
{
	PendingItems.Reset();

	for (const FItemDropEntry& Entry : Items)
	{
		if (!Entry.ItemClass || Entry.Count <= 0) continue;

		for (int32 i = 0; i < Entry.Count; ++i)
		{
			PendingItems.Add(Entry.ItemClass);
		}
	}

	// フレーム分散でエントリ順に出すと種類ごとに固まって出現してしまうため、
	// 混ざった順で出るようフィッシャー–イェーツで並べ替える
	for (int32 i = PendingItems.Num() - 1; i > 0; --i)
	{
		PendingItems.Swap(i, FMath::RandRange(0, i));
	}
}

void AItemDropGimmick::SpawnBatch()
{
	UWorld* World = GetWorld();
	if (!World) return;

	FActorSpawnParameters Params;
	Params.Owner = this;
	// 重なりで生成が失敗して個数が減らないようにする
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	for (int32 i = 0; i < SpawnPerBatch && !PendingItems.IsEmpty(); ++i)
	{
		const FVector Location = PickSpawnLocation();
		World->SpawnActor<AMagneticPickup>(PendingItems.Pop(EAllowShrinking::No),
			Location, FRotator::ZeroRotator, Params);

		PlacedPoints.Add(Location);
	}

	if (PendingItems.IsEmpty())
	{
		GetWorldTimerManager().ClearTimer(SpawnBatchHandle);
		PendingItems.Empty();
		PlacedPoints.Empty();
	}
}

FVector AItemDropGimmick::PickSpawnLocation() const
{
	FVector Candidate = RandomPointInDropArea();
	if (MinSeparation <= 0.0f) return Candidate;

	// 既存の点と近すぎたら引き直す。試行を使い切ったら最後の候補をそのまま採り、
	// 指定個数を必ず満たす(狭い範囲に大量指定された場合に個数が欠けないようにする)
	for (int32 Attempt = 1; Attempt < MaxSampleAttempts; ++Attempt)
	{
		const bool bTooClose = PlacedPoints.ContainsByPredicate([&](const FVector& Placed)
		{
			return FVector::Dist2D(Placed, Candidate) < MinSeparation;
		});
		if (!bTooClose) break;

		Candidate = RandomPointInDropArea();
	}

	return Candidate;
}

FVector AItemDropGimmick::RandomPointInDropArea() const
{
	// ローカル±Extentで抽選してからワールドへ変換する(箱の回転・スケールを反映させる)
	const FVector Extent = DropAreaBox->GetUnscaledBoxExtent();
	const FVector Local(
		FMath::FRandRange(-Extent.X, Extent.X),
		FMath::FRandRange(-Extent.Y, Extent.Y),
		FMath::FRandRange(-Extent.Z, Extent.Z));

	return DropAreaBox->GetComponentTransform().TransformPosition(Local);
}
