// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "ConductorObjectBase.generated.h"

/**
 * コンダクタ系UObjectの共通基底
 */
UCLASS(Abstract)
class CONDUCTORTEST_API UConductorObjectBase : public UObject
{
	GENERATED_BODY()

public:
	virtual UWorld* GetWorld() const override;

#if WITH_EDITOR
	virtual bool ImplementsGetWorld() const override { return true; }
#endif
};
