// Copyright (c) 2026, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.
#include "GodActionPlayerModule.h"

#include "PRJ_TIDE_P0/Composites/Player/PlayerModules/GodActionSubModule.h"
#include "PRJ_TIDE_P0/Composites/Player/PlayerModules/GodSlashSubModule.h"
#include "PRJ_TIDE_P0/Composites/Player/PlayerModules/GodGuidanceSubModule.h"
#include "PRJ_TIDE_P0/Composites/Player/PlayerModules/GodFrolicSubModule.h"
#include "PRJ_TIDE_P0/Composites/Player/PlayerModules/GodBirdPlayerModule.h"
#include "PRJ_TIDE_P0/Actors/Character/Player/TidePlayerCharacter.h"
#include "PRJ_TIDE_P0/Actors/Companion/TideGodBird.h"
#include "PRJ_TIDE_P0/Core/TideGameSettings.h"
#include "PRJ_TIDE_P0/Data/Player/TidePlayerParamDataAsset.h"
#include "PRJ_TIDE_P0/Data/Character/TideCharacterDataAsset.h"
#include "PRJ_TIDE_P0/Data/Combat/AttackParameterRow.h"
#include "PRJ_TIDE_P0/Data/Tags/TideGameplayTags.h"
#include "PRJ_TIDE_P0/Data/Combat/DamageInfo.h"
#include "PRJ_TIDE_P0/Data/Player/PlayerAnimTags.h"
#include "PRJ_TIDE_P0/Utilities/HitStopUtils.h"

#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"

#if !UE_BUILD_SHIPPING
#include "imgui.h"
#endif

// AttackParameterTable から AttackTypeTag + GearLevel が一致する行を返す。見つからなければ nullptr
static const FPlayerAttackParameterRow* FindPlayerAttackRow( const ATidePlayerCharacter* Player, const FGameplayTag& AttackTypeTag, int32 GearLevel )
{
	if ( !Player ) return nullptr;
	const UTideCharacterDataAsset* DataAsset = Player->GetCharacterData();
	if ( !DataAsset || !DataAsset->AttackParameterTable ) return nullptr;

	static const FString Context = TEXT( "PlayerAttackParamLookup" );
	TArray<FPlayerAttackParameterRow*> Rows;
	DataAsset->AttackParameterTable->GetAllRows<FPlayerAttackParameterRow>( Context, Rows );

	for ( const FPlayerAttackParameterRow* Row : Rows )
	{
		if ( Row && Row->AttackTypeTag == AttackTypeTag && Row->GearLevel == GearLevel )
		{
			return Row;
		}
	}
	return nullptr;
}

void UGodActionPlayerModule::Initialize( ATidePlayerCharacter* InOwner )
{
	Super::Initialize( InOwner );

	Techniques.Reset();

	SlashModule = NewObject<UGodSlashSubModule>( this );
	SlashModule->Initialize( this, InOwner );
	Techniques.Add( SlashModule );

	GuidanceModule = NewObject<UGodGuidanceSubModule>( this );
	GuidanceModule->Initialize( this, InOwner );
	Techniques.Add( GuidanceModule );

	FrolicModule = NewObject<UGodFrolicSubModule>( this );
	FrolicModule->Initialize( this, InOwner );
	Techniques.Add( FrolicModule );
}

void UGodActionPlayerModule::OnModuleUpdate( float DeltaTime )
{
	if ( !OwnerCharacter || !OwnerCharacter->PlayerParamData ) return;

	UpdateGaugeOrbs( DeltaTime );	// 状態に依らず常に進行させる

	for ( const TObjectPtr<UGodActionSubModule>& Technique : Techniques )
	{
		if ( Technique && Technique->IsEnabled() )
		{
			Technique->OnModuleUpdate( DeltaTime );
		}
	}

	if ( bGodArtSelecting && bGodArtStanceMontageActive )
	{
		UpdateGodArtStanceMontage( DeltaTime );
	}

	// 空中構えで飛行中は、ゆっくり降下させつつ、接地したら歩行へ戻す
	if ( bGodArtStanceActive && bStanceFlying && OwnerCharacter )
	{
		if ( UCharacterMovementComponent* Move = OwnerCharacter->GetCharacterMovement() )
		{
			if ( !IsPlayerAirborne() )
			{
				Move->SetMovementMode( MOVE_Walking );
				bStanceFlying = false;
			}
			else
			{
				// 水平速度を消してその場でゆっくり降下させる。Flying の制動に負けないよう毎フレーム直接設定する
				const float FallSpeed = OwnerCharacter->PlayerParamData ? OwnerCharacter->PlayerParamData->GodArtStanceFallSpeed : 150.0f;
				Move->Velocity = FVector( 0.0f, 0.0f, -FallSpeed );
			}
		}
	}

	// 地上で構えている間は突入時の慣性を減衰させ、滑っていくのを抑える
	if ( bGodArtStanceActive && !bStanceFlying )
	{
		UpdateGodArtStanceBraking( DeltaTime );
	}

	// 解除経路の取りこぼし対策
	if ( bGodArtStanceActive && !bGodArtSelecting )
	{
		ExitGodArtStance();
	}

#if !UE_BUILD_SHIPPING
	// 導きを選択中は、突進で通り抜ける範囲を黄緑 Box で可視化する
	if ( IsGodBirdModeEnabled() && bGodArtSelecting && SelectedArt == EGodArt::Guidance && GuidanceModule )
	{
		GuidanceModule->DrawRangePreview();
	}
#endif
}

// --- 入力要求（各神技サブモジュールへ橋渡し）---

void UGodActionPlayerModule::RequestLockOnStart()
{
	// L2 は長押しで構え。押している間だけ 3 択メニューに入る（発動は R2）
	if ( IsGodBirdModeEnabled() )
	{
		BeginGodArtSelect();
		return;
	}

	if ( SlashModule ) SlashModule->RequestLockOnStart();
}

void UGodActionPlayerModule::RequestLockOnRelease()
{
	bStanceBlockedUntilRelease = false;	// 離した時点で再突入禁止が解ける

	if ( IsGodBirdModeEnabled() )
	{
		CancelGodArtSelect();
		return;
	}

	if ( SlashModule ) SlashModule->RequestLockOnRelease();
}

void UGodActionPlayerModule::RequestCancel()
{
	if ( IsGodBirdModeEnabled() )
	{
		if ( bGodArtSelecting ) CancelGodArtSelect();
		return;
	}

	if ( SlashModule ) SlashModule->RequestCancel();
}

void UGodActionPlayerModule::NotifyOwnerDamaged()
{
	if ( !bGodArtSelecting ) return;

	// 押しっぱなしのまま構えへ戻らないよう、離すまで再突入を禁止する
	bStanceBlockedUntilRelease = true;
	CancelGodArtSelect();
}

void UGodActionPlayerModule::RequestGodArtExecute()
{
	if ( !IsGodBirdModeEnabled() ) return;
	if ( !bGodArtSelecting ) return;
	ExecuteSelectedArt();
}

// --- 神鳥検証モード（3択神技メニュー）---

bool UGodActionPlayerModule::IsGodBirdModeEnabled() const
{
	// 検証フラグは撤去済み。神技は常に 3 択メニュー方式で稼働する
	return true;
}

void UGodActionPlayerModule::BeginGodArtSelect()
{
	if ( bGodArtSelecting ) return;
	if ( bStanceBlockedUntilRelease ) return;
	bGodArtSelecting = true;

	// どの神技でも構え中はロックオン状態にしておく（神鳥表示・ターゲット走査の駆動源）。
	// 導きはレティクル表示だけ HUD 側で消し、構え自体は維持する
	if ( SlashModule && !SlashModule->IsLockingOn() )
	{
		SlashModule->RequestLockOnStart();
	}

	EnterGodArtStance();

	// 移動禁止モードなら、自由移動の代わりに構えモーションを流す
	if ( ShouldPlayStanceMontage() )
	{
		PlayGodArtStanceStartMontage();
	}
}

void UGodActionPlayerModule::CancelGodArtSelect()
{
	if ( !bGodArtSelecting ) return;
	bGodArtSelecting = false;

	ExitGodArtStance();

	// 走査中のロックオンを止める（発動中＝Slashing のときは触らない）
	if ( SlashModule && SlashModule->IsLockingOn() )
	{
		SlashModule->RequestCancel();
	}
}

void UGodActionPlayerModule::ExecuteSelectedArt()
{
	if ( !IsGaugeFull() ) return;	// 構えは維持＝スローも維持

	// 戯れ／導き中は共有の神鳥が出払っているため、鳥を使う神技は発動不可
	// （憑依は常駐鳥を mover に使う別扱いなので通す）
	if ( ( SelectedArt == EGodArt::Frolic || SelectedArt == EGodArt::Guidance )
		&& ( IsFrolicActive() || IsGuidanceActive() ) )
	{
		return;
	}

	// 以降の時間制御は各神技へ委ねる（憑依／戯れは自前の演出スロー、導きはスロー無し）
	ExitGodArtStance();

	// 空中で構え→発動した場合、残ったジャンプ状態＋着地オートダッシュ予約が神技後の着地でダッシュを暴発させる
	if ( OwnerCharacter )
	{
		OwnerCharacter->ClearJumpAndLandingDash();
	}

	switch ( SelectedArt )
	{
	case EGodArt::Possession:
		// 一閃のワープ移動と競合するので、進行中のチャージ／チャージダッシュをここで畳む。構え中は推進を凍結して
		// 残り寿命を温存しているため、畳まないと一閃明けに復活する（戯れ／導きは畳まない）
		if ( OwnerCharacter )
		{
			OwnerCharacter->CancelCharge( false );
		}
		// 解除経路で一閃を発動（ゲージ消費は一閃側）
		if ( SlashModule && SlashModule->IsLockingOn() )
		{
			SlashModule->RequestLockOnRelease();
		}
		break;

	case EGodArt::Frolic:
	{
		// 構え中のロックオン対象を控えてから解除し（鳥はこの対象を巡回する）、鳥を放つ
		TArray<AActor*> FrolicTargets;
		if ( SlashModule )
		{
			SlashModule->GetLockedTargetActors( FrolicTargets );
			if ( SlashModule->IsLockingOn() )
			{
				SlashModule->RequestCancel();
			}
		}
		if ( FrolicModule )
		{
			// プレイヤーは専用モーションを行わず直前のアクションを継続する（滞空ホールドもしない）
			FrolicModule->Execute( FrolicTargets );
		}
		break;
	}

	case EGodArt::Guidance:
		// ロックオンは使わないので先に解除する。ゲージ消費は導きサブモジュール側が担う
		if ( SlashModule && SlashModule->IsLockingOn() )
		{
			SlashModule->RequestCancel();
		}
		if ( GuidanceModule )
		{
			GuidanceModule->Execute();
		}
		break;

	default:
		break;
	}

	bGodArtSelecting = false;	// 実行中の状態は各サブモジュールが持つ
}

void UGodActionPlayerModule::CycleSelection( int32 Delta )
{
	const int32 Num = (int32)EGodArt::Count;
	// 左右折り返し。負の剰余対策で Num を足してから剰余を取る
	const int32 Index = ( ( (int32)SelectedArt + Delta ) % Num + Num ) % Num;
	SetSelectedArt( (EGodArt)Index );
}

void UGodActionPlayerModule::SetSelectedArt( EGodArt NewArt )
{
	// カーソルを変えるだけ。構え（ロックオン状態）は神技に依らず維持し、
	// 導きのレティクル非表示は HUD 側で行う
	SelectedArt = NewArt;
}

bool UGodActionPlayerModule::IsFaceButtonSelectEnabled() const
{
	return OwnerCharacter && OwnerCharacter->PlayerParamData
		&& OwnerCharacter->PlayerParamData->bGodArtSelectByFaceButtons;
}

void UGodActionPlayerModule::RequestGodArtCycle( int32 Delta )
{
	if ( !IsGodBirdModeEnabled() ) return;
	if ( !bGodArtSelecting ) return;
	if ( IsFaceButtonSelectEnabled() ) return;	// 直接選択モードではカーソル移動を殺す
	CycleSelection( Delta );
}

void UGodActionPlayerModule::RequestGodArtSelectAt( int32 ArtIndex )
{
	if ( !IsGodBirdModeEnabled() ) return;
	if ( !bGodArtSelecting ) return;
	if ( !IsFaceButtonSelectEnabled() ) return;
	if ( ArtIndex < 0 || ArtIndex >= (int32)EGodArt::Count ) return;
	SetSelectedArt( (EGodArt)ArtIndex );
}

void UGodActionPlayerModule::RequestGodArtStickSelect( float AxisX )
{
	if ( !IsGodBirdModeEnabled() ) return;
	if ( !bGodArtSelecting ) return;
	if ( IsFaceButtonSelectEnabled() ) return;

	constexpr float FireThreshold    = 0.6f;	// これ以上でカーソル移動
	constexpr float NeutralThreshold = 0.3f;	// これ未満で再アーム

	if ( FMath::Abs( AxisX ) < NeutralThreshold )
	{
		bGodArtStickArmed = true;
		return;
	}

	if ( bGodArtStickArmed && FMath::Abs( AxisX ) >= FireThreshold )
	{
		CycleSelection( AxisX > 0.0f ? 1 : -1 );
		bGodArtStickArmed = false;
	}
}

void UGodActionPlayerModule::EnterGodArtStance()
{
	if ( bGodArtStanceActive ) return;
	if ( !OwnerCharacter || !OwnerCharacter->PlayerParamData ) return;

	const UTidePlayerParamDataAsset* Params = OwnerCharacter->PlayerParamData;

	GodArtStanceBrakeRampTimer.Set( Params->GodArtStanceBrakeRampTime );

	// ワールド全体を遅くしプレイヤーだけ等速側へ補正する（一閃の StartSlashStaging と同流儀）
	if ( Params->bGodArtStanceWorldSlow )
	{
		if ( UWorld* World = OwnerCharacter->GetWorld() )
		{
			// 保留中の被弾ヒットストップの復帰タイマーが、あとから構え中の高速 CustomTimeDilation を
			// 等速へ上書きしてしまうため、先に解除してから補正を張る
			HitStopUtil::CancelHitStop( OwnerCharacter );

			const float GlobalDilation = FMath::Clamp( Params->GodArtStanceGlobalTimeDilation, 0.01f, 1.0f );
			UGameplayStatics::SetGlobalTimeDilation( World, GlobalDilation );
			OwnerCharacter->SetCustomTimeDilation( Params->GodArtStancePlayerTimeScale / GlobalDilation );
		}
	}

	// 落下の緩和。空中で構えを始めた場合は落下ステートだと構えモーションが出ないため MOVE_Flying へ切り替える
	if ( UCharacterMovementComponent* Move = OwnerCharacter->GetCharacterMovement() )
	{
		SavedPlayerGravityScale = Move->GravityScale;
		Move->GravityScale = Params->GodArtStanceGravityScale;

		if ( Move->IsFalling() )
		{
			Move->SetMovementMode( MOVE_Flying );
			bStanceFlying = true;
		}
	}

	bGodArtStanceActive = true;
}

void UGodActionPlayerModule::ExitGodArtStance()
{
	// 憑依の一閃は直後に ATK ST で上書きされるが、キャンセル・戯れ・導きでは
	// 後続のプレイヤーモーションが無いためここで確実に止める
	StopGodArtStanceMontage();

	if ( !bGodArtStanceActive ) return;
	bGodArtStanceActive = false;

	if ( !OwnerCharacter ) return;

	// CMC を強制グリップさせていたら現在の状態に応じた移動パラメータへ戻す
	if ( bStanceBrakeParamsApplied )
	{
		bStanceBrakeParamsApplied = false;
		OwnerCharacter->RefreshMovementParams();
	}

	if ( UWorld* World = OwnerCharacter->GetWorld() )
	{
		UGameplayStatics::SetGlobalTimeDilation( World, 1.0f );
	}
	// 構え中（高速 CustomTimeDilation）に入った被弾ヒットストップは復帰先にその高速値を記録するため、
	// 先に解除してから 1.0 を設定して確実に等速へ戻す
	HitStopUtil::CancelHitStop( OwnerCharacter );
	OwnerCharacter->SetCustomTimeDilation( 1.0f );

	if ( UCharacterMovementComponent* Move = OwnerCharacter->GetCharacterMovement() )
	{
		Move->GravityScale = SavedPlayerGravityScale;
		if ( bStanceFlying && Move->MovementMode == MOVE_Flying )
		{
			Move->SetMovementMode( MOVE_Falling );
		}
	}
	bStanceFlying = false;
}

bool UGodActionPlayerModule::IsPlayerAirborne() const
{
	if ( !OwnerCharacter ) return false;

	UWorld* World = OwnerCharacter->GetWorld();
	const UCapsuleComponent* Capsule = OwnerCharacter->GetCapsuleComponent();
	if ( !World || !Capsule ) return false;

	const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
	constexpr float GroundTolerance = 12.0f;	// 足元からこの距離内に地形があれば接地とみなす

	const FVector Center = OwnerCharacter->GetActorLocation();
	const FVector TraceEnd = Center - FVector( 0.0f, 0.0f, HalfHeight + GroundTolerance );

	FHitResult Hit;
	FCollisionQueryParams QueryParams( FName( TEXT( "GodArtStanceGround" ) ), false, OwnerCharacter );
	return !World->LineTraceSingleByChannel( Hit, Center, TraceEnd, ECC_Visibility, QueryParams );
}

void UGodActionPlayerModule::UpdateGodArtStanceBraking( float DeltaTime )
{
	// 自由移動モードは直前アクションを継続させる方針なので触らない
	if ( !ShouldPlayStanceMontage() ) return;

	const UTidePlayerParamDataAsset* Params = OwnerCharacter ? OwnerCharacter->PlayerParamData : nullptr;
	if ( !Params ) return;
	if ( Params->GodArtStanceBrakeTime <= 0.0f && Params->GodArtStanceBrakeDeceleration <= 0.0f ) return;

	// 突入直後は効きを 0 から立ち上げる。「効かせない空白」を作ると無減速で流れた後にフルブレーキが立って
	// 段差になる（＝かなり滑ってからピタッと止まる）ので、必ず係数で繋ぐ
	float BrakeAlpha = 1.0f;
	if ( !GodArtStanceBrakeRampTimer.IsFinish() )
	{
		GodArtStanceBrakeRampTimer.Update( DeltaTime );
		BrakeAlpha = FMath::InterpEaseIn( 0.0f, 1.0f, GodArtStanceBrakeRampTimer.GetRate(), 2.0f );
	}
#if !UE_BUILD_SHIPPING
	DebugStanceBrakeAlpha = BrakeAlpha;
#endif

	UCharacterMovementComponent* Move = OwnerCharacter->GetCharacterMovement();
	if ( !Move ) return;

	// CMC 側の制動は殺し、減速カーブは本ブレーキが単独で持つ。CMC の制動は GroundFriction × BrakingFrictionFactor
	// × 速度で効くため、チャージ中の摩擦が残っていると BrakeTime を上げても一瞬で速度が消える。
	// MaxWalkSpeed は触らない（上限を絞ると CMC が一気に落としに来て段差になる）
	Move->GroundFriction = Params->GodArtStanceBrakeFriction;
	Move->BrakingDecelerationWalking = 0.0f;
	bStanceBrakeParamsApplied = true;

	FVector Horizontal = Move->Velocity;
	Horizontal.Z = 0.0f;
	const float Speed = Horizontal.Size();
#if !UE_BUILD_SHIPPING
	DebugStanceBrakeSpeedIn = Speed;
	DebugStanceBrakeSpeedOut = 0.0f;
#endif
	if ( Speed <= KINDA_SMALL_NUMBER ) return;

	// 指数減衰で「急に落ちるが 0 には貼り付かない」効きにしつつ、速度非依存の一定減速を上乗せする
	// （指数だけだと高ギアほど滑走距離が伸びて止まって見えない）
	float NewSpeed = Speed;
	if ( Params->GodArtStanceBrakeTime > 0.0f )
	{
		NewSpeed *= FMath::Exp( -DeltaTime * BrakeAlpha / Params->GodArtStanceBrakeTime );
	}
	NewSpeed -= Params->GodArtStanceBrakeDeceleration * BrakeAlpha * DeltaTime;

	if ( NewSpeed <= Params->GodArtStanceBrakeStopSpeed )
	{
		NewSpeed = 0.0f;
	}

	const FVector NewHorizontal = Horizontal * ( NewSpeed / Speed );
	Move->Velocity.X = NewHorizontal.X;
	Move->Velocity.Y = NewHorizontal.Y;
#if !UE_BUILD_SHIPPING
	DebugStanceBrakeSpeedOut = NewSpeed;
#endif
}

bool UGodActionPlayerModule::ShouldPlayStanceMontage() const
{
	return OwnerCharacter && OwnerCharacter->PlayerParamData
		&& !OwnerCharacter->PlayerParamData->bGodArtStanceAllowMovement;
}

float UGodActionPlayerModule::GetGodArtStanceMontagePlayRate() const
{
	const UTidePlayerParamDataAsset* Params = OwnerCharacter ? OwnerCharacter->PlayerParamData : nullptr;
	if ( !Params || !Params->bGodArtStanceMontageRealtime || !Params->bGodArtStanceWorldSlow ) return 1.0f;

	// 構え中はプレイヤー本体が実時間比 GodArtStancePlayerTimeScale 倍で動く＝モンタージュもその分遅くなるため、
	// 逆数を再生レートに掛けて相殺する
	return 1.0f / FMath::Max( Params->GodArtStancePlayerTimeScale, 0.01f );
}

void UGodActionPlayerModule::PlayGodArtStanceStartMontage()
{
	if ( !OwnerCharacter ) return;

	const float PlayRate = GetGodArtStanceMontagePlayRate();
	// PlayAnimMontage の戻りは再生レートを含まない尺なので、タイマー用に自前で割る
	const float Duration = PlayAnimMontage( PlayerAnimTags::GOD_SLASH_ST, PlayRate ) / PlayRate;
	GodArtStanceMontageTimer.Set( Duration > 0.0f ? Duration : 0.1f );
	bGodArtStanceLoopStarted = false;
	bGodArtStanceMontageActive = true;
}

void UGodActionPlayerModule::UpdateGodArtStanceMontage( float DeltaTime )
{
	if ( bGodArtStanceLoopStarted ) return;	// 以降はループモンタージュ側に任せる

	GodArtStanceMontageTimer.Update( DeltaTime );
	if ( GodArtStanceMontageTimer.IsFinish() )
	{
		PlayAnimMontage( PlayerAnimTags::GOD_SLASH_LP, GetGodArtStanceMontagePlayRate() );
		bGodArtStanceLoopStarted = true;
	}
}

void UGodActionPlayerModule::StopGodArtStanceMontage()
{
	if ( !bGodArtStanceMontageActive ) return;
	bGodArtStanceMontageActive = false;
	bGodArtStanceLoopStarted = false;
	GodArtStanceMontageTimer.Clear();

	if ( OwnerCharacter )
	{
		OwnerCharacter->StopAnimMontage();
	}
}

// --- ゲージ（神技共通）---

void UGodActionPlayerModule::OnDealtDamage( const FDamageInfo& DamageInfo )
{
	if ( !OwnerCharacter || !OwnerCharacter->PlayerParamData ) return;

	const UTidePlayerParamDataAsset* Params = OwnerCharacter->PlayerParamData;

	// 加算量は AttackTypeTag + GearLevel で分かれた攻撃行から取る（非チャージは GearLevel 0。無ければ加算しない）
	const FGameplayTag& GearTag = DamageInfo.ChargeGearTag;
	int32 GearLevel = 0;
	if ( GearTag == TAG_Charge_Gear1 )		GearLevel = 1;
	else if ( GearTag == TAG_Charge_Gear2 )	GearLevel = 2;
	else if ( GearTag == TAG_Charge_Gear3 )	GearLevel = 3;
	else if ( GearTag == TAG_Charge_Gear4 )	GearLevel = 4;

	const FPlayerAttackParameterRow* AttackRow = FindPlayerAttackRow( OwnerCharacter, DamageInfo.AttackTypeTag, GearLevel );
	if ( !AttackRow ) return;

	const float Gain = AttackRow->GodActionGaugeGain;
	if ( Gain <= 0.0f ) return;

	// 出発点＝被弾箇所。HitResult が未設定なら 当たった相手→プレイヤー の順でフォールバック
	FVector StartLoc = DamageInfo.HitResult.ImpactPoint;
	if ( StartLoc.IsNearlyZero() )
	{
		if ( const AActor* HitActor = DamageInfo.HitResult.GetActor() )
		{
			StartLoc = HitActor->GetActorLocation();
		}
		else
		{
			StartLoc = OwnerCharacter->GetActorLocation();
		}
	}

	AddGauge( Gain, StartLoc );
}

void UGodActionPlayerModule::AddGauge( float Amount, const FVector& OrbStartWorldLoc, float OrbRadiusScale, float OrbAlphaScale, bool bAnchorOrbToOwner )
{
	if ( Amount <= 0.0f ) return;
	if ( !OwnerCharacter || !OwnerCharacter->PlayerParamData ) return;

	const UTidePlayerParamDataAsset* Params = OwnerCharacter->PlayerParamData;
	const FVector OwnerLoc = OwnerCharacter->GetActorLocation();

	// 演出 ON はゲージへ飛ぶ玉を発行し、到達時に加算する＝「収納された後に増える」。OFF は即時加算
	if ( Params->bEnableGodGaugeOrbEffect )
	{
		// 発生源の周りから複数個を散らして発生させる。加算量は個数で等分し、飛行時間を少しばらけさせて
		// 連続的に収納＝ゲージが小刻みに増える（フラッシュも複数回出る）
		const int32 CountMin = FMath::Max( 1, Params->GodGaugeOrbCountMin );
		const int32 CountMax = FMath::Max( CountMin, Params->GodGaugeOrbCountMax );
		const int32 OrbCount = FMath::RandRange( CountMin, CountMax );
		const float PerGain = Amount / (float)OrbCount;
		const float BaseDuration = FMath::Max( 0.05f, Params->GodGaugeOrbFlightTime );
		const float Scatter = FMath::Max( 0.0f, Params->GodGaugeOrbSpawnRadius );

		// 連結線（発生点→ゲージ）を中心に玉ごとに左右交互へ振り分け、膨らみ量は乱数幅でばらけさせる
		const float JitterMin = FMath::Max( 0.0f, Params->GodGaugeOrbCurveJitterMin );
		const float JitterMax = FMath::Max( JitterMin, Params->GodGaugeOrbCurveJitterMax );
		const float FirstSign = ( FMath::RandBool() ? 1.0f : -1.0f );
		for ( int32 i = 0; i < OrbCount; ++i )
		{
			FGodGaugeOrb Orb;
			Orb.StartWorldLoc = OrbStartWorldLoc + FMath::VRand() * FMath::FRandRange( 0.0f, Scatter );
			Orb.Gain = PerGain;
			Orb.Elapsed = 0.0f;
			Orb.Duration = BaseDuration * FMath::FRandRange( 0.85f, 1.18f );
			const float Sign = ( i % 2 == 0 ) ? FirstSign : -FirstSign;	// 交互に左右へ
			Orb.Lateral = Sign * FMath::FRandRange( JitterMin, JitterMax );
			Orb.RadiusScale = OrbRadiusScale;
			Orb.AlphaScale = OrbAlphaScale;

			// 散らばり分を AnchorLocalOffset に保持し、プレイヤーに対する相対位置を保ったまま追従させる
			Orb.bAnchorToOwner = bAnchorOrbToOwner;
			if ( bAnchorOrbToOwner )
			{
				Orb.AnchorLocalOffset = Orb.StartWorldLoc - OwnerLoc;
			}
			GaugeOrbs.Add( Orb );
		}
	}
	else
	{
		CurrentGauge = FMath::Clamp( CurrentGauge + Amount, 0.0f, Params->GodActionGaugeMax );
	}
}

void UGodActionPlayerModule::UpdateGaugeOrbs( float DeltaTime )
{
	if ( GaugeOrbs.Num() == 0 ) return;

	const UTidePlayerParamDataAsset* Params = OwnerCharacter ? OwnerCharacter->PlayerParamData : nullptr;
	const float GaugeMax = Params ? Params->GodActionGaugeMax : 100.0f;

	// 後ろから走査し、到達した玉のぶんだけ加算して除去する
	for ( int32 i = GaugeOrbs.Num() - 1; i >= 0; --i )
	{
		FGodGaugeOrb& Orb = GaugeOrbs[i];

		// 高速移動でも画面上で安定させるため、発生源を毎フレームオーナー基準へ更新する
		if ( Orb.bAnchorToOwner && OwnerCharacter )
		{
			Orb.StartWorldLoc = OwnerCharacter->GetActorLocation() + Orb.AnchorLocalOffset;
		}

		Orb.Elapsed += DeltaTime;
		if ( Orb.Elapsed >= Orb.Duration )
		{
			CurrentGauge = FMath::Clamp( CurrentGauge + Orb.Gain, 0.0f, GaugeMax );
			GaugeOrbs.RemoveAt( i );
		}
	}
}

void UGodActionPlayerModule::GetGaugeOrbViews( TArray<FGodGaugeOrbView>& Out ) const
{
	Out.Reset( GaugeOrbs.Num() );
	for ( const FGodGaugeOrb& Orb : GaugeOrbs )
	{
		FGodGaugeOrbView View;
		View.StartWorldLoc = Orb.StartWorldLoc;
		View.Progress = ( Orb.Duration > 0.0f ) ? FMath::Clamp( Orb.Elapsed / Orb.Duration, 0.0f, 1.0f ) : 1.0f;
		View.Lateral = Orb.Lateral;
		View.RadiusScale = Orb.RadiusScale;
		View.AlphaScale = Orb.AlphaScale;
		Out.Add( View );
	}
}

float UGodActionPlayerModule::GetGaugeRate() const
{
	if ( !OwnerCharacter || !OwnerCharacter->PlayerParamData ) return 0.0f;
	const float Max = OwnerCharacter->PlayerParamData->GodActionGaugeMax;
	return Max > 0.0f ? FMath::Clamp( CurrentGauge / Max, 0.0f, 1.0f ) : 0.0f;
}

bool UGodActionPlayerModule::IsGaugeFull() const
{
#if !UE_BUILD_SHIPPING
	if ( UTideGameSettings* Settings = UTideGameSettings::Get() )
	{
		if ( Settings->bDebugGodActionInfiniteGauge ) return true;
	}
#endif
	if ( !OwnerCharacter || !OwnerCharacter->PlayerParamData ) return false;
	return CurrentGauge >= OwnerCharacter->PlayerParamData->GodActionGaugeMax;
}

void UGodActionPlayerModule::ConsumeGauge( float Fraction )
{
#if !UE_BUILD_SHIPPING
	if ( UTideGameSettings* Settings = UTideGameSettings::Get() )
	{
		if ( Settings->bDebugGodActionInfiniteGauge ) return;
	}
#endif
	if ( !OwnerCharacter || !OwnerCharacter->PlayerParamData ) return;

	const float GaugeMax = OwnerCharacter->PlayerParamData->GodActionGaugeMax;
	CurrentGauge = FMath::Max( 0.0f, CurrentGauge - GaugeMax * FMath::Clamp( Fraction, 0.0f, 1.0f ) );
}

// --- 状態問い合わせ・演出カメラ・HUD（各神技サブモジュールへ橋渡し）---

bool UGodActionPlayerModule::IsActive() const
{
	return SlashModule ? SlashModule->IsActive() : false;
}

bool UGodActionPlayerModule::IsLockingOn() const
{
	return SlashModule ? SlashModule->IsLockingOn() : false;
}

bool UGodActionPlayerModule::IsExecuting() const
{
	// 専有するのは憑依の一閃だけ。構え中（LockingOn＝選択）は通常アクション自由にするため false
	return SlashModule ? SlashModule->IsSlashing() : false;
}

bool UGodActionPlayerModule::IsSlashing() const
{
	return SlashModule ? SlashModule->IsSlashing() : false;
}

bool UGodActionPlayerModule::IsFrolicActive() const
{
	return FrolicModule ? FrolicModule->IsActive() : false;
}

bool UGodActionPlayerModule::IsGuidanceActive() const
{
	return GuidanceModule ? GuidanceModule->IsActive() : false;
}

bool UGodActionPlayerModule::IsGodSlashWideCutLoopPassthrough() const
{
	return SlashModule ? SlashModule->IsWideCutLoopPassthrough() : false;
}

int32 UGodActionPlayerModule::GetLockedTargetCount() const
{
	return SlashModule ? SlashModule->GetLockedTargetCount() : 0;
}

int32 UGodActionPlayerModule::GetMaxLockOnCount() const
{
	return SlashModule ? SlashModule->GetMaxLockOnCount() : 0;
}

void UGodActionPlayerModule::GetLockedTargetLocations( TArray<FVector>& OutLocations ) const
{
	if ( SlashModule )
	{
		SlashModule->GetLockedTargetLocations( OutLocations );
	}
	else
	{
		OutLocations.Reset();
	}
}

bool UGodActionPlayerModule::GetCameraFraming( FVector& OutFocusLocation, FVector& OutTargetCenter, float& OutTargetRadius, int32& OutCutIndex, bool& bOutCutLanded, bool& bOutWideCut, bool& bOutWideCutFinishing ) const
{
	return SlashModule
		? SlashModule->GetCameraFraming( OutFocusLocation, OutTargetCenter, OutTargetRadius, OutCutIndex, bOutCutLanded, bOutWideCut, bOutWideCutFinishing )
		: false;
}

// --- デバッグ ---

#if !UE_BUILD_SHIPPING
void UGodActionPlayerModule::DrawDebugImGui()
{
	ImGui::Indent();

	const float Rate = GetGaugeRate();
	ImGui::ProgressBar( Rate, ImVec2( -1.0f, 0.0f ) );
	ImGui::Text( "ゲージ: %.1f (%.0f%%) %s", CurrentGauge, Rate * 100.0f, IsGaugeFull() ? "[FULL]" : "" );

	if ( ImGui::Button( "ゲージ満タン (Debug)" ) )
	{
		if ( OwnerCharacter && OwnerCharacter->PlayerParamData )
		{
			CurrentGauge = OwnerCharacter->PlayerParamData->GodActionGaugeMax;
		}
	}

	if ( IsGodBirdModeEnabled() )
	{
		static const char* ArtNames[] = { "戯れ", "導き", "ひょう依" };
		const int32 ArtIdx = FMath::Clamp( (int32)SelectedArt, 0, 2 );
		ImGui::Text( "神鳥検証: 構え=%s / 選択=%s / 選択入力=%s", bGodArtSelecting ? "ON" : "OFF", ArtNames[ArtIdx],
			IsFaceButtonSelectEnabled() ? "X/Y/B" : "カーソル" );

		// L2 を押しても構えへ入らないのはこれが立っているとき（離すと解ける）
		if ( bStanceBlockedUntilRelease )
		{
			ImGui::Text( "  構え再突入: 禁止中（被弾解除。L2 を離すまで）" );
		}
	}

	// 構えブレーキの切り分け。PrevOut と In が離れていれば、ブレーキの後に別処理が velocity を
	// 押し戻している（＝滑りの原因がブレーキ設定ではなく他処理）と判断できる
	if ( bGodArtStanceActive && OwnerCharacter )
	{
		ImGui::Separator();

		const UCharacterMovementComponent* Move = OwnerCharacter->GetCharacterMovement();
		const float PushBack = DebugStanceBrakeSpeedIn - DebugStanceBrakeSpeedOut;

		ImGui::Text( "構えブレーキ: In %.0f -> Out %.0f (押し戻し %+.0f)",
			DebugStanceBrakeSpeedIn, DebugStanceBrakeSpeedOut, PushBack );
		ImGui::Text( "  効き %.0f%% (残 %.2f) / Flying %s / Mode %d",
			DebugStanceBrakeAlpha * 100.0f,
			GodArtStanceBrakeRampTimer.Get(),
			bStanceFlying ? "ON" : "OFF",
			Move ? (int32)Move->MovementMode : -1 );
		// 駆動が ON なら CMC がルートモーション由来の速度で上書きするため、
		// ブレーキ設定をいくら触っても効かない＝アセット側の話になる
		ImGui::Text( "  RootMotion 再生 %s / 駆動 %s",
			OwnerCharacter->IsPlayingRootMotion() ? "ON" : "OFF",
			( Move && Move->HasAnimRootMotion() ) ? "ON" : "OFF" );
		ImGui::Text( "  Friction %.1f / Braking %.0f / MaxWalk %.0f",
			Move ? Move->GroundFriction : 0.0f,
			Move ? Move->BrakingDecelerationWalking : 0.0f,
			Move ? Move->MaxWalkSpeed : 0.0f );
		ImGui::Text( "  Charging %s / ChargeDash %s",
			OwnerCharacter->IsCharging() ? "ON" : "OFF",
			OwnerCharacter->IsPlayingChargeDash() ? "ON" : "OFF" );
	}

	ImGui::Separator();

	for ( const TObjectPtr<UGodActionSubModule>& Technique : Techniques )
	{
		if ( Technique ) Technique->DrawDebugImGui();
	}

	ImGui::Unindent();
}
#endif
