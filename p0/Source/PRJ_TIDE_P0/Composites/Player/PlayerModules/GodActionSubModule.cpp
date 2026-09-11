// Copyright (c) 2026, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.

#include "GodActionSubModule.h"

#include "PRJ_TIDE_P0/Actors/Character/Player/TidePlayerCharacter.h"

void UGodActionSubModule::Initialize( UGodActionPlayerModule* InOwnerModule, ATidePlayerCharacter* InOwner )
{
	OwnerModule = InOwnerModule;
	OwnerCharacter = InOwner;
}

const UTidePlayerParamDataAsset* UGodActionSubModule::GetParams() const
{
	return OwnerCharacter ? OwnerCharacter->PlayerParamData : nullptr;
}

float UGodActionSubModule::PlayAnimMontage( const FName& MontageName, float InPlayRate, FName StartSectionName )
{
	if ( !OwnerCharacter ) return 0.0f;
	return OwnerCharacter->PlayAnimMontage( MontageName, InPlayRate, StartSectionName );
}
