// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#include "PRJ_TIDE_P0/Actors/Gimmick/DecayedSteppingStone.h"

#include "Components/StaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "NiagaraFunctionLibrary.h"
#include "Engine/World.h"
#if WITH_EDITOR
#include "DrawDebugHelpers.h"
#endif

ADecayedSteppingStone::ADecayedSteppingStone()
{
	// StoneMeshは基底コンストラクタで生成済み(Root)。ツタと浮上先ハンドルを子として足す
	VineMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("VineMesh"));
	VineMesh->SetupAttachment(StoneMesh);
	VineMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	RiseTarget = CreateDefaultSubobject<USceneComponent>(TEXT("RiseTarget"));
	RiseTarget->SetupAttachment(StoneMesh);
}

void ADecayedSteppingStone::BeginPlay()
{
	Super::BeginPlay();

	State = ESteppingStoneState::Entangled;

	// 竜巻のスイープ
	// (AllDynamicObjects)に拾われる必要があるのでNoCollisionにはできない
	// ただしオブジェクトタイプ照会はObjectTypeしか見ず応答チャンネルは無視するため、
	// PawnだけBlockにしても囲いトリガは届く＝拘束中〜浮上中も乗って一緒に上がれる
	StoneMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	StoneMesh->SetCollisionObjectType(ECC_WorldDynamic);
	StoneMesh->SetCollisionResponseToAllChannels(ECR_Overlap);
	StoneMesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);

	// ツタのディゾルブMIDを作り無傷で初期化 ※1=無傷 → 0=消滅
	if (VineMesh)
	{
		for (int32 i = 0; i < VineMesh->GetNumMaterials(); ++i)
		{
			if (UMaterialInstanceDynamic* MID = VineMesh->CreateAndSetMaterialInstanceDynamic(i))
			{
				MID->SetScalarParameterValue(DissolveParamName, 1.0f);
				VineMIDs.Add(MID);
			}
		}
	}
}

void ADecayedSteppingStone::Tick(float DeltaTime)
{
#if WITH_EDITOR
	// エディタ配置プレビューでは浮上先を可視化する
	const UWorld* World = GetWorld();
	if (World && !World->IsGameWorld())
	{
		if (RiseTarget)
		{
			const FVector To = RiseTarget->GetComponentLocation();
			DrawDebugLine(World, GetActorLocation(), To, FColor::Green, false, -1.0f, 0, 3.0f);
			DrawDebugSphere(World, To, 40.0f, 12, FColor::Green, false, -1.0f, 0, 2.0f);
		}
		return;	// プレビューではゲーム挙動を進めない
	}
#endif
	Super::Tick(DeltaTime);
}

void ADecayedSteppingStone::OnWindEnter(const FWindInfluence& Wind)
{
	// 拘束中に囲われた最初の1回だけツタ破壊へ。浮上先は固定なので、以後は風が消えても続行する
	if (bFreed) return;
	if (State != ESteppingStoneState::Entangled) return;

	bFreed = true;
	EnterFreeing();
}

void ADecayedSteppingStone::TickPreSettled(float DeltaTime)
{
	if (State == ESteppingStoneState::Freeing)
	{
		TickFreeing(DeltaTime);
	}
	else if (State == ESteppingStoneState::Emerging)
	{
		TickEmerging(DeltaTime);
	}
	// Entangledは囲いトリガ待ちで何もしない
}

void ADecayedSteppingStone::EnterFreeing()
{
	State = ESteppingStoneState::Freeing;
	FreeElapsed = 0.0f;
}

void ADecayedSteppingStone::TickFreeing(float DeltaTime)
{
	FreeElapsed += DeltaTime;

	// ディゾルブ進捗をFreeDuration秒で0→1にし、パラメータは1→0で流す
	const float DissolveAlpha = (FreeDuration > 0.0f) ? FMath::Clamp(FreeElapsed / FreeDuration, 0.0f, 1.0f) : 1.0f;
	for (UMaterialInstanceDynamic* MID : VineMIDs)
	{
		if (MID) MID->SetScalarParameterValue(DissolveParamName, 1.0f - DissolveAlpha);
	}

	// 消え切った瞬間に一度だけVFX＋ツタ非表示
	if (!bVineBroken && DissolveAlpha >= 1.0f)
	{
		bVineBroken = true;
		if (BreakVFX)
		{
			UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, BreakVFX, GetActorLocation());
		}
		if (VineMesh) VineMesh->SetVisibility(false);
	}

	// ツタが消えてからFreeHoldDuration待ってから浮上開始
	if (FreeElapsed >= FreeDuration + FreeHoldDuration)
	{
		// 石を動かす前に浮上の始点・終点をワールド座標で確定する ※以後RiseTargetは再参照しない
		EmergeStartLocation  = GetActorLocation();
		EmergeTargetLocation = RiseTarget ? RiseTarget->GetComponentLocation() : EmergeStartLocation;
		EmergeElapsed = 0.0f;
		OrbitAngleDeg = 0.0f;
		State = ESteppingStoneState::Emerging;
	}
}

void ADecayedSteppingStone::TickEmerging(float DeltaTime)
{
	EmergeElapsed += DeltaTime;
	const float Alpha = FMath::Clamp(EmergeElapsed / EmergeDuration, 0.0f, 1.0f);
	// 発進/到達を柔らかくする
	const float Eased = FMath::SmoothStep(0.0f, 1.0f, Alpha);

	// 始点→終点の直線位置を「中心軸」とし、その周りを螺旋で巻き上げる
	// (地雷のUpdateCaptureを固定ターゲット版に)
	// 半径はsin包絡で始点/終点0・中間最大 → 段差なく巻き上がり、終点
	// (RiseTarget)へぴたりと収束する
	const FVector BasePos = FMath::Lerp(EmergeStartLocation, EmergeTargetLocation, Eased);
	OrbitAngleDeg += EmergeSpinSpeedDeg * DeltaTime;
	const float Radius = EmergeSpiralRadius * FMath::Sin(Alpha * PI);
	const float AngleRad = FMath::DegreesToRadians(OrbitAngleDeg);
	const FVector Spiral(Radius * FMath::Cos(AngleRad), Radius * FMath::Sin(AngleRad), 0.0f);

	SetActorLocation(BasePos + Spiral);

	if (Alpha >= 1.0f)
	{
		// 通常浮石のコリジョンへ戻す
		// (BlockAll=WorldStaticで乗れる・奈落検知のOverlapも戻す)
		StoneMesh->SetCollisionProfileName(TEXT("BlockAll"));
		StoneMesh->SetGenerateOverlapEvents(true);

		// 到達点を定位置に据えて基底の通常挙動(Floating)へ委譲する
		SettleAt(EmergeTargetLocation);
	}
}
