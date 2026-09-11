// Copyright (c) 2026, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.
#include "AnimNotifyState_WeaponTrail.h"

#include "Components/SkeletalMeshComponent.h"

#include "PRJ_TIDE_P0/Actors/Character/Player/TidePlayerCharacter.h"
#include "PRJ_TIDE_P0/Data/Player/PlayerNiagaraTags.h"

UAnimNotifyState_WeaponTrail::UAnimNotifyState_WeaponTrail()
{
	// 既定は通常攻撃トレイル
	NiagaraTag = PlayerNiagaraTags::WEAPON_TRAIL;

	// モンタージュ上で見分けやすいデフォルト色
#if WITH_EDITORONLY_DATA
	NotifyColor = FColor( 120, 200, 255, 255 );
#endif
}

void UAnimNotifyState_WeaponTrail::NotifyBegin( USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference )
{
	Super::NotifyBegin( MeshComp, Animation, TotalDuration, EventReference );

	if ( !MeshComp ) return;

	if ( ATidePlayerCharacter* Player = Cast<ATidePlayerCharacter>( MeshComp->GetOwner() ) )
	{
		Player->StartWeaponTrail( NiagaraTag, LifeTime, SocketName, LocationOffset, RotationOffset );
	}
}

void UAnimNotifyState_WeaponTrail::NotifyEnd( USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference )
{
	Super::NotifyEnd( MeshComp, Animation, EventReference );

	if ( !MeshComp ) return;

	if ( ATidePlayerCharacter* Player = Cast<ATidePlayerCharacter>( MeshComp->GetOwner() ) )
	{
		Player->StopWeaponTrail();
	}
}

FString UAnimNotifyState_WeaponTrail::GetNotifyName_Implementation() const
{
	return FString::Printf( TEXT( "WeaponTrail [%s]" ), *NiagaraTag.ToString() );
}
