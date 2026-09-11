// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#include "ContentDirectorSubsystem.h"

#include "Engine/DataTable.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "PRJ_TIDE_P0/Data/Director/MapDirectorRow.h"
#include "PRJ_TIDE_P0/Director/ContentDirector.h"
#include "PRJ_TIDE_P0/Director/DirectorSettings.h"
#include "PRJ_TIDE_P0/Director/MapDirector.h"
#include "PRJ_TIDE_P0/PRJ_TIDE_P0.h"

bool UContentDirectorSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);

	return World && World->IsGameWorld();
}

void UContentDirectorSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	LevelAddedHandle   = FWorldDelegates::LevelAddedToWorld.AddUObject(this, &UContentDirectorSubsystem::HandleLevelAdded);
	LevelRemovedHandle = FWorldDelegates::LevelRemovedFromWorld.AddUObject(this, &UContentDirectorSubsystem::HandleLevelRemoved);
}

void UContentDirectorSubsystem::Deinitialize()
{
	FWorldDelegates::LevelAddedToWorld.Remove(LevelAddedHandle);
	FWorldDelegates::LevelRemovedFromWorld.Remove(LevelRemovedHandle);

	for (const TObjectPtr<UMapDirector>& Director : MapDirectors)
	{
		Director->StopDirector();
	}
	MapDirectors.Reset();

	Super::Deinitialize();
}

void UContentDirectorSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	CacheTables();

	bWorldStarted = true;

	// 開始時点で既に読み込まれているレベル (パーシスタント + ストリーミング済み)
	for (const ULevel* Level : InWorld.GetLevels())
	{
		TryCreateMapDirector(Level);
	}
}

void UContentDirectorSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (MapDirectors.IsEmpty()) return;

	const float EvaluateInterval = UDirectorSettings::Get()->ConditionEvaluateInterval;

	for (const TObjectPtr<UMapDirector>& Director : MapDirectors)
	{
		Director->TickDirector(DeltaTime, EvaluateInterval);
	}
}

TStatId UContentDirectorSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UContentDirectorSubsystem, STATGROUP_Tickables);
}

UContentDirector* UContentDirectorSubsystem::FindContentDirector(FName ContentId) const
{
	for (const TObjectPtr<UMapDirector>& Director : MapDirectors)
	{
		if (UContentDirector* Found = Director ? Director->FindContentDirector(ContentId) : nullptr)
		{
			return Found;
		}
	}

	return nullptr;
}

void UContentDirectorSubsystem::HandleLevelAdded(ULevel* Level, UWorld* World)
{
	if (World != GetWorld() || !bWorldStarted) return;

	TryCreateMapDirector(Level);
}

void UContentDirectorSubsystem::HandleLevelRemoved(ULevel* Level, UWorld* World)
{
	if (World != GetWorld()) return;

	DestroyMapDirector(MakeLevelPackageName(Level));
}

void UContentDirectorSubsystem::TryCreateMapDirector(const ULevel* Level)
{
	if (!Level || !MapDirectorTable) return;

	const FName LevelPackageName = MakeLevelPackageName(Level);
	if (LevelPackageName.IsNone()) return;

	// 同じレベルの二重起動を防ぐ
	const bool bAlreadyRunning = MapDirectors.ContainsByPredicate(
		[&](const TObjectPtr<UMapDirector>& Director) { return Director && Director->GetLevelPackageName() == LevelPackageName; });
	if (bAlreadyRunning) return;

	MapDirectorTable->ForeachRow<FMapDirectorRow>(TEXT("UContentDirectorSubsystem::TryCreateMapDirector"),
		[&](const FName&, const FMapDirectorRow& Row)
		{
			const FName RowLevel = FName(*Row.Level.ToSoftObjectPath().GetLongPackageName());
			if (RowLevel != LevelPackageName) return;

			UMapDirector* Director = NewObject<UMapDirector>(this);
			MapDirectors.Add(Director);

			Director->StartDirector(Row, LevelPackageName);
		});
}

void UContentDirectorSubsystem::DestroyMapDirector(FName LevelPackageName)
{
	for (int32 Index = MapDirectors.Num() - 1; Index >= 0; --Index)
	{
		const TObjectPtr<UMapDirector>& Director = MapDirectors[Index];
		if (!Director || Director->GetLevelPackageName() != LevelPackageName) continue;

		Director->StopDirector();
		MapDirectors.RemoveAt(Index);
	}
}

FName UContentDirectorSubsystem::MakeLevelPackageName(const ULevel* Level)
{
	if (!Level) return NAME_None;

	const UPackage* Package = Level->GetOutermost();
	if (!Package) return NAME_None;

	return FName(*UWorld::RemovePIEPrefix(Package->GetName()));
}

void UContentDirectorSubsystem::CacheTables()
{
	const UDirectorSettings* Settings = UDirectorSettings::Get();

	MapDirectorTable = Settings->MapDirectorTable.LoadSynchronous();

	if (!MapDirectorTable)
	{
		UE_LOG(LogPRJ_TIDE_P0, Log, TEXT("[Director] レベル紐づけDTが未設定 (プロジェクト設定 > Game > Tide Content Director)"));
	}
}
