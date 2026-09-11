// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#include "PRJ_TIDE_P0/Actors/Gimmick/FragileFloorChunk.h"
#include "ProceduralMeshComponent.h"

AFragileFloorChunk::AFragileFloorChunk()
{
	PrimaryActorTick.bCanEverTick = false;

	MeshComp = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("MeshComp"));
	MeshComp->SetMobility(EComponentMobility::Movable);
	MeshComp->bUseComplexAsSimpleCollision = false;
	// 凸クックを非同期化して一斉崩落のゲームスレッドスパイクを避ける
	MeshComp->bUseAsyncCooking = true;
	MeshComp->SetCollisionProfileName(TEXT("PhysicsActor"));
	// カメラだけは押さない
	MeshComp->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	MeshComp->SetCastShadow(false);
	RootComponent = MeshComp;

	// ExCameraの壁スウィープから除外
	Tags.Add(TEXT("CameraSweepIgnore"));
}

void AFragileFloorChunk::Launch(const TArray<FVector2D>& LocalPolygon, float Thickness, UMaterialInterface* Material, float Life, const FVector& Impulse)
{
	const int32 N = LocalPolygon.Num();
	if (N < 3) { Destroy(); return; }

	TArray<FVector> Verts;
	TArray<int32> Tris;
	TArray<FVector> Normals;
	TArray<FVector2D> UVs;

	auto V3 = [](const FVector2D& P, float Z) { return FVector(P.X, P.Y, Z); };

	// UEはCWが表なので、上向きにk+1→kで張る
	const int32 TopBase = Verts.Num();
	for (int32 k = 0; k < N; ++k)
	{
		Verts.Add(V3(LocalPolygon[k], 0.0f));
		Normals.Add(FVector::UpVector);
		UVs.Add(LocalPolygon[k] / 100.0f);
	}
	for (int32 k = 1; k < N - 1; ++k)
	{
		Tris.Add(TopBase + 0); Tris.Add(TopBase + k + 1); Tris.Add(TopBase + k);
	}

	const int32 BotBase = Verts.Num();
	for (int32 k = 0; k < N; ++k)
	{
		Verts.Add(V3(LocalPolygon[k], -Thickness));
		Normals.Add(-FVector::UpVector);
		UVs.Add(LocalPolygon[k] / 100.0f);
	}
	for (int32 k = 1; k < N - 1; ++k)
	{
		Tris.Add(BotBase + 0); Tris.Add(BotBase + k); Tris.Add(BotBase + k + 1);
	}

	for (int32 k = 0; k < N; ++k)
	{
		const int32 k2 = (k + 1) % N;
		const FVector2D A = LocalPolygon[k];
		const FVector2D B = LocalPolygon[k2];
		const FVector2D D = B - A;
		const FVector Nrm = FVector(D.Y, -D.X, 0.0f).GetSafeNormal();

		const int32 Base = Verts.Num();
		Verts.Add(V3(A, 0.0f));
		Verts.Add(V3(B, 0.0f));
		Verts.Add(V3(B, -Thickness));
		Verts.Add(V3(A, -Thickness));
		for (int32 t = 0; t < 4; ++t) Normals.Add(Nrm);
		UVs.Add(FVector2D(0, 0)); UVs.Add(FVector2D(1, 0)); UVs.Add(FVector2D(1, 1)); UVs.Add(FVector2D(0, 1));

		Tris.Add(Base + 0); Tris.Add(Base + 1); Tris.Add(Base + 2);
		Tris.Add(Base + 0); Tris.Add(Base + 2); Tris.Add(Base + 3);
	}

	MeshComp->CreateMeshSection_LinearColor(0, Verts, Tris, Normals, UVs, TArray<FLinearColor>(), TArray<FProcMeshTangent>(), /*bCreateCollision=*/false);
	if (Material) MeshComp->SetMaterial(0, Material);

	// 当たりだけ少し内側に。穴と完全同寸だと落下時に縁を擦って挟まる。見た目メッシュは実寸のまま
	const float CollisionInset = 2.0f;
	FVector2D Center = FVector2D::ZeroVector;
	for (const FVector2D& V : LocalPolygon) Center += V;
	Center /= N;

	TArray<FVector> Convex;
	Convex.Reserve(N * 2);
	for (int32 k = 0; k < N; ++k)
	{
		FVector2D Dir = LocalPolygon[k] - Center;
		const float Len = Dir.Size();
		const FVector2D Q = (Len > CollisionInset) ? (LocalPolygon[k] - Dir / Len * CollisionInset) : Center;
		Convex.Add(V3(Q, 0.0f));
		Convex.Add(V3(Q, -Thickness));
	}
	MeshComp->AddCollisionConvexMesh(Convex);

	MeshComp->SetSimulatePhysics(true);
	if (!Impulse.IsNearlyZero())
	{
		MeshComp->AddImpulse(Impulse, NAME_None, true);
	}
	MeshComp->SetPhysicsAngularVelocityInDegrees(FMath::VRand() * FMath::FRandRange(20.0f, 90.0f));

	SetLifeSpan(Life);
}
