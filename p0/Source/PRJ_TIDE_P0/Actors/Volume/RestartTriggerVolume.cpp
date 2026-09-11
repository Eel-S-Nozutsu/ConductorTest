// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#include "PRJ_TIDE_P0/Actors/Volume/RestartTriggerVolume.h"
#include "PRJ_TIDE_P0/Actors/Character/Player/TidePlayerCharacter.h"
#include "PRJ_TIDE_P0/Components/Player/RestartComponent.h"
#include "PRJ_TIDE_P0/Data/Progression/RestartPointRow.h"

void ARestartTriggerVolume::NotifyActorBeginOverlap(AActor* OtherActor)
{
	Super::NotifyActorBeginOverlap(OtherActor);

	ATidePlayerCharacter* Player = Cast<ATidePlayerCharacter>(OtherActor);
	if (!Player) return;

	URestartComponent* Restart = Player->FindComponentByClass<URestartComponent>();
	if (!Restart) return;

	static const FString Context = TEXT("RestartTriggerVolumeOverlap");
	const FRestartPointRow* Row = RestartPoint.GetRow<FRestartPointRow>(Context);
	if (!Row) return;

	// 進行順が前進する場合のみリスタート地点を更新 ※Uターンで巻き戻さない
	Restart->SetRestartPoint(Row->Location, Row->Rotation, Row->ProgressOrder);
}
