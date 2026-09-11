// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DirectorIdComponent.generated.h"

/**
 * ディレクターから名前で指せるようにするための札。
 *
 * DTはアセットなのでレベル内アクターを直接参照できない(保存でNoneに戻る)。
 * 代わりにDT側はActorIdだけを持ち、ContentDirectorがこの札を探して実体に繋ぐ。
 */
UCLASS(ClassGroup = (Tide), meta = (BlueprintSpawnableComponent))
class PRJ_TIDE_P0_API UDirectorIdComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	// DT_DirectorActorsのActorIdと一致させる
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tide|Director")
	FName ActorId;

};
