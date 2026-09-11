// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "TideGameInstance.generated.h"

/**
 * プロジェクト共通GameInstance
 */
UCLASS()
class PRJ_TIDE_P0_API UTideGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:

	virtual void Init() override;
	virtual void Shutdown() override;

	void SetPendingJump(FVector Location, float Yaw);

private:

	struct FPendingJumpInfo
	{
		FVector Location;
		float   Yaw;

	};

	TOptional<FPendingJumpInfo> PendingJump;

	void ApplyPendingJump(UWorld* World);

};
