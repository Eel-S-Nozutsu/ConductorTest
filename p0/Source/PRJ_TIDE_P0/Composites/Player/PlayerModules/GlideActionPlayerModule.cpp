// Copyright (c) 2026, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.
#include "GlideActionPlayerModule.h"

#include <imgui.h>
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"

#include "PRJ_TIDE_P0/Actors/Character/Player/TidePlayerCharacter.h"
#include "PRJ_TIDE_P0/Data/Player/PlayerAnimTags.h"
#include "PRJ_TIDE_P0/Data/Player/TidePlayerParamDataAsset.h"
#include "PRJ_TIDE_P0/Data/Tags/TideGameplayTags.h"

void UGlideActionPlayerModule::OnModuleUpdate( float DeltaTime )
{
	if ( !OwnerCharacter || !GetPlayerParams() ) return;

	// 離した時点で 0 へ戻し、着地時の封印も解除する
	if ( bJumpInputHeld )
	{
		JumpHeldTime += DeltaTime;
	}
	else
	{
		JumpHeldTime = 0.0f;
		bBlockUntilJumpRelease = false;
	}

	ChargeHeldTime = bChargeInputHeld ? ( ChargeHeldTime + DeltaTime ) : 0.0f;

	if ( bIsGliding )
	{
		if ( !CanKeepGliding() )
		{
			StopGlide( true );	// 離した／着地した／他アクションが割り込んだ
			return;
		}

		// 持続時間切れ。着地までこの空中では再滑空させない
		if ( GetPlayerParams()->GlideMaxDuration > 0.0f )
		{
			DurationTimer.Update( DeltaTime );
			if ( DurationTimer.IsFinish() )
			{
				bDurationExhausted = true;
				StopGlide( true );
				return;
			}
		}

		GlideElapsedTime += DeltaTime;
		UpdateGlideMontage();
		UpdateGlideMovement( DeltaTime );
		return;
	}

	if ( CanStartGlide( false ) )
	{
		StartGlide( false );
		UpdateGlideMovement( DeltaTime );	// 開始フレームから緩降下を当てる（1フレームぶん落ちるのを防ぐ）
		return;
	}

	UpdateFallRamp( DeltaTime );
}

void UGlideActionPlayerModule::OnLanded()
{
	if ( !OwnerCharacter ) return;

	const bool bWasGliding = bIsGliding;
	StopGlide( false );

	// 滑空モーション中の着地は先に走る JumpActionPlayerModule がジャンプ系と認識できず JUMP_ED を流さないため、
	// 通常のジャンプ着地（End 状態）へ明示的に引き渡す（ED キャンセルの走り出しも従来どおり働く）
	if ( bWasGliding && !OwnerCharacter->IsDead() )
	{
		OwnerCharacter->EnterJumpLandingEnd();
	}

	DurationTimer.Clear();
	FallRampTimer.Clear();
	bDurationExhausted = false;
	JumpHeldTime = 0.0f;
	// 押しっぱなしで着地したら離すまで次の滑空を受け付けない（崖から出た瞬間の暴発防止）
	bBlockUntilJumpRelease = bJumpInputHeld;
}

void UGlideActionPlayerModule::SetJumpInputHeldRaw( bool bHeld )
{
	bJumpInputHeld = bHeld;
	if ( bHeld )
	{
		// 以降は通常操作（押している間だけ滑空／離すと落下）へ主導権を返す
		bAutoGlideActive = false;
	}
	else
	{
		JumpHeldTime = 0.0f;
		bBlockUntilJumpRelease = false;
	}
}

void UGlideActionPlayerModule::SetChargeInputHeldRaw( bool bHeld )
{
	bChargeInputHeld = bHeld;
	if ( bHeld )
	{
		// R2 が滑空の入力になる状況でだけ主導権を返す（溜めに使われる場面の R2 で自動展開を切らない）
		if ( IsChargeHoldGlideAllowed() ) bAutoGlideActive = false;
	}
	else
	{
		ChargeHeldTime = 0.0f;
	}
}

bool UGlideActionPlayerModule::RequestGlideAfterAirChargeDash()
{
	if ( bIsGliding ) return true;
	if ( !OwnerCharacter ) return false;

	const UTidePlayerParamDataAsset* PlayerParams = GetPlayerParams();
	if ( !PlayerParams ) return false;

	// 長押し中ならその押下ぶんとして展開し、押していなければパラメータ次第で自動展開する。
	// どちらもダッシュ終わりに残る上向き速度は待たない（緩降下で潰す）ため bAutoDeploy で開始判定を通す
	if ( !IsGlideInputHeldEnough() && !PlayerParams->bAutoGlideAfterAirChargeDash ) return false;
	if ( !CanStartGlide( true ) ) return false;

	StartGlide( true );
	return true;
}

void UGlideActionPlayerModule::CancelForWindJump()
{
	// 打ち上げ側が竜巻ジャンプの ST を張るので、落下ループへは引き渡さない
	if ( bIsGliding ) StopGlide( false );
}

void UGlideActionPlayerModule::ResetForLaunch()
{
	// 打ち上げ側がモーション（チャージジャンプLP）を張るので、落下ループへは引き渡さない
	if ( bIsGliding ) StopGlide( false );

	// 打ち上げ＝新しい滞空の始まりとして、着地と同じく持続時間の予算を張り直す。押しっぱなしのまま
	// 乗ったケースも受け付ける（着地直後の暴発防止とは事情が違い、打ち上げ後は空中に居るため）
	DurationTimer.Clear();
	FallRampTimer.Clear();
	bDurationExhausted = false;
	bBlockUntilJumpRelease = false;
}

float UGlideActionPlayerModule::GetRemainingTime() const
{
	return bIsGliding ? DurationTimer.Get() : 0.0f;
}

bool UGlideActionPlayerModule::CanStartGlide( bool bAutoDeploy ) const
{
	const UTidePlayerParamDataAsset* PlayerParams = GetPlayerParams();
	if ( !PlayerParams || !PlayerParams->bEnableAirGlide ) return false;

	if ( bDurationExhausted ) return false;
	if ( !OwnerCharacter->IsFalling() ) return false;
	if ( IsBlockedByOtherAction() ) return false;

	// 自動展開はボタン押下も上昇の終わりも待たない（上向き速度は緩降下で潰す）
	if ( bAutoDeploy ) return true;

	if ( !IsGlideInputHeldEnough() ) return false;

	// ジャンプが残っていれば押した瞬間のジャンプが先に出るので、上昇が終わってから滑空へ移る
	const UCharacterMovementComponent* MovementComp = GetCharacterMovement();
	return MovementComp && MovementComp->Velocity.Z <= 0.0f;
}

bool UGlideActionPlayerModule::CanKeepGliding() const
{
	const UTidePlayerParamDataAsset* PlayerParams = GetPlayerParams();
	if ( !PlayerParams || !PlayerParams->bEnableAirGlide ) return false;
	// 自動展開ぶんはボタンを押していなくても継続する
	if ( !IsGlideInputHeld() && !bAutoGlideActive ) return false;
	if ( !OwnerCharacter->IsFalling() ) return false;

	return !IsBlockedByOtherAction();
}

bool UGlideActionPlayerModule::IsGlideInputHeld() const
{
	return bJumpInputHeld || ( bChargeInputHeld && IsChargeHoldGlideAllowed() );
}

bool UGlideActionPlayerModule::IsGlideInputHeldEnough() const
{
	const UTidePlayerParamDataAsset* PlayerParams = GetPlayerParams();
	if ( !PlayerParams ) return false;

	const float HoldTime = PlayerParams->GlideJumpHoldTime;
	if ( bJumpInputHeld && !bBlockUntilJumpRelease && JumpHeldTime >= HoldTime ) return true;

	return bChargeInputHeld && ChargeHeldTime >= HoldTime && IsChargeHoldGlideAllowed();
}

bool UGlideActionPlayerModule::IsChargeHoldGlideAllowed() const
{
	const UTidePlayerParamDataAsset* PlayerParams = GetPlayerParams();
	if ( !PlayerParams || !PlayerParams->bGlideByChargeHoldAfterAirChargeDash ) return false;

	// R2 は本来「溜め」の入力なので、溜めが封じられる空中チャージダッシュ後の空中でだけ滑空へ回す
	return OwnerCharacter && OwnerCharacter->IsAirActionLimitedAfterAirCharge();
}

bool UGlideActionPlayerModule::IsBlockedByOtherAction() const
{
	return OwnerCharacter->IsDead()
		|| OwnerCharacter->HasStateTag( TAG_State_Common_Disable )
		|| OwnerCharacter->IsAttacking()
		|| OwnerCharacter->IsDodging()
		|| OwnerCharacter->IsHitReacting()
		|| OwnerCharacter->IsCharging()
		|| OwnerCharacter->IsPlayingChargeAction()
		|| OwnerCharacter->IsBoostDashing()
		|| OwnerCharacter->IsGodActionActive();
}

void UGlideActionPlayerModule::StartGlide( bool bAutoDeploy )
{
	bIsGliding = true;
	GlideElapsedTime = 0.0f;
	FallRampTimer.Clear();
	// 自動展開でもボタンを押していれば「離す＝落下」で閉じられるようにする
	bAutoGlideActive = bAutoDeploy && !IsGlideInputHeld();

	const UTidePlayerParamDataAsset* PlayerParams = GetPlayerParams();
	if ( PlayerParams->GlideMaxDuration > 0.0f )
	{
		// 空中 1 回ぶんの予算。滑空⇄落下を往復しても残り時間を引き継ぐ（着地でのみリセット）
		if ( !DurationTimer.IsValid() )
		{
			DurationTimer.Set( PlayerParams->GlideMaxDuration );
		}
	}
	else
	{
		DurationTimer.Clear();
	}

	// 再生し切ってから AirGlideLoop へ移す（緩降下の物理は再生完了を待たず並行して当たる）
	PlayAnimMontage( PlayerAnimTags::AIR_GLIDE_ST );
}

void UGlideActionPlayerModule::StopGlide( bool bEndedInAir )
{
	if ( !bIsGliding ) return;
	bIsGliding = false;
	bAutoGlideActive = false;

	// 次のモーションを流すだけではスロットが違うと AirGlideLoop がループし続けて滑空ポーズが残る
	StopGlideMontages();

	OwnerCharacter->RefreshMovementParams();	// GravityScale=0 で殺していた重力を戻す
	FallRampTimer.Clear();

	// 着地・他アクションへの移行では、モーションも落下速度もそちらに任せる
	// （ダイブ等の落下速度をランプで頭打ちにしないため）
	if ( !bEndedInAir || IsBlockedByOtherAction() || !OwnerCharacter->IsFalling() ) return;

	// JumpActionPlayerModule の Loop 状態へ入れ、着地の JUMP_ED まで繋ぐ
	OwnerCharacter->EnterJumpFallingLoop();

	// 解除直後の急降下を防ぐため、落下速度の上限を徐々に開放する
	const UTidePlayerParamDataAsset* PlayerParams = GetPlayerParams();
	if ( PlayerParams && PlayerParams->GlideReleaseFallRampTime > 0.0f )
	{
		FallRampTimer.Set( PlayerParams->GlideReleaseFallRampTime );
	}
}

void UGlideActionPlayerModule::StopGlideMontages()
{
	if ( UAnimMontage* StartMontage = GetAnimMontage( PlayerAnimTags::AIR_GLIDE_ST ) )
	{
		OwnerCharacter->StopAnimMontage( StartMontage );
	}
	if ( UAnimMontage* LoopMontage = GetAnimMontage( PlayerAnimTags::AIR_GLIDE_LP ) )
	{
		OwnerCharacter->StopAnimMontage( LoopMontage );
	}
}

void UGlideActionPlayerModule::UpdateGlideMontage()
{
	// 開始直後は AnimInstance の更新遅れで GetCurrentMontage() が古い値を返し ST を飛ばしてしまうため、
	// しばらくは終了判定そのものを行わない（チャージジャンプ ST→LP と同流儀）
	static constexpr float GlideStartCheckDelay = 0.1f;
	if ( GlideElapsedTime < GlideStartCheckDelay ) return;

	if ( !HasGlideStartMontageFinished() ) return;

	UAnimMontage* LoopMontage = GetAnimMontage( PlayerAnimTags::AIR_GLIDE_LP );
	if ( LoopMontage && OwnerCharacter->GetCurrentMontage() != LoopMontage )
	{
		PlayAnimMontage( PlayerAnimTags::AIR_GLIDE_LP );
	}
}

void UGlideActionPlayerModule::UpdateGlideMovement( float DeltaTime )
{
	UCharacterMovementComponent* MovementComp = GetCharacterMovement();
	if ( !MovementComp ) return;

	const UTidePlayerParamDataAsset* PlayerParams = GetPlayerParams();

	// RequestMove は滑空中スキップなので、移動・回頭はここで完結させる
	const FVector2D RawInput = OwnerCharacter->GetRawMovementInput();
	FVector InputDir = FVector::ZeroVector;
	if ( !RawInput.IsNearlyZero() )
	{
		InputDir = OwnerCharacter->GetControlRelativeInputDirection( RawInput ).GetSafeNormal2D();
	}

	// 機体を入力方向へ回頭させる
	if ( !InputDir.IsNearlyZero() )
	{
		const FRotator CurRot = OwnerCharacter->GetActorRotation();
		const FRotator TargetRot( 0.0f, InputDir.Rotation().Yaw, 0.0f );
		const FRotator NewRot = FMath::RInterpConstantTo( CurRot, TargetRot, DeltaTime, PlayerParams->GlideTurnRateYaw );
		OwnerCharacter->SetActorRotation( FRotator( 0.0f, NewRot.Yaw, 0.0f ) );
	}

	// 機体前方(0)と入力方向(1)をブレンド（1 ほど入力方向へ直接進む＝制御しやすい）
	FVector MoveDir = OwnerCharacter->GetActorForwardVector().GetSafeNormal2D();
	if ( !InputDir.IsNearlyZero() && PlayerParams->GlideAirControlStrength > 0.0f )
	{
		const float Strength = FMath::Clamp( PlayerParams->GlideAirControlStrength, 0.0f, 1.0f );
		const FVector Blended = FMath::Lerp( MoveDir, InputDir, Strength ).GetSafeNormal2D();
		if ( !Blended.IsNearlyZero() ) MoveDir = Blended;
	}

	// 前進は倒し量に比例（入力ゼロなら水平停止＝その場で緩降下）
	const float InputMag = FMath::Clamp( RawInput.Size(), 0.0f, 1.0f );
	const FVector TargetHorizVel = InputDir.IsNearlyZero()
		? FVector::ZeroVector
		: ( MoveDir * ( PlayerParams->GlideForwardSpeed * InputMag ) );

	// AccelRate>0 で目標速度へ補間（＝離しても少し滑る）。0 なら即時＝入力どおり
	FVector NewHorizVel = TargetHorizVel;
	if ( PlayerParams->GlideMoveAccelRate > 0.0f )
	{
		FVector CurrentHorizVel = MovementComp->Velocity;
		CurrentHorizVel.Z = 0.0f;
		NewHorizVel = FMath::VInterpTo( CurrentHorizVel, TargetHorizVel, DeltaTime, PlayerParams->GlideMoveAccelRate );
	}

	// 重力は自前制御にして緩降下を当てる
	FVector NewVelocity = NewHorizVel;
	NewVelocity.Z = -PlayerParams->GlideDescendSpeed;	// 正値パラメータ＝降下量なので符号反転
	MovementComp->Velocity = NewVelocity;
	MovementComp->GravityScale = 0.0f;
}

void UGlideActionPlayerModule::UpdateFallRamp( float DeltaTime )
{
	if ( FallRampTimer.IsFinish() ) return;

	// 着地した／他アクションが速度を握ったらランプは打ち切る
	if ( !OwnerCharacter->IsFalling() || IsBlockedByOtherAction() )
	{
		FallRampTimer.Clear();
		return;
	}

	UCharacterMovementComponent* MovementComp = GetCharacterMovement();
	if ( !MovementComp ) return;

	// 上限を GlideDescendSpeed → GlideReleaseTargetFallSpeed へ徐々に開放する
	const UTidePlayerParamDataAsset* PlayerParams = GetPlayerParams();
	FallRampTimer.Update( DeltaTime );
	const float Alpha = FallRampTimer.GetRate();
	const float MaxFallSpeed = FMath::Lerp( PlayerParams->GlideDescendSpeed, PlayerParams->GlideReleaseTargetFallSpeed, Alpha );
	if ( MovementComp->Velocity.Z < -MaxFallSpeed )
	{
		MovementComp->Velocity.Z = -MaxFallSpeed;
	}
}

bool UGlideActionPlayerModule::HasGlideStartMontageFinished() const
{
	// 別モンタージュへ切替わった／末尾に到達で完了とみなす
	UAnimMontage* StartMontage = GetAnimMontage( PlayerAnimTags::AIR_GLIDE_ST );
	if ( !StartMontage ) return true;	// 未設定なら待たずにループへ

	UAnimInstance* AnimInst = OwnerCharacter->GetMesh() ? OwnerCharacter->GetMesh()->GetAnimInstance() : nullptr;
	if ( !AnimInst ) return false;

	if ( OwnerCharacter->GetCurrentMontage() != StartMontage ) return true;

	const float CurrentPos = AnimInst->Montage_GetPosition( StartMontage );
	return CurrentPos >= ( StartMontage->GetPlayLength() - 0.05f );
}

const UTidePlayerParamDataAsset* UGlideActionPlayerModule::GetPlayerParams() const
{
	return OwnerCharacter ? OwnerCharacter->PlayerParamData : nullptr;
}

UCharacterMovementComponent* UGlideActionPlayerModule::GetCharacterMovement() const
{
	return OwnerCharacter ? OwnerCharacter->GetCharacterMovement() : nullptr;
}

void UGlideActionPlayerModule::DrawDebugImGui()
{
	if ( !ImGui::CollapsingHeader( "Glide Module", ImGuiTreeNodeFlags_DefaultOpen ) ) return;

	ImGui::Indent();

	const ImVec4 OkColor( 0.0f, 1.0f, 0.0f, 1.0f );
	const ImVec4 NgColor( 1.0f, 0.3f, 0.3f, 1.0f );
	const ImVec4 GrayColor( 0.5f, 0.5f, 0.5f, 1.0f );

	const UTidePlayerParamDataAsset* PlayerParams = GetPlayerParams();

	ImGui::TextColored( bIsGliding ? ImVec4( 0.0f, 1.0f, 0.5f, 1.0f ) : GrayColor, "滑空中: %s", bIsGliding ? "Yes" : "No" );
	const float HoldTime = PlayerParams ? PlayerParams->GlideJumpHoldTime : 0.0f;
	ImGui::Text( "Jump 押下: %s  押下時間: %.2f / %.2f", bJumpInputHeld ? "Held" : "-", JumpHeldTime, HoldTime );
	ImGui::Text( "R2 押下: %s  押下時間: %.2f / %.2f  滑空入力として有効: %s",
		bChargeInputHeld ? "Held" : "-", ChargeHeldTime, HoldTime,
		IsChargeHoldGlideAllowed() ? "Yes（空中ダッシュ後）" : "No" );
	ImGui::Text( "自動展開: %s", bAutoGlideActive ? "Yes（ボタン不要で継続中）" : "No" );

	if ( OwnerCharacter )
	{
		const UCharacterMovementComponent* MovementComp = GetCharacterMovement();
		ImGui::Text( "空中: %s  Vz: %.1f  打ち上げ上昇中: %s", OwnerCharacter->IsFalling() ? "Air" : "Ground",
			MovementComp ? MovementComp->Velocity.Z : 0.0f,
			OwnerCharacter->IsLaunchAscending() ? "Yes（チャージ抑止）" : "No" );

		// 発動しない原因はこの2行を見る（赤が原因）
		ImGui::TextColored( IsBlockedByOtherAction() ? NgColor : OkColor, "他アクション割り込み: %s",
			IsBlockedByOtherAction() ? "Blocked" : "None" );
		ImGui::TextColored( ( bDurationExhausted || bBlockUntilJumpRelease ) ? NgColor : OkColor,
			"持続時間切れ: %s  離し待ち: %s",
			bDurationExhausted ? "Yes" : "No", bBlockUntilJumpRelease ? "Yes" : "No" );
	}

	if ( DurationTimer.IsValid() )
	{
		ImGui::Text( "滑空持続: %.2f / %.2f", DurationTimer.Get(), DurationTimer.GetStart() );
	}
	if ( !FallRampTimer.IsFinish() )
	{
		ImGui::Text( "落下ランプ: %.2f / %.2f", FallRampTimer.Get(), FallRampTimer.GetStart() );
	}

	if ( PlayerParams )
	{
		ImGui::TextColored( GrayColor, "param: 滑空有効=%s / 空中ダッシュ後の自動展開=%s / R2長押し滑空=%s / 持続=%.2f / 前進=%.0f / 降下=%.0f",
			PlayerParams->bEnableAirGlide ? "true" : "false",
			PlayerParams->bAutoGlideAfterAirChargeDash ? "true" : "false",
			PlayerParams->bGlideByChargeHoldAfterAirChargeDash ? "true" : "false",
			PlayerParams->GlideMaxDuration, PlayerParams->GlideForwardSpeed, PlayerParams->GlideDescendSpeed );
	}

	ImGui::Unindent();
}
