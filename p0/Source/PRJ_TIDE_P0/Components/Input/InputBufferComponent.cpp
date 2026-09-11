// Copyright (c) 2026, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.
#include "InputBufferComponent.h"

UInputBufferComponent::UInputBufferComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UInputBufferComponent::TickComponent( float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction )
{
	Super::TickComponent( DeltaTime, TickType, ThisTickFunction );

	if ( const UWorld* World = GetWorld() )
	{
		const float CurrentTime = World->GetTimeSeconds();

		// 古い入力や、消費済みの入力を配列から一括削除
		CommandBuffer.RemoveAll( [&]( const FInputBufferData& Data ) {
			return Data.bIsConsumed || ( CurrentTime - Data.Timestamp > MaxBufferLifespan );
			} );
	}
}

void UInputBufferComponent::PushCommand( FGameplayTag CommandTag )
{
	if ( !CommandTag.IsValid() ) return;

	FInputBufferData NewData;
	NewData.CommandTag = CommandTag;
	NewData.bIsConsumed = false;

	if ( const UWorld* World = GetWorld() )
	{
		NewData.Timestamp = World->GetTimeSeconds();
	}

	CommandBuffer.Add( NewData );
}

bool UInputBufferComponent::ConsumeCommand( FGameplayTag CommandTag, float ValidDuration )
{
	if ( !CommandTag.IsValid() ) return false;

	if ( const UWorld* World = GetWorld() )
	{
		const float CurrentTime = World->GetTimeSeconds();
		for ( int32 i = CommandBuffer.Num() - 1; i >= 0; --i )
		{
			FInputBufferData& Data = CommandBuffer[i];
			if ( !Data.bIsConsumed )
			{
				if ( Data.CommandTag.MatchesTag( CommandTag ) )
				{
					if ( CurrentTime - Data.Timestamp <= ValidDuration )
					{
						Data.bIsConsumed = true;
						return true;
					}
				}
			}
		}
	}
	return false;
}

void UInputBufferComponent::ClearCommand( FGameplayTag CommandTag )
{
	for ( FInputBufferData& Data : CommandBuffer )
	{
		if ( Data.CommandTag == CommandTag )
		{
			Data.bIsConsumed = true;
		}
	}
}

bool UInputBufferComponent::HasCommand( FGameplayTag CommandTag, float ValidDuration ) const
{
	if ( !CommandTag.IsValid() ) return false;
	if ( const UWorld* World = GetWorld() )
	{
		const float CurrentTime = World->GetTimeSeconds();
		for ( const FInputBufferData& Data : CommandBuffer )
		{
			if ( !Data.bIsConsumed )
			{
				if ( Data.CommandTag.MatchesTag( CommandTag ) )
				{
					if ( CurrentTime - Data.Timestamp <= ValidDuration )
					{
						return true;
					}
				}
			}
		}
	}
	return false;
}
