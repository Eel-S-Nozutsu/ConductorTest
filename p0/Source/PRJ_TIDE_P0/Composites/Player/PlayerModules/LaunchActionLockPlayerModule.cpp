// Copyright (c) 2026, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.
#include "LaunchActionLockPlayerModule.h"

#include "Animation/AnimMontage.h"

#include "PRJ_TIDE_P0/Actors/Character/Player/TidePlayerCharacter.h"
#include "PRJ_TIDE_P0/Data/Player/TidePlayerParamDataAsset.h"
#include "PRJ_TIDE_P0/Data/Player/PlayerAnimTags.h"
#include "PRJ_TIDE_P0/Data/Tags/TideGameplayTags.h"

namespace
{
	constexpr float AscentVelocityThreshold = 10.0f;	// 上昇とみなす Z 速度（cm/s）
	constexpr float FallbackMaxLockDuration = 3.0f;		// PlayerParamData が無い場合の保険
}

void ULaunchActionLockPlayerModule::Initialize( ATidePlayerCharacter* InOwner )
{
	Super::Initialize( InOwner );

	bIsLocked = false;
	bIsAscending = false;
	bPendingChargeResolve = false;
	bObservedAscent = false;
	PeakAscentVz = 0.0f;
	ElapsedTime = 0.0f;
}

void ULaunchActionLockPlayerModule::BeginLaunchLock()
{
	if ( !OwnerCharacter ) return;

	// 連続して別パッドに乗ったケースも含め、頂点判定を取り直す
	ElapsedTime = 0.0f;
	bObservedAscent = false;
	PeakAscentVz = 0.0f;
	bIsAscending = true;
	bPendingChargeResolve = false;

	// 溜め中に打ち上げへ乗ったときの扱い。既定は即キャンセル（上昇中はチャージアクションを出さない）で、
	// ON なら溜めを保持し、封印中のリリースを解除時まで後回しにして発動する
	const UTidePlayerParamDataAsset* Params = GetPlayerParams();
	const bool bDeferChargeRelease = Params ? Params->bLaunchLockDeferChargeRelease : false;
	if ( !bDeferChargeRelease && OwnerCharacter->IsCharging() )
	{
		OwnerCharacter->CancelCharge( false );
	}

	// 上のキャンセル後に再評価するので、即キャンセル時は打ち上げ用モーションが流れる
	const bool bIsChargingOrActing = OwnerCharacter->IsCharging() || OwnerCharacter->IsPlayingChargeAction();

	// チャージ中はチャージ側のモーションを優先させるため上書きしない
	if ( !bIsChargingOrActing )
	{
		PlayAnimMontage( PlayerAnimTags::CHARGE_JUMP_LP );
	}

	// 竜巻ジャンプと同じ風切りトレイル（チャージダッシュ中は向こうが同じものを出しているので
	// JumpModule 側で弾かれる）。破棄は JumpModule の着地／再生秒数切れが担う
	OwnerCharacter->StartJumpWindTrailEffect();

	// タグの二重付与はしない（参照カウントを add/remove 1:1 に保つ）
	if ( bIsLocked ) return;

	bIsLocked = true;
	// 上昇中は移動を除く全アクションを封じる。移動は RequestMove / GetAllowedMovementInput 側が
	// IsLaunchActionLocked() のとき Disable を無視して許可する
	OwnerCharacter->AddStateTag( TAG_State_Common_Disable );
}

void ULaunchActionLockPlayerModule::OnModuleUpdate( float DeltaTime )
{
	if ( !OwnerCharacter ) return;
	// 封印が解けた後も、上昇が終わるまでは頂点判定のために回し続ける
	if ( !bIsLocked && !bIsAscending ) return;

	// 死亡側でも行動は止まるが、封印タグを残さないよう即解除する（チャージ発動もしない）
	if ( OwnerCharacter->IsDead() )
	{
		EndLaunchLock( false );
		EndLaunchAscent( false );
		return;
	}

	ElapsedTime += DeltaTime;

	const float Vz = OwnerCharacter->GetVelocity().Z;
	if ( Vz > AscentVelocityThreshold )
	{
		bObservedAscent = true;
		PeakAscentVz = FMath::Max( PeakAscentVz, Vz );	// レート解除のしきい値基準
	}

	// 上昇速度はほぼ線形に減るので、(1 - Rate) * PeakAscentVz は「上昇区間の先頭から Rate ぶん
	// 経過した地点」に一致する（Rate=1 で頂点、0 なら実質すぐ解除）
	const UTidePlayerParamDataAsset* Params = GetPlayerParams();
	const float Rate = Params ? FMath::Clamp( Params->LaunchActionLockAscentRate, 0.0f, 1.0f ) : 1.0f;
	const float ReleaseVz = ( 1.0f - Rate ) * PeakAscentVz;
	const bool bReachedReleasePoint = bObservedAscent && ( Vz <= ReleaseVz );

	// 横方向打ち上げなど、解除点に達しないケースの保険
	const float MaxDuration = Params ? Params->LaunchActionLockMaxDuration : FallbackMaxLockDuration;
	const bool bTimedOut = ( MaxDuration > 0.0f ) && ( ElapsedTime >= MaxDuration );

	if ( bIsLocked && ( bReachedReleasePoint || bTimedOut ) )
	{
		EndLaunchLock( true );
	}

	// 頂点は封印解除とは別に見る。Rate<1 では封印が先に解けるため、チャージだけはここまで待たせる
	if ( bIsAscending && ( ( bObservedAscent && Vz <= 0.0f ) || bTimedOut ) )
	{
		EndLaunchAscent( true );
	}
}

void ULaunchActionLockPlayerModule::OnLanded()
{
	// 頂点前に着地・天井衝突した場合の保険（頂点で既に解除済みなら何もしない）
	if ( bIsLocked )
	{
		EndLaunchLock( true );
	}
	EndLaunchAscent( true );

	// チャージジャンプ LP は着地まで流し続ける方針なので、停止はここだけで行う。
	// 頂点で解除後に別アクションへ移っていれば現在モンタージュは LP でないので no-op
	if ( !OwnerCharacter ) return;
	if ( UAnimMontage* LpMontage = GetAnimMontage( PlayerAnimTags::CHARGE_JUMP_LP ) )
	{
		if ( OwnerCharacter->GetCurrentMontage() == LpMontage )
		{
			constexpr float MontageBlendOutTime = 0.2f;
			OwnerCharacter->StopAnimMontage( MontageBlendOutTime, LpMontage );
		}
	}
}

void ULaunchActionLockPlayerModule::EndLaunchLock( bool bExecuteDeferredCharge )
{
	if ( !bIsLocked ) return;

	bIsLocked = false;
	// 上昇の計測はここでは捨てない。封印はレート地点で先に解けるので、頂点判定はそのあとも
	// 同じ計測で続ける（片付けは EndLaunchAscent）

	if ( OwnerCharacter )
	{
		// 解除するのはタグだけ。モーションの停止は OnLanded で行う
		OwnerCharacter->RemoveStateTag( TAG_State_Common_Disable );

		if ( bExecuteDeferredCharge )
		{
			// 打ち上げ中に溜めへ入られると、そのまま滑空・空中アクションの主導権を奪ってしまうため、
			// まだ上昇中（Rate<1 で頂点前に封印が解けた）ならチャージ関連は頂点まで持ち越す
			bPendingChargeResolve = true;
			if ( !bIsAscending ) EndLaunchAscent( true );
		}
	}
}

void ULaunchActionLockPlayerModule::EndLaunchAscent( bool bExecuteDeferredCharge )
{
	bIsAscending = false;
	bObservedAscent = false;
	PeakAscentVz = 0.0f;
	ElapsedTime = 0.0f;

	if ( !bPendingChargeResolve ) return;
	bPendingChargeResolve = false;

	if ( !bExecuteDeferredCharge || !OwnerCharacter ) return;

	OwnerCharacter->FlushPendingLaunchChargeRelease();

	// R2 を押し続けていれば、動けるようになった瞬間にチャージを再開する（即キャンセル後も含む）
	OwnerCharacter->ResumeHeldChargeAfterLaunchLock();
}

const UTidePlayerParamDataAsset* ULaunchActionLockPlayerModule::GetPlayerParams() const
{
	return OwnerCharacter ? OwnerCharacter->PlayerParamData : nullptr;
}
