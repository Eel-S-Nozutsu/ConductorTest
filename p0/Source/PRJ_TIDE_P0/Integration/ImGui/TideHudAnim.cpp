// Copyright (c) 2026, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.
#include "TideHudAnim.h"

#if !UE_BUILD_SHIPPING

#include "Kismet/GameplayStatics.h"

#include "PRJ_TIDE_P0/Core/TideGameSettings.h"
#include "PRJ_TIDE_P0/Core/TidePlayerController.h"

namespace TideHudAnim
{
	bool IsHudSuppressed( const UWorld* World )
	{
		const UTideGameSettings* Settings = UTideGameSettings::Get();
		if ( Settings && Settings->bDebugHideUI ) return true;

		if ( !World ) return false;

		// カットシーン中は HUD を out アニメで引っ込める。判定は敵 AI の停止（UEnemyBrainComponent::
		// IsCinematicSuspended）と同じ bCinematicMode を見るので、AI 停止と HUD 消えが必ず一致する
		const ATidePlayerController* PC = Cast<ATidePlayerController>( UGameplayStatics::GetPlayerController( World, 0 ) );
		return PC && PC->IsInCinematicMode();
	}
}

#endif // !UE_BUILD_SHIPPING
