// Copyright (c) 2026, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.
#include "FallActionPlayerModule.h"

#include <imgui.h>
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/CapsuleComponent.h"

#include "PRJ_TIDE_P0/Actors/Character/Player/TidePlayerCharacter.h"
#include "PRJ_TIDE_P0/Data/Player/TidePlayerParamDataAsset.h"
#include "PRJ_TIDE_P0/Components/Combat/StatusComponent.h"
#include "PRJ_TIDE_P0/Utilities/HitStopUtils.h"
#include "PRJ_TIDE_P0/Data/Player/PlayerAnimTags.h"

namespace
{
	// これを持つアクター／コンポーネントに着地したら、落下ダメージ・落下死・着地よろけを無効化する
	const FName NoFallDamageLandingTag( TEXT( "NoFallDamage" ) );

	bool HasNoFallDamageTag( const AActor* Actor, const UPrimitiveComponent* Component )
	{
		if ( Component && Component->ComponentHasTag( NoFallDamageLandingTag ) ) return true;
		if ( Actor && Actor->ActorHasTag( NoFallDamageLandingTag ) ) return true;
		return false;
	}
}

void UFallActionPlayerModule::Initialize( ATidePlayerCharacter* InOwner )
{
	Super::Initialize( InOwner );

	bIsTrackingFall = false;
	MaxFallZ = 0.0f;

	FallDescentTime = 0.0f;
	bGravityRampApplied = false;
}

void UFallActionPlayerModule::OnModuleUpdate( float DeltaTime )
{
	if ( !OwnerCharacter ) return;

	class UCharacterMovementComponent* MovementComp = OwnerCharacter->GetCharacterMovement();
	if ( !MovementComp ) return;

	UpdateFallGravityRamp( DeltaTime );	// 落下ダメージ判定とは独立して毎フレーム回す

	if ( MovementComp->IsFalling() )
	{
		if ( !bIsTrackingFall )
		{
			StartFallTracking();
		}
		else
		{
			// 空中でジャンプしてさらに高くなった場合に対応するため、最高 Z を更新し続ける
			const float CurrentZ = OwnerCharacter->GetActorLocation().Z;
			if ( CurrentZ > MaxFallZ )
			{
				MaxFallZ = CurrentZ;
			}
		}
	}
	else
	{
		// OnLanded が呼ばれない経路のフォールバック
		if ( bIsTrackingFall )
		{
			ProcessFallDamage();
			StopFallTracking();
		}
	}
}

void UFallActionPlayerModule::OnLanded( const FHitResult& Hit )
{
	if ( !bIsTrackingFall ) return;

	const AActor* LandedActor = Hit.GetActor();
	const UPrimitiveComponent* LandedComponent = Hit.GetComponent();

	// まず実際に足元をブロックした Hit で判定する
	bool bIsSoftLanding = HasNoFallDamageTag( LandedActor, LandedComponent );
	FString SoftLandingSourceName = bIsSoftLanding && LandedActor ? LandedActor->GetName() : FString();

	// Hit で判定できなければ Overlap 中の全アクターも見る。柔らかい着地面が Overlap 専用で
	// 実体の地面は別（例：草むらの中だが地面は Landscape）という構成に対応するため
	if ( !bIsSoftLanding && OwnerCharacter )
	{
		if ( UCapsuleComponent* Capsule = OwnerCharacter->GetCapsuleComponent() )
		{
			TArray<UPrimitiveComponent*> OverlappingComponents;
			Capsule->GetOverlappingComponents( OverlappingComponents );

			for ( UPrimitiveComponent* OverlappingComponent : OverlappingComponents )
			{
				if ( !OverlappingComponent ) continue;

				AActor* OverlappingActor = OverlappingComponent->GetOwner();
				if ( HasNoFallDamageTag( OverlappingActor, OverlappingComponent ) )
				{
					bIsSoftLanding = true;
					SoftLandingSourceName = OverlappingActor ? OverlappingActor->GetName() : TEXT( "Unknown" );
					break;
				}
			}
		}
	}

	// ImGui のデバッグパネル表示用
	LastLandedActorName = LandedActor ? LandedActor->GetName() : TEXT( "None" );
	LastLandedComponentName = LandedComponent ? LandedComponent->GetName() : TEXT( "None" );
	LastSoftLandingSourceName = SoftLandingSourceName;
	bLastLandingWasSoft = bIsSoftLanding;

	if ( !bIsSoftLanding )
	{
		ProcessFallDamage();
	}

	StopFallTracking();
}

void UFallActionPlayerModule::StartFallTracking()
{
	if ( !OwnerCharacter ) return;

	bIsTrackingFall = true;
	MaxFallZ = OwnerCharacter->GetActorLocation().Z;
}

void UFallActionPlayerModule::StopFallTracking()
{
	bIsTrackingFall = false;
	MaxFallZ = 0.0f;
}

void UFallActionPlayerModule::UpdateFallGravityRamp( float DeltaTime )
{
	if ( !OwnerCharacter ) return;

	UCharacterMovementComponent* MovementComp = OwnerCharacter->GetCharacterMovement();
	if ( !MovementComp ) return;

	const UTidePlayerParamDataAsset* PlayerParams = GetPlayerParams();

	// 機能フラグ OFF・パラメータ無しなら何もしない。直前まで書き換えていた場合のみ本来のパラメータへ戻す
	if ( !PlayerParams || !PlayerParams->bEnableFallGravityRamp )
	{
		if ( bGravityRampApplied )
		{
			OwnerCharacter->RefreshMovementParams();
			bGravityRampApplied = false;
		}
		FallDescentTime = 0.0f;
		return;
	}

	if ( ShouldApplyFallGravityRamp() )
	{
		// 下降時間を積算し、猶予後に倍率 1.0 → MaxScale まで線形に補間する
		FallDescentTime += DeltaTime;

		const float RampAlpha = FMath::Clamp(
			( FallDescentTime - PlayerParams->FallGravityRampStartDelay ) / FMath::Max( 0.01f, PlayerParams->FallGravityRampDuration ),
			0.0f, 1.0f );

		const float ScaleMultiplier = FMath::Lerp( 1.0f, PlayerParams->FallGravityRampMaxScale, RampAlpha );
		MovementComp->GravityScale = PlayerParams->GravityScale * ScaleMultiplier;
		bGravityRampApplied = true;
	}
	else
	{
		// 二段ジャンプの上昇に重い重力を持ち越さないよう、書き換えていた場合だけ本来の値へ戻す
		if ( bGravityRampApplied )
		{
			OwnerCharacter->RefreshMovementParams();
			bGravityRampApplied = false;
		}
		FallDescentTime = 0.0f;
	}
}

bool UFallActionPlayerModule::ShouldApplyFallGravityRamp() const
{
	if ( !OwnerCharacter ) return false;

	const UCharacterMovementComponent* MovementComp = OwnerCharacter->GetCharacterMovement();
	if ( !MovementComp || !MovementComp->IsFalling() ) return false;

	if ( MovementComp->Velocity.Z > 0.0f ) return false;	// 下降中のみ加速させる

	// 重力を自前で制御する特殊アクション中は干渉せず、各アクションのパラメータに任せる
	if ( OwnerCharacter->IsCharging() ||
		OwnerCharacter->IsPlayingChargeAction() ||
		OwnerCharacter->IsDodging() ||
		OwnerCharacter->IsBoostDashing() )
	{
		return false;
	}

	return true;
}

void UFallActionPlayerModule::ProcessFallDamage()
{
	if ( !OwnerCharacter || OwnerCharacter->IsDead() ) return;

	const UTidePlayerParamDataAsset* PlayerParams = GetPlayerParams();

	// 落下ダメージシステムが有効なときのみ落下ダメージ・落下死・着地よろけを行う（PlayerParamsDA・既定OFF）
	if ( !PlayerParams || !PlayerParams->bEnableFallDamageSystem ) return;

	const float DamageFallDist = PlayerParams->DamageFallDistance;
	const float DeathFallDist = PlayerParams->DeathFallDistance;

	const float CurrentZ = OwnerCharacter->GetActorLocation().Z;
	const float FallDistance = MaxFallZ - CurrentZ;

	if ( FallDistance < DamageFallDist ) return;

	// 落下ダメージは ReceiveDamage を経由しないため通常被弾時の CancelAllActions が走らない。空中チャージダッシュ中に
	// 着地すると FALL_DAMAGE モンタージュが ED を横から奪い、チャージ側は終了を観測できずアクション状態とエフェクトが
	// 残留する（以降チャージ不可になる）。ここで先に明示的にキャンセルしておく
	if ( OwnerCharacter->IsCharging() || OwnerCharacter->IsPlayingChargeAction() )
	{
		OwnerCharacter->CancelCharge( false );
	}

	// 検証用：HP には触れず着地よろけだけ行う（ヒットストップは専用パラメータ。Duration 0 でモーションだけ）
	if ( PlayerParams->bFallDamageMotionOnly )
	{
		if ( PlayerParams->FallMotionOnlyHitStopDuration > 0.0f )
		{
			HitStopUtil::ApplyHitStop( OwnerCharacter, PlayerParams->FallMotionOnlyHitStopDuration, PlayerParams->FallMotionOnlyHitStopDilation );
		}
		PlayAnimMontage( PlayerAnimTags::FALL_DAMAGE );
		return;
	}

	UStatusComponent* StatusComp = OwnerCharacter->GetStatusComponent();
	if ( !StatusComp ) return;

	// --- 強制死亡（DeathFallDist 以上）---
	if ( FallDistance >= DeathFallDist )
	{
		const float Duration = PlayerParams->FallDeathHitStopDuration;
		const float Dilation = PlayerParams->FallDeathHitStopDilation;
		HitStopUtil::ApplyHitStop( OwnerCharacter, Duration, Dilation );

		constexpr float DeathFallbackDamage = 100.0f;	// 暫定
		StatusComp->ModifyHP( -DeathFallbackDamage );
		return;
	}

	// --- 距離に応じた可変ダメージ（DamageFallDist 〜 DeathFallDist）---
	const float MinFallDamage = PlayerParams->NormalFallDamage;
	constexpr float MaxFallDamage = 100.0f;

	const float CalculatedDamage = FMath::GetMappedRangeValueClamped(
		FVector2D( DamageFallDist, DeathFallDist ),
		FVector2D( MinFallDamage, MaxFallDamage ),
		FallDistance
	);

	const float Duration = PlayerParams->FallDamageHitStopDuration;
	const float Dilation = PlayerParams->FallDamageHitStopDilation;

	HitStopUtil::ApplyHitStop( OwnerCharacter, Duration, Dilation );
	StatusComp->ModifyHP( -CalculatedDamage );

	if( OwnerCharacter->IsDead() )
	{
		return;
	}

	PlayAnimMontage( PlayerAnimTags::FALL_DAMAGE );
}

const UTidePlayerParamDataAsset* UFallActionPlayerModule::GetPlayerParams() const
{
	return OwnerCharacter ? OwnerCharacter->PlayerParamData : nullptr;
}

void UFallActionPlayerModule::DrawDebugImGui()
{
	if ( !ImGui::CollapsingHeader( "Fall Action Module", ImGuiTreeNodeFlags_DefaultOpen ) ) return;

	ImGui::Indent();

	const UTidePlayerParamDataAsset* PlayerParams = GetPlayerParams();

	if ( bIsTrackingFall )
	{
		ImGui::TextColored( ImVec4( 0.0f, 1.0f, 0.0f, 1.0f ), "Status: TRACKING FALL" );
	}
	else
	{
		ImGui::TextColored( ImVec4( 0.4f, 0.4f, 0.4f, 1.0f ), "Status: GROUNDED / IDLE" );
	}

	if ( OwnerCharacter )
	{
		const float CurrentZ = OwnerCharacter->GetActorLocation().Z;
		const float FallDistance = bIsTrackingFall ? ( MaxFallZ - CurrentZ ) : 0.0f;

		ImGui::Text( "Current Z     : %.2f", CurrentZ );

		if ( bIsTrackingFall )
		{
			ImGui::Text( "Max Fall Z    : %.2f", MaxFallZ );

			const float DamageFallDist = PlayerParams ? PlayerParams->DamageFallDistance : 2000.0f;
			const float DeathFallDist = PlayerParams ? PlayerParams->DeathFallDistance : 3000.0f;

			// 安全:白 → 被ダメージ確定:黄 → 即死圏内:赤
			ImVec4 DistanceColor = ImVec4( 1.0f, 1.0f, 1.0f, 1.0f );
			if ( FallDistance >= DeathFallDist )
			{
				DistanceColor = ImVec4( 1.0f, 0.1f, 0.1f, 1.0f );
			}
			else if ( FallDistance >= DamageFallDist )
			{
				DistanceColor = ImVec4( 1.0f, 1.0f, 0.4f, 1.0f );
			}

			ImGui::TextColored( DistanceColor, "Fall Distance : %.2f", FallDistance );
		}
		else
		{
			// 行の縦幅を維持してガタつきを防ぐ
			ImGui::TextColored( ImVec4( 0.4f, 0.4f, 0.4f, 1.0f ), "Max Fall Z    : ---" );
			ImGui::TextColored( ImVec4( 0.4f, 0.4f, 0.4f, 1.0f ), "Fall Distance : ---" );
		}
	}

	ImGui::Separator();

	// エディタを都度開かずに現在の閾値を確認できるようにする
	if ( PlayerParams )
	{
		ImGui::Text( "Thresholds (DataAsset):" );
		ImGui::Text( "  Damage Fall Distance : %.1f", PlayerParams->DamageFallDistance );
		ImGui::Text( "  Death Fall Distance  : %.1f", PlayerParams->DeathFallDistance );
	}
	else
	{
		ImGui::TextColored( ImVec4( 1.0f, 0.1f, 0.1f, 1.0f ), "DataAsset: NOT FOUND" );
	}

	ImGui::Separator();

	// 落下重力ランプ（下降が続くほど重力を強めて落下を加速させる）
	if ( PlayerParams )
	{
		ImGui::Text( "Fall Gravity Ramp: %s", PlayerParams->bEnableFallGravityRamp ? "Enabled" : "Disabled" );
		if ( PlayerParams->bEnableFallGravityRamp )
		{
			const float RampAlpha = FMath::Clamp(
				( FallDescentTime - PlayerParams->FallGravityRampStartDelay ) / FMath::Max( 0.01f, PlayerParams->FallGravityRampDuration ),
				0.0f, 1.0f );
			const float ScaleMultiplier = FMath::Lerp( 1.0f, PlayerParams->FallGravityRampMaxScale, RampAlpha );

			ImGui::Text( "  Descent Time : %.2f", FallDescentTime );
			ImGui::Text( "  Ramp Alpha   : %.2f", RampAlpha );
			ImGui::Text( "  Gravity Mul  : x%.2f", ScaleMultiplier );

			if ( bGravityRampApplied )
			{
				ImGui::TextColored( ImVec4( 1.0f, 0.6f, 0.2f, 1.0f ), "  -> RAMPING (GravityScale = %.2f)",
					OwnerCharacter && OwnerCharacter->GetCharacterMovement() ? OwnerCharacter->GetCharacterMovement()->GravityScale : 0.0f );
			}
			else
			{
				ImGui::TextColored( ImVec4( 0.4f, 0.4f, 0.4f, 1.0f ), "  -> IDLE" );
			}
		}
	}

	ImGui::Separator();

	// NoFallDamage タグが効いているか実機で確認する
	ImGui::Text( "Last Landing:" );
	ImGui::Text( "  Actor     : %s", LastLandedActorName.IsEmpty() ? "---" : TCHAR_TO_UTF8( *LastLandedActorName ) );
	ImGui::Text( "  Component : %s", LastLandedComponentName.IsEmpty() ? "---" : TCHAR_TO_UTF8( *LastLandedComponentName ) );
	if ( bLastLandingWasSoft )
	{
		ImGui::TextColored( ImVec4( 0.4f, 1.0f, 0.4f, 1.0f ), "  -> Soft Landing (NoFallDamage)" );
		ImGui::TextColored( ImVec4( 0.4f, 1.0f, 0.4f, 1.0f ), "     Source: %s",
			LastSoftLandingSourceName.IsEmpty() ? "---" : TCHAR_TO_UTF8( *LastSoftLandingSourceName ) );
	}
	else
	{
		ImGui::TextColored( ImVec4( 0.7f, 0.7f, 0.7f, 1.0f ), "  -> Normal Landing" );
	}

	ImGui::Unindent();
}
