// Copyright (c) 2025, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.
#include "NiagaraSystemListDataAsset.h"

UNiagaraSystem* UNiagaraSystemListDataAsset::GetNiagaraSystem( const FName& Label )
{
	// Categories全体を回して、各Categoryの中のEntries(TMap)を検索する
	for ( const auto& Kvp : Categories )
	{
		const FNiagaraCategory& Category = Kvp.Value;
		if ( Category.Entries.Contains( Label ) )
		{
			return Category.Entries[Label].NiagaraSystem;
		}
	}
	return nullptr;
}
