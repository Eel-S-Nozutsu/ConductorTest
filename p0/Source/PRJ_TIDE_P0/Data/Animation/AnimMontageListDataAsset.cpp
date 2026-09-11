// Copyright (c) 2025, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.
#include "AnimMontageListDataAsset.h"

UAnimMontage* UAnimMontageListDataAsset::GetAnimMontage( const FName& Label )
{
	// Categories全体を回して、各Categoryの中のEntries(TMap)を検索する
	for ( const auto& Kvp : Categories )
	{
		const FAnimCategory& Category = Kvp.Value;
		if ( Category.Entries.Contains( Label ) )
		{
			return Category.Entries[Label].AnimMontage;
		}
	}
	return nullptr;
}
