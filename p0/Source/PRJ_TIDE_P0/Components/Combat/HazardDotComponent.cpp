// Copyright (c) 2026, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.

#include "PRJ_TIDE_P0/Components/Combat/HazardDotComponent.h"

#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Character.h"
#include "Components/SkeletalMeshComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"

#include "PRJ_TIDE_P0/Interfaces/Combat/IDamageable.h"
#include "PRJ_TIDE_P0/Data/Combat/DamageInfo.h"
#include "PRJ_TIDE_P0/Utilities/HitStopUtils.h"

UHazardDotComponent::UHazardDotComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	// DoT を持つ間だけ Tick する（RefreshDot で有効化、空になったら無効化）
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UHazardDotComponent::RefreshDot( const FHazardDotSpec& Spec )
{
	if ( !Spec.TypeTag.IsValid() ) return;

	const UWorld* World = GetWorld();
	if ( !World ) return;
	const double Now = World->GetTimeSeconds();

	// 被弾中のループ演出（ゾンビのオーラと同じ）を、床にいる間はリフレッシュし続ける（RefreshGrace で退出停止）
	RefreshDamageAura( Spec.DamageAuraVFX, RefreshGrace );

	// 既存 entry（同じ型）があればパラメータを最新ソースで上書きし、カデンツ（LastTickTime）は維持する
	for ( FActiveHazardDot& Dot : ActiveDots )
	{
		if ( Dot.Spec.TypeTag == Spec.TypeTag )
		{
			Dot.Spec = Spec;
			Dot.ExpiryTime = Now + RefreshGrace;
			return;
		}
	}

	// 新規 entry。bTickImmediatelyOnEnter なら LastTickTime を過去へ置き、次 Tick で即発火させる
	FActiveHazardDot NewDot;
	NewDot.Spec = Spec;
	NewDot.ExpiryTime = Now + RefreshGrace;
	NewDot.LastTickTime = bTickImmediatelyOnEnter ? ( Now - Spec.TickInterval ) : Now;
	ActiveDots.Add( NewDot );

	SetComponentTickEnabled( true );
}

void UHazardDotComponent::TickComponent( float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction )
{
	Super::TickComponent( DeltaTime, TickType, ThisTickFunction );

	const UWorld* World = GetWorld();
	if ( !World ) return;
	const double Now = World->GetTimeSeconds();

	for ( int32 i = ActiveDots.Num() - 1; i >= 0; --i )
	{
		FActiveHazardDot& Dot = ActiveDots[i];

		// リフレッシュが途切れた（範囲外へ出た）→ 停止
		if ( Now > Dot.ExpiryTime )
		{
			ActiveDots.RemoveAt( i );
			continue;
		}

		// 刻み
		const float Interval = FMath::Max( 0.01f, Dot.Spec.TickInterval );
		if ( Now - Dot.LastTickTime >= Interval )
		{
			ApplyTick( Dot );
			Dot.LastTickTime = Now;
		}
	}

	// 被弾オーラの寿命：リフレッシュが途切れて猶予を過ぎたら Deactivate（コンポーネントは使い回す）
	if ( DamageAuraComponent && DamageAuraComponent->IsActive() && Now > DamageAuraExpiryTime )
	{
		DamageAuraComponent->Deactivate();
	}

	// DoT もオーラも無くなったら Tick を止める（アイドル時の負荷を避ける）
	const bool bAuraActive = DamageAuraComponent && DamageAuraComponent->IsActive();
	if ( ActiveDots.Num() == 0 && !bAuraActive )
	{
		SetComponentTickEnabled( false );
	}
}

void UHazardDotComponent::RefreshDamageAura( UNiagaraSystem* AuraVFX, float HoldDuration )
{
	if ( !AuraVFX || HoldDuration <= 0.0f ) return;

	const UWorld* World = GetWorld();
	if ( !World ) return;
	AActor* Owner = GetOwner();
	if ( !Owner ) return;

	// アセットが変わった／未生成なら（張り替えて）対象ルートへアタッチ生成する。
	// ゾンビのオーラと同じ：ループ前提・bAutoDestroy=false（こちらで Deactivate して使い回す）
	if ( !DamageAuraComponent || DamageAuraAsset.Get() != AuraVFX )
	{
		if ( DamageAuraComponent )
		{
			DamageAuraComponent->DestroyComponent();
			DamageAuraComponent = nullptr;
		}

		// アタッチ先は SkeletalMesh（キャラなら GetMesh()）。取れなければ任意の SkeletalMeshComponent を探し、
		// それも無ければ Root へフォールバック
		USkeletalMeshComponent* SkelMesh = nullptr;
		if ( const ACharacter* Character = Cast<ACharacter>( Owner ) )
		{
			SkelMesh = Character->GetMesh();
		}
		if ( !SkelMesh )
		{
			SkelMesh = Owner->FindComponentByClass<USkeletalMeshComponent>();
		}
		USceneComponent* AttachTarget = SkelMesh ? Cast<USceneComponent>( SkelMesh ) : Owner->GetRootComponent();

		if ( AttachTarget )
		{
			// SpawnSystemAttached はアセットの pre-cull やプーリングの都合で、アタッチ先が有効でも nullptr を返すことが
			// ある。確実にアタッチしたいのでコンポーネントを明示生成する（WindZone の追従 VFX と同方式）
			UNiagaraComponent* Comp = NewObject<UNiagaraComponent>( Owner );
			if ( Comp )
			{
				Comp->SetupAttachment( AttachTarget );
				Comp->SetRelativeLocation( FVector::ZeroVector );
				Comp->SetRelativeRotation( FRotator::ZeroRotator );
				Comp->SetAutoActivate( false );
				Comp->SetAsset( AuraVFX );
				Comp->RegisterComponent();
				Comp->Activate( true );

				DamageAuraComponent = Comp;
				DamageAuraAsset = AuraVFX;
			}
		}
	}
	else if ( !DamageAuraComponent->IsActive() )
	{
		// 一度 Deactivate 済み（床から出て戻ってきた等）なら再点火する
		DamageAuraComponent->Activate( true );
	}

	DamageAuraExpiryTime = World->GetTimeSeconds() + HoldDuration;
	SetComponentTickEnabled( true );
}

void UHazardDotComponent::ApplyTick( const FActiveHazardDot& Dot )
{
	AActor* Owner = GetOwner();
	if ( !Owner ) return;

	IDamageable* Damageable = Cast<IDamageable>( Owner );
	if ( !Damageable ) return;

	FDamageInfo DamageInfo;
	DamageInfo.BaseDamage        = Dot.Spec.DamagePerTick;
	DamageInfo.Instigator        = Dot.Spec.Instigator;
	DamageInfo.HitReactionTag    = Dot.Spec.HitReactionTag;
	DamageInfo.bIsDamageOverTime = true;	// DoT はプレイヤー側で被弾リアクション・カメラ揺れをスキップさせる

	const EDamageResult Result = Damageable->ReceiveDamage( DamageInfo );

	// 実際にダメージが通った刻みのみ演出を出す（無敵・回避では出さない）
	if ( Result != EDamageResult::Hit ) return;

	// ヒットエフェクト（対象位置へ）。刻みごとにビリッと見せる
	if ( Dot.Spec.HitEffect )
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation( Owner, Dot.Spec.HitEffect, Owner->GetActorLocation() );
	}

	// ヒットストップはプレイヤーのみ（毎刻みかかるので短めに）
	if ( Dot.Spec.bUseHitStop )
	{
		const APawn* Pawn = Cast<APawn>( Owner );
		if ( Pawn && Pawn->IsPlayerControlled() )
		{
			HitStopUtil::ApplyHitStop( Owner, Dot.Spec.HitStopDuration, Dot.Spec.HitStopDilation );
		}
	}
}
