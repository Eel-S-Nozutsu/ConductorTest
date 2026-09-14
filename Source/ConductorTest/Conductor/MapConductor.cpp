// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#include "MapConductor.h"

#include "Conductor/ContentConductor.h"
#include "Conductor/ConductorModule.h"
#include "Conductor/Data/MapConductorRow.h"
#include "Conductor/Data/ContentConductorRow.h"

void UMapConductor::StartConductor(const FMapConductorRow& Row, FName InLevelPackageName)
{
	LevelPackageName = InLevelPackageName;

	for (const TSubclassOf<UMapConductorModule>& ModuleClass : Row.Modules)
	{
		if (!ModuleClass) continue;

		UMapConductorModule* Module = NewObject<UMapConductorModule>(this, ModuleClass);
		Modules.Add(Module);
	}

	bStarted = true;

	// 開始は全モジュールが生成されている前提としたい
	for (const TObjectPtr<UMapConductorModule>& Module : Modules)
	{
		Module->StartModule();
	}

	ContentConductorList = Row.ContentConductorList.LoadSynchronous();
	if (!ContentConductorList) return;

	ContentConductorList->ForeachRow<FContentConductorRow>(
		TEXT("UMapConductor::StartConductor"),
		[&](const FName& RowName, const FContentConductorRow& ContentRow)
		{
			UClass* ConductorClass		 = UContentConductor::StaticClass();
			UContentConductor* Conductor = NewObject<UContentConductor>(this, ConductorClass);
			ContentConductors.Add(Conductor);

			Conductor->StartConductor(RowName, ContentRow);
		});

	UE_LOG(LogTemp, Log, TEXT("[Conductor] %s: MapConductor起動 (コンテンツ %d 件)"), *LevelPackageName.ToString(), ContentConductors.Num());
}

void UMapConductor::StopConductor()
{
	if (!bStarted) return;

	for (const TObjectPtr<UContentConductor>& Conductor : ContentConductors)
	{
		Conductor->StopConductor();
	}
	ContentConductors.Reset();

	for (const TObjectPtr<UMapConductorModule>& Module : Modules)
	{
		Module->StopModule();
	}
	Modules.Reset();

	bStarted = false;
}

void UMapConductor::TickConductor(float DeltaSeconds, float EvaluateInterval)
{
	if (!bStarted) return;

	for (const TObjectPtr<UMapConductorModule>& Module : Modules)
	{
		Module->TickModule(DeltaSeconds);
	}

	for (const TObjectPtr<UContentConductor>& Conductor : ContentConductors)
	{
		Conductor->TickConductor(DeltaSeconds, EvaluateInterval);
	}
}

UContentConductor* UMapConductor::FindContentConductor(FName ContentId) const
{
	for (const TObjectPtr<UContentConductor>& Conductor : ContentConductors)
	{
		if (Conductor && Conductor->GetContentId() == ContentId)
		{
			return Conductor;
		}
	}

	return nullptr;
}
