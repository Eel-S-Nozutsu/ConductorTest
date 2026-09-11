// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#include "PRJ_TIDE_P0/Actors/Gimmick/WaterSurfaceGimmick.h"

#include "Components/StaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "PRJ_TIDE_P0/Actors/Character/Player/TidePlayerCharacter.h"

AWaterSurfaceGimmick::AWaterSurfaceGimmick()
{
	PrimaryActorTick.bCanEverTick = true;

	WaterMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WaterMesh"));
	WaterMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RootComponent = WaterMesh;
}

void AWaterSurfaceGimmick::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!CachedPlayer.IsValid())
	{
		CachedPlayer = Cast<ATidePlayerCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0));
		if (!CachedPlayer.IsValid()) return;
	}

	const bool bChargeAction = CachedPlayer->IsPlayingChargeAction();
	const bool bCharging = CachedPlayer->IsCharging();
	// チャージ幅跳び(チャージジャンプ)中は、落下ループ
	// (LP)に入るとIsPlayingChargeAction()が意図的にfalseを返すため、
	// そのままだと着水前にコリジョンが切れて水面をすり抜けてしまう
	// ジャンプ種別を直接見て、上昇～落下～着地まで水面コリジョンを維持する
	const bool bChargeJump = CachedPlayer->IsPlayingChargeJump();
	// 水面の法線方向(板の上向き)で「面より上か」を判定する。Z比較だと水面を傾けたとき
	// XY位置ごとに面の高さが変わり、判定しない箇所が出るため、法線への符号付き距離で見る
	const FVector SurfaceUp = GetActorUpVector();
	const FVector ToPlayer = CachedPlayer->GetActorLocation() - GetActorLocation();
	const bool bAboveSurface = FVector::DotProduct(ToPlayer, SurfaceUp) >= 0.0f;
	const bool bMoving = CachedPlayer->GetVelocity().Size2D() > 50.0f;
	const bool bShouldEnable = bAboveSurface && (bChargeAction || bChargeJump || (bCharging && bMoving));
	const ECollisionEnabled::Type Target = bShouldEnable
		? ECollisionEnabled::QueryAndPhysics
		: ECollisionEnabled::NoCollision;

	if (WaterMesh->GetCollisionEnabled() != Target)
	{
		WaterMesh->SetCollisionEnabled(Target);
	}
}
