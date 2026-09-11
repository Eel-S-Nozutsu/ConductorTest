// Copyright (c) 2026, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

#include "TidePlayerModule.generated.h"

class ATidePlayerCharacter;

// プレイヤーモジュール基底
UCLASS( Abstract )
class UTidePlayerModule : public UObject
{
	GENERATED_BODY()

public:

	virtual void Initialize( ATidePlayerCharacter* InOwner )
	{
		OwnerCharacter = InOwner;
	}

	virtual void OnModuleUpdate( float DeltaTime )
	{
	}

	virtual void OnAttackHit( AActor* TargetActor, bool bIsRebounded ) {}

protected:
	bool TryConsumeCommand( FGameplayTag CommandTag, float BufferTime );
	bool HasCommand( FGameplayTag CommandTag, float BufferTime ) const;
	UAnimMontage* GetAnimMontage( const FName& MontageName ) const;
	float PlayAnimMontage( const FName& MontageName, float InPlayRate = 1.0f, FName StartSectionName = NAME_None );

protected:
	UPROPERTY()
	TObjectPtr<ATidePlayerCharacter> OwnerCharacter;
};
