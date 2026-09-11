// Copyright (c) 2026, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.
#include "PlayerAnimInstance.h"

#include "PRJ_TIDE_P0/Actors/Character/Player/TidePlayerCharacter.h"
#include "PRJ_TIDE_P0/Components/Targeting/LockOnComponent.h"

void UPlayerAnimInstance::NativeUpdateAnimation( float DeltaSeconds )
{
	Super::NativeUpdateAnimation( DeltaSeconds );

	if ( ATidePlayerCharacter* PlayerChar = Cast<ATidePlayerCharacter>( TryGetPawnOwner() ) )
	{
		if ( ULockOnComponent* LockOnComp = PlayerChar->FindComponentByClass<ULockOnComponent>() )
		{
			// チャージダッシュのLoopは専用BS（bIsChargeDashBSPlaying）で表現するため、その間は
			// ロックオン移動BSに奪われないよう無効化する（チャージダッシュは IsDashing() に含まれない）
			bIsLockOnMove = LockOnComp->HasTarget() && !PlayerChar->IsDashing() && !bIsChargeDashBSPlaying;
		}
	}
}
