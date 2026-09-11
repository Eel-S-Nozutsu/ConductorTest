// Copyright (c) 2026, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "LockOnTargetComponent.generated.h"

UCLASS( ClassGroup = ( LockOn ), meta = ( BlueprintSpawnableComponent ) )
class ULockOnTargetComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	ULockOnTargetComponent()
	{
		PrimaryComponentTick.bCanEverTick = false;
	}

	// この部位が現在ロックオン可能かどうか
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "Tide|LockOn" )
	bool bIsTargetable = true;

	// 部位ごとのロックオン優先度
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "Tide|LockOn" )
	float TargetPriorityScore = 1.0f;

	// メッシュの骨（ソケット）に追従させる場合はその名前を指定
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "Tide|LockOn" )
	FName TargetSocketName = NAME_None;

	// 実際のターゲット座標を取得する関数
	FVector GetTargetLocation() const
	{
		if ( TargetSocketName != NAME_None )
		{
			return GetSocketLocation( TargetSocketName );
		}
		return GetComponentLocation();
	}
};
