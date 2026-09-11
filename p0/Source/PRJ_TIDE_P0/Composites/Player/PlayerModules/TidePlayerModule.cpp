// Copyright (c) 2026, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.

#include "TidePlayerModule.h"

#include "PRJ_TIDE_P0/Components/Input/InputBufferComponent.h"
#include "PRJ_TIDE_P0/Actors/Character/Player/TidePlayerCharacter.h"
#include "PRJ_TIDE_P0/Data/Animation/AnimMontageListDataAsset.h"
#include "PRJ_TIDE_P0/Data/Tags/TideGameplayTags.h"

bool UTidePlayerModule::TryConsumeCommand( FGameplayTag CommandTag, float BufferTime )
{
	if ( !OwnerCharacter ) return false;
	// 死亡中はバッファ入力アクション（チャージ／攻撃／回避／ジャンプ）を開始させない。全モジュールがこのヘルパ経由で
	// バッファを消費するため、ここで弾けば死亡モーションが他アクションで上書きされるのを一括で防げる
	if ( OwnerCharacter->IsDead() ) return false;
	// 行動不能（State.Common.Disable とその子）中はバッファ入力アクションを一括で弾く
	// （即時型 Request は各 Request 側で Disable をゲート）。
	if ( OwnerCharacter->HasStateTag( TAG_State_Common_Disable ) ) return false;
	if ( UInputBufferComponent* Buffer = OwnerCharacter->GetInputBufferComponent() )
	{
		return Buffer->ConsumeCommand( CommandTag, BufferTime );
	}
	return false;
}

bool UTidePlayerModule::HasCommand( FGameplayTag CommandTag, float BufferTime ) const
{
	if ( !OwnerCharacter ) return false;
	// 死亡中は先行入力の有無も無効扱いにする（TryConsumeCommand と同じ理由）
	if ( OwnerCharacter->IsDead() ) return false;
	// 行動不能中も先行入力の有無を無効扱いにする（TryConsumeCommand と同じ理由）
	if ( OwnerCharacter->HasStateTag( TAG_State_Common_Disable ) ) return false;
	if ( UInputBufferComponent* Buffer = OwnerCharacter->GetInputBufferComponent() )
	{
		return Buffer->HasCommand( CommandTag, BufferTime );
	}
	return false;
}

UAnimMontage* UTidePlayerModule::GetAnimMontage( const FName& MontageName ) const
{
	if ( !OwnerCharacter ) return nullptr;
	return OwnerCharacter->GetAnimMontage( MontageName );
}

float UTidePlayerModule::PlayAnimMontage( const FName& MontageName, float InPlayRate, FName StartSectionName )
{
	if ( !OwnerCharacter ) return 0.0f;
	return OwnerCharacter->PlayAnimMontage( MontageName, InPlayRate, StartSectionName );
}
