// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#include "DirectorObjectBase.h"

UWorld* UDirectorObjectBase::GetWorld() const
{
	// CDOのOuterはパッケージなのでワールドに行き着かない。BPエディタでの評価を弾く
	if (HasAnyFlags(RF_ClassDefaultObject)) return nullptr;

	return Super::GetWorld();
}
