// Copyright (c) 2026, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.

#include "GodBirdPlayerModule.h"

#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"

#include "PRJ_TIDE_P0/Actors/Character/Player/TidePlayerCharacter.h"
#include "PRJ_TIDE_P0/Actors/Companion/TideGodBird.h"
#include "PRJ_TIDE_P0/Core/TideGameSettings.h"
#include "PRJ_TIDE_P0/Data/Niagara/NiagaraSystemListDataAsset.h"
#include "PRJ_TIDE_P0/Data/Player/PlayerNiagaraTags.h"
#include "PRJ_TIDE_P0/Data/Player/TidePlayerParamDataAsset.h"

namespace
{
	// USlidePassivePlayerModule::TrailDefaultColor と同値（黄緑）
	const FLinearColor GodBirdVFXColor( 0.4f, 1.0f, 0.1f, 1.0f );
	const FLinearColor GodBirdTrailColor( 0.4f, 1.0f, 0.1f, 0.1f );			// トレイルは Alpha を薄めに
	const FLinearColor GodBirdTrailColorActive( 0.4f, 1.0f, 0.1f, 1.0f );	// 戯れ中は軌跡を強調

	// 剣閃トレイル（GOD_ACTION_WEAPON_TRAIL 流用）のリボン寿命。
	// 高速飛行の軌跡を見せるため憑依の剣（想定 0.15）より長めに残す
	constexpr float FrolicSwordTrailLifeTime = 0.4f;
}

void UGodBirdPlayerModule::Initialize( ATidePlayerCharacter* InOwner )
{
	Super::Initialize( InOwner );

	GodBirdActor = nullptr;
	TrailVFX = nullptr;
	FrolicSwordTrailVFX = nullptr;
	GustSourceVFX = nullptr;
	bWasVisible = false;
	CurrentAnchor = EGodBirdAnchor::Side;
	bTrailBoosted = false;
}

void UGodBirdPlayerModule::OnModuleUpdate( float DeltaTime )
{
	if ( !OwnerCharacter ) return;

	// 判定は CurrentChargeActionType ベース——集約版 IsPlayingChargeAction() は落下(LP)中に false を
	// 返すため表示が途切れる。空中通常攻撃はチャージではないので明示的に除外する
	const bool bShouldShow = IsShownByPlayerAction()
		|| ( OwnerCharacter->IsGodActionActive() && !OwnerCharacter->IsAirNormalDiveAttack() );

	// 戯れ／導き中は専用の鳥が別個体で動くため、常駐追従鳥は隠して「1個体」に見せる
	// （一閃は常駐鳥自身を mover に使うので隠さない）
	const bool bSeparateActionBird = OwnerCharacter->IsGodFrolicActive() || OwnerCharacter->IsGodGuidanceActive();

	// 竜巻エスコート中はチャージ解除後でも鳥を出したまま演出を最後まで見せる
	const bool bCinematic = GodBirdActor && GodBirdActor->IsPlayingSlidePassiveCinematic();

	const bool bShow = ( bShouldShow || bCinematic ) && !bSeparateActionBird;

	if ( bShow && !bWasVisible )
	{
		ShowGodBird();
		// VFX は ShowGodBird の APPEAR に任せる（二重に出さない）
		CurrentAnchor = ComputeDesiredAnchor();
		ApplyAnchor( CurrentAnchor, /*bSnap=*/true, /*bVFX=*/false );
	}
	else if ( !bShow && bWasVisible )
	{
		HideGodBird();
		CurrentAnchor = EGodBirdAnchor::Side;
	}

	bWasVisible = bShow;

	if ( bShow )
	{
		// 竜巻エスコート中に滑空／神技が来たら演出を打ち切り、下のアンカー切り替えで回収させる
		if ( bCinematic && ( OwnerCharacter->IsInGlideSession() || OwnerCharacter->IsGodActionActive() ) )
		{
			GodBirdActor->CancelSlidePassiveCinematic();
		}

		// 演出（自前駆動）中はアンカーを触らない。通常時のみ状態に応じて張り替える
		if ( !GodBirdActor || !GodBirdActor->IsPlayingSlidePassiveCinematic() )
		{
			const EGodBirdAnchor Desired = ComputeDesiredAnchor();
			if ( Desired != CurrentAnchor )
			{
				// 手元へ入るときだけ位置が飛ぶので消失→出現を挟む。それ以外は滑らかに寄せる
				const bool bToHand = ( Desired == EGodBirdAnchor::Hand );
				ApplyAnchor( Desired, /*bSnap=*/bToHand, /*bVFX=*/bToHand );
				CurrentAnchor = Desired;
			}
		}
	}

	// 戯れ発動中（外部駆動で飛び回る間）の演出切り替え。状態が変わった瞬間だけ処理する
	if ( GodBirdActor )
	{
		const bool bFrolic = GodBirdActor->IsExternallyDriven();
		if ( bFrolic != bTrailBoosted )
		{
			bTrailBoosted = bFrolic;

			if ( TrailVFX )
			{
				TrailVFX->SetColorParameter( TEXT( "Color" ), bFrolic ? GodBirdTrailColorActive : GodBirdTrailColor );
			}

			// GODBIRD_TRAIL は高速飛行で軌跡を引けないため、戯れ中だけ憑依と同じ剣閃トレイルを足す
			if ( bFrolic )	StartFrolicSwordTrail();
			else			StopFrolicSwordTrail();
		}

		// 構え中（対象選択フェーズ）は鳥をカメラ方向へ向ける。
		// 戯れ発動中は一閃側が向きを制御するので触らない
		const bool bStance = OwnerCharacter->IsGodActionLockingOn() && !GodBirdActor->IsExternallyDriven();
		if ( bStance )
		{
			// カメラマネージャが取れなければコントロール回転で代用。Roll は水平を保つため 0 にする
			FRotator CamRot = OwnerCharacter->GetActorRotation();
			if ( const APlayerController* PC = Cast<APlayerController>( OwnerCharacter->GetController() ) )
			{
				CamRot = PC->PlayerCameraManager ? PC->PlayerCameraManager->GetCameraRotation() : PC->GetControlRotation();
			}
			GodBirdActor->SetFacingOverride( true, FRotator( CamRot.Pitch, CamRot.Yaw, 0.0f ) );

			// ワールドスローの影響を受けないよう、鳥をプレイヤーと同じ時間倍率にする
			GodBirdActor->CustomTimeDilation = OwnerCharacter->CustomTimeDilation;
		}
		else
		{
			GodBirdActor->SetFacingOverride( false, FRotator::ZeroRotator );

			// 外部駆動中は一閃側が時間倍率を管理するので触らない
			if ( !GodBirdActor->IsExternallyDriven() )
			{
				GodBirdActor->CustomTimeDilation = 1.0f;
			}
		}
	}
}

bool UGodBirdPlayerModule::IsShownByPlayerAction() const
{
	if ( !OwnerCharacter ) return false;
	if ( OwnerCharacter->IsAirNormalDiveAttack() ) return false;

	return OwnerCharacter->IsCharging()
		|| OwnerCharacter->IsPlayingChargeAttack()
		|| OwnerCharacter->IsPlayingChargeDash()
		|| OwnerCharacter->IsPlayingChargeJump()
		|| OwnerCharacter->IsInGlideSession();
}

ATideGodBird* UGodBirdPlayerModule::GetOrSpawnBird()
{
	// 未生成なら遅延生成する（チャージ前に神技を撃った場合でも鳥を用意できるように）
	EnsureGodBirdSpawned();
	return GodBirdActor;
}

bool UGodBirdPlayerModule::StartTornadoEscort( const FVector& TornadoCenter )
{
	if ( !OwnerCharacter ) return false;

	const UTidePlayerParamDataAsset* Params = OwnerCharacter->PlayerParamData;
	if ( !Params ) return false;

	ATideGodBird* Bird = GetOrSpawnBird();
	if ( !Bird ) return false;

	Bird->ActivateTornadoEscort( TornadoCenter,
		Params->GodBirdTornadoOrbitRadius, Params->GodBirdTornadoOrbitHeight, Params->GodBirdTornadoOrbitStartHeight,
		Params->GodBirdTornadoEscortSpeed, Params->GodBirdTornadoOrbitAngularSpeed, Params->GodBirdTornadoOrbitTurns );
	return true;
}

void UGodBirdPlayerModule::EnsureGodBirdSpawned()
{
	if ( GodBirdActor ) return;
	if ( !OwnerCharacter ) return;

	const UTidePlayerParamDataAsset* Params = OwnerCharacter->PlayerParamData;
	if ( !Params || !Params->GodBirdActorClass ) return;

	UWorld* World = OwnerCharacter->GetWorld();
	if ( !World ) return;

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = OwnerCharacter;
	SpawnParams.Instigator = OwnerCharacter;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	GodBirdActor = World->SpawnActor<ATideGodBird>(
		Params->GodBirdActorClass, OwnerCharacter->GetActorLocation(), OwnerCharacter->GetActorRotation(), SpawnParams );
	if ( !GodBirdActor ) return;

	// 剛体アタッチはせず補間追従させる。オフセット・向き調整はプレイヤー基準のローカルで渡す
	GodBirdActor->SetFollowTarget( OwnerCharacter, Params->GodBirdSideOffset, Params->GodBirdRelativeRotation );

	GodBirdActor->SetVisible( false );	// 生成直後は隠す（チャージ開始で表示）
}

UGodBirdPlayerModule::EGodBirdAnchor UGodBirdPlayerModule::ComputeDesiredAnchor() const
{
	if ( !OwnerCharacter ) return EGodBirdAnchor::Side;

	// 滑空中は手元固定を最優先
	if ( OwnerCharacter->IsInGlideSession() ) return EGodBirdAnchor::Hand;

	// バフを消費したチャージアクション進行中だけ正面へ回り込み、鳥が風源に見えるようにする
	// （バフ arm 中＝チャージ長押し中は寄せない。神技中は神技側優先で通常横）
	const UTidePlayerParamDataAsset* Params = OwnerCharacter->PlayerParamData;
	if ( Params && Params->bGodBirdSlidePassiveEscort
		&& OwnerCharacter->IsGustBuffedChargeActionActive() && !OwnerCharacter->IsGodActionActive() )
	{
		return EGodBirdAnchor::GustFront;
	}

	return EGodBirdAnchor::Side;
}

void UGodBirdPlayerModule::ApplyAnchor( EGodBirdAnchor Anchor, bool bSnap, bool bVFX )
{
	if ( !OwnerCharacter || !GodBirdActor ) return;

	const UTidePlayerParamDataAsset* Params = OwnerCharacter->PlayerParamData;
	if ( !Params ) return;

	// 位置が飛ぶ手元スナップ時のみ、移動前で消失→移動先で出現エフェクトを挟む
	if ( bVFX ) SpawnGodBirdVFX( PlayerNiagaraTags::GODBIRD_DISAPPEAR );

	switch ( Anchor )
	{
	case EGodBirdAnchor::Hand:
		if ( !Params->GodBirdGlideHandSocket.IsNone() )
		{
			// ソケット位置＋オフセットへ完全に引っ付く（補間・浮遊なし）
			GodBirdActor->SetFollowTarget( OwnerCharacter, Params->GodBirdGlideHandOffset, Params->GodBirdRelativeRotation, Params->GodBirdGlideHandSocket, /*bStickToTarget=*/true, bSnap );
		}
		else
		{
			GodBirdActor->SetFollowTarget( OwnerCharacter, Params->GodBirdSideOffset, Params->GodBirdRelativeRotation, NAME_None, /*bStickToTarget=*/false, bSnap );
		}
		break;

	case EGodBirdAnchor::GustFront:
		// プレイヤー正面へ回り込む。bSnap=false で横から滑らかに移動する
		GodBirdActor->SetFollowTarget( OwnerCharacter, Params->GodBirdGustFrontOffset, Params->GodBirdRelativeRotation, NAME_None, /*bStickToTarget=*/false, bSnap );
		break;

	case EGodBirdAnchor::Side:
	default:
		GodBirdActor->SetFollowTarget( OwnerCharacter, Params->GodBirdSideOffset, Params->GodBirdRelativeRotation, NAME_None, /*bStickToTarget=*/false, bSnap );
		break;
	}

	if ( bVFX ) SpawnGodBirdVFX( PlayerNiagaraTags::GODBIRD_APPEAR );

	// 正面回り込みの間だけ、鳥が風を出している VFX を出す
	if ( Anchor == EGodBirdAnchor::GustFront )	StartGustSourceVFX();
	else										StopGustSourceVFX();
}

void UGodBirdPlayerModule::ShowGodBird()
{
	EnsureGodBirdSpawned();
	if ( GodBirdActor )
	{
		GodBirdActor->SetVisible( true );
		SpawnGodBirdVFX( PlayerNiagaraTags::GODBIRD_APPEAR );
		StartGodBirdTrail();
	}
}

void UGodBirdPlayerModule::HideGodBird()
{
	if ( GodBirdActor )
	{
		StopGodBirdTrail();		// メッシュへアタッチしているので先に破棄する
		StopFrolicSwordTrail();	// 戯れ中に消えた場合の保険
		StopGustSourceVFX();
		bTrailBoosted = false;

		// 消失エフェクトは隠す前に出す（SetVisible の非表示は子へ伝播しないので残る）
		SpawnGodBirdVFX( PlayerNiagaraTags::GODBIRD_DISAPPEAR );
		GodBirdActor->SetVisible( false );
	}
}

void UGodBirdPlayerModule::SpawnGodBirdVFX( const FName& NiagaraTag )
{
	if ( !OwnerCharacter || !GodBirdActor ) return;
	if ( !OwnerCharacter->NiagaraSystemDataAsset ) return;

	UNiagaraSystem* Sys = OwnerCharacter->NiagaraSystemDataAsset->GetNiagaraSystem( NiagaraTag );
	if ( !Sys ) return;	// 未割当なら何も出さない（枠だけ用意）

	// メッシュへアタッチして単発再生する。SetVisible の非表示は子へ伝播しないので、
	// 消滅演出を出した直後にメッシュを隠しても描画され続ける
	USceneComponent* AttachTarget = GodBirdActor->GetBirdMesh();
	if ( !AttachTarget )
	{
		AttachTarget = GodBirdActor->GetRootComponent();
	}

	UNiagaraComponent* VFX = UNiagaraFunctionLibrary::SpawnSystemAttached(
		Sys,
		AttachTarget,
		NAME_None,
		FVector::ZeroVector,
		FRotator::ZeroRotator,
		EAttachLocation::SnapToTarget,
		true,	// bAutoDestroy — 単発なので再生後に自動破棄
		true	// bAutoActivate
	);
	if ( VFX )
	{
		VFX->SetColorParameter( TEXT( "Color" ), GodBirdVFXColor );
	}
}

void UGodBirdPlayerModule::StartGodBirdTrail()
{
	if ( TrailVFX ) return;	// 既に出ている
	if ( !OwnerCharacter || !GodBirdActor ) return;
	if ( !OwnerCharacter->NiagaraSystemDataAsset ) return;

	UNiagaraSystem* Sys = OwnerCharacter->NiagaraSystemDataAsset->GetNiagaraSystem( PlayerNiagaraTags::GODBIRD_TRAIL );
	if ( !Sys ) return;	// 未割当なら何も出さない（枠だけ用意）

	// メッシュへアタッチし、鳥の移動・ふわふわに追従して軌跡を引く
	USceneComponent* AttachTarget = GodBirdActor->GetBirdMesh();
	if ( !AttachTarget )
	{
		AttachTarget = GodBirdActor->GetRootComponent();
	}

	TrailVFX = UNiagaraFunctionLibrary::SpawnSystemAttached(
		Sys,
		AttachTarget,
		NAME_None,
		FVector::ZeroVector,
		FRotator::ZeroRotator,
		EAttachLocation::SnapToTarget,
		false,	// bAutoDestroy — 表示中は持続させ、非表示時にこちらで破棄
		true	// bAutoActivate
	);
	if ( TrailVFX )
	{
		// 生成直後は薄い方から。戯れ発動中なら次の OnModuleUpdate で上がる
		TrailVFX->SetColorParameter( TEXT( "Color" ), GodBirdTrailColor );
		bTrailBoosted = false;
	}
}

void UGodBirdPlayerModule::StopGodBirdTrail()
{
	if ( TrailVFX )
	{
		TrailVFX->Deactivate();
		TrailVFX->DestroyComponent();
		TrailVFX = nullptr;
	}
}

void UGodBirdPlayerModule::StartFrolicSwordTrail()
{
	if ( FrolicSwordTrailVFX ) return;	// 既に出ている
	if ( !OwnerCharacter || !GodBirdActor ) return;
	if ( !OwnerCharacter->NiagaraSystemDataAsset ) return;

	// 憑依の剣と同じ剣閃トレイル（ワールド空間リボン）を流用する
	UNiagaraSystem* Sys = OwnerCharacter->NiagaraSystemDataAsset->GetNiagaraSystem( PlayerNiagaraTags::GOD_ACTION_WEAPON_TRAIL );
	if ( !Sys ) return;

	// 鳥は剣ソケットを持たないためメッシュ原点へアタッチし、飛行の world 移動で軌跡を引く
	USceneComponent* AttachTarget = GodBirdActor->GetBirdMesh();
	if ( !AttachTarget )
	{
		AttachTarget = GodBirdActor->GetRootComponent();
	}

	FrolicSwordTrailVFX = UNiagaraFunctionLibrary::SpawnSystemAttached(
		Sys,
		AttachTarget,
		NAME_None,
		FVector::ZeroVector,
		FRotator::ZeroRotator,
		EAttachLocation::SnapToTarget,
		false,	// bAutoDestroy — 停止はこちらで管理し、残ったリボンをフェードさせる
		true	// bAutoActivate
	);
	if ( FrolicSwordTrailVFX )
	{
		FrolicSwordTrailVFX->SetFloatParameter( TEXT( "LifeTime" ), FrolicSwordTrailLifeTime );
		FrolicSwordTrailVFX->SetColorParameter( TEXT( "Color" ), GodBirdTrailColorActive );
	}
}

void UGodBirdPlayerModule::StopFrolicSwordTrail()
{
	if ( !FrolicSwordTrailVFX ) return;

	// 放出を止め、残ったリボンを LifeTime ぶんフェードさせてから自動消滅させる
	FrolicSwordTrailVFX->Deactivate();
	FrolicSwordTrailVFX->SetAutoDestroy( true );
	FrolicSwordTrailVFX = nullptr;
}

void UGodBirdPlayerModule::StartGustSourceVFX()
{
	if ( GustSourceVFX ) return;	// 既に出ている
	if ( !OwnerCharacter || !GodBirdActor ) return;
	if ( !OwnerCharacter->NiagaraSystemDataAsset ) return;

	UNiagaraSystem* Sys = OwnerCharacter->NiagaraSystemDataAsset->GetNiagaraSystem( PlayerNiagaraTags::GODBIRD_GUST );
	if ( !Sys ) return;	// 未割当なら何も出さない（枠だけ用意）

	USceneComponent* AttachTarget = GodBirdActor->GetBirdMesh();
	if ( !AttachTarget )
	{
		AttachTarget = GodBirdActor->GetRootComponent();
	}

	GustSourceVFX = UNiagaraFunctionLibrary::SpawnSystemAttached(
		Sys,
		AttachTarget,
		NAME_None,
		FVector::ZeroVector,
		FRotator::ZeroRotator,
		EAttachLocation::SnapToTarget,
		false,	// bAutoDestroy — 正面回り込み中は持続させ、離脱時にこちらで破棄
		true	// bAutoActivate
	);
	if ( GustSourceVFX )
	{
		GustSourceVFX->SetColorParameter( TEXT( "Color" ), GodBirdVFXColor );
	}
}

void UGodBirdPlayerModule::StopGustSourceVFX()
{
	if ( !GustSourceVFX ) return;

	GustSourceVFX->Deactivate();
	GustSourceVFX->DestroyComponent();
	GustSourceVFX = nullptr;
}
