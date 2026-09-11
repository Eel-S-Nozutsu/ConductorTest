// Copyright (c) 2026, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.

#include "SplineWavingFloorGimmick.h"

#include "Components/SceneComponent.h"
#include "Components/SplineComponent.h"
#include "Components/SplineMeshComponent.h"

ASplineWavingFloorGimmick::ASplineWavingFloorGimmick()
{
	PrimaryActorTick.bCanEverTick = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	PathSpline = CreateDefaultSubobject<USplineComponent>(TEXT("PathSpline"));
	PathSpline->SetupAttachment(SceneRoot);
	PathSpline->SetClosedLoop(false);
}

void ASplineWavingFloorGimmick::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	RegenerateFloor();
}

void ASplineWavingFloorGimmick::BeginPlay()
{
	Super::BeginPlay();

	ElapsedTime = 0.0f;

	// PIE用に複製されたインスタンスでは動的生成コンポーネント(FloorSegments)や
	// BaseLocalPositions/BaseDistancesが引き継がれない場合があるため、
	// 実行開始時に必ずRegenerateFloor()でスプラインの現在の形から作り直す
	RegenerateFloor();

	// TODO: 動作確認用の一時ログ。原因特定できたら削除する
	UE_LOG(LogTemp, Warning, TEXT("[SplineWavingFloor] BeginPlay: BasePoints=%d, Segments=%d, WaveHeight=%.1f, WaveFrequency=%.2f, WaveSpeed=%.1f"),
		BaseLocalPositions.Num(), FloorSegments.Num(), WaveHeight, WaveFrequency, WaveSpeed);
}

#if WITH_EDITOR
void ASplineWavingFloorGimmick::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// ビューポートでのスプライン点ドラッグはPostEditMove経由のOnConstructionで拾えるが、
	// NumDivisions/メッシュ等をDetailsパネルで変更した場合はそちらが呼ばれないためここで再生成する
	RegenerateFloor();
}
#endif

void ASplineWavingFloorGimmick::ClearFloorSegments()
{
	// FloorSegments配列の追跡だけに頼らず、PathSpline配下のSplineMeshComponentを
	// アタッチ階層から直接洗い出して破棄する。PIE用の複製ではFloorSegments配列の
	// 中身が引き継がれず、古いコンポーネントが孤立して残ることがあるため
	if (PathSpline)
	{
		TArray<TObjectPtr<USceneComponent>> AttachedChildren = PathSpline->GetAttachChildren();
		for (const TObjectPtr<USceneComponent>& Child : AttachedChildren)
		{
			if (USplineMeshComponent* Segment = Cast<USplineMeshComponent>(Child.Get()))
			{
				Segment->DestroyComponent();
			}
		}
	}

	FloorSegments.Reset();
}

void ASplineWavingFloorGimmick::RegenerateFloor()
{
	if (!PathSpline) return;

	const int32 NumPoints = FMath::Max(NumDivisions, 1) + 1;
	const float TotalLength = PathSpline->GetSplineLength();

	// 現在のスプライン形状(カーブ込み)を弧長で等間隔サンプリングし、均等な点群を作り直す
	TArray<FVector> NewPositions;
	NewPositions.Reserve(NumPoints);

	for (int32 i = 0; i < NumPoints; ++i)
	{
		const float Distance = (TotalLength / (NumPoints - 1)) * i;
		NewPositions.Add(PathSpline->GetLocationAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::Local));
	}

	PathSpline->SetSplinePoints(NewPositions, ESplineCoordinateSpace::Local, false);
	for (int32 i = 0; i < NewPositions.Num(); ++i)
	{
		PathSpline->SetSplinePointType(i, ESplinePointType::Curve, false);
	}
	PathSpline->UpdateSpline();

	CaptureBaseShape();

	// 区間メッシュを張り直す
	ClearFloorSegments();

	if (!FloorMesh)
	{
		// TODO: 動作確認用の一時ログ。原因特定できたら削除する
		UE_LOG(LogTemp, Warning, TEXT("[SplineWavingFloor] RegenerateFloor: FloorMesh未設定のためセグメント生成をスキップ"));
		return;
	}

	const int32 NumSegments = NumPoints - 1;
	FloorSegments.Reserve(NumSegments);

	for (int32 i = 0; i < NumSegments; ++i)
	{
		// NAME_Noneでユニーク名を自動割り当てする。固定名だと直前にDestroyComponentした
		// 同名オブジェクトがGC未実行でまだ残っている場合に名前衝突でクラッシュするため
		USplineMeshComponent* Segment = NewObject<USplineMeshComponent>(this, NAME_None);
		Segment->SetMobility(EComponentMobility::Movable);
		Segment->SetupAttachment(PathSpline);
		Segment->SetStaticMesh(FloorMesh);
		if (FloorMaterialOverride)
		{
			Segment->SetMaterial(0, FloorMaterialOverride);
		}
		Segment->SetForwardAxis(ESplineMeshAxis::X, false);
		// デフォルト任せにせず、床として乗れるように明示的にコリジョンを有効化する
		Segment->SetCollisionProfileName(TEXT("BlockAll"));
		Segment->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Segment->RegisterComponentWithWorld(GetWorld());

		FloorSegments.Add(Segment);

		UpdateSegmentFromSpline(i);
	}

	// TODO: 動作確認用の一時ログ。原因特定できたら削除する
	UE_LOG(LogTemp, Warning, TEXT("[SplineWavingFloor] RegenerateFloor: NumDivisions=%d, NumPoints=%d, FloorMesh=%s, SegmentsCreated=%d"),
		NumDivisions, NumPoints, FloorMesh ? *FloorMesh->GetName() : TEXT("null"), FloorSegments.Num());
}

void ASplineWavingFloorGimmick::CaptureBaseShape()
{
	BaseLocalPositions.Reset();
	BaseDistances.Reset();

	if (!PathSpline) return;

	const int32 NumPoints = PathSpline->GetNumberOfSplinePoints();
	BaseLocalPositions.Reserve(NumPoints);
	BaseDistances.Reserve(NumPoints);

	for (int32 i = 0; i < NumPoints; ++i)
	{
		BaseLocalPositions.Add(PathSpline->GetLocationAtSplinePoint(i, ESplineCoordinateSpace::Local));
		BaseDistances.Add(PathSpline->GetDistanceAlongSplineAtSplinePoint(i));
	}
}

void ASplineWavingFloorGimmick::UpdateSegmentFromSpline(int32 SegmentIndex)
{
	if (!FloorSegments.IsValidIndex(SegmentIndex)) return;

	USplineMeshComponent* Segment = FloorSegments[SegmentIndex];
	if (!Segment) return;

	const FVector StartPos = PathSpline->GetLocationAtSplinePoint(SegmentIndex, ESplineCoordinateSpace::Local);
	const FVector StartTangent = PathSpline->GetTangentAtSplinePoint(SegmentIndex, ESplineCoordinateSpace::Local);
	const FVector EndPos = PathSpline->GetLocationAtSplinePoint(SegmentIndex + 1, ESplineCoordinateSpace::Local);
	const FVector EndTangent = PathSpline->GetTangentAtSplinePoint(SegmentIndex + 1, ESplineCoordinateSpace::Local);
	Segment->SetStartAndEnd(StartPos, StartTangent, EndPos, EndTangent, true);
}

void ASplineWavingFloorGimmick::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	const int32 NumPoints = BaseLocalPositions.Num();
	if (!PathSpline || FloorSegments.Num() == 0 || NumPoints != FloorSegments.Num() + 1)
	{
		// TODO: 動作確認用の一時ログ。原因特定できたら削除する
		UE_LOG(LogTemp, Warning, TEXT("[SplineWavingFloor] Tick early-return: PathSpline=%s, Segments=%d, BasePoints=%d"),
			PathSpline ? TEXT("valid") : TEXT("null"), FloorSegments.Num(), NumPoints);
		return;
	}

	const float PrevElapsedTime = ElapsedTime;
	ElapsedTime += DeltaTime;

	const float SafeFrequency = FMath::Max(WaveFrequency, 0.0f);

	// 波数 = 2π/波長。波長は 速度 = 頻度 × 波長 の関係から逆算する(頻度か速度が0なら空間的な波打ちなしとみなす)
	const float WaveNumber = (SafeFrequency > KINDA_SMALL_NUMBER && !FMath::IsNearlyZero(WaveSpeed))
		? (2.0f * PI * SafeFrequency) / WaveSpeed
		: 0.0f;

	for (int32 i = 0; i < NumPoints; ++i)
	{
		const float Phase = 2.0f * PI * SafeFrequency * ElapsedTime - WaveNumber * BaseDistances[i];

		FVector Animated = BaseLocalPositions[i];
		Animated.Z += WaveHeight * FMath::Sin(Phase);

		PathSpline->SetLocationAtSplinePoint(i, Animated, ESplineCoordinateSpace::Local, false);
	}
	PathSpline->UpdateSpline();

	for (int32 i = 0; i < FloorSegments.Num(); ++i)
	{
		UpdateSegmentFromSpline(i);
	}

	// TODO: 動作確認用の一時ログ(約1秒に1回)。原因特定できたら削除する
	if (FMath::FloorToInt(PrevElapsedTime) != FMath::FloorToInt(ElapsedTime))
	{
		UE_LOG(LogTemp, Warning, TEXT("[SplineWavingFloor] Tick: t=%.2f WaveNumber=%.4f BaseZ0=%.1f AnimatedZ0=%.1f"),
			ElapsedTime, WaveNumber, BaseLocalPositions[0].Z,
			PathSpline->GetLocationAtSplinePoint(0, ESplineCoordinateSpace::Local).Z);
	}
}
