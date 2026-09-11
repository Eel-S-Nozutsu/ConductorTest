// Copyright (c) 2026, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.
#include "BoostDashPlayerModule.h"

#include "Animation/AnimMontage.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/PoseableMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Kismet/KismetMaterialLibrary.h"
#include "Kismet/KismetMathLibrary.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialParameterCollection.h"

#include "GameFramework/PlayerController.h"
#include "Engine/LocalPlayer.h"

#include "PRJ_TIDE_P0/Actors/Character/Player/TidePlayerCharacter.h"
#include "PRJ_TIDE_P0/Data/Player/PlayerAnimTags.h"
#include "PRJ_TIDE_P0/Data/Player/TidePlayerParamDataAsset.h"
#include "PRJ_TIDE_P0/Subsystems/Camera/ExCameraSubsystem.h"

void UBoostDashPlayerModule::Initialize( ATidePlayerCharacter* InOwner )
{
	Super::Initialize( InOwner );

	bIsBoosting = false;
	BoostMaxSpeed = 0.0f;
	BoostTimer.Clear();
	GravityLockTimer.Clear();
	BoostGroundExitTimer.Clear();
	GravityLockZVelocity = 0.0f;
	bWasFallingDuringBoost = false;
	AirDecayBaseSpeed = -1.0f;
}

void UBoostDashPlayerModule::BeginBoost( float MaxSpeed, float Duration, float VelocityCap, float AirZUpSpeed, float StartMontagePlayRate )
{
	if ( !OwnerCharacter || OwnerCharacter->IsDead() ) return;
	if ( MaxSpeed <= 0.0f || Duration <= 0.0f ) return;

	UCharacterMovementComponent* Movement = OwnerCharacter->GetCharacterMovement();
	if ( !Movement ) return;

	// 現在の向きへ MaxSpeed ぶん加速する（既存の速度に加算し、垂直は維持）。
	// 状態で分岐せず常にこの共通挙動を適用する（空中で触れた場合の水平維持のみ例外）
	const FVector Facing = OwnerCharacter->GetActorForwardVector().GetSafeNormal2D();

	const UTidePlayerParamDataAsset* PlayerParams = OwnerCharacter->PlayerParamData;
	const float AirLockTime = PlayerParams ? PlayerParams->BoostDashAirGravityLockTime : 0.0f;
	const bool  bInAir = OwnerCharacter->IsFalling();

	// 旧仕様への切替：チャージ溜め中に触れたとき、溜めを維持して速度加算だけ行う（既定 false＝溜めを中断して
	// モーションを統一）。チャージダッシュ／ジャンプ中の中断・再開はこのフラグに依らず常に働く
	const bool bKeepChargeMotionWhileCharging = PlayerParams && PlayerParams->bBoostKeepChargeMotionWhileCharging;
	const bool bPreserveChargeIdle = bKeepChargeMotionWhileCharging && OwnerCharacter->IsCharging();

	// 溜めを中断してギア／コンボ段を退避する。これで「どの状況で触れてもブースト専用モーションを流す」に統一でき、
	// 終了後は長押し継続なら退避ギアのまま再開する
	if ( !bPreserveChargeIdle )
	{
		OwnerCharacter->PauseChargeForBoost();
	}

	// 残り持続時間を退避しておき、EndBoost から ResumeChargeDashAfterBoost で残りぶん再開する
	OwnerCharacter->PauseChargeDashForBoost();

	// ジャンプの状態機械も中断してブーストへ主導権を渡す。中断しないと IsPlayingChargeAction() が true のまま
	// 残って ST 再生とループ張り直しが抑制され、専用モーションが CHARGE_JUMP_LP に上書きされる。
	// ダッシュと違い残り時間の退避・再開はしない
	OwnerCharacter->CancelChargeJumpForBoost();

	// 滑空中は UGlideActionPlayerModule 側が「ブースト中」を見て自分で畳む（重力も復帰させる）

	// DodgeActionPlayerModule は Stepping 中に毎フレーム Velocity を上書きするため、
	// キャンセルしないとブーストの加速が同フレームで消える
	OwnerCharacter->CancelDodge();

	BoostMaxSpeed = MaxSpeed;
	bIsBoosting = true;
	BoostTimer.Set( Duration );
	bWasFallingDuringBoost = bInAir;	// 切り替わったら旋回パラメータを張り直す（UpdateBoostTurnParams）
	AirDecayBaseSpeed = -1.0f;
	BoostGroundExitTimer.Clear();		// 前回の終了時の滑らか旋回が残っていても打ち切る

	// MaxWalkSpeed をブースト速度へ上書き・低摩擦にする。
	// IsBoostDashing() が最優先で拾われるため、他状態より先にブーストパラメータが載る
	OwnerCharacter->RefreshMovementParams();

	PushBoostDashCamera();	// キー未設定なら何もしない。EndBoost で Pop する

	Movement->Velocity.X += Facing.X * BoostMaxSpeed;
	Movement->Velocity.Y += Facing.Y * BoostMaxSpeed;

	// 既に高速移動中（チャージダッシュ等）に触れて単純加算すると際限なく速度が伸びるため、
	// 上限を超えていれば向きを保ったまま縮尺する
	if ( VelocityCap > 0.0f )
	{
		const FVector2D HorizontalVelocity( Movement->Velocity.X, Movement->Velocity.Y );
		const float HorizontalSpeed = HorizontalVelocity.Size();
		if ( HorizontalSpeed > VelocityCap )
		{
			const FVector2D ClampedVelocity = HorizontalVelocity * ( VelocityCap / HorizontalSpeed );
			Movement->Velocity.X = ClampedVelocity.X;
			Movement->Velocity.Y = ClampedVelocity.Y;
		}
	}

	if ( bInAir )
	{
		if ( AirLockTime > 0.0f )
		{
			// 一定時間だけ落下を止めて水平維持する（チャージダッシュの空中水平維持と同方式）。
			// AirZUpSpeed 指定時はその上昇速度を維持したまま水平を保つ
			GravityLockTimer.Set( AirLockTime );
			GravityLockZVelocity = AirZUpSpeed;
			Movement->Velocity.Z = GravityLockZVelocity;
		}
		else if ( AirZUpSpeed != 0.0f )
		{
			// ロックを使わない場合は既存の垂直速度へ一回だけ加算する
			Movement->Velocity.Z += AirZUpSpeed;
		}
	}

	// 突進モーションは ST から再生し、終了後は UpdateBoostMontageState が LP へ繋ぐ
	// （空中取得等で持続時間が変わる場合はギミック側で算出した再生速度で流す）
	if ( !bPreserveChargeIdle )
	{
		PlayAnimMontage( PlayerAnimTags::CHARGE_03_DASH_ST, StartMontagePlayRate );
	}
}

void UBoostDashPlayerModule::OnModuleUpdate( float DeltaTime )
{
	UpdateGravityLock( DeltaTime );

	// 以下 2 つはフェードアウト・終了直後の滑らか旋回を継続させるため、bIsBoosting チェックより前で更新する
	UpdateGhostTrails( DeltaTime );
	UpdateFresnelEffect( DeltaTime );
	UpdateBoostGroundExit( DeltaTime );

	if ( !bIsBoosting ) return;

	BoostTimer.Update( DeltaTime );
	if ( BoostTimer.IsFinish() )
	{
		EndBoost();
		return;
	}

	UpdateBoostTurnParams();
	UpdateBoostAirSteering();
	UpdateBoostMontageState( DeltaTime );
}

void UBoostDashPlayerModule::UpdateBoostMontageState( float DeltaTime )
{
	if ( !OwnerCharacter || !bIsBoosting ) return;

	// 溜め中・チャージアクション進行中はそちらの見た目を優先する。IsPlayingChargeAction() を見ないと、
	// 解放直後に IsCharging()==false となった瞬間ここが素通りして上書きしてしまう
	if ( OwnerCharacter->IsCharging() || OwnerCharacter->IsPlayingChargeAction() )
	{
		OwnerCharacter->StopChargeDashLoopBlendSpace();
		return;
	}

	UAnimMontage* StartMontage = GetAnimMontage( PlayerAnimTags::CHARGE_03_DASH_ST );
	UAnimMontage* CurrentMontage = OwnerCharacter->GetCurrentMontage();

	// ST 再生中は最後まで再生させ、Loopへ割り込ませない
	if ( StartMontage && CurrentMontage == StartMontage )
	{
		OwnerCharacter->StopChargeDashLoopBlendSpace();
		return;
	}

	// 空中では ST 終了後、突進 Loop ではなく通常の落下ループへ繋ぐ
	// （JUMP_LP が既に流れていれば張り直さないため毎フレーム呼んでも安全）
	if ( OwnerCharacter->IsFalling() )
	{
		OwnerCharacter->StopChargeDashLoopBlendSpace();
		OwnerCharacter->EnterJumpFallingLoop();
		return;
	}

	// 地上突進 Loop はチャージダッシュと同じ BlendSpace で表現する。
	// ブーストは最大速度の突進なのでギア参（3）相当を使う
	OwnerCharacter->UpdateChargeDashLoopBlendSpace( 3, DeltaTime );
}

void UBoostDashPlayerModule::UpdateBoostTurnParams()
{
	if ( !OwnerCharacter || !bIsBoosting ) return;

	// チャージ側が主導権を握っている間は、再開したチャージダッシュ等のパラメータを上書きしないよう触らない
	if ( OwnerCharacter->IsCharging() || OwnerCharacter->IsPlayingChargeAction() ) return;

	// 接地状態が切り替わったフレームだけ張り直し、地上/空中それぞれの旋回速度を反映する
	const bool bFalling = OwnerCharacter->IsFalling();
	if ( bFalling != bWasFallingDuringBoost )
	{
		bWasFallingDuringBoost = bFalling;
		OwnerCharacter->RefreshMovementParams();
	}
}

void UBoostDashPlayerModule::UpdateBoostAirSteering()
{
	if ( !OwnerCharacter || !bIsBoosting ) return;

	// 空中のみ。地上は RequestMove が AddMovementInput( 前方向 )＋摩擦で速度を追従させるため不要
	if ( !OwnerCharacter->IsFalling() ) return;

	// チャージ側が自前で velocity を制御するため、主導権を握っている間は触らない
	if ( OwnerCharacter->IsCharging() || OwnerCharacter->IsPlayingChargeAction() ) return;

	UCharacterMovementComponent* Movement = OwnerCharacter->GetCharacterMovement();
	if ( !Movement ) return;

	// 「向き」はアクター前方向へ揃える。アクター自体は RequestMove が RotationRate（空中値）で
	// 入力方向へ回頭させているので、これで空中でも旋回力どおりに進行方向が変わる
	const FVector2D HorizontalVel( Movement->Velocity.X, Movement->Velocity.Y );
	const float HorizontalSpeed = HorizontalVel.Size();
	if ( HorizontalSpeed <= KINDA_SMALL_NUMBER ) return;

	const FVector Forward = OwnerCharacter->GetActorForwardVector().GetSafeNormal2D();
	if ( Forward.IsNearlyZero() ) return;

	// 「大きさ」は DecayStartRatio までは現在速度を維持し（ブレーキ減速の自然な挙動を壊さない）、
	// 過ぎたら減速開始時の速度から EaseOut で EndSpeed まで落とす（0 にしないので終了後も慣性が残る）
	const UTidePlayerParamDataAsset* PlayerParams = OwnerCharacter->PlayerParamData;
	const float DecayStartRatio = PlayerParams ? FMath::Clamp( PlayerParams->BoostDashAirDecayStartRatio, 0.0f, 0.99f ) : 0.5f;
	const float Progress = BoostTimer.GetRate();		// 0（開始）→ 1（終了）

	float TargetSpeed;
	if ( Progress <= DecayStartRatio )
	{
		// 維持区間：向きだけ前方向へ揃え、減速はまだ始めない
		TargetSpeed = HorizontalSpeed;
		AirDecayBaseSpeed = -1.0f;
	}
	else
	{
		// 減速区間：この区間に入った最初のフレームの速度を基準に EaseOut でフロア速度へ。
		// 基準速度が既にフロア以下なら減速しない（加速もしない）
		if ( AirDecayBaseSpeed < 0.0f )
		{
			AirDecayBaseSpeed = HorizontalSpeed;
		}
		const float EndSpeed = FMath::Min( PlayerParams ? PlayerParams->BoostDashAirDecayEndSpeed : 800.0f, AirDecayBaseSpeed );
		const float LocalProgress = ( Progress - DecayStartRatio ) / ( 1.0f - DecayStartRatio );
		const float SpeedMultiplier = UKismetMathLibrary::Ease( 1.0f, 0.0f, LocalProgress, EEasingFunc::EaseOut );
		TargetSpeed = FMath::Lerp( EndSpeed, AirDecayBaseSpeed, SpeedMultiplier );
	}

	Movement->Velocity.X = Forward.X * TargetSpeed;
	Movement->Velocity.Y = Forward.Y * TargetSpeed;
}

void UBoostDashPlayerModule::UpdateBoostGroundExit( float DeltaTime )
{
	if ( BoostGroundExitTimer.IsFinish() ) return;

	BoostGroundExitTimer.Update( DeltaTime );

	if ( !OwnerCharacter )
	{
		BoostGroundExitTimer.Clear();
		return;
	}

	// 空中へ出た／チャージへ移った／ダッシュが切れたら終了。これらは自前で速度・向きを制御するため競合する
	if ( OwnerCharacter->IsFalling() ||
		OwnerCharacter->IsCharging() ||
		OwnerCharacter->IsPlayingChargeAction() ||
		!OwnerCharacter->IsDashing() )
	{
		BoostGroundExitTimer.Clear();
		return;
	}

	UCharacterMovementComponent* Movement = OwnerCharacter->GetCharacterMovement();
	if ( !Movement ) return;

	// 慣性減速中は velocity の向きがロックされて自然に曲がれないため、「大きさ」はダッシュ側の減速に任せ、
	// 「向き」だけをアクター前方向へ揃える
	const FVector2D HorizontalVel( Movement->Velocity.X, Movement->Velocity.Y );
	const float HorizontalSpeed = HorizontalVel.Size();
	if ( HorizontalSpeed <= KINDA_SMALL_NUMBER ) return;

	const FVector Forward = OwnerCharacter->GetActorForwardVector().GetSafeNormal2D();
	if ( Forward.IsNearlyZero() ) return;

	Movement->Velocity.X = Forward.X * HorizontalSpeed;
	Movement->Velocity.Y = Forward.Y * HorizontalSpeed;
}

void UBoostDashPlayerModule::UpdateGravityLock( float DeltaTime )
{
	if ( GravityLockTimer.IsFinish() ) return;

	GravityLockTimer.Update( DeltaTime );

	// Velocity.Z を書き戻し続けるとチャージ側の LaunchCharacter による Z 初速が毎フレーム打ち消されるため、
	// チャージアクションへ進んだら垂直方向・重力の制御をそちらへ譲る
	const bool bChargeActionTookOver = OwnerCharacter && OwnerCharacter->IsPlayingChargeAction();

	if ( !bChargeActionTookOver )
	{
		if ( UCharacterMovementComponent* Movement = OwnerCharacter ? OwnerCharacter->GetCharacterMovement() : nullptr )
		{
			// 重力をカットし、上下速度を GravityLockZVelocity（既定 0）に固定する
			Movement->Velocity.Z = GravityLockZVelocity;
			Movement->GravityScale = 0.0f;
		}
	}

	// 終了した瞬間に通常の重力パラメータへ戻す
	if ( GravityLockTimer.IsFinish() && OwnerCharacter )
	{
		OwnerCharacter->RefreshMovementParams();
	}
}

void UBoostDashPlayerModule::EndBoost()
{
	if ( !bIsBoosting ) return;

	bIsBoosting = false;
	BoostMaxSpeed = 0.0f;
	BoostTimer.Clear();

	// 以降の分岐へ進む前に必ず Pop する。チャージダッシュ再開時は再開側が自前のカメラを Push し直す
	PopBoostDashCamera();

	if ( !OwnerCharacter )
	{
		return;
	}

	UCharacterMovementComponent* Movement = OwnerCharacter->GetCharacterMovement();

	// ブースト時間が短く ST 再生中に終わるケース。Loop は BlendSpace なので BS フラグも折る
	UAnimMontage* CurrentBoostMontage = OwnerCharacter->GetCurrentMontage();
	if ( CurrentBoostMontage == GetAnimMontage( PlayerAnimTags::CHARGE_03_DASH_ST ) )
	{
		OwnerCharacter->StopAnimMontage( CurrentBoostMontage );
	}
	OwnerCharacter->StopChargeDashLoopBlendSpace();

	// 中断していたチャージダッシュ／溜めを、通常ダッシュ・棒立ち ED より優先して再開する
	// （溜めは長押し継続時のみ、退避ギアのまま）。どちらもフレネル演出（共有 MPC パラメータ）の制御は
	// チャージ側へ譲る——本モジュールは ChargeActionPlayerModule_V2 より後に Tick されるため、
	// フェードアウトを続けると再開したチャージ側の点灯を上書きしてしまう
	if ( OwnerCharacter->ResumeChargeDashAfterBoost() )
	{
		CurrentFresnelIntensity = 0.0f;
		return;
	}
	if ( OwnerCharacter->ResumeChargeAfterBoost() )
	{
		CurrentFresnelIntensity = 0.0f;
		return;
	}

	// 滑空へ戻すのは滑空側（ジャンプ長押しの継続判定）に任せる

	// チャージが進行中ならそちらの状態機械へ制御を譲る（通常ダッシュ移行や ChargeDashEnd は競合する）。
	// RefreshMovementParams だけ呼び直し、IsBoostDashing() が外れた優先順位のパラメータへ切り替える
	if ( OwnerCharacter->IsCharging() || OwnerCharacter->IsPlayingChargeAction() )
	{
		OwnerCharacter->RefreshMovementParams();
		return;
	}

	// 空中終了は落下ループへ。水平速度は UpdateBoostAirSteering が後半でフロア速度まで EaseOut 済みなので
	// 消さずに引き継ぐ（0 にすると急停止する）。以降の地上向け分岐は摩擦前提なので通さない
	if ( OwnerCharacter->IsFalling() )
	{
		GravityLockTimer.Clear();	// 水平維持（重力カット）の解除
		OwnerCharacter->RefreshMovementParams();
		OwnerCharacter->EnterJumpFallingLoop();
		return;
	}

	// 移動入力があれば通常ダッシュへ移行（ダッシュ側が自前でパラメータを張る）
	if ( HasMovementInput() )
	{
		// 終了瞬間に速度を入力方向へ即スナップするとカクッと曲がるため、この時間は UpdateBoostGroundExit が
		// 向きだけを揃えて滑らかに曲げる。同時に Dash 側の Turn 検知も抑止されるので、逆方向入力でも
		// DashInertiaTimer が即クリアされず急制動にならない
		const float ExitTurnTime = OwnerCharacter->PlayerParamData ? OwnerCharacter->PlayerParamData->BoostDashGroundExitTurnTime : 0.0f;
		if ( ExitTurnTime > 0.0f )
		{
			BoostGroundExitTimer.Set( ExitTurnTime );
		}

		// 既にダッシュ中だと ForceStartDash→StartDash が早期 return で何もせず、上書きされた MaxWalkSpeed が
		// 戻らない。再始動せず RefreshMovementParams で戻し、慣性減速フェーズだけ張り直す
		if ( OwnerCharacter->IsDashing() )
		{
			const float CurrentSpeed = Movement ? Movement->Velocity.Size2D() : 0.0f;
			OwnerCharacter->RefreshMovementParams();
			OwnerCharacter->StartDashInertiaDecayFromSpeed( CurrentSpeed );
		}
		else
		{
			OwnerCharacter->ForceStartDash();
		}
		return;
	}

	// 棒立ち：残速はブレーキ減速度で落ちる
	UAnimMontage* DashEndMontage = GetAnimMontage( PlayerAnimTags::CHARGE_DASH_ED );
	if ( DashEndMontage && OwnerCharacter->GetCurrentMontage() != DashEndMontage )
	{
		PlayAnimMontage( PlayerAnimTags::CHARGE_DASH_ED );
	}
	OwnerCharacter->RefreshMovementParams();
}

// --- 残像（ゴーストトレイル）。ChargeEffectSubModule と同方式（色・素材はギア参固定）---

void UBoostDashPlayerModule::UpdateGhostTrails( float DeltaTime )
{
	const UTidePlayerParamDataAsset* PlayerParams = OwnerCharacter ? OwnerCharacter->PlayerParamData : nullptr;
	if ( !OwnerCharacter || !PlayerParams ) return;

	// 新規スポーンはブースト中のみ
	if ( bIsBoosting )
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

	for ( int32 i = ActiveGhostTrails.Num() - 1; i >= 0; --i )
	{
		FBoostGhostTrailData& Trail = ActiveGhostTrails[i];
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

void UBoostDashPlayerModule::SpawnGhostTrail()
{
	UMaterialInterface* GhostMaterial = GetBoostGhostTrailMaterial();
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

	FBoostGhostTrailData NewTrailData;
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

	if ( UWorld* World = OwnerCharacter->GetWorld() )
	{
		GhostMesh->RegisterComponentWithWorld( World );
	}

	ActiveGhostTrails.Add( NewTrailData );
}

UMaterialInterface* UBoostDashPlayerModule::GetBoostGhostTrailMaterial() const
{
	const UTidePlayerParamDataAsset* PlayerParams = OwnerCharacter ? OwnerCharacter->PlayerParamData : nullptr;
	if ( !PlayerParams ) return nullptr;

	// 素材は FadeAmount のみ受け取る単色発光で色は焼き付け済み。
	// フレネルと同じ黄緑にしたい場合は対応する素材を割り当てる
	if ( PlayerParams->BoostDashGhostTrailMaterial )
	{
		return PlayerParams->BoostDashGhostTrailMaterial;
	}

	return PlayerParams->ChargeActionGhostTrailMaterial;
}

// --- フレネルエフェクト。ChargeEffectSubModule と同方式（色は SlidePassive のトレイル色固定・点滅なし）---

void UBoostDashPlayerModule::UpdateFresnelEffect( float DeltaTime )
{
	if ( !OwnerCharacter || !OwnerCharacter->PlayerMPC || !OwnerCharacter->PlayerParamData ) return;

	static const FName ParamName_Intensity = TEXT( "FresnelIntensity" );
	static const FName ParamName_Color = TEXT( "FresnelColor" );

	// USlidePassivePlayerModule::TrailDefaultColor と同じ値
	static const FLinearColor BoostFresnelColor( 0.4f, 1.0f, 0.1f, 1.0f );

	// 未発動・フェードアウト完了後は共有 MPC パラメータに一切触れない。本モジュールは Modules 配列の最後尾で
	// ChargeEffectSubModule より後に呼ばれるため、無条件に書き込むと一度も発動していなくても毎フレーム 0 が入り、
	// 同じ FresnelIntensity を使うチャージダッシュのフレネル演出を消してしまう
	if ( !bIsBoosting && CurrentFresnelIntensity <= KINDA_SMALL_NUMBER )
	{
		return;
	}

	UMaterialParameterCollection* MPC = OwnerCharacter->PlayerMPC;
	UWorld* World = OwnerCharacter->GetWorld();

	if ( bIsBoosting )
	{
		CurrentFresnelIntensity = 1.0f;
		UKismetMaterialLibrary::SetVectorParameterValue( World, MPC, ParamName_Color, BoostFresnelColor );
	}
	else
	{
		const float FresnelFadeOutSpeed = OwnerCharacter->PlayerParamData->ChargeDashFresnelFadeOutSpeed;
		CurrentFresnelIntensity = FMath::FInterpTo( CurrentFresnelIntensity, 0.0f, DeltaTime, FresnelFadeOutSpeed );
	}

	UKismetMaterialLibrary::SetScalarParameterValue( World, MPC, ParamName_Intensity, CurrentFresnelIntensity );
}

float UBoostDashPlayerModule::GetInputThreshold() const
{
	if ( OwnerCharacter && OwnerCharacter->PlayerParamData )
	{
		return OwnerCharacter->PlayerParamData->DashCancelInputThreshold;
	}
	return 0.2f;
}

bool UBoostDashPlayerModule::HasMovementInput() const
{
	if ( !OwnerCharacter ) return false;
	return OwnerCharacter->GetRawMovementInput().Size() >= GetInputThreshold();
}

void UBoostDashPlayerModule::PushBoostDashCamera()
{
	// 再取得でブーストが延長されても既存カメラを維持する
	if ( BoostDashCameraHandle.IsValid() ) return;
	if ( !OwnerCharacter || !OwnerCharacter->PlayerParamData ) return;

	const FName CameraKey = OwnerCharacter->PlayerParamData->BoostDashCameraModeKey;
	if ( CameraKey.IsNone() ) return;

	if ( UExCameraSubsystem* CameraSubsystem = GetCameraSubsystem() )
	{
		BoostDashCameraHandle = CameraSubsystem->PushThirdPersonCameraByKey( CameraKey );
	}
}

void UBoostDashPlayerModule::PopBoostDashCamera()
{
	if ( !BoostDashCameraHandle.IsValid() ) return;

	if ( UExCameraSubsystem* CameraSubsystem = GetCameraSubsystem() )
	{
		CameraSubsystem->PopCameraMode( BoostDashCameraHandle );
	}
	BoostDashCameraHandle.Clear();
}

UExCameraSubsystem* UBoostDashPlayerModule::GetCameraSubsystem() const
{
	if ( const APlayerController* PC = Cast<APlayerController>( OwnerCharacter ? OwnerCharacter->GetController() : nullptr ) )
	{
		if ( ULocalPlayer* LocalPlayer = PC->GetLocalPlayer() )
		{
			return LocalPlayer->GetSubsystem<UExCameraSubsystem>();
		}
	}

	return nullptr;
}
