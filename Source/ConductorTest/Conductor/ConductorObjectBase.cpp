// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#include "ConductorObjectBase.h"

UWorld* UConductorObjectBase::GetWorld() const
{
	if (HasAnyFlags(RF_ClassDefaultObject)) return nullptr;

	return Super::GetWorld();
}
