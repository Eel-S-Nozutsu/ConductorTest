// Copyright (c) 2026, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.
#include "SlidePassiveSubModule.h"

#include "PRJ_TIDE_P0/Actors/Character/Player/TidePlayerCharacter.h"
#include "PRJ_TIDE_P0/Data/Player/TidePlayerParamDataAsset.h"

void USlidePassiveSubModule::Initialize( USlidePassivePlayerModule* InOwnerModule, ATidePlayerCharacter* InOwner )
{
	OwnerModule = InOwnerModule;
	OwnerCharacter = InOwner;
}

const UTidePlayerParamDataAsset* USlidePassiveSubModule::GetParams() const
{
	return OwnerCharacter ? OwnerCharacter->PlayerParamData : nullptr;
}
