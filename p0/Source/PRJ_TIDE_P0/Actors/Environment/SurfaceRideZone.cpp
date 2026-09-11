// Copyright (c) 2026, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.

#include "SurfaceRideZone.h"

#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "DrawDebugHelpers.h"
#include "Kismet/GameplayStatics.h"
#include "PRJ_TIDE_P0/Actors/Character/Player/TidePlayerCharacter.h"

ASurfaceRideZone::ASurfaceRideZone()
{
	// 判定自体はオーバーラップのイン／アウトで完結する。Tick はエリア可視化と、
	// 開始時から重なっていた場合の取りこぼし解決にのみ使う
	PrimaryActorTick.bCanEverTick = true;

	AreaBox = CreateDefaultSubobject<UBoxComponent>( TEXT( "AreaBox" ) );
	SetRootComponent( AreaBox );

	AreaBox->SetBoxExtent( FVector( 500.0f, 500.0f, 500.0f ) );
	AreaBox->SetCollisionEnabled( ECollisionEnabled::QueryOnly );
	AreaBox->SetCollisionObjectType( ECC_WorldStatic );
	AreaBox->SetCollisionResponseToAllChannels( ECR_Ignore );
	AreaBox->SetCollisionResponseToChannel( ECC_Pawn, ECR_Overlap );
	AreaBox->SetGenerateOverlapEvents( true );

	AreaBox->OnComponentBeginOverlap.AddDynamic( this, &ASurfaceRideZone::OnAreaBeginOverlap );
	AreaBox->OnComponentEndOverlap.AddDynamic( this, &ASurfaceRideZone::OnAreaEndOverlap );
}

void ASurfaceRideZone::BeginPlay()
{
	Super::BeginPlay();

	// エディタの「ここから開始」等でエリアの中から始まると、状態変化が起きないため
	// BeginOverlap が飛ばず検知できない。開始時点の重なりを自分で拾う
	bInitialOverlapResolved = ResolveInitialOverlap();
}

void ASurfaceRideZone::Tick( float DeltaTime )
{
	Super::Tick( DeltaTime );

	// BeginPlay 時点ではプレイヤーがまだ生成されていないことがあるため、取れるまで再試行する
	if ( !bInitialOverlapResolved )
	{
		bInitialOverlapResolved = ResolveInitialOverlap();
	}

#if !UE_BUILD_SHIPPING
	if ( !bDebugDraw || !AreaBox ) return;

	// プレイヤーが入っている間は緑、それ以外は水色。どのエリアに反応しているか一目で分かるようにする。
	// 上り坂アシスト（＝物理準拠でない嘘のエリア）は在籍外でも橙にして、通常のエリアと区別する
	const FColor DrawColor = bPlayerInside ? FColor::Green : ( bUphillAssist ? FColor::Orange : FColor::Cyan );
	DrawDebugBox( GetWorld(), AreaBox->GetComponentLocation(), AreaBox->GetScaledBoxExtent(),
		AreaBox->GetComponentQuat(), DrawColor, false, -1.0f, 0, 4.0f );
#endif
}

bool ASurfaceRideZone::ResolveInitialOverlap()
{
	if ( !AreaBox ) return true;	// 形状が無い＝再試行しても無意味

	// プレイヤーがまだ居ないなら未解決のまま次フレームへ持ち越す
	auto* Player = Cast<ATidePlayerCharacter>( UGameplayStatics::GetPlayerCharacter( this, 0 ) );
	if ( !Player ) return false;

	// 配置直後は重なり情報が未生成のことがあるので明示的に更新してから問い合わせる
	AreaBox->UpdateOverlaps();
	if ( AreaBox->IsOverlappingActor( Player ) )
	{
		SetPlayerInside( Player, true );
	}
	return true;
}

void ASurfaceRideZone::SetPlayerInside( ATidePlayerCharacter* Player, bool bInside )
{
	// 開始時検知とオーバーラップイベントの両方から呼ばれるため、同じ状態への再設定は捨てる。
	// ここを通さないとプレイヤー側の重なり数が二重に増減してカウントがずれる
	if ( !Player || bPlayerInside == bInside ) return;

	bPlayerInside = bInside;

	FSurfaceRideZoneFlags Flags;
	Flags.bInvertLateral = bInvertLateralInput;
	Flags.bInvertForward = bInvertForwardInput;
	Flags.bScreenRelative = bScreenRelativeInput;
	Flags.bCameraRoll = bCameraRollFollowUp;
	Flags.bChaseCamera = bChaseCameraFollowTravel;
	Flags.bTubeRelative = bTubeRelativeInput;
	Flags.bUphillAssist = bUphillAssist;

	if ( bInside )
	{
		Player->EnterSurfaceRideZone( Flags );
	}
	else
	{
		Player->ExitSurfaceRideZone( Flags );
	}
}

namespace
{
	// カプセル以外（メッシュ等）も Pawn チャンネルで重なると Begin/End が複数回飛ぶため、カプセル 1 本だけを見る
	ATidePlayerCharacter* GetOverlappingPlayer( AActor* OtherActor, UPrimitiveComponent* OtherComp )
	{
		ATidePlayerCharacter* Player = Cast<ATidePlayerCharacter>( OtherActor );
		if ( !Player || OtherComp != Player->GetCapsuleComponent() ) return nullptr;
		return Player;
	}
}

void ASurfaceRideZone::OnAreaBeginOverlap( UPrimitiveComponent* /*OverlappedComponent*/, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 /*OtherBodyIndex*/, bool /*bFromSweep*/, const FHitResult& /*SweepResult*/ )
{
	SetPlayerInside( GetOverlappingPlayer( OtherActor, OtherComp ), true );
}

void ASurfaceRideZone::OnAreaEndOverlap( UPrimitiveComponent* /*OverlappedComponent*/, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 /*OtherBodyIndex*/ )
{
	SetPlayerInside( GetOverlappingPlayer( OtherActor, OtherComp ), false );
}
