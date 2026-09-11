// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FragileFloorChunk.generated.h"

class UProceduralMeshComponent;
class UMaterialInterface;

/**
 * 崩落したセルが落ちる塊。凸コリジョンのクックは非同期にして一斉崩落でも重くしない。
 */
UCLASS()
class PRJ_TIDE_P0_API AFragileFloorChunk : public AActor
{
	GENERATED_BODY()

public:

	AFragileFloorChunk();

	void Launch(const TArray<FVector2D>& LocalPolygon, float Thickness, UMaterialInterface* Material, float Life, const FVector& Impulse);

	UPROPERTY(VisibleAnywhere, Category = "Tide|Gimmick")
	TObjectPtr<UProceduralMeshComponent> MeshComp;

};
