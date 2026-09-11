// Copyright (c) 2026, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.
#include "ChargeGuardBrakeSubModule.h"

#include "GameFramework/CharacterMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"

#include "PRJ_TIDE_P0/Composites/Player/PlayerModules/ChargeActionPlayerModule_V2.h"
#include "PRJ_TIDE_P0/Actors/Character/Player/TidePlayerCharacter.h"
#include "PRJ_TIDE_P0/Data/Player/TidePlayerParamDataAsset.h"
#include "PRJ_TIDE_P0/Data/Player/PlayerAnimTags.h"

void UChargeGuardBrakeSubModule::Initialize( UChargeActionPlayerModule_V2* InOwnerModule, ATidePlayerCharacter* InOwnerCharacter )
{
	OwnerModule = InOwnerModule;
	OwnerCharacter = InOwnerCharacter;
}

void UChargeGuardBrakeSubModule::Update( float DeltaTime )
{
	if ( !OwnerModule || !OwnerCharacter || !OwnerModule->IsGuardBraking() ) return;

	UpdateMovement( DeltaTime );
	UpdateRotation( DeltaTime );
	UpdateMontageState();
}

bool UChargeGuardBrakeSubModule::TryConsumeBlock()
{
	// ガードブレーキ中でなければ防御しない
	if ( !OwnerModule || !OwnerModule->IsGuardBraking() ) return false;

	// このガードブレーキで既に1回防御済みなら、2回目以降は防御せず通常被弾に任せる
	if ( bBlockConsumed ) return false;

	bBlockConsumed = true;
	return true;
}

void UChargeGuardBrakeSubModule::UpdateMovement( float DeltaTime )
{
	if ( UCharacterMovementComponent* MovementComp = OwnerCharacter->GetCharacterMovement() )
	{
		const UTidePlayerParamDataAsset* PlayerParams = OwnerCharacter->PlayerParamData;
		const float Deceleration = PlayerParams ? PlayerParams->ChargeGuardBrakeDeceleration : 1500.0f;

		if ( !CurrentBrakeVelocity.IsNearlyZero() )
		{
			const float CurrentSpeed = CurrentBrakeVelocity.Size2D();
			const FVector CurrentDir = CurrentBrakeVelocity.GetSafeNormal2D();
			const FVector ForwardDir = OwnerCharacter->GetActorForwardVector().GetSafeNormal2D();
			const float ShiftSpeed = PlayerParams ? PlayerParams->ChargeGuardBrakeDirectionShiftSpeed : 3.0f;
			const FVector NewDir = FMath::VInterpTo( CurrentDir, ForwardDir, DeltaTime, ShiftSpeed ).GetSafeNormal2D();
			CurrentBrakeVelocity = NewDir * CurrentSpeed;
		}

		CurrentBrakeVelocity = FMath::VInterpConstantTo( CurrentBrakeVelocity, FVector::ZeroVector, DeltaTime, Deceleration );
		MovementComp->Velocity.X = CurrentBrakeVelocity.X;
		MovementComp->Velocity.Y = CurrentBrakeVelocity.Y;
	}
}

void UChargeGuardBrakeSubModule::UpdateRotation( float DeltaTime )
{
	if ( OwnerModule->IsFrictionRecoveryFinished() )
	{
		return;
	}

	const FVector2D RawInput = OwnerCharacter->GetRawMovementInput();
	if ( RawInput.IsNearlyZero() )
	{
		return;
	}

	const FRotator ControlRot = OwnerCharacter->GetControlRotation();
	const FRotator YawRot( 0.0f, ControlRot.Yaw, 0.0f );
	const FVector TargetDir = ( YawRot.Vector() * RawInput.Y + FRotationMatrix( YawRot ).GetUnitAxis( EAxis::Y ) * RawInput.X ).GetSafeNormal();
	const float Progress = OwnerModule->GetFrictionRecoveryRate();
	const UTidePlayerParamDataAsset* PlayerParams = OwnerCharacter->PlayerParamData;
	if ( !PlayerParams )
	{
		return;
	}

	const float CurrentTurnSpeed = FMath::Lerp( PlayerParams->ChargeGuardBrakeMaxTurnSpeed, PlayerParams->ChargeGuardBrakeMinTurnSpeed, Progress );
	const FRotator CurrentRot = OwnerCharacter->GetActorRotation();
	const FRotator TargetRot = TargetDir.Rotation();
	const FRotator NewRot = FMath::RInterpTo( CurrentRot, TargetRot, DeltaTime, CurrentTurnSpeed );
	OwnerCharacter->SetActorRotation( FRotator( 0.0f, NewRot.Yaw, 0.0f ) );
}

void UChargeGuardBrakeSubModule::UpdateMontageState()
{
	UAnimMontage* CurrentMontage = OwnerCharacter->GetCurrentMontage();
	UAnimMontage* BrakeSt = OwnerCharacter->GetAnimMontage( PlayerAnimTags::CHARGE_BRAKE_ST );
	UAnimMontage* BrakeLp = OwnerCharacter->GetAnimMontage( PlayerAnimTags::CHARGE_BRAKE_LP );
	UAnimMontage* BrakeEd = OwnerCharacter->GetAnimMontage( PlayerAnimTags::CHARGE_BRAKE_ED );

	if ( bInputHeld )
	{
		if ( HasStartMontageFinished( CurrentMontage, BrakeSt, BrakeLp ) )
		{
			OwnerCharacter->PlayAnimMontage( PlayerAnimTags::CHARGE_BRAKE_LP );
		}
		return;
	}

	if ( CurrentMontage != BrakeEd )
	{
		OwnerModule->EndGuardBrakeAction();
	}
}

bool UChargeGuardBrakeSubModule::HasStartMontageFinished( UAnimMontage* CurrentMontage, UAnimMontage* BrakeSt, UAnimMontage* BrakeLp ) const
{
	if ( UAnimInstance* AnimInst = OwnerCharacter->GetMesh()->GetAnimInstance() )
	{
		if ( CurrentMontage != BrakeSt && CurrentMontage != BrakeLp )
		{
			return true;
		}

		if ( CurrentMontage == BrakeSt )
		{
			const float CurrentPos = AnimInst->Montage_GetPosition( BrakeSt );
			const float Length = BrakeSt->GetPlayLength();
			return CurrentPos >= ( Length - 0.05f );
		}
	}

	return false;
}
