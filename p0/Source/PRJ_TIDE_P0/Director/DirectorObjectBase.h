// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "DirectorObjectBase.generated.h"

/**
 * ディレクター系UObjectの共通基底。
 *
 * Outerは ContentDirector -> MapDirector -> Subsystem(Outer=UWorld) と繋がるため、
 * 実行時のGetWorldはUObjectの既定(Outer鎖)で解決できる。ここで明示的にoverrideするのは
 * BPエディタ側(ImplementsGetWorld)にワールドコンテキストがあると知らせるためで、
 * これが無いとBP派生でDelayやタイマーのノードが置けない。
 */
UCLASS(Abstract)
class PRJ_TIDE_P0_API UDirectorObjectBase : public UObject
{
	GENERATED_BODY()

public:

	virtual UWorld* GetWorld() const override;

#if WITH_EDITOR
	virtual bool ImplementsGetWorld() const override { return true; }
#endif

};
