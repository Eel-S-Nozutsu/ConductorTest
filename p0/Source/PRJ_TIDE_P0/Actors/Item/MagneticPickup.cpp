// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#include "PRJ_TIDE_P0/Actors/Item/MagneticPickup.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "PRJ_TIDE_P0/PRJ_TIDE_P0.h"
#include "PRJ_TIDE_P0/Actors/Character/TideCharacter.h"
#include "PRJ_TIDE_P0/Components/Combat/StatusComponent.h"

AMagneticPickup::AMagneticPickup()
{
	PrimaryActorTick.bCanEverTick = true;
	// アイドル個体はTickを持たない
	// 吸着中と重力落下中だけ回す
	// 起こすのはオーバーラップ
	PrimaryActorTick.bStartWithTickEnabled = false;

	AttractionComp = CreateDefaultSubobject<USphereComponent>(TEXT("AttractionComp"));
	AttractionComp->InitSphereRadius(AttractionRadius);
	AttractionComp->SetCollisionProfileName(TEXT("OverlapOnlyPawn"));
	AttractionComp->SetGenerateOverlapEvents(true);
	RootComponent = AttractionComp;
	CollectionComp = CreateDefaultSubobject<USphereComponent>(TEXT("CollectionComp"));
	CollectionComp->InitSphereRadius(CollectionRadius);
	CollectionComp->SetupAttachment(RootComponent);
	CollectionComp->SetCollisionProfileName(TEXT("OverlapOnlyPawn"));
	CollectionComp->SetGenerateOverlapEvents(true);
	SparkleComp = CreateDefaultSubobject<UNiagaraComponent>(TEXT("SparkleComp"));
	SparkleComp->SetupAttachment(RootComponent);
	SparkleComp->bAutoActivate = true;
}

void AMagneticPickup::BeginPlay()
{
	Super::BeginPlay();
	SpawnLocation = GetActorLocation();

	// デリゲートはランタイムで束縛する(コンストラクタ束縛はインスタンスに焼き付き、古い個体で剥がれる)。
	// ストリーミング再入の二重束縛を避けるためAddUnique、EndPlayで解除。ConfigureOverlapが同期的に
	// オーバーラップを発火し得るので、束縛はその前に済ませる
	AttractionComp->OnComponentBeginOverlap.AddUniqueDynamic(this, &AMagneticPickup::OnAttractionBeginOverlap);
	CollectionComp->OnComponentBeginOverlap.AddUniqueDynamic(this, &AMagneticPickup::OnCollectionBeginOverlap);

	// エディタで個体ごとに半径を変えてもトリガー球の半径を実値に合わせる
	AttractionComp->SetSphereRadius(AttractionRadius);
	CollectionComp->SetSphereRadius(CollectionRadius);

	// BPに旧コード由来の古いコリジョン設定(NoCollision)が焼き付いていても確実に上書きするため、
	// 当たり判定はランタイムで張り直す。全チャンネルIgnore→PawnのみOverlapで、トリガーボリューム等への無駄ヒットも消す
	auto ConfigureOverlap = [](USphereComponent* Sphere)
	{
		Sphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		Sphere->SetCollisionObjectType(ECC_WorldDynamic);
		Sphere->SetCollisionResponseToAllChannels(ECR_Ignore);
		Sphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
		Sphere->SetGenerateOverlapEvents(true);
	};
	ConfigureOverlap(AttractionComp);
	ConfigureOverlap(CollectionComp);

	// 同時スポーンした個体の足元チェックが同フレームに固まらないよう位相をばらす
	GroundCheckTimer = FMath::FRandRange(0.0f, GroundCheckInterval);

	if (bUseGravity)
	{
		// 落下は距離に関係なく完了させたいのでこの個体はTickを起こしておく
		SetActorTickEnabled(true);
		BeginDrop(DropInitialUpSpeed);
	}

	// 起動時から吸着範囲内にいる個体を拾う。BeginOverlapは「外→内」の遷移でしか鳴らず、
	// 最初から内側だと発火しないため(ストリーミングの初期オーバーラップ抑止も同様)、ここで一度だけ判定する
	if (const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0))
	{
		if (FVector::DistSquared(Player->GetActorLocation(), GetActorLocation()) <= FMath::Square(AttractionRadius))
		{
			BeginAttraction();
		}
	}

	if (SparkleEffect)
	{
		SparkleComp->SetAsset(SparkleEffect);
	}
}

void AMagneticPickup::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// BeginPlayでランタイム束縛した分を解除(ストリーミングでの再入時に多重束縛しないため)
	AttractionComp->OnComponentBeginOverlap.RemoveDynamic(this, &AMagneticPickup::OnAttractionBeginOverlap);
	CollectionComp->OnComponentBeginOverlap.RemoveDynamic(this, &AMagneticPickup::OnCollectionBeginOverlap);

	Super::EndPlay(EndPlayReason);
}

void AMagneticPickup::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// 吸着中はプレイヤーへ飛行
	// 取得はCollectionCompのオーバーラップが担う
	if (bAttracting)
	{
		const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
		if (!Player) return;

		const FVector ToPlayer = Player->GetActorLocation() - GetActorLocation();
		CurrentAttractionSpeed = FMath::Min(
			CurrentAttractionSpeed + AttractionAcceleration * DeltaTime,
			AttractionMaxSpeed);

		SetActorLocation(GetActorLocation() + ToPlayer.GetSafeNormal()
			* CurrentAttractionSpeed * DeltaTime);
		return;
	}

	if (bDropping)
	{
		UpdateDrop(DeltaTime);
		return;
	}

	// 着地済みの重力個体は足場が壊れていないかだけ監視し続ける (見た目は動かさない)
	if (bUseGravity)
	{
		UpdateGroundCheck(DeltaTime);
		return;
	}

	// 非重力のアイドルはそもそもTickを持たない 念のための保険で停止
	SetActorTickEnabled(false);
}

void AMagneticPickup::OnAttractionBeginOverlap(
	UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
	bool bFromSweep, const FHitResult& SweepResult)
{
	// 敵ポーンも範囲に入るためプレイヤー本人のときだけ反応する
	if (OtherActor != UGameplayStatics::GetPlayerPawn(this, 0)) return;

	BeginAttraction();
}

void AMagneticPickup::BeginAttraction()
{
	if (bAttracting) return;

	bAttracting = true;
	CurrentAttractionSpeed = AttractionInitialSpeed;
	SetActorTickEnabled(true);
}

void AMagneticPickup::OnCollectionBeginOverlap(
	UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
	bool bFromSweep, const FHitResult& SweepResult)
{
	if (OtherActor != UGameplayStatics::GetPlayerPawn(this, 0)) return;

	Pickup();
}

void AMagneticPickup::BeginDrop(float InitialUpSpeed)
{
	bDropping = true;
	DropVelocityZ = InitialUpSpeed;
	DropStartZ = GetActorLocation().Z;
}

void AMagneticPickup::UpdateGroundCheck(float DeltaTime)
{
	if (!bUseGravity) return;

	GroundCheckTimer -= DeltaTime;
	if (GroundCheckTimer > 0.0f) return;
	GroundCheckTimer = GroundCheckInterval;

	// 浮遊で上下するので、揺れの基準であるSpawnLocationから測る
	FHitResult Hit;
	const FVector TraceEnd = SpawnLocation - FVector(
		0.0f, 0.0f, GroundOffset + GroundCheckTolerance);

	if (!TraceGround(SpawnLocation, TraceEnd, Hit))
	{
		// 足場が壊れた/動いた
		// 初速なしで落下を再開する
		BeginDrop(0.0f);
	}
}

bool AMagneticPickup::TraceGround(const FVector& Start, const FVector& End, FHitResult& OutHit) const
{
	const UWorld* World = GetWorld();
	if (!World) return false;

	// ECC_VisibilityのByChannelは「Visibilityをブロックするもの」を拾うので、キャラのカプセル
	// (ATideCharacterがBlockに設定している)やトリガーボリュームまで床とみなして空中で止まる。
	// 床だけを狙うためObjectType列挙で引く(AAreaHazardField::ResolveGroundPointと同じ方針)
	FCollisionObjectQueryParams GroundQuery;
	GroundQuery.AddObjectTypesToQuery(ECC_WorldStatic);
	GroundQuery.AddObjectTypesToQuery(ECC_WorldDynamic);
	FCollisionQueryParams TraceParams(SCENE_QUERY_STAT(MagneticPickupGround), false, this);

	TArray<FHitResult> Hits;
	if (!World->LineTraceMultiByObjectType(Hits, Start, End, GroundQuery, TraceParams)) return false;

	// ByObjectTypeは相手のResponseを見ないため、全チャンネルIgnoreのカプセル・ボリューム・
	// 他個体の球まで返ってくる。シェイプは床ではないので捨て、その奥の本当の床まで貫通させる
	const FHitResult* GroundHit = Hits.FindByPredicate([](const FHitResult& Hit)
	{
		return Hit.GetComponent() && !Hit.GetComponent()->IsA<UShapeComponent>();
	});
	if (!GroundHit) return false;

	OutHit = *GroundHit;
	return true;
}

void AMagneticPickup::UpdateDrop(float DeltaTime)
{
	DropVelocityZ -= DropGravity * DeltaTime;

	const FVector CurrentLoc = GetActorLocation();
	const FVector NextLoc = CurrentLoc + FVector(0.0f, 0.0f, DropVelocityZ * DeltaTime);

	// 上昇中は接地判定しない
	// 跳ね上がりの途中で頭上の足場を拾わないようにする
	if (DropVelocityZ <= 0.0f)
	{
		FHitResult Hit;
		const FVector TraceEnd = NextLoc - FVector(0.0f, 0.0f, GroundOffset);

		if (TraceGround(CurrentLoc, TraceEnd, Hit))
		{
			Land(Hit.ImpactPoint + FVector(0.0f, 0.0f, GroundOffset));
			return;
		}
	}

	SetActorLocation(NextLoc);

	// 穴に落ちた等で接地面が見つからない場合の保険
	if (DropStartZ - NextLoc.Z > MaxDropDistance)
	{
		UE_LOG(LogPRJ_TIDE_P0, Warning,
			TEXT("[MagneticPickup] %s: 落下%.0fcmで床が見つからず打ち切った。生成位置が高すぎるか真下に床が無い"),
			*GetName(), MaxDropDistance);

		Land(NextLoc);
		// 打ち切った個体はTickを畳む。足元判定を回すと「落ちて止まる」を延々繰り返す
		// (足元判定のトレースはGroundOffset+Tolerance分しかないので必ず空振りして再落下する)
		SetActorTickEnabled(false);
	}
}

void AMagneticPickup::Land(const FVector& Location)
{
	bDropping = false;
	DropVelocityZ = 0.0f;
	GroundCheckTimer = GroundCheckInterval;
	SpawnLocation = Location;
	SetActorLocation(Location);
}

void AMagneticPickup::Pickup()
{
	if (HealAmount > 0.0f)
	{
		if (ATideCharacter* TideChar = Cast<ATideCharacter>(
			UGameplayStatics::GetPlayerPawn(this, 0)))
		{
			if (UStatusComponent* StatusComp = TideChar->GetStatusComponent())
			{
				StatusComp->ModifyHP(HealAmount);
			}
		}
	}

	if (PickupEffect)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			this, PickupEffect, GetActorLocation());
	}

	Destroy();
}
