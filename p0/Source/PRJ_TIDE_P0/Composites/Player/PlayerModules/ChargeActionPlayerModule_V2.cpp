// Copyright (c) 2026, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.

#include "ChargeActionPlayerModule_V2.h"
#include "ChargeEffectSubModule.h"
#include "ChargeGuardBrakeSubModule.h"

#include <imgui.h>
#include <string>
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/KismetMaterialLibrary.h"
#include "Math/UnrealMathUtility.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraComponent.h"
#include "Components/PoseableMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "TimerManager.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/OverlapResult.h"
#include "KismetAnimationLibrary.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "Components/CapsuleComponent.h"
#include "Math/UnrealMathUtility.h"
#include "DrawDebugHelpers.h"

#include "PRJ_TIDE_P0/Actors/Character/Player/TidePlayerCharacter.h"
#include "PRJ_TIDE_P0/Actors/SlidePassive/SlidePassiveGustBurst.h"
#include "PRJ_TIDE_P0/Data/Player/TidePlayerParamDataAsset.h"
#include "PRJ_TIDE_P0/Data/Character/TideCharacterDataAsset.h"
#include "PRJ_TIDE_P0/Data/Combat/AttackParameterRow.h"
#include "PRJ_TIDE_P0/Data/Combat/DamageInfo.h"
#include "PRJ_TIDE_P0/Interfaces/Combat/IDamageable.h"
#include "PRJ_TIDE_P0/Utilities/TideCombatUtils.h"
#include "PRJ_TIDE_P0/Data/Animation/AnimMontageListDataAsset.h"
#include "PRJ_TIDE_P0/Data/Niagara/NiagaraSystemListDataAsset.h"
#include "PRJ_TIDE_P0/Data/Player/PlayerAnimTags.h"
#include "PRJ_TIDE_P0/Data/Player/PlayerNiagaraTags.h"
#include "PRJ_TIDE_P0/Data/Tags/TideGameplayTags.h"
#include "PRJ_TIDE_P0/Subsystems/Camera/ExCameraSubsystem.h"
#include "PRJ_TIDE_P0/Data/Camera/CameraModeParam/ExCameraModeParam.h"
#include "PRJ_TIDE_P0/Composites/Camera/CameraMode/AirChargeDashExCameraMode.h"
#include "PRJ_TIDE_P0/Components/Targeting/LockOnComponent.h"
#include "PRJ_TIDE_P0/Components/Targeting/LockOnTargetComponent.h"
#include "PRJ_TIDE_P0/Animation/Instances/Character/Player/PlayerAnimInstance.h"
#include "PRJ_TIDE_P0/Core/TideGameSettings.h"
#include "PRJ_TIDE_P0/Utilities/TargetingUtils.h"
#include "PRJ_TIDE_P0/Animation/Notifies/Combat/AnimNotifyState_CommonAttack.h"
#include "Animation/AnimMontage.h"

void UChargeActionPlayerModule_V2::Initialize( ATidePlayerCharacter* InOwner )
{
	Super::Initialize( InOwner );

	EffectSubModule = NewObject<UChargeEffectSubModule>( this );
	EffectSubModule->Initialize( this, InOwner );

	GuardBrakeSubModule = NewObject<UChargeGuardBrakeSubModule>( this );
	GuardBrakeSubModule->Initialize( this, InOwner );
}

void UChargeActionPlayerModule_V2::OnModuleUpdate( float DeltaTime )
{
	const UTidePlayerParamDataAsset* PlayerParams = GetPlayerParams();
	if ( !OwnerCharacter || !PlayerParams ) return;
	if ( !PlayerParams->bUseChargeV2 ) return;

	// --- 1. 継続ステートと物理・タイマーの更新 ---
	UpdateChargeKeptByDodge();
	UpdatePostActionChargeWindow( DeltaTime );	// 短縮チャージの猶予
	UpdateComboReset();							// 完全な通常状態に戻ったときだけ段数を戻す

	// ブースト中断中（bResumeChargeDashAfterBoost）は片付けない——再開後の継続ダッシュへ引き継ぐため
	if ( bGustBuffedActionActive && !IsGustCarryingChargeActionActive() && !bResumeChargeDashAfterBoost )
	{
		bGustBuffedActionActive = false;
		BeginFadeOutActiveGustBurst();
	}
	// 未消費の突風バフは「溜め中／そこから派生したチャージアクション中」だけ有効。風まとい・足元印は arm を条件に
	// 表示しているため、キャンセル経路（被弾・回避・落下ダメージ着地・打ち上げ・面沿い離脱・ガードブレーキ・
	// 空中キャンセル・竜巻ジャンプ …）で arm が残ると印が出たままになり、次のアクションで暴発する。
	// 個別のキャンセル箇所ではなくここで一括して落とす（CancelCharge 側には入れられない——
	// ダッシュ→チャージ攻撃／幅跳びの正規派生でも通るため）
	if ( bGustChargeBuffArmed && !bIsCharging && !IsGustCarryingChargeActionActive()
		&& !bResumeChargeHoldAfterBoost && !bResumeChargeDashAfterBoost )
	{
		ClearGustChargeBuff();
	}
	UpdateGustBurstFadeOut( DeltaTime );
	UpdateGravityLock( DeltaTime );				// 空中アクション時の水平維持
	UpdateGodArtStanceChargeFreeze();			// 構え中に凍結保持した溜めを構えの終わりで解決する
	UpdateChargeShift( DeltaTime );				// 長押しによるギア段階の自動シフト
	// 判定に使う HitCancelChargingSteeringTimer を減算する UpdateChargingState より前に置くこと
	// （後だと窓の最終フレームで先にゲートが閉じて発火しない）
	UpdateHitbackChargeGearUp( DeltaTime );
	UpdateChargingState( DeltaTime );			// ヒットキャンセル時のステアリング制限を含む
	HitCancelChargeActionLockTimer.Update( DeltaTime );			// 実時間で消化する
	AirChargeAttackDiveAttackCooldownTimer.Update( DeltaTime );
	HitbackComboIntervalTimer.Update( DeltaTime );
	UpdatePendingHitbackComboAttack();
	UpdateDriftBoostState( DeltaTime );			// 単一ソース。演出より先に更新する
	UpdateDriftGearUp( DeltaTime );
	if ( EffectSubModule ) EffectSubModule->UpdateChargeBoostEffect( DeltaTime );
	UpdateDriftAttack( DeltaTime );				// 火花演出と同じ bIsDriftBoostActive を単一ソースに発生させる
	UpdatePropulsionLock( DeltaTime );			// 突進アクション中の移動ロックと強制推進力
	UpdateFrictionRecoveryState( DeltaTime );	// アクション終了後の地面摩擦（スライド感）の復帰補間
	UpdateAirDashEndInertia( DeltaTime );
	UpdateChargeHopSpeedMaintain( DeltaTime );
	EffectSubModule->UpdateSpawnedChargeEffectRotation();
	UpdateStaleChargeHoldRelease();				// 入力判定より前に行うこと

	// --- 2. 入力とアクション開始判定 ---
	// 構え選択中は通常アクションを許すので IsGodActionExecuting で見る。
	// 死亡時は CancelCharge しても bChargeInputHeld が残るため、長押し継続からの BeginCharge もここで止める
	if ( !OwnerCharacter->IsGodActionExecuting() && !OwnerCharacter->IsDead() )
	{
		// コマンドディスパッチが走らなかったフレームのみ、新規チャージ開始を判定する
		if ( !UpdateInputCommands() )
		{
			UpdateChargeBeginCheck();
		}
	}

	// --- 3. ステート・アニメーションの監視 ---
	UpdateChargeAnimState( DeltaTime );				// 地上チャージの ST -> BS 移行
	UpdateChargeJumpAnimState( DeltaTime );			// ジャンプ中の上昇/滞空/着地の状態遷移
	UpdateAirChargeAttackAnimState( DeltaTime );	// ST -> LP
	UpdateAirChargeAttackDive( DeltaTime );			// AnimState の後＝phase 確定後に速度を当てる
	UpdateAirChargeAttackEndCheck();				// タグによる終了監視
	UpdateAirChargeAttackEdAirborneLog();			// 計測のみ
	UpdateAirborneTransitionLog();					// 計測のみ
	if ( EffectSubModule ) EffectSubModule->UpdateGhostTrails( DeltaTime );
	if ( EffectSubModule ) EffectSubModule->UpdateSkidEffects( DeltaTime );
	if ( EffectSubModule ) EffectSubModule->UpdateHitbackSkidEffects( DeltaTime, bIsHitCancelableToCharge );
	UpdateChargeDashMontageState( DeltaTime );		// 滞空状態に応じた Montage/BlendSpace 再生制御
	UpdateChargeDashEndCancel();
	UpdateChargeHopEndCheck();
	UpdateAirChargeDashSwingCamera( DeltaTime );
	UpdateReservedDashOnLanding();
	if ( EffectSubModule ) EffectSubModule->UpdateFresnelEffect( DeltaTime );
	if ( bIsCharging && GetPlayerParams()->bEnableChargingGroundSnapping )
	{
		ApplyGroundSnap();
	}
	if ( GuardBrakeSubModule ) GuardBrakeSubModule->Update( DeltaTime );

	// 通常の Pop は終了・キャンセル時（ResetChargedActionState）。
	// 地上チャージ攻撃の出し切り後と幅跳びの着地後だけ、ここで遅延 Pop を進める
	UpdateDeferredChargeActionCameraPop( DeltaTime );
}

void UChargeActionPlayerModule_V2::UpdateComboReset()
{
	if ( !OwnerCharacter ) return;

	if ( ShouldSkipComboReset() ) return;
	if ( ShouldPreserveChargeComboState() ) return;

	ResetChargeComboAndGearState();
}

void UChargeActionPlayerModule_V2::UpdatePostActionChargeWindow( float DeltaTime )
{
	const UTidePlayerParamDataAsset* PlayerParams = GetPlayerParams();
	if ( !OwnerCharacter || !PlayerParams ) return;

	if ( ShouldClearPostActionChargeWindowForNormalAttack() )
	{
		PostActionChargeTimer.Clear();
		return;
	}

	if ( IsPostActionChargeWindowActive() )
	{
		PostActionChargeTimer.Set( PlayerParams->PostActionChargeWindowTime );
	}
	else
	{
		PostActionChargeTimer.Update( DeltaTime );
	}
}

void UChargeActionPlayerModule_V2::UpdateChargeJumpAnimState( float DeltaTime )
{
	if ( !OwnerCharacter ) return;

	// ジャンプ中もチャージダッシュの残り持続時間を消費し続ける
	if ( bResumeChargeDashOnLanding && ResumeChargeDashRemainingTime > 0.0f )
	{
		ResumeChargeDashRemainingTime -= DeltaTime;
	}

	// 上昇から落下への自動遷移（ST -> LP）
	if ( CurrentChargeActionType == EChargeActionV2Type::Jump && !bIsChargeJumpInLoop )
	{
		// 発動直後は AnimInstance の更新遅れで GetCurrentMontage() が ST 再生前の古い値を返す
		if ( ChargedActionLockTimer.GetElapsed() < 0.1f )
		{
			return;
		}

		if ( IsEffectivelyInAir() && HasChargeJumpStartMontageFinished() )
		{
			TransitionChargeJumpToLoop();
		}
	}
}

void UChargeActionPlayerModule_V2::UpdateChargeDashMontageState( float DeltaTime )
{
	if ( !OwnerCharacter ) return;
	// フラグ解除の集約点は ResetChargedActionState（ブーストダッシュも同じ BS フラグを立てるためここでは折らない）
	if ( !IsPlayingChargeDash() ) return;
	if ( bIsReboundSliding ) { StopChargeDashBlendSpace(); return; }

	UAnimMontage* DashStartMontage = GetAnimMontage( GetChargeDashStartAnimTag() );
	UAnimMontage* JumpLoopMontage = GetAnimMontage( PlayerAnimTags::JUMP_LP );
	UAnimMontage* CurrentMontage = OwnerCharacter->GetCurrentMontage();

	// ターンモーション再生中は、ダッシュループ等の再生でターンを上書きしないよう一切触らない
	if ( CurrentMontage == GetAnimMontage( PlayerAnimTags::CHARGE_DASH_TURN ) ) { StopChargeDashBlendSpace(); return; }

	// 空中は専用の ST モンタージュ＋落下ループで表現するため、地上用ダッシュ Loop の BS は止める
	if ( bIsAirChargeDash )
	{
		StopChargeDashBlendSpace();
		if ( !IsEffectivelyInAir() ) return;	// 着地は OnLanded が処理

		// ST が終わり次第ロック満了を待たず落下ループへ移し、無モーションの空白を防ぐ。
		// JumpActionPlayerModule は抑止中なのでモーションだけ先に繋ぎ、Loop 状態の引き渡しは OnEndAction が行う
		if ( CurrentMontage != DashStartMontage && CurrentMontage != JumpLoopMontage )
		{
			PlayAnimMontage( PlayerAnimTags::JUMP_LP );
		}
		return;
	}

	if ( IsEffectivelyInAir() )
	{
		StopChargeDashBlendSpace();

		// 面沿い（天井・壁）から降りた落下はチャージジャンプのモーションで見せる。
		// ST→LP をここで回すのは、ダッシュ継続中は JumpActionPlayerModule がモーション制御を中断するため
		if ( bDashFallChargeJumpMotion )
		{
			if ( !bDashFallChargeJumpStStarted )
			{
				// 直後は GetCurrentMontage() が古い値を返すため、再生開始を観測するまで終了判定を始めない
				bDashFallChargeJumpStStarted = ( CurrentMontage == GetAnimMontage( PlayerAnimTags::CHARGE_JUMP_ST ) );
			}
			// タグを直接見る（HasChargeJumpStartMontageFinished はホップ用タグへ分岐し、
			// 幅跳び継続時は bIsChargeHopJump が残るため）
			else if ( !bDashFallChargeJumpInLoop && HasMontageFinished( PlayerAnimTags::CHARGE_JUMP_ST ) )
			{
				PlayAnimMontage( PlayerAnimTags::CHARGE_JUMP_LP );
				bDashFallChargeJumpInLoop = true;
			}
			return;
		}

		if ( CurrentMontage != DashStartMontage && CurrentMontage != JumpLoopMontage )
		{
			PlayAnimMontage( PlayerAnimTags::JUMP_LP );
			OwnerCharacter->ReserveAutoDashOnLanding();
		}
		return;
	}

	// 着地。JumpActionPlayerModule の OnLanded が先に CHARGE_JUMP_ED ＋着地オートダッシュ予約を張るため、
	// 放置すると通常ダッシュへ化けて「ED のまま走る」絵になる。ステートと予約はここで返してもらう
	if ( bDashFallChargeJumpMotion )
	{
		bDashFallChargeJumpMotion = false;
		bDashFallChargeJumpStStarted = false;
		bDashFallChargeJumpInLoop = false;

		OwnerCharacter->ClearJumpAndLandingDash();
		OwnerCharacter->RefreshMovementParams();

		const UTidePlayerParamDataAsset* PlayerParams = GetPlayerParams();
		const float LandingEdTime = PlayerParams ? PlayerParams->SurfaceRideFallLandingEdTime : 0.0f;
		if ( LandingEdTime > 0.0f && CurrentMontage == GetAnimMontage( PlayerAnimTags::CHARGE_JUMP_ED ) )
		{
			bDashFallChargeJumpLanding = true;
			DashFallLandingEdTimer.Set( LandingEdTime );
		}
		else
		{
			// ED を見せない設定、または ED が張られていない（他モーションに取られた）ケース
			StopFinishedChargeJumpMontages();
			CurrentMontage = nullptr;
		}
	}

	// ED を見せている間は BS を張らない（ダッシュは続くので指定時間で切り上げてブレンドする）
	if ( bDashFallChargeJumpLanding )
	{
		UAnimMontage* ChargeJumpEdMontage = GetAnimMontage( PlayerAnimTags::CHARGE_JUMP_ED );
		DashFallLandingEdTimer.Update( DeltaTime );

		// 時間切れ、または ED が別モーションへ差し替わったら終わり
		if ( !DashFallLandingEdTimer.IsFinish() && CurrentMontage == ChargeJumpEdMontage ) return;

		if ( CurrentMontage == ChargeJumpEdMontage )
		{
			static constexpr float LandingEdBlendOutTime = 0.15f;
			OwnerCharacter->StopAnimMontage( LandingEdBlendOutTime, ChargeJumpEdMontage );
		}
		bDashFallChargeJumpLanding = false;
		DashFallLandingEdTimer.Clear();
		CurrentMontage = nullptr;
	}

	// 着地時にジャンプ用のループ（LP）が残っていればクリアする
	if ( CurrentMontage == JumpLoopMontage )
	{
		static constexpr float JumpLoopBlendOutTime = 0.5f;
		OwnerCharacter->StopAnimMontage( JumpLoopBlendOutTime, JumpLoopMontage );
		CurrentMontage = nullptr;
	}

	if ( bAllowDashLoop )
	{
		// Set 直後は GetCurrentMontage() が古い値を返すため、OnEndAction（ED へ逆戻り）の誤爆を防ぐ
		if ( ChargedActionLockTimer.GetElapsed() < 0.1f )
		{
			return;
		}

		// ST が終わった以降はダッシュ Loop フェーズ。Loop は BlendSpace で表現するので LP モンタージュは再生しない
		if ( CurrentMontage != DashStartMontage )
		{
			const FVector2D RawInput = OwnerCharacter->GetRawMovementInput();
			const float InputThreshold = OwnerCharacter->PlayerParamData ? OwnerCharacter->PlayerParamData->DashCancelInputThreshold : 0.2f;

			if ( RawInput.Size() < InputThreshold )
			{
				// 突進タイマーを強制終了して OnEndAction() へ流し、ED を再生させる
				StopChargeDashBlendSpace();
				ChargedActionLockTimer.Clear();
				OnEndAction();
				return;
			}
			else
			{
				UpdateChargeDashBlendSpace( DeltaTime );
			}
		}
	}
}

void UChargeActionPlayerModule_V2::UpdateChargeHopEndCheck()
{
	if ( !OwnerCharacter || CurrentChargeActionType != EChargeActionV2Type::Jump || !bIsChargeHopJump ) return;

	UAnimMontage* EdMontage = GetAnimMontage( PlayerAnimTags::CHARGE_HOP_ED );
	UAnimMontage* CurrentMontage = OwnerCharacter->GetCurrentMontage();

	if ( CurrentMontage != nullptr && CurrentMontage == EdMontage )
	{
		bChargeHopEdMontageStarted = true;

		// ジャンプ入力で ED をキャンセルして連続チャージ幅跳びへ繋ぐ（CanJump() は IsPlayingChargeJump() 中を弾くため直接処理する）。
		// 無条件に継続すると着地→ジャンプ連打で無限に跳べるので、残り時間が無ければ通常ジャンプで終わらせる
		const UTidePlayerParamDataAsset* PlayerParams = GetPlayerParams();
		const float JumpBufferTime = PlayerParams ? PlayerParams->JumpBufferTime : 0.1f;
		if ( TryConsumeCommand( TAG_Input_Command_Jump, JumpBufferTime ) )
		{
			// 滞空がほぼ 0 の地形（上り坂）では実時間の消費もほぼ 0 なので、再ホップに固定コストと回数上限を課す
			const float HopTimeCost = PlayerParams ? FMath::Max( 0.0f, PlayerParams->ChargeHopTimeCostPerHop ) : 0.0f;
			const int32 MaxChainCount = PlayerParams ? PlayerParams->ChargeHopMaxChainCount : 0;
			const bool bChainCountAvailable = MaxChainCount <= 0 || ChargeHopChainCount < MaxChainCount;
			const float RemainingResumeTime = ResumeChargeDashRemainingTime - HopTimeCost;
			const bool bCanContinueHop = bResumeChargeDashOnLanding && RemainingResumeTime > 0.0f && bChainCountAvailable;
			// 下の OnEndAction（→ResetChargedActionState）が連続回数もクリアするため、引き継ぐぶんを退避する
			const int32 ChainCountBeforeRehop = ChargeHopChainCount;

			// 通常ジャンプへ落とす場合は OnEndAction で Pop させたいので先にフラグを下ろす
			if ( !bCanContinueHop )
			{
				bKeepDashCameraDuringHop = false;
			}

			OwnerCharacter->StopAnimMontage( 0.1f, EdMontage );
			OnEndAction();

			if ( bCanContinueHop )
			{
				// 同じチャージホップとして即座に再ジャンプし、ダッシュ継続予約を引き継ぐ
				OnStartChargeJump( true );
				bResumeChargeDashOnLanding = true;
				ResumeChargeDashRemainingTime = RemainingResumeTime;
				ChargeHopChainCount = ChainCountBeforeRehop + 1;
			}
			else
			{
				OwnerCharacter->ForceJump( false );
			}
			return;
		}

		// CanMove 中は ED を止めず移動速度だけダッシュ速度へ合わせる（MaxWalkSpeed のままだと着地直後に動きが詰まって見える）
		if ( OwnerCharacter->HasStateTag( TAG_State_Player_CanMove ) )
		{
			if ( UCharacterMovementComponent* MovementComp = OwnerCharacter->GetCharacterMovement() )
			{
				MovementComp->MaxWalkSpeed = CachedChargeDashSpeed;
			}
		}

		// 末尾に達した時点（まだフルウェイトの間）で判定する。GetCurrentMontage() が ED を指さなくなるまで
		// 待つと、ブレンドアウト中に一度ロコモーションへ戻って絵が崩れる
		if ( UAnimInstance* AnimInst = OwnerCharacter->GetMesh()->GetAnimInstance() )
		{
			const float CurrentPos = AnimInst->Montage_GetPosition( EdMontage );
			const float Length = EdMontage->GetPlayLength();
			if ( CurrentPos >= ( Length - 0.05f ) )
			{
				ResolveChargeHopLandingDecision();
			}
		}
		return;
	}

	// 再生開始を一度も観測していない間（PlayAnimMontage 直後の AnimInstance 更新遅れ）は誤爆を避けて待つ
	if ( !bChargeHopEdMontageStarted ) return;

	// 上のブレンドアウト前判定で拾いきれなかった場合のフォールバック
	ResolveChargeHopLandingDecision();
}

void UChargeActionPlayerModule_V2::ResolveChargeHopLandingDecision()
{
	bChargeHopEdMontageStarted = false;

	const float InputThreshold = OwnerCharacter->PlayerParamData ? OwnerCharacter->PlayerParamData->DashCancelInputThreshold : 0.2f;
	const bool bHasMoveInput = OwnerCharacter->GetRawMovementInput().Size() >= InputThreshold;

	if ( bHasMoveInput )
	{
		// 継続できる持続時間が無ければ ResumeChargeDashAfterLanding 内で通常終了へフォールバックする
		ResumeChargeDashAfterLanding();
	}
	else
	{
		// ダッシュへ継続しないので、維持していたチャージダッシュカメラを Pop させる
		bKeepDashCameraDuringHop = false;
		bResumeChargeDashOnLanding = false;
		ResumeChargeDashRemainingTime = 0.0f;
		OnEndAction();
	}
}

FName UChargeActionPlayerModule_V2::MakeChargeActionCameraKey( const TCHAR* Prefix ) const
{
	// ギア4以上（極）は用意しているギア3カメラに寄せる
	const int32 Gear = FMath::Clamp( CurrentChargeGearIndex, 1, 3 );
	return FName( *FString::Printf( TEXT( "%sGear%d" ), Prefix, Gear ) );
}

void UChargeActionPlayerModule_V2::SetChargeActionCamera( const FName& RowName )
{
	UExCameraSubsystem* CameraSubsystem = GetCameraSubsystem();
	if ( !CameraSubsystem ) return;

	// 同じキーなら Pop/再 Push せず維持する（毎回張り替えると Pop→Push のブレンドでガクつく）
	if ( ChargeActionCameraHandle.IsValid() && RowName == CurrentChargeActionCameraKey )
	{
		bPendingChargeActionCameraPop = false;
		ChargeActionCameraHoldTimer.Clear();
		return;
	}

	// 直前のカメラはこのアクションが引き継ぐので、遅延 Pop 予約が残っていれば破棄する
	bPendingChargeActionCameraPop = false;
	ChargeActionCameraHoldTimer.Clear();

	if ( ChargeActionCameraHandle.IsValid() )
	{
		CameraSubsystem->PopCameraMode( ChargeActionCameraHandle );
		ChargeActionCameraHandle.Clear();
	}

	ChargeActionCameraHandle = CameraSubsystem->PushThirdPersonCameraByKey( RowName );
	CurrentChargeActionCameraKey = RowName;
}

void UChargeActionPlayerModule_V2::PopChargeActionCamera()
{
	// どの経路から Pop されても遅延 Pop 予約はここで終了させる（二重制御・取りこぼし防止）
	bPendingChargeActionCameraPop = false;
	ChargeActionCameraHoldTimer.Clear();

	if ( !ChargeActionCameraHandle.IsValid() )
	{
		CurrentChargeActionCameraKey = NAME_None;
		return;
	}
	if ( UExCameraSubsystem* CameraSubsystem = GetCameraSubsystem() )
	{
		CameraSubsystem->PopCameraMode( ChargeActionCameraHandle );
	}
	ChargeActionCameraHandle.Clear();
	CurrentChargeActionCameraKey = NAME_None;
}

void UChargeActionPlayerModule_V2::UpdateDeferredChargeActionCameraPop( float DeltaTime )
{
	// 時間内に次のチャージアクションが同じキーを要求すれば、SetChargeActionCamera が予約を解除して維持する
	if ( !bPendingChargeActionCameraPop ) return;

	// ヒットバック連続コンボの予約中は次のチャージ攻撃が控えているので Pop を保留する。
	// インターバルがホールド時間より長くても、通常カメラへ戻る→再 Push のブレンドが起きないようにする
	if ( bPendingChargeComboAttack ) return;

	ChargeActionCameraHoldTimer.Update( DeltaTime );
	if ( ChargeActionCameraHoldTimer.IsFinish() )
	{
		PopChargeActionCamera();	// 予約フラグ／タイマーもここでクリアされる
	}
}

void UChargeActionPlayerModule_V2::StartAirChargeDashSwingCamera()
{
	if ( !OwnerCharacter || !OwnerCharacter->PlayerParamData ) return;

	// デバッグフラグ（既定 OFF）が有効なときだけ専用の寄せカメラへ切り替える
	if ( !UTideGameSettings::Get()->bDebugFlagEnableAirChargeDashCamera ) return;

	UExCameraModeParam* CamParam = OwnerCharacter->AirChargeDashCameraParam;
	if ( !CamParam || !CamParam->ModeClass ) return;

	UExCameraSubsystem* CameraSubsystem = GetCameraSubsystem();
	if ( !CameraSubsystem ) return;

	// カメラ調整数値は専用モード側に集約しているので、CDO（＝BP 既定値）から読む
	const UAirChargeDashExCameraMode* ModeCDO = CamParam->ModeClass->GetDefaultObject<UAirChargeDashExCameraMode>();
	if ( !ModeCDO ) return;

	// Yaw は直進方向（水平）へ完全に合わせる。Pitch はダッシュ方向が水平（Pitch≒0）で見下ろしが
	// 作れないため、明示角 DivePitchDegrees × 適用率で作る
	FVector DiveDir = CachedChargeDashDirection.GetSafeNormal();
	if ( DiveDir.IsNearlyZero() )
	{
		DiveDir = OwnerCharacter->GetActorForwardVector().GetSafeNormal();
	}

	const FRotator DiveRot = DiveDir.Rotation();
	FRotator TargetRot( 0.0f, DiveRot.Yaw, 0.0f );
	TargetRot.Pitch = ModeCDO->DivePitchDegrees * FMath::Clamp( ModeCDO->PitchAlignRate, 0.0f, 1.0f );

	// 実カメラの Yaw が目標構図から許容角を超えて離れていれば寄せ演出をしない（大振りなスイングを避ける）。
	// Pitch は日常的にズレるので見ない。専用カメラ表示中（張り替え）は判定しない——
	// スキップすると目標回転だけ古いまま残るため
	if ( !AirChargeDashSwingCameraHandle.IsValid() && ModeCDO->SwingStartMaxYawDiffDegrees < 180.0f )
	{
		const APlayerController* PC = Cast<APlayerController>( OwnerCharacter->GetController() );
		if ( PC && PC->PlayerCameraManager )
		{
			const float YawDiff = FMath::Abs( FMath::FindDeltaAngleDegrees(
				PC->PlayerCameraManager->GetCameraRotation().Yaw, TargetRot.Yaw ) );
			if ( YawDiff > ModeCDO->SwingStartMaxYawDiffDegrees )
			{
				return;
			}
		}
	}

	OwnerCharacter->SetAirChargeDashCameraTarget( TargetRot );

	// 既に張っていれば張り替えない（多重 Push 防止）
	if ( !AirChargeDashSwingCameraHandle.IsValid() )
	{
		AirChargeDashSwingCameraHandle = CameraSubsystem->PushCameraMode( CamParam );
	}

	AirChargeDashSwingTimer.Set( FMath::Max( 0.0f, ModeCDO->SwingDuration ) );
}

void UChargeActionPlayerModule_V2::UpdateAirChargeDashSwingCamera( float DeltaTime )
{
	if ( !AirChargeDashSwingCameraHandle.IsValid() ) return;

	AirChargeDashSwingTimer.Update( DeltaTime );
	if ( AirChargeDashSwingTimer.IsFinish() )
	{
		// ダッシュ終了まで保持するモード（既定）では寄せ完了後も Pop しない——通常 TPS は縦デッドゾーンで
		// 高速落下を置いていくため、ダイブ途中で返すと画面下に見切れる（Pop は ResetChargedActionState）
		if ( OwnerCharacter && OwnerCharacter->AirChargeDashCameraParam && OwnerCharacter->AirChargeDashCameraParam->ModeClass )
		{
			if ( const UAirChargeDashExCameraMode* ModeCDO =
				OwnerCharacter->AirChargeDashCameraParam->ModeClass->GetDefaultObject<UAirChargeDashExCameraMode>() )
			{
				if ( ModeCDO->bHoldUntilDashEnd )
				{
					// 寄せ完了の瞬間に一度だけ ControlRotation をダイブ角へ同期してからロックを解除する
					if ( OwnerCharacter->IsAirChargeDashCameraSwinging() )
					{
						FRotator TargetRot;
						if ( OwnerCharacter->GetAirChargeDashCameraFraming( TargetRot ) )
						{
							if ( APlayerController* PC = Cast<APlayerController>( OwnerCharacter->GetController() ) )
							{
								PC->SetControlRotation( TargetRot );
							}
						}
						OwnerCharacter->SetAirChargeDashCameraInputLocked( false );
					}
					return;
				}
			}
		}

		// 寄せ切った：ControlRotation を目標へ同期してから Pop → 通常カメラがその向きから引き継ぐ（操作へ返す）
		StopAirChargeDashSwingCamera( true );
	}
}

void UChargeActionPlayerModule_V2::StopAirChargeDashSwingCamera( bool bSyncControlRotation )
{
	if ( !OwnerCharacter ) return;

	// 目標（ダイブ背後）へ同期してから Pop すると通常カメラがスイングバックしない。ただし寄せ完了後は
	// ユーザーが操作しているため同期し直すとスナップバックする。ロック中（＝まだ寄せ演出中）のみ同期する
	if ( bSyncControlRotation && OwnerCharacter->IsAirChargeDashCameraSwinging() )
	{
		FRotator TargetRot;
		if ( OwnerCharacter->GetAirChargeDashCameraFraming( TargetRot ) )
		{
			if ( APlayerController* PC = Cast<APlayerController>( OwnerCharacter->GetController() ) )
			{
				PC->SetControlRotation( TargetRot );
			}
		}
	}

	if ( AirChargeDashSwingCameraHandle.IsValid() )
	{
		if ( UExCameraSubsystem* CameraSubsystem = GetCameraSubsystem() )
		{
			CameraSubsystem->PopCameraMode( AirChargeDashSwingCameraHandle );
		}
		AirChargeDashSwingCameraHandle.Clear();
	}

	AirChargeDashSwingTimer.Clear();
	OwnerCharacter->ClearAirChargeDashCameraFraming();	// 入力ロック解除
}

bool UChargeActionPlayerModule_V2::UpdateInputCommands()
{
	const UTidePlayerParamDataAsset* PlayerParams = GetPlayerParams();
	if ( !OwnerCharacter || !PlayerParams ) return false;

	// Jump を消費すると HitReaction のキャンセル入力を奪い、チャージジャンプが暴発する
	if ( OwnerCharacter->IsHitReacting() ) return false;

	ConsumeBufferedChargeInput( PlayerParams->ChargeBufferTime );

	// 予約済みでインターバル消化中は攻撃入力を飲み込み、優先権が落ちる区間でも通常攻撃へ漏らさない
	// （実際の発動はインターバル明けに UpdatePendingHitbackComboAttack が行う）
	if ( bPendingChargeComboAttack && !bChargeInputHeld && IsHitbackComboIntervalActive() )
	{
		TryConsumeCommand( TAG_Input_Command_Attack, PlayerParams->AttackBufferTime );
	}

	if ( !IsChargePriorityActive() )
	{
		return false;
	}

	// 空中チャージダッシュ中は、地上のような割り込み（ジャンプ／攻撃ボタンでの派生）を許可しない
	if ( CurrentChargeActionType == EChargeActionV2Type::Dash && bIsAirChargeDash )
	{
		return false;
	}

	// 以下 2 つはコマンドを消費せずに抜けることで、通常ジャンプ・振り下ろしへそのまま流れる
	if ( OwnerCharacter->IsAirActionLimitedAfterAirCharge() )
	{
		return false;
	}
	if ( OwnerCharacter->IsGodArtSelecting() )
	{
		return false;
	}

	return DispatchChargePriorityCommands( PlayerParams );
}

bool UChargeActionPlayerModule_V2::IsChargeJumpBlockedAfterJump() const
{
	const UTidePlayerParamDataAsset* PlayerParams = GetPlayerParams();
	if ( !OwnerCharacter || !PlayerParams || !PlayerParams->bBlockChargeJumpAfterNormalJump ) return false;

	return OwnerCharacter->HasJumpedThisAirtime();
}

bool UChargeActionPlayerModule_V2::HandleChargePriorityJumpInput( float JumpBufferTime )
{
	// ジャンプ後の空中ではチャージジャンプへ派生させない。コマンドを消費せずに抜けることで、
	// 多段ジャンプが残っていれば JumpActionPlayerModule が通常ジャンプとして拾う
	if ( IsChargeJumpBlockedAfterJump() )
	{
		return false;
	}

	if ( !TryConsumeCommand( TAG_Input_Command_Jump, JumpBufferTime ) )
	{
		return false;
	}

	// CanJump タグ中（チャージ攻撃をジャンプで抜ける窓）は実行中の攻撃を破棄してジャンプする
	if ( OwnerCharacter && OwnerCharacter->IsAttacking() &&
		OwnerCharacter->HasStateTag( TAG_State_Player_CanJump ) &&
		OwnerCharacter->TryJumpCancelDuringAction() )
	{
		return true;
	}

	return RequestChargeJump();
}

bool UChargeActionPlayerModule_V2::TryChargeAttackCancelJump()
{
	if ( !OwnerCharacter ) return false;

	// 押し直し待ちで長押しが無効な間は「離している」扱い（ここは物理押下を直接見るため個別に弾く）
	const bool bChargeHeld = bChargeInputPhysicallyHeld && !IsChargeHoldStale();

	// ギアは保持。攻撃モンタージュの停止は呼び出し先の CancelAttack が行う
	InterruptCurrentChargeAction();

	// R2 を離している／ジャンプ後の空中／空中チャージ使い切り／構え中なら通常ジャンプへ返す
	if ( !bChargeHeld || IsChargeJumpBlockedAfterJump() ||
		OwnerCharacter->IsAirActionLimitedAfterAirCharge() ||
		OwnerCharacter->IsGodArtSelecting() ) return false;

	// 溜め直後で charge が無くても現在ギアで発動する
	OnStartChargeJump();
	ClearChargeState();
	return true;
}

bool UChargeActionPlayerModule_V2::HandleChargePriorityAttackInput( float AttackBufferTime )
{
	if ( !CanStartChargeAttackFromCurrentState() )
	{
		return false;
	}

	if ( !TryConsumeCommand( TAG_Input_Command_Attack, AttackBufferTime ) )
	{
		return false;
	}

	return RequestChargeAttack();
}

bool UChargeActionPlayerModule_V2::HandleChargeComboAttackInput( float AttackBufferTime )
{
	const UTidePlayerParamDataAsset* PlayerParams = GetPlayerParams();
	if ( !OwnerCharacter || !PlayerParams ) return false;
	if ( !CanStartChargeAttackComboFromCurrentState() ) return false;
	if ( !TryConsumeCommand( TAG_Input_Command_Attack, AttackBufferTime ) ) return false;

	// インターバル中（R2 非ホールド）に CanCombo 窓で来た攻撃は即時発動せず予約する
	if ( !bChargeInputHeld && IsHitbackComboIntervalActive() )
	{
		bPendingChargeComboAttack = true;
		return true;
	}

	if ( CurrentChargeComboIndex == PlayerParams->MaxChargeComboCount && !bChargeInputHeld )
	{
		CancelCharge( false );
		OwnerCharacter->CancelAttack();

		CurrentChargeComboIndex = 1;
		CurrentChargeGearIndex = 1;

		OnStartLightAttack();
		if ( EffectSubModule ) EffectSubModule->ResetGhostTrailSpawnTimer();
		return true;
	}

	return RequestChargeAttack();
}

void UChargeActionPlayerModule_V2::ConsumeBufferedChargeInput( float ChargeBufferTime )
{
	if ( TryConsumeCommand( TAG_Input_Command_ChargeAction, ChargeBufferTime ) )
	{
		bChargeInputHeld = true;
		// 新規押下＝自主的な溜め直しなので、押し直し待ちを解除する
		bRequireChargeRepress = false;
		bRequireChargeRepressByEvent = false;
	}
}

bool UChargeActionPlayerModule_V2::DispatchChargePriorityCommands( const UTidePlayerParamDataAsset* PlayerParams )
{
	if ( !PlayerParams )
	{
		return false;
	}

	if ( HandleChargePriorityJumpInput( PlayerParams->JumpBufferTime ) )
	{
		return true;
	}

	// 優先権はコンボ段・ギア由来でも立つため、ラッチが落ちていてもここを抜けないとチャージ攻撃がさらう
	if ( IsChargeJumpNormalAttackWindow() )
	{
		return false;
	}

	if ( IsAttackButtonChargeComboEnabled() )
	{
		return HandleChargeComboAttackInput( PlayerParams->AttackBufferTime );
	}

	return HandleChargePriorityAttackInput( PlayerParams->AttackBufferTime );
}

bool UChargeActionPlayerModule_V2::CanRestartChargeFromCurrentAction( bool bIsAirChargeAttackEdCancelable ) const
{
	if ( !IsPlayingChargeAction() )
	{
		return true;
	}

	// CanCombo 窓中は R2 長押しだけで次のコンボチャージへ入れる。ただし残留タグ無視窓中は許可しない——
	// 前段の CanCombo が残ったまま R2 を保持していると、始まったばかりの攻撃を即キャンセルして段が飛ぶ（2→4）
	const bool bIsGroundChargeAttackAction =
		CurrentChargeActionType == EChargeActionV2Type::Attack &&
		!bIsAirChargeAttack && !bIsAirNormalAttack;

	const bool bGroundChargeAttackComboWindow =
		bIsGroundChargeAttackAction &&
		!IsWithinChargeActionTagResidueWindow() &&
		OwnerCharacter && OwnerCharacter->HasStateTag( TAG_State_Player_CanCombo );

	// ヒット由来の再チャージも地上チャージ攻撃では上の CanCombo 窓に集約する（殴った瞬間に溜め直すと
	// 攻撃モンタージュが止まって CanCombo が来ず、コンボ段が永久に 1 のままになる）
	const bool bHitCancelWindow = bIsHitCancelableToCharge && !bIsGroundChargeAttackAction;

	if ( !IsPlayingChargeDash() &&
		!bIsAirChargeAttackEdCancelable &&
		!bHitCancelWindow &&
		!bGroundChargeAttackComboWindow )
	{
		return false;
	}

	if ( IsPlayingChargeDash() && IsChargeStartBlockedByDashStartMontage() )
	{
		return false;
	}

	return true;
}

bool UChargeActionPlayerModule_V2::IsChargeStartBlockedByDashStartMontage() const
{
	if ( !OwnerCharacter )
	{
		return false;
	}

	UAnimMontage* CurrentMontage = OwnerCharacter->GetCurrentMontage();
	UAnimMontage* DashStartMontage = GetAnimMontage( GetChargeDashStartAnimTag() );
	return CurrentMontage != nullptr && CurrentMontage == DashStartMontage;
}

bool UChargeActionPlayerModule_V2::IsChargeActionStateFree( bool bIsAirChargeAttackEdCancelable ) const
{
	const bool bCanProceedCombo = OwnerCharacter->HasStateTag( TAG_State_Player_CanCombo );
	const bool bIsAttacking = OwnerCharacter->IsAttacking();

	return !IsPlayingChargeAction() ||
		IsPlayingChargeDash() ||
		bIsAirChargeAttackEdCancelable ||
		bIsAttacking ||
		bCanProceedCombo ||
		bIsHitCancelableToCharge;
}

bool UChargeActionPlayerModule_V2::IsChargeStartStateValid( bool bIsAirChargeAttackEdCancelable ) const
{
	const bool bCanProceedCombo = OwnerCharacter->HasStateTag( TAG_State_Player_CanCombo );

	return !OwnerCharacter->HasStateTag( TAG_State_Common_Disable ) &&
		( OwnerCharacter->HasStateTag( TAG_State_Player_CanCharge ) ||
			bCanProceedCombo ||
			bIsAirChargeAttackEdCancelable ||
			IsPlayingChargeDash() ||
			bIsHitCancelableToCharge ) &&
		!OwnerCharacter->IsHitReacting();
}

bool UChargeActionPlayerModule_V2::IsAttackButtonChargeComboEnabled() const
{
	const UTidePlayerParamDataAsset* PlayerParams = GetPlayerParams();
	return PlayerParams && PlayerParams->bEnableAttackButtonChargeCombo;
}

bool UChargeActionPlayerModule_V2::IsChargePriorityActive() const
{
	if ( IsAttackButtonChargeComboEnabled() )
	{
		const bool bIsChargeComboActive = IsPlayingChargeAttackMontage();
		return bChargeInputHeld || IsPlayingChargeDash() || bIsChargeComboActive || bIsGearShiftedByComboFinish;
	}

	return bChargeInputHeld || IsPlayingChargeDash();
}

bool UChargeActionPlayerModule_V2::IsWithinChargeActionTagResidueWindow() const
{
	const UTidePlayerParamDataAsset* PlayerParams = GetPlayerParams();
	const float TagResidueIgnoreTime = PlayerParams ? PlayerParams->ChargeV2TagResidueIgnoreTime : 0.25f;
	return !ChargedActionLockTimer.IsFinish() && ChargedActionLockTimer.GetElapsed() < TagResidueIgnoreTime;
}

bool UChargeActionPlayerModule_V2::CanStartChargeAttackFromCurrentState() const
{
	if ( !OwnerCharacter ) return false;

	if ( CurrentChargeActionType == EChargeActionV2Type::Attack && IsWithinChargeActionTagResidueWindow() )
	{
		return false;
	}

	return OwnerCharacter->HasStateTag( TAG_State_Player_CanAttack );
}

bool UChargeActionPlayerModule_V2::CanStartChargeAttackComboFromCurrentState() const
{
	if ( !OwnerCharacter ) return false;

	const bool bIsChargeComboActive = IsPlayingChargeAttackMontage();
	if ( CurrentChargeActionType == EChargeActionV2Type::Attack && IsWithinChargeActionTagResidueWindow() )
	{
		return false;
	}

	if ( bIsChargeComboActive )
	{
		return OwnerCharacter->HasStateTag( TAG_State_Player_CanCombo );
	}

	return OwnerCharacter->HasStateTag( TAG_State_Player_CanAttack ) ||
		OwnerCharacter->HasStateTag( TAG_State_Player_CanCombo );
}

void UChargeActionPlayerModule_V2::UpdateChargeBeginCheck()
{
	if ( !OwnerCharacter ) return;
	if ( bBlockChargeInputUntilRelease ) return;
	// R2 を握りっぱなしのままの自動再チャージ（攻撃後・被弾後など）を抑止する
	if ( IsAutoChargeRestartBlocked() ) return;

	if ( CanBeginChargeFromCurrentState() )
	{
		BeginCharge();
	}
}

bool UChargeActionPlayerModule_V2::IsAirChargeAttackEndCancelable() const
{
	if ( !OwnerCharacter ) return false;
	if ( !IsAnyAirDiveAttackActive() ) return false;

	UAnimMontage* CurrentMontage = OwnerCharacter->GetCurrentMontage();
	UAnimMontage* EdMontage = GetAnimMontage( GetAirDiveAttackEndTag() );
	if ( CurrentMontage == nullptr || CurrentMontage != EdMontage )
	{
		return false;
	}

	// CanMove は「移動可能」を示すだけでモーションキャンセルの合図ではないため見ない
	return OwnerCharacter->HasStateTag( TAG_State_Player_MoveCancelable );
}

bool UChargeActionPlayerModule_V2::IsAutoChargeRestartBlocked() const
{
	const UTidePlayerParamDataAsset* PlayerParams = GetPlayerParams();
	if ( !PlayerParams ) return false;

	// 全経路で押し直しを要求するモード
	if ( bRequireChargeRepress && PlayerParams->bRequireChargeRepressToRestart ) return true;

	// 部分適用モード（チャージ攻撃最終段・被弾のときだけ要求する）
	return bRequireChargeRepressByEvent && PlayerParams->bRequireChargeRepressAfterComboFinishAndDamage;
}

void UChargeActionPlayerModule_V2::RequestChargeRepressByEvent()
{
	// R2 を離していれば次の押下がそのまま自主的な溜め直しになるので、待ちを立てる必要はない
	if ( !bChargeInputPhysicallyHeld ) return;
	bRequireChargeRepressByEvent = true;
}

bool UChargeActionPlayerModule_V2::IsChargeHoldStale() const
{
	// 溜め中は対象外（その溜め自体が自主的な押下から始まっているため）
	return !bIsCharging && IsAutoChargeRestartBlocked();
}

void UChargeActionPlayerModule_V2::UpdateStaleChargeHoldRelease()
{
	if ( !bChargeInputHeld || !IsChargeHoldStale() ) return;

	// ブースト中断からの復帰予約中は落とさない（プレイヤー操作で失った溜めではないため）
	if ( bResumeChargeHoldAfterBoost ) return;

	// 「R2 を離している」扱いに揃える。自動再チャージも長押し由来の派生も出なくなり、通常アクションへ流れる
	bChargeInputHeld = false;
}

bool UChargeActionPlayerModule_V2::CanBeginChargeFromCurrentState() const
{
	if ( !OwnerCharacter ) return false;
	if ( !bChargeInputHeld || bIsCharging ) return false;

	// bChargeInputHeld は握られたままなので、抑止しないと次フレームで即再チャージして
	// ブースト専用モーションを毎フレーム潰す
	if ( bResumeChargeHoldAfterBoost ) return false;

	// ブースト中の新規チャージ可否はフラグで切り替える
	if ( OwnerCharacter->IsBoostDashing() )
	{
		const UTidePlayerParamDataAsset* BoostChargeParams = GetPlayerParams();
		if ( !BoostChargeParams || !BoostChargeParams->bBoostKeepChargeMotionWhileCharging )
		{
			return false;
		}
	}

	// ST 終了で通常落下へ移るので、次のダッシュ（MaxAirChargeDashCount>=2）はその落下中の再チャージ→解放で出す
	if ( IsAirChargeDashing() )
	{
		return false;
	}

	// 使い切った空中では溜め自体を始めさせない。R2 を握り続けていれば着地時に自動で溜め直す
	if ( OwnerCharacter->IsAirActionLimitedAfterAirCharge() )
	{
		return false;
	}

	// 封印（Disable）は LaunchActionLockAscentRate で頂点より手前に解けるため、
	// そこを通ると打ち上がりの途中でチャージへ移ってしまう（滑空も奪われる）
	if ( OwnerCharacter->IsLaunchAscending() )
	{
		return false;
	}

	// CanCombo 区間だけ次のコンボチャージへ入れる。待たずに溜め直すと攻撃モンタージュが止まり
	// CanCombo が来ないまま段が上がらない（終わり切ったあとは通常どおり1段目から）
	if ( IsPlayingGroundChargeAttackMontage() && !OwnerCharacter->HasStateTag( TAG_State_Player_CanCombo ) )
	{
		return false;
	}

	const bool bIsAirChargeAttackEdCancelable = IsAirChargeAttackEndCancelable();
	if ( !CanRestartChargeFromCurrentAction( bIsAirChargeAttackEdCancelable ) )
	{
		return false;
	}

	return IsChargeActionStateFree( bIsAirChargeAttackEdCancelable ) &&
		IsChargeStartStateValid( bIsAirChargeAttackEdCancelable );
}

void UChargeActionPlayerModule_V2::UpdateChargeShift( float DeltaTime )
{
	const UTidePlayerParamDataAsset* PlayerParams = GetPlayerParams();
	if ( !PlayerParams ||
		!bIsCharging ||
		!bChargeInputHeld ||
		// R1 限定モード中はタイマー自体を進めない（ドリフト／スピードボーナスもここ経由なので同時に無効化される）
		PlayerParams->bManualGearUpOnly ||
		CurrentChargeGearIndex >= PlayerParams->MaxChargeGearCount )
	{
		return;
	}

	const float TimeMultiplier = GetDynamicChargeMultiplier();
	ChargeShiftTimer.Update( DeltaTime * TimeMultiplier );

	if ( ChargeShiftTimer.IsFinish() )
	{
		const float ShiftHoldTime = bIsShortenedChargeActive ? PlayerParams->PostActionMaxChargeTime : PlayerParams->ChargeV2ShiftHoldTime;
		ChargeShiftTimer.Set( ShiftHoldTime );

		ShiftUpGear();
	}
}

void UChargeActionPlayerModule_V2::UpdateChargingState( float DeltaTime )
{
	if ( !bIsCharging ) return;

	const UTidePlayerParamDataAsset* PlayerParams = GetPlayerParams();
	if ( !PlayerParams ) return;

	const float TimeMultiplier = GetDynamicChargeMultiplier();
	CurrentChargeTimer.Update( DeltaTime * TimeMultiplier );

	const float ChargeRate = CurrentChargeTimer.GetRate();

	// ヒットキャンセルでの開始直後はステアリングを重くし、後退しながら自由に動き回れないようにする
	float DriftSteeringScale = 1.0f;
	if ( !HitCancelChargingSteeringTimer.IsFinish() )
	{
		HitCancelChargingSteeringTimer.Update( DeltaTime );
		DriftSteeringScale = PlayerParams->HitCancelChargingDriftSteeringRate;
	}

	OwnerCharacter->UpdateChargingMovementParams( ChargeRate, DriftSteeringScale );

	if ( UTideGameSettings::Get()->bDebugFlagDrawHomingArea )
	{
		// 攻撃（シアン）とチャージダッシュ（オレンジ）の扇は別パラメータなので両方プレビューする
		constexpr float PreviewDuration = 0.0f;	// 1フレームのみ
		const FVector PreviewDir = GetActorYawForwardDirection();
		GetHomingDirection( PreviewDir, PreviewDuration );
		GetChargeDashHomingDirection( PreviewDir, PreviewDuration );
	}

	if ( !bHasReachedMaxCharge && CurrentChargeTimer.IsFinish() )
	{
		bHasReachedMaxCharge = true;
	}

#if !UE_BUILD_SHIPPING
	if ( EffectSubModule )
	{
		EffectSubModule->DrawChargeGaugeUI();
	}
#endif
}

float UChargeActionPlayerModule_V2::GetHitCancelPropulsionScale() const
{
	const UTidePlayerParamDataAsset* PlayerParams = GetPlayerParams();
	if ( !PlayerParams ) return 1.0f;

	if ( HitCancelChargingSteeringTimer.IsFinish() ) return 1.0f;
	return PlayerParams->HitCancelChargingPropulsionRate;
}

void UChargeActionPlayerModule_V2::UpdatePropulsionLock( float DeltaTime )
{
	if ( UpdateReboundSliding( DeltaTime ) )
		return;

	if ( ChargedActionLockTimer.IsFinish() ) return;

	// 速度のハードセットを止めて構えブレーキを効かせ、寿命タイマーも止めて残り時間を保持する
	if ( ShouldFreezeChargeDashForGodArt() )
	{
		bChargeDashFrozenByGodArt = true;
		return;
	}
	if ( bChargeDashFrozenByGodArt )
	{
		bChargeDashFrozenByGodArt = false;
		BeginGodArtDashResumeEase();
	}

	ChargedActionLockTimer.Update( DeltaTime );

	if ( CurrentChargeActionType == EChargeActionV2Type::Dash )
	{
		// ターン中はロック移動を止め、ターンモーションのルートモーションに旋回を任せる
		if ( !TryUpdateChargeDashTurn( DeltaTime ) )
		{
			UpdateChargeDashLockedMovement( DeltaTime );
		}
	}
	else if ( CurrentChargeActionType == EChargeActionV2Type::Attack && bChargeAttackTimedApproachActive )
	{
		UpdateChargeAttackTimedApproach( DeltaTime );
	}
	// 空中チャージ攻撃の移動は UpdateAirChargeAttackDive が専任するので、ここでは何もしない

	if ( ChargedActionLockTimer.IsFinish() )
	{
		if ( ShouldDeferChargedActionEnd() ) return;

		OnEndAction();
	}
}

void UChargeActionPlayerModule_V2::UpdateFrictionRecoveryState( float DeltaTime )
{
	if ( FrictionRecoveryTimer.IsFinish() ) return;
	if ( !OwnerCharacter ) return;

	// 「攻撃中」は通常攻撃に限定する——チャージ攻撃の出し切り直後はモンタージュの余韻で IsAttacking()==true が
	// 残り、移行と誤判定すると次フレームで摩擦が通常値へ戻ってヒットバックの後退がすぐ止まる
	const bool bInterruptedByNewAttack = OwnerCharacter->IsAttacking() && !IsPlayingChargeAttackMontage();
	if ( OwnerCharacter->IsDashing() ||
		OwnerCharacter->IsDodging() ||
		bInterruptedByNewAttack )
	{
		FrictionRecoveryTimer.Clear();
		OwnerCharacter->RefreshMovementParams();
		return;
	}

	// 開始時と接地／空中が入れ替わったら即キャンセル
	if ( IsEffectivelyInAir() && !bIsAirFrictionRecovery )
	{
		FrictionRecoveryTimer.Clear();
		OwnerCharacter->RefreshMovementParams();
		return;
	}
	if ( !IsEffectivelyInAir() && bIsAirFrictionRecovery )
	{
		FrictionRecoveryTimer.Clear();
		OwnerCharacter->RefreshMovementParams();
		return;
	}

	FrictionRecoveryTimer.Update( DeltaTime );
	OwnerCharacter->UpdateFrictionRecovery( FrictionRecoveryTimer.GetRate() );
}

void UChargeActionPlayerModule_V2::UpdateGravityLock( float DeltaTime )
{
	if ( GravityLockTimer.IsFinish() ) return;

	GravityLockTimer.Update( DeltaTime );

	if ( UCharacterMovementComponent* MovementComp = GetCharacterMovement() )
	{
		// GravityLockZUpSpeed が 0 なら水平維持、正値ならその速度ぶん上昇
		MovementComp->Velocity.Z = GravityLockZUpSpeed;
		MovementComp->GravityScale = 0.0f;
	}

	if ( GravityLockTimer.IsFinish() )
	{
		if ( OwnerCharacter )
		{
			OwnerCharacter->RefreshMovementParams();
		}
	}
}

void UChargeActionPlayerModule_V2::UpdateReservedDashOnLanding()
{
	if ( !bReserveDashOnLanding || !OwnerCharacter ) return;

	// 空中で別の行動を始めていたら予約をキャンセルする
	if ( OwnerCharacter->IsAttacking() || OwnerCharacter->IsDodging() || IsPlayingChargeAction() || bIsCharging )
	{
		bReserveDashOnLanding = false;
		return;
	}

	if ( !IsEffectivelyInAir() )
	{
		bReserveDashOnLanding = false;
		OwnerCharacter->RequestDash();
	}
}

void UChargeActionPlayerModule_V2::UpdateAirChargeAttackAnimState( float DeltaTime )
{
	if ( !OwnerCharacter || !IsAnyAirDiveAttackActive() ) return;

	// 空中判定は IsFalling() で見る。IsEffectivelyInAir() は「真下130cm以内に地面あり」で false になり、
	// 地面へ近づいた 2 回目以降で ST→LP 遷移が止まって ST のまま固まる（着地処理は OnLanded）
	if ( bIsAirChargeAttackInLoop || !OwnerCharacter->IsFalling() ) return;

	// 滞空タイマーがセットされていれば（AirChargeAttackHoverTime>0）その経過で、
	// 未設定なら ST モーションの再生終了で突進（LP）へ移す
	if ( AirChargeAttackHoverTimer.IsValid() )
	{
		AirChargeAttackHoverTimer.Update( DeltaTime );
		if ( AirChargeAttackHoverTimer.IsFinish() )
		{
			TransitionAirChargeAttackToLoop();
		}
		return;
	}

	if ( HasAirChargeAttackStartMontageFinished() )
	{
		TransitionAirChargeAttackToLoop();
	}
}

void UChargeActionPlayerModule_V2::UpdateAirChargeAttackDive( float DeltaTime )
{
	// 振り下ろし（bIsAirNormalAttack）は真下プランジのままなので対象外
	if ( !OwnerCharacter || !IsRevampAirDiveAttack() ) return;

	// 判定は IsFalling()——IsEffectivelyInAir() は地面近接で false になり、着地前に突進が止まってしまう
	// （着地 ED は OnLanded が再生し、重力復元は ResetChargedActionState が行う）
	if ( !OwnerCharacter->IsFalling() ) return;

	UCharacterMovementComponent* MovementComp = GetCharacterMovement();
	const UTidePlayerParamDataAsset* PlayerParams = GetPlayerParams();
	if ( !MovementComp || !PlayerParams ) return;

	// 突進を終えた後は速度・重力を強制せず通常落下に委ねる。
	// これをしないと重力 0 のまま落ちず、着地できずハングする
	if ( bAirChargeAttackDiveWallStuck || bAirChargeAttackDiveEnded ) return;

	if ( !bIsAirChargeAttackInLoop )
	{
		// ST：水平・垂直とも 0 に固定し、重力もカットしてその場でホバーさせる
		MovementComp->Velocity = FVector::ZeroVector;
		MovementComp->GravityScale = 0.0f;
	}
	else if ( bIsAirChargeAttackLockOnRush && AirChargeAttackRushTarget.IsValid() )
	{
		// LP（ロックオン）：角度に依らずロック部位へ直接（3D）突進し、毎フレーム対象を追う
		const FVector ToTarget = ( AirChargeAttackRushTarget->GetTargetLocation() - OwnerCharacter->GetActorLocation() ).GetSafeNormal();
		if ( !ToTarget.IsNearlyZero() )
		{
			// 傾けたピッチは突進終了時に EndAirChargeAttackDive が戻す
			if ( PlayerParams->bAirChargeAttackLockOnRushOrientToTarget )
			{
				FRotator RushRot = ToTarget.Rotation();
				RushRot.Pitch += PlayerParams->AirChargeAttackLockOnRushPitchOffset;
				RushRot.Roll = 0.0f;
				OwnerCharacter->SetActorRotation( RushRot );
			}
			else
			{
				const FVector Face = ToTarget.GetSafeNormal2D();
				if ( !Face.IsNearlyZero() )
				{
					OwnerCharacter->SetActorRotation( Face.Rotation() );
				}
			}
			MovementComp->Velocity = ToTarget * PlayerParams->AirChargeAttackDiveSpeed;
			MovementComp->GravityScale = 0.0f;
		}
	}
	else
	{
		// LP（非ロック）：機体前方を水平成分とし、そこから下向きへ AngleDeg 傾けた方向へ等速突進する
		const FVector Forward = OwnerCharacter->GetActorForwardVector().GetSafeNormal2D();
		const float AngleRad = FMath::DegreesToRadians( FMath::Clamp( PlayerParams->AirChargeAttackDiveAngleDeg, 0.0f, 89.0f ) );
		FVector DiveDir = ( Forward * FMath::Cos( AngleRad ) ) + ( FVector( 0.0f, 0.0f, -1.0f ) * FMath::Sin( AngleRad ) );
		DiveDir = DiveDir.GetSafeNormal();

		// 傾けたピッチは終了時に RestoreUprightAfterAirChargeAttackDive が直立へ戻す
		if ( PlayerParams->bAirChargeAttackDiveOrientToDirection && !DiveDir.IsNearlyZero() )
		{
			FRotator DiveRot = DiveDir.Rotation();
			DiveRot.Pitch += PlayerParams->AirChargeAttackDivePitchOffset;
			DiveRot.Roll = 0.0f;
			OwnerCharacter->SetActorRotation( DiveRot );
		}

		MovementComp->Velocity = DiveDir * PlayerParams->AirChargeAttackDiveSpeed;
		MovementComp->GravityScale = 0.0f;
	}

	// LP の持続時間で終了（当たらなくても攻撃終わり）。ロック突進・角度ダイブ共通で、未設定（パラメータ 0）なら無制限＝着地・壁ヒットまで。
	// 角度ダイブは満了後に残す水平速度を DA で決める（ロック突進は対象至近で終わるため従来どおり全量残す）
	if ( bIsAirChargeAttackInLoop )
	{
		if ( AirChargeAttackDiveLogs.Num() > 0 ) AirChargeAttackDiveLogs[0].LoopTime += DeltaTime;

		if ( AirChargeAttackDiveTimer.IsValid() )
		{
			AirChargeAttackDiveTimer.Update( DeltaTime );
			if ( AirChargeAttackDiveTimer.IsFinish() )
			{
				const bool bLockOnRush = bIsAirChargeAttackLockOnRush;
				EndAirChargeAttackDive(
					bLockOnRush ? 1.0f : PlayerParams->AirChargeAttackDiveEndSpeedRate,
					TEXT( "持続時間満了" ) );

				// 角度ダイブは着地まで持ち越さずここで攻撃アクションごと手放す。着地 ED は
				// IsAnyAirDiveAttackActive() の間だけ再生されるため、手放せば ED は出ず消灯フェードへ向かう
				// （着地は落下ループを引き継いだ JumpModule が JUMP_ED で締める）。
				// ロック突進の満了は従来どおり着地 ED まで持ち越す
				if ( !bLockOnRush )
				{
					if ( OwnerCharacter->IsAttacking() ) OwnerCharacter->CancelAttack();
					OnEndAction();
				}
			}
		}
	}
}

void UChargeActionPlayerModule_V2::EndAirChargeAttackDive( float HorizontalSpeedRate, const TCHAR* Reason )
{
	if ( !OwnerCharacter ) return;

	// 速度・重力強制をやめて通常落下へ返す（壁ヒットと同じ「強制やめ→自然落下」ルート）。
	// 以降は落下→着地（OnLanded）で ED を再生して終了する
	AirChargeAttackDiveTimer.Clear();
	bAirChargeAttackDiveEnded = true;
	RestoreUprightAfterAirChargeAttackDive();
	OwnerCharacter->RefreshMovementParams();	// GravityScale=0 を通常へ戻す

	// 強制をやめるだけでは突進速度（既定 5000cm/s）が水平慣性として残る——落下中の減速
	// （ChargeAttackBrakingDecelerationFalling）は CMC の仕様上「加速度入力が無いときだけ」効き、
	// スティックを倒したままだと働かないため。Z（落下）は物理に任せ、水平だけをここで落とす
	if ( HorizontalSpeedRate < 1.0f )
	{
		if ( UCharacterMovementComponent* MovementComp = GetCharacterMovement() )
		{
			const float Rate = FMath::Clamp( HorizontalSpeedRate, 0.0f, 1.0f );
			MovementComp->Velocity.X *= Rate;
			MovementComp->Velocity.Y *= Rate;
		}
	}

	// LP のまま落ちると突進ポーズで落下してしまうため落下ループへ引き渡す
	// （アクション自体は着地まで続き、OnLanded が ED を再生する）
	if ( OwnerCharacter->IsFalling() )
	{
		OwnerCharacter->EnterJumpFallingLoop();
	}

	RecordAirChargeAttackDiveEnd( Reason, HorizontalSpeedRate );
}

void UChargeActionPlayerModule_V2::BeginAirChargeAttackDiveLog()
{
	FAirChargeAttackDiveLog NewLog;
	NewLog.bLockOnRush = bIsAirChargeAttackLockOnRush;
	NewLog.MaxTime = AirChargeAttackDiveTimer.GetStart();

	AirChargeAttackDiveLogs.Insert( NewLog, 0 );
	if ( AirChargeAttackDiveLogs.Num() > AirChargeAttackDiveLogMax )
	{
		AirChargeAttackDiveLogs.SetNum( AirChargeAttackDiveLogMax );
	}
}

void UChargeActionPlayerModule_V2::RecordAirChargeAttackDiveEnd( const TCHAR* Reason, float SpeedRate )
{
	if ( AirChargeAttackDiveLogs.Num() == 0 ) return;

	FAirChargeAttackDiveLog& Log = AirChargeAttackDiveLogs[0];
	if ( Log.EndSpeed2D >= 0.0f ) return;	// 既に閉じたログは上書きしない（着地までに複数の終了経路が走るため）

	const UCharacterMovementComponent* MovementComp = GetCharacterMovement();
	Log.EndSpeed2D = MovementComp ? MovementComp->Velocity.Size2D() : 0.0f;
	Log.EndSpeedRate = SpeedRate;
	Log.EndReason = Reason;
}

void UChargeActionPlayerModule_V2::RecordAirChargeAttackDiveLanding()
{
	if ( AirChargeAttackDiveLogs.Num() == 0 ) return;

	FAirChargeAttackDiveLog& Log = AirChargeAttackDiveLogs[0];
	if ( Log.LandSpeed2D >= 0.0f ) return;

	// 突進が終わらないまま着地した場合はここで閉じる（＝持続時間が効いていない証拠になる）
	RecordAirChargeAttackDiveEnd( TEXT( "着地" ), 1.0f );

	const UCharacterMovementComponent* MovementComp = GetCharacterMovement();
	Log.LandSpeed2D = MovementComp ? MovementComp->Velocity.Size2D() : 0.0f;
}

void UChargeActionPlayerModule_V2::RestoreUprightAfterAirChargeAttackDive()
{
	// 傾けていない（Yaw のみ・OFF）ケースは Pitch/Roll がほぼ 0 なので何もしない
	if ( !OwnerCharacter ) return;

	FRotator Rot = OwnerCharacter->GetActorRotation();
	if ( FMath::IsNearlyZero( Rot.Pitch ) && FMath::IsNearlyZero( Rot.Roll ) ) return;

	Rot.Pitch = 0.0f;
	Rot.Roll = 0.0f;
	OwnerCharacter->SetActorRotation( Rot );
}

void UChargeActionPlayerModule_V2::UpdateAirChargeAttackEndCheck()
{
	if ( !OwnerCharacter || !IsAnyAirDiveAttackActive() ) return;

	UAnimMontage* CurrentMontage = OwnerCharacter->GetCurrentMontage();
	UAnimMontage* EdMontage = GetAnimMontage( GetAirDiveAttackEndTag() );

	// 空中チャージ攻撃の ED（着地）：CanMove まで硬直。CanMove 中に移動入力が入ったら ED を打ち切って
	// 他アクションへ遷移し、入力が無ければモーションを最後まで見せ切ってから終了する
	if ( IsRevampAirDiveAttack() )
	{
		if ( CurrentMontage != nullptr && CurrentMontage == EdMontage )
		{
			bAirChargeAttackEdMontageStarted = true;

			// ED 開始からの経過。残留タグ無視と自然終了の両方に使う
			float EdPos = 0.0f;
			const float EdLength = EdMontage->GetPlayLength();
			if ( UAnimInstance* AnimInst = OwnerCharacter->GetMesh()->GetAnimInstance() )
			{
				EdPos = AnimInst->Montage_GetPosition( EdMontage );
			}

			// ED 開始直後は前状態の CanMove 残留＋（突進中から握りっぱなしの）移動入力で
			// 「開始即キャンセル」になるため、この間はキャンセルを無視する
			const float ResidueIgnoreTime = OwnerCharacter->PlayerParamData ? OwnerCharacter->PlayerParamData->AirChargeAttackEndResidueIgnoreTime : 0.0f;
			const bool bPastResidueWindow = ( EdPos >= ResidueIgnoreTime );

			// ED 中は通常攻撃モジュールが優先ブロックで弾かれるため、攻撃入力をここで直接引き受ける。
			// 地上の弱攻撃がマスクされているときは、出せない攻撃のために ED を打ち切らないよう受付を止める
			if ( bPastResidueWindow && !OwnerCharacter->IsGroundNormalAttackMasked()
				&& OwnerCharacter->HasStateTag( TAG_State_Player_CanAttack ) )
			{
				const float AttackBufferTime = OwnerCharacter->PlayerParamData ? OwnerCharacter->PlayerParamData->AttackBufferTime : 0.2f;
				if ( TryConsumeCommand( TAG_Input_Command_Attack, AttackBufferTime ) )
				{
					if ( OwnerCharacter->IsAttacking() ) OwnerCharacter->CancelAttack();
					OnEndAction();
					OwnerCharacter->RequestAttack( EPlayerAttackType::Light );
					return;
				}
			}

			// CanMove が来るまでは RequestMove が弾くため、そもそも動けない＝硬直
			if ( bPastResidueWindow && OwnerCharacter->HasStateTag( TAG_State_Player_CanMove ) )
			{
				const float InputThreshold = OwnerCharacter->PlayerParamData ? OwnerCharacter->PlayerParamData->DashCancelInputThreshold : 0.2f;
				if ( OwnerCharacter->GetRawMovementInput().Size() >= InputThreshold )
				{
					OwnerCharacter->StopAnimMontage( 0.1f, EdMontage );
					if ( OwnerCharacter->IsAttacking() ) OwnerCharacter->CancelAttack();	// 完全に手放す
					OnEndAction();
					return;
				}
			}

			// 入力が無ければ ED を最後まで再生し、終端で自然終了させる（ハング防止）
			if ( EdLength > 0.0f && EdPos >= ( EdLength - 0.05f ) )
			{
				OnEndAction();
				return;
			}
		}
		else if ( bAirChargeAttackEdMontageStarted )
		{
			// ED が既に外れている（別モンタージュ／ブレンドアウト完了）＝終了とみなす（ハング防止）
			OnEndAction();
		}
		return;
	}

	// 従来（振り下ろし等）：CanMove は「移動可能」を示すだけでキャンセルの合図ではないため見ない
	if ( CurrentMontage != nullptr && CurrentMontage == EdMontage )
	{
		if ( OwnerCharacter->HasStateTag( TAG_State_Player_MoveCancelable ) )
		{
			OnEndAction();
		}
	}
}

float UChargeActionPlayerModule_V2::MeasureGroundDistanceBelow() const
{
	if ( !OwnerCharacter || !OwnerCharacter->GetWorld() ) return -1.0f;

	constexpr float TraceDistance = 130.0f;	// IsEffectivelyInAir のトレースと同じ距離
	const FVector StartLoc = OwnerCharacter->GetActorLocation();
	const FVector EndLoc = StartLoc - FVector( 0.0f, 0.0f, TraceDistance );

	FHitResult Hit;
	FCollisionQueryParams Params( SCENE_QUERY_STAT( ChargeGroundBelowCheck ), false, OwnerCharacter );
	if ( OwnerCharacter->GetWorld()->LineTraceSingleByChannel( Hit, StartLoc, EndLoc, ECC_Visibility, Params ) )
	{
		return Hit.Distance;
	}
	return -1.0f;
}

void UChargeActionPlayerModule_V2::RecordChargeAttackTriggerLog()
{
	if ( !OwnerCharacter ) return;

	FChargeAttackTriggerLog Log;
	Log.bResultAir = bIsAirChargeAttack;
	Log.bFalling = OwnerCharacter->IsFalling();
	Log.bEffectivelyInAir = IsEffectivelyInAir();
	Log.bLaunchAirborne = bAirborneByGroundChargeLaunch;
	Log.bUsedThisAirtime = bAirChargeAttackUsedThisAirtime;
	Log.bChargeHeld = bChargeInputHeld;
	Log.Source = ChargeAttackTriggerSource;
	Log.GroundDistance = MeasureGroundDistanceBelow();

	if ( const UCharacterMovementComponent* MovementComp = GetCharacterMovement() )
	{
		Log.VelocityZ = MovementComp->Velocity.Z;
		Log.Speed2D = MovementComp->Velocity.Size2D();
		Log.MovementMode = ( MovementComp->MovementMode == MOVE_Falling ) ? FName( TEXT( "Falling" ) )
			: ( MovementComp->MovementMode == MOVE_Walking ) ? FName( TEXT( "Walking" ) )
			: FName( TEXT( "Other" ) );
	}

	// 発動直前のモンタージュ（SetupAirChargeAttackState の時点ではまだ張り替わっていない）
	if ( UAnimMontage* PrevMontage = OwnerCharacter->GetCurrentMontage() )
	{
		Log.PrevMontage = PrevMontage->GetFName();
		Log.bDuringAirDiveEd = ( PrevMontage == GetAnimMontage( PlayerAnimTags::AIRCHARGE_ATK_ED ) );
	}

	if ( LastLandedTimeSeconds >= 0.0f && OwnerCharacter->GetWorld() )
	{
		Log.TimeSinceLanded = OwnerCharacter->GetWorld()->GetTimeSeconds() - LastLandedTimeSeconds;
	}

	if ( OwnerCharacter->HasStateTag( TAG_State_Player_CanAttack ) )		Log.Tags += TEXT( "CanAttack " );
	if ( OwnerCharacter->HasStateTag( TAG_State_Player_CanCombo ) )		Log.Tags += TEXT( "CanCombo " );
	if ( OwnerCharacter->HasStateTag( TAG_State_Player_CanCharge ) )	Log.Tags += TEXT( "CanCharge " );
	if ( OwnerCharacter->HasStateTag( TAG_State_Player_MoveCancelable ) ) Log.Tags += TEXT( "MoveCancelable " );
	if ( Log.Tags.IsEmpty() ) Log.Tags = TEXT( "---" );

	ChargeAttackTriggerLogs.Insert( Log, 0 );
	if ( ChargeAttackTriggerLogs.Num() > ChargeAttackTriggerLogMax )
	{
		ChargeAttackTriggerLogs.SetNum( ChargeAttackTriggerLogMax );
	}
	ChargeAttackTriggerSource = TEXT( "---" );	// 経路名は 1 回ぶんで使い切る
}

void UChargeActionPlayerModule_V2::UpdateAirborneTransitionLog()
{
	if ( !OwnerCharacter ) return;

	const bool bFalling = OwnerCharacter->IsFalling();
	const bool bWasFalling = bAirborneTransitionLastFalling;
	bAirborneTransitionLastFalling = bFalling;
	if ( !bFalling || bWasFalling ) return;	// 接地→落下の立ち上がりだけ残す

	FAirborneTransitionLog Log;
	Log.bCharging = bIsCharging;
	Log.GroundDistance = MeasureGroundDistanceBelow();

	switch ( CurrentChargeActionType )
	{
	case EChargeActionV2Type::Dash:		Log.ActionType = TEXT( "Dash" ); break;
	case EChargeActionV2Type::Attack:	Log.ActionType = TEXT( "Attack" ); break;
	case EChargeActionV2Type::Jump:		Log.ActionType = TEXT( "Jump" ); break;
	default: break;
	}

	if ( UAnimMontage* CurrentMontage = OwnerCharacter->GetCurrentMontage() )
	{
		Log.Montage = CurrentMontage->GetFName();
	}
	if ( const UCharacterMovementComponent* MovementComp = GetCharacterMovement() )
	{
		Log.VelocityZ = MovementComp->Velocity.Z;
		Log.Speed2D = MovementComp->Velocity.Size2D();
	}
	if ( LastLandedTimeSeconds >= 0.0f && OwnerCharacter->GetWorld() )
	{
		Log.TimeSinceLanded = OwnerCharacter->GetWorld()->GetTimeSeconds() - LastLandedTimeSeconds;
	}

	AirborneTransitionLogs.Insert( Log, 0 );
	if ( AirborneTransitionLogs.Num() > AirborneTransitionLogMax )
	{
		AirborneTransitionLogs.SetNum( AirborneTransitionLogMax );
	}
}

void UChargeActionPlayerModule_V2::UpdateAirChargeAttackEdAirborneLog()
{
	if ( !OwnerCharacter ) return;

	UAnimMontage* EdMontage = GetAnimMontage( PlayerAnimTags::AIRCHARGE_ATK_ED );
	const bool bEdPlaying = ( EdMontage != nullptr && OwnerCharacter->GetCurrentMontage() == EdMontage );
	if ( !bEdPlaying )
	{
		bAirChargeAttackEdAirborneLogged = false;
		return;
	}

	// ED 1回につき最初に浮いた瞬間だけ残す（毎フレーム積むと読めないため）
	if ( bAirChargeAttackEdAirborneLogged || !OwnerCharacter->IsFalling() ) return;
	bAirChargeAttackEdAirborneLogged = true;

	FAirChargeAttackEdAirborneLog Log;
	if ( UAnimInstance* AnimInst = OwnerCharacter->GetMesh()->GetAnimInstance() )
	{
		Log.EdPosition = AnimInst->Montage_GetPosition( EdMontage );
	}
	if ( const UCharacterMovementComponent* MovementComp = GetCharacterMovement() )
	{
		Log.Speed2D = MovementComp->Velocity.Size2D();
		Log.VelocityZ = MovementComp->Velocity.Z;
	}
	if ( LastLandedTimeSeconds >= 0.0f && OwnerCharacter->GetWorld() )
	{
		Log.TimeSinceLanded = OwnerCharacter->GetWorld()->GetTimeSeconds() - LastLandedTimeSeconds;
	}

	AirChargeAttackEdAirborneLogs.Insert( Log, 0 );
	if ( AirChargeAttackEdAirborneLogs.Num() > AirChargeAttackEdAirborneLogMax )
	{
		AirChargeAttackEdAirborneLogs.SetNum( AirChargeAttackEdAirborneLogMax );
	}
}

bool UChargeActionPlayerModule_V2::IsChargeDashEndMovementLocked() const
{
	if ( !OwnerCharacter ) return false;

	// ED は OnEndAction 後の回復モーションなので、再生中のモンタージュが ED のときだけ対象
	UAnimMontage* EdMontage = GetAnimMontage( GetChargeDashEndAnimTag() );
	if ( !EdMontage || OwnerCharacter->GetCurrentMontage() != EdMontage ) return false;

	return !OwnerCharacter->HasStateTag( TAG_State_Player_MoveCancelable );
}

void UChargeActionPlayerModule_V2::UpdateChargeDashEndCancel()
{
	if ( !OwnerCharacter ) return;

	// この時点でアクションは終了済み（CurrentChargeActionType==None）なのでモンタージュで判定する
	UAnimMontage* EdMontage = GetAnimMontage( GetChargeDashEndAnimTag() );
	if ( !EdMontage || OwnerCharacter->GetCurrentMontage() != EdMontage ) return;

	// 手前の硬直中は RequestMove／CanDash 側が弾く
	if ( !OwnerCharacter->HasStateTag( TAG_State_Player_MoveCancelable ) ) return;

	const float InputThreshold = OwnerCharacter->PlayerParamData ? OwnerCharacter->PlayerParamData->DashCancelInputThreshold : 0.2f;
	if ( OwnerCharacter->GetRawMovementInput().Size() < InputThreshold ) return;

	OwnerCharacter->StopAnimMontage( 0.1f, EdMontage );
}

void UChargeActionPlayerModule_V2::UpdateChargeAnimState( float DeltaTime )
{
	if ( !bIsCharging || !OwnerCharacter ) return;

	UPlayerAnimInstance* AnimInst = Cast<UPlayerAnimInstance>( OwnerCharacter->GetMesh()->GetAnimInstance() );
	if ( !AnimInst ) return;

	if ( HasChargeStartMontageFinished( AnimInst ) )
	{
		HandleFinishedChargeStartState( AnimInst );
	}

	UpdateChargeBlendSpaceState( AnimInst, DeltaTime );
}


void UChargeActionPlayerModule_V2::UpdateChargeKeptByDodge()
{
	if ( !OwnerCharacter ) return;

	if ( OwnerCharacter->IsDodging() )
	{
		if ( CurrentChargeComboIndex > 1 )
		{
			ResetChargeComboIndex();
		}

		if ( ShouldKeepChargeByDodge() )
		{
			bIsChargeKeptByDodge = true;
		}
	}
	else if ( ShouldReleaseChargeKeptByDodge() )
	{
		bIsChargeKeptByDodge = false;
	}
}

bool UChargeActionPlayerModule_V2::ShouldSkipComboReset() const
{
	if ( CurrentChargeComboIndex <= 1 && CurrentChargeGearIndex <= 1 )
	{
		return true;
	}

	if ( IsEffectivelyInAir() )
	{
		return true;
	}

	return false;
}

bool UChargeActionPlayerModule_V2::ShouldPreserveChargeComboState() const
{
	if ( OwnerCharacter->IsAttacking() && !IsPlayingChargeAttackMontage() )
	{
		return false;
	}

	// ブースト中断中は CurrentChargeActionType が None に戻るため、保護しないとギア／コンボ段数がリセットされ、
	// ブースト終了後の再開が触れた瞬間より低いギアになる
	if ( bResumeChargeDashAfterBoost )
	{
		return true;
	}

	// ホップの着地ダッシュ継続待ちも同じ理由。地上かつ IsPlayingChargeAction() が false になるため、
	// 保護しないとダッシュ再開前にギアが 1 へ戻る
	if ( bResumeChargeDashOnLanding && ResumeChargeDashRemainingTime > 0.0f )
	{
		return true;
	}

	if ( bIsCharging || IsPlayingChargeAction() || OwnerCharacter->IsAttacking() )
	{
		return true;
	}

	// インターバル中はモンタージュが途切れるため、明けに同じコンボ進行で継続できるよう維持する
	if ( IsHitbackComboIntervalActive() )
	{
		return true;
	}

	if ( CurrentChargeComboIndex > 1 && OwnerCharacter->HasStateTag( TAG_State_Player_CanCombo ) )
	{
		return true;
	}

	if ( bIsChargeKeptByDodge )
	{
		return true;
	}

	if ( EffectSubModule && EffectSubModule->IsGhostTrailCycleActive() )
	{
		return true;
	}

	return false;
}

void UChargeActionPlayerModule_V2::ResetChargeComboAndGearState()
{
	CurrentChargeComboIndex = 1;
	CurrentChargeGearIndex = 1;
	bIsGearShiftedByComboFinish = false;
}

bool UChargeActionPlayerModule_V2::ShouldClearPostActionChargeWindowForNormalAttack() const
{
	return OwnerCharacter->IsAttacking() && !IsPlayingChargeAttackMontage();
}

bool UChargeActionPlayerModule_V2::IsPostActionChargeWindowActive() const
{
	if ( bIsCharging || IsPlayingChargeAction() || CurrentChargeActionType != EChargeActionV2Type::None )
	{
		return true;
	}

	if ( IsPlayingChargeAttackMontage() ||
		( CurrentChargeComboIndex > 1 && OwnerCharacter->HasStateTag( TAG_State_Player_CanCombo ) ) )
	{
		return true;
	}

	return IsCurrentMontageKeepingPostActionWindowAlive();
}

bool UChargeActionPlayerModule_V2::IsCurrentMontageKeepingPostActionWindowAlive() const
{
	if ( !OwnerCharacter )
	{
		return false;
	}

	UAnimMontage* CurrentMontage = OwnerCharacter->GetCurrentMontage();
	// 未設定タグ（→null）と null 同士で誤マッチしないよう先頭で非 null をガードする
	return CurrentMontage != nullptr && (
		CurrentMontage == GetAnimMontage( PlayerAnimTags::CHARGE_JUMP_ST ) ||
		CurrentMontage == GetAnimMontage( PlayerAnimTags::CHARGE_JUMP_LP ) ||
		CurrentMontage == GetAnimMontage( PlayerAnimTags::CHARGE_HOP_ST ) ||
		CurrentMontage == GetAnimMontage( PlayerAnimTags::CHARGE_HOP_LP ) ||
		CurrentMontage == GetAnimMontage( GetChargeDashEndAnimTag() ) );
}

bool UChargeActionPlayerModule_V2::ShouldKeepChargeByDodge() const
{
	return bChargeInputHeld ||
		CurrentChargeGearIndex > 1 ||
		CurrentChargeComboIndex > 1 ||
		( EffectSubModule && EffectSubModule->HasActiveGhostTrails() );
}

bool UChargeActionPlayerModule_V2::ShouldReleaseChargeKeptByDodge() const
{
	return !bChargeInputHeld && ( !EffectSubModule || !EffectSubModule->HasActiveGhostTrails() );
}

bool UChargeActionPlayerModule_V2::HasMontageFinished( FName Tag ) const
{
	if ( !OwnerCharacter ) return false;

	UAnimMontage* Montage = GetAnimMontage( Tag );
	if ( UAnimInstance* AnimInst = OwnerCharacter->GetMesh()->GetAnimInstance() )
	{
		// 別モンタージュへ切り替わっているなら終了扱い
		if ( OwnerCharacter->GetCurrentMontage() != Montage ) return true;

		// 末尾で停止してポーズ保持しているケースも終了とみなす
		const float CurrentPos = AnimInst->Montage_GetPosition( Montage );
		return CurrentPos >= ( Montage->GetPlayLength() - 0.05f );
	}

	return false;
}

bool UChargeActionPlayerModule_V2::HasChargeJumpStartMontageFinished() const
{
	return HasMontageFinished( bIsChargeHopJump ? PlayerAnimTags::CHARGE_HOP_ST : PlayerAnimTags::CHARGE_JUMP_ST );
}

void UChargeActionPlayerModule_V2::TransitionChargeJumpToLoop()
{
	PlayAnimMontage( bIsChargeHopJump ? PlayerAnimTags::CHARGE_HOP_LP : PlayerAnimTags::CHARGE_JUMP_LP );
	OwnerCharacter->ReserveAutoDashOnLanding();
	bIsChargeJumpInLoop = true;
}

bool UChargeActionPlayerModule_V2::HasAirChargeAttackStartMontageFinished() const
{
	if ( !OwnerCharacter ) return false;

	UAnimMontage* AtkStartMontage = GetAnimMontage( GetAirDiveAttackStartTag() );
	if ( UAnimInstance* AnimInst = OwnerCharacter->GetMesh()->GetAnimInstance() )
	{
		if ( OwnerCharacter->GetCurrentMontage() != AtkStartMontage )
		{
			return true;
		}

		const float CurrentPos = AnimInst->Montage_GetPosition( AtkStartMontage );
		const float Length = AtkStartMontage->GetPlayLength();
		return CurrentPos >= ( Length - 0.05f );
	}

	return false;
}

void UChargeActionPlayerModule_V2::TransitionAirChargeAttackToLoop()
{
	const UTidePlayerParamDataAsset* PlayerParams = GetPlayerParams();

	// 横ダイブ（空中チャージ攻撃）は専用の LP 再生速度を適用する
	const float LoopRate = ( IsRevampAirDiveAttack() && PlayerParams ) ? PlayerParams->AirChargeAttackLoopPlayRate : 1.0f;
	PlayAnimMontage( GetAirDiveAttackLoopTag(), LoopRate );
	bIsAirChargeAttackInLoop = true;

	// 真下プランジは縦ダイブのみ。横ダイブの前進は UpdateChargeDashLockedMovement が維持する
	if ( ShouldAirDivePlunge() )
	{
		const float PlungeSpeed = OwnerCharacter->PlayerParamData->AirChargeAttackPlungeSpeed;
		OwnerCharacter->LaunchCharacter( FVector( 0.0f, 0.0f, PlungeSpeed ), false, true );
	}

	// 突進開始のこのタイミングにロック対象が居れば、角度に依らずロック部位へ直接突進する。
	// 対象・持続時間はここで確定し、以降 UpdateAirChargeAttackDive が毎フレーム対象を追う
	bIsAirChargeAttackLockOnRush = false;
	AirChargeAttackRushTarget = nullptr;
	AirChargeAttackDiveTimer.Clear();
	if ( IsRevampAirDiveAttack() && PlayerParams )
	{
		if ( PlayerParams->bEnableAirChargeAttackLockOnRush && OwnerCharacter->IsLockOnActive() && OwnerCharacter->GetLockOnComponent() != nullptr )
		{
			if ( ULockOnTargetComponent* LockedComp = OwnerCharacter->GetLockOnComponent()->GetTarget() )
			{
				bIsAirChargeAttackLockOnRush = true;
				AirChargeAttackRushTarget = LockedComp;
			}
		}

		// 0 なら未設定＝無制限（着地・壁ヒットまで）
		const float DiveMaxTime = bIsAirChargeAttackLockOnRush
			? PlayerParams->AirChargeAttackLockOnRushMaxTime
			: PlayerParams->AirChargeAttackDiveMaxTime;
		if ( DiveMaxTime > 0.0f )
		{
			AirChargeAttackDiveTimer.Set( DiveMaxTime );
		}

		BeginAirChargeAttackDiveLog();
	}
}

bool UChargeActionPlayerModule_V2::HasChargeStartMontageFinished( UPlayerAnimInstance* AnimInst ) const
{
	if ( !OwnerCharacter || !AnimInst ) return false;

	UAnimMontage* StMontage = GetAnimMontage( GetChargeStartAnimTag() );
	if ( OwnerCharacter->GetCurrentMontage() != StMontage )
	{
		return true;
	}

	const float CurrentPos = AnimInst->Montage_GetPosition( StMontage );
	const float Length = StMontage->GetPlayLength();
	return CurrentPos >= ( Length - 0.05f );
}

void UChargeActionPlayerModule_V2::HandleFinishedChargeStartState( UPlayerAnimInstance* AnimInst )
{
	if ( IsEffectivelyInAir() )
	{
		UAnimMontage* AirChargeLp = GetAnimMontage( PlayerAnimTags::AIRCHARGE_LP );
		if ( OwnerCharacter->GetCurrentMontage() != AirChargeLp )
		{
			PlayAnimMontage( PlayerAnimTags::AIRCHARGE_LP );
		}
		return;
	}

	if ( !AnimInst->bIsChargeBSPlaying )
	{
		AnimInst->bIsChargeBSPlaying = true;
		AnimInst->ChargeComboIndex = CurrentChargeComboIndex;
	}
}

void UChargeActionPlayerModule_V2::UpdateChargeBlendSpaceState( UPlayerAnimInstance* AnimInst, float DeltaTime )
{
	if ( !AnimInst->bIsChargeBSPlaying )
	{
		return;
	}

	if ( IsEffectivelyInAir() )
	{
		AnimInst->bIsChargeBSPlaying = false;
		return;
	}

	UpdateChargeBlendSpaceDirection( AnimInst, DeltaTime );
	UpdateChargeBlendSpacePhase( AnimInst, DeltaTime );
}

void UChargeActionPlayerModule_V2::UpdateChargeBlendSpaceDirection( UPlayerAnimInstance* AnimInst, float DeltaTime ) const
{
	UCharacterMovementComponent* MovementComp = OwnerCharacter->GetCharacterMovement();
	if ( !MovementComp )
	{
		return;
	}

	const FVector InputVector = MovementComp->GetLastInputVector();
	float TargetDirection = 0.0f;
	if ( !InputVector.IsNearlyZero() )
	{
		const FRotator Rotation = OwnerCharacter->GetActorRotation();
		TargetDirection = UKismetAnimationLibrary::CalculateDirection( InputVector, Rotation );
	}

	const float DirectionInterpSpeed = OwnerCharacter->PlayerParamData->ChargingDirectionInterpSpeed;
	const FRotator CurrentRot( 0.0f, AnimInst->ChargeDirection, 0.0f );
	const FRotator TargetRot( 0.0f, TargetDirection, 0.0f );
	const FRotator InterpedRot = FMath::RInterpTo( CurrentRot, TargetRot, DeltaTime, DirectionInterpSpeed );
	AnimInst->ChargeDirection = InterpedRot.Yaw;
}

void UChargeActionPlayerModule_V2::UpdateChargeBlendSpacePhase( UPlayerAnimInstance* AnimInst, float DeltaTime ) const
{
	constexpr float StateTransitionSpeed = 2.0f;
	if ( AnimInst->ChargeState < 1.0f )
	{
		AnimInst->ChargeState = FMath::FInterpConstantTo( AnimInst->ChargeState, 1.0f, DeltaTime, StateTransitionSpeed );
	}
}

void UChargeActionPlayerModule_V2::UpdateChargeDashBlendSpace( float DeltaTime )
{
	if ( !OwnerCharacter ) return;
	// 駆動本体はキャラクター側の共通ヘルパ（ブーストダッシュと共用）
	OwnerCharacter->UpdateChargeDashLoopBlendSpace( CurrentChargeGearIndex, DeltaTime );
}

void UChargeActionPlayerModule_V2::StopChargeDashBlendSpace()
{
	if ( !OwnerCharacter ) return;
	OwnerCharacter->StopChargeDashLoopBlendSpace();
}

void UChargeActionPlayerModule_V2::BeginCharge()
{
	bChargeInputHeld = true;
	// この溜めで押下ラッチを使い切る（bRequireChargeRepressToRestart が OFF なら参照されない）
	bRequireChargeRepress = true;

	// R2 を握り直したらヒットバック自動継続は中断する
	bPendingChargeComboAttack = false;
	HitbackComboIntervalTimer.Clear();

	if ( !OwnerCharacter ) return;
	const UTidePlayerParamDataAsset* PlayerParams = GetPlayerParams();
	if ( !PlayerParams ) return;

	InitializeChargeStartFlags();

	// 直後の InterruptCurrentChargeAction()→ResetChargedActionState() でクリアされるので退避する
	const bool bFromHitCancel = bIsChargeFromHitCancel;

	if ( bFromHitCancel )
	{
		HitCancelChargingSteeringTimer.Set( PlayerParams->HitCancelChargingSteeringDuration );
		HitCancelChargeActionLockTimer.Set( PlayerParams->HitCancelChargeActionLockDuration );
	}

	UpdateChargeStartComboState();

	InterruptCurrentChargeAction();
	PrepareOwnerForCharge();

	// ST モーションの再生速度や ST 中のチャージアクション禁止判定で参照されるため復元する
	bIsChargeFromHitCancel = bFromHitCancel;

	StartChargeState();
	ActivateChargeStartPresentation();
}

void UChargeActionPlayerModule_V2::ReleaseCharge()
{
	bChargeInputHeld = false;
	bBlockChargeInputUntilRelease = false;

	if ( !bIsCharging ) return;

	// 構え中の R2 は神技の発動ボタン。溜めは凍結保持し、
	// モーション／エフェクト／ギアカメラを触らない（構え中に Push/Pop が走るのを避ける）
	if ( OwnerCharacter && OwnerCharacter->IsGodArtSelecting() )
	{
		bChargeFrozenByGodArtStance = true;
		return;
	}

	ExecuteChargeReleaseAction();

	bIsCharging = false;
}

void UChargeActionPlayerModule_V2::UpdateGodArtStanceChargeFreeze()
{
	if ( !bChargeFrozenByGodArtStance ) return;
	if ( !OwnerCharacter || OwnerCharacter->IsGodArtSelecting() ) return;

	bChargeFrozenByGodArtStance = false;

	// 構えが終わった時点で R2 を握り直していれば、そのまま通常の溜めへ戻す（離したら発動する）
	if ( bChargeInputHeld ) return;

	// 離したままなら発動はさせず、ここで溜めを畳む（カメラ Pop も構えの切れ目に寄せる）
	DiscardChargeForGodArtStance();
}

void UChargeActionPlayerModule_V2::DiscardChargeForGodArtStance()
{
	if ( !OwnerCharacter ) return;

	// 進行中のチャージダッシュは構え中の凍結で温存して構え明けに戻すため、
	// ResetChargedActionState（CancelCharge）は呼ばない
	UAnimMontage* ChargeStartMontage = GetAnimMontage( GetChargeStartAnimTag() );
	if ( ChargeStartMontage && OwnerCharacter->GetCurrentMontage() == ChargeStartMontage )
	{
		OwnerCharacter->StopAnimMontage( ChargeStartMontage );
	}

	ClearChargeState();	// bIsCharging / ギアカメラ / チャージ BS をここで落とす
	if ( EffectSubModule ) EffectSubModule->DestroyActiveChargeEffect();
	OwnerCharacter->RefreshMovementParams();
}

void UChargeActionPlayerModule_V2::DeferChargeReleaseForLaunchLock()
{
	// ボタンは離された扱いにして溜めの成長を止め、発動は封印解除まで後回しにする。
	// ギア／溜めは bIsCharging を true のまま凍結して保持する
	bChargeInputHeld = false;
	bBlockChargeInputUntilRelease = false;

	if ( !bIsCharging ) return;

	bPendingLaunchChargeRelease = true;
}

void UChargeActionPlayerModule_V2::FlushPendingChargeRelease()
{
	if ( !bPendingLaunchChargeRelease ) return;

	bPendingLaunchChargeRelease = false;

	if ( !bIsCharging ) return;

	// 解除時点で構えに入っていれば発動させない（ReleaseCharge と同じ扱い＝溜めを凍結保持）
	if ( OwnerCharacter && OwnerCharacter->IsGodArtSelecting() )
	{
		bChargeFrozenByGodArtStance = true;
		return;
	}

	ExecuteChargeReleaseAction();

	bIsCharging = false;
}

void UChargeActionPlayerModule_V2::ResumeHeldChargeAfterLaunchLock()
{
	// 「動けるようになったら勝手にチャージ」を満たすため状態ゲート（IsChargeStartStateValid）は見ずに
	// 強制開始し、判定は物理押下で行う
	if ( !OwnerCharacter || OwnerCharacter->IsDead() ) return;
	if ( !bChargeInputPhysicallyHeld || bIsCharging || bBlockChargeInputUntilRelease ) return;
	if ( OwnerCharacter->IsLaunchAscending() ) return;	// 呼び出し側が頂点まで持ち越すが、経路増加への保険
	// 滑空中／滑空の長押しに R2 を使う区間では、溜めへ移すと滑空を奪ってしまう
	if ( OwnerCharacter->IsInGlideSession() ) return;
	if ( OwnerCharacter->IsAirActionLimitedAfterAirCharge() ) return;

	BeginCharge();
}

bool UChargeActionPlayerModule_V2::IsPlayingChargeAction() const
{
	if ( !ChargedActionLockTimer.IsFinish() ) return true;

	if ( CurrentChargeActionType == EChargeActionV2Type::GuardBrake ) return true;

	if ( CurrentChargeActionType == EChargeActionV2Type::Jump )
	{
		// 落下（LP）に入っていればアクション中判定を外して BeginCharge を許可する
		if ( bIsChargeJumpInLoop ) return false;
		return true;	// 上昇（ST）中は弾く
	}

	if ( IsAnyAirDiveAttackActive() )
	{
		return true;
	}

	// 空中チャージダッシュは着地後 ED を待つ間 ChargedActionLockTimer を先に止めるため、
	// ここで拾わないとアクション中判定が早期に外れて割り込みを許す
	if ( CurrentChargeActionType == EChargeActionV2Type::Dash && bIsAirChargeDash )
	{
		return true;
	}

	return false;
}

bool UChargeActionPlayerModule_V2::IsPlayingChargeDashStartMontage() const
{
	if ( !OwnerCharacter ) return false;
	UAnimMontage* CurrentMontage = OwnerCharacter->GetCurrentMontage();
	UAnimMontage* DashStartMontage = GetAnimMontage( GetChargeDashStartAnimTag() );
	return CurrentMontage != nullptr && CurrentMontage == DashStartMontage;
}

float UChargeActionPlayerModule_V2::GetChargedActionLockRemainingTime() const
{
	return ChargedActionLockTimer.Get();
}

FLinearColor UChargeActionPlayerModule_V2::GetCurrentGearColor() const
{
	switch ( CurrentChargeGearIndex )
	{
	case 2: return FLinearColor( 1.0f, 1.0f, 0.4f, 1.0f ); // 弐：マイルドな黄色
	case 3: return FLinearColor( 1.0f, 0.5f, 0.0f, 1.0f ); // 参：鮮やかなオレンジ
	default:
		if ( CurrentChargeGearIndex >= 4 )
		{
			return FLinearColor( 1.0f, 0.1f, 0.1f, 1.0f ); // 極：はっきりとした赤
		}
		return FLinearColor( 0.95f, 0.95f, 0.85f, 1.0f );   // 壱：アイボリー
	}
}

float UChargeActionPlayerModule_V2::GetCurrentChargeEffectSpeed() const
{
	switch ( CurrentChargeGearIndex )
	{
	case 1:		return 0.5f;
	case 2:		return 0.75f;
	case 3:		return 1.0f;
	default:	return 1.0f;
	}
}

bool UChargeActionPlayerModule_V2::RequestChargeDash()
{
	if ( IsPlayingChargeAction() ) return false;

	// 回数上限に達している／受付猶予切れならダッシュを発動せず溜めだけ解除する
	if ( IsEffectivelyInAir() && !CanStartAirChargeDashNow() )
	{
		CancelCharge( false );
		return false;
	}

	if ( !CanExecuteChargedAction() )
	{
		CancelCharge( false );	// 閾値未満
		return false;
	}

	OnStartChargeDash();
	ClearChargeState();
	return true;
}

bool UChargeActionPlayerModule_V2::RequestChargeAttackCombo()
{
	if ( TryStartChargeAttackFromCombo() )
	{
		return true;
	}

	if ( TryStartChargeAttackFromDash() )
	{
		return true;
	}

	// 通常攻撃からの派生時は、条件を満たすときだけコンボ段数をリセットしておく（ここでは攻撃を開始しない）
	ResetChargeComboForNormalAttackDerivation();

	return TryStartFreshChargeAttack();
}

bool UChargeActionPlayerModule_V2::RequestChargeAttackLegacy()
{
	if ( CurrentChargeActionType == EChargeActionV2Type::Attack && !ChargedActionLockTimer.IsFinish() )
	{
		return false;
	}

	if ( TryStartChargeAttackFromDash() )
	{
		return true;
	}

	if ( OwnerCharacter->IsAttacking() )
	{
		if ( !OwnerCharacter->HasStateTag( TAG_State_Player_CanAttack ) )
		{
			return false;
		}
	}
	else if ( IsPlayingChargeAction() || CurrentChargeActionType == EChargeActionV2Type::Jump )
	{
		return false;
	}

	return TryStartFreshChargeAttack();
}

bool UChargeActionPlayerModule_V2::TryStartChargeAttackFromDash()
{
	if ( !IsPlayingChargeDash() )
	{
		return false;
	}

	const bool bWasAirDash = IsEffectivelyInAir();
	CancelCharge( false );
	ResetChargeComboIndex();
	ChargeAttackTriggerSource = TEXT( "ダッシュ派生" );	// 計測ログ用
	OnStartChargeAttack( bWasAirDash );
	EffectSubModule->SpawnActiveChargeEffect();
	return true;
}

bool UChargeActionPlayerModule_V2::TryStartChargeAttackFromCombo()
{
	const bool bIsPlayingChargeAtkMontage = IsPlayingChargeAttackMontage();
	if ( ( CurrentChargeActionType == EChargeActionV2Type::Attack && !ChargedActionLockTimer.IsFinish() ) || bIsPlayingChargeAtkMontage )
	{
		if ( IsWithinChargeActionTagResidueWindow() )
		{
			return false;
		}

		if ( !OwnerCharacter->HasStateTag( TAG_State_Player_CanCombo ) )
		{
			return false;
		}

		IncrementChargeComboIndex();
		ChargeAttackTriggerSource = TEXT( "コンボ（CanCombo）" );	// 計測ログ用
		OnStartChargeAttack();
		ClearChargeState();
		return true;
	}

	return false;
}

void UChargeActionPlayerModule_V2::ResetChargeComboForNormalAttackDerivation()
{
	if ( !OwnerCharacter ) return;

	// 通常攻撃モンタージュ中かつ攻撃/コンボ受付タグがある場合のみ戻す（開始判定は呼び出し側に委ねる）
	if ( !OwnerCharacter->IsAttacking() || IsPlayingChargeAttackMontage() )
	{
		return;
	}

	if ( OwnerCharacter->HasStateTag( TAG_State_Player_CanAttack ) ||
		OwnerCharacter->HasStateTag( TAG_State_Player_CanCombo ) )
	{
		ResetChargeComboIndex();
	}
}

bool UChargeActionPlayerModule_V2::TryStartFreshChargeAttack()
{
	if ( !CanExecuteChargedAction() )
	{
		return false;
	}

	if ( CurrentChargeComboIndex <= 1 && !OwnerCharacter->IsAttacking() )
	{
		ResetChargeComboIndex();
	}

	ChargeAttackTriggerSource = TEXT( "新規（溜め解放・単発）" );	// 計測ログ用
	OnStartChargeAttack();
	ClearChargeState();
	return true;
}

bool UChargeActionPlayerModule_V2::TryStartChargeJumpFromDash()
{
	if ( !IsPlayingChargeDash() )
	{
		return false;
	}

	const UTidePlayerParamDataAsset* PlayerParams = GetPlayerParams();

	// 面沿い中の幅跳びは通常のチャージジャンプへ差し替える（壁からの脱出手段）。幅跳びのままだとチャージが
	// 途切れず面沿いが解除されないうえ、射出も床法線基準に傾いて壁沿いに跳んで乗り続けてしまう
	const bool bReleaseSurfaceRide = PlayerParams && PlayerParams->bSurfaceRideChargeHopBecomesChargeJump
		&& OwnerCharacter && OwnerCharacter->IsSurfaceRiding();
	const bool bUseDashJump = PlayerParams && PlayerParams->bEnableChargeHop && !bReleaseSurfaceRide;

	// 面沿い解除ジャンプでも着地後のダッシュ継続は予約する（解除したいのは面沿いだけ）
	const bool bReserveDashResume = bUseDashJump || bReleaseSurfaceRide;

	// CancelCharge でクリアされる前に残り時間を退避しておく
	const float RemainingDashTime = bReserveDashResume ? ChargedActionLockTimer.Get() : 0.0f;

	// CancelCharge → OnStartChargeJump の Pop/Push を抑止して着地→ダッシュ継続まで同じカメラを使う
	// （CancelCharge より前に立てる必要がある）
	bKeepDashCameraDuringHop = bUseDashJump && RemainingDashTime > 0.0f;

	CancelCharge( false );

	// ダッシュ由来のジャンプはギア壱固定。OnStartChargeJump 内で resume 予約はクリアされる
	OnStartChargeJump( bUseDashJump );

	if ( bReserveDashResume && RemainingDashTime > 0.0f )
	{
		bResumeChargeDashOnLanding = true;
		ResumeChargeDashRemainingTime = RemainingDashTime;
		// 上の CancelCharge → ResetChargedActionState で 0 に戻っているので 1 を入れ直す。
		// 面沿い解除ジャンプは幅跳びの連鎖ではないので数えない
		if ( bUseDashJump ) ChargeHopChainCount = 1;
	}

	EffectSubModule->SpawnActiveChargeEffect();
	return true;
}

bool UChargeActionPlayerModule_V2::TryStartFreshChargeJump()
{
	if ( IsPlayingChargeAction() )
	{
		return false;
	}

	if ( !CanExecuteChargedAction() )
	{
		CancelCharge();
		return false;
	}

	OnStartChargeJump();
	ClearChargeState();
	return true;
}

bool UChargeActionPlayerModule_V2::UpdateReboundSliding( float DeltaTime )
{
	if ( !bIsReboundSliding )
	{
		return false;
	}

	const FVector2D RawInput = OwnerCharacter->GetRawMovementInput();
	ReboundRecoveryTimer.Update( DeltaTime );

	FVector SteerDir = FVector::ZeroVector;
	if ( !RawInput.IsNearlyZero() )
	{
		SteerDir = OwnerCharacter->GetControlRelativeInputDirection( RawInput ).GetSafeNormal();
	}

	if ( UCharacterMovementComponent* MovementComp = OwnerCharacter->GetCharacterMovement() )
	{
		const float SkidFriction = OwnerCharacter->PlayerParamData ? OwnerCharacter->PlayerParamData->ChargeReboundBrakingDeceleration : 1500.0f;

		if ( !SteerDir.IsNearlyZero() )
		{
			const float SteerPower = OwnerCharacter->PlayerParamData ? OwnerCharacter->PlayerParamData->ChargeReboundSteerPower : 1500.0f;
			CurrentReboundVelocity += SteerDir * SteerPower * DeltaTime;
		}

		CurrentReboundVelocity = FMath::VInterpConstantTo( CurrentReboundVelocity, FVector::ZeroVector, DeltaTime, SkidFriction );

		if ( ReboundRecoveryTimer.IsFinish() && !RawInput.IsNearlyZero() )
		{
			bIsReboundSliding = false;
			bIsReboundDash = true;
			CurrentChargeActionType = EChargeActionV2Type::Dash;
			CurrentDashSpeed = CachedChargeDashSpeed * 0.3f;
			OwnerCharacter->SetActorRotation( SteerDir.Rotation() );
			return true;
		}

		MovementComp->Velocity.X = CurrentReboundVelocity.X;
		MovementComp->Velocity.Y = CurrentReboundVelocity.Y;

		if ( MovementComp->IsMovingOnGround() )
		{
			MovementComp->Velocity.Z = 0.0f;
		}

		if ( ReboundRecoveryTimer.IsFinish() && CurrentReboundVelocity.IsNearlyZero() )
		{
			bIsReboundSliding = false;
			OnEndAction( true );
		}
	}

	return true;
}

void UChargeActionPlayerModule_V2::UpdateChargeDashLockedMovement( float DeltaTime )
{
	// 吸着対象へ当たるまではレバー操舵をロックし、対象方向へ向け続ける（速度は向きに追従）
	if ( bChargeDashHomingRotationLock )
	{
		if ( ChargeDashHomingTarget.IsValid() )
		{
			const FVector MyLoc = OwnerCharacter->GetActorLocation();
			const FVector TargetLoc = ChargeDashHomingTarget->GetTargetLocation();
			const FVector ToTarget = ( TargetLoc - MyLoc ).GetSafeNormal2D();
			if ( !ToTarget.IsNearlyZero() )
			{
				OwnerCharacter->SetActorRotation( ToTarget.Rotation() );
			}

			const float ReachDist = OwnerCharacter->PlayerParamData ? OwnerCharacter->PlayerParamData->ChargeDashHomingReachDistance : 150.0f;
			if ( FVector::Dist2D( TargetLoc, MyLoc ) <= ReachDist )
			{
				bChargeDashHomingRotationLock = false;
				ChargeDashHomingTarget = nullptr;
			}
		}
		else
		{
			bChargeDashHomingRotationLock = false;	// 対象が消失（破壊等）
		}
	}

	if ( UCharacterMovementComponent* MovementComp = OwnerCharacter->GetCharacterMovement() )
	{
		if ( OwnerCharacter->IsPushingPawn() )
		{
			MovementComp->Velocity.X = 0.0f;
			MovementComp->Velocity.Y = 0.0f;
			OwnerCharacter->SetActorLocation( OwnerCharacter->GetPushTargetSafetyLocation(), false );
			return;
		}

		if ( bIsReboundDash )
		{
			const float AccelRate = OwnerCharacter->PlayerParamData ? OwnerCharacter->PlayerParamData->ChargeReboundAccelationRate : 2000.0f;
			CurrentDashSpeed += AccelRate * DeltaTime;
			CurrentDashSpeed = FMath::Min( CurrentDashSpeed, CachedChargeDashSpeed );
		}
		else
		{
			// 初速バースト：Power から持続速度 Speed へ指定秒数かけて減衰させる（0 なら即 Speed）
			const float BurstDuration = OwnerCharacter->PlayerParamData ? OwnerCharacter->PlayerParamData->ChargeDashSpeedBurstDuration : 0.0f;
			if ( BurstDuration > 0.0f )
			{
				const float SpeedDiff = FMath::Abs( CachedChargeDashPower - CachedChargeDashSpeed );
				const float DecayRate = SpeedDiff / BurstDuration;	// cm/s per 秒
				CurrentDashSpeed = FMath::FInterpConstantTo( CurrentDashSpeed, CachedChargeDashSpeed, DeltaTime, DecayRate );
			}
			else
			{
				CurrentDashSpeed = CachedChargeDashSpeed;
			}
		}

		// 平滑化済み法線は空中で上向きへ寄っていくため、接地/空中で分岐せず常に投影してよい。
		// 分岐させると歩行⇔落下が切り替わる瞬間に進行方向がスナップし、壁を擦るたびに「ガクッ」となる
		const FVector FloorNormal = OwnerCharacter->GetSmoothedFloorNormal();
		FVector CurrentDir = FVector::VectorPlaneProject( OwnerCharacter->GetActorForwardVector(), FloorNormal ).GetSafeNormal();

		// ブースト→ダッシュ再開直後は、進行方向を「中断前のブースト速度の向き」から機体前方向へ寄せる。
		// ハードセットだと、低摩擦の滑りで向きがズレていた場合に再開の瞬間へ直角スナップする
		float AppliedDashSpeed = CurrentDashSpeed;

		if ( !BoostResumeTurnEaseTimer.IsFinish() )
		{
			BoostResumeTurnEaseTimer.Update( DeltaTime );

			const float EasedRate = FMath::InterpEaseInOut( 0.0f, 1.0f, BoostResumeTurnEaseTimer.GetRate(), 2.0f );

			const FVector TargetDir2D = CurrentDir.GetSafeNormal2D();
			if ( !BoostResumeInitialVelDir.IsNearlyZero() && !TargetDir2D.IsNearlyZero() )
			{
				const FQuat SmoothedQuat = FQuat::Slerp( BoostResumeInitialVelDir.ToOrientationQuat(), TargetDir2D.ToOrientationQuat(), EasedRate );
				CurrentDir = SmoothedQuat.GetForwardVector();
			}

			// 大きさも同様に寄せる。即セットだとブースト速度が速い場合に一気に落ちて「ガクッ」となる
			AppliedDashSpeed = FMath::Lerp( BoostResumeInitialSpeed, CurrentDashSpeed, EasedRate );

			// 回頭速度も再開直後だけ遅くし、イージング終了までに本来の値へ戻す
			if ( OwnerCharacter->PlayerParamData )
			{
				const float StartYaw = OwnerCharacter->PlayerParamData->BoostResumeChargeDashTurnRateYaw;
				const float EndYaw = OwnerCharacter->GetChargeDashRotationRateYaw();
				MovementComp->RotationRate.Yaw = FMath::Lerp( StartYaw, EndYaw, EasedRate );
			}

			if ( BoostResumeTurnEaseTimer.IsFinish() )
			{
				OwnerCharacter->RefreshMovementParams();
			}
		}
		else if ( !GodArtDashResumeEaseTimer.IsFinish() )
		{
			// 構えで凍結している間にブレーキで落ちた速度から、本来のダッシュ速度へ戻す。
			// 向きは構え中も固定（RequestMove が封じられている）ので、大きさだけ寄せれば足りる
			GodArtDashResumeEaseTimer.Update( DeltaTime );

			const float EasedRate = FMath::InterpEaseOut( 0.0f, 1.0f, GodArtDashResumeEaseTimer.GetRate(), 2.0f );
			AppliedDashSpeed = FMath::Lerp( GodArtDashResumeInitialSpeed, CurrentDashSpeed, EasedRate );
		}

		// 下のワールド XY 固定は「上方向＝真上」前提なので、壁面に立っていると進行方向が破綻する
		if ( OwnerCharacter->IsSurfaceRiding() )
		{
			const FVector Up = OwnerCharacter->GetSurfaceRideUp();
			const FVector TangentDir = FVector::VectorPlaneProject( CurrentDir, Up ).GetSafeNormal();
			if ( !TangentDir.IsNearlyZero() )
			{
				// 面法線方向の速度（貼り付き・跳ね）は CMC に任せ、接平面成分だけ置き換える。毎フレーム接線速度を
				// ハードセットするため、アシストエリアの登り伸ばしは加速加算ではなく倍率で効かせる
				const FVector NormalVel = Up * FVector::DotProduct( MovementComp->Velocity, Up );
				const float AssistScale = OwnerCharacter->GetSurfaceRideUphillAssistDashSpeedScale();
				MovementComp->Velocity = TangentDir * ( AppliedDashSpeed * AssistScale ) + NormalVel;
			}
			return;
		}

		// 床面へ投影した進行方向は単位ベクトルなので、急斜面ほど成分が Z へ回って水平成分が縮む（85°で 1 割以下）。
		// そのまま XY へ入れると触れた瞬間に「ガクッ」と減速するので、向きだけ取り大きさは維持する
		FVector HorizontalDir = CurrentDir.GetSafeNormal2D();
		if ( HorizontalDir.IsNearlyZero() )
		{
			HorizontalDir = OwnerCharacter->GetActorForwardVector().GetSafeNormal2D();
		}

		const float Preserve = OwnerCharacter->PlayerParamData
			? FMath::Clamp( OwnerCharacter->PlayerParamData->ChargeDashSlopeSpeedPreserve, 0.0f, 1.0f )
			: 1.0f;
		const float HorizontalScale = FMath::Lerp( CurrentDir.Size2D(), 1.0f, Preserve );	// 0 で従来どおり縮む

		const FVector NewVelocity = HorizontalDir * ( AppliedDashSpeed * HorizontalScale );
		MovementComp->Velocity.X = NewVelocity.X;
		MovementComp->Velocity.Y = NewVelocity.Y;
	}
}

bool UChargeActionPlayerModule_V2::TryUpdateChargeDashTurn( float DeltaTime )
{
	if ( !OwnerCharacter || !OwnerCharacter->PlayerParamData ) return false;

	UAnimMontage* TurnMontage = GetAnimMontage( PlayerAnimTags::CHARGE_DASH_TURN );
	if ( !TurnMontage ) return false;

	// 再生中はロック移動を止めてルートモーションに任せ、出口方向の微調整だけ回頭補助で効かせる
	if ( OwnerCharacter->GetCurrentMontage() == TurnMontage )
	{
		// 面沿いエリア内では回頭補助を止める：Yaw だけの姿勢代入が面へ傾けた姿勢を毎フレーム潰してしまう
		if ( !OwnerCharacter->IsInSurfaceRideZone() )
		{
			UpdateChargeDashTurnSteering( DeltaTime );
		}
		return true;
	}

	// 地上の通常チャージダッシュのみ対象
	if ( bIsAirChargeDash || bChargeDashHomingRotationLock || bIsReboundSliding || bIsReboundDash ) return false;
	// 判定も旋回も水平前提なので面へ傾けた姿勢と噛み合わず、TurnBrakeRate の急制動で壁の途中で失速する。
	// 面沿い中ではなくエリア在籍で見るのは、乗る前や一時解除の区間でもターンが挟まると助走が切れるため
	if ( OwnerCharacter->IsInSurfaceRideZone() ) return false;
	if ( OwnerCharacter->IsFalling() ) return false;

	UCharacterMovementComponent* MovementComp = OwnerCharacter->GetCharacterMovement();
	if ( !MovementComp ) return false;

	const FVector2D RawInput = OwnerCharacter->GetRawMovementInput();
	if ( RawInput.IsNearlyZero() ) return false;

	// 判定式は走り／ダッシュと同じだが、パラメータはチャージダッシュ専用のものを使う
	const FPlayerTurnParams TurnParams = OwnerCharacter->PlayerParamData->ResolveTurnParams( OwnerCharacter->PlayerParamData->ChargeDashTurnParams );

	if ( MovementComp->Velocity.Size2D() < TurnParams.MinSpeedForTurn ) return false;

	const FVector WorldInput = OwnerCharacter->GetControlRelativeInputDirection( RawInput ).GetSafeNormal();
	const FVector CurrentDir = MovementComp->Velocity.GetSafeNormal2D();
	if ( FVector::DotProduct( WorldInput, CurrentDir ) >= TurnParams.TurnThresholdDot ) return false;

	// 慣性を削ってルートモーションの移動に任せる
	PlayAnimMontage( PlayerAnimTags::CHARGE_DASH_TURN );
	if ( UPlayerAnimInstance* AnimInst = Cast<UPlayerAnimInstance>( OwnerCharacter->GetMesh()->GetAnimInstance() ) )
	{
		AnimInst->SetRootMotionMode( ERootMotionMode::RootMotionFromMontagesOnly );
		AnimInst->bIsTurning = true;
	}
	MovementComp->Velocity *= TurnParams.TurnBrakeRate;
	return true;
}

void UChargeActionPlayerModule_V2::UpdateChargeDashTurnSteering( float DeltaTime )
{
	const float SteerRateYaw = OwnerCharacter->PlayerParamData->ChargeDashTurnSteerRateYaw;
	if ( SteerRateYaw <= 0.0f ) return;

	const FVector2D RawInput = OwnerCharacter->GetRawMovementInput();
	if ( RawInput.IsNearlyZero() ) return;

	const FVector WorldInput = OwnerCharacter->GetControlRelativeInputDirection( RawInput ).GetSafeNormal();
	if ( WorldInput.IsNearlyZero() ) return;

	const FRotator CurrentRot = OwnerCharacter->GetActorRotation();
	const FRotator TargetRot = WorldInput.Rotation();
	const FRotator NewRot = FMath::RInterpConstantTo( CurrentRot, TargetRot, DeltaTime, SteerRateYaw );
	OwnerCharacter->SetActorRotation( FRotator( 0.0f, NewRot.Yaw, 0.0f ) );
}

bool UChargeActionPlayerModule_V2::ShouldFreezeChargeDashForGodArt() const
{
	// 自由移動モードは直前アクションをそのまま継続させる方針なので触らない（構えブレーキ側と揃える）
	return OwnerCharacter && OwnerCharacter->IsGodArtStanceMovementLocked();
}

void UChargeActionPlayerModule_V2::BeginGodArtDashResumeEase()
{
	GodArtDashResumeEaseTimer.Clear();
	GodArtDashResumeInitialSpeed = 0.0f;

	const UTidePlayerParamDataAsset* PlayerParams = GetPlayerParams();
	if ( !OwnerCharacter || !PlayerParams || PlayerParams->GodArtStanceDashResumeEaseTime <= 0.0f ) return;

	const UCharacterMovementComponent* MovementComp = OwnerCharacter->GetCharacterMovement();
	if ( !MovementComp ) return;

	GodArtDashResumeInitialSpeed = MovementComp->Velocity.Size2D();
	GodArtDashResumeEaseTimer.Set( PlayerParams->GodArtStanceDashResumeEaseTime );
}

bool UChargeActionPlayerModule_V2::ShouldDeferChargedActionEnd() const
{
	if ( CurrentChargeActionType == EChargeActionV2Type::Jump && IsEffectivelyInAir() )
	{
		return true;
	}

	if ( IsAnyAirDiveAttackActive() )
	{
		return true;
	}

	// 空中チャージダッシュは ST 終了で継続せず終了＝通常落下（MaxAirChargeDashCount>=2 なら落下中の再チャージで次を出す）
	return false;
}

void UChargeActionPlayerModule_V2::UpdateDashLoopAllowance( const FChargePropulsionSettings& Settings )
{
	if ( CurrentChargeActionType == EChargeActionV2Type::Dash )
	{
		float StartMontageLength = 0.0f;
		if ( UAnimMontage* StartMontage = GetAnimMontage( GetChargeDashStartAnimTag() ) )
		{
			StartMontageLength = StartMontage->GetPlayLength();
		}

		// ロック時間がダッシュ ST モーション長より長ければ LP（ループ）再生を許可する
		bAllowDashLoop = ( Settings.LockTime > StartMontageLength );
		return;
	}

	bAllowDashLoop = false;
}

FVector UChargeActionPlayerModule_V2::BuildChargeDashDirection( ULockOnTargetComponent*& OutTarget ) const
{
	OutTarget = nullptr;

	FVector DashDirection = FVector::ZeroVector;
	const FVector2D RawInput = OwnerCharacter->GetRawMovementInput();

	if ( !RawInput.IsNearlyZero() )
	{
		DashDirection = OwnerCharacter->GetControlRelativeInputDirection( RawInput ).GetSafeNormal();
	}
	else
	{
		const FRotator ActorRot = OwnerCharacter->GetActorRotation();
		const FRotator YawRot( 0.0f, ActorRot.Yaw, 0.0f );
		DashDirection = YawRot.Vector();
	}

	// 解決した方向を扇の中心に吸着させる。攻撃／ダッシュで独立した距離・角度パラメータを使う
	// （範囲内に対象が居なければ素通り）
	if ( CurrentChargeActionType == EChargeActionV2Type::Attack )
	{
		DashDirection = GetHomingDirection( DashDirection );
	}
	else if ( CurrentChargeActionType == EChargeActionV2Type::Dash )
	{
		// 空中は自由な方向制御を優先し、吸着しない（滑空へ繋ぐ動きに敵吸着が絡むと暴れるため）
		const bool bSkipHomingForAirDash = bIsAirChargeDash;
		if ( !bSkipHomingForAirDash )
		{
			// 探索中心はスティック入力方向。キャラ正面基準だと、到達前にキャンセルして再ダッシュしたとき
			// 旋回が追いつかず同じ敵を再ロックしてしまう。入力が無いときだけカメラ正面で補助的に探す
			const FVector SearchCenter = !RawInput.IsNearlyZero()
				? DashDirection
				: FRotator( 0.0f, OwnerCharacter->GetControlRotation().Yaw, 0.0f ).Vector();

			ULockOnTargetComponent* DashTarget = nullptr;
			const FVector HomedDir = GetChargeDashHomingDirection( SearchCenter, 0.0f, &DashTarget );

			// 見つかれば吸着方向を優先。居なければレバー／正面のまま
			if ( DashTarget )
			{
				DashDirection = HomedDir;
				OutTarget = DashTarget;
			}
		}
	}

	return DashDirection;
}

void UChargeActionPlayerModule_V2::ApplyChargeLaunchVelocity( const FVector& DashDirection, float FinalDashPower, bool bShouldLaunch )
{
	if ( bShouldLaunch )
	{
		OwnerCharacter->LaunchCharacter( DashDirection * FinalDashPower, true, true );
		return;
	}

	if ( UCharacterMovementComponent* MovementComp = OwnerCharacter->GetCharacterMovement() )
	{
		MovementComp->Velocity.X = 0.0f;
		MovementComp->Velocity.Y = 0.0f;
	}
}

bool UChargeActionPlayerModule_V2::RequestChargeAttack()
{
	const UTidePlayerParamDataAsset* PlayerParams = GetPlayerParams();
	if ( !OwnerCharacter || !PlayerParams ) return false;

	// 攻撃連打・R2 リリースのどちらもここへ集まるので、空中ダイブ中はこの 1 箇所で塞ぐ
	if ( IsAirNormalDiveAttack() || IsAirChargeAttackActionLocked() ) return false;

	if ( PlayerParams->bEnableAttackButtonChargeCombo )
	{
		return RequestChargeAttackCombo();
	}

	return RequestChargeAttackLegacy();
}

bool UChargeActionPlayerModule_V2::RequestChargeJump()
{
	// 通常ジャンプと同じタグを見て、打ち上げの放物線を打ち消せないようにする
	if ( OwnerCharacter && OwnerCharacter->HasStateTag( TAG_State_Player_CannotJump ) )
	{
		return false;
	}

	if ( bHasUsedChargeJump )
	{
		return false;
	}

	// ジャンプ後の空中からは出させない（RequestJumpStart 等、バッファを介さない経路の保険）
	if ( IsChargeJumpBlockedAfterJump() )
	{
		return false;
	}

	if ( TryStartChargeJumpFromDash() )
	{
		return true;
	}

	return TryStartFreshChargeJump();
}

void UChargeActionPlayerModule_V2::OnCharacterHit( const FHitResult& Hit )
{
	if ( !IsPlayingChargeAction() ) return;

	// ジャンプ中は止めず、ダッシュと攻撃の突進中のみ対象とする
	if ( CurrentChargeActionType != EChargeActionV2Type::Attack &&
		CurrentChargeActionType != EChargeActionV2Type::Dash ) return;

	if ( !OwnerCharacter || !OwnerCharacter->PlayerParamData ) return;

	// 法線の上向き具合を見て、地面（斜面）との接触は壁衝突とみなさない
	UCharacterMovementComponent* MovementComp = OwnerCharacter->GetCharacterMovement();
	if ( !MovementComp ) return;

	if ( Hit.Normal.Z >= MovementComp->GetWalkableFloorZ() )
	{
		return;
	}

	// 突進中に壁へ当たったら打ち切って通常落下へ移す。放置すると壁に押し付けたまま重力 0 で
	// 落ちも着地もできずハングする（リバウンドへは進めない）
	if ( IsRevampAirDiveAttack() && bIsAirChargeAttackInLoop && !bAirChargeAttackDiveWallStuck )
	{
		bAirChargeAttackDiveWallStuck = true;
		MovementComp->Velocity = FVector::ZeroVector;	// 壁を押し続けず重力で素直に落下させる
		RecordAirChargeAttackDiveEnd( TEXT( "壁ヒット" ), 0.0f );
		RestoreUprightAfterAirChargeAttackDive();
		OwnerCharacter->RefreshMovementParams();	// GravityScale=0 を通常へ戻す
		OwnerCharacter->EnterJumpFallingLoop();		// 突進ポーズのまま落ちないようにする
		return;
	}

	// 遅すぎる／かすり（角度が浅すぎる）なら跳ね返らない
	const float CurrentSpeed = MovementComp->Velocity.Size();
	if ( CurrentSpeed < OwnerCharacter->PlayerParamData->MinReboundSpeed )
	{
		return;
	}

	const FVector ForwardDir = OwnerCharacter->GetActorForwardVector();
	const float DotResult = FVector::DotProduct( ForwardDir, Hit.Normal );
	if ( DotResult > -OwnerCharacter->PlayerParamData->MinReboundAngleCos )
	{
		return;
	}

	const float StopThreshold = OwnerCharacter->PlayerParamData->ChargeMoveStopThreshold;
	if ( DotResult < StopThreshold )
	{
		ExecuteRebound( Hit.Normal );
	}
}

void UChargeActionPlayerModule_V2::OnLanded()
{
	bHasUsedChargeJump = false;

	// 空中チャージ系の制限は着地でまとめて解除する
	AirChargeDashStageCount = 0;
	bAirborneByGroundChargeLaunch = false;
	bAirChargeAttackUsedThisAirtime = false;

	if ( OwnerCharacter && OwnerCharacter->GetWorld() )
	{
		LastLandedTimeSeconds = OwnerCharacter->GetWorld()->GetTimeSeconds();
	}

	// 突進が終わらないまま着地したケースもここで閉じる
	if ( IsRevampAirDiveAttack() )
	{
		RecordAirChargeAttackDiveLanding();
	}

	// 下の Jump 分岐で CHARGE_HOP_ED を経由するか終了するかの判定に使うため、先に確定させる
	const bool bChargeHopDashTimeRemains = bIsChargeHopJump && bResumeChargeDashOnLanding && ResumeChargeDashRemainingTime > 0.0f;
	const float ChargeHopInputThreshold = ( OwnerCharacter && OwnerCharacter->PlayerParamData ) ? OwnerCharacter->PlayerParamData->DashCancelInputThreshold : 0.2f;
	const bool bHasChargeHopMoveInput = OwnerCharacter && OwnerCharacter->GetRawMovementInput().Size() >= ChargeHopInputThreshold;
	const bool bWillContinueChargeHopDash = CurrentChargeActionType == EChargeActionV2Type::Jump && bChargeHopDashTimeRemains && bHasChargeHopMoveInput;

	// 滞空でいくら食われたかの区切り
	if ( bChargeHopSpeedMaintainActive && ChargeHopSpeedLogs.IsValidIndex( 0 ) && OwnerCharacter )
	{
		if ( const UCharacterMovementComponent* MovementComp = OwnerCharacter->GetCharacterMovement() )
		{
			ChargeHopSpeedLogs[0].LandingSpeed = MovementComp->Velocity.Size2D();
		}
	}

	// ホップ中はダッシュカメラを維持しているので無効ハンドルへの no-op になる
	if ( OwnerCharacter ) OwnerCharacter->RequestPopFirstJumpCamera();

	// 空中で終わった1回目ダッシュのために保持していたダッシュカメラをここで初めて Pop する
	// （FirstJump→ChargeDash の順で抜けるので Default へのブレンドは1回だけ）
	if ( bKeepChargeDashCameraUntilLanding )
	{
		PopChargeActionCamera();
		bKeepChargeDashCameraUntilLanding = false;
	}

	// 着地後に地上ダッシュへ継続するケースは ResetChargedActionState を経由しないため、
	// ここで Pop しないと地上でも専用カメラが残る
	StopAirChargeDashSwingCamera( true );

	// 死亡中の着地でアクションが復活して死亡モーションを上書きしないよう、予約だけ消費して抜ける
	if ( OwnerCharacter && OwnerCharacter->IsDead() )
	{
		bKeepDashCameraDuringHop = false;
		bResumeChargeDashOnLanding = false;
		ResumeChargeDashRemainingTime = 0.0f;
		return;
	}

	if ( CurrentChargeActionType == EChargeActionV2Type::Jump )
	{
		// ホップはダッシュ持続タイマーが残り、着地の瞬間に移動入力があれば CHARGE_HOP_ED を経由して
		// ダッシュへ継続する（継続/終了の判定は UpdateChargeHopEndCheck）
		if ( bIsChargeHopJump )
		{
			if ( bWillContinueChargeHopDash )
			{
				ChargedActionLockTimer.Clear();
				bChargeHopEdMontageStarted = false;
				PlayAnimMontage( PlayerAnimTags::CHARGE_HOP_ED );
				return;
			}

			// ここから先はホップ終了なので、維持していたダッシュカメラを通常どおり Pop させる
			bKeepDashCameraDuringHop = false;
			bResumeChargeDashOnLanding = false;
			ResumeChargeDashRemainingTime = 0.0f;

			if ( bHasChargeHopMoveInput )
			{
				// OnEndAction でホップのモーション・状態・カメラを片付けてから強制開始する
				OnEndAction();
				OwnerCharacter->ForceStartDash();
				return;
			}

			PlayAnimMontage( PlayerAnimTags::JUMP_ED );
			OnEndAction();
			return;
		}

		// 面沿い解除ジャンプも幅跳びと同じく着地でダッシュへ戻す。予約は TryStartChargeJumpFromDash が
		// 立てたものしか無い（通常のチャージジャンプは OnStartChargeJump がクリアする）
		if ( bResumeChargeDashOnLanding && ResumeChargeDashRemainingTime > 0.0f && bHasChargeHopMoveInput )
		{
			// 先に走る JumpActionPlayerModule::OnLanded が CHARGE_JUMP_ED＋End 状態＋着地オートダッシュ予約を
			// 張っているので打ち消す（放置すると移動入力で通常ダッシュへ化ける）
			UAnimMontage* ChargeJumpEdMontage = GetAnimMontage( PlayerAnimTags::CHARGE_JUMP_ED );
			if ( ChargeJumpEdMontage != nullptr && OwnerCharacter->GetCurrentMontage() == ChargeJumpEdMontage )
			{
				OwnerCharacter->StopAnimMontage( ChargeJumpEdMontage );
			}
			OwnerCharacter->ClearJumpAndLandingDash();

			// CHARGE_HOP_ED はホップ専用の絵なので経由せず、モーションはそのままダッシュへ繋ぐ
			ResumeChargeDashAfterLanding();
			return;
		}

		OnEndAction();
	}
	else if ( CurrentChargeActionType == EChargeActionV2Type::Dash && bIsAirChargeDash )
	{
		// 空中チャージダッシュは専用 ED を持たない。通常は JumpActionPlayerModule が JUMP_ED を再生済みだが、
		// ダッシュ本体（AirChargeDashStart）中の着地は JumpModule がジャンプ系と認識せず流さないため
		// 空中モーションが残る。流れていなければ End 状態へ引き継ぐ
		UAnimMontage* JumpEdMontage = GetAnimMontage( PlayerAnimTags::JUMP_ED );
		if ( JumpEdMontage == nullptr || OwnerCharacter->GetCurrentMontage() != JumpEdMontage )
		{
			OwnerCharacter->EnterJumpLandingEnd();
		}

		// bFromLanding を渡さないと OnEndAction が「空中で終わった」と誤認し（Landed は MOVE_Falling のまま
		// 呼ばれる）、落下ループの引き渡しやカメラ保持が上の JUMP_ED を潰す
		ChargedActionLockTimer.Clear();
		OnEndAction( true, true );
		return;
	}
	// 地上チャージダッシュが落下 → 着地（ロック時間が残ったまま着地）
	else if ( CurrentChargeActionType == EChargeActionV2Type::Dash && !bIsAirChargeDash )
	{
		// ロック時間が続く限り維持する。先に走る JumpModule が JUMP_ED（着地モーション＋End 状態＋着地オート
		// ダッシュ予約）へ移していれば打ち消して主導権を戻す（放置すると移動入力で通常ダッシュへ化ける）
		UAnimMontage* JumpEdMontage = GetAnimMontage( PlayerAnimTags::JUMP_ED );
		const bool bJumpModuleTookLanding = ( JumpEdMontage != nullptr && OwnerCharacter->GetCurrentMontage() == JumpEdMontage );
		if ( !ChargedActionLockTimer.IsFinish() && bJumpModuleTookLanding )
		{
			OwnerCharacter->StopAnimMontage( JumpEdMontage );
			OwnerCharacter->ClearJumpAndLandingDash();

			// 移動パラメータを戻し、Loop の BlendSpace を再開させる
			OwnerCharacter->RefreshMovementParams();
			if ( UPlayerAnimInstance* AnimInst = Cast<UPlayerAnimInstance>( OwnerCharacter->GetMesh()->GetAnimInstance() ) )
			{
				AnimInst->bIsChargeDashBSPlaying = true;
				AnimInst->ChargeDashGearIndex = CurrentChargeGearIndex;
			}
		}
		return;
	}
	else if ( IsAnyAirDiveAttackActive() )
	{
		// 突進で傾けていた場合、着地 ED 再生前に直立へ戻す
		RestoreUprightAfterAirChargeAttackDive();

		if ( UExCameraSubsystem* CameraSubsystem = GetCameraSubsystem() )
		{
			if ( AirAttackStartCameraHandle.IsValid() )
			{
				CameraSubsystem->PopCameraMode( AirAttackStartCameraHandle );
				AirAttackStartCameraHandle.Clear();
			}

			// ロックオン中は着地カメラも Push しない（ロックオンカメラのまま）
			if ( !AirAttackEndCameraHandle.IsValid() && !OwnerCharacter->IsLockOnActive() )
			{
				AirAttackEndCameraHandle = CameraSubsystem->PushThirdPersonCameraByKey( TEXT( "AirAttackEnd" ), TEXT( "PushAirAttackEnd" ) );
			}
		}

		// 終了判定はタグで行う。既に ED 再生中の再着地（跳ね・射出直後の即着地）では頭から鳴らし直さない
		UAnimMontage* EdMontage = GetAnimMontage( GetAirDiveAttackEndTag() );
		if ( EdMontage == nullptr || OwnerCharacter->GetCurrentMontage() != EdMontage )
		{
			PlayAnimMontage( GetAirDiveAttackEndTag() );
		}
	}
	// 空中での溜め中に着地
	else if ( bIsCharging && CurrentChargeActionType == EChargeActionV2Type::None )
	{
		UAnimMontage* CurrentMontage = OwnerCharacter->GetCurrentMontage();
		UAnimMontage* AirChargeSt = GetAnimMontage( PlayerAnimTags::AIRCHARGE_ST );
		UAnimMontage* AirChargeLp = GetAnimMontage( PlayerAnimTags::AIRCHARGE_LP );

		if ( CurrentMontage != nullptr && ( CurrentMontage == AirChargeSt || CurrentMontage == AirChargeLp ) )
		{
			OwnerCharacter->StopAnimMontage( 0.15f, CurrentMontage );	// 着地のカクつき防止にフェード
		}
	}
}

void UChargeActionPlayerModule_V2::ExecuteRebound( FVector HitNormal )
{
	if ( !OwnerCharacter || !OwnerCharacter->PlayerParamData ) return;
	if( !OwnerCharacter->PlayerParamData->bEnableChargeDashRebound )
	{
		return;
	}

	UCharacterMovementComponent* Movement = OwnerCharacter->GetCharacterMovement();
	if ( !Movement ) return;

	if ( bIsReboundSliding ) return;	// 重複実行の防止

	// 詰め中に壁ではじかれたら、対象詰めは中断してリバウンド滑りに委ねる
	ClearChargeAttackTimedApproach();

	const float CurrentSpeed = FMath::Max( Movement->Velocity.Size(), CachedChargeDashSpeed );
	const float FinalReboundPower = OwnerCharacter->PlayerParamData->ChargeBaseReboundPower + ( CurrentSpeed * OwnerCharacter->PlayerParamData->ChargeReboundMultiplier );

	// 反射ベクトル R = I - 2 * (I ⋅ N) * N を水平方向に絞って使う
	FVector DashDir = CachedChargeDashDirection.GetSafeNormal();
	FVector ReflectDir = DashDir - 2.0f * FVector::DotProduct( DashDir, HitNormal ) * HitNormal;
	ReflectDir.Z = 0.0f;
	ReflectDir = ReflectDir.GetSafeNormal();

	StopCurrentChargeMontages();
	PlayAnimMontage( GetChargeDashEndAnimTag() );

	CurrentReboundVelocity = ReflectDir * FinalReboundPower;
	const float RecoveryTime = OwnerCharacter->PlayerParamData ? OwnerCharacter->PlayerParamData->ChargeReboundRecoveryTime : 0.3f;
	ReboundRecoveryTimer.Set( RecoveryTime );

	bIsReboundSliding = true;
}

bool UChargeActionPlayerModule_V2::IsHitCancelChargeActionLocked() const
{
	return !HitCancelChargeActionLockTimer.IsFinish();
}

bool UChargeActionPlayerModule_V2::IsHitbackComboIntervalActive() const
{
	return !HitbackComboIntervalTimer.IsFinish();
}

void UChargeActionPlayerModule_V2::UpdatePendingHitbackComboAttack()
{
	if ( !bPendingChargeComboAttack ) return;
	if ( !OwnerCharacter ) { bPendingChargeComboAttack = false; return; }

	// R2 の握り直し／別アクションへの移行／被弾・死亡では自動継続を破棄する。
	// 構え中も対象＝構え中にインターバルが明けてチャージ攻撃が暴発するのを防ぐ
	if ( bChargeInputHeld ||
		OwnerCharacter->IsDodging() ||
		OwnerCharacter->IsDashing() ||
		OwnerCharacter->IsGodArtSelecting() ||
		OwnerCharacter->IsGodActionExecuting() ||
		OwnerCharacter->IsHitReacting() ||
		OwnerCharacter->IsDead() )
	{
		bPendingChargeComboAttack = false;
		HitbackComboIntervalTimer.Clear();
		return;
	}

	if ( IsHitbackComboIntervalActive() ) return;	// インターバル消化中はまだ待機

	bPendingChargeComboAttack = false;

	// インターバル明けは CanCombo 窓が閉じていても発動する。攻撃不能状態のときだけ破棄する。
	// 空中チャージ攻撃中は RequestChargeAttack を通らないこの経路も封じる
	if ( IsAirChargeAttackActionLocked() ) return;
	if ( !IsPlayingChargeAttackMontage() && !CanStartChargeAttackComboFromCurrentState() ) return;

	ContinueHitbackChargeCombo();
}

void UChargeActionPlayerModule_V2::ContinueHitbackChargeCombo()
{
	const UTidePlayerParamDataAsset* PlayerParams = GetPlayerParams();
	if ( !OwnerCharacter || !PlayerParams ) return;

	// HandleChargeComboAttackInput と同じ分岐。最終段かつ R2 非ホールドなら弱攻撃で締める
	if ( CurrentChargeComboIndex == PlayerParams->MaxChargeComboCount && !bChargeInputHeld )
	{
		CancelCharge( false );
		OwnerCharacter->CancelAttack();

		CurrentChargeComboIndex = 1;
		CurrentChargeGearIndex = 1;

		OnStartLightAttack();
		if ( EffectSubModule ) EffectSubModule->ResetGhostTrailSpawnTimer();
		return;
	}

	IncrementChargeComboIndex();
	ChargeAttackTriggerSource = TEXT( "ヒットバック予約" );	// 計測ログ用
	OnStartChargeAttack();
	ClearChargeState();
}

bool UChargeActionPlayerModule_V2::CanExecuteChargedAction() const
{
	const UTidePlayerParamDataAsset* PlayerParams = GetPlayerParams();
	if ( !OwnerCharacter || !PlayerParams )
	{
		return false;
	}

	if ( IsHitCancelChargeActionLocked() )
	{
		return false;
	}

	// 以下 2 つは溜め開始側でも弾いているが、攻撃派生など溜め無しで発動できる経路の保険としてここでも塞ぐ
	if ( OwnerCharacter->IsAirActionLimitedAfterAirCharge() )
	{
		return false;
	}
	if ( OwnerCharacter->IsGodArtSelecting() )
	{
		return false;
	}

	// コンボ2段目以降／攻撃ステート・コンボ枠からの派生はコンボ中とみなし、最低溜め時間を無視して即発動する
	const bool bIsDerivingFromAttack = OwnerCharacter->IsAttacking() ||
		OwnerCharacter->HasStateTag( TAG_State_Player_CanCombo );

	if ( CurrentChargeComboIndex >= 2 || bIsDerivingFromAttack )
	{
		return true;
	}

	return CurrentChargeTimer.GetElapsed() >= /*PlayerParams->MinChargeTimeForDash*/0.0f;
}

void UChargeActionPlayerModule_V2::ClearChargeState()
{
	bIsCharging = false;
	// 溜めが消える経路（キャンセル／死亡等）では、打ち上げの後回し予約と構えの凍結保持も破棄する
	bPendingLaunchChargeRelease = false;
	bChargeFrozenByGodArtStance = false;
	bIsShortenedChargeActive = false;
	CurrentChargeTimer.Clear();
	ChargeShiftTimer.Clear();
	// ステアリング制限は UpdateChargingState でしか進まないため、溜めが切れると残り時間が凍結する。
	// 畳んでおかないと、次の（ヒットキャンセル由来でない）溜めの頭で残りぶんの制限が復活してしまう
	HitCancelChargingSteeringTimer.Clear();
	if ( EffectSubModule ) EffectSubModule->StopChargeBoostEffect();

	if ( OwnerCharacter )
	{
		if ( UPlayerAnimInstance* AnimInst = Cast<UPlayerAnimInstance>( OwnerCharacter->GetMesh()->GetAnimInstance() ) )
		{
			AnimInst->bIsChargeBSPlaying = false;
		}
	}

	EffectSubModule->DeactivateChargeEffect();

	if ( GearShiftCameraHandle.IsValid() )
	{
		if ( UExCameraSubsystem* CameraSubsystem = GetCameraSubsystem() )
		{
			CameraSubsystem->PopCameraMode( GearShiftCameraHandle );
		}
		GearShiftCameraHandle.Clear();
	}
}

void UChargeActionPlayerModule_V2::ResetChargedActionState()
{
	// UpdateGravityLock は「タイマーが自然終了した瞬間」だけ復元するため、Clear して打ち切ると
	// GravityScale=0 と正の Velocity.Z が残って「ふわっと浮いたまま落ちてこない」
	const bool bWasGravityLockActive = !GravityLockTimer.IsFinish() || IsRevampAirDiveAttack();

	ChargedActionLockTimer.Clear();
	GravityLockTimer.Clear();
	CurrentChargeActionType = EChargeActionV2Type::None;
	bAllowDashLoop = false;
	bChargeDashHomingRotationLock = false;
	ChargeDashHomingTarget = nullptr;
	bReserveDashOnLanding = false;
	bResumeChargeDashOnLanding = false;
	ResumeChargeDashRemainingTime = 0.0f;
	// 面沿い落下のジャンプモーション状態も畳む（モンタージュ自体は次のアクションが張り替えるか、
	// 着地の StopFinishedChargeJumpMontages が止める）
	bDashFallChargeJumpMotion = false;
	bDashFallChargeJumpStStarted = false;
	bDashFallChargeJumpInLoop = false;
	bDashFallChargeJumpLanding = false;
	DashFallLandingEdTimer.Clear();
	// 着地ED のジャンプキャンセルで繋ぐ経路はこの Reset を挟むため、呼び出し側が退避・復元している
	ChargeHopChainCount = 0;
	bResumeChargeDashAfterBoost = false;
	ChargeDashResumeTimeAfterBoost = 0.0f;
	// 途中のイージング（ブースト再開の旋回・構え凍結からの復帰）は持ち越さない
	BoostResumeTurnEaseTimer.Clear();
	bChargeDashFrozenByGodArt = false;
	GodArtDashResumeEaseTimer.Clear();
	GodArtDashResumeInitialSpeed = 0.0f;
	bIsAirChargeAttack = false;
	bIsAirNormalAttack = false;
	bIsAirChargeAttackInLoop = false;
	AirChargeAttackHoverTimer.Clear();
	// リセットしないと、2回目の発動直後に UpdateAirChargeAttackEndCheck の
	// 「ED が外れた＝終了」分岐が誤発火して即 OnEndAction されてしまう
	bAirChargeAttackEdMontageStarted = false;
	bAirChargeAttackDiveWallStuck = false;
	bAirChargeAttackDiveEnded = false;
	bIsAirChargeAttackLockOnRush = false;
	AirChargeAttackRushTarget = nullptr;
	AirChargeAttackDiveTimer.Clear();
	bIsAirChargeDash = false;
	// AirChargeDashStageCount はここでは消さない（落下中の次ダッシュ判定に跨って必要。リセットは OnLanded）
	bIsChargeHopJump = false;
	bChargeHopEdMontageStarted = false;
	ResetChargeHopSpeedMaintain();	// 連続幅跳びは次の発射で張り直される

	// 別チャージアクションが割り込んだら破棄する
	// （OnEndAction はこの Reset の後で BeginAirDashEndInertia を呼ぶため順序は競合しない）
	AirDashEndInertiaTimer.Clear();

	bIsReboundSliding = false;
	CurrentReboundVelocity = FVector::ZeroVector;
	bIsReboundDash = false;
	bIsHitCancelableToCharge = false;
	bIsChargeFromHitCancel = false;

	ClearChargeAttackTimedApproach();
	ChargeApproachMeasuredSpeed2D = 0.0f;	// 次の通常攻撃のとどめへ持ち越さない

	if ( OwnerCharacter )
	{
		if ( UPlayerAnimInstance* AnimInst = Cast<UPlayerAnimInstance>( OwnerCharacter->GetMesh()->GetAnimInstance() ) )
		{
			AnimInst->bIsChargeBSPlaying = false;
			AnimInst->ChargeState = 0.0f;
			AnimInst->bIsChargeDashBSPlaying = false;
		}

		if ( bWasGravityLockActive )
		{
			OwnerCharacter->RefreshMovementParams();	// 浮きっぱなし防止
		}

		// 破棄は接地時だけここで行う。空中で Pop すると直後の ChargeDash Pop が FirstJump を飛ばして
		// 一気に Default へブレンドし、まだ空中なのに構図が飛ぶ（空中での破棄は OnLanded が担う）
		if ( !OwnerCharacter->IsFalling() )
		{
			OwnerCharacter->RequestPopFirstJumpCamera();
		}
	}

	// 遅延 Pop 予約中は予約側へ任せ、ホップ中のカメラ維持と空中終了ダッシュの着地保持では Pop しない
	// （激しい瞬間のブレンドを避けるため。後者は OnLanded が Pop する）
	if ( !bPendingChargeActionCameraPop && !bKeepDashCameraDuringHop && !bKeepChargeDashCameraUntilLanding )
	{
		PopChargeActionCamera();
	}

	if ( EffectSubModule ) EffectSubModule->DestroyChargedDashWindEffect();

	if ( UExCameraSubsystem* CameraSubsystem = GetCameraSubsystem() )
	{
		if ( AirAttackStartCameraHandle.IsValid() )
		{
			CameraSubsystem->PopCameraMode( AirAttackStartCameraHandle );
			AirAttackStartCameraHandle.Clear();
		}
		if ( AirAttackEndCameraHandle.IsValid() )
		{
			CameraSubsystem->PopCameraMode( AirAttackEndCameraHandle, TEXT( "PopAirAttackEnd" ) );
			AirAttackEndCameraHandle.Clear();
		}
	}

	// 寄せ切る前に終わっても、目標アングルへ同期してスイングバックを防ぐ
	StopAirChargeDashSwingCamera( true );
}

void UChargeActionPlayerModule_V2::CancelCharge( bool bRestoreDash )
{
	if ( !OwnerCharacter ) return;
	FinishChargeCancel( bRestoreDash );
}

bool UChargeActionPlayerModule_V2::TryConsumeGuardBrakeBlock()
{
	return GuardBrakeSubModule ? GuardBrakeSubModule->TryConsumeBlock() : false;
}

void UChargeActionPlayerModule_V2::ExecuteManualCancel()
{
	if ( GuardBrakeSubModule ) GuardBrakeSubModule->SetInputHeld( true );

	// チャージダッシュ中、またはチャージ溜め中であればガードブレーキの対象にする
	const bool bIsCancelableToBrake = ( CurrentChargeActionType == EChargeActionV2Type::Dash ) || bIsCharging;

	if ( bIsCancelableToBrake )
	{
		// 空中はガードブレーキに移行せず、ただのキャンセル
		if ( IsEffectivelyInAir() )
		{
			CancelCharge( false );
			EffectSubModule->DestroyActiveChargeEffect();
			return;
		}

		// 溜め中から移行した場合は R2 を離すまで再チャージを禁止する
		if ( bIsCharging )
		{
			bBlockChargeInputUntilRelease = true;
		}

		ResetChargedActionState();
		CurrentChargeActionType = EChargeActionV2Type::GuardBrake;
		if ( GuardBrakeSubModule ) GuardBrakeSubModule->ResetBlockConsumed();	// 1回耐えを張り直す

		if ( OwnerCharacter )
		{
			UAnimMontage* DashStMontage = GetAnimMontage( GetChargeDashStartAnimTag() );
			UAnimMontage* ChargeStMontage = GetAnimMontage( GetChargeStartAnimTag() );

			if ( DashStMontage )   OwnerCharacter->StopAnimMontage( DashStMontage );
			if ( ChargeStMontage ) OwnerCharacter->StopAnimMontage( ChargeStMontage );

			if ( UPlayerAnimInstance* AnimInst = Cast<UPlayerAnimInstance>( OwnerCharacter->GetMesh()->GetAnimInstance() ) )
			{
				AnimInst->bIsDashBSPlaying = false;
			}

			PlayAnimMontage( PlayerAnimTags::CHARGE_BRAKE_ST );

			OwnerCharacter->CaptureFrictionRecoveryStartParams();

			if ( UCharacterMovementComponent* MovementComp = OwnerCharacter->GetCharacterMovement() )
			{
				// キャンセルした瞬間の速度（ダッシュの最高速 or 溜め中の慣性）を初速として保持する
				if ( GuardBrakeSubModule ) GuardBrakeSubModule->SetBrakeVelocity( MovementComp->Velocity );
			}

			OwnerCharacter->RefreshMovementParams();

			if ( const UTidePlayerParamDataAsset* PlayerParams = GetPlayerParams() )
			{
				FrictionRecoveryTimer.Set( PlayerParams->ChargeGuardBrakeSlideTime );
				bIsAirFrictionRecovery = false;
			}
			OwnerCharacter->UpdateFrictionRecovery( 0.0f );
		}

		ClearChargeState();
		EffectSubModule->DestroyActiveChargeEffect();
		return;
	}

	// それ以外（攻撃中など）はハードキャンセル
	if ( !bIsCharging && CurrentChargeActionType == EChargeActionV2Type::None ) return;

	CancelCharge( false );
	bBlockChargeInputUntilRelease = true;
	EffectSubModule->DestroyActiveChargeEffect();
}

void UChargeActionPlayerModule_V2::ReleaseManualCancel()
{
	if ( GuardBrakeSubModule ) GuardBrakeSubModule->SetInputHeld( false );

	if ( CurrentChargeActionType == EChargeActionV2Type::GuardBrake )
	{
		if ( OwnerCharacter )
		{
			UAnimMontage* BrakeSt = GetAnimMontage( PlayerAnimTags::CHARGE_BRAKE_ST );
			UAnimMontage* BrakeLp = GetAnimMontage( PlayerAnimTags::CHARGE_BRAKE_LP );
			OwnerCharacter->StopAnimMontage( BrakeSt );
			OwnerCharacter->StopAnimMontage( BrakeLp );
		}

		// チョイ押し ＆ 移動入力ありならダッシュへ移行する
		const UTidePlayerParamDataAsset* PlayerParams = GetPlayerParams();
		const float QuickTapWindow = PlayerParams ? PlayerParams->ChargeGuardBrakeDashCancelWindow : 0.25f;
		const float InputThreshold = PlayerParams ? PlayerParams->DashCancelInputThreshold : 0.2f;

		const FVector2D RawInput = OwnerCharacter->GetRawMovementInput();

		// FrictionRecoveryTimer はブレーキ開始と同時にセットされるので、GetElapsed() が押下時間になる
		const bool bIsQuickTap = FrictionRecoveryTimer.GetElapsed() <= QuickTapWindow;
		const bool bHasMovementInput = RawInput.Size() >= InputThreshold;

		if ( bIsQuickTap && bHasMovementInput )
		{
			CurrentChargeActionType = EChargeActionV2Type::None;
			OwnerCharacter->ForceStartDash();
			return;
		}

		PlayAnimMontage( PlayerAnimTags::CHARGE_BRAKE_ED );
	}
}

void UChargeActionPlayerModule_V2::OnEndAction( bool bSkipEdAnimation, bool bFromLanding )
{
	const EChargeActionV2Type FinishedActionType = CurrentChargeActionType;
	const bool bWasAirChargeAttack = bIsAirChargeAttack;	// ResetChargedActionState で false に戻るため先に退避
	const bool bWasAirNormalAttack = bIsAirNormalAttack;	// 同上（空中通常攻撃＝振り下ろし）
	const bool bWasAirChargeDash = bIsAirChargeDash;		// 同上

	// 真ならダッシュ用カメラを着地まで保持して激しい瞬間のブレンドを避ける
	// （Pop ゲートがこのフラグを見るので ResetChargedActionState より前に立てる）。
	// 着地由来は IsFalling() では弾けない——ProcessLanded は MOVE_Falling のまま Landed() を呼ぶため、
	// bFromLanding で明示的に弾かないと落下ループの引き渡しが走って JUMP_ED を上書きしてしまう
	const bool bAirDashEndInAir = ( FinishedActionType == EChargeActionV2Type::Dash && bWasAirChargeDash
		&& !bFromLanding && OwnerCharacter && OwnerCharacter->IsFalling() );
	if ( bAirDashEndInAir )
	{
		bKeepChargeDashCameraUntilLanding = true;
	}

	// 即 Pop するとモーションの途中で通常カメラへ戻るため、カメラの戻りだけ後ろ倒しする
	const UTidePlayerParamDataAsset* PlayerParams = GetPlayerParams();
	const float CameraHoldTime = PlayerParams ? PlayerParams->ChargeAttackCameraHoldTime : 0.0f;
	if ( FinishedActionType == EChargeActionV2Type::Attack && !bWasAirChargeAttack && !bWasAirNormalAttack
		&& ChargeActionCameraHandle.IsValid() && CameraHoldTime > 0.0f )
	{
		bPendingChargeActionCameraPop = true;
		ChargeActionCameraHoldTimer.Set( CameraHoldTime );
	}

	// 幅跳びの終了も同じ遅延 Pop に乗せる（即 Pop すると跳ぶたびに Pop→Push のブレンドでガタつく）。
	// bIsChargeHopJump は下の Reset でクリアされるので前に判定する
	const float HopCameraDelay = PlayerParams ? PlayerParams->ChargeHopCameraPopDelayTime : 0.0f;
	if ( FinishedActionType == EChargeActionV2Type::Jump && bIsChargeHopJump
		&& ChargeActionCameraHandle.IsValid() && HopCameraDelay > 0.0f )
	{
		bPendingChargeActionCameraPop = true;
		ChargeActionCameraHoldTimer.Set( HopCameraDelay );
	}

	// 空中チャージ攻撃の直後は振り下ろしへ繋げられないよう待ち時間を置く（着地しても消化し切るまで継続する）
	const float DiveAttackCooldown = PlayerParams ? PlayerParams->AirChargeAttackToDiveAttackCooldown : 0.0f;
	if ( bWasAirChargeAttack && DiveAttackCooldown > 0.0f )
	{
		AirChargeAttackDiveAttackCooldownTimer.Set( DiveAttackCooldown );
	}

	ResetChargedActionState();

	if ( OwnerCharacter )
	{
		if ( FinishedActionType == EChargeActionV2Type::Jump )
		{
			StopFinishedChargeJumpMontages();
		}

		// 空中チャージダッシュは着地時に OnLanded が ED を再生済みなので地上向けの終了処理はしない
		if ( FinishedActionType == EChargeActionV2Type::Dash && !bIsCharging && !bWasAirChargeDash )
		{
			HandleFinishedChargeDashEnd( bSkipEdAnimation );
		}

		StartPostActionFrictionRecovery( FinishedActionType );
	}

	if ( !bIsCharging )
	{
		ClearChargeState();
	}
	CurrentChargeActionType = EChargeActionV2Type::None;
	EffectSubModule->DestroyActiveChargeEffect();

	// 空中のまま ST 終了で通常落下へ移った瞬間、残っている水平ダッシュ速度を DA 設定で減衰させる（EaseOut）
	if ( bAirDashEndInAir )
	{
		// 滑空へ繋げた場合は移動・重力を滑空側が握るので、落下ループの引き渡しも終了慣性も要らない
		if ( !OwnerCharacter->RequestGlideAfterAirChargeDash() )
		{
			// 終了時は ZUp の上向き速度が残り、JumpModule のがけ落下検知は Velocity.Z <= 0 まで
			// JUMP_LP を流さないため、その間モーションが無くなる
			OwnerCharacter->EnterJumpFallingLoop();

			BeginAirDashEndInertia();
		}
	}
}

void UChargeActionPlayerModule_V2::BeginAirDashEndInertia()
{
	AirDashEndInertiaTimer.Clear();

	const UTidePlayerParamDataAsset* PlayerParams = GetPlayerParams();
	if ( !PlayerParams || PlayerParams->AirChargeDashEndInertiaTime <= 0.0f ) return;	// 無効＝従来どおり水平速度を残す
	if ( !OwnerCharacter ) return;

	UCharacterMovementComponent* MovementComp = OwnerCharacter->GetCharacterMovement();
	if ( !MovementComp ) return;

	// 速度がほぼ無い（向きが取れない）ときは機体前方向で代用する
	const FVector HorizVel( MovementComp->Velocity.X, MovementComp->Velocity.Y, 0.0f );
	AirDashEndInertiaInitialSpeed = HorizVel.Size();
	AirDashEndInertiaDir = HorizVel.GetSafeNormal();
	if ( AirDashEndInertiaDir.IsNearlyZero() )
	{
		AirDashEndInertiaDir = OwnerCharacter->GetActorForwardVector().GetSafeNormal2D();
	}

	// 初速倍率（終了直後にガクッと落としたいとき用。1.0＝そのまま）
	AirDashEndInertiaInitialSpeed *= FMath::Clamp( PlayerParams->AirChargeDashEndInertiaStartRate, 0.0f, 1.0f );

	AirDashEndInertiaTimer.Set( PlayerParams->AirChargeDashEndInertiaTime );
}

void UChargeActionPlayerModule_V2::UpdateAirDashEndInertia( float DeltaTime )
{
	if ( AirDashEndInertiaTimer.IsFinish() ) return;

	if ( !OwnerCharacter )
	{
		AirDashEndInertiaTimer.Clear();
		return;
	}

	// 別チャージアクション開始／着地／滑空開始で即終了し、通常のエアコントロールへ返す
	if ( CurrentChargeActionType != EChargeActionV2Type::None || OwnerCharacter->IsInGlideSession() || !OwnerCharacter->IsFalling() )
	{
		AirDashEndInertiaTimer.Clear();
		return;
	}

	const UTidePlayerParamDataAsset* PlayerParams = GetPlayerParams();
	UCharacterMovementComponent* MovementComp = OwnerCharacter->GetCharacterMovement();
	if ( !PlayerParams || !MovementComp )
	{
		AirDashEndInertiaTimer.Clear();
		return;
	}

	AirDashEndInertiaTimer.Update( DeltaTime );

	const float TargetRate = FMath::Clamp( PlayerParams->AirChargeDashEndInertiaTargetRate, 0.0f, 1.0f );
	const float SpeedMul = FMath::InterpEaseOut( 1.0f, TargetRate, AirDashEndInertiaTimer.GetRate(), 2.0f );
	const float DesiredSpeed = AirDashEndInertiaInitialSpeed * SpeedMul;

	// 向きはエアコントロールでの操舵結果を優先し、大きさだけを合わせる（Z＝落下は物理に任せる）
	FVector Dir = FVector( MovementComp->Velocity.X, MovementComp->Velocity.Y, 0.0f ).GetSafeNormal();
	if ( Dir.IsNearlyZero() )
	{
		Dir = AirDashEndInertiaDir;
	}
	else
	{
		AirDashEndInertiaDir = Dir;	// 追従した向きを保持
	}

	MovementComp->Velocity.X = Dir.X * DesiredSpeed;
	MovementComp->Velocity.Y = Dir.Y * DesiredSpeed;
}

FName UChargeActionPlayerModule_V2::GetChargeStartAnimTag() const
{
	if ( !OwnerCharacter ) return PlayerAnimTags::CHARGE_01_ST;

	if ( IsEffectivelyInAir() )
	{
		return PlayerAnimTags::AIRCHARGE_ST;
	}

	switch ( CurrentChargeComboIndex )
	{
	case 1: return PlayerAnimTags::CHARGE_01_ST;
	case 2: return PlayerAnimTags::CHARGE_02_ST;
	case 3: return PlayerAnimTags::CHARGE_03_ST;
	case 4: return PlayerAnimTags::CHARGE_04_ST;
	default: return PlayerAnimTags::CHARGE_01_ST;
	}
}

void UChargeActionPlayerModule_V2::OnStartChargeDash()
{
	InitializeChargeDashAction();
	EffectSubModule->SpawnChargeReleaseEffect();
	ConsumeGustChargeBuffIfArmed();
	ExecuteChargePropulsion();

	// 寿命（ChargeV2DashLockTimes）は秒基準でモーション長を見ないため、ST が長いと切れる。
	// データ側で尺を詰められるよう空中だけ再生速度を調整する
	const float DashStPlayRate = bIsAirChargeDash ? GetAirChargeDashStPlayRate() : 1.0f;
	PlayAnimMontage( GetChargeDashStartAnimTag(), DashStPlayRate );

	// ExecuteChargePropulsion で CachedChargeDashDirection が確定した後に呼ぶ
	if ( bIsAirChargeDash )
	{
		StartAirChargeDashSwingCamera();
	}
}

bool UChargeActionPlayerModule_V2::TryStartAirNormalDiveAttack()
{
	if ( !OwnerCharacter || OwnerCharacter->IsHitReacting() ) return false;
	if ( !IsEffectivelyInAir() ) return false;
	if ( bIsCharging ) return false;	// 溜め中は空中チャージ攻撃が優先
	if ( !AirChargeAttackDiveAttackCooldownTimer.IsFinish() ) return false;
	// 割り込めるのは None（通常ジャンプ・落下・滑空中）と Jump（チャージジャンプ／幅跳び）のみ
	const bool bCanInterrupt =
		CurrentChargeActionType == EChargeActionV2Type::None ||
		CurrentChargeActionType == EChargeActionV2Type::Jump;
	if ( !bCanInterrupt ) return false;

	ChargeAttackTriggerSource = TEXT( "振り下ろし（通常）" );	// 計測ログ用
	OnStartAirNormalDiveAttack();
	return true;
}

void UChargeActionPlayerModule_V2::OnStartAirNormalDiveAttack()
{
	if ( !OwnerCharacter ) return;

	// 滑空 Loop は CanAttack を持たない前提のため、滑空からの割り込みは RequestAttack を強制発動させる
	// （滑空自体は UGlideActionPlayerModule が「チャージアクション開始」を見て自分で畳む）
	const bool bFromGlideSession = OwnerCharacter->IsInGlideSession();

	// 縦ダイブ機械をチャージ攻撃と共用しつつ、bIsAirNormalAttack で「通常攻撃」として識別する
	bIsHitCancelableToCharge = false;
	bKeepDashCameraDuringHop = false;

	CurrentChargeActionType = EChargeActionV2Type::Attack;
	bIsAirChargeAttack = false;
	bIsAirNormalAttack = true;
	bIsAirChargeAttackInLoop = false;

	// 計測のみ：振り下ろしは SetupAirChargeAttackState を通らないためここで記録する
	RecordChargeAttackTriggerLog();

	FrictionRecoveryTimer.Clear();
	bResumeChargeDashOnLanding = false;
	ResumeChargeDashRemainingTime = 0.0f;
	ClearChargeAttackTimedApproach();

	// Jump 種別から割り込んだ場合の後始末。放置すると ST->LP 監視がジャンプ montage を見続け、
	// カメラも Pop されず残る
	bIsChargeJumpInLoop = false;
	bIsChargeHopJump = false;
	PopChargeActionCamera();
	OwnerCharacter->RequestPopFirstJumpCamera();

	// 重力スケールはこの後の ExecuteChargePropulsion→RefreshMovementParams で戻る
	bIsAirChargeDash = false;

	ResetChargeComboIndex();	// 推進力パラメータの参照インデックス用

	// ロックオン中はロックオンカメラのまま攻撃するため Push しない
	if ( !AirAttackStartCameraHandle.IsValid() && !OwnerCharacter->IsLockOnActive() )
	{
		if ( UExCameraSubsystem* CameraSubsystem = GetCameraSubsystem() )
		{
			AirAttackStartCameraHandle = CameraSubsystem->PushThirdPersonCameraByKey( TEXT( "AirAttackStart" ) );
		}
	}

	ExecuteChargePropulsion();
	OwnerCharacter->RequestAttack( EPlayerAttackType::Charged, /*bForce=*/bFromGlideSession );
}

void UChargeActionPlayerModule_V2::OnStartChargeAttack( bool bForceAirAttack )
{
	// 次の攻撃サイクルの加算を1回許可する（コンボ加算の「1攻撃1回」ガードを解除）
	bChargeComboAdvancedThisAttack = false;

	// 最終段を出したら以降の溜め直しに R2 の押し直しを要求する（部分適用モードのみ）
	const UTidePlayerParamDataAsset* ComboFinishParams = GetPlayerParams();
	if ( ComboFinishParams && CurrentChargeComboIndex >= ComboFinishParams->MaxChargeComboCount )
	{
		RequestChargeRepressByEvent();
	}

	InitializeChargeAttackAction( bForceAirAttack );
	EffectSubModule->SpawnChargeReleaseEffect();
	ConsumeGustChargeBuffIfArmed();
	ExecuteChargePropulsion();
	OwnerCharacter->RequestAttack( EPlayerAttackType::Charged );
}

void UChargeActionPlayerModule_V2::OnStartChargeJump( bool bForceGearOne )
{
	if ( !OwnerCharacter || !OwnerCharacter->PlayerParamData ) return;

	// ダッシュ由来でないジャンプでは、過去に立てた着地ダッシュ継続予約を確実に下ろす
	if ( !bForceGearOne )
	{
		bResumeChargeDashOnLanding = false;
		ResumeChargeDashRemainingTime = 0.0f;
	}

	const int32 EffectiveGearIndex = bForceGearOne ? 1 : CurrentChargeGearIndex;

	InitializeChargeJumpAction( EffectiveGearIndex );
	EffectSubModule->SpawnChargeReleaseEffect();
	ConsumeGustChargeBuffIfArmed();

	// ホップはモーション・移動ロックがギア壱固定でも、ジャンプ力だけは発動元ダッシュのギアで引く
	float FinalJumpZ = GetChargeJumpZPowerForGear( CurrentChargeGearIndex, bForceGearOne );

	// 竜巻内はギア段階の高さに倍率を上乗せすると跳びすぎるため、専用の基準初速 × 倍率に揃える
	const float WindJumpBoost = OwnerCharacter->GetWindJumpBoostMultiplier();
	if ( WindJumpBoost > 1.0f )
	{
		const float BaseJumpZ = bForceGearOne
			? OwnerCharacter->PlayerParamData->WindJumpChargeHopZVelocity
			: OwnerCharacter->PlayerParamData->WindJumpChargeJumpZVelocity;
		FinalJumpZ = BaseJumpZ * WindJumpBoost;
	}

	// 幅跳びは水平速度そのもので決まる MoveSpeed 方式なので前方初速（飛距離）を持たない
	const float BaseForwardPower = bForceGearOne ? 0.0f : GetChargeJumpMovePowerForGear( CurrentChargeGearIndex );
	FVector ForwardDirection = FVector::ZeroVector;
	float InputScale = 0.0f;
	SetupChargeJumpDirection( ForwardDirection, InputScale );
	const float FinalForwardPower = BaseForwardPower * InputScale;
	ApplyChargeJumpLaunch( ForwardDirection, FinalForwardPower, FinalJumpZ, bForceGearOne );
	// ホップはモーション同様ギア壱固定なので EffectiveGearIndex（＝1）で引く
	ApplyChargeJumpActionLock( EffectiveGearIndex );

	OwnerCharacter->RefreshMovementParams();

	bIsChargeHopJump = bForceGearOne;	// ホップは専用モーション（CHARGE_HOP_ST/LP）を使う
	const FName JumpStartTag = bIsChargeHopJump ? PlayerAnimTags::CHARGE_HOP_ST : PlayerAnimTags::CHARGE_JUMP_ST;
	OwnerCharacter->PlayAnimMontage( JumpStartTag, GetChargeJumpPlayRateForGear( EffectiveGearIndex ) );

	bIsChargeJumpInLoop = false;
}

void UChargeActionPlayerModule_V2::ResumeChargeDashAfterLanding()
{
	const float RemainingTime = ResumeChargeDashRemainingTime;
	bResumeChargeDashOnLanding = false;
	ResumeChargeDashRemainingTime = 0.0f;

	// 残り時間が無い／前提が崩れている場合は通常のジャンプ終了として処理する
	if ( !OwnerCharacter || RemainingTime <= 0.0f )
	{
		bKeepDashCameraDuringHop = false;	// 維持していたダッシュカメラを Pop させる
		OnEndAction();
		return;
	}

	StopFinishedChargeJumpMontages();

	RestartChargeDashWithRemainingTime( RemainingTime );
}

bool UChargeActionPlayerModule_V2::TryStartChargeDashFallJumpMotion()
{
	if ( !OwnerCharacter || bDashFallChargeJumpMotion ) return false;

	// 空中チャージダッシュは専用の ST＋落下ループを持っているので触らない
	if ( !IsPlayingChargeDash() || bIsAirChargeDash ) return false;
	// 空中判定は UpdateChargeDashMontageState と揃える。ここだけ IsFalling() で通すと、
	// 着地間際（真下 130cm 以内）に張った ST を向こうが即畳んで 1 フレーム点滅する
	if ( !OwnerCharacter->IsFalling() || !IsEffectivelyInAir() ) return false;
	if ( bIsReboundSliding || bIsReboundDash ) return false;

	// 他のモーションが主導権を持っている間は割り込まない
	if ( OwnerCharacter->IsAttacking() || OwnerCharacter->IsDodging() || OwnerCharacter->IsHitReacting()
		|| OwnerCharacter->IsBoostDashing() || OwnerCharacter->IsInGlideSession() ) return false;

	// 以降の ST→LP と着地の後始末は UpdateChargeDashMontageState が持つ
	PlayAnimMontage( PlayerAnimTags::CHARGE_JUMP_ST );
	OwnerCharacter->ReserveAutoDashOnLanding();
	bDashFallChargeJumpMotion = true;
	bDashFallChargeJumpStStarted = false;
	bDashFallChargeJumpInLoop = false;
	return true;
}

bool UChargeActionPlayerModule_V2::PauseChargeDashForBoost()
{
	if ( !IsPlayingChargeDash() ) return false;

	// CancelCharge でクリアされる前に残り時間を退避しておく
	const float RemainingDashTime = ChargedActionLockTimer.Get();
	if ( RemainingDashTime <= 0.0f ) return false;

	CancelCharge( false );

	bResumeChargeDashAfterBoost = true;
	ChargeDashResumeTimeAfterBoost = RemainingDashTime;
	return true;
}

bool UChargeActionPlayerModule_V2::CancelChargeJumpForBoost()
{
	if ( !IsPlayingChargeJump() ) return false;

	// 残すと UpdateChargeJumpAnimState が「ST が再生中でない（＝ブースト ST で置き換わった）」のを
	// ST 終了と誤検知して CHARGE_JUMP_LP を張り直し、ブースト専用モーションを上書きする
	bKeepDashCameraDuringHop = false;
	StopFinishedChargeJumpMontages();
	CancelCharge( false );
	return true;
}

bool UChargeActionPlayerModule_V2::CancelChargeActionForWindJump()
{
	// 判定に IsPlayingChargeAction() は使えない——ジャンプ／幅跳びは LP（落下）へ入ると false を返すが、
	// 竜巻へ突っ込むのはまさにその落下中で、状態機械もモーションもまだ生きている
	if ( !bIsCharging && !IsAnyChargeActionTypeSet() ) return false;

	// 畳まないと幅跳びの水平速度維持やダッシュの速度ロックが毎フレーム当て直し、
	// 打ち上げても「触れたときのアクションのまま」飛んでしまう
	bKeepDashCameraDuringHop = false;
	StopFinishedChargeJumpMontages();
	CancelCharge( false );

	// 空中の溜め自動再開を止める。再開すると IsCharging() が立って UJumpActionPlayerModule が
	// モーション制御を降り、竜巻ジャンプの ST が末尾ポーズのまま固まる
	bBlockChargeInputUntilRelease = true;
	return true;
}

bool UChargeActionPlayerModule_V2::ResumeChargeDashAfterBoost()
{
	if ( !bResumeChargeDashAfterBoost ) return false;

	const float RemainingTime = ChargeDashResumeTimeAfterBoost;
	bResumeChargeDashAfterBoost = false;
	ChargeDashResumeTimeAfterBoost = 0.0f;

	if ( !OwnerCharacter || RemainingTime <= 0.0f ) return false;

	RestartChargeDashWithRemainingTime( RemainingTime );

	// RestartChargeDashWithRemainingTime は velocity を触らないので、ここではまだ中断前のブースト速度が残る。
	// その向きを退避し、再開直後はハードセットせず機体前方向へ徐々に寄せる（直角スナップを防ぐ）
	if ( OwnerCharacter->PlayerParamData )
	{
		const float EaseTime = OwnerCharacter->PlayerParamData->BoostResumeChargeDashTurnEaseTime;
		if ( EaseTime > 0.0f )
		{
			if ( const UCharacterMovementComponent* Move = OwnerCharacter->GetCharacterMovement() )
			{
				BoostResumeInitialVelDir = Move->Velocity.GetSafeNormal2D();
				// 大きさも退避。即戻すと「ガクッ」となるため、窓の間に滑らかに寄せる
				BoostResumeInitialSpeed = Move->Velocity.Size2D();
			}
			// 速度ゼロなら寄せる基準が無いのでイージング不要
			if ( !BoostResumeInitialVelDir.IsNearlyZero() )
			{
				BoostResumeTurnEaseTimer.Set( EaseTime );
			}
		}
	}
	return true;
}

bool UChargeActionPlayerModule_V2::PauseChargeForBoost()
{
	// 溜め中だけを対象にする（アクション中断は PauseChargeDashForBoost / CancelChargeJumpForBoost が担当）
	if ( !bIsCharging ) return false;

	// CancelCharge 後もブースト中は UpdateComboReset がギア／コンボを1へ戻すため、値の退避が必須
	const int32 SavedGear = CurrentChargeGearIndex;
	const int32 SavedCombo = CurrentChargeComboIndex;

	// bChargeInputHeld は維持されるので、長押し継続かどうかは ResumeChargeAfterBoost で判定できる
	CancelCharge( false );

	// ブースト中は CanBeginChargeFromCurrentState がこのフラグを見て再チャージを抑止する
	bResumeChargeHoldAfterBoost = true;
	SavedChargeGearForBoostResume = SavedGear;
	SavedChargeComboForBoostResume = SavedCombo;
	return true;
}

bool UChargeActionPlayerModule_V2::ResumeChargeAfterBoost()
{
	if ( !bResumeChargeHoldAfterBoost ) return false;

	bResumeChargeHoldAfterBoost = false;
	const int32 SavedGear = SavedChargeGearForBoostResume;
	const int32 SavedCombo = SavedChargeComboForBoostResume;
	SavedChargeGearForBoostResume = 1;
	SavedChargeComboForBoostResume = 1;

	// 長押し継続時のみ復帰する
	if ( !OwnerCharacter || !bChargeInputHeld ) return false;

	// BeginCharge→UpdateChargeStartComboState は条件次第でギアを1へ落とすため、開始後に上書きする
	BeginCharge();
	CurrentChargeGearIndex = SavedGear;
	CurrentChargeComboIndex = SavedCombo;
	return true;
}

void UChargeActionPlayerModule_V2::RestartChargeDashWithRemainingTime( float RemainingTime )
{
	// 以降はこのダッシュがカメラのライフサイクルを持つため維持フラグを下ろす
	// （下の SetChargeActionCamera はキー一致で no-op になり、維持していたカメラはそのまま保たれる）
	bKeepDashCameraDuringHop = false;

	// 幅跳びからダッシュへ戻る経路は ResetChargedActionState を経由しない。
	// ダッシュがこの後の速度を握るので、その前に維持と計測を確定させる
	ResetChargeHopSpeedMaintain();
	ChargeHopSequenceIndex = 0;

	bIsChargeJumpInLoop = false;
	bIsHitCancelableToCharge = false;
	bIsReboundDash = false;
	bIsReboundSliding = false;
	CurrentReboundVelocity = FVector::ZeroVector;
	CurrentChargeActionType = EChargeActionV2Type::Dash;
	CurrentDashSpeed = CachedChargeDashSpeed;

	GravityLockTimer.Clear();
	FrictionRecoveryTimer.Clear();

	ChargedActionLockTimer.Set( RemainingTime );

	// LP 許可は消費済みの残り時間ではなく本来の持続時間で判定する。残り時間で比較すると中断タイミング次第で
	// bAllowDashLoop=false のまま再開し、LP 終了後に何も再生されず短く見える
	float StartMontageLength = 0.0f;
	if ( UAnimMontage* StartMontage = GetAnimMontage( GetChargeDashStartAnimTag() ) )
	{
		StartMontageLength = StartMontage->GetPlayLength();
	}
	bAllowDashLoop = ( CachedChargeDashLockTime > StartMontageLength );

	OwnerCharacter->RefreshMovementParams();

	// ホップ着地からの継続では維持中のカメラとキーが一致して no-op になる（カメラのカットが見えない）。
	// ブースト復帰では通常どおり Push される
	SetChargeActionCamera( MakeChargeActionCameraKey( TEXT( "ChargeDash" ) ) );

	// 中断からの復帰なので ST は再生せず Loop の BS を直接 ON にする
	if ( UPlayerAnimInstance* AnimInst = Cast<UPlayerAnimInstance>( OwnerCharacter->GetMesh()->GetAnimInstance() ) )
	{
		AnimInst->bIsChargeDashBSPlaying = true;
		AnimInst->ChargeDashGearIndex = CurrentChargeGearIndex;
	}

	EffectSubModule->SpawnChargedDashWindEffect();	// 中断中に破棄されているため張り直す
}

void UChargeActionPlayerModule_V2::InitializeChargeDashAction()
{
	bIsHitCancelableToCharge = false;	// 前のアクションからの残留を消費する

	bIsReboundDash = false;
	CurrentDashSpeed = CachedChargeDashSpeed;
	ResetChargeComboIndex();
	CurrentChargeActionType = EChargeActionV2Type::Dash;
	bIsAirChargeDash = IsEffectivelyInAir();

	if ( bIsAirChargeDash )
	{
		const int32 MaxCount = OwnerCharacter->PlayerParamData
			? FMath::Max( 1, OwnerCharacter->PlayerParamData->MaxAirChargeDashCount ) : 1;
		AirChargeDashStageCount = FMath::Min( AirChargeDashStageCount + 1, MaxCount );
	}

	bResumeChargeDashOnLanding = false;
	ResumeChargeDashRemainingTime = 0.0f;

	// 新しいダッシュがカメラを所有するため、前ダッシュの「着地までカメラ保持」予約は解除する
	bKeepChargeDashCameraUntilLanding = false;

	// アクション終了まで出しっぱなし
	SetChargeActionCamera( MakeChargeActionCameraKey( TEXT( "ChargeDash" ) ) );

	// 破棄は ResetChargedActionState が行う
	EffectSubModule->SpawnChargedDashWindEffect();
}

void UChargeActionPlayerModule_V2::InitializeChargeAttackAction( bool bForceAirAttack )
{
	bIsHitCancelableToCharge = false;	// 前のアクションからの残留を消費する

	// この攻撃が以降のカメラを所有するため、ホップ・空中ダッシュのカメラ維持予約を解除する
	bKeepDashCameraDuringHop = false;
	bKeepChargeDashCameraUntilLanding = false;

	CurrentChargeActionType = EChargeActionV2Type::Attack;
	// 残ると以降のチャージ攻撃が空中ダイブ扱いのままになり、着地のたびに AIR_ATK_ED が再生される。
	// SetupAirChargeAttackState（bIsAirChargeAttack の決定）より前に必ず落とす
	bIsAirNormalAttack = false;
	bIsAirChargeAttackInLoop = false;
	bAirChargeAttackDiveEnded = false;
	bIsAirChargeAttackLockOnRush = false;
	AirChargeAttackRushTarget = nullptr;
	AirChargeAttackDiveTimer.Clear();

	// UpdateFrictionRecoveryState はヒットバック保持のため余韻ではキャンセルしないので、新規側で明示的に切る
	FrictionRecoveryTimer.Clear();

	bResumeChargeDashOnLanding = false;
	ResumeChargeDashRemainingTime = 0.0f;

	ClearChargeAttackTimedApproach();
	// 詰めの実測速度は Clear では消さない（詰め終了の直後に読むため）ので、攻撃ごとにここで消す
	ChargeApproachMeasuredSpeed2D = 0.0f;
	SetupAirChargeAttackState( bForceAirAttack );

	// 正値でその秒数ぶん滞空してから突進へ移る。0（or 縦ダイブ）は ST モーション終了で切替える
	AirChargeAttackHoverTimer.Clear();
	if ( IsRevampAirDiveAttack() )
	{
		const float HoverTime = OwnerCharacter->PlayerParamData ? OwnerCharacter->PlayerParamData->AirChargeAttackHoverTime : 0.0f;
		if ( HoverTime > 0.0f )
		{
			AirChargeAttackHoverTimer.Set( HoverTime );
		}
	}

	// 空中は SetupAirChargeAttackState 側。ロックオン中は Push せずロックオンカメラのまま攻撃する
	// （自動フレーミングの左右回り込みが突進の当たり際にぶれるため）
	if ( !bIsAirChargeAttack && !OwnerCharacter->IsLockOnActive() )
	{
		SetChargeActionCamera( MakeChargeActionCameraKey( TEXT( "ChargeAttack" ) ) );
	}
}

void UChargeActionPlayerModule_V2::InitializeChargeJumpAction( int32 EffectiveGearIndex )
{
	bIsHitCancelableToCharge = false;	// 前のアクションからの残留を消費する

	OwnerCharacter->StopDash();
	if ( OwnerCharacter->IsAttacking() )
	{
		OwnerCharacter->CancelAttack();
	}

	CurrentChargeActionType = EChargeActionV2Type::Jump;
	FrictionRecoveryTimer.Clear();
	GravityLockTimer.Clear();
	bHasUsedChargeJump = true;

	// ホップ中は維持するので触らない
	if ( !bKeepDashCameraDuringHop )
	{
		PopChargeActionCamera();
	}

	SetupChargeJumpCameraAndAirState( EffectiveGearIndex );
}

void UChargeActionPlayerModule_V2::SetupAirChargeAttackState( bool bForceAirAttack )
{
	// 判定に IsEffectivelyInAir() ではなく IsFalling() を使う——前者は「真下130cm以内に地面あり」で
	// false を返すため、斜め下へ突進して地面に近づいた 2 回目以降が地上扱いになり即終了してしまう
	static_cast<void>( bForceAirAttack );	// 落下中なら常に air 扱いのため個別フラグは不要
	bIsAirChargeAttack = OwnerCharacter->IsFalling();

	// 空中扱いは「落下中 かつ 真下130cm以内に地面が無い」まで絞る。IsFalling() だけで通すと、地上での射出・
	// 対象への詰め・ルートモーションで数フレーム浮いただけで空中チャージ攻撃へ化ける
	// （実測：IsFalling=Yes / Vz=+290 / 真下地面=92cm で発動していた）。
	// 【重要】ここで IsEffectivelyInAir() を使ってはいけない——あれは ZVel>100 でトレース前に true を返すため、
	// 浮かせている上向き速度そのもので「空中」になり素通りする。トレース単独で見ること
	if ( bIsAirChargeAttack && MeasureGroundDistanceBelow() >= 0.0f )
	{
		bIsAirChargeAttack = false;
	}

	RecordChargeAttackTriggerLog();	// 計測のみ。モンタージュが張り替わる前に呼ぶ

	if ( !bIsAirChargeAttack )
	{
		return;
	}

	// 以降は着地まで攻撃・ジャンプ・神技のみ（空中チャージダッシュ使い切りと同じ制限）
	bAirChargeAttackUsedThisAirtime = true;

	// ロックオン中はロックオンカメラ（SimpleLockOn）のまま攻撃するため、攻撃専用カメラは Push しない
	if ( !AirAttackStartCameraHandle.IsValid() && !OwnerCharacter->IsLockOnActive() )
	{
		if ( UExCameraSubsystem* CameraSubsystem = GetCameraSubsystem() )
		{
			AirAttackStartCameraHandle = CameraSubsystem->PushThirdPersonCameraByKey( TEXT( "AirAttackStart" ) );
		}
	}
}

void UChargeActionPlayerModule_V2::SetupChargeJumpDirection( FVector& OutForwardDirection, float& OutInputScale ) const
{
	const FVector2D MovementInput = OwnerCharacter->GetRawMovementInput();
	OutInputScale = FMath::Clamp( MovementInput.Size(), 0.0f, 1.0f );

	if ( !MovementInput.IsNearlyZero() )
	{
		OutForwardDirection = OwnerCharacter->GetControlRelativeInputDirection( MovementInput ).GetSafeNormal();

		// 面沿い中は入力基準が接平面なので、そのままだと「壁を登る向き＝ほぼ真上」が前方になり、射出の Z が
		// 二重に乗って真上へ跳ね上がる。壁から離れるジャンプにしたいので水平へ倒す
		if ( OwnerCharacter->IsSurfaceRiding() )
		{
			OutForwardDirection = FlattenSurfaceRideLaunchDirection( OutForwardDirection );
		}

		OwnerCharacter->SetActorRotation( OutForwardDirection.Rotation() );
		return;
	}

	const FRotator ActorRot = OwnerCharacter->GetActorRotation();
	const FRotator YawRot( 0.0f, ActorRot.Yaw, 0.0f );
	OutForwardDirection = YawRot.Vector();
}

FVector UChargeActionPlayerModule_V2::FlattenSurfaceRideLaunchDirection( const FVector& InDirection ) const
{
	FVector Flat( InDirection.X, InDirection.Y, 0.0f );
	if ( !Flat.IsNearlyZero() ) return Flat.GetSafeNormal();

	// 垂直な壁を真っ直ぐ登っていると前方がほぼ真上になり水平成分が消える。
	// 乗っている面の法線（＝壁から離れる向き）へ逃がす
	Flat = FVector( OwnerCharacter->GetSurfaceRideUp().X, OwnerCharacter->GetSurfaceRideUp().Y, 0.0f );
	if ( !Flat.IsNearlyZero() ) return Flat.GetSafeNormal();

	// 天井（法線が真下）では法線の水平成分も消えるので機体前方へ
	Flat = FVector( OwnerCharacter->GetActorForwardVector().X, OwnerCharacter->GetActorForwardVector().Y, 0.0f );
	return Flat.IsNearlyZero() ? FVector::ForwardVector : Flat.GetSafeNormal();
}

void UChargeActionPlayerModule_V2::SetupChargeJumpCameraAndAirState( int32 EffectiveGearIndex )
{
	if ( IsEffectivelyInAir() )
	{
		OwnerCharacter->ConsumeAirJumps();
	}
	else
	{
		// チャージホップ中はチャージダッシュカメラを維持するため、ジャンプ用カメラは Push しない
		if ( bKeepDashCameraDuringHop )
		{
			return;
		}

		// ギア4以上（極）はギア3カメラに丸める
		FName CameraRowName;
		switch ( FMath::Clamp( EffectiveGearIndex, 1, 3 ) )
		{
		case 1:  CameraRowName = TEXT( "ChargeJumpGear1" ); break;
		case 2:  CameraRowName = TEXT( "ChargeJumpGear2" ); break;
		default: CameraRowName = TEXT( "ChargeJumpGear3" ); break;
		}
		OwnerCharacter->RequestPushJumpCamera( CameraRowName );
	}
}

void UChargeActionPlayerModule_V2::ApplyChargeJumpLaunch( const FVector& ForwardDirection, float FinalForwardPower, float FinalJumpZ, bool bIsHop )
{
	float PreLaunchSpeed = 0.0f;
	float InheritedSpeed = 0.0f;
	if ( UCharacterMovementComponent* MovementComp = OwnerCharacter->GetCharacterMovement() )
	{
		InheritedSpeed = MovementComp->Velocity.Size2D();
		PreLaunchSpeed = InheritedSpeed;

		// 幅跳びは速度を全量引き継いだうえで、下の MoveSpeed 方式でクランプする
		if ( !bIsHop )
		{
			InheritedSpeed *= OwnerCharacter->PlayerParamData->ChargeJumpInheritedSpeedRate;

			const float MaxInheritedSpeed = OwnerCharacter->PlayerParamData->ChargeJumpMaxInheritedSpeed;
			if ( MaxInheritedSpeed > 0.0f )	// 0以下なら無制限
			{
				InheritedSpeed = FMath::Min( InheritedSpeed, MaxInheritedSpeed );
			}
		}
	}

	float HorizontalSpeed = FinalForwardPower + InheritedSpeed;

	// 維持速度が設定されていればその速度で射出し、未設定（0以下）なら引き継ぎ速度をそのまま使う
	if ( bIsHop )
	{
		const float GearMoveSpeed = GetChargeHopMoveSpeedForGear( CurrentChargeGearIndex );
		if ( GearMoveSpeed > 0.0f )
		{
			HorizontalSpeed = GearMoveSpeed;
		}
		HorizontalSpeed = ClampChargeHopHorizontalSpeed( HorizontalSpeed );
	}

	// 幅跳びだけ坂へ沿わせる（上り坂で滞空が消える対策。通常チャージジャンプは従来どおりワールド基準）
	const FVector LaunchVelocity = bIsHop
		? BuildChargeHopLaunchVelocity( ForwardDirection, HorizontalSpeed, FinalJumpZ )
		: ( ForwardDirection * HorizontalSpeed ) + FVector( 0.0f, 0.0f, FinalJumpZ );
	OwnerCharacter->LaunchCharacter( LaunchVelocity, true, true );

	if ( bIsHop )
	{
		// 維持目標は「実際に射出した水平速度」で始める。坂へ沿わせると水平成分が Z へ回るため、
		// 傾ける前の HorizontalSpeed を目標にすると維持が毎フレーム水平へ戻し、傾けたぶんだけ速度が増える
		const FVector LaunchDir2D = LaunchVelocity.GetSafeNormal2D();
		BeginChargeHopSpeedMaintain( LaunchVelocity.Size2D(),
			LaunchDir2D.IsNearlyZero() ? ForwardDirection : LaunchDir2D, PreLaunchSpeed );
	}
}

FVector UChargeActionPlayerModule_V2::BuildChargeHopLaunchVelocity( const FVector& ForwardDirection, float HorizontalSpeed, float JumpZ ) const
{
	// 沿わせない場合のフォールバック（ワールド基準）
	const FVector FlatLaunch = ( ForwardDirection * HorizontalSpeed ) + FVector( 0.0f, 0.0f, JumpZ );

	const UTidePlayerParamDataAsset* PlayerParams = GetPlayerParams();
	if ( !OwnerCharacter || !PlayerParams || !PlayerParams->bEnableChargeHopSlopeAlign ) return FlatLaunch;

	// 接地していないと床法線が意味を持たない
	const UCharacterMovementComponent* MovementComp = OwnerCharacter->GetCharacterMovement();
	if ( !MovementComp || !MovementComp->IsMovingOnGround() ) return FlatLaunch;

	// 生法線はポリゴンの継ぎ目で飛ぶので平滑化済みを使う（坂道補正・ダッシュの投影と同じ）
	const FVector FloorNormal = OwnerCharacter->GetSmoothedFloorNormal().GetSafeNormal();
	const float FloorAngleDeg = FMath::RadiansToDegrees( FMath::Acos( FMath::Clamp( FloorNormal.Z, -1.0f, 1.0f ) ) );
	if ( FloorAngleDeg < KINDA_SMALL_NUMBER ) return FlatLaunch;		// 平地

	// 下り坂で沿わせると射出が下向きへ傾いて滞空が縮む。既定は上り（面沿い前方が上を向く）だけ対象にする
	if ( PlayerParams->bChargeHopSlopeAlignUphillOnly )
	{
		if ( FVector::VectorPlaneProject( ForwardDirection, FloorNormal ).Z <= 0.0f ) return FlatLaunch;
	}

	float TiltDeg = FloorAngleDeg;
	if ( PlayerParams->ChargeHopSlopeAlignMaxAngleDeg > 0.0f )
	{
		TiltDeg = FMath::Min( TiltDeg, PlayerParams->ChargeHopSlopeAlignMaxAngleDeg );
	}
	TiltDeg *= FMath::Clamp( PlayerParams->ChargeHopSlopeAlignRate, 0.0f, 1.0f );
	if ( TiltDeg < KINDA_SMALL_NUMBER ) return FlatLaunch;

	// ワールド真上を床法線へ倒す回転を前方にも同じだけ掛ける。回転なので射出の大きさは変わらず、
	// 向きだけが坂基準になる＝坂に対して垂直な初速が確保される
	const FVector TiltAxis = FVector::CrossProduct( FVector::UpVector, FloorNormal ).GetSafeNormal();
	if ( TiltAxis.IsNearlyZero() ) return FlatLaunch;

	const FQuat Tilt( TiltAxis, FMath::DegreesToRadians( TiltDeg ) );
	return ( Tilt.RotateVector( ForwardDirection ) * HorizontalSpeed ) + ( Tilt.RotateVector( FVector::UpVector ) * JumpZ );
}

float UChargeActionPlayerModule_V2::ClampChargeHopHorizontalSpeed( float InSpeed ) const
{
	// 維持速度と同じく発動元チャージダッシュのギア段階で引く
	const float MaxSpeed = GetChargeHopMaxMoveSpeedForGear( CurrentChargeGearIndex );
	if ( MaxSpeed <= 0.0f ) return InSpeed;	// 0以下＝上限なし

	return FMath::Min( InSpeed, MaxSpeed );
}

float UChargeActionPlayerModule_V2::GetChargeHopMoveSpeedForGear( int32 GearIndex1Based ) const
{
	const UTidePlayerParamDataAsset* PlayerParams = GetPlayerParams();
	if ( !PlayerParams ) return 0.0f;

	// 空配列は「未設定＝発射初速を維持」の意味なので、中立値ではなく 0 を返す
	return UTidePlayerParamDataAsset::GetValueForGear( PlayerParams->ChargeHopMoveSpeedForGear, GearIndex1Based, 0.0f );
}

float UChargeActionPlayerModule_V2::GetChargeHopMaxMoveSpeedForGear( int32 GearIndex1Based ) const
{
	const UTidePlayerParamDataAsset* PlayerParams = GetPlayerParams();
	if ( !PlayerParams ) return 0.0f;

	// 空配列は「未設定＝上限なし」の意味なので、中立値ではなく 0 を返す
	return UTidePlayerParamDataAsset::GetValueForGear( PlayerParams->ChargeHopMaxMoveSpeedForGear, GearIndex1Based, 0.0f );
}

void UChargeActionPlayerModule_V2::BeginChargeHopSpeedMaintain( float InLaunchSpeed, const FVector& InDirection, float InPreLaunchSpeed )
{
	bChargeHopSpeedMaintainActive = true;
	bChargeHopBlockedBySolid = false;
	ChargeHopMaintainSpeed = ClampChargeHopHorizontalSpeed( InLaunchSpeed );
	ChargeHopMaintainDir = InDirection.GetSafeNormal2D();
	// 発射は PendingLaunchVelocity 経由で次の移動更新に反映されるため、今フレームの Velocity には乗っていない。
	// 起点を「与えたはずの速度」に置き、次フレームの入口との差を CMC ロスとして拾う
	ChargeHopLastExitSpeed = InLaunchSpeed;

	FChargeHopSpeedLog NewLog;
	NewLog.Index = ++ChargeHopSequenceIndex;
	NewLog.PreLaunchSpeed = InPreLaunchSpeed;
	NewLog.LaunchSpeed = InLaunchSpeed;
	NewLog.EndSpeed = InLaunchSpeed;
	ChargeHopSpeedLogs.Insert( NewLog, 0 );
	if ( ChargeHopSpeedLogs.Num() > ChargeHopSpeedLogMax )
	{
		ChargeHopSpeedLogs.SetNum( ChargeHopSpeedLogMax );
	}
}

void UChargeActionPlayerModule_V2::UpdateChargeHopSpeedMaintain( float DeltaTime )
{
	if ( !bChargeHopSpeedMaintainActive ) return;

	// ホップが終わった時点で計測を確定して抜ける
	if ( !OwnerCharacter || !IsPlayingChargeHopJump() )
	{
		ResetChargeHopSpeedMaintain();
		return;
	}

	UCharacterMovementComponent* MovementComp = OwnerCharacter->GetCharacterMovement();
	const UTidePlayerParamDataAsset* PlayerParams = GetPlayerParams();
	if ( !MovementComp || !PlayerParams )
	{
		ResetChargeHopSpeedMaintain();
		return;
	}

	const bool bFalling = OwnerCharacter->IsFalling();
	FChargeHopSpeedLog* Log = ChargeHopSpeedLogs.IsValidIndex( 0 ) ? &ChargeHopSpeedLogs[0] : nullptr;

	// 前フレーム出口からの差。負なら CMC のブレーキ・摩擦が食っている
	const float EntrySpeed = MovementComp->Velocity.Size2D();
	if ( Log )
	{
		const float CmcDelta = EntrySpeed - ChargeHopLastExitSpeed;
		if ( bFalling )
		{
			Log->AirTime += DeltaTime;
			if ( CmcDelta < 0.0f ) Log->AirCmcLoss += -CmcDelta;
		}
		else
		{
			Log->GroundTime += DeltaTime;
			if ( CmcDelta < 0.0f ) Log->GroundCmcLoss += -CmcDelta;
		}
	}

	float ExitSpeed = EntrySpeed;

	// 滞空中は入力に依らず維持し、接地区間は移動入力があるときだけ維持する（離しているのに滑ると止まれない）
	const bool bHasMoveInput = OwnerCharacter->GetRawMovementInput().Size() >= PlayerParams->DashCancelInputThreshold;
	const bool bMaintainPhase = bFalling || ( PlayerParams->bChargeHopMaintainDuringLanding && bHasMoveInput );
	if ( bMaintainPhase && !bChargeHopBlockedBySolid && !OwnerCharacter->IsPushingPawn() )
	{
		if ( PlayerParams->ChargeHopSpeedDecayPerSec > 0.0f )	// 0 で完全維持
		{
			ChargeHopMaintainSpeed = FMath::Max( 0.0f, ChargeHopMaintainSpeed - PlayerParams->ChargeHopSpeedDecayPerSec * DeltaTime );
		}

		// 壁に当たって速度が潰されている間も当て直すと壁へ押し付けたまま滞空してしまう。
		// 目標に対して速度が極端に落ちたフレームは「ぶつかった」とみなし、以降このホップでは維持しない
		if ( ChargeHopMaintainSpeed > 0.0f && EntrySpeed < ChargeHopMaintainSpeed * 0.25f )
		{
			bChargeHopBlockedBySolid = true;
		}
		else
		{
			// 現速度が目標を上回っているとき（坂の加速等）は削らず、上限だけ効かせる
			const float DesiredSpeed = ClampChargeHopHorizontalSpeed( FMath::Max( EntrySpeed, ChargeHopMaintainSpeed ) );

			// 向きはエアコントロールでの操舵結果を優先し、消えていれば発射方向で代用する
			FVector Dir = FVector( MovementComp->Velocity.X, MovementComp->Velocity.Y, 0.0f ).GetSafeNormal();
			if ( Dir.IsNearlyZero() )
			{
				Dir = ChargeHopMaintainDir;
			}
			else
			{
				ChargeHopMaintainDir = Dir;
			}

			if ( !Dir.IsNearlyZero() )
			{
				MovementComp->Velocity.X = Dir.X * DesiredSpeed;
				MovementComp->Velocity.Y = Dir.Y * DesiredSpeed;
				ExitSpeed = DesiredSpeed;
			}

			// 歩行上限を下回る目標を維持していると、CMC が超過分をブレーキで削りにくる（接地中は特に強い）。
			// 綱引きさせないよう、維持中だけ上限を引き上げる（RefreshMovementParams で元へ戻る）
			if ( MovementComp->MaxWalkSpeed < DesiredSpeed )
			{
				MovementComp->MaxWalkSpeed = DesiredSpeed;
			}
		}
	}

	// 出口＝自前で戻した量
	if ( Log )
	{
		const float SelfDelta = ExitSpeed - EntrySpeed;
		if ( SelfDelta > 0.0f )
		{
			if ( bFalling ) Log->AirSelfGain += SelfDelta;
			else            Log->GroundSelfGain += SelfDelta;
		}
		Log->EndSpeed = ExitSpeed;
	}

	ChargeHopLastExitSpeed = ExitSpeed;
}

void UChargeActionPlayerModule_V2::ResetChargeHopSpeedMaintain()
{
	if ( bChargeHopSpeedMaintainActive && OwnerCharacter )
	{
		// 終了時点の速度をログへ確定させる（次ホップの引き継ぎ元がこの値になる）
		if ( ChargeHopSpeedLogs.IsValidIndex( 0 ) )
		{
			if ( const UCharacterMovementComponent* MovementComp = OwnerCharacter->GetCharacterMovement() )
			{
				ChargeHopSpeedLogs[0].EndSpeed = MovementComp->Velocity.Size2D();
			}
		}
	}

	bChargeHopSpeedMaintainActive = false;
	bChargeHopBlockedBySolid = false;
	ChargeHopMaintainSpeed = 0.0f;
	ChargeHopMaintainDir = FVector::ZeroVector;
	ChargeHopLastExitSpeed = 0.0f;
}

void UChargeActionPlayerModule_V2::ApplyChargeJumpActionLock( int32 GearIndex1Based )
{
	ChargedActionLockTimer.Set( GetChargeJumpLockTimeForGear( GearIndex1Based ) );
}

float UChargeActionPlayerModule_V2::GetChargeJumpLockTimeForGear( int32 GearIndex1Based ) const
{
	const UTidePlayerParamDataAsset* PlayerParams = GetPlayerParams();
	if ( !PlayerParams ) return 0.0f;

	const int32 GearIdx = FMath::Clamp( GearIndex1Based - 1, 0, PlayerParams->MaxChargeGearCount - 1 );
	if ( PlayerParams->ChargeJumpMovementLockTimeForGear.IsValidIndex( GearIdx ) )
	{
		return PlayerParams->ChargeJumpMovementLockTimeForGear[GearIdx];
	}

	// 配列未設定時は旧V1の Min〜Max をギアで線形補間する
	const int32 MaxGear = FMath::Max( 1, PlayerParams->MaxChargeGearCount - 1 );
	const float GearRate = FMath::Clamp( static_cast< float >( GearIndex1Based - 1 ) / static_cast< float >( MaxGear ), 0.0f, 1.0f );
	return FMath::Lerp( PlayerParams->MinChargeJumpMovementLockTime, PlayerParams->MaxChargeJumpMovementLockTime, GearRate );
}

float UChargeActionPlayerModule_V2::GetChargeJumpPlayRateForGear( int32 GearIndex1Based ) const
{
	if ( !OwnerCharacter || !OwnerCharacter->PlayerParamData )
	{
		return 1.0f;
	}

	const int32 GearIdx = FMath::Clamp( GearIndex1Based - 1, 0, OwnerCharacter->PlayerParamData->MaxChargeGearCount - 1 );
	if ( OwnerCharacter->PlayerParamData->ChargeJumpAnimPlayRatesForGear.IsValidIndex( GearIdx ) )
	{
		return OwnerCharacter->PlayerParamData->ChargeJumpAnimPlayRatesForGear[GearIdx];
	}

	return 1.0f;
}

float UChargeActionPlayerModule_V2::GetChargeJumpZPowerForGear( int32 GearIndex1Based, bool bIsHop ) const
{
	const UTidePlayerParamDataAsset* PlayerParams = GetPlayerParams();
	if ( !PlayerParams ) return 0.0f;

	const int32 GearIdx = FMath::Clamp( GearIndex1Based - 1, 0, PlayerParams->MaxChargeGearCount - 1 );
	const TArray<float>& PowerForGear = bIsHop ? PlayerParams->ChargeHopPowerForGear : PlayerParams->ChargeJumpPowerForGear;
	if ( PowerForGear.IsValidIndex( GearIdx ) )
	{
		return PowerForGear[GearIdx];
	}

	// 配列未設定時：通常はギアで線形補間、ホップはギア壱固定
	if ( bIsHop )
	{
		return PlayerParams->MinChargeJumpPower;
	}
	const int32 MaxGear = FMath::Max( 1, PlayerParams->MaxChargeGearCount - 1 );
	const float GearRate = FMath::Clamp( static_cast< float >( GearIndex1Based - 1 ) / static_cast< float >( MaxGear ), 0.0f, 1.0f );
	return FMath::Lerp( PlayerParams->MinChargeJumpPower, PlayerParams->MaxChargeJumpPower, GearRate );
}

float UChargeActionPlayerModule_V2::GetChargeJumpMovePowerForGear( int32 GearIndex1Based ) const
{
	const UTidePlayerParamDataAsset* PlayerParams = GetPlayerParams();
	if ( !PlayerParams ) return 0.0f;

	const int32 GearIdx = FMath::Clamp( GearIndex1Based - 1, 0, PlayerParams->MaxChargeGearCount - 1 );
	if ( PlayerParams->ChargeJumpMovePowerForGear.IsValidIndex( GearIdx ) )
	{
		return PlayerParams->ChargeJumpMovePowerForGear[GearIdx];
	}

	// 配列未設定時は Min〜Max をギアで線形補間する
	const int32 MaxGear = FMath::Max( 1, PlayerParams->MaxChargeGearCount - 1 );
	const float GearRate = FMath::Clamp( static_cast< float >( GearIndex1Based - 1 ) / static_cast< float >( MaxGear ), 0.0f, 1.0f );
	return FMath::Lerp( PlayerParams->MinChargeJumpMovePower, PlayerParams->MaxChargeJumpMovePower, GearRate );
}

void UChargeActionPlayerModule_V2::BuildDashPropulsionSettings( FChargePropulsionSettings& OutSettings, int32 ComboIdx, int32 GearIdx ) const
{
	const UTidePlayerParamDataAsset* PlayerParams = GetPlayerParams();
	if ( !PlayerParams ) return;

	const float BaseDashSpeed = PlayerParams->ChargeV2DashSpeedsForGear.IsValidIndex( GearIdx ) ? PlayerParams->ChargeV2DashSpeedsForGear[GearIdx] : 0.0f;
	const float BaseDashPower = PlayerParams->ChargeV2DashPowersForGear.IsValidIndex( GearIdx ) ? PlayerParams->ChargeV2DashPowersForGear[GearIdx] : 0.0f;
	const float DashSpeedMult = PlayerParams->ChargeV2DashSpeedsRateForCombo.IsValidIndex( ComboIdx ) ? PlayerParams->ChargeV2DashSpeedsRateForCombo[ComboIdx] : 1.0f;
	const float DashPowerMult = PlayerParams->ChargeV2DashPowersRateForCombo.IsValidIndex( ComboIdx ) ? PlayerParams->ChargeV2DashPowersRateForCombo[ComboIdx] : 1.0f;

	OutSettings.Speed = BaseDashSpeed * DashSpeedMult;
	OutSettings.Power = BaseDashPower * DashPowerMult;

	if ( PlayerParams->ChargeV2DashLockTimes.IsValidIndex( GearIdx ) )
	{
		OutSettings.LockTime = PlayerParams->ChargeV2DashLockTimes[GearIdx];
	}
	OutSettings.GhostTrailDuration = OutSettings.LockTime;

	// 2回目以降は AirChargeDash2*ForGear を優先し、空／範囲外なら1回目のセットへフォールバックする
	const bool bUse2ndAirDashSet = ( AirChargeDashStageCount >= 2 );
	auto PickAirDashGearValue = [&]( const TArray<float>& Second, const TArray<float>& First, float Fallback ) -> float
	{
		if ( bUse2ndAirDashSet && Second.IsValidIndex( GearIdx ) ) return Second[GearIdx];
		return First.IsValidIndex( GearIdx ) ? First[GearIdx] : Fallback;
	};

	if ( IsEffectivelyInAir() )
	{
		OutSettings.GravityLockTime = PickAirDashGearValue( PlayerParams->AirChargeDash2GravityLockTimesForGear, PlayerParams->AirChargeDashGravityLockTimesForGear, 0.0f );
		OutSettings.ZUpSpeed = PickAirDashGearValue( PlayerParams->AirChargeDash2ZUpSpeedsForGear, PlayerParams->AirChargeDashZUpSpeedsForGear, 0.0f );

		// 0 以下なら地上の ChargeV2DashSpeedsForGear を使う
		const float AirDashSpeed = PickAirDashGearValue( PlayerParams->AirChargeDash2SpeedsForGear, PlayerParams->AirChargeDashSpeedsForGear, 0.0f );
		if ( bIsAirChargeDash && AirDashSpeed > 0.0f )
		{
			OutSettings.Speed = AirDashSpeed * DashSpeedMult;
		}
	}
	else
	{
		OutSettings.GravityLockTime = 0.0f;
		OutSettings.ZUpSpeed = 0.0f;
	}

	// 寿命は地上と分ける——地上共用の ChargeV2DashLockTimes は尺が長く、ST 終了後もアクションが続いて
	// 落下・次ダッシュへ移れない。0 以下なら ST モーション長へ自動追従する
	if ( bIsAirChargeDash )
	{
		float AirLockTime = PickAirDashGearValue( PlayerParams->AirChargeDash2LockTimesForGear, PlayerParams->AirChargeDashLockTimesForGear, 0.0f );
		if ( AirLockTime <= 0.0f )
		{
			AirLockTime = GetAirChargeDashStMontageDuration();
		}
		if ( AirLockTime > 0.0f )
		{
			OutSettings.LockTime = AirLockTime;
			OutSettings.GhostTrailDuration = AirLockTime;
		}
	}
}

void UChargeActionPlayerModule_V2::BuildAttackPropulsionSettings( FChargePropulsionSettings& OutSettings, int32 ComboIdx, int32 GearIdx ) const
{
	const UTidePlayerParamDataAsset* PlayerParams = GetPlayerParams();
	if ( !PlayerParams ) return;

	// コンボ段階の基礎パワー × ギア段階の倍率
	const float BaseAtkPower = PlayerParams->ChargeV2AttackDashPowersForCombo.IsValidIndex( ComboIdx ) ? PlayerParams->ChargeV2AttackDashPowersForCombo[ComboIdx] : 0.0f;
	const float AtkPowerMult = PlayerParams->ChargeV2AttackDashPowersRateForGear.IsValidIndex( GearIdx ) ? PlayerParams->ChargeV2AttackDashPowersRateForGear[GearIdx] : 1.0f;

	OutSettings.Power = BaseAtkPower * AtkPowerMult;
	OutSettings.LockTime = PlayerParams->MinChargeAttackMovementLockTime;

	if ( bIsAirChargeAttack )
	{
		// 横ダイブは滞空・上昇・前進速度・ロック時間のフィールを空中ダッシュと揃えるため、
		// ダッシュ側のギア別パラメータを流用する（Power は上で算出した攻撃側の突進力を使う）
		OutSettings.GravityLockTime = PlayerParams->AirChargeDashGravityLockTimesForGear.IsValidIndex( GearIdx )
			? PlayerParams->AirChargeDashGravityLockTimesForGear[GearIdx]
			: PlayerParams->AirChargeAttackGravityLockTime;
		OutSettings.ZUpSpeed = PlayerParams->AirChargeDashZUpSpeedsForGear.IsValidIndex( GearIdx )
			? PlayerParams->AirChargeDashZUpSpeedsForGear[GearIdx]
			: 0.0f;
		OutSettings.Speed = PlayerParams->ChargeV2DashSpeedsForGear.IsValidIndex( GearIdx )
			? PlayerParams->ChargeV2DashSpeedsForGear[GearIdx]
			: 0.0f;
		if ( PlayerParams->ChargeV2DashLockTimes.IsValidIndex( GearIdx ) )
		{
			OutSettings.LockTime = PlayerParams->ChargeV2DashLockTimes[GearIdx];
		}
		OutSettings.GhostTrailDuration = OutSettings.LockTime;
	}
	else if ( bIsAirNormalAttack )
	{
		// 縦ダイブは滞空（重力ロック）のみで前進しない（Speed=0）
		OutSettings.GravityLockTime = PlayerParams->AirChargeAttackGravityLockTime;
	}
	else if ( PlayerParams->ChargeAttackGravityLockTimes.IsValidIndex( ComboIdx ) )
	{
		OutSettings.GravityLockTime = PlayerParams->ChargeAttackGravityLockTimes[ComboIdx];
	}
	else
	{
		OutSettings.GravityLockTime = 0.0f;
	}
}

void UChargeActionPlayerModule_V2::OnAttackHit( AActor* TargetActor, bool bIsRebounded )
{
	if ( !TargetActor || !OwnerCharacter ) return;

	// ロックオン突進は着地・持続時間より前にヒットしたらそこで終了する。
	// 突進を止めて通常落下へ返し、着地（ED）まで繋ぐ（後退バウンドはさせない）
	if ( bIsAirChargeAttackLockOnRush && bIsAirChargeAttackInLoop && !bAirChargeAttackDiveEnded )
	{
		EndAirChargeAttackDive( 1.0f, TEXT( "ヒット" ) );
		return;
	}

	// 空中ダイブ攻撃は後退バウンドさせない
	if ( bIsAirChargeAttack || bIsAirNormalAttack ) return;

	// チャージダッシュ中のヒットはノックバック・コンボ進行の対象外
	if ( CurrentChargeActionType == EChargeActionV2Type::Dash ) return;

	// OnEndAction 後、当たり判定の遅延フレームで OnAttackHit が来ると bIsHitCancelableToCharge が再セットされ、
	// 次のチャージ開始で予期しない移動とコンボ進行が起きる。ただしモンタージュ再生中なら
	// 「正規のヒットがロック窓より後に来た」ケースなので弾かない
	if ( ChargedActionLockTimer.IsFinish() && !IsPlayingChargeAttackMontage() ) return;

	const UTidePlayerParamDataAsset* PlayerParams = GetPlayerParams();
	if ( !PlayerParams ) return;

	// ターゲットから自分への後退方向（水平2D）
	FVector PushbackDir = ( OwnerCharacter->GetActorLocation() - TargetActor->GetActorLocation() ).GetSafeNormal2D();

	const int32 GearIdx = FMath::Clamp( CurrentChargeGearIndex - 1, 0, PlayerParams->MaxChargeGearCount - 1 );
	const int32 ComboIdx = FMath::Clamp( CurrentChargeComboIndex - 1, 0, PlayerParams->MaxChargeComboCount - 1 );

	// コンボ段階の基礎パワー × ギア段階の倍率
	float BasePushbackPower = 2500.0f;
	if ( PlayerParams->ChargeAttackSelfPushbackPowersForCombo.IsValidIndex( ComboIdx ) )
	{
		BasePushbackPower = PlayerParams->ChargeAttackSelfPushbackPowersForCombo[ComboIdx];
	}

	float PowerMultiplier = 1.0f;
	if ( PlayerParams->ChargeAttackSelfPushbackPowersRateForGear.IsValidIndex( GearIdx ) )
	{
		PowerMultiplier = PlayerParams->ChargeAttackSelfPushbackPowersRateForGear[GearIdx];
	}

	const float FinalPushbackPower = BasePushbackPower * PowerMultiplier;
	OwnerCharacter->LaunchCharacter( PushbackDir * FinalPushbackPower, true, false );

	// 傾斜地でもキャンセル時に慣性を引き継げるよう、付与した速度をキャッシュしておく
	CachedHitCancelVelocity = PushbackDir * FinalPushbackPower;
	bIsHitCancelableToCharge = true;

	// 満了しても Clear 前のフレームにヒットが来ると、翌フレームの更新が Velocity.XY=0 で上書きして
	// CMC が乗せた押し返しを殺す＝まれにヒットバックしない
	ClearChargeAttackTimedApproach();

	// 長押し中は再チャージ時間が自然なインターバルになるので、攻撃連打のときだけ設ける
	if ( !bChargeInputHeld )
	{
		const float HitbackComboInterval = PlayerParams->ChargeAttackHitbackComboIntervalsForGear.IsValidIndex( GearIdx )
			? PlayerParams->ChargeAttackHitbackComboIntervalsForGear[GearIdx]
			: 0.0f;
		if ( HitbackComboInterval > 0.0f )
		{
			HitbackComboIntervalTimer.Set( HitbackComboInterval );
		}
	}
}

void UChargeActionPlayerModule_V2::CancelHitBack()
{
	// OnAttackHit の LaunchCharacter は PendingLaunchVelocity にセットされ CMC が次 tick で適用する。
	// Velocity を消すだけでは残って吹き飛ぶため、ゼロ Launch（XY上書き・Z維持）で上書きクリアする
	if ( OwnerCharacter )
	{
		OwnerCharacter->LaunchCharacter( FVector::ZeroVector, true, false );
	}
	if ( UCharacterMovementComponent* Move = GetCharacterMovement() )
	{
		Move->Velocity.X = 0.0f;
		Move->Velocity.Y = 0.0f;
	}
	CachedHitCancelVelocity = FVector::ZeroVector;
	bIsHitCancelableToCharge = false;
}

void UChargeActionPlayerModule_V2::ApplyBreakthroughMove( const FVector& Direction, float Speed )
{
	// 同フレームの OnAttackHit で後退ローンチを与えたときだけ引き受ける
	if ( !bIsHitCancelableToCharge ) return;

	if ( !OwnerCharacter || Speed <= 0.0f || Direction.IsNearlyZero() )
	{
		CancelHitBack();	// 前進量が無いなら後退だけ消す（その場停止）
		return;
	}

	// 後退ローンチは PendingLaunchVelocity に載っているだけなので、同フレームに前進ローンチで上書きすれば
	// 後退は出ない（XY上書き・Z維持）。Velocity は消さない＝慣性をそのまま前へ流す
	const FVector ThroughVelocity = Direction.GetSafeNormal2D() * Speed;
	OwnerCharacter->LaunchCharacter( ThroughVelocity, true, false );

	// フラグ側の後始末は CancelHitBack と揃える（差分は移動だけ）
	CachedHitCancelVelocity = ThroughVelocity;
	bIsHitCancelableToCharge = false;
}

bool UChargeActionPlayerModule_V2::CancelAirDiveActionForLaunch()
{
	// 空中ダイブは UpdateGravityLock が毎フレーム Velocity.Z を固定するため、打ち上げを打ち消して
	// 同じパッド上へ落とし直し無限に跳ねる。溜め・チャージジャンプ・地上ダッシュは競合しないので対象外
	const bool bIsAirDive =
		IsAnyAirDiveAttackActive() ||
		( CurrentChargeActionType == EChargeActionV2Type::Dash && bIsAirChargeDash );
	if ( !bIsAirDive ) return false;

	// GravityLockTimer の Clear と GravityScale の復帰で打ち上げ速度がそのまま乗る
	CancelCharge( false );
	return true;
}

void UChargeActionPlayerModule_V2::ExecuteChargePropulsion()
{
	if ( !OwnerCharacter || !OwnerCharacter->PlayerParamData ) return;

	// このアクションがカメラを所有する。ホップ着地からの継続は RestartChargeDashWithRemainingTime 経由で
	// この関数を通らないため影響しない
	bKeepDashCameraDuringHop = false;

	const FChargePropulsionSettings Settings = BuildChargePropulsionSettings();
	float FinalDashPower = Settings.Power;
	bool bShouldLaunch = true;
	if ( bIsReboundDash )
	{
		// リバウンド後は射出せず、0 スタートから毎フレームの更新だけで加速させる
		bShouldLaunch = false;
	}

	// 横ダイブは ST が滞空（その場待機）なので開始時の launch をしない。
	// 滞空・突進の速度制御と GravityLock は UpdateAirChargeAttackDive が受け持つ
	const bool bIsRevampAirChargeAttack = IsRevampAirDiveAttack();
	if ( bIsRevampAirChargeAttack )
	{
		bShouldLaunch = false;
	}

	UpdateDashLoopAllowance( Settings );

	if ( class AController* Controller = OwnerCharacter->GetController() )
	{
		// 吸着対象が居れば、当たるまで回頭を制御する（レバーで吸着向きが即上書きされるのを防ぐ）
		ULockOnTargetComponent* DashHomingTarget = nullptr;
		FVector DashDirection = BuildChargeDashDirection( DashHomingTarget );

		bChargeDashHomingRotationLock = ( CurrentChargeActionType == EChargeActionV2Type::Dash && DashHomingTarget != nullptr );
		ChargeDashHomingTarget = DashHomingTarget;

		OwnerCharacter->SetActorRotation( DashDirection.Rotation() );

		// 成立時は開始時の弾道射出を行わず、UpdateChargeAttackTimedApproach が毎フレーム移動を駆動する
		const bool bUseTimedApproach = TrySetupChargeAttackTimedApproach( DashDirection );
		if ( bUseTimedApproach )
		{
			bShouldLaunch = false;
		}

		DashDirection = ProjectDirectionToGround( DashDirection );	// 坂道補正

		CachedChargeDashDirection = DashDirection;
		CachedChargeDashSpeed = Settings.Speed;
		CachedChargeDashPower = Settings.Power;

		// 走り出しは初速＝Power から始め、UpdateChargeDashLockedMovement で Speed へ減衰させる
		// （ChargeDashSpeedBurstDuration が 0 なら次フレームで即 Speed に落ちる＝バーストなし）
		CurrentDashSpeed = CachedChargeDashPower;

		if ( Settings.GravityLockTime > 0.0f && !bIsRevampAirChargeAttack )
		{
			GravityLockTimer.Set( Settings.GravityLockTime );
			GravityLockZUpSpeed = Settings.ZUpSpeed;	// 0 なら水平維持、正値ならその速度で上昇
			if ( UCharacterMovementComponent* MovementComp = OwnerCharacter->GetCharacterMovement() )
			{
				MovementComp->Velocity.Z = GravityLockZUpSpeed;
			}
		}

		// 地上からの射出は LaunchCharacter で必ず MOVE_Falling に入る。この「自前の浮き」を
		// 空中扱いにしないよう記録しておく（判定は SetupAirChargeAttackState・着地でクリア）
		if ( bShouldLaunch && !OwnerCharacter->IsFalling() )
		{
			bAirborneByGroundChargeLaunch = true;
		}

		ApplyChargeLaunchVelocity( DashDirection, FinalDashPower, bShouldLaunch );

		// 詰め駆動中は、詰め完了（＝判定発生）までアクションが終わらないようロック時間を確保する
		const float LockTime = bUseTimedApproach
			? FMath::Max( Settings.LockTime, ChargeApproachDuration )
			: Settings.LockTime;
		ChargedActionLockTimer.Set( LockTime );
		CachedChargeDashLockTime = LockTime;

		OwnerCharacter->RefreshMovementParams();
	}

	CurrentChargeTimer.Clear();
}

bool UChargeActionPlayerModule_V2::TrySetupChargeAttackTimedApproach( const FVector& DashDirection )
{
	ClearChargeAttackTimedApproach();

	const UTidePlayerParamDataAsset* PlayerParams = GetPlayerParams();
	if ( !OwnerCharacter || !PlayerParams ) return false;
	if ( !PlayerParams->bEnableChargeAttackTimedApproach ) return false;

	// 地上のチャージ攻撃のみ対象（空中ダイブは急降下ロジックを優先）
	if ( CurrentChargeActionType != EChargeActionV2Type::Attack || bIsAirChargeAttack || bIsAirNormalAttack ) return false;
	if ( IsEffectivelyInAir() ) return false;

	// 吸着範囲に対象が居なければ従来の弾道射出にフォールバック
	ULockOnTargetComponent* TargetComp = FindHomingTargetComponent( DashDirection );
	if ( !TargetComp ) return false;
	AActor* TargetActor = TargetComp->GetOwner();
	if ( !TargetActor ) return false;

	// モンタージュの最初の CommonAttack 判定枠の開始秒 → 再生レートで実時間に換算
	UAnimMontage* AtkMontage = GetAnimMontage( GetChargeAttackAnimTag() );
	const float TriggerTime = GetFirstHitWindowTriggerTime( AtkMontage );
	if ( TriggerTime <= 0.0f ) return false;	// 判定枠が無ければフォールバック

	const int32 GearIdx = FMath::Clamp( CurrentChargeGearIndex - 1, 0, PlayerParams->MaxChargeGearCount - 1 );
	float PlayRate = 1.0f;
	if ( PlayerParams->ChargeAttackAnimPlayRatesForGear.IsValidIndex( GearIdx ) )
	{
		PlayRate = PlayerParams->ChargeAttackAnimPlayRatesForGear[GearIdx];
	}
	if ( PlayRate <= 0.0f ) PlayRate = 1.0f;

	const float ApproachDuration = TriggerTime / PlayRate;
	if ( ApproachDuration <= 0.0f ) return false;

	const FVector StartLoc = OwnerCharacter->GetActorLocation();

	// 方向・距離はロック部位基準で測る。ダッシュ／ホーミングと基準点を揃え、
	// 巨大敵でアクター中心へ吸い寄せられて張り付くのを防ぐ
	const FVector PartLoc = TargetComp->GetTargetLocation();
	FVector ToPart2D = PartLoc - StartLoc;
	ToPart2D.Z = 0.0f;
	const float PartDistH = ToPart2D.Size();
	const FVector DirToPartH = ToPart2D.GetSafeNormal();
	const float ConfiguredStandoff = FMath::Max( 0.0f, PlayerParams->ChargeAttackApproachStandoff );

	if ( DirToPartH.IsNearlyZero() ) return false;	// 真上/重なりは詰め不能 → 弾道射出に委ねる

	UWorld* World = OwnerCharacter->GetWorld();
	if ( !World ) return false;

	// 縦長カプセルを水平スイープして実際に当たる面の距離を測る。斜めのスイープだと遠距離では緩い傾きのまま
	// 部位の高さを進み、被弾面の上を素通りする。高さ MidZ 固定にしてカプセルの縦半径で
	// 「プレイヤーZ〜部位Z」の帯を覆えば、高さがズレていても被弾面を捉えられる
	const UCapsuleComponent* MyCapsule = OwnerCharacter->GetCapsuleComponent();
	const float ProbeRadius = MyCapsule ? MyCapsule->GetScaledCapsuleRadius() : 34.0f;
	const float MyHalfHeight = MyCapsule ? MyCapsule->GetScaledCapsuleHalfHeight() : 88.0f;

	const float MidZ = ( StartLoc.Z + PartLoc.Z ) * 0.5f;
	const float ProbeHalfHeight = FMath::Abs( PartLoc.Z - StartLoc.Z ) * 0.5f + MyHalfHeight;

	// 部位 socket が体の手前側にある場合、固定値だと奥の被弾面へ届かない。
	// 対象の水平サイズぶん（最低 200）延長して確実に貫く
	FVector BoundsOrigin, BoundsExtent;
	TargetActor->GetActorBounds( true, BoundsOrigin, BoundsExtent );
	const float ProbeOvershoot = FMath::Max( 200.0f, BoundsExtent.Size2D() );
	const float ProbeLength = PartDistH + ProbeOvershoot;

	const FVector ProbeStart( StartLoc.X, StartLoc.Y, MidZ );
	const FVector ProbeEnd( StartLoc.X + DirToPartH.X * ProbeLength,
	                        StartLoc.Y + DirToPartH.Y * ProbeLength,
	                        MidZ );

	TArray<FHitResult> Hits;
	const FCollisionObjectQueryParams ObjQuery = FCollisionObjectQueryParams::AllDynamicObjects;
	FCollisionQueryParams Params( SCENE_QUERY_STAT( ChargeApproachProbe ), false, OwnerCharacter );
	World->SweepMultiByObjectType( Hits, ProbeStart, ProbeEnd, FQuat::Identity, ObjQuery,
		FCollisionShape::MakeCapsule( ProbeRadius, ProbeHalfHeight ), Params );

	// 対象の DamageLayer コリジョンのうち最も手前のヒットまでの距離（＝水平距離）を採用する
	float BestSweepDist = -1.0f;
	for ( const FHitResult& Hit : Hits )
	{
		if ( Hit.GetActor() != TargetActor ) continue;
		const UPrimitiveComponent* Prim = Hit.GetComponent();
		if ( !Prim || !Prim->ComponentHasTag( TEXT( "DamageLayer" ) ) ) continue;

		// bStartPenetrating（既に重なっている）は Distance=0 なので最手前になる
		if ( BestSweepDist < 0.0f || Hit.Distance < BestSweepDist )
		{
			BestSweepDist = Hit.Distance;
		}
	}

#if !UE_BUILD_SHIPPING
	if ( UTideGameSettings::Get()->bDebugFlagDrawHomingArea )
	{
		DrawDebugLine( World, ProbeStart, ProbeEnd,
			BestSweepDist >= 0.0f ? FColor::Green : FColor::Red, false, 1.5f, 0, 2.0f );
	}
#endif

	// 捉えられなければ部位中心へめり込ませず従来の弾道射出にフォールバックする
	if ( BestSweepDist < 0.0f ) return false;

	// スイープは水平なので Hit.Distance ＝ 接触位置までの水平距離
	const float ContactDistH = BestSweepDist;
	const FVector ContactCenter( StartLoc.X + DirToPartH.X * ContactDistH,
	                             StartLoc.Y + DirToPartH.Y * ContactDistH,
	                             MidZ );

#if !UE_BUILD_SHIPPING
	if ( UTideGameSettings::Get()->bDebugFlagDrawHomingArea )
	{
		DrawDebugCapsule( World, ContactCenter, ProbeHalfHeight, ProbeRadius,
			FQuat::Identity, FColor::Yellow, false, 1.5f, 0, 1.5f );
	}
#endif

	// 実効スタンドオフ＝設定値＋被弾面オフセット（部位中心−接触点）。
	// 「詰める」だけで「離れない」ように、到達距離が開始距離を超えないようクランプする
	const float SurfaceOffset = FMath::Max( 0.0f, PartDistH - ContactDistH );
	const float EffectiveStandoff = ConfiguredStandoff + SurfaceOffset;

	bChargeAttackTimedApproachActive = true;
	ChargeApproachTargetActor = TargetActor;
	ChargeApproachTargetComp = TargetComp;
	ChargeApproachStartLocation = StartLoc;
	ChargeApproachStandoff = FMath::Clamp( EffectiveStandoff, 0.0f, PartDistH );
	ChargeApproachDuration = ApproachDuration;
	ChargeApproachTimer.Set( ApproachDuration );

	return true;
}

void UChargeActionPlayerModule_V2::UpdateChargeAttackTimedApproach( float DeltaTime )
{
	if ( !bChargeAttackTimedApproachActive ) return;

	const UTidePlayerParamDataAsset* PlayerParams = GetPlayerParams();
	UCharacterMovementComponent* MovementComp = GetCharacterMovement();
	ULockOnTargetComponent* TargetComp = ChargeApproachTargetComp.Get();

	// 対象消失・空中化など前提が崩れたら詰めを止め、以降は通常のロックに任せる
	if ( !OwnerCharacter || !PlayerParams || !MovementComp || !TargetComp || IsEffectivelyInAir() )
	{
		ClearChargeAttackTimedApproach();
		return;
	}

	ChargeApproachTimer.Update( DeltaTime );

	const float Alpha = FMath::Clamp( ChargeApproachTimer.GetRate(), 0.0f, 1.0f );
	const float EaseExp = FMath::Max( 1.0f, PlayerParams->ChargeAttackApproachEaseExp );
	const float Eased = FMath::Pow( Alpha, EaseExp );	// イーズイン：終盤に一気に詰める

	// 到達点＝ロック部位の手前 standoff（水平）。方向は開始位置→部位で安定させる
	// （横移動する対象でも詰め線がぶれない）
	const FVector MyLoc = OwnerCharacter->GetActorLocation();
	const FVector TargetLoc = TargetComp->GetTargetLocation();
	FVector ToTarget = TargetLoc - ChargeApproachStartLocation;
	ToTarget.Z = 0.0f;
	const FVector DirToTarget = ToTarget.GetSafeNormal();
	const FVector ArrivalPoint = TargetLoc - DirToTarget * ChargeApproachStandoff;

	FVector DesiredLoc = FMath::Lerp( ChargeApproachStartLocation, ArrivalPoint, Eased );
	DesiredLoc.Z = MyLoc.Z;	// 既定は高さ据え置き

	// 到達 XY 直下の歩行可能床へ高さを合わせる。据え置きのまま水平に詰めると、上り坂の敵へは
	// 迫り上がる床面にスイープがブロックされて手前で止まり、攻撃が届かない
	const float FloorProbe = FMath::Max( 0.0f, PlayerParams->ChargeAttackApproachFloorProbe );
	if ( FloorProbe > 0.0f )
	{
		if ( UWorld* World = OwnerCharacter->GetWorld() )
		{
			const UCapsuleComponent* Capsule = OwnerCharacter->GetCapsuleComponent();
			const float HalfHeight = Capsule ? Capsule->GetScaledCapsuleHalfHeight() : 88.0f;
			const float Radius = Capsule ? Capsule->GetScaledCapsuleRadius() : 34.0f;

			const FVector ProbeStart( DesiredLoc.X, DesiredLoc.Y, MyLoc.Z + FloorProbe );
			const FVector ProbeEnd( DesiredLoc.X, DesiredLoc.Y, MyLoc.Z - FloorProbe );

			FHitResult FloorHit;
			FCollisionQueryParams FloorParams( SCENE_QUERY_STAT( ChargeApproachFloor ), false, OwnerCharacter );
			if ( AActor* TargetActor = ChargeApproachTargetActor.Get() )
			{
				FloorParams.AddIgnoredActor( TargetActor );	// 部位コリジョンを床と誤検出しない
			}

			if ( World->SweepSingleByChannel( FloorHit, ProbeStart, ProbeEnd, FQuat::Identity, ECC_Visibility,
					FCollisionShape::MakeCapsule( Radius, HalfHeight ), FloorParams )
				&& FloorHit.Normal.Z >= MovementComp->GetWalkableFloorZ() )
			{
				DesiredLoc.Z = FloorHit.Location.Z;
			}
		}
	}

	// CMC の速度で動かすと遠距離では必要速度が上限を超えて判定時に届かない（空振り）ため、
	// スイープ付き SetActorLocation で詰める（壁・敵に当たれば停止）
	OwnerCharacter->SetActorLocation( DesiredLoc, true );

	// SetActorLocation 由来の移動は下で 0 にする Velocity に出ず、とどめの切り抜けが慣性を読めない。
	// 実移動量から計測して渡す
	if ( DeltaTime > KINDA_SMALL_NUMBER )
	{
		ChargeApproachMeasuredSpeed2D = ( OwnerCharacter->GetActorLocation() - MyLoc ).Size2D() / DeltaTime;
	}

	// 二重移動・慣性残りを防ぐ
	MovementComp->Velocity.X = 0.0f;
	MovementComp->Velocity.Y = 0.0f;

	if ( !DirToTarget.IsNearlyZero() )
	{
		OwnerCharacter->SetActorRotation( DirToTarget.Rotation() );
	}

	// 到達＝判定発生タイミング。以降はヒット→OnAttackHit の後退に委ねる
	if ( ChargeApproachTimer.IsFinish() )
	{
		MovementComp->Velocity.X = 0.0f;
		MovementComp->Velocity.Y = 0.0f;
		ClearChargeAttackTimedApproach();
	}
}

void UChargeActionPlayerModule_V2::ClearChargeAttackTimedApproach()
{
	bChargeAttackTimedApproachActive = false;
	ChargeApproachTargetActor.Reset();
	ChargeApproachTargetComp.Reset();
	ChargeApproachStartLocation = FVector::ZeroVector;
	ChargeApproachStandoff = 0.0f;
	ChargeApproachDuration = 0.0f;
	ChargeApproachTimer.Clear();
}

ULockOnTargetComponent* UChargeActionPlayerModule_V2::FindHomingTargetComponent( const FVector& InDefaultDir ) const
{
	if ( !OwnerCharacter || !OwnerCharacter->PlayerParamData || !OwnerCharacter->PlayerParamData->bEnableChargeAttackHoming )
	{
		return nullptr;
	}

	// GetHomingDirection と同じ優先順位・同じパラメータで引く
	const int32 GearIdx = FMath::Clamp( CurrentChargeGearIndex - 1, 0, OwnerCharacter->PlayerParamData->MaxChargeGearCount - 1 );

	static constexpr float FallbackMaxDist = 2000.0f;
	float MaxDist = FallbackMaxDist;
	if ( OwnerCharacter->PlayerParamData->ChargeAttackHomingDistanceForGear.IsValidIndex( GearIdx ) )
	{
		MaxDist = OwnerCharacter->PlayerParamData->ChargeAttackHomingDistanceForGear[GearIdx];
	}

	static constexpr float FallbackAngle = 10.0f;
	float Angle = FallbackAngle;
	if ( OwnerCharacter->PlayerParamData->ChargeAttackHomingAngleForGear.IsValidIndex( GearIdx ) )
	{
		Angle = OwnerCharacter->PlayerParamData->ChargeAttackHomingAngleForGear[GearIdx];
	}

	const float HalfAngleRad = FMath::DegreesToRadians( Angle * 0.5f );
	const FVector MyLoc = OwnerCharacter->GetActorLocation();

	// ロックオン対象を最優先するが、吸着範囲外の敵へ高速接近が届かないよう MaxDist 以内のときだけ採用し、
	// 範囲外・非ロック時は扇形サーチへフォールバックする
	if ( OwnerCharacter->IsLockOnActive() && OwnerCharacter->GetLockOnComponent() != nullptr )
	{
		if ( ULockOnTargetComponent* LockedComp = OwnerCharacter->GetLockOnComponent()->GetTarget() )
		{
			if ( ( LockedComp->GetTargetLocation() - MyLoc ).SizeSquared() <= MaxDist * MaxDist )
			{
				return LockedComp;
			}
		}
	}

	return TargetingUtil::FindBestTargetInFan(
		OwnerCharacter->GetWorld(), MyLoc, InDefaultDir, MaxDist, HalfAngleRad, OwnerCharacter,
		OwnerCharacter->PlayerParamData->ChargeAttackHomingApexBackOffset,
		OwnerCharacter->PlayerParamData->ChargeAttackHomingMaxHeightDiff );
}

float UChargeActionPlayerModule_V2::GetFirstHitWindowTriggerTime( UAnimMontage* Montage ) const
{
	if ( !Montage ) return -1.0f;

	float Best = -1.0f;
	for ( const FAnimNotifyEvent& Event : Montage->Notifies )
	{
		if ( Cast<UAnimNotifyState_CommonAttack>( Event.NotifyStateClass ) )
		{
			const float TriggerTime = Event.GetTriggerTime();
			if ( Best < 0.0f || TriggerTime < Best )
			{
				Best = TriggerTime;
			}
		}
	}
	return Best;
}

float UChargeActionPlayerModule_V2::GetChargeDashRemainingTime() const
{
	if ( IsPlayingChargeDash() )
	{
		return ChargedActionLockTimer.Get();
	}
	else if ( CurrentChargeActionType == EChargeActionV2Type::Jump && bResumeChargeDashOnLanding )
	{
		return ResumeChargeDashRemainingTime;
	}
	else if ( bResumeChargeDashAfterBoost )
	{
		return ChargeDashResumeTimeAfterBoost;
	}

	return 0.0f;
}

bool UChargeActionPlayerModule_V2::IsPlayingGroundChargeAttackMontage() const
{
	if ( !OwnerCharacter ) return false;

	UAnimMontage* CurrentMontage = OwnerCharacter->GetCurrentMontage();
	if ( CurrentMontage == nullptr ) return false;

	return CurrentMontage == GetAnimMontage( PlayerAnimTags::CHARGE_01_ATK ) ||
		CurrentMontage == GetAnimMontage( PlayerAnimTags::CHARGE_02_ATK ) ||
		CurrentMontage == GetAnimMontage( PlayerAnimTags::CHARGE_03_ATK ) ||
		CurrentMontage == GetAnimMontage( PlayerAnimTags::CHARGE_04_ATK );
}

bool UChargeActionPlayerModule_V2::IsPlayingChargeAttackMontage() const
{
	const UTidePlayerParamDataAsset* PlayerParams = GetPlayerParams();
	if ( !OwnerCharacter || !PlayerParams ) return false;

	if ( !PlayerParams->bEnableAttackButtonChargeCombo && !OwnerCharacter->IsAttacking() )
	{
		return false;
	}

	UAnimMontage* CurrentMontage = OwnerCharacter->GetCurrentMontage();
	if ( CurrentMontage == nullptr )
	{
		return false;
	}

	// 振り下ろしは「通常攻撃」なので弾く（縦ダイブ機械を共用しているため通すとチャージ扱いになる）
	if ( bIsAirNormalAttack )
	{
		return false;
	}

	if ( IsPlayingGroundChargeAttackMontage() )
	{
		return true;
	}

	return CurrentMontage == GetAnimMontage( PlayerAnimTags::AIRCHARGE_ATK_ST ) ||
		CurrentMontage == GetAnimMontage( PlayerAnimTags::AIRCHARGE_ATK_LP ) ||
		CurrentMontage == GetAnimMontage( PlayerAnimTags::AIRCHARGE_ATK_ED );
}

const UTidePlayerParamDataAsset* UChargeActionPlayerModule_V2::GetPlayerParams() const
{
	return OwnerCharacter ? OwnerCharacter->PlayerParamData : nullptr;
}

bool UChargeActionPlayerModule_V2::ShouldKeepAirDashFresnelSolid() const
{
	// 滑空中は持続時間の残りで点滅させたいので solid にしない
	// （滑空は別モジュールで IsAirChargeDashing() が false になるため自動的に外れる）
	return IsAirChargeDashing();
}

bool UChargeActionPlayerModule_V2::IsRevampAirDiveAttack() const
{
	return CurrentChargeActionType == EChargeActionV2Type::Attack
		&& bIsAirChargeAttack;
}

bool UChargeActionPlayerModule_V2::IsAirChargeAttackActionLocked() const
{
	// 着地 ED のキャンセル窓（MoveCancelable）へ入るまでは他アクションを一切通さない
	return IsRevampAirDiveAttack() && !IsAirChargeAttackEndCancelable();
}

bool UChargeActionPlayerModule_V2::IsAnyAirDiveAttackActive() const
{
	return CurrentChargeActionType == EChargeActionV2Type::Attack
		&& ( bIsAirChargeAttack || bIsAirNormalAttack );
}

bool UChargeActionPlayerModule_V2::ShouldAirDivePlunge() const
{
	return bIsAirNormalAttack;	// 真下プランジは縦ダイブのみ
}

FName UChargeActionPlayerModule_V2::GetAirDiveAttackStartTag() const
{
	if ( bIsAirNormalAttack ) return PlayerAnimTags::AIR_ATK_ST;
	return PlayerAnimTags::AIRCHARGE_ATK_ST;
}

FName UChargeActionPlayerModule_V2::GetAirDiveAttackLoopTag() const
{
	if ( bIsAirNormalAttack ) return PlayerAnimTags::AIR_ATK_LP;
	return PlayerAnimTags::AIRCHARGE_ATK_LP;
}

FName UChargeActionPlayerModule_V2::GetAirDiveAttackEndTag() const
{
	if ( bIsAirNormalAttack ) return PlayerAnimTags::AIR_ATK_ED;
	return PlayerAnimTags::AIRCHARGE_ATK_ED;
}

float UChargeActionPlayerModule_V2::GetAirChargeDashStPlayRate() const
{
	const UTidePlayerParamDataAsset* PlayerParams = GetPlayerParams();
	if ( !PlayerParams ) return 1.0f;

	// 2回目以降は AirChargeDash2StAnimPlayRatesForGear を優先し、空／範囲外なら1回目へフォールバック
	const int32 GearIdx = FMath::Clamp( CurrentChargeGearIndex - 1, 0, PlayerParams->MaxChargeGearCount - 1 );
	const bool bUse2ndAirDashSet = ( AirChargeDashStageCount >= 2 );
	if ( bUse2ndAirDashSet && PlayerParams->AirChargeDash2StAnimPlayRatesForGear.IsValidIndex( GearIdx ) )
	{
		const float Rate = PlayerParams->AirChargeDash2StAnimPlayRatesForGear[GearIdx];
		if ( Rate > 0.0f ) return Rate;
	}
	if ( PlayerParams->AirChargeDashStAnimPlayRatesForGear.IsValidIndex( GearIdx ) )
	{
		const float Rate = PlayerParams->AirChargeDashStAnimPlayRatesForGear[GearIdx];
		if ( Rate > 0.0f ) return Rate;
	}
	return 1.0f;
}

float UChargeActionPlayerModule_V2::GetAirChargeDashStMontageDuration() const
{
	UAnimMontage* StMontage = GetAnimMontage( GetChargeDashStartAnimTag() );
	if ( !StMontage ) return 0.0f;

	const float PlayRate = GetAirChargeDashStPlayRate();
	const float Length = StMontage->GetPlayLength();
	return ( PlayRate > 0.0f ) ? ( Length / PlayRate ) : Length;
}

bool UChargeActionPlayerModule_V2::IsAirChargeDashExhausted() const
{
	return !CanStartAirChargeDashNow();
}

bool UChargeActionPlayerModule_V2::CanStartAirChargeDashNow() const
{
	const UTidePlayerParamDataAsset* PlayerParams = GetPlayerParams();
	if ( !PlayerParams ) return true;

	// AirChargeDashStageCount は着地でリセットされる
	const int32 MaxCount = FMath::Max( 1, PlayerParams->MaxAirChargeDashCount );
	return AirChargeDashStageCount < MaxCount;
}

UCharacterMovementComponent* UChargeActionPlayerModule_V2::GetCharacterMovement() const
{
	return OwnerCharacter ? OwnerCharacter->GetCharacterMovement() : nullptr;
}

UExCameraSubsystem* UChargeActionPlayerModule_V2::GetCameraSubsystem() const
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

FVector UChargeActionPlayerModule_V2::GetActorYawForwardDirection() const
{
	if ( !OwnerCharacter )
	{
		return FVector::ForwardVector;
	}

	const FRotator ActorRot = OwnerCharacter->GetActorRotation();
	const FRotator YawRot( 0.0f, ActorRot.Yaw, 0.0f );
	return YawRot.Vector();
}

FVector UChargeActionPlayerModule_V2::ProjectDirectionToGround( const FVector& InDirection ) const
{
	FVector ProjectedDirection = InDirection;

	if ( UCharacterMovementComponent* MovementComp = GetCharacterMovement() )
	{
		if ( MovementComp->IsMovingOnGround() )
		{
			const FVector FloorNormal = OwnerCharacter->GetSmoothedFloorNormal();
			ProjectedDirection = FVector::VectorPlaneProject( ProjectedDirection, FloorNormal ).GetSafeNormal();
		}
	}

	return ProjectedDirection;
}

bool UChargeActionPlayerModule_V2::ShouldContinueChargeCombo() const
{
	// CanCombo タグだけで判定する。ヒットバックキャンセルでも許すと、モンタージュから CanCombo を外しても
	// 殴っただけで次段へ進み、受付区間をアセット側で制御できなくなる
	return OwnerCharacter && OwnerCharacter->HasStateTag( TAG_State_Player_CanCombo );
}

void UChargeActionPlayerModule_V2::ApplyGroundSnap()
{
	if ( !OwnerCharacter ) return;

	if ( IsEffectivelyInAir() ) return;

	UCharacterMovementComponent* MovementComp = GetCharacterMovement();
	if ( !MovementComp ) return;

	// 【重要】吸着は「キャラクターの上方向」基準で行う。ワールド真下へトレースして垂直上方へ置く作りだと、
	// 面沿いで傾いている間 カプセル半分×(1-cos 床角度) ぶん浮かせ続け（41°で約22cm）、
	// SetActorLocation は Velocity に出ないため CMC との位置の綱引きで「ガクッ」と見える
	const FVector Up = OwnerCharacter->GetSurfaceRideUp();

	constexpr float AscendingVelocityThreshold = 150.0f;
	if ( FVector::DotProduct( MovementComp->Velocity, Up ) > AscendingVelocityThreshold ) return;
	if ( CurrentChargeActionType == EChargeActionV2Type::Jump ) return;

	const UTidePlayerParamDataAsset* PlayerParams = GetPlayerParams();
	if ( !PlayerParams ) return;

	const FVector StartLoc = OwnerCharacter->GetActorLocation();
	const FVector EndLoc = StartLoc - Up * PlayerParams->ChargingGroundSnapTraceDistance;

	FHitResult Hit;
	FCollisionQueryParams Params( SCENE_QUERY_STAT( GroundSnapTrace ), false, OwnerCharacter );

	bool bHit = OwnerCharacter->GetWorld()->LineTraceSingleByChannel(
		Hit,
		StartLoc,
		EndLoc,
		ECC_Visibility,
		Params );

	// 歩ける床かの判定も上方向基準（重力空間）で行う
	if ( bHit && FVector::DotProduct( Hit.Normal, Up ) >= MovementComp->GetWalkableFloorZ() )
	{
		if ( AActor* HitActor = Hit.GetActor() )
		{
			if ( HitActor->IsA( ATideCharacter::StaticClass() ) )
			{
				return;
			}
		}

		const float CapsuleHalfHeight = OwnerCharacter->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
		// 距離も上方向で測る（平地では従来の Z 差分と一致する）
		const float Distance = FVector::DotProduct( StartLoc - Hit.ImpactPoint, Up ) - CapsuleHalfHeight;

		// 浮いているときだけ最大スナップ距離を見る。めり込み（負）は無条件で吸着させて強制復帰する
		bool bIsInRange = true;
		if ( Distance >= 0.0f )
		{
			bIsInRange = ( Distance < PlayerParams->ChargingGroundSnapDistance );
		}

		if ( bIsInRange )
		{
			// カプセル軸は上方向と揃っているので、傾いた面でもこれが足を着けた姿勢の中心になる
			const FVector NewLocation = Hit.ImpactPoint + Up * CapsuleHalfHeight;

			// スイープ付きで動かす。コリジョンを見ない移動だと、想定外の面を拾ったときカプセルを地形の内側へ
			// 置き、次の移動で CMC のめり込み解消に大きく押し出される（＝突然の打ち上げ）
			OwnerCharacter->SetActorLocation( NewLocation, true );

			if ( MovementComp->MovementMode == MOVE_Falling )
			{
				MovementComp->SetMovementMode( MOVE_Walking );
				// 抜くのは上方向の成分だけ。壁面ではワールド Z の速度が沿面速度そのものなので、
				// Z を 0 にすると勢いを削ってしまう
				MovementComp->Velocity -= Up * FVector::DotProduct( MovementComp->Velocity, Up );
			}
		}
	}
}

void UChargeActionPlayerModule_V2::IncrementChargeComboIndex()
{
	const UTidePlayerParamDataAsset* PlayerParams = GetPlayerParams();
	if ( !PlayerParams ) return;

	// 加算経路（FromCombo／チャージ開始／ヒットバック自動コンボ）が同一サイクルで走っても段を飛ばさない。
	// フラグは OnStartChargeAttack（攻撃発動）で解除する
	if ( bChargeComboAdvancedThisAttack ) return;
	bChargeComboAdvancedThisAttack = true;

	if ( CurrentChargeComboIndex < PlayerParams->MaxChargeComboCount )
	{
		CurrentChargeComboIndex++;
	}
	else
	{
		CurrentChargeComboIndex = 1;
		CurrentChargeGearIndex = 1;
		bIsGearShiftedByComboFinish = true;
	}
}

void UChargeActionPlayerModule_V2::ResetChargeComboIndex()
{
	CurrentChargeComboIndex = 1;
}

void UChargeActionPlayerModule_V2::InterruptCurrentChargeAction()
{
	// IsPlayingChargeAction() の真偽に関わらずリセットする
	// （ジャンプ LP から空中チャージへ移行したときに Jump ステートを破棄するため）
	if ( CurrentChargeActionType != EChargeActionV2Type::None )
	{
		ResetChargedActionState();
	}
}

void UChargeActionPlayerModule_V2::PrepareOwnerForCharge()
{
	OwnerCharacter->CancelDodge();

	if ( OwnerCharacter->IsAttacking() )
	{
		OwnerCharacter->CancelAttack();
	}

	bWasDashingBeforeCharge = OwnerCharacter->IsDashing();

	if ( bWasDashingBeforeCharge )
	{
		OwnerCharacter->StopDash();
	}
}

void UChargeActionPlayerModule_V2::InitializeChargeStartFlags()
{
	const bool bWasHitCancel = bIsHitCancelableToCharge;
	const UTidePlayerParamDataAsset* PlayerParams = GetPlayerParams();
	if ( !PlayerParams ) return;

	if ( PlayerParams->bEnableSpeedAndDriftChargeBonus )
	{
		bIsShortenedChargeActive = false;
	}
	else
	{
		bIsShortenedChargeActive = !PostActionChargeTimer.IsFinish();
	}

	PostActionChargeTimer.Clear();
	bIsChargeFromHitCancel = bWasHitCancel;

	if ( !bWasHitCancel )
	{
		CachedHitCancelVelocity = FVector::ZeroVector;
	}
}

void UChargeActionPlayerModule_V2::UpdateChargeStartComboState()
{
	const UTidePlayerParamDataAsset* PlayerParams = GetPlayerParams();
	if ( !OwnerCharacter || !PlayerParams ) return;

	const bool bIsFromCombo = ShouldContinueChargeCombo() || OwnerCharacter->IsAttacking();
	// R2 チャージ→離しのコンボは離し時に攻撃モンタージュが無く TryStartChargeAttackFromCombo が動かないため、
	// ここでの加算が要る（二重加算は IncrementChargeComboIndex が抑止する）
	if ( ShouldContinueChargeCombo() )
	{
		IncrementChargeComboIndex();
	}

	if ( !PlayerParams->bEnableChargeGearKeep &&
		!bIsGearShiftedByComboFinish &&
		!bIsFromCombo &&
		!bIsChargeKeptByDodge )
	{
		CurrentChargeGearIndex = 1;
	}

	bIsGearShiftedByComboFinish = false;
	bIsChargeKeptByDodge = false;
}

void UChargeActionPlayerModule_V2::ActivateChargeStartPresentation()
{
	PlayChargeStartMontage();
	EffectSubModule->ActivateChargeEffect();
	EffectSubModule->SpawnActiveChargeEffect();
	ApplyChargeStartMovement();
	EffectSubModule->SpawnChargeCompleteEffect();
}

void UChargeActionPlayerModule_V2::ExecuteChargeReleaseAction()
{
	if ( OwnerCharacter &&
		OwnerCharacter->PlayerParamData &&
		OwnerCharacter->PlayerParamData->bEnableChargeReleaseAttack )
	{
		RequestChargeAttack();
	}
	else
	{
		RequestChargeDash();
	}
}

void UChargeActionPlayerModule_V2::ResetChargeRuntimeState()
{
	bIsShortenedChargeActive = false;
	StopCurrentChargeMontages();
	PostActionChargeTimer.Clear();
	ResetChargedActionState();
	FrictionRecoveryTimer.Clear();
	EffectSubModule->DestroyActiveChargeEffect();
}

void UChargeActionPlayerModule_V2::RestoreDashIfNeeded( bool bRestoreDash )
{
	if ( !bWasDashingBeforeCharge || !bRestoreDash || !OwnerCharacter )
	{
		return;
	}

	ClearChargeState();
	bWasDashingBeforeCharge = false;
	OwnerCharacter->ForceStartDash();
}

void UChargeActionPlayerModule_V2::FinishChargeCancel( bool bRestoreDash )
{
	ResetChargeRuntimeState();

	if ( bWasDashingBeforeCharge && bRestoreDash )
	{
		RestoreDashIfNeeded( bRestoreDash );
		return;
	}

	ClearChargeState();
	OwnerCharacter->RefreshMovementParams();
}

void UChargeActionPlayerModule_V2::StopFinishedChargeJumpMontages()
{
	if ( !OwnerCharacter ) return;

	UAnimMontage* CurrentMontage = OwnerCharacter->GetCurrentMontage();
	UAnimMontage* JumpStMontage = GetAnimMontage( PlayerAnimTags::CHARGE_JUMP_ST );
	UAnimMontage* JumpLpMontage = GetAnimMontage( PlayerAnimTags::CHARGE_JUMP_LP );
	// ResetChargedActionState（bIsChargeHopJump クリア）がこの呼び出しより先に走るため、
	// フラグではなく再生中モンタージュとの一致だけでホップ用モンタージュも判定する
	UAnimMontage* HopStMontage = GetAnimMontage( PlayerAnimTags::CHARGE_HOP_ST );
	UAnimMontage* HopLpMontage = GetAnimMontage( PlayerAnimTags::CHARGE_HOP_LP );

	if ( CurrentMontage && ( CurrentMontage == JumpStMontage || CurrentMontage == JumpLpMontage ||
		CurrentMontage == HopStMontage || CurrentMontage == HopLpMontage ) )
	{
		constexpr float MontageBlendOutTime = 0.2f;
		OwnerCharacter->StopAnimMontage( MontageBlendOutTime, CurrentMontage );
	}
}

void UChargeActionPlayerModule_V2::HandleFinishedChargeDashEnd( bool bSkipEdAnimation )
{
	if ( !OwnerCharacter ) return;

	// Loop は BlendSpace で表現するためモンタージュを持たない（ST のみ停止する）
	UAnimMontage* DashStMontage = GetAnimMontage( GetChargeDashStartAnimTag() );
	OwnerCharacter->StopAnimMontage( DashStMontage );

	const float InputThreshold = OwnerCharacter->PlayerParamData ? OwnerCharacter->PlayerParamData->DashCancelInputThreshold : 0.2f;
	const FVector2D RawInput = OwnerCharacter->GetRawMovementInput();

	if ( bSkipEdAnimation || IsEffectivelyInAir() )
	{
		return;
	}

	// ダッシュ終了時に R2 を握っていれば溜め直す（押し直し待ち中は自動再開しない）
	if ( bChargeInputHeld && !IsAutoChargeRestartBlocked() )
	{
		BeginCharge();
		return;
	}

	if ( RawInput.Size() > InputThreshold )
	{
		OwnerCharacter->ForceStartDash();
		return;
	}

	if ( UPlayerAnimInstance* AnimInst = Cast<UPlayerAnimInstance>( OwnerCharacter->GetMesh()->GetAnimInstance() ) )
	{
		AnimInst->bIsDashBSPlaying = false;
	}

	UAnimMontage* DashEdMontage = GetAnimMontage( GetChargeDashEndAnimTag() );
	if ( OwnerCharacter->GetCurrentMontage() != DashEdMontage )
	{
		PlayAnimMontage( GetChargeDashEndAnimTag() );
	}
}

void UChargeActionPlayerModule_V2::StartPostActionFrictionRecovery( EChargeActionV2Type FinishedActionType )
{
	if ( !OwnerCharacter ) return;

	OwnerCharacter->CaptureFrictionRecoveryStartParams();
	OwnerCharacter->RefreshMovementParams();

	if ( const UTidePlayerParamDataAsset* PlayerParams = GetPlayerParams() )
	{
		// チャージ攻撃の出し切りだけはヒットバックを長く滑らせるため専用時間を使う（0 以下なら共通値）
		float RecoveryTime = PlayerParams->ChargeActionFrictionRecoveryTime;
		if ( FinishedActionType == EChargeActionV2Type::Attack && PlayerParams->ChargeAttackFrictionRecoveryTime > 0.0f )
		{
			RecoveryTime = PlayerParams->ChargeAttackFrictionRecoveryTime;
		}

		FrictionRecoveryTimer.Set( RecoveryTime );
		bIsAirFrictionRecovery = IsEffectivelyInAir();
	}

	OwnerCharacter->UpdateFrictionRecovery( 0.0f );
}

void UChargeActionPlayerModule_V2::StartChargeState()
{
	const UTidePlayerParamDataAsset* PlayerParams = GetPlayerParams();
	if ( !PlayerParams ) return;

	bIsCharging = true;
	bHasReachedMaxCharge = false;

	OwnerCharacter->ResetChargingTurnRamp();
	OwnerCharacter->ResetDriftSpeedMaintain();
	ResetDriftGearUpState();	// 溜め直しは新しいドリフトセッション扱い（クールダウン・方向履歴を持ち越さない）

	CurrentChargeTimer.Set( PlayerParams->MaxChargeTime );
	FrictionRecoveryTimer.Clear();

	// ギアが上がるまでの時間を短縮フラグで切り替える
	const float ShiftHoldTime = bIsShortenedChargeActive ? PlayerParams->PostActionMaxChargeTime : PlayerParams->ChargeV2ShiftHoldTime;
	ChargeShiftTimer.Set( ShiftHoldTime );

	if ( EffectSubModule ) EffectSubModule->NotifyChargeStarted( PlayerParams->GhostTrailSpawnInterval );

	UpdateGearShiftCamera();

	if ( EffectSubModule ) EffectSubModule->NotifyChargeGearUIPop();
}

void UChargeActionPlayerModule_V2::PlayChargeStartMontage()
{
	float PlayRate = 1.0f;

	if ( bIsChargeFromHitCancel )
	{
		const UTidePlayerParamDataAsset* PlayerParams = GetPlayerParams();
		if ( PlayerParams )
		{
			PlayRate = PlayerParams->ChargeStartAnimPlayRateFromHitCancel;
		}
	}

	PlayAnimMontage( GetChargeStartAnimTag(), PlayRate );
}

void UChargeActionPlayerModule_V2::ApplyChargeStartMovement()
{
	const UTidePlayerParamDataAsset* PlayerParams = GetPlayerParams();
	OwnerCharacter->RefreshMovementParams();

	// ヒットバックキャンセル由来なら踏み込み推進力は加えず、キャッシュした慣性だけ復元する
	// （傾斜地ではヒットバック直後に着地して摩擦で速度が消えるため）
	if ( bIsChargeFromHitCancel )
	{
		if ( !CachedHitCancelVelocity.IsNearlyZero() )
		{
			if ( UCharacterMovementComponent* MovementComp = GetCharacterMovement() )
			{
				MovementComp->Velocity.X = CachedHitCancelVelocity.X;
				MovementComp->Velocity.Y = CachedHitCancelVelocity.Y;
			}
			CachedHitCancelVelocity = FVector::ZeroVector;
		}
		return;
	}

	if ( !PlayerParams || PlayerParams->ChargeBeginMovePower <= 0.0f )
	{
		return;
	}

	// 棒立ちチャージでは前進させない
	const FVector2D RawInput = OwnerCharacter->GetRawMovementInput();
	if ( RawInput.Size() < PlayerParams->DashCancelInputThreshold )
	{
		return;
	}

	const FVector ForwardDir = ProjectDirectionToGround( GetActorYawForwardDirection() );

	OwnerCharacter->LaunchCharacter( ForwardDir * PlayerParams->ChargeBeginMovePower, false, false );
}

void UChargeActionPlayerModule_V2::StopCurrentChargeMontages()
{
	UAnimMontage* StartMontage = GetAnimMontage( GetChargeStartAnimTag() );
	UAnimMontage* DashStartMontage = GetAnimMontage( GetChargeDashStartAnimTag() );
	UAnimMontage* DashEndMontage = GetAnimMontage( GetChargeDashEndAnimTag() );

	if ( StartMontage && OwnerCharacter->GetCurrentMontage() == StartMontage )
	{
		OwnerCharacter->StopAnimMontage( StartMontage );
	}
	if ( DashStartMontage && OwnerCharacter->GetCurrentMontage() == DashStartMontage )
	{
		OwnerCharacter->StopAnimMontage( DashStartMontage );
	}
	if ( DashEndMontage && OwnerCharacter->GetCurrentMontage() == DashEndMontage )
	{
		OwnerCharacter->StopAnimMontage( DashEndMontage );
	}
}

void UChargeActionPlayerModule_V2::ShiftUpGear( bool bFromManualInput )
{
	const UTidePlayerParamDataAsset* PlayerParams = GetPlayerParams();
	if ( !PlayerParams ) return;
	if ( CurrentChargeGearIndex >= PlayerParams->MaxChargeGearCount ) return;

	if ( bFromManualInput )
	{
		// R1 はモード ON のときだけ受け付ける。溜め中限定なのは、ギアカメラの Push が
		// 溜め終了時の Pop と対にならないため
		if ( !PlayerParams->bManualGearUpOnly || !bIsCharging ) return;
	}
	else
	{
		// モード ON の間は自動上昇（時間経過シフト・AnimNotify_ShiftUpGear）を弾く
		if ( PlayerParams->bManualGearUpOnly ) return;
	}

	CurrentChargeGearIndex++;
	UpdateGearShiftCamera();
	if ( EffectSubModule ) EffectSubModule->NotifyChargeGearUIPop();
	EffectSubModule->SpawnChargeCompleteEffect();
	EffectSubModule->NotifyGearShifted();
	EffectSubModule->NotifyDriftSparkGearUpBurst();
}

bool UChargeActionPlayerModule_V2::IsHitbackChargeHoldActive() const
{
	// ステアリング制限と同じ区間。このタイマーは UpdateChargingState でしか進まないので、
	// 溜めが切れて凍結したものを拾わないよう bIsCharging も見る
	return bIsCharging && !HitCancelChargingSteeringTimer.IsFinish();
}

bool UChargeActionPlayerModule_V2::IsHitbackChargeSparkActive() const
{
	const UTidePlayerParamDataAsset* PlayerParams = GetPlayerParams();
	return PlayerParams && PlayerParams->bEnableHitbackChargeSpark && IsHitbackChargeHoldActive();
}

void UChargeActionPlayerModule_V2::UpdateHitbackChargeGearUp( float DeltaTime )
{
	const UTidePlayerParamDataAsset* PlayerParams = GetPlayerParams();
	if ( !PlayerParams ) return;

	if ( !IsHitbackChargeHoldActive() )
	{
		// 溜めが切れた／別の溜めに変わったので計測を畳む
		bWasHitbackChargeHold = false;
		bHitbackChargeGearUpDone = false;
		HitbackChargeHoldTimer.Clear();
		return;
	}

	if ( !bWasHitbackChargeHold )
	{
		bWasHitbackChargeHold = true;
		HitbackChargeHoldTimer.Set( FMath::Max( PlayerParams->HitbackChargeGearUpTime, 0.0f ) );
	}

	if ( !PlayerParams->bEnableHitbackChargeGearUp || bHitbackChargeGearUpDone ) return;

	HitbackChargeHoldTimer.Update( DeltaTime );
	if ( !HitbackChargeHoldTimer.IsFinish() ) return;

	bHitbackChargeGearUpDone = true;

	const int32 PrevGear = CurrentChargeGearIndex;
	ShiftUpGear();

	// 火花は「ギアが上がった印」なので実際に段が上がったときだけ出す（突風と同じ規約）。
	// ドリフトしていないと火花ループ自体が出ないため強制バーストを使う
	if ( CurrentChargeGearIndex > PrevGear && EffectSubModule )
	{
		EffectSubModule->NotifyDriftSparkForcedBurst();
	}
}

void UChargeActionPlayerModule_V2::ShiftDownGear()
{
	const UTidePlayerParamDataAsset* PlayerParams = GetPlayerParams();
	if ( !PlayerParams || !PlayerParams->bEnableManualGearDown ) return;
	if ( !bIsCharging || CurrentChargeGearIndex <= 1 ) return;

	CurrentChargeGearIndex--;

	// 下げた直後に残っていたシフト時間で即座に上がり直さないよう、次のシフトまでの時間を張り直す
	const float ShiftHoldTime = bIsShortenedChargeActive ? PlayerParams->PostActionMaxChargeTime : PlayerParams->ChargeV2ShiftHoldTime;
	ChargeShiftTimer.Set( ShiftHoldTime );

	UpdateGearShiftCamera();
	// 演出はギア色・UI の追従のみ（シフトアップの爆発は出さない）
	if ( EffectSubModule )
	{
		EffectSubModule->NotifyChargeGearUIPop();
		EffectSubModule->NotifyGearShifted();
	}
}

bool UChargeActionPlayerModule_V2::SetGearToMax()
{
	const UTidePlayerParamDataAsset* PlayerParams = GetPlayerParams();
	if ( !PlayerParams ) return false;
	if ( CurrentChargeGearIndex >= PlayerParams->MaxChargeGearCount ) return false;
	// R1 限定モード中は突風による最大ギア化も自動上昇として弾く
	if ( PlayerParams->bManualGearUpOnly ) return false;

	// 演出は ShiftUpGear と同じ口を使う
	CurrentChargeGearIndex = PlayerParams->MaxChargeGearCount;
	UpdateGearShiftCamera();
	if ( EffectSubModule )
	{
		EffectSubModule->NotifyChargeGearUIPop();
		EffectSubModule->SpawnChargeCompleteEffect();
		EffectSubModule->NotifyGearShifted();
		EffectSubModule->NotifyDriftSparkGearUpBurst();
	}

	return true;
}

void UChargeActionPlayerModule_V2::NotifyDriftSparkForcedBurst()
{
	if ( EffectSubModule ) EffectSubModule->NotifyDriftSparkForcedBurst();
}

void UChargeActionPlayerModule_V2::ArmGustChargeBuff()
{
	bGustChargeBuffArmed = true;
}

void UChargeActionPlayerModule_V2::ClearGustChargeBuff()
{
	// 残すと次のチャージダッシュで ConsumeGustChargeBuffIfArmed が走ってバーストが暴発する
	// （VFX・足元印は SubModule が片付ける）
	bGustChargeBuffArmed = false;
}

namespace
{
	// ギア概念なしのタグ一致・1 行想定。見つからなければ nullptr（呼び出し側は DA の値を使う）
	const FPlayerAttackParameterRow* FindGustAttackRow( const ATidePlayerCharacter* Player )
	{
		if ( !Player ) return nullptr;
		const UTideCharacterDataAsset* DataAsset = Player->GetCharacterData();
		if ( !DataAsset || !DataAsset->AttackParameterTable ) return nullptr;

		static const FString Context = TEXT( "SlidePassiveGustAttackParamLookup" );
		TArray<FPlayerAttackParameterRow*> Rows;
		DataAsset->AttackParameterTable->GetAllRows<FPlayerAttackParameterRow>( Context, Rows );
		for ( const FPlayerAttackParameterRow* Row : Rows )
		{
			if ( Row && Row->AttackTypeTag == TAG_AttackType_Player_SlidePassiveGust )
			{
				return Row;
			}
		}
		return nullptr;
	}
}

void UChargeActionPlayerModule_V2::ConsumeGustChargeBuffIfArmed()
{
	if ( !bGustChargeBuffArmed ) return;
	bGustChargeBuffArmed = false;		// 単発消費
	bGustBuffedActionActive = true;		// このチャージアクションは突風バフ付き（アクション終了で下ろす）

	if ( !OwnerCharacter ) return;
	const UTidePlayerParamDataAsset* PlayerParams = GetPlayerParams();
	if ( !PlayerParams ) return;

	UWorld* World = OwnerCharacter->GetWorld();
	if ( !World ) return;

	UNiagaraSystem* BurstVFX = nullptr;
	if ( OwnerCharacter->NiagaraSystemDataAsset )
	{
		BurstVFX = OwnerCharacter->NiagaraSystemDataAsset->GetNiagaraSystem( PlayerNiagaraTags::PASSIVE_GUST_BURST );
	}

	DestroyActiveGustBurst();	// 多重スポーン防止
	const FVector SpawnLoc = OwnerCharacter->GetActorLocation();
	const FVector BurstRelativeLocation = PlayerParams->GustBurstRelativeLocation;
	const FRotator BurstRelativeRotation = PlayerParams->GustBurstRelativeRotation;

	// AttackParameterTable（基礎攻撃力×行の倍率）を優先し、行が無ければ DA の値を使う
	float GustDamage = PlayerParams->GustBurstDamage;
	if ( const FPlayerAttackParameterRow* AttackRow = FindGustAttackRow( OwnerCharacter ) )
	{
		GustDamage = OwnerCharacter->CalculateDamage( AttackRow->DamageMultiplier );
	}

	// 寿命は固定せず「バフ付きアクションが終わるまで」持続させる（LifeTime 0 ＝自己破棄しない外部管理）
	if ( PlayerParams->GustBurstClass )
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = OwnerCharacter;
		SpawnParams.Instigator = OwnerCharacter;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		ASlidePassiveGustBurst* Burst = World->SpawnActor<ASlidePassiveGustBurst>(
			PlayerParams->GustBurstClass, SpawnLoc, OwnerCharacter->GetActorRotation(), SpawnParams );
		if ( Burst )
		{
			// SnapToTarget で一度親の Transform に完全同期させてから相対値を当てる
			Burst->AttachToActor( OwnerCharacter, FAttachmentTransformRules::SnapToTargetNotIncludingScale );
			Burst->SetActorRelativeLocation( BurstRelativeLocation );
			Burst->SetActorRelativeRotation( BurstRelativeRotation );

			Burst->Activate( BurstVFX, PlayerParams->GustBurstRadius, GustDamage,
				/*LifeTime=*/0.0f, PlayerParams->GustBurstScale );
			ActiveGustBurst = Burst;
		}
	}
	else if ( BurstVFX )
	{
		// クラス未設定時のフォールバック。これも追従させ、アクション終了まで持続させる
		UNiagaraComponent* Effect = UNiagaraFunctionLibrary::SpawnSystemAttached(
			BurstVFX, OwnerCharacter->GetRootComponent(), NAME_None,
			BurstRelativeLocation, BurstRelativeRotation, EAttachLocation::KeepRelativeOffset,
			false,	// bAutoDestroy — アクション終了でこちらが破棄する
			true );
		if ( Effect )
		{
			Effect->SetFloatParameter( TEXT( "Scale" ), PlayerParams->GustBurstScale );
			ActiveGustBurstVFX = Effect;
		}
	}
}

void UChargeActionPlayerModule_V2::DestroyActiveGustBurst()
{
	// フェード中の破棄でも確実に状態をリセットする
	bGustBurstFadingOut = false;
	GustBurstFadeElapsed = 0.0f;

	if ( ActiveGustBurst )
	{
		ActiveGustBurst->Destroy();
		ActiveGustBurst = nullptr;
	}
	if ( ActiveGustBurstVFX )
	{
		ActiveGustBurstVFX->Deactivate();
		ActiveGustBurstVFX->DestroyComponent();
		ActiveGustBurstVFX = nullptr;
	}
}

void UChargeActionPlayerModule_V2::BeginFadeOutActiveGustBurst()
{
	if ( !ActiveGustBurst && !ActiveGustBurstVFX ) return;

	const UTidePlayerParamDataAsset* PlayerParams = GetPlayerParams();
	const float FadeDuration = PlayerParams ? PlayerParams->GustBurstFadeOutTime : 0.0f;

	if ( FadeDuration <= 0.0f )
	{
		DestroyActiveGustBurst();
		return;
	}

	// フェード中は見た目だけ残し、範囲ダメージは止める（消えかけのバーストで多段ヒットしないように）
	if ( ActiveGustBurst )
	{
		ActiveGustBurst->SuspendDamage();
	}

	bGustBurstFadingOut = true;
	GustBurstFadeElapsed = 0.0f;
}

void UChargeActionPlayerModule_V2::UpdateGustBurstFadeOut( float DeltaTime )
{
	if ( !bGustBurstFadingOut ) return;

	const UTidePlayerParamDataAsset* PlayerParams = GetPlayerParams();
	const float FadeDuration = PlayerParams ? PlayerParams->GustBurstFadeOutTime : 0.0f;

	GustBurstFadeElapsed += DeltaTime;
	const float Alpha = ( FadeDuration > 0.0f )
		? FMath::Clamp( 1.0f - GustBurstFadeElapsed / FadeDuration, 0.0f, 1.0f )
		: 0.0f;

	if ( ActiveGustBurst && ActiveGustBurst->BurstVFX )
	{
		ActiveGustBurst->BurstVFX->SetFloatParameter( TEXT( "Alpha" ), Alpha );
	}
	if ( ActiveGustBurstVFX )
	{
		ActiveGustBurstVFX->SetFloatParameter( TEXT( "Alpha" ), Alpha );
	}

	if ( GustBurstFadeElapsed >= FadeDuration )
	{
		DestroyActiveGustBurst();
	}
}

UChargeActionPlayerModule_V2::FChargePropulsionSettings UChargeActionPlayerModule_V2::BuildChargePropulsionSettings() const
{
	FChargePropulsionSettings Settings;
	const UTidePlayerParamDataAsset* PlayerParams = GetPlayerParams();
	if ( !OwnerCharacter || !PlayerParams ) return Settings;

	const int32 ComboIdx = FMath::Clamp( CurrentChargeComboIndex - 1, 0, PlayerParams->MaxChargeComboCount - 1 );
	const int32 GearIdx = FMath::Clamp( CurrentChargeGearIndex - 1, 0, PlayerParams->MaxChargeGearCount - 1 );

	switch ( CurrentChargeActionType )
	{
	case EChargeActionV2Type::Dash:
		BuildDashPropulsionSettings( Settings, ComboIdx, GearIdx );
		break;

	case EChargeActionV2Type::Attack:
		BuildAttackPropulsionSettings( Settings, ComboIdx, GearIdx );
		break;
	default:
		break;
	}

	return Settings;
}

void UChargeActionPlayerModule_V2::OnStartLightAttack()
{
	if ( !OwnerCharacter ) return;
	// 地上の弱攻撃マスク中（DA: bEnableGroundNormalAttack が OFF）は RequestAttack が弾くため、
	// コンボ最終段の締めは何も出ずロコモーションへ戻る
	OwnerCharacter->RequestAttack( EPlayerAttackType::Light );
}

FVector UChargeActionPlayerModule_V2::GetHomingDirection( const FVector& InDefaultDir, float InDebugDuration ) const
{
	if ( !OwnerCharacter || !OwnerCharacter->PlayerParamData || !OwnerCharacter->PlayerParamData->bEnableChargeAttackHoming )
	{
		return InDefaultDir;
	}

	const int32 GearIdx = FMath::Clamp( CurrentChargeGearIndex - 1, 0, OwnerCharacter->PlayerParamData->MaxChargeGearCount - 1 );

	// Fallback は配列の設定漏れに備えたフェイルセーフ
	static constexpr float FallbackMaxDist = 2000.0f;
	float MaxDist = FallbackMaxDist;
	if ( OwnerCharacter->PlayerParamData->ChargeAttackHomingDistanceForGear.IsValidIndex( GearIdx ) )
	{
		MaxDist = OwnerCharacter->PlayerParamData->ChargeAttackHomingDistanceForGear[GearIdx];
	}

	static constexpr float FallbackAngle = 10.0f;
	float Angle = FallbackAngle;
	if ( OwnerCharacter->PlayerParamData->ChargeAttackHomingAngleForGear.IsValidIndex( GearIdx ) )
	{
		Angle = OwnerCharacter->PlayerParamData->ChargeAttackHomingAngleForGear[GearIdx];
	}

	// 攻撃の扇はシアンで描画（ダッシュのオレンジと区別する）
	const float ApexBackOffset = OwnerCharacter->PlayerParamData->ChargeAttackHomingApexBackOffset;
	const float MaxHeightDiff = OwnerCharacter->PlayerParamData->ChargeAttackHomingMaxHeightDiff;
	return GetHomingDirectionWithParams( InDefaultDir, MaxDist, Angle, ApexBackOffset, MaxHeightDiff, InDebugDuration, FColor::Cyan );
}

FVector UChargeActionPlayerModule_V2::GetChargeDashHomingDirection( const FVector& InDefaultDir, float InDebugDuration, ULockOnTargetComponent** OutTargetComp ) const
{
	if ( OutTargetComp ) *OutTargetComp = nullptr;

	if ( !OwnerCharacter || !OwnerCharacter->PlayerParamData || !OwnerCharacter->PlayerParamData->bEnableChargeDashHoming )
	{
		return InDefaultDir;
	}

	const int32 GearIdx = FMath::Clamp( CurrentChargeGearIndex - 1, 0, OwnerCharacter->PlayerParamData->MaxChargeGearCount - 1 );

	static constexpr float FallbackMaxDist = 2000.0f;
	float MaxDist = FallbackMaxDist;
	if ( OwnerCharacter->PlayerParamData->ChargeDashHomingDistanceForGear.IsValidIndex( GearIdx ) )
	{
		MaxDist = OwnerCharacter->PlayerParamData->ChargeDashHomingDistanceForGear[GearIdx];
	}

	static constexpr float FallbackAngle = 10.0f;
	float Angle = FallbackAngle;
	if ( OwnerCharacter->PlayerParamData->ChargeDashHomingAngleForGear.IsValidIndex( GearIdx ) )
	{
		Angle = OwnerCharacter->PlayerParamData->ChargeDashHomingAngleForGear[GearIdx];
	}

	// ロックオン中でも対象へ強制ホーミングせず、通常時と同じ扇形サーチで吸着させる（bPreferLockOnTarget=false）
	const float ApexBackOffset = OwnerCharacter->PlayerParamData->ChargeDashHomingApexBackOffset;
	const float MaxHeightDiff = OwnerCharacter->PlayerParamData->ChargeDashHomingMaxHeightDiff;
	return GetHomingDirectionWithParams( InDefaultDir, MaxDist, Angle, ApexBackOffset, MaxHeightDiff, InDebugDuration, FColor::Orange, OutTargetComp, /*bPreferLockOnTarget=*/false );
}

FVector UChargeActionPlayerModule_V2::GetHomingDirectionWithParams( const FVector& InDefaultDir, float MaxDist, float Angle, float ApexBackOffset, float MaxHeightDiff, float InDebugDuration, FColor DebugAreaColor, ULockOnTargetComponent** OutTargetComp, bool bPreferLockOnTarget ) const
{
	const float HalfAngleRad = FMath::DegreesToRadians( Angle * 0.5f );
	const FVector MyLoc = OwnerCharacter->GetActorLocation();

	// bPreferLockOnTarget=false のときはロックオン対象への強制ホーミングを行わず、扇形サーチに任せる
	ULockOnTargetComponent* LockedTargetComp = nullptr;
	if ( bPreferLockOnTarget && OwnerCharacter->IsLockOnActive() && OwnerCharacter->GetLockOnComponent() != nullptr )
	{
		LockedTargetComp = OwnerCharacter->GetLockOnComponent()->GetTarget();
	}

	FVector ResultDir = InDefaultDir;
	ULockOnTargetComponent* FinalTargetComp = nullptr;

	if ( LockedTargetComp != nullptr )
	{
		ResultDir = ( LockedTargetComp->GetTargetLocation() - MyLoc ).GetSafeNormal2D();
		FinalTargetComp = LockedTargetComp;
	}
	else
	{
		FinalTargetComp = TargetingUtil::FindBestTargetInFan(
			OwnerCharacter->GetWorld(), MyLoc, InDefaultDir, MaxDist, HalfAngleRad, OwnerCharacter, ApexBackOffset, MaxHeightDiff );

		if ( FinalTargetComp )
		{
			ResultDir = ( FinalTargetComp->GetTargetLocation() - MyLoc ).GetSafeNormal2D();
		}
	}

	if ( UTideGameSettings::Get()->bDebugFlagDrawHomingArea )
	{
		// 実際の角度判定と同じく、扇の頂点を後方シフトした位置から描画する
		const FVector ApexLoc = MyLoc - InDefaultDir * ApexBackOffset;
		OwnerCharacter->DrawDebugHomingArea( ApexLoc, InDefaultDir, ResultDir, FinalTargetComp, MaxDist, Angle, InDebugDuration, DebugAreaColor, MaxHeightDiff );
	}

	if ( OutTargetComp ) *OutTargetComp = FinalTargetComp;

	return ResultDir;
}

float UChargeActionPlayerModule_V2::GetSlopeChargeTimeMultiplier() const
{
	float Multiplier = 1.0f;

	UCharacterMovementComponent* Movement = GetCharacterMovement();
	if ( !Movement || !Movement->IsMovingOnGround() || Movement->Velocity.IsNearlyZero() )
	{
		return Multiplier;
	}

	const FVector FloorNormal = OwnerCharacter->GetSmoothedFloorNormal();
	const FVector ForwardDir = OwnerCharacter->GetActorForwardVector();
	const FVector SlopeVector = FVector::VectorPlaneProject( ForwardDir, FloorNormal ).GetSafeNormal();

	constexpr float DownhillThresholdZ = -0.05f;
	if ( SlopeVector.Z >= DownhillThresholdZ )
	{
		return Multiplier;
	}

	const UTidePlayerParamDataAsset* PlayerParams = OwnerCharacter->PlayerParamData;
	if ( !PlayerParams ) return Multiplier;

	// 下り系パラメータは坂道補正（UpdateChargingMovementParams）と同じギア別配列を共有する。
	// ここだけ別の値を見ていると、ギア別に分けたとき蓄積レートと移動補正がズレる
	auto ForGear = [&]( const TArray<float>& Arr )
	{
		return UTidePlayerParamDataAsset::GetValueForGear( Arr, CurrentChargeGearIndex );
	};

	const float CurrentSpeed = Movement->Velocity.Size();
	const float MaxDownhillSpeed = ForGear( PlayerParams->MaxSlideSpeedForGear );
	const float SpeedRatio = FMath::Clamp( CurrentSpeed / MaxDownhillSpeed, 0.0f, 1.0f );

	const float DescentAngleRad = FMath::Asin( FMath::Abs( SlopeVector.Z ) );
	const float DescentAngleDeg = FMath::RadiansToDegrees( DescentAngleRad );
	const float SteepAngle = ForGear( PlayerParams->SteepDescentAngleForGear );

	float TerrainBonus = 0.0f;

	if ( DescentAngleDeg >= SteepAngle )
	{
		TerrainBonus = 1.0f;
	}
	else
	{
		const float AngleRatio = DescentAngleDeg / SteepAngle;
		TerrainBonus = AngleRatio * ForGear( PlayerParams->GentleDownhillMaxBonusRateForGear );
		TerrainBonus = FMath::Pow( TerrainBonus, ForGear( PlayerParams->DownhillSensitivityForGear ) );
	}

	const float EffectiveBaseBonus = ForGear( PlayerParams->ChargingDownhillBaseBonusForGear ) * SpeedRatio * TerrainBonus;
	Multiplier += ( EffectiveBaseBonus * static_cast< float >( CurrentChargeGearIndex ) );

	Multiplier = FMath::Clamp( Multiplier,
		ForGear( PlayerParams->MinChargingDownhillMultiplierForGear ),
		ForGear( PlayerParams->MaxChargingDownhillMultiplierForGear ) );

	return Multiplier;
}

void UChargeActionPlayerModule_V2::UpdateDriftBoostState( float DeltaTime )
{
	// 条件は「チャージ中・接地・速度」＋「n°以上のドリフトを t 秒継続」。
	// 結果（bIsDriftBoostActive / CurrentDriftBoostAlpha）はボーナスと演出が共有する
	bIsDriftBoostActive = false;
	CurrentDriftBoostAlpha = 0.0f;
	bIsRawDrifting = false;
	CurrentDriftTurnDirection = EDriftTurnDirection::None;

	const UTidePlayerParamDataAsset* PlayerParams = GetPlayerParams();
	if ( !PlayerParams || !PlayerParams->bEnableSpeedAndDriftChargeBonus || !OwnerCharacter )
	{
		DriftSustainTimer = 0.0f;
		return;
	}

	UCharacterMovementComponent* Movement = GetCharacterMovement();

	// チャージ中・接地中のみ。ヒットキャンセル制限窓中は後退の勢いで暴発しないよう無効化。
	if ( !Movement || !IsCharging() || IsEffectivelyInAir() || !HitCancelChargingSteeringTimer.IsFinish() )
	{
		DriftSustainTimer = 0.0f;
		return;
	}

	const float CurrentSpeed = Movement->Velocity.Size2D();

	// 瞬間ドリフト条件（角度 ＋ 速度 ＋ 入力）
	bool bRawDrifting = false;
	float RawDriftAlpha = 0.0f;
	EDriftTurnDirection RawDriftDirection = EDriftTurnDirection::None;

	const FVector2D RawInput = OwnerCharacter->GetRawMovementInput();
	if ( CurrentSpeed >= PlayerParams->MinSpeedForChargeBonus && !RawInput.IsNearlyZero() )
	{
		// 壁ずり時の微小な横滑りを無視し、低速時は正面方向を進行方向として扱う
		FVector CurrentDir = FVector::ZeroVector;
		if ( CurrentSpeed < PlayerParams->MinSpeedForVelocityDirection )
		{
			CurrentDir = OwnerCharacter->GetActorForwardVector().GetSafeNormal2D();
		}
		else
		{
			CurrentDir = Movement->Velocity.GetSafeNormal2D();
		}

		const FVector InputWorldDir = OwnerCharacter->GetControlRelativeInputDirection( RawInput ).GetSafeNormal2D();
		const float DotProduct = FVector::DotProduct( CurrentDir, InputWorldDir );

		// なす角が n°以上 ⇔ DotProduct <= cos(n°)
		const float AngleThresholdDot = FMath::Cos( FMath::DegreesToRadians( PlayerParams->DriftBoostAngleThreshold ) );

		if ( DotProduct <= AngleThresholdDot )
		{
			bRawDrifting = true;
			// 角度が深いほど 1.0 に近づく強度
			RawDriftAlpha = FMath::GetMappedRangeValueClamped(
				FVector2D( AngleThresholdDot, -1.0f ),
				FVector2D( 0.0f, 1.0f ),
				DotProduct
			);

			// 外積 Z が正なら右ターン。真後ろ（内積 -1）付近では外積がほぼ 0 で左右が確定しないため
			// None のままにする（切り返し判定に使わない）
			const float TurnSign = FVector::CrossProduct( CurrentDir, InputWorldDir ).Z;
			constexpr float TurnSignEpsilon = 0.05f;
			if ( TurnSign > TurnSignEpsilon )		RawDriftDirection = EDriftTurnDirection::Right;
			else if ( TurnSign < -TurnSignEpsilon )	RawDriftDirection = EDriftTurnDirection::Left;
		}
	}

	bIsRawDrifting = bRawDrifting;
	CurrentDriftTurnDirection = RawDriftDirection;

	// 継続タイマー（途切れたら即リセット）
	if ( bRawDrifting )
	{
		DriftSustainTimer += DeltaTime;
	}
	else
	{
		DriftSustainTimer = 0.0f;
	}

	// t 秒継続でゲート開放
	if ( DriftSustainTimer >= PlayerParams->DriftBoostSustainTime )
	{
		bIsDriftBoostActive = true;
		CurrentDriftBoostAlpha = RawDriftAlpha;
	}
}

void UChargeActionPlayerModule_V2::UpdateDriftGearUp( float DeltaTime )
{
	// 同じ方向で曲がり続けるとギアが1段上がり、以降その方向はクールダウンで塞がる。
	// 左右を切り返すと解けて再び上げられる（右→左と繋いで壱→弐→参まで持っていける）
	const UTidePlayerParamDataAsset* PlayerParams = GetPlayerParams();
	if ( !PlayerParams || !PlayerParams->bEnableSpeedAndDriftChargeBonus || !PlayerParams->bUseDriftGearUpBonus )
	{
		ResetDriftGearUpState();
		return;
	}

	// クールダウンは実時間で消化する（ドリフトが途切れている間も進む）
	DriftGearUpCooldownTimer = FMath::Max( 0.0f, DriftGearUpCooldownTimer - DeltaTime );

	// 角度・速度・入力のどれかが切れている間は継続をやり直す
	if ( !bIsDriftBoostActive )
	{
		DriftGearUpSustainTimer = 0.0f;
		DriftGearUpSustainDirection = EDriftTurnDirection::None;
		return;
	}

	// 最後に上げた方向と逆へドリフトしたらクールダウンを解除し、
	// 方向履歴も消化して次のギアアップで記録し直す
	if ( CurrentDriftTurnDirection != EDriftTurnDirection::None &&
		LastDriftGearUpDirection != EDriftTurnDirection::None &&
		CurrentDriftTurnDirection != LastDriftGearUpDirection )
	{
		DriftGearUpCooldownTimer = 0.0f;
		LastDriftGearUpDirection = EDriftTurnDirection::None;

		if ( PlayerParams->bDriftGearUpImmediateOnReverse )
		{
			TryDriftGearUp();
			return;
		}
	}

	// 継続中の方向が変わったら計測をやり直す（「同じ角度で曲がり続ける」の担保）
	if ( CurrentDriftTurnDirection != DriftGearUpSustainDirection )
	{
		DriftGearUpSustainTimer = 0.0f;
		DriftGearUpSustainDirection = CurrentDriftTurnDirection;
	}

	if ( DriftGearUpCooldownTimer > 0.0f )
	{
		DriftGearUpSustainTimer = 0.0f;
		return;
	}

	DriftGearUpSustainTimer += DeltaTime;
	if ( DriftGearUpSustainTimer >= PlayerParams->DriftGearUpSustainTime )
	{
		TryDriftGearUp();
	}
}

void UChargeActionPlayerModule_V2::TryDriftGearUp()
{
	const UTidePlayerParamDataAsset* PlayerParams = GetPlayerParams();
	if ( !PlayerParams ) return;

	DriftGearUpSustainTimer = 0.0f;

	// 上がらなかった場合（最大ギア到達・bManualGearUpOnly でマスク）はクールダウンも張らない
	const int32 GearBefore = CurrentChargeGearIndex;
	ShiftUpGear();
	if ( CurrentChargeGearIndex == GearBefore ) return;

	// 真後ろへのターン（左右が確定しない）で上げた場合は前回方向を残す。None で上書きすると
	// 切り返しでクールダウンを解除できなくなるため
	if ( CurrentDriftTurnDirection != EDriftTurnDirection::None )
	{
		LastDriftGearUpDirection = CurrentDriftTurnDirection;
	}
	DriftGearUpCooldownTimer = PlayerParams->DriftGearUpCooldownTime;

	// 時間経過シフトの残り時間で直後にもう1段上がらないよう、次のシフトまでの時間を張り直す（ShiftDownGear と同じ流儀）
	const float ShiftHoldTime = bIsShortenedChargeActive ? PlayerParams->PostActionMaxChargeTime : PlayerParams->ChargeV2ShiftHoldTime;
	ChargeShiftTimer.Set( ShiftHoldTime );
}

void UChargeActionPlayerModule_V2::ResetDriftGearUpState()
{
	DriftGearUpSustainTimer = 0.0f;
	DriftGearUpCooldownTimer = 0.0f;
	DriftGearUpSustainDirection = EDriftTurnDirection::None;
	LastDriftGearUpDirection = EDriftTurnDirection::None;
}

namespace
{
	// 行は AttackTypeTag + GearLevel で分かれるので、現在のチャージギアに一致する行を使う。
	// 見つからなければ nullptr（＝無ダメージ）
	const FPlayerAttackParameterRow* FindChargeDriftRow( const ATidePlayerCharacter* Player, int32 GearLevel )
	{
		if ( !Player ) return nullptr;
		const UTideCharacterDataAsset* DataAsset = Player->GetCharacterData();
		if ( !DataAsset || !DataAsset->AttackParameterTable ) return nullptr;

		static const FString Context = TEXT( "ChargeDriftAttackParamLookup" );
		TArray<FPlayerAttackParameterRow*> Rows;
		DataAsset->AttackParameterTable->GetAllRows<FPlayerAttackParameterRow>( Context, Rows );
		for ( const FPlayerAttackParameterRow* Row : Rows )
		{
			if ( Row && Row->AttackTypeTag == TAG_AttackType_Player_ChargeDrift && Row->GearLevel == GearLevel )
			{
				return Row;
			}
		}
		return nullptr;
	}
}

void UChargeActionPlayerModule_V2::UpdateDriftAttack( float DeltaTime )
{
	// 火花演出と同じ単一ソース（bIsDriftBoostActive）が有効な間だけ周囲へ攻撃判定を出す
	if ( !bIsDriftBoostActive )
	{
		// ドリフトが切れたら再アーム履歴をクリアし、再突入時は最初から当たるようにする
		if ( DriftAttackLastHitTimes.Num() > 0 ) DriftAttackLastHitTimes.Reset();
		return;
	}

	const UTidePlayerParamDataAsset* PlayerParams = GetPlayerParams();
	if ( !PlayerParams || !PlayerParams->bEnableDriftAttack || !OwnerCharacter ) return;

	UWorld* World = OwnerCharacter->GetWorld();
	if ( !World ) return;

	const float Radius = FMath::Max( 1.0f, PlayerParams->DriftAttackRadius );

	// 行が無ければ何もしない（AttackParameterTable を単一ソースにする）
	const int32 GearIndex = OwnerCharacter->GetCurrentChargeGearIndex();
	const FPlayerAttackParameterRow* AttackRow = FindChargeDriftRow( OwnerCharacter, GearIndex );
	if ( !AttackRow ) return;
	const float Damage = OwnerCharacter->CalculateDamage( AttackRow->DamageMultiplier );

	// 中心は足元（アクター座標からカプセル半径ぶん下げた位置）＋ローカルオフセット。
	// 火花エフェクト（foot_r）とタイミング・位置感を揃える
	const FTransform ActorXf = OwnerCharacter->GetActorTransform();
	float CapsuleHalfHeight = 0.0f;
	if ( const UCapsuleComponent* Capsule = OwnerCharacter->GetCapsuleComponent() )
	{
		CapsuleHalfHeight = Capsule->GetScaledCapsuleHalfHeight();
	}
	const FVector FeetBase = ActorXf.GetLocation() - FVector( 0.0f, 0.0f, CapsuleHalfHeight );
	const FVector Center = FeetBase + ActorXf.TransformVectorNoScale( PlayerParams->DriftAttackOffset );

	// 短い縦スイープで範囲内アクターを集める（AllDynamicObjects なので地形は拾わない）
	FCollisionObjectQueryParams ObjQuery = FCollisionObjectQueryParams::AllDynamicObjects;
	FCollisionQueryParams QueryParams( SCENE_QUERY_STAT( ChargeDriftAttack ), false, OwnerCharacter );

	const FCollisionShape Shape = FCollisionShape::MakeSphere( Radius );
	const FVector SweepStart = Center - FVector( 0.0f, 0.0f, 1.0f );
	const FVector SweepEnd   = Center + FVector( 0.0f, 0.0f, 1.0f );

#if !UE_BUILD_SHIPPING
	// AnimNotifyState_CommonAttack と同じフラグで可視化する
	if ( const UTideGameSettings* GameSettings = UTideGameSettings::Get() )
	{
		if ( GameSettings->bDebugDrawAttackHitbox )
		{
			DrawDebugSphere( World, Center, Radius, 16, FColor::Red, false, -1.0f, 0, 1.5f );
		}
	}
#endif

	TArray<FHitResult> Hits;
	World->SweepMultiByObjectType( Hits, SweepStart, SweepEnd, FQuat::Identity, ObjQuery, Shape, QueryParams );

	const double Now = World->GetTimeSeconds();
	const float ReArm = FMath::Max( 0.0f, PlayerParams->DriftAttackReArmInterval );

	// 同一アクターに複数コンポーネントがヒットしても 1 回に絞る
	TSet<AActor*> Processed;
	for ( const FHitResult& Hit : Hits )
	{
		AActor* Other = Hit.GetActor();
		if ( !Other || Other == OwnerCharacter ) continue;
		if ( Processed.Contains( Other ) ) continue;
		if ( !TideCombatUtil::IsHostileTo( OwnerCharacter, Other ) ) continue;

		IDamageable* Damageable = Cast<IDamageable>( Other );
		if ( !Damageable ) continue;

		Processed.Add( Other );

		// 再アーム間隔内なら今回はスキップ（多段ヒット防止）
		if ( const double* Last = DriftAttackLastHitTimes.Find( Other ) )
		{
			if ( Now - *Last < ReArm ) continue;
		}
		DriftAttackLastHitTimes.Add( Other, Now );

		// 空中の相手には空中用リアクション、無ければ地上用を使う（OnModifyDamageInfo と同じ解決）
		const ACharacter* TargetChar = Cast<ACharacter>( Other );
		const bool bTargetAirborne = TargetChar && TargetChar->GetCharacterMovement() && TargetChar->GetCharacterMovement()->IsFalling();
		const FGameplayTag ReactionTag = ( bTargetAirborne && AttackRow->EnemyAirReactionTag.IsValid() )
			? AttackRow->EnemyAirReactionTag
			: AttackRow->EnemyReactionTag;

		FDamageInfo Info;
		Info.BaseDamage     = Damage;
		Info.StaggerDamage  = AttackRow->StaggerDamage;
		Info.Instigator     = OwnerCharacter;
		Info.AttackTypeTag  = TAG_AttackType_Player_ChargeDrift;
		Info.HitReactionTag = ReactionTag;
		Info.HitResult      = Hit;

		Damageable->ReceiveDamage( Info );
	}
}

float UChargeActionPlayerModule_V2::GetDynamicChargeMultiplier() const
{
	const UTidePlayerParamDataAsset* PlayerParams = GetPlayerParams();
	if ( !PlayerParams ) return 1.0f;

	// ヒットキャンセルチャージ直後の制限窓ではボーナスを無視する。
	// 後退している勢いでスピード／ドリフトボーナスが乗り、ギアや充電が不当に加速するのを防ぐ
	if ( !HitCancelChargingSteeringTimer.IsFinish() )
	{
		return 1.0f;
	}

	// 【新仕様】スピード＆ドリフト（スピン）ボーナス
	if ( PlayerParams->bEnableSpeedAndDriftChargeBonus )
	{
		float Multiplier = 1.0f;
		UCharacterMovementComponent* Movement = GetCharacterMovement();
		if ( !Movement || !OwnerCharacter ) return Multiplier;

		// スピードボーナス（移動している時のみ加算）
		if ( PlayerParams->bEnableChargeSpeedBonus )
		{
			const float SpeedAlpha = FMath::GetMappedRangeValueClamped(
				FVector2D( PlayerParams->MinSpeedForChargeBonus, PlayerParams->MaxSpeedForChargeBonus ),
				FVector2D( 0.0f, 1.0f ),
				Movement->Velocity.Size2D()
			);
			Multiplier += SpeedAlpha * PlayerParams->MaxSpeedChargeBonusRate;
		}

		// ドリフトボーナス：UpdateDriftBoostState のゲート判定を使い、演出と完全一致させる。
		// 固定値モードは角度由来の強度を掛けず常に満額。ギア直上昇モードは見返りがギア1段そのものなので
		// ここでの溜め速度加算はしない（スピードボーナスは従来どおり効く）
		if ( bIsDriftBoostActive && !PlayerParams->bUseDriftGearUpBonus )
		{
			const float DriftScale = PlayerParams->bUseFixedDriftChargeBonus ? 1.0f : CurrentDriftBoostAlpha;
			Multiplier += DriftScale * PlayerParams->DriftChargeBonusRate;
		}
		return Multiplier;
	}
	else
	{
		// 【旧仕様】坂道ボーナスを使用
		return GetSlopeChargeTimeMultiplier();
	}
}

bool UChargeActionPlayerModule_V2::IsEffectivelyInAir() const
{
	if ( !OwnerCharacter || !GetCharacterMovement() ) return false;

	if ( !OwnerCharacter->IsFalling() ) return false;
	if ( CurrentChargeActionType == EChargeActionV2Type::Jump ) return true;

	constexpr float JumpZThreshold = 100.0f;
	constexpr float FallZThreshold = -400.0f;
	constexpr float GroundTraceDistance = 130.0f; // カプセル半高 + 許容する段差の深さ

	const float ZVel = OwnerCharacter->GetVelocity().Z;

	// 明確に上昇／下降していれば即座に空中とみなす
	if ( ZVel > JumpZThreshold || ZVel < FallZThreshold )
	{
		DrawAirCheckDebug( true, ZVel, FVector::ZeroVector, FVector::ZeroVector, false );
		return true;
	}

	// 真下に地面があればアニメーション的には地上扱い
	const FVector StartLoc = OwnerCharacter->GetActorLocation();
	const FVector EndLoc = StartLoc - FVector( 0.0f, 0.0f, GroundTraceDistance );

	FHitResult Hit;
	FCollisionQueryParams Params( SCENE_QUERY_STAT( FloatBugCheck ), false, OwnerCharacter );
	const bool bGroundHit = OwnerCharacter->GetWorld()->LineTraceSingleByChannel( Hit, StartLoc, EndLoc, ECC_Visibility, Params );

	const bool bResultInAir = !bGroundHit;
	DrawAirCheckDebug( bResultInAir, ZVel, StartLoc, bGroundHit ? Hit.ImpactPoint : EndLoc, bGroundHit );
	return bResultInAir;
}

void UChargeActionPlayerModule_V2::DrawAirCheckDebug( bool bInAir, float ZVel, const FVector& TraceStart, const FVector& TraceEnd, bool bGroundHit ) const
{
#if !UE_BUILD_SHIPPING
	// 浮きバグ調査用。必要なときだけ true にして使う
	constexpr bool bDrawDebug = false;
	if ( !bDrawDebug || !OwnerCharacter ) return;

	// 始点・終点が指定されている＝トレース判定を経た場合のみ描画する
	if ( !TraceStart.IsZero() || !TraceEnd.IsZero() )
	{
		const FColor LineColor = bGroundHit ? FColor::Green : FColor::Red;
		DrawDebugLine( OwnerCharacter->GetWorld(), TraceStart, TraceEnd, LineColor, false, 0.0f, 0, 2.0f );
		if ( bGroundHit )
		{
			DrawDebugBox( OwnerCharacter->GetWorld(), TraceEnd, FVector( 4.0f ), FColor::Green, false, 0.0f, 0, 2.0f );
		}
	}

	if ( GEngine )
	{
		const FColor MsgColor = bInAir ? FColor::Red : FColor::Green;
		const FString Msg = FString::Printf( TEXT( "[Air Check] Result: %s | ZVel: %.1f" ),
			bInAir ? TEXT( "AIR" ) : TEXT( "GROUND" ), ZVel );
		// メッセージIDを固定して毎フレーム上書きする
		GEngine->AddOnScreenDebugMessage( 1001, 0.0f, MsgColor, Msg );
	}
#endif
}

void UChargeActionPlayerModule_V2::UpdateGearShiftCamera()
{
	UExCameraSubsystem* CameraSubsystem = GetCameraSubsystem();
	if ( !CameraSubsystem ) return;

	// 前の段階のカメラスタックを解除して張り替える
	if ( GearShiftCameraHandle.IsValid() )
	{
		CameraSubsystem->PopCameraMode( GearShiftCameraHandle );
		GearShiftCameraHandle.Clear();
	}

	if( OwnerCharacter && OwnerCharacter->IsLockOnActive() )
	{
		return;
	}
	if ( CurrentChargeGearIndex >= 1 )
	{
		// "ChargeGear2" / "ChargeGear3" / "ChargeGear4"
		const FString CameraKey = FString::Printf( TEXT( "ChargeGear%d" ), CurrentChargeGearIndex );

		GearShiftCameraHandle = CameraSubsystem->PushThirdPersonCameraByKey( *CameraKey, TEXT( "PushChargeGear" ) );
	}
}

FName UChargeActionPlayerModule_V2::GetChargeAttackAnimTag() const
{
	const FName PickedTag = [this]() -> FName
	{
		if ( bIsAirChargeAttack || bIsAirNormalAttack )
		{
			// 空中ダイブ攻撃（縦＝振り下ろし／横＝空中チャージ攻撃）は共通ヘルパでタグ解決する
			return GetAirDiveAttackStartTag();
		}

		switch ( CurrentChargeComboIndex )
		{
		case 1: return PlayerAnimTags::CHARGE_01_ATK;
		case 2: return PlayerAnimTags::CHARGE_02_ATK;
		case 3: return PlayerAnimTags::CHARGE_03_ATK;
		case 4: return PlayerAnimTags::CHARGE_04_ATK;
		}
		return PlayerAnimTags::CHARGE_01_ATK;
	}();

	RecordChargeAttackMontagePickLog( PickedTag );	// 計測のみ
	return PickedTag;
}

void UChargeActionPlayerModule_V2::RecordChargeAttackMontagePickLog( const FName& PickedTag ) const
{
	FChargeAttackMontagePickLog Log;
	Log.Tag = PickedTag;
	Log.bAirFlag = bIsAirChargeAttack;
	Log.bNormalDive = bIsAirNormalAttack;
	if ( OwnerCharacter )
	{
		Log.bFalling = OwnerCharacter->IsFalling();
		if ( LastLandedTimeSeconds >= 0.0f && OwnerCharacter->GetWorld() )
		{
			Log.TimeSinceLanded = OwnerCharacter->GetWorld()->GetTimeSeconds() - LastLandedTimeSeconds;
		}
	}

	ChargeAttackMontagePickLogs.Insert( Log, 0 );
	if ( ChargeAttackMontagePickLogs.Num() > ChargeAttackMontagePickLogMax )
	{
		ChargeAttackMontagePickLogs.SetNum( ChargeAttackMontagePickLogMax );
	}
}

FName UChargeActionPlayerModule_V2::GetChargeDashStartAnimTag() const
{
	// 空中チャージダッシュはギアに依らずダッシュ本体の1本（ST 終了で通常落下へ）
	if ( bIsAirChargeDash )
	{
		return PlayerAnimTags::AIRCHARGE_DASH_ST;
	}

	switch ( CurrentChargeGearIndex )
	{
	case 1: return PlayerAnimTags::CHARGE_01_DASH_ST;
	case 2: return PlayerAnimTags::CHARGE_02_DASH_ST;
	case 3: return PlayerAnimTags::CHARGE_03_DASH_ST;
	case 4: return PlayerAnimTags::CHARGE_04_DASH_ST;
	}
	return PlayerAnimTags::CHARGE_01_DASH_ST;
}

FName UChargeActionPlayerModule_V2::GetChargeDashEndAnimTag() const
{
	// 空中チャージダッシュは ED を持たない（着地は JumpActionPlayerModule の JUMP_ED）。地上のみ ED
	return bIsAirChargeDash ? NAME_None : PlayerAnimTags::CHARGE_DASH_ED;
}

void UChargeActionPlayerModule_V2::DrawDebugImGui()
{
	if ( !ImGui::CollapsingHeader( "Charge V2 Module", ImGuiTreeNodeFlags_DefaultOpen ) ) return;

	ImGui::Indent();

	// 共通の描画ヘルパーラムダ（行のガタつき防止）
	auto DrawTimerRow = []( const char* Label, const FAutomaticTimer& Timer, const ImVec4& ActiveColor = ImVec4( 1, 1, 1, 1 ) )
		{
			if ( !Timer.IsFinish() )
			{
				ImGui::TextColored( ActiveColor, "%s: %.2f / %.2f", Label, Timer.GetElapsed(), Timer.GetStart() );
			}
			else
			{
				// 非アクティブ時はグレーアウトして行を維持
				ImGui::TextColored( ImVec4( 0.4f, 0.4f, 0.4f, 1.0f ), "%s: ---", Label );
			}
		};

	// --- 入力と基本ステート ---
	ImGui::TextColored( bChargeInputHeld ? ImVec4( 0, 1, 0, 1 ) : ImVec4( 0.4f, 0.4f, 0.4f, 1.0f ), "Input Held: %s", bChargeInputHeld ? "True" : "False" );
	ImGui::TextColored( bIsCharging ? ImVec4( 1, 1, 0, 1 ) : ImVec4( 0.4f, 0.4f, 0.4f, 1.0f ), "Is Charging: %s", bIsCharging ? "True" : "False" );

	// 溜め直しに R2 の押し直しが要るモード（攻撃後・被弾後の自動再開を止める検証機能）の状態
	{
		const UTidePlayerParamDataAsset* RepressParams = GetPlayerParams();
		const bool bRepressMode = RepressParams && RepressParams->bRequireChargeRepressToRestart;
		const bool bEventRepressMode = RepressParams && RepressParams->bRequireChargeRepressAfterComboFinishAndDamage;
		const bool bRepressWaiting = IsAutoChargeRestartBlocked();
		ImGui::TextColored( bRepressWaiting ? ImVec4( 1, 0.3f, 0.3f, 1 ) : ImVec4( 0.4f, 0.4f, 0.4f, 1.0f ),
			"押し直し要求: %s（待機中: %s / R2 物理押下: %s）",
			bRepressMode ? "ON" : "OFF",
			bRepressWaiting ? "Yes" : "No",
			bChargeInputPhysicallyHeld ? "Held" : "Released" );
		// 部分適用（最終段・被弾のみ）。上のモードが ON ならそちらに吸収される
		ImGui::TextColored( bRequireChargeRepressByEvent ? ImVec4( 1, 0.3f, 0.3f, 1 ) : ImVec4( 0.4f, 0.4f, 0.4f, 1.0f ),
			"  最終段/被弾のみ: %s（待機中: %s）",
			bEventRepressMode ? "ON" : "OFF",
			bRequireChargeRepressByEvent ? "Yes" : "No" );
		if ( IsChargeHoldStale() )
		{
			// 長押しラッチを落としているので Input Held は False になる（＝離している扱い）
			ImGui::TextColored( ImVec4( 1, 0.3f, 0.3f, 1 ), "  長押し無効中（チャージ攻撃／チャージジャンプも出さない）" );
		}
	}

	// ジャンプ後の空中でチャージジャンプを封印しているか（DA: bBlockChargeJumpAfterNormalJump）
	{
		const UTidePlayerParamDataAsset* JumpBlockParams = GetPlayerParams();
		const bool bBlockMode = JumpBlockParams && JumpBlockParams->bBlockChargeJumpAfterNormalJump;
		const bool bBlocking = IsChargeJumpBlockedAfterJump();
		ImGui::TextColored( bBlocking ? ImVec4( 1, 0.3f, 0.3f, 1 ) : ImVec4( 0.4f, 0.4f, 0.4f, 1.0f ),
			"ジャンプ後CJ封印: %s（封印中: %s / ジャンプ済み: %s）",
			bBlockMode ? "ON" : "OFF",
			bBlocking ? "Yes" : "No",
			( OwnerCharacter && OwnerCharacter->HasJumpedThisAirtime() ) ? "Yes" : "No" );
	}

	// --- 空中チャージダッシュ（回数。ダッシュが出ない時はここを見る。滑空は Glide Module 側）---
	{
		const ImVec4 OkColor( 0.0f, 1.0f, 0.0f, 1.0f );
		const ImVec4 NgColor( 1.0f, 0.3f, 0.3f, 1.0f );
		const ImVec4 GrayColor( 0.5f, 0.5f, 0.5f, 1.0f );

		ImGui::SeparatorText( "AirChargeDash" );

		const int32 MaxCount = GetPlayerParams() ? FMath::Max( 1, GetPlayerParams()->MaxAirChargeDashCount ) : 1;
		ImGui::Text( "回数: %d / %d", AirChargeDashStageCount, MaxCount );

		const bool bCanStartAirDash = CanStartAirChargeDashNow();
		ImGui::TextColored( bCanStartAirDash ? OkColor : NgColor, "CanStartAirChargeDashNow: %s", bCanStartAirDash ? "True" : "False" );
		ImGui::Text( "空中判定: %s   空中ダッシュ中: %s",
			IsEffectivelyInAir() ? "Air" : "Ground",
			IsAirChargeDashing() ? "Yes" : "No" );

		// 回数を使い切った／空中チャージ攻撃を出した空中は攻撃・ジャンプ（滑空）・神技のみ（チャージ／回避／ダッシュを封印）
		const bool bAirActionLimited = OwnerCharacter && OwnerCharacter->IsAirActionLimitedAfterAirCharge();
		ImGui::TextColored( bAirActionLimited ? NgColor : GrayColor, "空中アクション制限: %s（要因: ダッシュ使い切り=%s / 空中チャージ攻撃=%s）",
			bAirActionLimited ? "ON（攻撃/ジャンプ/神技のみ）" : "OFF",
			IsAirChargeDashExhausted() ? "Yes" : "No",
			bAirChargeAttackUsedThisAirtime ? "Yes" : "No" );

		// 溜め開始のゲート（R2 を押しても溜めが始まらない場合はここが False）
		if ( OwnerCharacter )
		{
			const bool bHasCanCharge = OwnerCharacter->HasStateTag( TAG_State_Player_CanCharge );
			const bool bCanBeginCharge = CanBeginChargeFromCurrentState();
			ImGui::TextColored( bHasCanCharge ? OkColor : NgColor, "CanCharge タグ: %s", bHasCanCharge ? "有" : "無" );
			ImGui::TextColored( bCanBeginCharge ? OkColor : NgColor, "CanBeginChargeFromCurrentState: %s", bCanBeginCharge ? "True" : "False" );
		}

		if ( const UTidePlayerParamDataAsset* DebugParams = GetPlayerParams() )
		{
			ImGui::TextColored( GrayColor, "param: 最大回数=%d", DebugParams->MaxAirChargeDashCount );
		}
	}

	// --- 空中チャージ攻撃の突進（LP）。「いつまで突っ込むか」が想定と違う時はここを見る ---
	{
		const ImVec4 OkColor( 0.0f, 1.0f, 0.0f, 1.0f );
		const ImVec4 NgColor( 1.0f, 0.3f, 0.3f, 1.0f );
		const ImVec4 GrayColor( 0.5f, 0.5f, 0.5f, 1.0f );

		ImGui::SeparatorText( "空中チャージ攻撃（ダイブ）" );

		const bool bDiveActive = IsRevampAirDiveAttack();
		ImGui::TextColored( bDiveActive ? OkColor : GrayColor, "突進中: %s   フェーズ: %s   モード: %s",
			bDiveActive ? "Yes" : "No",
			bIsAirChargeAttackInLoop ? "LP（突進）" : "ST（滞空）",
			bIsAirChargeAttackLockOnRush ? "ロック突進" : "角度ダイブ" );

		const bool bActionLocked = IsAirChargeAttackActionLocked();
		ImGui::TextColored( bActionLocked ? NgColor : GrayColor, "行動封印: %s",
			bActionLocked ? "Yes（再発動・回避不可）" : "No" );

		const bool bDiveAttackCoolingDown = !AirChargeAttackDiveAttackCooldownTimer.IsFinish();
		ImGui::TextColored( bDiveAttackCoolingDown ? NgColor : GrayColor, "振り下ろし待ち: %s",
			bDiveAttackCoolingDown ? "Yes" : "No" );

		if ( AirChargeAttackDiveTimer.IsValid() )
		{
			ImGui::Text( "持続: %.2f / %.2f", AirChargeAttackDiveTimer.GetElapsed(), AirChargeAttackDiveTimer.GetStart() );
		}
		else
		{
			ImGui::TextColored( GrayColor, "持続: ---（無制限＝着地・壁ヒットまで）" );
		}

		if ( bAirChargeAttackDiveEnded || bAirChargeAttackDiveWallStuck )
		{
			ImGui::TextColored( NgColor, "突進終了: %s（通常落下中）",
				bAirChargeAttackDiveWallStuck ? "壁ヒット" : "ヒット/持続時間満了" );
		}

		// 満了しても「止まって見えない」ときはここを見る。落下中の減速はスティックを倒している間は効かないため、
		// 水平速度が残っていれば突進は続いて見える（AirChargeAttackDiveEndSpeedRate で落とす）
		if ( const UCharacterMovementComponent* DiveMovement = GetCharacterMovement() )
		{
			ImGui::Text( "速度: 水平=%.0f / 垂直=%.0f", DiveMovement->Velocity.Size2D(), DiveMovement->Velocity.Z );
		}

		// 発動判定（SetupAirChargeAttackState）は IsFalling() 一本。ED 中にここが Falling なら地上に見えていても空中扱い
		if ( OwnerCharacter )
		{
			const bool bDiveFalling = OwnerCharacter->IsFalling();
			ImGui::TextColored( bDiveFalling ? NgColor : GrayColor, "発動判定 IsFalling: %s   IsEffectivelyInAir: %s   1滞空1回: %s   自前射出の浮き: %s",
				bDiveFalling ? "Falling（空中扱い）" : "接地",
				IsEffectivelyInAir() ? "Air" : "Ground",
				bAirChargeAttackUsedThisAirtime ? "使用済み" : "未使用",
				bAirborneByGroundChargeLaunch ? "Yes" : "No" );
		}

		if ( const UTidePlayerParamDataAsset* DiveParams = GetPlayerParams() )
		{
			ImGui::TextColored( GrayColor, "param: 滞空=%.2f / 角度ダイブ持続=%.2f（0=無制限） / ロック突進持続=%.2f（0=無制限）",
				DiveParams->AirChargeAttackHoverTime,
				DiveParams->AirChargeAttackDiveMaxTime,
				DiveParams->AirChargeAttackLockOnRushMaxTime );
			ImGui::TextColored( GrayColor, "        突進速度=%.0f / 満了後に残す水平割合=%.2f（0=止める）",
				DiveParams->AirChargeAttackDiveSpeed,
				DiveParams->AirChargeAttackDiveEndSpeedRate );
		}

		// 突進中は実機で数値を追えないので、着地後にここを読む。
		// 「終了=着地」なら持続時間が効いていない／「終了=持続時間満了」なのに着地速度が高いなら慣性が残っている
		if ( AirChargeAttackDiveLogs.Num() > 0 )
		{
			ImGui::TextColored( GrayColor, "  突進ログ（新しい順）" );
			for ( const FAirChargeAttackDiveLog& Log : AirChargeAttackDiveLogs )
			{
				const bool bEndedByTime = ( FCString::Strcmp( Log.EndReason, TEXT( "持続時間満了" ) ) == 0 );
				ImGui::TextColored( bEndedByTime ? OkColor : NgColor,
					"  [%s] LP=%.2f秒 / 上限=%.2f / 終了=%s",
					Log.bLockOnRush ? "ロック" : "角度",
					Log.LoopTime,
					Log.MaxTime,
					TCHAR_TO_UTF8( Log.EndReason ) );
				ImGui::TextColored( GrayColor, "        終了時 水平=%.0f（×%.2f 適用後） → 着地時 水平=%.0f",
					Log.EndSpeed2D, Log.EndSpeedRate, Log.LandSpeed2D );
			}
		}
		else
		{
			ImGui::TextColored( GrayColor, "  突進ログ: ---（空中チャージ攻撃を出すと記録される）" );
		}

		// --- 発動判定ログ：チャージ攻撃を出した瞬間の「空中／地上」とその根拠（新しい順）---
		// 着地ED 中に空中チャージ攻撃が出るときは、その行が [空中] かつ 直前=AirChargeAttackEnd になる。
		// IsFalling が Yes なら「本当に浮いている」、No なのに [空中] ならこの判定以外の経路が犯人
		ImGui::Spacing();
		if ( ChargeAttackTriggerLogs.Num() > 0 )
		{
			ImGui::TextColored( GrayColor, "  発動判定ログ（新しい順）" );
			for ( const FChargeAttackTriggerLog& Log : ChargeAttackTriggerLogs )
			{
				// 画面幅で切れないよう 3 行に分ける（1行目＝結果と経路／2行目＝空中判定の材料／3行目＝フラグとタグ）
				const FString PrevMontageName = Log.PrevMontage.IsNone() ? FString( TEXT( "---" ) ) : Log.PrevMontage.ToString();
				ImGui::TextColored( Log.bResultAir ? NgColor : OkColor,
					"  [%s] %s / R2=%s / 直前=%s%s",
					Log.bResultAir ? "空中" : "地上",
					TCHAR_TO_UTF8( Log.Source ),
					Log.bChargeHeld ? "保持" : "離し",
					TCHAR_TO_UTF8( *PrevMontageName ),
					Log.bDuringAirDiveEd ? "（着地ED中）" : "" );

				const FString ModeName = Log.MovementMode.IsNone() ? FString( TEXT( "---" ) ) : Log.MovementMode.ToString();
				ImGui::TextColored( GrayColor, "     IsFalling=%s EffInAir=%s Mode=%s 真下地面=%.0f Vz=%.0f 水平=%.0f",
					Log.bFalling ? "Yes" : "No",
					Log.bEffectivelyInAir ? "Yes" : "No",
					TCHAR_TO_UTF8( *ModeName ),
					Log.GroundDistance,
					Log.VelocityZ, Log.Speed2D );

				// 射出の浮き=Yes かつ 真下地面>=0 なら、地上扱いへ戻す保険が効いている（＝結果が [地上] になる）
				ImGui::TextColored( Log.bLaunchAirborne ? NgColor : GrayColor,
					"     射出の浮き=%s 1滞空1回=%s 着地から=%.2f秒 タグ=%s",
					Log.bLaunchAirborne ? "Yes" : "No",
					Log.bUsedThisAirtime ? "使用済み" : "未使用",
					Log.TimeSinceLanded,
					TCHAR_TO_UTF8( *Log.Tags ) );
			}
		}
		else
		{
			ImGui::TextColored( GrayColor, "  発動判定ログ: ---（チャージ攻撃を出すと記録される）" );
		}

		// --- モーション選択ログ：チャージ攻撃のタグが選ばれた瞬間（誰が張ったかの追跡）---
		// ここに [CHARGE_01_ATK] が出ているのに発動判定ログへ対応行が無ければ、V2 の発動経路を通らずに張られている
		if ( ChargeAttackMontagePickLogs.Num() > 0 )
		{
			ImGui::TextColored( GrayColor, "  モーション選択ログ（新しい順）" );
			for ( const FChargeAttackMontagePickLog& Log : ChargeAttackMontagePickLogs )
			{
				const FString TagName = Log.Tag.IsNone() ? FString( TEXT( "---" ) ) : Log.Tag.ToString();
				ImGui::TextColored( Log.bAirFlag ? NgColor : GrayColor,
					"  %s / 空中フラグ=%s / 振り下ろし=%s / IsFalling=%s / 着地から=%.2f秒",
					TCHAR_TO_UTF8( *TagName ),
					Log.bAirFlag ? "Yes" : "No",
					Log.bNormalDive ? "Yes" : "No",
					Log.bFalling ? "Yes" : "No",
					Log.TimeSinceLanded );
			}
		}

		// --- 浮きログ：接地→落下へ移った瞬間と、そのとき流れていたモンタージュ ---
		// 「溜め開始のモーションで一瞬浮く」ならここに溜め系モンタージュが並ぶ。真下地面が 0 以上なら足元に地面がある浮き
		if ( AirborneTransitionLogs.Num() > 0 )
		{
			ImGui::TextColored( GrayColor, "  浮きログ（接地→落下・新しい順）" );
			for ( const FAirborneTransitionLog& Log : AirborneTransitionLogs )
			{
				const FString MontageName = Log.Montage.IsNone() ? FString( TEXT( "---" ) ) : Log.Montage.ToString();
				ImGui::TextColored( Log.GroundDistance >= 0.0f ? NgColor : GrayColor,
					"  %s / 溜め中=%s / 種別=%s / Vz=%.0f 水平=%.0f / 真下地面=%.0f / 着地から=%.2f秒",
					TCHAR_TO_UTF8( *MontageName ),
					Log.bCharging ? "Yes" : "No",
					TCHAR_TO_UTF8( Log.ActionType ),
					Log.VelocityZ, Log.Speed2D,
					Log.GroundDistance,
					Log.TimeSinceLanded );
			}
		}

		// --- 着地ED 中に再び浮いたか（浮けば以降の発動判定は空中扱いになる）---
		if ( AirChargeAttackEdAirborneLogs.Num() > 0 )
		{
			ImGui::TextColored( NgColor, "  着地ED 中の浮きログ（新しい順）" );
			for ( const FAirChargeAttackEdAirborneLog& Log : AirChargeAttackEdAirborneLogs )
			{
				ImGui::TextColored( GrayColor, "  ED %.2f秒目で浮いた / 着地から=%.2f秒 / 水平=%.0f 垂直=%.0f",
					Log.EdPosition, Log.TimeSinceLanded, Log.Speed2D, Log.VelocityZ );
			}
		}
		else
		{
			ImGui::TextColored( GrayColor, "  着地ED 中の浮きログ: ---（ED 中は接地したまま）" );
		}
	}

	// --- 幅跳び（チャージホップ）の水平速度維持と速度収支。「跳ぶたびに慣性が落ちる」の切り分け用（CMC ロスが大きい区間が犯人）---
	{
		const ImVec4 OkColor( 0.0f, 1.0f, 0.0f, 1.0f );
		const ImVec4 NgColor( 1.0f, 0.3f, 0.3f, 1.0f );
		const ImVec4 GrayColor( 0.5f, 0.5f, 0.5f, 1.0f );

		ImGui::SeparatorText( "幅跳び（Hop）速度" );

		const UTidePlayerParamDataAsset* HopParams = GetPlayerParams();
		const float CurrentSpeed2D = ( OwnerCharacter && OwnerCharacter->GetCharacterMovement() )
			? OwnerCharacter->GetCharacterMovement()->Velocity.Size2D() : 0.0f;

		ImGui::TextColored( bChargeHopSpeedMaintainActive ? OkColor : GrayColor, "幅跳び中（速度維持）: %s",
			bChargeHopSpeedMaintainActive ? "Yes" : "No" );

		if ( bChargeHopSpeedMaintainActive )
		{
			ImGui::Text( "  維持目標: %.0f / 現在: %.0f  (%s)",
				ChargeHopMaintainSpeed, CurrentSpeed2D,
				( OwnerCharacter && OwnerCharacter->IsFalling() ) ? "滞空" : "接地" );
			if ( bChargeHopBlockedBySolid )
			{
				ImGui::TextColored( NgColor, "  壁ヒット判定：維持を停止中（押し付け防止）" );
			}
		}
		else
		{
			ImGui::TextColored( GrayColor, "  維持目標: --- / 現在: %.0f", CurrentSpeed2D );
		}

		if ( HopParams )
		{
			ImGui::TextColored( GrayColor, "  param: [ギア%d] 維持速度=%.0f（0=初速維持） / 上限=%.0f（0=無制限） / 減衰=%.0f / 着地中も維持=%s",
				FMath::Max( 1, CurrentChargeGearIndex ),
				GetChargeHopMoveSpeedForGear( CurrentChargeGearIndex ),
				GetChargeHopMaxMoveSpeedForGear( CurrentChargeGearIndex ),
				HopParams->ChargeHopSpeedDecayPerSec,
				HopParams->bChargeHopMaintainDuringLanding ? "ON" : "OFF" );

			// 連打対策の状態。上り坂で回数が伸びるときは「ダッシュ残り」の減り方（＝1ホップの消費）を見る
			const bool bChainLimited = HopParams->ChargeHopMaxChainCount > 0 && ChargeHopChainCount >= HopParams->ChargeHopMaxChainCount;
			ImGui::TextColored( bChainLimited ? NgColor : GrayColor,
				"  連続回数: %d / 上限 %d（0=無制限） / 1ホップのコスト %.2fs / ダッシュ残り %.2fs",
				ChargeHopChainCount, HopParams->ChargeHopMaxChainCount,
				HopParams->ChargeHopTimeCostPerHop, GetChargeDashRemainingTime() );

			// 坂沿い射出。床角度が出ていて傾け量が 0 なら、強さ／上限角で抑えられている
			const float FloorAngleDeg = OwnerCharacter
				? FMath::RadiansToDegrees( FMath::Acos( FMath::Clamp( OwnerCharacter->GetSmoothedFloorNormal().GetSafeNormal().Z, -1.0f, 1.0f ) ) )
				: 0.0f;
			ImGui::TextColored( HopParams->bEnableChargeHopSlopeAlign ? OkColor : GrayColor,
				"  坂沿い射出: %s（強さ %.2f / 上限 %.0f度 / 上りのみ %s） 床角度 %.1f度",
				HopParams->bEnableChargeHopSlopeAlign ? "ON" : "OFF",
				HopParams->ChargeHopSlopeAlignRate, HopParams->ChargeHopSlopeAlignMaxAngleDeg,
				HopParams->bChargeHopSlopeAlignUphillOnly ? "ON" : "OFF", FloorAngleDeg );

			// カメラの遅延 Pop（着地後にもう一度跳ぶ猶予）。予約中に次のホップ／ダッシュが来れば維持される
			ImGui::TextColored( bPendingChargeActionCameraPop ? OkColor : GrayColor,
				"  カメラ遅延Pop: %s（残り %.2fs / 設定 %.2fs）",
				bPendingChargeActionCameraPop ? "予約中" : "---",
				ChargeActionCameraHoldTimer.Get(), HopParams->ChargeHopCameraPopDelayTime );
		}

		// 速度収支ログ（最新が上）。引継→発射→滞空→着地→ED→終了 の順に、どこで何 cm/s 失ったか
		if ( ChargeHopSpeedLogs.Num() > 0 )
		{
			ImGui::TextColored( GrayColor, "  収支ログ（新しい順・CMC=エンジンが食った量／自前=維持で戻した量）" );
			for ( const FChargeHopSpeedLog& Log : ChargeHopSpeedLogs )
			{
				const float NetDelta = Log.EndSpeed - Log.PreLaunchSpeed;	// 1ホップでの正味の増減（負＝落ちた）
				ImGui::TextColored( NetDelta < 0.0f ? NgColor : OkColor,
					"  #%d 引継%.0f → 発射%.0f → 着地%.0f → 終了%.0f（正味 %+.0f）",
					Log.Index, Log.PreLaunchSpeed, Log.LaunchSpeed, Log.LandingSpeed, Log.EndSpeed, NetDelta );
				ImGui::TextColored( GrayColor,
					"      滞空 %.2fs: CMC -%.0f / 自前 +%.0f   接地 %.2fs: CMC -%.0f / 自前 +%.0f",
					Log.AirTime, Log.AirCmcLoss, Log.AirSelfGain,
					Log.GroundTime, Log.GroundCmcLoss, Log.GroundSelfGain );
			}
		}
		else
		{
			ImGui::TextColored( GrayColor, "  収支ログ: ---（幅跳びを出すと記録される）" );
		}
	}

	if ( bIsCharging )
	{
		ImGui::TextColored( bIsShortenedChargeActive ? ImVec4( 0, 1, 1, 1 ) : ImVec4( 1, 1, 1, 1 ), "  Type: %s", bIsShortenedChargeActive ? u8"継続 (短縮)" : u8"通常" );
		ImGui::Text( "  Timer: %.2f / %.2f", CurrentChargeTimer.GetElapsed(), CurrentChargeTimer.GetStart() );
		if ( bHasReachedMaxCharge )
		{
			ImGui::SameLine();
			ImGui::TextColored( ImVec4( 1, 0, 0, 1 ), "[MAX]" );
		}
	}
	else
	{
		// 非チャージ時も行を確保してガタつきを防ぐ
		ImGui::TextColored( ImVec4( 0.4f, 0.4f, 0.4f, 1.0f ), "  Type: ---" );
		ImGui::TextColored( ImVec4( 0.4f, 0.4f, 0.4f, 1.0f ), "  Timer: --- / ---" );
	}

	ImGui::Separator();

	// --- 2. コンボとギア進行 ---
	ImGui::Text( "Combo Index: %d", CurrentChargeComboIndex );
	ImGui::Text( "Gear Index : %d", CurrentChargeGearIndex );

	if ( const UTidePlayerParamDataAsset* GearParams = GetPlayerParams() )
	{
		ImGui::Text( "  手動ギア: 下げ(L1)=%s / R1限定上昇=%s",
			GearParams->bEnableManualGearDown ? "ON" : "OFF",
			GearParams->bManualGearUpOnly ? "ON（自動上昇マスク中）" : "OFF" );

		// ヒットバック由来の溜めの継続。上がらないときは、そもそも継続中になっているかを見る
		ImGui::Text( "  ヒットバック溜め: %s   残り %.2f / %.2f%s",
			IsHitbackChargeHoldActive() ? "継続中" : "---",
			HitbackChargeHoldTimer.Get(), GearParams->HitbackChargeGearUpTime,
			bHitbackChargeGearUpDone ? "（上昇済み）" : "" );
	}

	DrawTimerRow( "Shift Timer", ChargeShiftTimer );

	ImGui::Separator();

	// --- 3. 実行中のアクションステート ---
	const char* ActionTypeName = "None";
	switch ( CurrentChargeActionType )
	{
	case EChargeActionV2Type::Dash:   ActionTypeName = "Dash";   break;
	case EChargeActionV2Type::Attack: ActionTypeName = "Attack"; break;
	case EChargeActionV2Type::Jump:   ActionTypeName = "Jump";   break;
	case EChargeActionV2Type::GuardBrake:   ActionTypeName = "GuardBrake";   break;
	}

	ImGui::TextColored( CurrentChargeActionType != EChargeActionV2Type::None ? ImVec4( 1, 0.5f, 0, 1 ) : ImVec4( 0.4f, 0.4f, 0.4f, 1.0f ), "Action Type: %s", ActionTypeName );

	DrawTimerRow( "  Lock Timer", ChargedActionLockTimer );

	ImGui::Separator();

	// --- 4. 裏で動いている特殊タイマーと判定 ---
	DrawTimerRow( "Post Action Window", PostActionChargeTimer, ImVec4( 0, 1, 1, 1 ) );
	DrawTimerRow( "Gravity Lock", GravityLockTimer );
	DrawTimerRow( "Friction Recovery", FrictionRecoveryTimer );
	DrawTimerRow( "HitCancel ActionLock", HitCancelChargeActionLockTimer );
	// ヒットバック後の連続チャージ攻撃インターバル（R2 を押さない連打時のみブロック）
	DrawTimerRow( "Hitback Combo Interval", HitbackComboIntervalTimer, ImVec4( 1, 0.3f, 0.3f, 1 ) );
	ImGui::TextColored( bPendingChargeComboAttack ? ImVec4( 1, 0.6f, 0.2f, 1 ) : ImVec4( 0.4f, 0.4f, 0.4f, 1.0f ),
		"  Pending Combo Atk: %s", bPendingChargeComboAttack ? "True" : "False" );

	// 厳密な空中判定の結果
	const bool bInAir = IsEffectivelyInAir();
	ImGui::TextColored( bInAir ? ImVec4( 1, 0.5f, 0, 1 ) : ImVec4( 0, 1, 0, 1 ), "Air State: %s", bInAir ? "AIR" : "GROUND" );

	// --- 5. ドリフト（連続ドリフトが続かない時はここを見る）---
	if ( OwnerCharacter )
	{
		const ImVec4 OkColor( 0.0f, 1.0f, 0.0f, 1.0f );
		const ImVec4 NgColor( 1.0f, 0.3f, 0.3f, 1.0f );
		const ImVec4 GrayColor( 0.5f, 0.5f, 0.5f, 1.0f );

		ImGui::SeparatorText( "Drift" );

		const UTidePlayerParamDataAsset* DebugParams = GetPlayerParams();
		const UCharacterMovementComponent* DebugMovement = GetCharacterMovement();
		const float SustainTime = DebugParams ? DebugParams->DriftBoostSustainTime : 0.0f;
		const float MinSpeed = DebugParams ? DebugParams->MinSpeedForChargeBonus : 0.0f;
		const float CurrentSpeed = DebugMovement ? DebugMovement->Velocity.Size2D() : 0.0f;

		ImGui::TextColored( bIsRawDrifting ? OkColor : GrayColor, "ドリフト条件: %s   継続: %.2f / %.2f",
			bIsRawDrifting ? "満たす" : "---", DriftSustainTimer, SustainTime );
		ImGui::TextColored( bIsDriftBoostActive ? OkColor : GrayColor, "ブースト: %s   強度: %.2f",
			bIsDriftBoostActive ? "ON" : "OFF", CurrentDriftBoostAlpha );

		// ギア直上昇モード：継続が伸びない／上がらないときは「方向」と「CD」を見る。
		// CD が残っている間は同方向では上がらず、切り返し（前回上昇と逆方向）で 0 へ落ちる
		{
			auto DirLabel = []( EDriftTurnDirection Dir )
			{
				switch ( Dir )
				{
				case EDriftTurnDirection::Left:  return "左";
				case EDriftTurnDirection::Right: return "右";
				default:                         return "---";
				}
			};

			const bool bGearUpMode = DebugParams && DebugParams->bEnableSpeedAndDriftChargeBonus && DebugParams->bUseDriftGearUpBonus;
			ImGui::TextColored( bGearUpMode ? OkColor : GrayColor, "ギア直上昇: %s   方向: %s   前回上昇: %s",
				bGearUpMode ? "ON" : "OFF", DirLabel( CurrentDriftTurnDirection ), DirLabel( LastDriftGearUpDirection ) );
			if ( bGearUpMode )
			{
				ImGui::TextColored( DriftGearUpCooldownTimer > 0.0f ? NgColor : GrayColor, "  継続: %.2f / %.2f   CD: %.2f / %.2f",
					DriftGearUpSustainTimer, DebugParams->DriftGearUpSustainTime,
					DriftGearUpCooldownTimer, DebugParams->DriftGearUpCooldownTime );
			}
		}

		// 速度がドリフト成立の下限を割ると、角度を保っていてもドリフトが切れる（連続ドリフトが途切れる主因）
		ImGui::TextColored( CurrentSpeed >= MinSpeed ? OkColor : NgColor, "速度: %.0f （必要 %.0f）", CurrentSpeed, MinSpeed );

		// 速度維持：前フレーム出口 → 今フレーム入口の差が CMC（摩擦・ブレーキ）に食われた分、入口 → 出口が自前の補正分。
		// 食われる量が大きいなら DriftGroundFrictionRate を下げる
		const float CmcDelta = OwnerCharacter->GetChargingSpeedIn() - OwnerCharacter->GetChargingSpeedPrevOut();
		const float FixDelta = OwnerCharacter->GetChargingSpeedOut() - OwnerCharacter->GetChargingSpeedIn();
		ImGui::TextColored( OwnerCharacter->IsDriftSpeedMaintaining() ? OkColor : GrayColor, "速度維持: %s   目標: %.0f",
			OwnerCharacter->IsDriftSpeedMaintaining() ? "ON" : "OFF", OwnerCharacter->GetDriftMaintainSpeed() );
		ImGui::TextColored( CmcDelta < 0.0f ? NgColor : GrayColor, "  CMC ロス: %.0f → %.0f (%+.0f)",
			OwnerCharacter->GetChargingSpeedPrevOut(), OwnerCharacter->GetChargingSpeedIn(), CmcDelta );
		ImGui::TextColored( FixDelta > 0.0f ? OkColor : GrayColor, "  自前補正: %.0f → %.0f (%+.0f)",
			OwnerCharacter->GetChargingSpeedIn(), OwnerCharacter->GetChargingSpeedOut(), FixDelta );
		// 坂道倍率は歩行上限（×）と摩擦（÷）に同時に効く。ここが跳ねる瞬間が「急に加速してガクッ」の正体なので、
		// 上限・摩擦と並べて出す（ChargingSlopeMultiplierInterpSpeed で追従を鈍らせられる）
		ImGui::TextColored( GrayColor, "  摩擦: %.2f   歩行上限: %.0f   坂道倍率: %.2f",
			OwnerCharacter->GetChargingAppliedGroundFriction(),
			DebugMovement ? DebugMovement->MaxWalkSpeed : 0.0f,
			OwnerCharacter->GetChargingSlopeMultiplier() );

		// 坂道系パラメータはギア別配列で上書きできる。倍率が想定と違うときは「今のギアで実際に引かれている値」を見る。
		// クランプ下限は倍率を強制するので、下限が高いままだとボーナスを 0 にしても速度が落ちない
		if ( DebugParams )
		{
			ImGui::TextColored( GrayColor, "  [ギア %d] 下り倍率クランプ: %.2f 〜 %.2f   ボーナス: %.2f   速度上限: %.0f",
				CurrentChargeGearIndex,
				OwnerCharacter->GetSlopeParamForGear( DebugParams->MinChargingDownhillMultiplierForGear ),
				OwnerCharacter->GetSlopeParamForGear( DebugParams->MaxChargingDownhillMultiplierForGear ),
				OwnerCharacter->GetSlopeParamForGear( DebugParams->ChargingDownhillBaseBonusForGear ),
				OwnerCharacter->GetSlopeParamForGear( DebugParams->MaxSlideSpeedForGear ) );
			ImGui::TextColored( GrayColor, "  [ギア %d] 上り倍率クランプ: %.2f 〜 %.2f   ペナルティ: %.2f",
				CurrentChargeGearIndex,
				OwnerCharacter->GetSlopeParamForGear( DebugParams->MinChargingUphillMultiplierForGear ),
				OwnerCharacter->GetSlopeParamForGear( DebugParams->MaxChargingUphillMultiplierForGear ),
				OwnerCharacter->GetSlopeParamForGear( DebugParams->ChargingUphillBasePenaltyForGear ) );
		}
	}

	ImGui::Unindent();
}

