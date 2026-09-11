// Copyright (c) 2026, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.
#include "ChargeEffectSubModule.h"

#include <imgui.h>
#include <string>

#include "NiagaraFunctionLibrary.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "Components/PoseableMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/KismetMaterialLibrary.h"
#include "Kismet/KismetMathLibrary.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialParameterCollection.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"

#include "Camera/PlayerCameraManager.h"
#include "GameFramework/PlayerController.h"

#include "PRJ_TIDE_P0/Composites/Player/PlayerModules/ChargeActionPlayerModule_V2.h"
#include "PRJ_TIDE_P0/Actors/Character/Player/TidePlayerCharacter.h"
#include "PRJ_TIDE_P0/Data/Player/TidePlayerParamDataAsset.h"
#include "PRJ_TIDE_P0/Data/Niagara/NiagaraSystemListDataAsset.h"
#include "PRJ_TIDE_P0/Data/Player/PlayerNiagaraTags.h"
#include "PRJ_TIDE_P0/Core/TideGameSettings.h"
#include "PRJ_TIDE_P0/Integration/ImGui/TideHudColors.h"
#include "PRJ_TIDE_P0/Integration/ImGui/TideHudAnim.h"

void UChargeEffectSubModule::Initialize( UChargeActionPlayerModule_V2* InOwnerModule, ATidePlayerCharacter* InOwnerCharacter )
{
	OwnerModule = InOwnerModule;
	OwnerCharacter = InOwnerCharacter;
}

void UChargeEffectSubModule::UpdateSkidEffects( float DeltaTime )
{
	if ( !OwnerCharacter || !OwnerModule ) return;
	const UTidePlayerParamDataAsset* PlayerParams = OwnerCharacter->PlayerParamData;
	if ( !PlayerParams ) return;

	UCharacterMovementComponent* MovementComp = OwnerCharacter->GetCharacterMovement();
	if ( !MovementComp || !MovementComp->IsMovingOnGround() )
	{
		SkidEffectSpawnTimer.Clear();
		return;
	}

	// 空中通常攻撃（振り下ろし）はチャージではないので除外する（IsPlayingChargeAttack が拾ってしまうため）
	const bool bIsActiveChargeMove = ( OwnerModule->IsCharging() || OwnerModule->IsPlayingChargeDash() || OwnerModule->IsPlayingChargeAttack() )
		&& !OwnerModule->IsAirNormalDiveAttack();

	// 立ち止まっている時は出さない
	const float CurrentSpeedSq = MovementComp->Velocity.SizeSquared2D();
	constexpr float MinSpeedSqForSkid = 50.0f * 50.0f;

	if ( bIsActiveChargeMove && CurrentSpeedSq > MinSpeedSqForSkid )
	{
		SkidEffectSpawnTimer.Update( DeltaTime );

		if ( SkidEffectSpawnTimer.IsFinish() )
		{
			SpawnSkidEffect();
			SkidEffectSpawnTimer.Set( PlayerParams->ChargeSkidEffectInterval );
		}
	}
	else
	{
		SkidEffectSpawnTimer.Clear();
	}
}

void UChargeEffectSubModule::SpawnSkidEffect()
{
	if ( !OwnerCharacter ) return;
	const UTidePlayerParamDataAsset* PlayerParams = OwnerCharacter->PlayerParamData;
	if ( !PlayerParams ) return;

	UWorld* World = OwnerCharacter->GetWorld();
	if ( !World ) return;

	// キャラ原点から足元より少し下まで
	const FVector StartLoc = OwnerCharacter->GetActorLocation();
	const FVector EndLoc = StartLoc - FVector( 0.0f, 0.0f, 150.0f );

	FCollisionQueryParams QueryParams( SCENE_QUERY_STAT( SkidEffectTrace ), false, OwnerCharacter );
	QueryParams.bReturnPhysicalMaterial = true;	// 地面の種類でエフェクトを切り替えるため

	FHitResult Hit;
	if ( World->LineTraceSingleByChannel( Hit, StartLoc, EndLoc, ECC_Visibility, QueryParams ) )
	{
		EPhysicalSurface HitSurfaceType = SurfaceType_Default;
		if ( Hit.PhysMaterial.IsValid() )
		{
			HitSurfaceType = Hit.PhysMaterial->SurfaceType;
		}

		FName EffectTag = NAME_None;

		// ヒットした地形用のタグ、無ければ Default
		if ( const FName* FoundTag = PlayerParams->ChargeSkidEffectTagsMap.Find( HitSurfaceType ) )
		{
			EffectTag = *FoundTag;
		}
		else if ( const FName* DefaultTag = PlayerParams->ChargeSkidEffectTagsMap.Find( SurfaceType_Default ) )
		{
			EffectTag = *DefaultTag;
		}

		if ( EffectTag != NAME_None && OwnerCharacter->NiagaraSystemDataAsset )
		{
			if ( UNiagaraSystem* EffectSys = OwnerCharacter->NiagaraSystemDataAsset->GetNiagaraSystem( EffectTag ) )
			{
				FRotator EffectRotation = OwnerCharacter->GetActorRotation();
				UNiagaraFunctionLibrary::SpawnSystemAtLocation( World, EffectSys, Hit.ImpactPoint, EffectRotation, FVector( 1.0f ) );
			}
		}
	}
}

void UChargeEffectSubModule::UpdateHitbackSkidEffects( float DeltaTime, bool bIsInKnockback )
{
	if ( !OwnerCharacter || !OwnerModule ) return;
	const UTidePlayerParamDataAsset* PlayerParams = OwnerCharacter->PlayerParamData;
	if ( !PlayerParams ) return;

	UCharacterMovementComponent* MovementComp = OwnerCharacter->GetCharacterMovement();

	constexpr float MinSpeedSqForSkid = 50.0f * 50.0f;	// UpdateSkidEffects と同じ閾値
	const bool bIsMoving = MovementComp && MovementComp->Velocity.SizeSquared2D() > MinSpeedSqForSkid;

	if ( !bIsInKnockback || !MovementComp || !MovementComp->IsMovingOnGround() || !bIsMoving )
	{
		HitbackSkidSpawnTimer.Clear();
		return;
	}

	HitbackSkidSpawnTimer.Update( DeltaTime );
	if ( HitbackSkidSpawnTimer.IsFinish() )
	{
		SpawnSkidEffect();	// キャラ原点の真下へ 1 つだけ（両足から 2 つ出さない）
		HitbackSkidSpawnTimer.Set( PlayerParams->HitbackSkidEffectInterval );
	}
}

// --- 残像（ゴーストトレイル）---

void UChargeEffectSubModule::NotifyChargeStarted( float GhostTrailSpawnInterval )
{
	bBlockGhostTrailSpawn = false;
	GhostTrailSpawnTimer.Set( GhostTrailSpawnInterval );
}

void UChargeEffectSubModule::ResetGhostTrailSpawnTimer()
{
	GhostTrailSpawnTimer.Clear();
}

bool UChargeEffectSubModule::HasActiveGhostTrails() const
{
	return ActiveGhostTrails.Num() > 0;
}

bool UChargeEffectSubModule::IsGhostTrailCycleActive() const
{
	return HasActiveGhostTrails() || ShouldSpawnGhostTrail();
}

void UChargeEffectSubModule::UpdateGhostTrails( float DeltaTime )
{
	const UTidePlayerParamDataAsset* PlayerParams = OwnerCharacter ? OwnerCharacter->PlayerParamData : nullptr;
	if ( !OwnerCharacter || !OwnerModule || !PlayerParams ) return;

	if ( ShouldSpawnGhostTrail() )
	{
		GhostTrailSpawnTimer.Update( DeltaTime );

		if ( GhostTrailSpawnTimer.IsFinish() )
		{
			SpawnGhostTrail();
			GhostTrailSpawnTimer.Set( PlayerParams->GhostTrailSpawnInterval );
		}
	}
	else
	{
		GhostTrailSpawnTimer.Clear();
	}

	// フェードアウトと破棄
	for ( int32 i = ActiveGhostTrails.Num() - 1; i >= 0; --i )
	{
		FGhostTrailData& Trail = ActiveGhostTrails[i];
		Trail.ElapsedTime += DeltaTime;

		if ( Trail.ElapsedTime >= Trail.Lifespan )
		{
			if ( IsValid( Trail.MeshComponent ) )
			{
				Trail.MeshComponent->DestroyComponent();
			}
			ActiveGhostTrails.RemoveAt( i );
			continue;
		}

		const float FadeAlpha = 1.0f - ( Trail.ElapsedTime / Trail.Lifespan );
		for ( UMaterialInstanceDynamic* MID : Trail.MIDs )
		{
			if ( IsValid( MID ) )
			{
				MID->SetScalarParameterValue( FName( "FadeAmount" ), FadeAlpha );
			}
		}
	}
}

void UChargeEffectSubModule::SpawnGhostTrail()
{
	UMaterialInterface* GhostMaterial = GetCurrentGhostTrailMaterial();
	if ( !OwnerCharacter || !OwnerCharacter->PlayerParamData || !GhostMaterial ) return;

	USkeletalMeshComponent* CharacterMesh = OwnerCharacter->GetMesh();
	if ( !CharacterMesh ) return;

	UPoseableMeshComponent* GhostMesh = NewObject<UPoseableMeshComponent>( OwnerCharacter );
	if ( !GhostMesh ) return;

	GhostMesh->RegisterComponent();
	GhostMesh->SetWorldLocationAndRotation( CharacterMesh->GetComponentLocation(), CharacterMesh->GetComponentRotation() );
	GhostMesh->SetSkinnedAssetAndUpdate( CharacterMesh->GetSkeletalMeshAsset() );
	GhostMesh->CopyPoseFromSkeletalComponent( CharacterMesh );
	GhostMesh->SetCollisionEnabled( ECollisionEnabled::NoCollision );
	GhostMesh->CastShadow = false;

	FGhostTrailData NewTrailData;
	NewTrailData.MeshComponent = GhostMesh;
	NewTrailData.Lifespan = OwnerCharacter->PlayerParamData->GhostTrailLifespan;

	const int32 NumMaterials = GhostMesh->GetNumMaterials();
	for ( int32 i = 0; i < NumMaterials; ++i )
	{
		UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create( GhostMaterial, GhostMesh );
		if ( MID )
		{
			MID->SetScalarParameterValue( FName( "FadeAmount" ), 1.0f );
			GhostMesh->SetMaterial( i, MID );
			NewTrailData.MIDs.Add( MID );
		}
	}

	UWorld* World = OwnerCharacter->GetWorld();
	if ( World )
	{
		GhostMesh->RegisterComponentWithWorld( World );
	}

	ActiveGhostTrails.Add( NewTrailData );
}

bool UChargeEffectSubModule::ShouldSpawnGhostTrail() const
{
	if ( !OwnerCharacter || !OwnerModule || bBlockGhostTrailSpawn ) return false;
	// ダッシュ ST モーションが再生されている間だけ残像の生成を許可する
	return OwnerModule->IsPlayingChargeDash() && OwnerModule->IsPlayingChargeDashStartMontage();
}

UMaterialInterface* UChargeEffectSubModule::GetCurrentGhostTrailMaterial() const
{
	const UTidePlayerParamDataAsset* PlayerParams = OwnerCharacter ? OwnerCharacter->PlayerParamData : nullptr;
	if ( !PlayerParams ) return nullptr;

	const int32 ArrayIndex = FMath::Clamp( OwnerModule->GetCurrentChargeGearIndex() - 1, 0, PlayerParams->MaxChargeGearCount - 1 );

	if ( PlayerParams->ChargeV2GhostTrailMaterials.IsValidIndex( ArrayIndex ) )
	{
		if ( UMaterialInterface* Mat = PlayerParams->ChargeV2GhostTrailMaterials[ArrayIndex] )
		{
			return Mat;
		}
	}

	return PlayerParams->ChargeActionGhostTrailMaterial;
}

// --- フレネルエフェクト ---

void UChargeEffectSubModule::UpdateFresnelEffect( float DeltaTime )
{
	if ( !OwnerCharacter || !OwnerCharacter->PlayerMPC || !OwnerModule ) return;

	static const FName ParamName_Intensity = TEXT( "FresnelIntensity" );
	static const FName ParamName_Color = TEXT( "FresnelColor" );

	constexpr float DefaultIntensity = 0.0f;
	constexpr float ActiveBaseIntensity = 1.0f;
	const UTidePlayerParamDataAsset* PlayerParams = OwnerCharacter->PlayerParamData;
	if ( !PlayerParams ) return;

	const float FresnelFadeOutSpeed = PlayerParams->ChargeDashFresnelFadeOutSpeed;
	const float DashBlinkTriggerTime = PlayerParams->ChargeDashFresnelBlinkTriggerTime;
	constexpr float MinBlinkIntensity = 0.0f;
	constexpr float MaxBlinkIntensity = 1.0f;
	const float BaseBlinkHz = PlayerParams->ChargeDashFresnelBaseBlinkHz;
	const float MaxBlinkHz = PlayerParams->ChargeDashFresnelMaxBlinkHz;

	UMaterialParameterCollection* MPC = OwnerCharacter->PlayerMPC;
	UWorld* World = OwnerCharacter->GetWorld();

	const float DashRemainingTime = OwnerModule->GetChargeDashRemainingTime();

	// タイマーが切れている間は CurrentChargeActionType がまだ Jump でもフル発光へスナップさせずフェードさせる
	// （無いと点滅がタイマー切れの瞬間に止まりフル発光で固定される）
	const bool bChargeHopTimerExpired = OwnerModule->IsPlayingChargeHopJump() && DashRemainingTime <= 0.0f;

	// 空中通常攻撃は縦ダイブ機械をチャージ攻撃と共用する都合で CurrentChargeActionType==Attack になるが
	// 通常攻撃なので、明示的に除外しないとギアバフのフレネルが点灯してしまう
	const bool bIsAirNormalDive = OwnerModule->IsAirNormalDiveAttack();

	// 滑空中は滑空の残り時間と専用しきい値で点滅させる。滑空はチャージアクションではないので点灯条件に明示的に足す
	const bool bGliding = OwnerCharacter->IsInGlideSession();
	const float BlinkRemainingTime = bGliding ? OwnerCharacter->GetGlideRemainingTime() : DashRemainingTime;
	const float BlinkTriggerTime = bGliding ? PlayerParams->GlideFresnelBlinkTriggerTime : DashBlinkTriggerTime;

	const bool bChargeActionActive = OwnerModule->IsAnyChargeActionTypeSet() && !bChargeHopTimerExpired && !bIsAirNormalDive;
	if ( bChargeActionActive || bGliding )
	{
		TargetFresnelIntensity = ActiveBaseIntensity;

		UKismetMaterialLibrary::SetVectorParameterValue( World, MPC, ParamName_Color, OwnerModule->GetCurrentGearColor() );

		// 空中チャージダッシュ中は点滅させず点灯し続ける（チカチカ防止）。滑空中は外れて残り時間で点滅する
		const bool bKeepSolid = OwnerModule->ShouldKeepAirDashFresnelSolid();

		// 滑空の GlideMaxDuration<=0＝無制限のときは残り時間 0 で点灯維持
		if ( BlinkRemainingTime > 0.0f && !bKeepSolid && BlinkTriggerTime > 0.0f )
		{
			if ( BlinkRemainingTime <= BlinkTriggerTime )
			{
				const float TimeRatio = 1.0f - ( BlinkRemainingTime / BlinkTriggerTime );
				const float CurrentBlinkHz = FMath::Lerp( BaseBlinkHz, MaxBlinkHz, TimeRatio );

				FresnelBlinkPhase += CurrentBlinkHz * DeltaTime;
				if ( FresnelBlinkPhase >= 1.0f )
				{
					FresnelBlinkPhase -= 1.0f;
				}

				const float BlinkAlpha = FMath::Pow( 1.0f - FresnelBlinkPhase, 2.0f );
				TargetFresnelIntensity = FMath::Lerp( MinBlinkIntensity, MaxBlinkIntensity, BlinkAlpha );
			}
			else
			{
				FresnelBlinkPhase = 0.0f;
			}
		}
		else
		{
			FresnelBlinkPhase = 0.0f;
		}

		CurrentFresnelIntensity = TargetFresnelIntensity;
	}
	else
	{
		TargetFresnelIntensity = DefaultIntensity;
		CurrentFresnelIntensity = FMath::FInterpTo( CurrentFresnelIntensity, TargetFresnelIntensity, DeltaTime, FresnelFadeOutSpeed );
		FresnelBlinkPhase = 0.0f;
	}

	UKismetMaterialLibrary::SetScalarParameterValue( World, MPC, ParamName_Intensity, CurrentFresnelIntensity );
}

// --- Niagara チャージエフェクト ---

void UChargeEffectSubModule::ActivateChargeEffect()
{
	if ( SpawnedChargeEffect )
	{
		SpawnedChargeEffect->Activate();
		SpawnedChargeEffect->SetColorParameter( "Color", OwnerModule->GetCurrentGearColor() );
		SpawnedChargeEffect->SetFloatParameter( "SpeedRate", OwnerModule->GetCurrentChargeEffectSpeed() );
		return;
	}

	if ( !OwnerCharacter || !OwnerCharacter->NiagaraSystemDataAsset ) return;
	if ( UNiagaraSystem* EffectSys = OwnerCharacter->NiagaraSystemDataAsset->GetNiagaraSystem( PlayerNiagaraTags::CHARGING ) )
	{
		SpawnedChargeEffect = UNiagaraFunctionLibrary::SpawnSystemAttached(
			EffectSys,
			OwnerCharacter->GetRootComponent(),
			NAME_None,
			FVector::ZeroVector,
			FRotator::ZeroRotator,
			EAttachLocation::SnapToTarget,
			false
		);

		SpawnedChargeEffect->SetAbsolute( false, true, false );
		SpawnedChargeEffect->SetColorParameter( "Color", OwnerModule->GetCurrentGearColor() );
		SpawnedChargeEffect->SetFloatParameter( "SpeedRate", OwnerModule->GetCurrentChargeEffectSpeed() );
	}
}

void UChargeEffectSubModule::DeactivateChargeEffect()
{
	if ( SpawnedChargeEffect )
	{
		SpawnedChargeEffect->Deactivate();
	}
}

void UChargeEffectSubModule::SpawnChargeCompleteEffect()
{
	if ( !OwnerCharacter || !OwnerCharacter->NiagaraSystemDataAsset ) return;
	if ( UNiagaraSystem* EffectSys = OwnerCharacter->NiagaraSystemDataAsset->GetNiagaraSystem( PlayerNiagaraTags::CHARGE_COMPLETE ) )
	{
		SpawnedChargeCompleteEffect = UNiagaraFunctionLibrary::SpawnSystemAttached(
			EffectSys,
			OwnerCharacter->GetRootComponent(),
			NAME_None,
			FVector::ZeroVector,
			FRotator::ZeroRotator,
			EAttachLocation::SnapToTarget,
			true
		);
		SpawnedChargeCompleteEffect->SetColorParameter( "Color", OwnerModule->GetCurrentGearColor() );
	}
}

void UChargeEffectSubModule::SpawnChargedDashEffect()
{
	if ( !OwnerCharacter || !OwnerCharacter->NiagaraSystemDataAsset ) return;
	DestroyChargedDashEffect();
	if ( UNiagaraSystem* EffectSys = OwnerCharacter->NiagaraSystemDataAsset->GetNiagaraSystem( PlayerNiagaraTags::CHARGED_DASH ) )
	{
		SpawnedChargedDashEffect = UNiagaraFunctionLibrary::SpawnSystemAttached(
			EffectSys,
			OwnerCharacter->GetRootComponent(),
			NAME_None,
			FVector::ZeroVector,
			FRotator::ZeroRotator,
			EAttachLocation::SnapToTarget,
			true
		);
		SpawnedChargedDashEffect->SetColorParameter( "Color", OwnerModule->GetCurrentGearColor() );
	}
}

void UChargeEffectSubModule::DestroyChargedDashEffect()
{
	if ( SpawnedChargedDashEffect )
	{
		SpawnedChargedDashEffect->DestroyComponent();
		SpawnedChargedDashEffect = nullptr;
	}
}

void UChargeEffectSubModule::SpawnChargedDashWindEffect()
{
	if ( !OwnerCharacter || !OwnerCharacter->NiagaraSystemDataAsset ) return;
	DestroyChargedDashWindEffect();

	const UTidePlayerParamDataAsset* PlayerParams = OwnerCharacter->PlayerParamData;
	const float ForwardOffset = PlayerParams ? PlayerParams->ChargeDashWindEffectForwardOffset : 200.0f;

	if ( UNiagaraSystem* EffectSys = OwnerCharacter->NiagaraSystemDataAsset->GetNiagaraSystem( PlayerNiagaraTags::CHARGE_DASH_WIND ) )
	{
		SpawnedChargedDashWindEffect = UNiagaraFunctionLibrary::SpawnSystemAttached(
			EffectSys,
			OwnerCharacter->GetRootComponent(),
			NAME_None,
			FVector( ForwardOffset, 0.0f, 0.0f ),	// プレイヤー前方へオフセット
			FRotator( 0.0f, 180.0f, 0.0f ),			// Yaw をプレイヤー＋180（逆向き）
			EAttachLocation::SnapToTarget,
			true
		);
		if ( SpawnedChargedDashWindEffect )
		{
			const int32 Gear = FMath::Clamp( OwnerModule ? OwnerModule->GetCurrentChargeGearIndex() : 1, 1, 3 );
			SpawnedChargedDashWindEffect->SetIntParameter( TEXT( "Gear" ), Gear );
		}
	}
}

void UChargeEffectSubModule::DestroyChargedDashWindEffect()
{
	if ( SpawnedChargedDashWindEffect )
	{
		SpawnedChargedDashWindEffect->DestroyComponent();
		SpawnedChargedDashWindEffect = nullptr;
	}
}

// アクション中の常時エフェクト（CHARGED_DASH のアタッチ再生）は現在オフ。呼び出し側（ChargeActionPlayerModule_V2）は
// 残してあるので、復活させるならここで ActiveChargeEffect を生成・破棄する
void UChargeEffectSubModule::SpawnActiveChargeEffect()
{
}

void UChargeEffectSubModule::DestroyActiveChargeEffect()
{
}

void UChargeEffectSubModule::SpawnChargeReleaseEffect()
{
	if ( !OwnerCharacter || !OwnerCharacter->NiagaraSystemDataAsset ) return;

	const UTidePlayerParamDataAsset* PlayerParams = OwnerCharacter->PlayerParamData;
	if ( !PlayerParams ) return;

	const int32 GearIndex = OwnerModule->GetCurrentChargeGearIndex();

	// ギア段階に応じて切り替え（極(4)も最大の 3 を流す）
	FName EffectTag = PlayerNiagaraTags::CHARGE_RELEASE_01;
	if ( GearIndex == 2 )
	{
		EffectTag = PlayerNiagaraTags::CHARGE_RELEASE_02;
	}
	else if ( GearIndex >= 3 )
	{
		EffectTag = PlayerNiagaraTags::CHARGE_RELEASE_03;
	}

	if ( UNiagaraSystem* EffectSys = OwnerCharacter->NiagaraSystemDataAsset->GetNiagaraSystem( EffectTag ) )
	{
		const float ForwardOffset = PlayerParams->ChargeReleaseEffectForwardOffset;

		auto SpawnedEffect = UNiagaraFunctionLibrary::SpawnSystemAttached(
			EffectSys,
			OwnerCharacter->GetRootComponent(),
			NAME_None,
			FVector( ForwardOffset, 0.0f, 0.0f ),
			FRotator( 90.f, 0.f, 0.f ),
			EAttachLocation::SnapToTarget,
			true
		);
		SpawnedEffect->SetColorParameter( "Color", OwnerModule->GetCurrentGearColor() );
	}
}

void UChargeEffectSubModule::UpdateSpawnedChargeEffectRotation()
{
	if ( !SpawnedChargeEffect || !OwnerCharacter ) return;

	APlayerController* PC = Cast<APlayerController>( OwnerCharacter->GetController() );
	if ( !PC ) return;

	if ( const APlayerCameraManager* CameraManager = PC->PlayerCameraManager )
	{
		const FVector CameraLocation = CameraManager->GetCameraLocation();
		const FVector EffectLocation = SpawnedChargeEffect->GetComponentLocation();

		FVector DirectionToCamera = ( CameraLocation - EffectLocation );
		DirectionToCamera.Normalize();

		if ( !DirectionToCamera.IsNearlyZero() )
		{
			const FQuat LookAtQuat = DirectionToCamera.ToOrientationQuat();

			constexpr float RollAngleRad = FMath::DegreesToRadians( 90.0f );
			const FQuat RollOffsetQuat( FVector::ForwardVector, RollAngleRad );

			const FQuat FinalQuat = LookAtQuat * RollOffsetQuat;

			SpawnedChargeEffect->SetWorldRotation( FinalQuat );

		}
	}
}

void UChargeEffectSubModule::NotifyGearShifted()
{
	if ( SpawnedChargeEffect )
	{
		SpawnedChargeEffect->SetColorParameter( "Color", OwnerModule->GetCurrentGearColor() );
		SpawnedChargeEffect->SetFloatParameter( "SpeedRate", OwnerModule->GetCurrentChargeEffectSpeed() );
	}
	if ( ActiveChargeEffect )
	{
		ActiveChargeEffect->SetColorParameter( "Color", OwnerModule->GetCurrentGearColor() );
	}
	if ( SpawnedChargedDashWindEffect )
	{
		const int32 Gear = FMath::Clamp( OwnerModule ? OwnerModule->GetCurrentChargeGearIndex() : 1, 1, 3 );
		SpawnedChargedDashWindEffect->SetIntParameter( TEXT( "Gear" ), Gear );
	}
}

// --- ドリフトブースト演出（足元の加速 VFX）---

void UChargeEffectSubModule::UpdateChargeBoostEffect( float DeltaTime )
{
	// バーストは実時間で消化する（ドリフトが途切れている間も進める）
	DriftSparkBurstTimer.Update( DeltaTime );

	if ( bDriftSparkBurstForced && DriftSparkBurstTimer.IsFinish() )
	{
		bDriftSparkBurstForced = false;
	}

	// 強制バースト（突風成立・ヒットバックのギアアップ）は溜めを離しても残り時間ぶんは出し切る
	if ( !OwnerCharacter || !OwnerModule || ( !OwnerModule->IsCharging() && !bDriftSparkBurstForced ) )
	{
		DeactivateChargeBoostEffect();
		bWasChargeBoosting = false;
		return;
	}

	// 後退で流されている間は、ドリフトしていなくても小さい火花を出す
	const bool bHitbackSpark = OwnerModule->IsHitbackChargeSparkActive();

	const UTidePlayerParamDataAsset* PlayerParams = OwnerCharacter->PlayerParamData;
	if ( !PlayerParams ) return;

	const float CurrentMultiplier = OwnerModule->GetDynamicChargeMultiplier();
	const float BonusValue = CurrentMultiplier - 1.5f;

	float CurrentSpeed = 0.0f;
	if ( UCharacterMovementComponent* Movement = OwnerCharacter->GetCharacterMovement() )
	{
		CurrentSpeed = Movement->Velocity.Size2D();
	}

	const float MinSpeedForBoostEffect = PlayerParams->MinSpeedForBoostEffect;

	const bool bIsBoostingNow = ( BonusValue > 0.0f &&
		!OwnerModule->IsEffectivelyInAir() &&
		CurrentSpeed >= MinSpeedForBoostEffect );

	// --- 開始エフェクト（CHARGE_BOOST_ST）---
	if ( bIsBoostingNow && !bWasChargeBoosting )
	{
		if ( OwnerCharacter->NiagaraSystemDataAsset )
		{
			if ( UNiagaraSystem* EffectSys = OwnerCharacter->NiagaraSystemDataAsset->GetNiagaraSystem( PlayerNiagaraTags::CHARGE_BOOST_ST ) )
			{
				// X のマイナスで「後ろ」、Z のマイナスで「足元」
				constexpr float ChargeBoostStBackwardOffset = 80.0f;
				const FVector Offset = FVector(
					-ChargeBoostStBackwardOffset,
					0.0f,
					-OwnerCharacter->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()
				);

				UNiagaraComponent* StEffect = UNiagaraFunctionLibrary::SpawnSystemAttached(
					EffectSys,
					OwnerCharacter->GetRootComponent(),
					NAME_None,
					Offset,
					FRotator::ZeroRotator,
					EAttachLocation::KeepRelativeOffset,
					true
				);

				if ( StEffect )
				{
					StEffect->SetAbsolute( false, true, false );	// 親ボーンの回転に引きずられないように
				}
			}
		}
	}
	bWasChargeBoosting = bIsBoostingNow;

	// --- ループエフェクト（CHARGE_BOOST）---
	// On/Off はモジュール側の回転ブースト判定を単一ソースとして参照し、ゲーム側ボーナスと完全に一致させる
	const bool bIsDrifting = OwnerModule->IsDriftBoostActive();
	const float DriftAlpha = OwnerModule->GetDriftBoostAlpha();

	if ( bIsDrifting || bDriftSparkBurstForced || bHitbackSpark )
	{
		ActivateChargeBoostEffect();

		if ( ActiveChargeBoostEffect )
		{
			const float MinVelocityRate = PlayerParams->ChargeBoostMinVelocityRate;
			const float MaxVelocityRate = PlayerParams->ChargeBoostMaxVelocityRate;

			// VelocityRate が火花の実サイズ。通常は小さく／ギアアップ直後だけ満額にする。強制バーストは
			// ドリフト強度が無いため満額扱いで揃え、ヒットバック中の小火花は専用サイズで出す
			float SizeAlpha = DriftAlpha;
			if ( bDriftSparkBurstForced )				SizeAlpha = 1.0f;
			else if ( bHitbackSpark && !bIsDrifting )	SizeAlpha = PlayerParams->HitbackChargeSparkSizeRate;

			const float VelocityRate = FMath::Lerp( MinVelocityRate, MaxVelocityRate, SizeAlpha ) * GetDriftSparkScale();
			ActiveChargeBoostEffect->SetFloatParameter( TEXT( "VelocityRate" ), VelocityRate );

			// 進行方向を基準に横へ倒すワールド回転を組む
			const FVector2D RawInput = OwnerCharacter->GetRawMovementInput();
			const FRotator ControlRot = OwnerCharacter->GetControlRotation();
			const FRotator YawRot( 0.0f, ControlRot.Yaw, 0.0f );
			const FVector InputWorldDir = ( YawRot.Vector() * RawInput.Y + FRotationMatrix( YawRot ).GetUnitAxis( EAxis::Y ) * RawInput.X ).GetSafeNormal2D();

			UCharacterMovementComponent* Movement = OwnerCharacter->GetCharacterMovement();
			if ( Movement )
			{
				const FVector CurrentDir = Movement->Velocity.GetSafeNormal2D();

				const float TurnSign = FVector::CrossProduct( CurrentDir, InputWorldDir ).Z;
				const float TurnDirection = ( TurnSign < 0.0f ) ? -1.0f : 1.0f;

				// DriftAlpha（速度×回転）で Base〜Max を補間するので、速いほど深く傾き、
				// ドリフトしていない強制バースト（ギアアップ・突風）でも Base ぶんは寝る
				const float LeanAngle = FMath::Lerp( PlayerParams->ChargeBoostBaseLeanAngle, PlayerParams->ChargeBoostMaxLeanAngle, DriftAlpha );
				const float LeanAngleRad = FMath::DegreesToRadians( TurnDirection * LeanAngle );

				const FQuat BaseFacingQuat = CurrentDir.ToOrientationQuat();		// 進行方向を向いて真上に立つ
				const FQuat LeanRollQuat( FVector::ForwardVector, LeanAngleRad );	// 横に倒す

				// 前後へ寝かせる静的オフセット。左右の別が無いので旋回方向では反転させない
				const FQuat LeanPitchQuat( FVector::RightVector,
					FMath::DegreesToRadians( PlayerParams->ChargeBoostLeanPitchOffset ) );

				// Pitch→Roll の順に重ねると Roll は Pitch 適用後の火花自身の長軸まわりになる
				const FQuat FinalWorldQuat = BaseFacingQuat * LeanPitchQuat * LeanRollQuat;
				ActiveChargeBoostEffect->SetWorldRotation( FinalWorldQuat );
			}
		}
	}
	else
	{
		DeactivateChargeBoostEffect();
	}
}

void UChargeEffectSubModule::ActivateChargeBoostEffect()
{
	if ( ActiveChargeBoostEffect )
	{
		ActiveChargeBoostEffect->Activate();
		return;
	}

	if ( !OwnerCharacter || !OwnerCharacter->NiagaraSystemDataAsset ) return;

	const float CapsuleHalfHeight = OwnerCharacter->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	FVector Offset = FVector( 0.0, 0.0f, -CapsuleHalfHeight );

	if ( UNiagaraSystem* EffectSys = OwnerCharacter->NiagaraSystemDataAsset->GetNiagaraSystem( PlayerNiagaraTags::CHARGE_BOOST ) )
	{
		ActiveChargeBoostEffect = UNiagaraFunctionLibrary::SpawnSystemAttached(
			EffectSys,
			OwnerCharacter->GetMesh(),
			TEXT( "foot_r" ),
			FVector::ZeroVector,
			FRotator::ZeroRotator,
			EAttachLocation::SnapToTarget,
			false
		);

		ActiveChargeBoostEffect->SetAbsolute( false, true, false );
	}
}

void UChargeEffectSubModule::DeactivateChargeBoostEffect()
{
	if ( ActiveChargeBoostEffect )
	{
		ActiveChargeBoostEffect->Deactivate();
	}
}

void UChargeEffectSubModule::StopChargeBoostEffect()
{
	DeactivateChargeBoostEffect();
	bWasChargeBoosting = false;
	DriftSparkBurstTimer.Clear();
	bDriftSparkBurstForced = false;
}

void UChargeEffectSubModule::NotifyDriftSparkGearUpBurst()
{
	const UTidePlayerParamDataAsset* PlayerParams = OwnerCharacter ? OwnerCharacter->PlayerParamData : nullptr;
	if ( !PlayerParams || !PlayerParams->bEnableDriftSparkGearUpBurst ) return;

	const float HoldTime = FMath::Max( PlayerParams->DriftSparkGearUpHoldTime, 0.0f );
	const float BlendOutTime = FMath::Max( PlayerParams->DriftSparkGearUpBlendOutTime, 0.0f );
	DriftSparkBurstTimer.Set( HoldTime + BlendOutTime );
}

void UChargeEffectSubModule::NotifyDriftSparkForcedBurst()
{
	const UTidePlayerParamDataAsset* PlayerParams = OwnerCharacter ? OwnerCharacter->PlayerParamData : nullptr;
	if ( !PlayerParams || !PlayerParams->bEnableDriftSparkGearUpBurst ) return;

	NotifyDriftSparkGearUpBurst();
	bDriftSparkBurstForced = !DriftSparkBurstTimer.IsFinish();
}

float UChargeEffectSubModule::GetDriftSparkScale() const
{
	const UTidePlayerParamDataAsset* PlayerParams = OwnerCharacter ? OwnerCharacter->PlayerParamData : nullptr;
	if ( !PlayerParams || !PlayerParams->bEnableDriftSparkGearUpBurst ) return 1.0f;

	const float NormalScale = PlayerParams->DriftSparkNormalScale;
	if ( DriftSparkBurstTimer.IsFinish() ) return NormalScale;

	// 残りが BlendOutTime を切るまでは満額を保ち、そこから通常サイズへ落とす
	const float BlendOutTime = FMath::Max( PlayerParams->DriftSparkGearUpBlendOutTime, 0.0f );
	if ( BlendOutTime <= 0.0f ) return PlayerParams->DriftSparkGearUpScale;

	const float BlendAlpha = FMath::Clamp( DriftSparkBurstTimer.Get() / BlendOutTime, 0.0f, 1.0f );
	return FMath::Lerp( NormalScale, PlayerParams->DriftSparkGearUpScale, BlendAlpha );
}

// --- ギア表示 UI（漢字ギアのポップ表示）---

float UChargeEffectSubModule::GetGearUIPopDuration() const
{
	const UTidePlayerParamDataAsset* PlayerParams = OwnerCharacter ? OwnerCharacter->PlayerParamData : nullptr;
	if ( !PlayerParams || PlayerParams->ChargeV2ShiftHoldTime <= 0.0f ) return UI_PopDuration;

	// In＋Hold より短いシフト間隔でもフェードアウトの尺は最低限残す
	return FMath::Max( PlayerParams->ChargeV2ShiftHoldTime, UI_PopInDuration + UI_PopHoldDuration + UI_PopMinOutDuration );
}

void UChargeEffectSubModule::NotifyChargeGearUIPop()
{
	GearUIPopTimer.Set( GetGearUIPopDuration() );
}

#if !UE_BUILD_SHIPPING
void UChargeEffectSubModule::DrawChargeGaugeUI()
{
	if ( !OwnerCharacter || !OwnerModule ) return;

	// 0.3 秒で消えるポップ演出なので、他の HUD と違い out アニメは挟まず即座に消す
	if ( TideHudAnim::IsHudSuppressed( OwnerCharacter->GetWorld() ) ) return;

	const UTidePlayerParamDataAsset* PlayerParams = OwnerCharacter->PlayerParamData;
	if ( !PlayerParams ) return;

	APlayerController* PC = Cast<APlayerController>( OwnerCharacter->GetController() );
	if ( !PC || !PC->GetWorld() ) return;

	const float DeltaTime = PC->GetWorld()->GetDeltaSeconds();
	GearUIPopTimer.Update( DeltaTime );

	if ( GearUIPopTimer.IsFinish() ) return;

	FVector2D GameViewportSize = FVector2D( 1920.0, 1080.0 );
	if ( GEngine && GEngine->GameViewport )
	{
		GEngine->GameViewport->GetViewportSize( GameViewportSize );
	}

	constexpr float BaseResolutionY = 1080.0f;
	constexpr float MinScaleFactor = 0.1f;
	const float     ScaleFactor = FMath::Max( GameViewportSize.Y / BaseResolutionY, MinScaleFactor );

	// 総尺はギアシフト間隔。In／Hold は固定で差分はフェードアウトが吸収する
	// ＝次のギアがポップする瞬間に前のギアが消え切る
	constexpr float InAnimDuration = UI_PopInDuration;
	constexpr float HoldDuration = UI_PopHoldDuration;
	const float     OutAnimDuration = FMath::Max( GetGearUIPopDuration() - InAnimDuration - HoldDuration, UI_PopMinOutDuration );

	const float ElapsedTime = GearUIPopTimer.GetElapsed();

	float ScaleProgress = 0.0f;
	float AlphaProgress = 0.0f;
	float TargetAlpha = 1.0f;
	float EasedScale = 1.0f;

	if ( ElapsedTime < InAnimDuration )
	{
		// In：小さい状態から膨らみ、等倍へ縮む「ポップアップ」
		ScaleProgress = ElapsedTime / InAnimDuration;
		TargetAlpha = UKismetMathLibrary::Ease( 0.0f, 1.0f, ScaleProgress, EEasingFunc::EaseOut );

		constexpr float StartScale = 0.0f;
		constexpr float PeakScale = 1.5f;
		constexpr float EndScale = 1.0f;
		constexpr float PeakTiming = 0.6f;	// In 時間の 60% で最大になる

		if ( ScaleProgress < PeakTiming )
		{
			const float GrowProgress = ScaleProgress / PeakTiming;
			EasedScale = UKismetMathLibrary::Ease( StartScale, PeakScale, GrowProgress, EEasingFunc::ExpoOut );
		}
		else
		{
			const float ShrinkProgress = ( ScaleProgress - PeakTiming ) / ( 1.0f - PeakTiming );
			EasedScale = UKismetMathLibrary::Ease( PeakScale, EndScale, ShrinkProgress, EEasingFunc::EaseOut );
		}
	}
	else if ( ElapsedTime < ( InAnimDuration + HoldDuration ) )
	{
		// Hold
		ScaleProgress = 1.0f;
		TargetAlpha = 1.0f;
		EasedScale = 1.0f;
	}
	else
	{
		// Out
		ScaleProgress = 1.0f;
		AlphaProgress = ( ElapsedTime - ( InAnimDuration + HoldDuration ) ) / OutAnimDuration;
		AlphaProgress = FMath::Clamp( AlphaProgress, 0.0f, 1.0f );
		TargetAlpha = UKismetMathLibrary::Ease( 1.0f, 0.0f, AlphaProgress, EEasingFunc::EaseIn );
		EasedScale = 1.0f;
	}

	constexpr float BaseTextScale = 8.0f;
	const float     ScaledTextScale = BaseTextScale * ScaleFactor * EasedScale;

	const float OutlineAlphaFloat = TargetAlpha * TargetAlpha * TargetAlpha;
	const uint8 OutlineAlphaInt = static_cast< uint8 >( OutlineAlphaFloat * 255.0f );

	// キャラクターの現在位置を常に追いかける
	const FVector PlayerLocation = OwnerCharacter->GetActorLocation();
	FVector2D ScreenPos;
	if ( !PC->ProjectWorldLocationToScreen( PlayerLocation, ScreenPos ) ) return;

	ImGui::SetNextWindowPos( ImVec2( ScreenPos.X, ScreenPos.Y ), ImGuiCond_Always, ImVec2( 0.5f, 0.5f ) );
	ImGui::PushStyleColor( ImGuiCol_WindowBg, ImVec4( 0.0f, 0.0f, 0.0f, 0.0f ) );
	ImGui::PushStyleVar( ImGuiStyleVar_WindowBorderSize, 0.0f );
	ImGui::Begin( "Charge Gear Text UI V2", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoInputs );

	ImDrawList* DrawList = ImGui::GetWindowDrawList();
	ImVec2      StartPos = ImGui::GetCursorScreenPos();

	FString KanjiText = TEXT( "壱" );
	ImVec4  BaseColor = TideHudColors::WithAlpha( TideHudColors::Charge::Gear1, TargetAlpha );	// 色は TideHudColors::Charge に集約

	constexpr int32 GearLevel2 = 2;
	constexpr int32 GearLevel3 = 3;
	constexpr int32 GearLevel4 = 4;

	const int32 GearIndex = OwnerModule->GetCurrentChargeGearIndex();
	if ( GearIndex == GearLevel2 )
	{
		KanjiText = TEXT( "弐" );
		BaseColor = TideHudColors::WithAlpha( TideHudColors::Charge::Gear2, TargetAlpha );
	}
	else if ( GearIndex == GearLevel3 )
	{
		KanjiText = TEXT( "参" );
		BaseColor = TideHudColors::WithAlpha( TideHudColors::Charge::Gear3, TargetAlpha );
	}
	else if ( GearIndex >= GearLevel4 )
	{
		KanjiText = TEXT( "極" );
		BaseColor = TideHudColors::WithAlpha( TideHudColors::Charge::Gear4, TargetAlpha );
	}

	const ImU32 TextColor = ImGui::GetColorU32( BaseColor );
	const ImU32 InnerOutlineColor = IM_COL32( 0, 0, 0, OutlineAlphaInt );
	const ImU32 OuterOutlineColor = IM_COL32( 255, 255, 255, static_cast< uint8 >( OutlineAlphaInt * 0.7f ) );

	std::string U8Str = TCHAR_TO_UTF8( *KanjiText );

	ImGui::SetWindowFontScale( ScaledTextScale );
	ImVec2 TextSize = ImGui::CalcTextSize( U8Str.c_str() );

	ImFont* CurrentFont = ImGui::GetFont();
	const float CurrentFontSize = ImGui::GetFontSize();

	constexpr float BaseInnerOutline = 4.0f;
	constexpr float BaseOuterOutline = 8.0f;
	const float     ScaledInnerOutline = BaseInnerOutline * ScaleFactor;
	const float     ScaledOuterOutline = BaseOuterOutline * ScaleFactor;

	const ImVec2 Offsets[8] = {
		ImVec2( -1.0f, 0.0f ), ImVec2( 1.0f, 0.0f ), ImVec2( 0.0f, -1.0f ), ImVec2( 0.0f, 1.0f ),
		ImVec2( -0.7f, -0.7f ), ImVec2( 0.7f, -0.7f ), ImVec2( -0.7f, 0.7f ), ImVec2( 0.7f, 0.7f )
	};

	if ( OutlineAlphaInt > 0 )
	{
		// 外側（白）
		for ( int32 i = 0; i < 8; ++i )
		{
			DrawList->AddText( CurrentFont, CurrentFontSize, ImVec2( StartPos.x + Offsets[i].x * ScaledOuterOutline, StartPos.y + Offsets[i].y * ScaledOuterOutline ), OuterOutlineColor, U8Str.c_str() );
		}
		// 内側（黒）
		for ( int32 i = 0; i < 8; ++i )
		{
			DrawList->AddText( CurrentFont, CurrentFontSize, ImVec2( StartPos.x + Offsets[i].x * ScaledInnerOutline, StartPos.y + Offsets[i].y * ScaledInnerOutline ), InnerOutlineColor, U8Str.c_str() );
		}
	}

	// メイン（一番上）
	DrawList->AddText( CurrentFont, CurrentFontSize, StartPos, TextColor, U8Str.c_str() );

	ImGui::Dummy( ImVec2( TextSize.x, TextSize.y ) );
	ImGui::SetWindowFontScale( 1.0f );

	ImGui::End();
	ImGui::PopStyleVar();
	ImGui::PopStyleColor();
}
#endif
