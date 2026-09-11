// Copyright (c) 2026, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.
#include "DashActionPlayerModule.h"

#include "GameFramework/CharacterMovementComponent.h"
#include "KismetAnimationLibrary.h"

#include "PRJ_TIDE_P0/Actors/Character/Player/TidePlayerCharacter.h"
#include "PRJ_TIDE_P0/Data/Player/TidePlayerParamDataAsset.h"
#include "PRJ_TIDE_P0/Data/Player/PlayerAnimTags.h"
#include "PRJ_TIDE_P0/Data/Tags/TideGameplayTags.h"
#include "PRJ_TIDE_P0/Animation/Instances/Character/Player/PlayerAnimInstance.h"

#if !UE_BUILD_SHIPPING
#include "imgui.h"
#endif

void UDashActionPlayerModule::Initialize( ATidePlayerCharacter* InOwner )
{
	Super::Initialize( InOwner );
}

void UDashActionPlayerModule::OnModuleUpdate( float DeltaTime )
{
	if ( !OwnerCharacter ) return;

	// ダッシュモジュールだが、通常移動中のターンも一括でここで管理する
	UAnimMontage* RunTurnMontage = GetAnimMontage( PlayerAnimTags::RUN_TURN );
	UAnimMontage* DashTurnMontage = GetAnimMontage( PlayerAnimTags::DASH_TURN );

	// チャージダッシュ中のターン（発火は V2 側）も Turn フラグへ含めて旋回ロック等を共通化する
	UAnimMontage* ChargeDashTurnMontage = GetAnimMontage( PlayerAnimTags::CHARGE_DASH_TURN );
	UAnimMontage* CurrentMontage = OwnerCharacter->GetCurrentMontage();

	bool bIsTurning = ( CurrentMontage != nullptr ) &&
		( CurrentMontage == RunTurnMontage || CurrentMontage == DashTurnMontage || CurrentMontage == ChargeDashTurnMontage );

	if ( UPlayerAnimInstance* AnimInst = Cast<UPlayerAnimInstance>( OwnerCharacter->GetMesh()->GetAnimInstance() ) )
	{
		AnimInst->bIsTurning = bIsTurning;
	}

	// 空中・攻撃中・回避中・吹き飛び復帰直後はターン判定を行わない。ブーストダッシュ中も対象外——
	// Turn は TurnBrakeRate で速度を大きく削るため、逆方向入力で急停止したように見える
	if ( !OwnerCharacter->IsCharging() &&
		!OwnerCharacter->IsFalling() &&
		!OwnerCharacter->IsDodging() &&
		!OwnerCharacter->IsPlayingChargeAction() &&
		!OwnerCharacter->IsAttacking() &&
		!OwnerCharacter->IsHitReacting() &&
		!OwnerCharacter->IsRecoveryMoveAssistActive() &&
		!OwnerCharacter->IsBoostDashing() &&
		// 終了直後の滑らか旋回中も対象外。Turn が発火すると DashInertiaTimer が即クリア＋
		// TurnBrakeRate で急制動し、「カクッと曲がる」印象になる（滑らか旋回は BoostDash 側が担う）
		!OwnerCharacter->IsBoostDashExiting() )
	{
		if ( !bIsTurning )
		{
			const FVector2D RawInput = OwnerCharacter->GetRawMovementInput();
			if ( !RawInput.IsNearlyZero() )
			{
				const FVector WorldInput = OwnerCharacter->GetControlRelativeInputDirection( RawInput ).GetSafeNormal();

				if ( UCharacterMovementComponent* MovementComp = OwnerCharacter->GetCharacterMovement() )
				{
					const FPlayerTurnParams TurnParams = OwnerCharacter->PlayerParamData->ResolveTurnParams(
						bIsDashing ? OwnerCharacter->PlayerParamData->DashTurnParams : OwnerCharacter->PlayerParamData->RunTurnParams );

					// ある程度の速度が出ている時のみターンを許可
					const float CurrentSpeed2D = MovementComp->Velocity.Size2D();

					if ( CurrentSpeed2D >= TurnParams.MinSpeedForTurn )
					{
						const FVector CurrentDir = MovementComp->Velocity.GetSafeNormal2D();

						// 約 120 度以上逆に入力されたらターン
						const float InputDot = FVector::DotProduct( WorldInput, CurrentDir );

						if ( InputDot < TurnParams.TurnThresholdDot )
						{
							const FName TurnTag = bIsDashing ? PlayerAnimTags::DASH_TURN : PlayerAnimTags::RUN_TURN;
							PlayAnimMontage( TurnTag );
							bIsTurning = true;

							// ルートモーションを復活させる
							if ( UPlayerAnimInstance* AnimInst = Cast<UPlayerAnimInstance>( OwnerCharacter->GetMesh()->GetAnimInstance() ) )
							{
								AnimInst->SetRootMotionMode( ERootMotionMode::RootMotionFromMontagesOnly );
								AnimInst->bIsTurning = true;
							}

							// ダッシュの速度上書きを止める
							DashInertiaTimer.Clear();
							InitialTurnTimer.Clear();

							// 慣性を削り、モーション側のルートモーションによる移動に任せる
							MovementComp->Velocity *= TurnParams.TurnBrakeRate;
						}
					}
				}
			}
		}
	}

	if ( !bIsDashing ) return;
	if ( OwnerCharacter->IsFalling() ) return;

	// 移行直後は初速と滑りやすさを維持し、時間経過で通常ダッシュ速度へ落ち着かせる。ブースト中は止める——
	// ここは毎フレーム MaxWalkSpeed / Braking / Friction / Velocity を直接上書きするため、止めないと
	// ブースト側が設定した高速状態を旧タイマーの減衰値で潰してしまう
	if ( !DashInertiaTimer.IsFinish() && OwnerCharacter->PlayerParamData && !OwnerCharacter->IsBoostDashing() )
	{
		DashInertiaTimer.Update( DeltaTime );

		if ( UCharacterMovementComponent* MovementComp = OwnerCharacter->GetCharacterMovement() )
		{
			// 摩擦回復ロジックと手触りを統一するため InterpEaseIn を使う
			const float EasedRate = FMath::InterpEaseIn( 0.0f, 1.0f, DashInertiaTimer.GetRate(), OwnerCharacter->PlayerParamData->DashInertiaEaseExpo );

			const float TargetMaxSpeed = OwnerCharacter->PlayerParamData->DashWalkSpeed;

			const float StartBraking = OwnerCharacter->PlayerParamData->DashInertiaBrakingDeceleration;
			const float StartFriction = OwnerCharacter->PlayerParamData->ChargeDashGroundFriction;

			const float TargetBraking = OwnerCharacter->PlayerParamData->DashBrakingDeceleration;
			const float TargetFriction = OwnerCharacter->PlayerParamData->DashGroundFriction;

			// 上限速度とブレーキ力を段階的に通常ダッシュ値へ近づける
			const float CurrentInertiaSpeed = FMath::Lerp( CachedInertiaStartSpeed, TargetMaxSpeed, EasedRate );
			MovementComp->MaxWalkSpeed = FMath::Lerp( CachedInertiaStartSpeed, TargetMaxSpeed, EasedRate );
			MovementComp->BrakingDecelerationWalking = FMath::Lerp( StartBraking, TargetBraking, EasedRate );
			MovementComp->GroundFriction = FMath::Lerp( StartFriction, TargetFriction, EasedRate );

			// UE 物理による急制動を防ぐため、減速期間中も Velocity の大きさを Lerp 後の速度へ強制同期する
			if ( !MovementComp->Velocity.IsNearlyZero() )
			{
				const FVector DashDir = MovementComp->Velocity.GetSafeNormal2D();
				FVector NewVelocity = DashDir * CurrentInertiaSpeed;
				NewVelocity.Z = MovementComp->Velocity.Z;
				MovementComp->Velocity = NewVelocity;
			}
		}
	}

	const FVector2D CurrentMovementInput = OwnerCharacter->GetRawMovementInput();
	const float InputThreshold = OwnerCharacter->PlayerParamData ? OwnerCharacter->PlayerParamData->DashCancelInputThreshold : 0.2f;

	// スティックを離した／中央を通過中
	if ( CurrentMovementInput.Size() < InputThreshold )
	{
		// ターン中は猶予タイマーを進めず、ダッシュを強制終了させない
		if ( !bIsTurning )
		{
			DashGraceTimer.Update( DeltaTime );

			if ( UPlayerAnimInstance* AnimInst = Cast<UPlayerAnimInstance>( OwnerCharacter->GetMesh()->GetAnimInstance() ) )
			{
				AnimInst->bIsDashBSPlaying = false;
			}

			if ( DashGraceTimer.IsFinish() )
			{
				StopDash();
			}
		}
		else
		{
			// モーション再生中に入力が抜けても、リセットしてダッシュ状態を維持する
			DashGraceTimer.Set( OwnerCharacter->PlayerParamData->DashGracePeriod );
		}
	}
	else
	{
		DashGraceTimer.Set( OwnerCharacter->PlayerParamData->DashGracePeriod );

		if ( UPlayerAnimInstance* AnimInst = Cast<UPlayerAnimInstance>( OwnerCharacter->GetMesh()->GetAnimInstance() ) )
		{
			AnimInst->bIsDashBSPlaying = !bIsTurning;
		}
	}

	// 旋回ブースト
	if ( !InitialTurnTimer.IsFinish() )
	{
		InitialTurnTimer.Update( DeltaTime );

		// 終了した瞬間にパラメータを元へ戻す
		if ( InitialTurnTimer.IsFinish() && OwnerCharacter )
		{
			OwnerCharacter->RefreshMovementParams();
		}
	}

	// ダッシュ中の傾き（Direction）を毎フレーム更新する
	if ( UPlayerAnimInstance* AnimInst = Cast<UPlayerAnimInstance>( OwnerCharacter->GetMesh()->GetAnimInstance() ) )
	{
		if ( OwnerCharacter && OwnerCharacter->PlayerParamData )
		{
			const FVector2D RawInput = OwnerCharacter->GetRawMovementInput();
			float TargetDirection = 0.0f;

			if ( !RawInput.IsNearlyZero() )
			{
				// カメラの向きでワールド空間へ変換し、アクター基準の角度を求める
				const FVector WorldInput = OwnerCharacter->GetControlRelativeInputDirection( RawInput ).GetSafeNormal();

				const FVector ActorForward = OwnerCharacter->GetActorForwardVector();
				const FVector ActorRight = OwnerCharacter->GetActorRightVector();

				const float ForwardDot = FVector::DotProduct( WorldInput, ActorForward );
				const float RightDot = FVector::DotProduct( WorldInput, ActorRight );

				TargetDirection = FMath::RadiansToDegrees( FMath::Atan2( RightDot, ForwardDot ) );
			}

			const float DirectionInterpSpeed = OwnerCharacter->PlayerParamData->DashDirectionInterpSpeed;
			const FRotator CurrentRot( 0.0f, AnimInst->DashDirection, 0.0f );
			const FRotator TargetRot( 0.0f, TargetDirection, 0.0f );

			AnimInst->DashDirection = FMath::RInterpTo( CurrentRot, TargetRot, DeltaTime, DirectionInterpSpeed ).Yaw;
		}
	}
}

bool UDashActionPlayerModule::CanDash() const
{
	if ( bIsDashing || !OwnerCharacter ) return false;
	if ( OwnerCharacter->HasStateTag( TAG_State_Common_Disable ) ) return false;
	if ( OwnerCharacter->IsCharging() || OwnerCharacter->IsPlayingChargeAction() ) return false;
	if ( OwnerCharacter->IsChargeDashEndMovementLocked() ) return false;	// ED の硬直中（MoveCancelable 前）
	if ( OwnerCharacter->HasDodgeInputBuffered() ) return false;

	// 空中チャージダッシュを使い切った空中は攻撃・ジャンプ・神技のみ
	if ( OwnerCharacter->IsAirActionLimitedAfterAirCharge() ) return false;

	const bool bCanDash = OwnerCharacter->HasStateTag( TAG_State_Player_CanDash );

	// アクションからのキャンセル（優先）
	if ( bCanDash && OwnerCharacter->HasStateTag( TAG_State_Player_MoveCancelable ) )
	{
		return true;
	}

	// 完全なフリー状態
	if ( OwnerCharacter->IsFalling() ) return false;

	if ( !OwnerCharacter->IsAttacking() && !OwnerCharacter->IsDodging() && !OwnerCharacter->IsHitReacting() )
	{
		if ( bCanDash )
		{
			return true;
		}
	}

	return false;
}

void UDashActionPlayerModule::RequestStartDash( bool bIsFromDodge )
{
	if ( CanDash() )
	{
		StartDash( bIsFromDodge );
	}
}

void UDashActionPlayerModule::ForceStartDash( bool bIsFromDodge )
{
	StartDash( bIsFromDodge );
}

void UDashActionPlayerModule::StartDash( bool bIsFromDodge )
{
	if ( bIsDashing || !OwnerCharacter ) return;

	// 呼び出し元が false を渡してきても、実際に回避中なら「回避からの派生ダッシュ」として扱う
	const bool bWasDodging = OwnerCharacter->IsDodging();
	if ( bWasDodging )
	{
		bIsFromDodge = true;
	}

	// 回避終了処理の前に、判定用のステート（速度・方向・モーション）を退避する
	float ClearCurrentSpeed = 0.0f;
	FVector SavedDodgeVelocityDir = FVector::ForwardVector;
	UAnimMontage* SavedDodgeMontage = nullptr;

	if ( UCharacterMovementComponent* MovementComp = OwnerCharacter->GetCharacterMovement() )
	{
		ClearCurrentSpeed = MovementComp->Velocity.Size2D();
		SavedDodgeVelocityDir = MovementComp->Velocity.GetSafeNormal2D();
	}
	SavedDodgeMontage = OwnerCharacter->GetCurrentMontage();


	if ( OwnerCharacter->IsAttacking() )
	{
		OwnerCharacter->CancelAttack();
	}

	bIsDashing = true;
	OwnerCharacter->CancelDodge();
	DashGraceTimer.Clear();

	// ブレンドアウト中の回避モーションにキャラが引っ張られるのを防ぐため、
	// ルートモーションの物理適用を一時的にミュートする
	if ( UAnimInstance* AnimInst = OwnerCharacter->GetMesh()->GetAnimInstance() )
	{
		AnimInst->SetRootMotionMode( ERootMotionMode::IgnoreRootMotion );
	}

	// InitialTurnTimer の判定は MoveDir 確定後に行うので、ここではブレンドアウトだけ
	if ( bIsFromDodge && SavedDodgeMontage )
	{
		const float BlendTime = OwnerCharacter->PlayerParamData->DodgeToDashBlendOutTime;
		OwnerCharacter->StopAnimMontage( BlendTime, SavedDodgeMontage );
	}

	// RefreshMovementParams より前に MoveDir と InitialTurnTimer を確定させる
	FVector MoveDir = FVector::ZeroVector;
	const FVector2D RawInput = OwnerCharacter->GetRawMovementInput();

	if ( !RawInput.IsNearlyZero() )
	{
		MoveDir = OwnerCharacter->GetControlRelativeInputDirection( RawInput ).GetSafeNormal();
	}
	else
	{
		MoveDir = OwnerCharacter->GetActorForwardVector();
	}

	if ( bIsFromDodge )
	{
		UAnimMontage* StepBMontage = GetAnimMontage( PlayerAnimTags::STEP_B );
		UAnimMontage* StepLMontage = GetAnimMontage( PlayerAnimTags::STEP_L );
		UAnimMontage* StepRMontage = GetAnimMontage( PlayerAnimTags::STEP_R );

		bool bShouldTriggerTurnBoost = false;

		if ( SavedDodgeMontage && !RawInput.IsNearlyZero() )
		{
			if ( SavedDodgeMontage == StepBMontage || SavedDodgeMontage == StepLMontage || SavedDodgeMontage == StepRMontage )
			{
				const float AlignmentDot = FVector::DotProduct( SavedDodgeVelocityDir, MoveDir );
				constexpr float DodgeToDashAlignmentThreshold = 0.5f;

				if ( AlignmentDot >= DodgeToDashAlignmentThreshold )
				{
					bShouldTriggerTurnBoost = true;
				}
			}
		}

		if ( bShouldTriggerTurnBoost )
		{
			InitialTurnTimer.Set( OwnerCharacter->PlayerParamData->DodgeToDashInitialTurnBoostTime );
		}
		else
		{
			InitialTurnTimer.Clear();
		}
	}

	OwnerCharacter->RefreshMovementParams();	// ここでダッシュ用の移動速度が載る

	if ( UCharacterMovementComponent* MovementComp = OwnerCharacter->GetCharacterMovement() )
	{
		MovementComp->ClearAccumulatedForces();

		FVector CurrentVelDir = MovementComp->Velocity.GetSafeNormal2D();
		float DirectionDot = FVector::DotProduct( CurrentVelDir, MoveDir );
		float MomentumRetentionRate = FMath::Max( 0.0f, DirectionDot );

		float SpeedRetentionRate = MomentumRetentionRate;
		float DirectionLerpRate = MomentumRetentionRate;

		const float DashTargetSpeed = OwnerCharacter->PlayerParamData->DashWalkSpeed;

		if ( ClearCurrentSpeed > DashTargetSpeed + 50.0f )
		{
			SpeedRetentionRate = 1.0f;
			DirectionLerpRate = 0.0f;
		}

		float CurrentSpeed2D = ClearCurrentSpeed * SpeedRetentionRate;

		const float MinInitialSpeed = OwnerCharacter->PlayerParamData->MaxWalkSpeed;
		const float MaxInheritableSpeed = DashTargetSpeed * 3.0f;

		float FinalInitialSpeed = 0.0f;
		if ( bIsFromDodge )
		{
			FinalInitialSpeed = FMath::Max( ClearCurrentSpeed, DashTargetSpeed );
		}
		else
		{
			FinalInitialSpeed = FMath::Clamp( CurrentSpeed2D, MinInitialSpeed, MaxInheritableSpeed );
		}

		FVector ApplyDir = FMath::Lerp( OwnerCharacter->GetActorForwardVector(), MoveDir, DirectionLerpRate ).GetSafeNormal();

		if ( bIsFromDodge )
		{
			ApplyDir = SavedDodgeVelocityDir;
		}

		MovementComp->Velocity.X = ApplyDir.X * FinalInitialSpeed;
		MovementComp->Velocity.Y = ApplyDir.Y * FinalInitialSpeed;

		if ( bIsFromDodge || FinalInitialSpeed > DashTargetSpeed + 50.0f )
		{
			DashInertiaTimer.Set( OwnerCharacter->PlayerParamData->DashInertiaDecelerationTime );
			CachedInertiaStartSpeed = FinalInitialSpeed;
		}
		else
		{
			DashInertiaTimer.Clear();
		}
	}

	if ( OwnerCharacter )
	{
		if ( UPlayerAnimInstance* AnimInst = Cast<UPlayerAnimInstance>( OwnerCharacter->GetMesh()->GetAnimInstance() ) )
		{
			AnimInst->bIsDashBSPlaying = true;
		}
	}
}

void UDashActionPlayerModule::StartInertiaDecayFromSpeed( float StartSpeed )
{
	if ( !OwnerCharacter || !OwnerCharacter->PlayerParamData ) return;

	const float DashTargetSpeed = OwnerCharacter->PlayerParamData->DashWalkSpeed;

	// StartDash と同じ基準。超えている場合のみ慣性減速フェーズへ入る
	if ( StartSpeed > DashTargetSpeed + 50.0f )
	{
		CachedInertiaStartSpeed = StartSpeed;
		DashInertiaTimer.Set( OwnerCharacter->PlayerParamData->DashInertiaDecelerationTime );

		// 呼び出し元が先に通常ダッシュの最終値を適用済みなので、次フレームの補間が効くまでの 1 フレームだけ
		// 強めのブレーキが物理に使われて「ガクッ」となる。補間の開始値を即座に適用して隙間を無くす
		if ( UCharacterMovementComponent* Movement = OwnerCharacter->GetCharacterMovement() )
		{
			Movement->MaxWalkSpeed = StartSpeed;
			Movement->BrakingDecelerationWalking = OwnerCharacter->PlayerParamData->DashInertiaBrakingDeceleration;
			Movement->GroundFriction = OwnerCharacter->PlayerParamData->ChargeDashGroundFriction;
		}
	}
	else
	{
		DashInertiaTimer.Clear();
	}
}

void UDashActionPlayerModule::StopDash()
{
	if ( !bIsDashing ) return;

	bIsDashing = false;
	DashGraceTimer.Clear();
	DashInertiaTimer.Clear();
	InitialTurnTimer.Clear();

	if ( OwnerCharacter )
	{
		// ルートモーションの設定を標準へ戻す
		if ( UPlayerAnimInstance* AnimInst = Cast<UPlayerAnimInstance>( OwnerCharacter->GetMesh()->GetAnimInstance() ) )
		{
			AnimInst->SetRootMotionMode( ERootMotionMode::RootMotionFromMontagesOnly );
			AnimInst->bIsDashBSPlaying = false;
		}

		UAnimMontage* DashMontage = GetAnimMontage( PlayerAnimTags::DASH );
		if ( DashMontage && OwnerCharacter->GetCurrentMontage() == DashMontage )
		{
			OwnerCharacter->StopAnimMontage( DashMontage );
		}

		OwnerCharacter->RefreshMovementParams();
	}
}

bool UDashActionPlayerModule::IsTurning() const
{
	if ( !OwnerCharacter ) return false;

	// アニメーションの遅延による誤爆を避けるため、フラグを最優先で確認する
	if ( UPlayerAnimInstance* AnimInst = Cast<UPlayerAnimInstance>( OwnerCharacter->GetMesh()->GetAnimInstance() ) )
	{
		if ( AnimInst->bIsTurning )
		{
			return true;
		}
	}

	UAnimMontage* CurrentMontage = OwnerCharacter->GetCurrentMontage();
	if ( !CurrentMontage ) return false;
	UAnimMontage* RunTurnMontage = GetAnimMontage( PlayerAnimTags::RUN_TURN );
	UAnimMontage* DashTurnMontage = GetAnimMontage( PlayerAnimTags::DASH_TURN );
	UAnimMontage* ChargeDashTurnMontage = GetAnimMontage( PlayerAnimTags::CHARGE_DASH_TURN );
	return ( CurrentMontage == RunTurnMontage || CurrentMontage == DashTurnMontage || CurrentMontage == ChargeDashTurnMontage );
}

#if !UE_BUILD_SHIPPING
void UDashActionPlayerModule::DrawDebugImGui()
{
	if ( !ImGui::CollapsingHeader( "Dash Module", ImGuiTreeNodeFlags_DefaultOpen ) ) return;

	ImGui::Indent();
	ImGui::TextColored( bIsDashing ? ImVec4( 0, 1, 0, 1 ) : ImVec4( 0.6f, 0.6f, 0.6f, 1.0f ), "Is Dashing: %s", bIsDashing ? "True" : "False" );

	if ( bIsDashing )
	{
		ImGui::Text( "Grace Timer: %.2f", DashGraceTimer.GetElapsed() );
	}
	ImGui::Unindent();
}
#endif
