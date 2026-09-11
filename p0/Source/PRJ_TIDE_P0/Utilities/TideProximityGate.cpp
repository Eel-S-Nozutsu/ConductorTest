// Copyright (c) 2026, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.

#include "PRJ_TIDE_P0/Utilities/TideProximityGate.h"

#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#if !UE_BUILD_SHIPPING
#include "Engine/World.h"
#include "DrawDebugHelpers.h"
#endif

namespace TideProximityGate
{
	APawn* ResolvePlayerPawn( const UObject* WorldContext, TWeakObjectPtr<APawn>& CachedPlayer )
	{
		if ( APawn* Cached = CachedPlayer.Get() )
		{
			return Cached;
		}

		APawn* Player = UGameplayStatics::GetPlayerPawn( WorldContext, 0 );
		CachedPlayer = Player;
		return Player;
	}

	bool IsPlayerWithinDistance( const UObject* WorldContext, const FVector& Center,
		float ActivationDistance, TWeakObjectPtr<APawn>& CachedPlayer, bool bActivateWhenNoPlayer )
	{
		// 0 以下はゲート無効＝常に作動させる（設計者が距離カリングを切りたいとき）
		if ( ActivationDistance <= 0.0f )
		{
			return true;
		}

		const APawn* Player = ResolvePlayerPawn( WorldContext, CachedPlayer );
		if ( !Player )
		{
			return bActivateWhenNoPlayer;
		}

		const float DistSq = FVector::DistSquared( Center, Player->GetActorLocation() );
		return DistSq <= ( ActivationDistance * ActivationDistance );
	}

#if !UE_BUILD_SHIPPING
	void DrawActivationRange( const UObject* WorldContext, const FVector& Center,
		float ActivationDistance, bool bWithinRange )
	{
		if ( ActivationDistance <= 0.0f ) return;

		UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
		if ( !World ) return;

		// 作動中は緑・スキップ中は赤。ワイヤーフレーム球で作動範囲そのものを描く
		const FColor Color = bWithinRange ? FColor::Green : FColor::Red;
		DrawDebugSphere( World, Center, ActivationDistance, 24, Color, false, -1.0f, 0, 1.5f );
	}
#endif
}
