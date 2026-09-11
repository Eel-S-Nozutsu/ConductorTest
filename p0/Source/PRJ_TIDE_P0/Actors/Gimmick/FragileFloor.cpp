// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#include "PRJ_TIDE_P0/Actors/Gimmick/FragileFloor.h"
#include "PRJ_TIDE_P0/Actors/Gimmick/FragileFloorChunk.h"
#include "PRJ_TIDE_P0/Actors/Character/Player/TidePlayerCharacter.h"
#include "PRJ_TIDE_P0/Actors/Projectile/EnemyProjectile.h"

#include "ProceduralMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"
#include "Math/RandomStream.h"
#include "DrawDebugHelpers.h"

namespace
{
	// PLが床の上に居るとみなす高さ(床アクタのローカルZ)
	constexpr float StimulusZCeiling = 300.0f;

	// 凸ポリゴンを半平面 { P : dot(P-M, N) >= 0 } でクリップ
	void ClipConvexByHalfPlane(TArray<FVector2D>& Poly, const FVector2D& M, const FVector2D& N)
	{
		const int32 Count = Poly.Num();
		if (Count < 3) return;

		TArray<FVector2D> Out;
		Out.Reserve(Count + 2);
		for (int32 i = 0; i < Count; ++i)
		{
			const FVector2D& A = Poly[i];
			const FVector2D& B = Poly[(i + 1) % Count];
			const float dA = FVector2D::DotProduct(A - M, N);
			const float dB = FVector2D::DotProduct(B - M, N);

			if (dA >= 0.0f) Out.Add(A);
			if ((dA < 0.0f) != (dB < 0.0f))
			{
				const float T = dA / (dA - dB);
				Out.Add(A + (B - A) * T);
			}
		}
		Poly = MoveTemp(Out);
	}

	FVector2D PolygonCentroid(const TArray<FVector2D>& Poly)
	{
		FVector2D C = FVector2D::ZeroVector;
		for (const FVector2D& V : Poly) C += V;
		return Poly.Num() > 0 ? C / Poly.Num() : C;
	}

	// 重心へ寄せて縮める(ひびのすき間用)
	void InsetPolygon(TArray<FVector2D>& Poly, float Dist)
	{
		if (Dist <= 0.0f) return;
		const FVector2D C = PolygonCentroid(Poly);
		for (FVector2D& V : Poly)
		{
			FVector2D Dir = V - C;
			const float Len = Dir.Size();
			V = (Len > Dist) ? (V - Dir / Len * Dist) : C;
		}
	}

	void AddQuad(TArray<FVector>& Verts, TArray<int32>& Tris, TArray<FVector>& Normals, TArray<FVector2D>& UVs,
		const FVector& P0, const FVector& P1, const FVector& P2, const FVector& P3)
	{
		const int32 B = Verts.Num();
		// cross(P1-P0,P3-P0) は裏(下・内向き)になるので反転する。誤ると表面が暗く陰る
		const FVector Nrm = FVector::CrossProduct(P3 - P0, P1 - P0).GetSafeNormal();
		Verts.Add(P0); Verts.Add(P1); Verts.Add(P2); Verts.Add(P3);
		for (int32 t = 0; t < 4; ++t) Normals.Add(Nrm);
		UVs.Add(FVector2D(0, 0)); UVs.Add(FVector2D(1, 0)); UVs.Add(FVector2D(1, 1)); UVs.Add(FVector2D(0, 1));
		Tris.Add(B + 0); Tris.Add(B + 1); Tris.Add(B + 2);
		Tris.Add(B + 0); Tris.Add(B + 2); Tris.Add(B + 3);
	}
}

AFragileFloor::AFragileFloor()
{
	PrimaryActorTick.bCanEverTick = true;

	// ドラッグ移動中の毎フレーム再構築を止める(巨大なプレビューPMCの
	// 退避・再登録が毎tick走ってエディタが固まるため。確定時のみ再構築)
#if WITH_EDITORONLY_DATA
	bRunConstructionScriptOnDrag = false;
#endif

	// エディタプレビュー描画専用。実行時の描画・コリジョンはタイルPMC群が持つ
	FloorMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("FloorMesh"));
	FloorMesh->SetMobility(EComponentMobility::Movable);
	FloorMesh->SetCollisionProfileName(TEXT("NoCollision"));
	// 多数セクションの影でVSMが溢れるため
	FloorMesh->SetCastShadow(false);
	RootComponent = FloorMesh;

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MatFinder(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (MatFinder.Succeeded()) DefaultMaterial = MatFinder.Object;
}

void AFragileFloor::BeginPlay()
{
	Super::BeginPlay();

	GenerateVoronoi();
	// エディタから複製されてきたプレビューを破棄(実行時はタイルPMC側で描画・コリジョン)
	FloorMesh->ClearAllMeshSections();
	InitRuntimeTiles();
}

void AFragileFloor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// ストリームアウト・破棄のたびにタイルとデリゲート束縛を確実に片づける
	// GC任せだとOnComponentHitのスパースデリゲート束縛が残留し、再入時に
	// 二重束縛のensureと弾トリガの不発(床が壊れない)を招く
	TeardownRuntimeTiles();

	Super::EndPlay(EndPlayReason);
}

void AFragileFloor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	// ゲーム中はBeginPlayに任せる(二重生成回避)
	const UWorld* W = GetWorld();
	if (!W || W->IsGameWorld()) return;

	// 既定は外形の箱だけ(配置調整優先)。フルのひび表示は巨大メッシュを抱え、
	// D&Dのトランザクション退避・保存が重くなるためオプトイン
	// スピンボックスのドラッグ中も確定までは箱に落とす
	bool bBoxOnly = !bPreviewCracksInEditor;
#if WITH_EDITOR
	bBoxOnly |= bInteractiveEdit;
#endif
	if (bBoxOnly)
	{
		LastPreviewSignature.Reset();
		BuildPreviewBoxMesh();
		return;
	}

	// 生成パラメータが変わったときだけ作り直す。移動では走らせない(毎ドラッグ再生成で固まるため)
	const FString Sig = FString::Printf(TEXT("%.0f_%.0f_%.0f_%.2f_%d_%.0f_%.0f_%.0f"),
		FloorExtent.X, FloorExtent.Y, CellSize, SiteJitter, RandomSeed, CellGap, CrackDepth, Thickness);
	if (Sig == LastPreviewSignature) return;
	LastPreviewSignature = Sig;

	// プレビューは当たりを焼かない
	GenerateVoronoi();
	BuildPreviewMesh();
}

#if WITH_EDITOR
void AFragileFloor::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	// ドラッグ編集中かをOnConstructionへ伝える(Superが構築スクリプトを再実行する)
	bInteractiveEdit = PropertyChangedEvent.ChangeType == EPropertyChangeType::Interactive;
	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif

void AFragileFloor::GenerateVoronoi()
{
	Cells.Reset();

	// セル数が上限を超えると起動時のコリジョンのクックでメモリが破綻するので、
	// CellSizeを粗くして抑える
	float EffCellSize = CellSize;
	const float MinCellForCap = FMath::Sqrt(FloorExtent.X * FloorExtent.Y / FMath::Max(1, MaxCellCount));
	if (EffCellSize < MinCellForCap)
	{
		UE_LOG(LogTemp, Warning, TEXT("FragileFloor: セル数が上限(%d)を超えるため CellSize を %.0f→%.0f に引き上げ"), MaxCellCount, CellSize, MinCellForCap);
		EffCellSize = MinCellForCap;
	}

	const int32 GX = FMath::Max(1, FMath::FloorToInt(FloorExtent.X / EffCellSize));
	const int32 GY = FMath::Max(1, FMath::FloorToInt(FloorExtent.Y / EffCellSize));
	HashDimX = GX;
	HashDimY = GY;
	const float SpX = FloorExtent.X / GX;
	const float SpY = FloorExtent.Y / GY;
	const float HalfX = FloorExtent.X * 0.5f;
	const float HalfY = FloorExtent.Y * 0.5f;

	FRandomStream Rng(RandomSeed);
	Cells.Reserve(GX * GY);
	for (int32 iy = 0; iy < GY; ++iy)
	{
		for (int32 ix = 0; ix < GX; ++ix)
		{
			FFragileCell Cell;
			const float BaseX = -HalfX + (ix + 0.5f) * SpX;
			const float BaseY = -HalfY + (iy + 0.5f) * SpY;
			const float JX = Rng.FRandRange(-1.0f, 1.0f) * SiteJitter * SpX * 0.5f;
			const float JY = Rng.FRandRange(-1.0f, 1.0f) * SiteJitter * SpY * 0.5f;
			Cell.Site = FVector2D(
				FMath::Clamp(BaseX + JX, -HalfX + 1.0f, HalfX - 1.0f),
				FMath::Clamp(BaseY + JY, -HalfY + 1.0f, HalfY - 1.0f));
			Cells.Add(Cell);
		}
	}

	BuildSiteHash();

	const float BorderEps = 1.0f;
	for (int32 i = 0; i < Cells.Num(); ++i)
	{
		// 矩形を全サイトの垂直二等分線でクリップ＝そのサイトのVoronoiセル
		TArray<FVector2D> Poly;
		Poly.Add(FVector2D(-HalfX, -HalfY));
		Poly.Add(FVector2D(HalfX, -HalfY));
		Poly.Add(FVector2D(HalfX, HalfY));
		Poly.Add(FVector2D(-HalfX, HalfY));

		// 近傍サイトの二等分線だけでクリップ。Voronoi隣接は必ず近傍にいるので、
		// 全サイト走査(O(N^2))は不要
		const FVector2D Si = Cells[i].Site;
		const int32 bx = FMath::Clamp(FMath::FloorToInt((Si.X + HalfX) / (FloorExtent.X / HashDimX)), 0, HashDimX - 1);
		const int32 by = FMath::Clamp(FMath::FloorToInt((Si.Y + HalfY) / (FloorExtent.Y / HashDimY)), 0, HashDimY - 1);
		for (int32 dy = -2; dy <= 2; ++dy)
		{
			for (int32 dx = -2; dx <= 2; ++dx)
			{
				const int32 cx = bx + dx;
				const int32 cy = by + dy;
				if (cx < 0 || cx >= HashDimX || cy < 0 || cy >= HashDimY) continue;
				for (int32 j : SiteBuckets[cy * HashDimX + cx])
				{
					if (j == i) continue;
					const FVector2D Sj = Cells[j].Site;
					const FVector2D M = (Si + Sj) * 0.5f;
					const FVector2D Nrm = (Si - Sj).GetSafeNormal();
					if (!Nrm.IsNearlyZero())
					{
						ClipConvexByHalfPlane(Poly, M, Nrm);
					}
				}
			}
		}

		// 隣接・外周は実エッジ(インセット前)で判定する
		const int32 PN = Poly.Num();
		for (int32 k = 0; k < PN; ++k)
		{
			const FVector2D& A = Poly[k];
			const FVector2D& B = Poly[(k + 1) % PN];
			const FVector2D Em = (A + B) * 0.5f;

			const bool bBorder =
				FMath::IsNearlyEqual(Em.X, -HalfX, BorderEps) || FMath::IsNearlyEqual(Em.X, HalfX, BorderEps) ||
				FMath::IsNearlyEqual(Em.Y, -HalfY, BorderEps) || FMath::IsNearlyEqual(Em.Y, HalfY, BorderEps);
			if (bBorder)
			{
				Cells[i].bBoundary = true;
				continue;
			}
			const int32 Nb = NearestSite(Em, i);
			if (Nb != INDEX_NONE)
			{
				Cells[i].Neighbors.AddUnique(Nb);
			}
		}

		Cells[i].Polygon = MoveTemp(Poly);
	}

	// 片側からしか張られなかった隣接を補う
	for (int32 i = 0; i < Cells.Num(); ++i)
	{
		for (int32 Nb : Cells[i].Neighbors)
		{
			if (Cells.IsValidIndex(Nb)) Cells[Nb].Neighbors.AddUnique(i);
		}
	}

	WeldCellVertices();
}

void AFragileFloor::WeldCellVertices()
{
	const float BucketSize = 8.0f;
	const float WeldEpsSq = 2.0f * 2.0f;

	TArray<FVector2D> Canon;
	TMap<int64, TArray<int32>> Grid;
	auto Key = [](int32 gx, int32 gy) -> int64 { return (int64)gx * 1000003 + gy; };

	auto FindOrAdd = [&](const FVector2D& P) -> FVector2D
	{
		const int32 gx = FMath::FloorToInt(P.X / BucketSize);
		const int32 gy = FMath::FloorToInt(P.Y / BucketSize);
		for (int32 dx = -1; dx <= 1; ++dx)
		{
			for (int32 dy = -1; dy <= 1; ++dy)
			{
				if (const TArray<int32>* Arr = Grid.Find(Key(gx + dx, gy + dy)))
				{
					for (int32 Idx : *Arr)
					{
						if (FVector2D::DistSquared(Canon[Idx], P) <= WeldEpsSq)
						{
							return Canon[Idx];
						}
					}
				}
			}
		}
		const int32 NewIdx = Canon.Add(P);
		Grid.FindOrAdd(Key(gx, gy)).Add(NewIdx);
		return P;
	};

	for (FFragileCell& Cell : Cells)
	{
		for (FVector2D& V : Cell.Polygon)
		{
			V = FindOrAdd(V);
		}
	}
}

void AFragileFloor::BuildSiteHash()
{
	// HashDimX/YはGenerateVoronoiがグリッド寸法(実効セルサイズ基準)で設定済み
	SiteBuckets.Empty();
	SiteBuckets.SetNum(HashDimX * HashDimY);

	const float HalfX = FloorExtent.X * 0.5f;
	const float HalfY = FloorExtent.Y * 0.5f;
	for (int32 i = 0; i < Cells.Num(); ++i)
	{
		const int32 bx = FMath::Clamp(FMath::FloorToInt((Cells[i].Site.X + HalfX) / (FloorExtent.X / HashDimX)), 0, HashDimX - 1);
		const int32 by = FMath::Clamp(FMath::FloorToInt((Cells[i].Site.Y + HalfY) / (FloorExtent.Y / HashDimY)), 0, HashDimY - 1);
		SiteBuckets[by * HashDimX + bx].Add(i);
	}
}

int32 AFragileFloor::NearestSite(const FVector2D& LocalXY, int32 Exclude) const
{
	if (Cells.Num() == 0) return INDEX_NONE;

	const float HalfX = FloorExtent.X * 0.5f;
	const float HalfY = FloorExtent.Y * 0.5f;
	const int32 bx = FMath::Clamp(FMath::FloorToInt((LocalXY.X + HalfX) / (FloorExtent.X / HashDimX)), 0, HashDimX - 1);
	const int32 by = FMath::Clamp(FMath::FloorToInt((LocalXY.Y + HalfY) / (FloorExtent.Y / HashDimY)), 0, HashDimY - 1);

	int32 Best = INDEX_NONE;
	float BestD2 = TNumericLimits<float>::Max();
	for (int32 dy = -2; dy <= 2; ++dy)
	{
		for (int32 dx = -2; dx <= 2; ++dx)
		{
			const int32 cx = bx + dx;
			const int32 cy = by + dy;
			if (cx < 0 || cx >= HashDimX || cy < 0 || cy >= HashDimY) continue;
			for (int32 Idx : SiteBuckets[cy * HashDimX + cx])
			{
				if (Idx == Exclude) continue;
				const float D2 = FVector2D::DistSquared(Cells[Idx].Site, LocalXY);
				if (D2 < BestD2) { BestD2 = D2; Best = Idx; }
			}
		}
	}

	// 近傍バケットに無ければ全走査で保険
	if (Best == INDEX_NONE)
	{
		for (int32 i = 0; i < Cells.Num(); ++i)
		{
			if (i == Exclude) continue;
			const float D2 = FVector2D::DistSquared(Cells[i].Site, LocalXY);
			if (D2 < BestD2) { BestD2 = D2; Best = i; }
		}
	}
	return Best;
}

FLinearColor AFragileFloor::MakeCellMask(const FFragileCell& Cell) const
{
	// マテリアルが点滅を作るときに全セルが同位相で光らないよう、セル位置から固定の位相を配る
	const float Phase = FMath::Frac(FMath::Sin(Cell.Site.X * 12.9898f + Cell.Site.Y * 78.233f) * 43758.5453f);
	return FLinearColor(Cell.WarnBlend, Phase, 0.0f, 1.0f);
}

void AFragileFloor::AppendCellVisual(const FFragileCell& Cell, TArray<FVector>& Verts, TArray<int32>& Tris, TArray<FVector>& Normals, TArray<FVector2D>& UVs, TArray<FLinearColor>& Colors) const
{
	const float Depth = FMath::Min(CrackDepth, Thickness);
	const TArray<FVector2D>& Full = Cell.Polygon;
	const int32 N = Full.Num();

	// 内側へ寄せた上面(立つ面)。隣との間はベベル溝で埋める
	TArray<FVector2D> In = Full;
	InsetPolygon(In, CellGap * 0.5f);
	if (In.Num() != N) In = Full;

	auto V3 = [](const FVector2D& P, float Z) { return FVector(P.X, P.Y, Z); };

	// UEはCWが表なので、上向きに表を向けるためk+1→kで張る。誤ると上からバックフェースで消える
	const int32 TopBase = Verts.Num();
	for (int32 k = 0; k < N; ++k)
	{
		Verts.Add(V3(In[k], 0.0f));
		Normals.Add(FVector::UpVector);
		UVs.Add(In[k] / 100.0f);
	}
	for (int32 k = 1; k < N - 1; ++k)
	{
		Tris.Add(TopBase + 0); Tris.Add(TopBase + k + 1); Tris.Add(TopBase + k);
	}

	// ひびの溝
	for (int32 k = 0; k < N; ++k)
	{
		const int32 k2 = (k + 1) % N;
		AddQuad(Verts, Tris, Normals, UVs, V3(In[k], 0.0f), V3(In[k2], 0.0f), V3(Full[k2], -Depth), V3(Full[k], -Depth));
	}

	// 側面。崩落時は隣が露出して穴の断面を塞ぐ
	for (int32 k = 0; k < N; ++k)
	{
		const int32 k2 = (k + 1) % N;
		AddQuad(Verts, Tris, Normals, UVs, V3(Full[k], -Depth), V3(Full[k2], -Depth), V3(Full[k2], -Thickness), V3(Full[k], -Thickness));
	}

	// 底面
	const int32 BotBase = Verts.Num();
	for (int32 k = 0; k < N; ++k)
	{
		Verts.Add(V3(Full[k], -Thickness));
		Normals.Add(-FVector::UpVector);
		UVs.Add(Full[k] / 100.0f);
	}
	for (int32 k = 1; k < N - 1; ++k)
	{
		Tris.Add(BotBase + 0); Tris.Add(BotBase + k); Tris.Add(BotBase + k + 1);
	}

	// このセルが積んだ頂点すべてへ同じマスクを配る(AddQuad側に手を入れずに済む)
	const FLinearColor Mask = MakeCellMask(Cell);
	for (int32 i = TopBase; i < Verts.Num(); ++i)
	{
		Colors.Add(Mask);
	}
}

void AFragileFloor::AppendCellCollision(const FFragileCell& Cell, TArray<FVector>& Verts, TArray<int32>& Tris) const
{
	// 歩行・着弾は上面だけで受かる。溝・側面・底面までクックすると崩落のたびのコストが数倍になる
	// 溝をまたがないFullポリゴンなので、カプセルが溝に引っかかることもない
	const TArray<FVector2D>& Full = Cell.Polygon;
	const int32 N = Full.Num();

	const int32 Base = Verts.Num();
	for (int32 k = 0; k < N; ++k)
	{
		Verts.Add(FVector(Full[k].X, Full[k].Y, 0.0f));
	}
	// コリジョンもCW=表。裏向きだと片面判定で着地がすり抜ける
	for (int32 k = 1; k < N - 1; ++k)
	{
		Tris.Add(Base + 0); Tris.Add(Base + k + 1); Tris.Add(Base + k);
	}
}

void AFragileFloor::BuildPreviewMesh()
{
	TArray<FVector> Verts;
	TArray<int32> Tris;
	TArray<FVector> Normals;
	TArray<FVector2D> UVs;
	TArray<FLinearColor> Colors;
	Verts.Reserve(Cells.Num() * 60);
	Tris.Reserve(Cells.Num() * 120);
	Normals.Reserve(Cells.Num() * 60);
	UVs.Reserve(Cells.Num() * 60);
	Colors.Reserve(Cells.Num() * 60);

	for (const FFragileCell& Cell : Cells)
	{
		if (Cell.State == EFragileCellState::Collapsed) continue;
		if (Cell.Polygon.Num() < 3) continue;
		AppendCellVisual(Cell, Verts, Tris, Normals, UVs, Colors);
	}

	FloorMesh->ClearAllMeshSections();
	if (Verts.Num() >= 3)
	{
		FloorMesh->CreateMeshSection_LinearColor(0, Verts, Tris, Normals, UVs, Colors, TArray<FProcMeshTangent>(), false);
		UMaterialInterface* Mat = FloorMaterial ? FloorMaterial : DefaultMaterial;
		if (Mat) FloorMesh->SetMaterial(0, Mat);
	}
}

void AFragileFloor::BuildPreviewBoxMesh()
{
	const float HalfX = FloorExtent.X * 0.5f;
	const float HalfY = FloorExtent.Y * 0.5f;

	// 外形だけの1セル扱いで箱を作る(AppendCellVisualを流用)
	FFragileCell Box;
	Box.Polygon = { FVector2D(-HalfX, -HalfY), FVector2D(HalfX, -HalfY), FVector2D(HalfX, HalfY), FVector2D(-HalfX, HalfY) };

	TArray<FVector> Verts;
	TArray<int32> Tris;
	TArray<FVector> Normals;
	TArray<FVector2D> UVs;
	TArray<FLinearColor> Colors;
	AppendCellVisual(Box, Verts, Tris, Normals, UVs, Colors);

	FloorMesh->ClearAllMeshSections();
	FloorMesh->CreateMeshSection_LinearColor(0, Verts, Tris, Normals, UVs, Colors, TArray<FProcMeshTangent>(), false);
	UMaterialInterface* Mat = FloorMaterial ? FloorMaterial : DefaultMaterial;
	if (Mat) FloorMesh->SetMaterial(0, Mat);
}

void AFragileFloor::InitRuntimeTiles()
{
	// ストリーミングの再入などで再初期化される場合に備え、まず既存タイルを
	// 束縛解除＋破棄してから作り直す(残留束縛・孤立コンポーネントの防止)
	TeardownRuntimeTiles();

	// 崩落のたびに床全体のコリジョンを再クックするとFPSが落ちるため、
	// タイル別PMCに分けて作り直しを崩落タイルだけに限定する
	TileDimX = FMath::Max(1, FMath::CeilToInt(FloorExtent.X / TileSize));
	TileDimY = FMath::Max(1, FMath::CeilToInt(FloorExtent.Y / TileSize));

	TileCellIndices.Reset();
	TileCellIndices.SetNum(TileDimX * TileDimY);

	const float HalfX = FloorExtent.X * 0.5f;
	const float HalfY = FloorExtent.Y * 0.5f;
	for (int32 i = 0; i < Cells.Num(); ++i)
	{
		const int32 tx = FMath::Clamp(FMath::FloorToInt((Cells[i].Site.X + HalfX) / FloorExtent.X * TileDimX), 0, TileDimX - 1);
		const int32 ty = FMath::Clamp(FMath::FloorToInt((Cells[i].Site.Y + HalfY) / FloorExtent.Y * TileDimY), 0, TileDimY - 1);
		Cells[i].TileIndex = ty * TileDimX + tx;
		TileCellIndices[Cells[i].TileIndex].Add(i);
	}

	TileMeshes.Reset();
	TileMeshes.SetNum(TileDimX * TileDimY);
	for (int32 t = 0; t < TileMeshes.Num(); ++t)
	{
		if (TileCellIndices[t].Num() == 0) continue;

		UProceduralMeshComponent* Tile = NewObject<UProceduralMeshComponent>(this, NAME_None, RF_Transient);
		Tile->SetupAttachment(RootComponent);
		Tile->SetMobility(EComponentMobility::Movable);
		Tile->SetCollisionProfileName(TEXT("BlockAll"));
		Tile->bUseComplexAsSimpleCollision = true;
		// 同期クック。非同期だと作り直し中コリジョンが一瞬消え、
		// 上のキャラが落下→着地を繰り返す(着地モーション連発)ため
		Tile->bUseAsyncCooking = false;
		// 多数セクションの影でVSMが溢れるため
		Tile->SetCastShadow(false);
		// 敵弾の着弾を床側で受ける
		Tile->SetNotifyRigidBodyCollision(true);
		Tile->OnComponentHit.AddDynamic(this, &AFragileFloor::OnFloorHit);
		Tile->RegisterComponent();
		TileMeshes[t] = Tile;

		RebuildTile(t);
	}
}

void AFragileFloor::TeardownRuntimeTiles()
{
	// スパースデリゲート束縛はGC任せにせずここで確実に外す。残すと同一
	// UFUNCTIONの二重束縛ensure(ScriptDelegates.h)や着弾トリガ不発を招く
	for (TObjectPtr<UProceduralMeshComponent>& Tile : TileMeshes)
	{
		if (Tile)
		{
			Tile->OnComponentHit.RemoveDynamic(this, &AFragileFloor::OnFloorHit);
			Tile->DestroyComponent();
		}
	}

	TileMeshes.Reset();
	TileCellIndices.Reset();
	DirtyTiles.Reset();
}

void AFragileFloor::RebuildTile(int32 TileIdx)
{
	if (!TileMeshes.IsValidIndex(TileIdx) || !TileMeshes[TileIdx]) return;
	UProceduralMeshComponent* Tile = TileMeshes[TileIdx];

	TArray<FVector> Verts;
	TArray<int32> Tris;
	TArray<FVector> Normals;
	TArray<FVector2D> UVs;
	TArray<FLinearColor> Colors;
	TArray<FVector> ColVerts;
	TArray<int32> ColTris;

	for (int32 CellIdx : TileCellIndices[TileIdx])
	{
		const FFragileCell& Cell = Cells[CellIdx];
		if (Cell.State == EFragileCellState::Collapsed) continue;	// 崩落セルは抜いて穴に
		if (Cell.Polygon.Num() < 3) continue;
		AppendCellVisual(Cell, Verts, Tris, Normals, UVs, Colors);
		AppendCellCollision(Cell, ColVerts, ColTris);
	}

	Tile->ClearAllMeshSections();
	if (Verts.Num() >= 3)
	{
		// 見た目は描画専用、コリジョンは上面だけの不可視セクションに分けてクック量を抑える
		Tile->CreateMeshSection_LinearColor(0, Verts, Tris, Normals, UVs, Colors, TArray<FProcMeshTangent>(), false);
		Tile->CreateMeshSection_LinearColor(1, ColVerts, ColTris, TArray<FVector>(), TArray<FVector2D>(), TArray<FLinearColor>(), TArray<FProcMeshTangent>(), true);
		Tile->SetMeshSectionVisible(1, false);
		UMaterialInterface* Mat = FloorMaterial ? FloorMaterial : DefaultMaterial;
		if (Mat) Tile->SetMaterial(0, Mat);
	}
}

void AFragileFloor::UpdateTileColors(int32 TileIdx)
{
	if (!TileMeshes.IsValidIndex(TileIdx) || !TileMeshes[TileIdx]) return;
	UProceduralMeshComponent* Tile = TileMeshes[TileIdx];

	TArray<FVector> Verts;
	TArray<int32> Tris;
	TArray<FVector> Normals;
	TArray<FVector2D> UVs;
	TArray<FLinearColor> Colors;

	for (int32 CellIdx : TileCellIndices[TileIdx])
	{
		const FFragileCell& Cell = Cells[CellIdx];
		if (Cell.State == EFragileCellState::Collapsed) continue;
		if (Cell.Polygon.Num() < 3) continue;
		AppendCellVisual(Cell, Verts, Tris, Normals, UVs, Colors);
	}

	// 見た目セクションだけをUpdateで差し替える。トポロジもコリジョン(セクション1)も
	// 変わらないので再クックが走らない。法線・UVは空で渡せば更新をスキップされる
	// (Updateは頂点数が一致する配列だけ反映する)
	Tile->UpdateMeshSection_LinearColor(0, Verts, TArray<FVector>(), TArray<FVector2D>(),
		Colors, TArray<FProcMeshTangent>(), false);
}

void AFragileFloor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// PLの刺激(仕様: 移動や攻撃で刺激が加わる)
	if (bMovementStimulus || bAttackStimulus)
	{
		if (!CachedPlayer.IsValid())
		{
			CachedPlayer = Cast<ATidePlayerCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0));
		}
		if (CachedPlayer.IsValid())
		{
			const FVector PlayerLoc = CachedPlayer->GetActorLocation();
			const FVector Local = GetActorTransform().InverseTransformPosition(PlayerLoc);
			const bool bOnFloor = Local.Z > -Thickness && Local.Z < StimulusZCeiling;
			if (bOnFloor)
			{
				// 接地を要求する。水平速度だけ見ると走りジャンプの滞空中も刺激が入り続ける
				const UCharacterMovementComponent* Move = CachedPlayer->GetCharacterMovement();
				const bool bMoving = Move && Move->IsMovingOnGround()
					&& CachedPlayer->GetVelocity().Size2D() > MovementSpeedThreshold;
				const bool bAttacking = CachedPlayer->IsAttacking();
				if ((bMovementStimulus && bMoving) || (bAttackStimulus && bAttacking))
				{
					TriggerCellsInRadius(PlayerLoc, StimulusRadius);
				}
			}
		}
	}

	for (int32 i = 0; i < Cells.Num(); ++i)
	{
		if (Cells[i].State == EFragileCellState::Triggered)
		{
			// 警告色へフェード。飽和後は触らない(毎フレームのセクション更新を止める)
			if (Cells[i].WarnBlend < 1.0f)
			{
				Cells[i].WarnBlend = WarnFadeSeconds > 0.0f
					? FMath::Min(1.0f, Cells[i].WarnBlend + DeltaTime / WarnFadeSeconds)
					: 1.0f;
				ColorDirtyTiles.Add(Cells[i].TileIndex);
			}

			Cells[i].CollapseTimer -= DeltaTime;
			if (Cells[i].CollapseTimer <= 0.0f)
			{
				CollapseCell(i);
			}
		}
	}

	if (bEnclosureDirty)
	{
		RunEnclosureCheck();
		bEnclosureDirty = false;
	}

	// このTickで崩落があったタイルだけ、立ちセルで作り直す(1回にまとめる)
	for (int32 TileIdx : DirtyTiles)
	{
		RebuildTile(TileIdx);
	}

	// 色だけ変わったタイルは見た目セクションのみ更新する(コリジョン再クックを避ける)
	for (int32 TileIdx : ColorDirtyTiles)
	{
		// 作り直したタイルは最新の色で出来ているので重ねて更新しない
		if (DirtyTiles.Contains(TileIdx)) continue;
		UpdateTileColors(TileIdx);
	}

	DirtyTiles.Reset();
	ColorDirtyTiles.Reset();

	if (bDrawDebug)
	{
		DrawDebugCells();
	}
}

void AFragileFloor::NotifyStimulusAtLocation(const FVector& WorldLocation)
{
	const int32 Index = LocationToCell(WorldLocation);
	if (Index != INDEX_NONE)
	{
		TriggerCell(Index, CollapseDelay);
	}
}

void AFragileFloor::ResetFloor()
{
	int32 RestoredCount = 0;
	for (FFragileCell& Cell : Cells)
	{
		if (Cell.State == EFragileCellState::Cracked) continue;

		Cell.State = EFragileCellState::Cracked;
		Cell.CollapseTimer = 0.0f;
		Cell.WarnBlend = 0.0f;
		++RestoredCount;
	}

	// 崩落も予兆も無ければ何もしない ※初回入室で全タイルを無駄に再クックしてヒッチさせない
	if (RestoredCount == 0) return;

	// 復活直後に穴が開く場合の切り分け用(復活後の着弾なのか、復活漏れなのか)
	UE_LOG(LogTemp, Log, TEXT("FragileFloor(%s): %d/%d セルを復活"), *GetName(), RestoredCount, Cells.Num());

	// 落下中・寿命前の塊を片づける ※復活した床の下に残骸が浮いて見えるのを防ぐ
	for (TActorIterator<AFragileFloorChunk> It(GetWorld()); It; ++It)
	{
		if (It->GetOwner() == this)
		{
			It->Destroy();
		}
	}

	// 立ちセルが戻ったので全タイルを作り直す。GenerateVoronoiはやり直さない
	// (セル形状・隣接・タイル割りは不変。ひび模様が入室ごとに変わらないようにもなる)
	DirtyTiles.Reset();
	ColorDirtyTiles.Reset();
	for (int32 t = 0; t < TileMeshes.Num(); ++t)
	{
		RebuildTile(t);
	}
	bEnclosureDirty = false;
}

int32 AFragileFloor::LocationToCell(const FVector& WorldLocation) const
{
	const FVector Local = GetActorTransform().InverseTransformPosition(WorldLocation);
	if (FMath::Abs(Local.X) > FloorExtent.X * 0.5f || FMath::Abs(Local.Y) > FloorExtent.Y * 0.5f)
	{
		return INDEX_NONE;
	}
	return NearestSite(FVector2D(Local.X, Local.Y));
}

void AFragileFloor::TriggerCell(int32 Index, float Delay)
{
	if (!Cells.IsValidIndex(Index)) return;
	if (Cells[Index].State != EFragileCellState::Cracked) return;

	Cells[Index].State = EFragileCellState::Triggered;
	Cells[Index].CollapseTimer = Delay;
}

void AFragileFloor::TriggerCellsInRadius(const FVector& WorldLocation, float Radius)
{
	const FVector Local = GetActorTransform().InverseTransformPosition(WorldLocation);
	if (FMath::Abs(Local.X) > FloorExtent.X * 0.5f || FMath::Abs(Local.Y) > FloorExtent.Y * 0.5f) return;

	const FVector2D P(Local.X, Local.Y);

	TriggerCell(NearestSite(P), CollapseDelay);

	if (Radius <= 0.0f) return;

	const float R2 = Radius * Radius;
	for (int32 i = 0; i < Cells.Num(); ++i)
	{
		if (FVector2D::DistSquared(Cells[i].Site, P) <= R2)
		{
			TriggerCell(i, CollapseDelay);
		}
	}
}

void AFragileFloor::OnFloorHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	// 敵弾の着弾だけ反応する(PL・落下片・その他の当たりは無視)
	if (Cast<AEnemyProjectile>(OtherActor))
	{
		TriggerCellsInRadius(Hit.ImpactPoint, StimulusRadius);
	}
}

void AFragileFloor::CollapseCell(int32 Index)
{
	if (!Cells.IsValidIndex(Index)) return;
	FFragileCell& Cell = Cells[Index];
	if (Cell.State == EFragileCellState::Collapsed) return;

	Cell.State = EFragileCellState::Collapsed;

	// 実際の作り直しはフレーム末に崩落タイルだけまとめて(TickのDirtyTiles)
	DirtyTiles.Add(Cell.TileIndex);

	if (Cell.Polygon.Num() >= 3)
	{
		FBox2D Bounds(ForceInit);
		for (const FVector2D& V : Cell.Polygon) Bounds += V;
		const FVector2D Ref = Bounds.GetCenter();

		TArray<FVector2D> LocalPoly;
		LocalPoly.Reserve(Cell.Polygon.Num());
		for (const FVector2D& V : Cell.Polygon) LocalPoly.Add(V - Ref);

		// 崩落セルと同じ位置・向きでスポーン
		const FVector SpawnLoc = GetActorTransform().TransformPosition(FVector(Ref.X, Ref.Y, 0.0f));

		// 支えが消えても自動では落ちないスリープ中の剛体を、このセルの真上で起こす
		WakeBodiesOverCell(SpawnLoc, Bounds.GetExtent());

		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Params.Owner = this;
		if (AFragileFloorChunk* Chunk = GetWorld()->SpawnActor<AFragileFloorChunk>(AFragileFloorChunk::StaticClass(), SpawnLoc, GetActorRotation(), Params))
		{
			const FVector Impulse(
				FMath::FRandRange(-100.0f, 100.0f),
				FMath::FRandRange(-100.0f, 100.0f),
				-FMath::Abs(ChunkDownImpulse));
			Chunk->Launch(LocalPoly, Thickness, FloorMaterial ? FloorMaterial : DefaultMaterial, ChunkLifeSpan, Impulse);
		}
	}

	bEnclosureDirty = true;
}

void AFragileFloor::WakeBodiesOverCell(const FVector& WorldCenter, const FVector2D& HalfExtentXY)
{
	UWorld* World = GetWorld();
	if (!World) return;

	// セルのフットプリント × WakeBodiesHeightの箱で真上を探る。床の回転に追従させる
	const float HalfHeight = FMath::Max(1.0f, WakeBodiesHeight * 0.5f);
	const FVector BoxExtent(HalfExtentXY.X, HalfExtentXY.Y, HalfHeight);
	const FVector QueryCenter = WorldCenter + GetActorRotation().RotateVector(FVector(0.0f, 0.0f, HalfHeight));

	FCollisionObjectQueryParams ObjQuery;
	ObjQuery.AddObjectTypesToQuery(ECC_WorldDynamic);
	ObjQuery.AddObjectTypesToQuery(ECC_PhysicsBody);

	FCollisionQueryParams Params(SCENE_QUERY_STAT(FragileFloorWake), false, this);
	Params.AddIgnoredActor(this);

	TArray<FOverlapResult> Overlaps;
	World->OverlapMultiByObjectType(Overlaps, QueryCenter, GetActorQuat(),
		ObjQuery, FCollisionShape::MakeBox(BoxExtent), Params);

	if (bDrawDebug)
	{
		DrawDebugBox(World, QueryCenter, BoxExtent, GetActorQuat(), FColor::Cyan, false, 2.0f, 0, 2.0f);
	}

	for (const FOverlapResult& Ov : Overlaps)
	{
		UPrimitiveComponent* Comp = Ov.GetComponent();
		if (Comp && Comp->IsSimulatingPhysics())
		{
			Comp->WakeAllRigidBodies();
		}
	}
}

void AFragileFloor::RunEnclosureCheck()
{
	const int32 N = Cells.Num();
	if (N == 0) return;

	// 外周の立ちセルから隣接を辿って「本土と繋がる＝安全」を塗る
	TArray<bool> Visited;
	Visited.Init(false, N);

	TArray<int32> Stack;
	for (int32 i = 0; i < N; ++i)
	{
		if (Cells[i].bBoundary && Cells[i].State != EFragileCellState::Collapsed && !Visited[i])
		{
			Visited[i] = true;
			Stack.Push(i);
		}
	}

	for (int32 Head = 0; Head < Stack.Num(); ++Head)
	{
		const int32 Cur = Stack[Head];
		for (int32 Nb : Cells[Cur].Neighbors)
		{
			if (Cells.IsValidIndex(Nb) && !Visited[Nb] && Cells[Nb].State != EFragileCellState::Collapsed)
			{
				Visited[Nb] = true;
				Stack.Push(Nb);
			}
		}
	}

	// 塗られなかった立ちセル＝孤立(囲われた)→ 時間差で崩落
	int32 StaggerK = 0;
	for (int32 i = 0; i < N; ++i)
	{
		if (!Visited[i] && Cells[i].State == EFragileCellState::Cracked)
		{
			TriggerCell(i, EnclosureStagger * StaggerK);
			++StaggerK;
		}
	}
}

void AFragileFloor::DrawDebugCells() const
{
	const UWorld* World = GetWorld();
	if (!World) return;

	const FTransform& Xf = GetActorTransform();
	for (const FFragileCell& Cell : Cells)
	{
		FColor Color = FColor::Green;
		switch (Cell.State)
		{
		case EFragileCellState::Cracked:   Color = FColor::Green;  break;
		case EFragileCellState::Triggered: Color = FColor::Yellow; break;
		case EFragileCellState::Collapsed: Color = FColor::Red;    break;
		}

		const int32 N = Cell.Polygon.Num();
		for (int32 k = 0; k < N; ++k)
		{
			const FVector2D& A = Cell.Polygon[k];
			const FVector2D& B = Cell.Polygon[(k + 1) % N];
			const FVector WA = Xf.TransformPosition(FVector(A.X, A.Y, 2.0f));
			const FVector WB = Xf.TransformPosition(FVector(B.X, B.Y, 2.0f));
			DrawDebugLine(World, WA, WB, Color, false, -1.0f, 0, 2.0f);
		}
	}
}
