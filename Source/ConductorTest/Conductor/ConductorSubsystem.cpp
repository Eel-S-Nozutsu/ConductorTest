// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#include "ConductorSubsystem.h"

#include "Conductor/MapConductor.h"
#include "Conductor/MapConductorRow.h"
#include "Conductor/ContentConductor.h"
#include "Conductor/ConductorSettings.h"

bool UConductorSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && World->IsGameWorld();
}

void UConductorSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	LevelAddedHandle = FWorldDelegates::LevelAddedToWorld.AddUObject(
		this, &UConductorSubsystem::HandleLevelAdded);
	LevelRemovedHandle = FWorldDelegates::LevelRemovedFromWorld.AddUObject(
		this, &UConductorSubsystem::HandleLevelRemoved);
}

void UConductorSubsystem::Deinitialize()
{
	FWorldDelegates::LevelAddedToWorld.Remove(LevelAddedHandle);
	FWorldDelegates::LevelRemovedFromWorld.Remove(LevelRemovedHandle);

	for (const TObjectPtr<UMapConductor>& Conductor : MapConductors)
	{
		Conductor->StopConductor();
	}
	MapConductors.Reset();

	Super::Deinitialize();
}

void UConductorSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	CacheTables();

	bWorldStarted = true;

	// すでに読み込まれているレベルがあった場合の対処
	// UEのライフサイクル上起こりえないのであれば不要なので要検証
	// パーシスタントとストリーミング済みとかが該当するのかな？
	for (const ULevel* Level : InWorld.GetLevels())
	{
		TryCreateMapConductor(Level);
	}
}

void UConductorSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (MapConductors.IsEmpty()) return;

	const float EvaluateInterval = UConductorSettings::Get()->ConditionEvaluateInterval;

	for (const TObjectPtr<UMapConductor>& Conductor : MapConductors)
	{
		Conductor->TickConductor(DeltaTime, EvaluateInterval);
	}
}

TStatId UConductorSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UConductorSubsystem, STATGROUP_Tickables);
}

UContentConductor* UConductorSubsystem::FindContentConductor(FName ContentId) const
{
	for (const TObjectPtr<UMapConductor>& Conductor : MapConductors)
	{
		if (UContentConductor* Found = Conductor ? Conductor->FindContentConductor(ContentId) : nullptr)
		{
			return Found;
		}
	}

	return nullptr;
}

void UConductorSubsystem::HandleLevelAdded(ULevel* Level, UWorld* World)
{
	if (World != GetWorld() || !bWorldStarted) return;

	TryCreateMapConductor(Level);
}

void UConductorSubsystem::HandleLevelRemoved(ULevel* Level, UWorld* World)
{
	if (World != GetWorld()) return;

	DestroyMapConductor(MakeLevelPackageName(Level));
}

void UConductorSubsystem::TryCreateMapConductor(const ULevel* Level)
{
	if (!Level || !MapConductorTable) return;

	const FName LevelPackageName = MakeLevelPackageName(Level);
	if (LevelPackageName.IsNone()) return;

	// 二重起動の防止
	const bool bAlreadyRunning = MapConductors.ContainsByPredicate(
		[&](const TObjectPtr<UMapConductor>& Conductor)
		{
			return Conductor && Conductor->GetLevelPackageName() == LevelPackageName;
		});
	if (bAlreadyRunning) return;

	MapConductorTable->ForeachRow<FMapConductorRow>(
		TEXT("UConductorSubsystem::TryCreateMapConductor"),
		[&](const FName&, const FMapConductorRow& Row)
		{
			const FName RowLevel = FName(*Row.Level.ToSoftObjectPath().GetLongPackageName());
			if (RowLevel != LevelPackageName) return;

			UClass* ConductorClass	 = UMapConductor::StaticClass();
			UMapConductor* Conductor = NewObject<UMapConductor>(this, ConductorClass);
			MapConductors.Add(Conductor);

			Conductor->StartConductor(Row, LevelPackageName);
		});
}

void UConductorSubsystem::DestroyMapConductor(FName LevelPackageName)
{
	for (int32 Index = MapConductors.Num() - 1; Index >= 0; --Index)
	{
		const TObjectPtr<UMapConductor>& Conductor = MapConductors[Index];
		if (!Conductor || Conductor->GetLevelPackageName() != LevelPackageName) continue;

		Conductor->StopConductor();
		MapConductors.RemoveAt(Index);
	}
}

FName UConductorSubsystem::MakeLevelPackageName(const ULevel* Level)
{
	if (!Level) return NAME_None;

	const UPackage* Package = Level->GetOutermost();
	if (!Package) return NAME_None;

	return FName(*UWorld::RemovePIEPrefix(Package->GetName()));
}

void UConductorSubsystem::CacheTables()
{
	const UConductorSettings* Settings = UConductorSettings::Get();

	MapConductorTable = Settings->MapConductorTable.LoadSynchronous();

	if (!MapConductorTable)
	{
		UE_LOG(LogTemp, Log, TEXT("[Conductor] レベル紐づけDTが未設定"));
	}
}
