// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#include "PRJ_TIDE_P0/Animation/Notifies/Camera/AnimNotify_CameraShake.h"

#include "Camera/CameraShakeBase.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "DrawDebugHelpers.h"

void UAnimNotify_CameraShake::Notify(USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (!MeshComp || !CameraShake) return;

	UWorld* World = MeshComp->GetWorld();
	if (!World) return;

	// 発生源: ソケット指定があればその位置、無ければメッシュ所有アクターの位置
	FVector OriginLoc;
	if (!OriginSocket.IsNone() && MeshComp->DoesSocketExist(OriginSocket))
	{
		OriginLoc = MeshComp->GetSocketLocation(OriginSocket);
	}
	else if (const AActor* Owner = MeshComp->GetOwner())
	{
		OriginLoc = Owner->GetActorLocation();
	}
	else
	{
		OriginLoc = MeshComp->GetComponentLocation();
	}

	const float Near = FMath::Min(ShakeNearDistance, ShakeFarDistance); // ここより近いと最大
	const float Far  = FMath::Max(ShakeNearDistance, ShakeFarDistance); // ここより遠いと0

	// 発生源とPCの距離でシェイク強度を決定する (近=最大 /
	// 遠=0のSmoothStepフォールオフ)
	if (APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0))
	{
		if (APlayerCameraManager* CameraManager = PC->PlayerCameraManager)
		{
			const APawn* Pawn = PC->GetPawn();
			const FVector PcLocation = Pawn ? Pawn->GetActorLocation() : CameraManager->GetCameraLocation();
			const float Dist = FVector::Distance(PcLocation, OriginLoc);

			const float Range = FMath::Max(Far - Near, KINDA_SMALL_NUMBER);
			const float t = FMath::Clamp((Far - Dist) / Range, 0.0f, 1.0f);
			const float ShakeScaleRate = FMath::SmoothStep(0.0f, 1.0f, t);

			if (ShakeScaleRate > KINDA_SMALL_NUMBER)
			{
				CameraManager->StartCameraShake(CameraShake, ShakeScaleRate);
			}
		}
	}

#if !UE_BUILD_SHIPPING
	// シェイク範囲の近距離 (赤) / 遠距離 (黄) リングを発生源へ水平描画する
	if (bDebugDrawShakeArea)
	{
		const float Duration = 2.0f;
		// 水平リング (XY平面) を張る軸
		const FVector AxisY(1.0f, 0.0f, 0.0f);
		const FVector AxisZ(0.0f, 1.0f, 0.0f);
		DrawDebugCircle(World, OriginLoc, Near, 48, FColor::Red,    false, Duration, 0, 6.0f, AxisY, AxisZ, false);
		DrawDebugCircle(World, OriginLoc, Far,  48, FColor::Yellow, false, Duration, 0, 6.0f, AxisY, AxisZ, false);
		DrawDebugSphere(World, OriginLoc, 60.0f, 12, FColor::Cyan, false, Duration);
	}
#endif
}
