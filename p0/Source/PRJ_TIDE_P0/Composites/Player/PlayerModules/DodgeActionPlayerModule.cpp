// Copyright (c) 2026, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.
#include "DodgeActionPlayerModule.h"

#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraComponent.h"
#include "Components/PoseableMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "TimerManager.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/KismetMathLibrary.h"

#include "PRJ_TIDE_P0/Actors/Character/Player/TidePlayerCharacter.h"
#include "PRJ_TIDE_P0/Data/Player/TidePlayerParamDataAsset.h"
#include "PRJ_TIDE_P0/Data/Player/PlayerAnimTags.h"
#include "PRJ_TIDE_P0/Data/Tags/TideGameplayTags.h"
#include "PRJ_TIDE_P0/Data/Player/PlayerNiagaraTags.h"
#include "PRJ_TIDE_P0/Data/Niagara/NiagaraSystemListDataAsset.h"
#include "PRJ_TIDE_P0/Utilities/HitStopUtils.h"

#if !UE_BUILD_SHIPPING
#include "imgui.h"
#endif

void UDodgeActionPlayerModule::Initialize( ATidePlayerCharacter* InOwner )
{
	Super::Initialize( InOwner );
}

void UDodgeActionPlayerModule::OnModuleUpdate( float DeltaTime )
{
	if ( !OwnerCharacter || !OwnerCharacter->PlayerParamData ) return;

	if ( !CooldownTimer.IsFinish() )
	{
		CooldownTimer.Update( DeltaTime );
	}

	UpdateGravityLock( DeltaTime );
	UpdateGhostTrails( DeltaTime );
	UpdateDodgeSplitPoses( DeltaTime );

	switch ( CurrentState )
	{
	case EDodgeState::Idle:
	{
		if ( !CooldownTimer.IsFinish() ) break;
		if ( OwnerCharacter->HasStateTag( TAG_State_Common_Disable ) ) break;
		if ( OwnerCharacter->HasStateTag( TAG_State_Player_CannotDodge ) ) break;
		// 空中チャージダッシュを使い切った空中は攻撃・ジャンプ・神技のみ
		if ( OwnerCharacter->IsAirActionLimitedAfterAirCharge() ) break;
		// 空中チャージ攻撃中は他アクション禁止（着地 ED のキャンセル窓で解除）
		if ( OwnerCharacter->IsAirChargeAttackActionLocked() ) break;

		if ( TryConsumeCommand( TAG_Input_Command_Dodge, OwnerCharacter->PlayerParamData->DodgeBufferTime ) )
		{
			CurrentDodgeCount = 0;
			const FVector2D CurrentInput = OwnerCharacter->GetRawMovementInput();

			if ( !CurrentInput.IsNearlyZero() )
			{
				OnStartStep( CurrentInput );
			}
			else
			{
				InputWaitTimer.Set( OwnerCharacter->PlayerParamData->DodgeDirectionWaitTime );
				CurrentState = EDodgeState::InputWait;
			}
		}
	} break;
	case EDodgeState::InputWait:
	{
		InputWaitTimer.Update( DeltaTime );
		const FVector2D CurrentInput = OwnerCharacter->GetRawMovementInput();

		if ( !CurrentInput.IsNearlyZero() )
		{
			OnStartStep( CurrentInput );
			break;
		}

		if ( InputWaitTimer.IsFinish() )
		{
			OnStartStep( FVector2D::ZeroVector );
			break;
		}
	} break;
	case EDodgeState::Stepping:
	{
		StepTimer.Update( DeltaTime );

		// 回避中は速度・旋回を自前で完全に制御する
		if ( UCharacterMovementComponent* MovementComp = OwnerCharacter->GetCharacterMovement() )
		{
			// 前回避のときだけ、モデルの向きを入力方向へ回す
			if ( bIsSteerableDodge )
			{
				const FVector2D RawInput = OwnerCharacter->GetRawMovementInput();
				if ( !RawInput.IsNearlyZero() )
				{
					const FVector TargetDir = OwnerCharacter->GetControlRelativeInputDirection( RawInput ).GetSafeNormal();

					const FRotator CurrentRot = OwnerCharacter->GetActorRotation();
					const FRotator TargetRot = TargetDir.Rotation();

					const float DodgeTurnSpeed = OwnerCharacter->PlayerParamData->DodgeTurnSpeed;
					const FRotator NewRot = FMath::RInterpTo( CurrentRot, TargetRot, DeltaTime, DodgeTurnSpeed );

					// Yaw だけを適用する
					OwnerCharacter->SetActorRotation( FRotator( 0.0f, NewRot.Yaw, 0.0f ) );
				}

				// 旋回結果を上書き保存し、落下モーションへ切り替わっても曲がった方向を維持する
				CachedDodgeDirection = OwnerCharacter->GetActorForwardVector();
			}

			const float Progress = StepTimer.GetRate();
			const float MaintainRatio = OwnerCharacter->PlayerParamData ? OwnerCharacter->PlayerParamData->DodgeSpeedMaintainRatio : 0.5f;

			// 維持割合を超えたら、そこからの進行度を 0〜1 へ正規化して急減速させる
			float SpeedMultiplier = 1.0f;
			if ( Progress > MaintainRatio )
			{
				const float LocalProgress = ( Progress - MaintainRatio ) / ( 1.0f - MaintainRatio );
				SpeedMultiplier = UKismetMathLibrary::Ease( 1.0f, 0.0f, LocalProgress, EEasingFunc::EaseOut );
			}

			const float DashTargetSpeed = OwnerCharacter->PlayerParamData->DashWalkSpeed;
			const float DodgeSpeedMultiplier = OwnerCharacter->PlayerParamData->DodgeSpeedMultiplier;
			const float BasePower = DashTargetSpeed * DodgeSpeedMultiplier;

			const float CurrentSpeed = BasePower * SpeedMultiplier;

			// 向きは開始時にキャッシュした方向を使う
			FVector MoveDirection = CachedDodgeDirection;
			FVector NewVelocity = MoveDirection * CurrentSpeed;
			NewVelocity.Z = MovementComp->Velocity.Z;

			MovementComp->Velocity = NewVelocity;
		}

		if ( CurrentDodgeCount < OwnerCharacter->PlayerParamData->MaxConsecutiveDodges )
		{
			if ( TryConsumeCommand( TAG_Input_Command_Dodge, OwnerCharacter->PlayerParamData->DodgeBufferTime ) )
			{
				bNextDodgeReserved = true;
			}
		}

		if ( !OwnerCharacter->HasStateTag( TAG_State_Player_CannotDodge ) &&
			!OwnerCharacter->IsAirActionLimitedAfterAirCharge() &&
			!OwnerCharacter->IsAirChargeAttackActionLocked() )
		{
			// 連続回避
			if ( bNextDodgeReserved )
			{
				bNextDodgeReserved = false;
				const FVector2D CurrentInput = OwnerCharacter->GetRawMovementInput();
				OnStartStep( !CurrentInput.IsNearlyZero() ? CurrentInput : FVector2D::ZeroVector );
			}
		}

		static constexpr float StickInputThresholdSq = 0.01f;
		if ( OwnerCharacter->IsDodgeInputHeld() &&
			OwnerCharacter->GetRawMovementInput().SizeSquared() > StickInputThresholdSq )
		{
			OwnerCharacter->RequestDash( true );
		}

		if ( StepTimer.IsFinish() )
		{
			OnEndStep();
		}
	} break;
	}
}

bool UDodgeActionPlayerModule::IsDodging() const
{
	return CurrentState != EDodgeState::Idle;
}

void UDodgeActionPlayerModule::OnStartStep( const FVector2D& InputVector )
{
	if ( !OwnerCharacter || !OwnerCharacter->PlayerParamData ) return;

	CurrentState = EDodgeState::Stepping;
	CurrentDodgeCount++;

	OwnerCharacter->StopDash();
	OwnerCharacter->CancelCharge( false );

	FVector DashDirection = FVector::ZeroVector;
	if ( const AController* Controller = OwnerCharacter->GetController() )
	{
		const FRotator YawRotation( 0.0f, Controller->GetControlRotation().Yaw, 0.0f );
		const FVector CameraForward = FRotationMatrix( YawRotation ).GetUnitAxis( EAxis::X );
		const FVector CameraRight = FRotationMatrix( YawRotation ).GetUnitAxis( EAxis::Y );

		DashDirection = ( CameraForward * InputVector.Y ) + ( CameraRight * InputVector.X );

		if ( DashDirection.IsNearlyZero() )
		{
			DashDirection = -OwnerCharacter->GetActorForwardVector();
		}
		else
		{
			DashDirection.Normalize();
		}
	}

	CachedDodgeDirection = DashDirection;

	FName StepAnimTag = PlayerAnimTags::STEP_B;

	if ( !InputVector.IsNearlyZero() )
	{
		const FVector ActorForward = OwnerCharacter->GetActorForwardVector();
		const FVector ActorRight = OwnerCharacter->GetActorRightVector();

		const float LocalForward = FVector::DotProduct( DashDirection, ActorForward );
		const float LocalRight = FVector::DotProduct( DashDirection, ActorRight );

		if ( FMath::Abs( LocalForward ) > FMath::Abs( LocalRight ) )
		{
			if ( LocalForward > 0.0f )
			{
				StepAnimTag = PlayerAnimTags::STEP_F;
			}
			else
			{
				StepAnimTag = PlayerAnimTags::STEP_B;
			}
		}
		else
		{
			if ( LocalRight > 0.0f )
			{
				StepAnimTag = PlayerAnimTags::STEP_R;
			}
			else
			{
				StepAnimTag = PlayerAnimTags::STEP_L;
			}
		}
	}

	bIsSteerableDodge = ( StepAnimTag == PlayerAnimTags::STEP_F );

	// 空中は一定時間だけ水平維持する
	if ( OwnerCharacter->IsFalling() )
	{
		GravityLockTimer.Set( OwnerCharacter->PlayerParamData->DodgeGravityLockTime );

		if ( UCharacterMovementComponent* MovementComp = OwnerCharacter->GetCharacterMovement() )
		{
			MovementComp->Velocity.Z = 0.0f;
		}
	}

	OwnerCharacter->RefreshMovementParams();

	const float AnimDuration = PlayAnimMontage( StepAnimTag );
	if ( AnimDuration > 0.0f )
	{
		StepTimer.Set( AnimDuration );
	}

	if ( SpawnedDodgeEffect )
	{
		SpawnedDodgeEffect->Deactivate();
		SpawnedDodgeEffect->DestroyComponent();
		SpawnedDodgeEffect = nullptr;
	}

	if ( UNiagaraSystem* EffectSys = OwnerCharacter->NiagaraSystemDataAsset->GetNiagaraSystem( PlayerNiagaraTags::CHARGED_DASH ) )
	{
		SpawnedDodgeEffect = UNiagaraFunctionLibrary::SpawnSystemAttached(
			EffectSys,
			OwnerCharacter->GetRootComponent(),
			NAME_None,
			FVector::ZeroVector,
			FRotator::ZeroRotator,
			EAttachLocation::SnapToTarget,
			true
		);
		FLinearColor NewColor( 0.0f, 0.0f, 1.0f, 1.0f );
		SpawnedDodgeEffect->SetColorParameter( "Color", NewColor );
	}
}

void UDodgeActionPlayerModule::OnEndStep()
{
	// 連続回避を行った場合のみクールダウンへ
	if ( CurrentDodgeCount > 1 )
	{
		StartCooldown();
	}
	ClearState();
}

void UDodgeActionPlayerModule::ClearState()
{
	CurrentState = EDodgeState::Idle;
	StepTimer.Clear();
	GravityLockTimer.Clear();

	if ( SpawnedDodgeEffect )
	{
		SpawnedDodgeEffect->Deactivate();
		SpawnedDodgeEffect->DestroyComponent();
		SpawnedDodgeEffect = nullptr;
	}

	if ( OwnerCharacter )
	{
		if ( OwnerCharacter->IsDashing() ) return;	// ダッシュへ移行済みなら後始末は Dash 側に任せる

		OwnerCharacter->RefreshMovementParams();

		UAnimMontage* CurrentMontage = OwnerCharacter->GetCurrentMontage();
		if ( CurrentMontage )
		{
			UAnimMontage* StepF = GetAnimMontage( PlayerAnimTags::STEP_F );
			UAnimMontage* StepB = GetAnimMontage( PlayerAnimTags::STEP_B );
			UAnimMontage* StepL = GetAnimMontage( PlayerAnimTags::STEP_L );
			UAnimMontage* StepR = GetAnimMontage( PlayerAnimTags::STEP_R );

			if ( CurrentMontage == StepF || CurrentMontage == StepB ||
				CurrentMontage == StepL || CurrentMontage == StepR )
			{
				OwnerCharacter->StopAnimMontage( CurrentMontage );
			}
		}
	}
}

void UDodgeActionPlayerModule::StartCooldown()
{
	if ( !OwnerCharacter || !OwnerCharacter->PlayerParamData ) return;
	CooldownTimer.Set( OwnerCharacter->PlayerParamData->DodgeCooldownTime );
}

void UDodgeActionPlayerModule::CancelDodge()
{
	if ( CurrentState != EDodgeState::Idle )
	{
		ClearState();
		StartCooldown();
	}
}

void UDodgeActionPlayerModule::OnDodgeSuccess()
{
	if ( !OwnerCharacter || !OwnerCharacter->PlayerParamData ) return;
	auto& PlayerParamData = OwnerCharacter->PlayerParamData;
	UWorld* World = GetWorld();
	if ( !World ) return;

	const float GlobalDilation = PlayerParamData->DodgeSuccessGlobalTimeDilation;
	const float PlayerScale = PlayerParamData->DodgeSuccessPlayerTimeScale;
	const float Duration = PlayerParamData->DodgeSuccessSlowMotionDuration;

	// ワールドを遅くし、プレイヤーだけ等速側へ補正する
	UGameplayStatics::SetGlobalTimeDilation( World, GlobalDilation );
	if ( GlobalDilation > 0.0f )
	{
		OwnerCharacter->SetCustomTimeDilation( PlayerScale / GlobalDilation );
	}

	// 現実時間に合わせてタイマーの尺を圧縮する
	const float DilatedDuration = Duration * GlobalDilation;

	World->GetTimerManager().SetTimer(
		GlobalTimeDilationResetTimer,
		this,
		&UDodgeActionPlayerModule::OnEndDodgeSuccess,
		DilatedDuration,
		false
	);

	// 回避成立が分かるよう、左右へ分身ポーズを出す
	SpawnDodgeSplitPoses();
}

void UDodgeActionPlayerModule::ForceDodge()
{
	if ( !OwnerCharacter || !OwnerCharacter->PlayerParamData ) return;

	// クールダウン・入力待ちは経由せず即ステップ開始する
	CurrentDodgeCount = 0;
	bNextDodgeReserved = false;

	const FVector2D CurrentInput = OwnerCharacter->GetRawMovementInput();
	OnStartStep( CurrentInput );
}

bool UDodgeActionPlayerModule::HasDodgeInputBuffered() const
{
	if ( !OwnerCharacter || !OwnerCharacter->PlayerParamData ) return false;

	if ( bNextDodgeReserved ) return true;
	if( HasCommand( TAG_Input_Command_Dodge, OwnerCharacter->PlayerParamData->DodgeBufferTime ) ) return true;

	return false;
}

void UDodgeActionPlayerModule::OnEndDodgeSuccess()
{
	ResetGlobalTimeDilation();
}

void UDodgeActionPlayerModule::ResetGlobalTimeDilation()
{
	UWorld* World = GetWorld();
	if ( World )
	{
		UGameplayStatics::SetGlobalTimeDilation( World, 1.0f );
		World->GetTimerManager().ClearTimer( GlobalTimeDilationResetTimer );
	}

	if ( OwnerCharacter )
	{
		// スロー中（高速 CustomTimeDilation）に入ったヒットストップは復帰先にその高速値を記録するため、
		// 先に解除してから 1.0 を設定して確実に等速へ戻す
		HitStopUtil::CancelHitStop( OwnerCharacter );
		OwnerCharacter->SetCustomTimeDilation( 1.0f );
	}
}

void UDodgeActionPlayerModule::UpdateGravityLock( float DeltaTime )
{
	if ( GravityLockTimer.IsFinish() ) return;

	GravityLockTimer.Update( DeltaTime );

	if ( UCharacterMovementComponent* MovementComp = OwnerCharacter->GetCharacterMovement() )
	{
		MovementComp->Velocity.Z = 0.0f;
		MovementComp->GravityScale = 0.0f;
	}

	if ( GravityLockTimer.IsFinish() )
	{
		if ( OwnerCharacter )
		{
			OwnerCharacter->RefreshMovementParams();

			if ( OwnerCharacter->IsFalling() )
			{
				PlayAnimMontage( PlayerAnimTags::JUMP_LP );
				OwnerCharacter->ReserveAutoDashOnLanding();
			}
		}
	}
}

void UDodgeActionPlayerModule::StartGhostTrailSpawing( float Duration )
{
	RemainingGhostTrailDuration = Duration;
	SpawnGhostTrail();	// 初回分

	if ( OwnerCharacter && OwnerCharacter->PlayerParamData )
	{
		GhostTrailSpawnTimer.Set( OwnerCharacter->PlayerParamData->GhostTrailSpawnInterval );
	}
}

void UDodgeActionPlayerModule::UpdateGhostTrails( float DeltaTime )
{
	if ( !OwnerCharacter || !OwnerCharacter->PlayerParamData ) return;

	if ( RemainingGhostTrailDuration > 0.0f )
	{
		RemainingGhostTrailDuration -= DeltaTime;
		GhostTrailSpawnTimer.Update( DeltaTime );

		if ( GhostTrailSpawnTimer.IsFinish() )
		{
			SpawnGhostTrail();
			GhostTrailSpawnTimer.Set( OwnerCharacter->PlayerParamData->GhostTrailSpawnInterval );
		}
	}

	// 末尾からループして安全に削除する
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

		// フェードアウト（1.0 → 0.0）
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

UPoseableMeshComponent* UDodgeActionPlayerModule::BuildGhostPoseMesh( UMaterialInterface* Material, TArray<TObjectPtr<UMaterialInstanceDynamic>>& OutMIDs )
{
	if ( !OwnerCharacter || !Material ) return nullptr;

	USkeletalMeshComponent* CharacterMesh = OwnerCharacter->GetMesh();
	if ( !CharacterMesh ) return nullptr;

	UPoseableMeshComponent* GhostMesh = NewObject<UPoseableMeshComponent>( OwnerCharacter );
	if ( !GhostMesh ) return nullptr;

	GhostMesh->RegisterComponent();
	GhostMesh->SetWorldLocationAndRotation( CharacterMesh->GetComponentLocation(), CharacterMesh->GetComponentRotation() );
	GhostMesh->SetSkinnedAssetAndUpdate( CharacterMesh->GetSkeletalMeshAsset() );
	GhostMesh->CopyPoseFromSkeletalComponent( CharacterMesh );
	GhostMesh->SetCollisionEnabled( ECollisionEnabled::NoCollision );
	GhostMesh->CastShadow = false;

	// フェード制御のため MID へ差し替えて保持する
	const int32 NumMaterials = GhostMesh->GetNumMaterials();
	for ( int32 i = 0; i < NumMaterials; ++i )
	{
		UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create( Material, GhostMesh );
		if ( MID )
		{
			MID->SetScalarParameterValue( FName( "FadeAmount" ), 1.0f );
			GhostMesh->SetMaterial( i, MID );
			OutMIDs.Add( MID );
		}
	}

	return GhostMesh;
}

void UDodgeActionPlayerModule::SpawnGhostTrail()
{
	if ( !OwnerCharacter || !OwnerCharacter->PlayerParamData ) return;

	FGhostTrailData NewTrailData;
	NewTrailData.MeshComponent = BuildGhostPoseMesh( OwnerCharacter->PlayerParamData->DodgeGhostTrailMaterial, NewTrailData.MIDs );
	if ( !NewTrailData.MeshComponent ) return;

	NewTrailData.Lifespan = OwnerCharacter->PlayerParamData->GhostTrailLifespan;
	ActiveGhostTrails.Add( NewTrailData );
}

void UDodgeActionPlayerModule::SpawnDodgeSplitPoses()
{
	if ( !OwnerCharacter || !OwnerCharacter->PlayerParamData ) return;
	auto& Params = OwnerCharacter->PlayerParamData;

	// マテリアル未設定なら分身演出そのものを出さない
	UMaterialInterface* Material = Params->DodgeSuccessPoseMaterial;
	if ( !Material ) return;

	SpawnDodgeSplitWave( Material, Params->DodgeSuccessPoseLateralOffset, Params->DodgeSuccessPoseDuration );

	// 遅延ぶんだけ待って同じ演出をもう 1 組
	bSecondWavePending = true;
	SecondWaveDelayRemaining = Params->DodgeSuccessSecondPoseDelay;
}

void UDodgeActionPlayerModule::SpawnDodgeSplitWave( UMaterialInterface* Material, float MaxOffset, float Duration )
{
	if ( !OwnerCharacter || !Material ) return;

	for ( int32 Side = 0; Side < 2; ++Side )	// 左右で 2 体
	{
		FDodgeSplitPose Pose;
		Pose.MeshComponent = BuildGhostPoseMesh( Material, Pose.MIDs );
		if ( !Pose.MeshComponent ) continue;

		Pose.LateralSign = ( Side == 0 ) ? 1.0f : -1.0f;
		Pose.Duration = Duration;
		Pose.MaxOffset = MaxOffset;
		ActiveDodgeSplitPoses.Add( Pose );
	}
}

void UDodgeActionPlayerModule::UpdateDodgeSplitPoses( float DeltaTime )
{
	if ( bSecondWavePending )
	{
		SecondWaveDelayRemaining -= DeltaTime;
		if ( SecondWaveDelayRemaining <= 0.0f )
		{
			bSecondWavePending = false;
			if ( OwnerCharacter && OwnerCharacter->PlayerParamData )
			{
				auto& Params = OwnerCharacter->PlayerParamData;
				SpawnDodgeSplitWave( Params->DodgeSuccessPoseMaterial, Params->DodgeSuccessSecondPoseLateralOffset, Params->DodgeSuccessSecondPoseDuration );
			}
		}
	}

	if ( ActiveDodgeSplitPoses.Num() == 0 ) return;

	// プレイヤーに追従させるため基準は毎フレームの現在位置・向き
	FVector BaseLocation = FVector::ZeroVector;
	FVector RightDir = FVector::ForwardVector;
	if ( const USkeletalMeshComponent* CharacterMesh = OwnerCharacter ? OwnerCharacter->GetMesh() : nullptr )
	{
		BaseLocation = CharacterMesh->GetComponentLocation();
		RightDir = OwnerCharacter->GetActorRightVector();
	}

	for ( int32 i = ActiveDodgeSplitPoses.Num() - 1; i >= 0; --i )
	{
		FDodgeSplitPose& Pose = ActiveDodgeSplitPoses[i];
		Pose.ElapsedTime += DeltaTime;

		if ( Pose.ElapsedTime >= Pose.Duration || !IsValid( Pose.MeshComponent ) )
		{
			if ( IsValid( Pose.MeshComponent ) ) Pose.MeshComponent->DestroyComponent();
			ActiveDodgeSplitPoses.RemoveAt( i );
			continue;
		}

		// sinのカーブで「横に広がって（中間で最大）プレイヤー位置へ戻る」。フェードも同じ山形で出入り
		const float T = Pose.Duration > 0.0f ? ( Pose.ElapsedTime / Pose.Duration ) : 1.0f;
		const float Shape = FMath::Sin( PI * T );

		Pose.MeshComponent->SetWorldLocation( BaseLocation + RightDir * ( Pose.LateralSign * Pose.MaxOffset * Shape ) );

		for ( UMaterialInstanceDynamic* MID : Pose.MIDs )
		{
			if ( IsValid( MID ) ) MID->SetScalarParameterValue( FName( "FadeAmount" ), Shape );
		}
	}
}

#if !UE_BUILD_SHIPPING
void UDodgeActionPlayerModule::DrawDebugImGui()
{
	if ( !ImGui::CollapsingHeader( "Dodge Module", ImGuiTreeNodeFlags_DefaultOpen ) ) return;

	ImGui::Indent();
	const char* StateName = ( CurrentState == EDodgeState::Idle ) ? "Idle" : ( CurrentState == EDodgeState::InputWait ) ? "InputWait" : "Stepping";
	ImGui::Text( "State: %s", StateName );
	ImGui::Separator();

	if ( OwnerCharacter && OwnerCharacter->PlayerParamData )
	{
		ImGui::Text( "Dodge Count: %d / %d", CurrentDodgeCount, OwnerCharacter->PlayerParamData->MaxConsecutiveDodges );
	}
	ImGui::TextColored( bNextDodgeReserved ? ImVec4( 1, 1, 0, 1 ) : ImVec4( 1, 1, 1, 1 ), "Next Reserved: %s", bNextDodgeReserved ? "True" : "False" );

	ImGui::Separator();
	if ( !CooldownTimer.IsFinish() ) ImGui::TextColored( ImVec4( 1, 0, 0, 1 ), "Cooldown: %.2f", CooldownTimer.GetElapsed() );
	if ( CurrentState == EDodgeState::InputWait ) ImGui::Text( "InputWait Timer: %.2f", InputWaitTimer.GetElapsed() );
	if ( CurrentState == EDodgeState::Stepping ) ImGui::Text( "Step Timer: %.2f", StepTimer.GetElapsed() );

	ImGui::Unindent();
}
#endif
