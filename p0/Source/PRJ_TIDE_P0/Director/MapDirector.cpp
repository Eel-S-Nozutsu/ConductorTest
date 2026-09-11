// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#include "MapDirector.h"

#include "PRJ_TIDE_P0/Data/Director/ContentDirectorRow.h"
#include "PRJ_TIDE_P0/Data/Director/MapDirectorRow.h"
#include "PRJ_TIDE_P0/Director/ContentDirector.h"
#include "PRJ_TIDE_P0/Director/DirectorModule.h"
#include "PRJ_TIDE_P0/PRJ_TIDE_P0.h"

void UMapDirector::StartDirector(const FMapDirectorRow& Row, FName InLevelPackageName)
{
	LevelPackageName = InLevelPackageName;

	for (const TSubclassOf<UMapDirectorModule>& ModuleClass : Row.Modules)
	{
		if (!ModuleClass) continue;

		UMapDirectorModule* Module = NewObject<UMapDirectorModule>(this, ModuleClass);
		Modules.Add(Module);
	}

	bStarted = true;

	for (const TObjectPtr<UMapDirectorModule>& Module : Modules)
	{
		Module->StartModule();
	}

	ContentList = Row.ContentList.LoadSynchronous();
	if (!ContentList) return;

	// 一覧はレベルごとに1枚なので、全行がこのレベルのコンテンツ
	ContentList->ForeachRow<FContentDirectorRow>(TEXT("UMapDirector::StartDirector"),
		[&](const FName& RowName, const FContentDirectorRow& ContentRow)
		{
			UContentDirector* Director = NewObject<UContentDirector>(this);
			ContentDirectors.Add(Director);

			Director->StartDirector(RowName, ContentRow);
		});

	UE_LOG(LogPRJ_TIDE_P0, Log, TEXT("[Director] %s: MapDirector起動 (コンテンツ %d 件)"),
		*LevelPackageName.ToString(), ContentDirectors.Num());
}

void UMapDirector::StopDirector()
{
	if (!bStarted) return;

	for (const TObjectPtr<UContentDirector>& Director : ContentDirectors)
	{
		Director->StopDirector();
	}
	ContentDirectors.Reset();

	for (const TObjectPtr<UMapDirectorModule>& Module : Modules)
	{
		Module->StopModule();
	}
	Modules.Reset();

	bStarted = false;
}

void UMapDirector::TickDirector(float DeltaSeconds, float EvaluateInterval)
{
	if (!bStarted) return;

	for (const TObjectPtr<UMapDirectorModule>& Module : Modules)
	{
		Module->TickModule(DeltaSeconds);
	}

	for (const TObjectPtr<UContentDirector>& Director : ContentDirectors)
	{
		Director->TickDirector(DeltaSeconds, EvaluateInterval);
	}
}

UContentDirector* UMapDirector::FindContentDirector(FName ContentId) const
{
	for (const TObjectPtr<UContentDirector>& Director : ContentDirectors)
	{
		if (Director && Director->GetContentId() == ContentId)
		{
			return Director;
		}
	}

	return nullptr;
}
