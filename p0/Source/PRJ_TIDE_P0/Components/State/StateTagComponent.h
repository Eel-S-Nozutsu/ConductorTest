// Copyright (c) 2026, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"

#include "StateTagComponent.generated.h"



UCLASS( ClassGroup = ( Custom ), meta = ( BlueprintSpawnableComponent ) )
class UStateTagComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	void AddStateTag( const FGameplayTag& Tag );
	void RemoveStateTag( const FGameplayTag& Tag );
	bool HasStateTag( const FGameplayTag& Tag ) const;
	void ForceRemoveStateTagsByParent( const FGameplayTag& ParentTag );

	const FGameplayTagContainer& GetActiveStateTags() const { return ActiveStateTags; }

private:
	UPROPERTY( VisibleAnywhere, BlueprintReadOnly, Category = "Tide|State", meta = ( AllowPrivateAccess = "true", Categories = "State" ) )
	FGameplayTagContainer ActiveStateTags;

	// タグごとの付与数を記録するマップ（内部管理用）
	TMap<FGameplayTag, int32> TagCountMap;
};
