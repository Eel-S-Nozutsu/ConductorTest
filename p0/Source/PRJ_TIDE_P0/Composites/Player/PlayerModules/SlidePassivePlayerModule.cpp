// Copyright (c) 2026, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.
#include "SlidePassivePlayerModule.h"

#include "Engine/World.h"
#include "Components/CapsuleComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraComponent.h"

#include "PRJ_TIDE_P0/Actors/Character/Player/TidePlayerCharacter.h"
#include "PRJ_TIDE_P0/Data/Player/TidePlayerParamDataAsset.h"
#include "PRJ_TIDE_P0/Data/Player/PlayerNiagaraTags.h"
#include "PRJ_TIDE_P0/Data/Niagara/NiagaraSystemListDataAsset.h"

#include "PRJ_TIDE_P0/Composites/Player/PlayerModules/SlidePassiveSubModule.h"
#include "PRJ_TIDE_P0/Composites/Player/PlayerModules/SlidePassiveTornadoSubModule.h"
#include "PRJ_TIDE_P0/Composites/Player/PlayerModules/SlidePassiveGustSubModule.h"

#if !UE_BUILD_SHIPPING
#include "imgui.h"
#endif

void USlidePassivePlayerModule::Initialize( ATidePlayerCharacter* InOwner )
{
	Super::Initialize( InOwner );

	SubModules.Reset();

	USlidePassiveTornadoSubModule* Tornado = NewObject<USlidePassiveTornadoSubModule>( this );
	Tornado->Initialize( this, InOwner );
	SubModules.Add( Tornado );

	USlidePassiveGustSubModule* Gust = NewObject<USlidePassiveGustSubModule>( this );
	Gust->Initialize( this, InOwner );
	SubModules.Add( Gust );
}

void USlidePassivePlayerModule::OnModuleUpdate( float DeltaTime )
{
	if ( !OwnerCharacter || !OwnerCharacter->PlayerParamData ) return;

	// 有効な機能が1つも無ければ追跡しない（トレイルも出さない）
	bool bAnyEnabled = false;
	for ( const TObjectPtr<USlidePassiveSubModule>& Sub : SubModules )
	{
		if ( Sub && Sub->IsEnabled() ) { bAnyEnabled = true; break; }
	}

	bInChargeDashGrace = false;

	if ( !bAnyEnabled )
	{
		if ( bIsTracking ) StopTracking( /*bChargeReleased=*/false );
	}
	else
	{
		const UTidePlayerParamDataAsset* Params = OwnerCharacter->PlayerParamData;

		// 基本はチャージ中だけ軌跡を記録するが、チャージダッシュ派生後も猶予時間だけ判定を残す。
		// ただし「解除時まとめ竜巻モード」では解除＝即終了にしたいので延長しない
		const bool bUseChargeDashGrace = !Params->bSlidePassiveDeferOnRelease;

		const bool bCharging = OwnerCharacter->IsCharging();
		if ( bCharging )
		{
			ChargeDashTrackGraceTimer = Params->SlidePassiveChargeDashGraceTime;
		}

		bool bShouldTrack = bCharging;
		if ( !bCharging )
		{
			if ( bUseChargeDashGrace && OwnerCharacter->IsPlayingChargeDash() && ChargeDashTrackGraceTimer > 0.0f )
			{
				ChargeDashTrackGraceTimer -= DeltaTime;
				bShouldTrack = true;
				bInChargeDashGrace = true;
			}
			else
			{
				ChargeDashTrackGraceTimer = 0.0f;
			}
		}

		if ( bShouldTrack && !bIsTracking )
		{
			StartTracking();
		}
		else if ( !bShouldTrack && bIsTracking )
		{
			StopTracking( /*bChargeReleased=*/true );	// チャージ解除・ダッシュ猶予切れ
		}

		// 完成インターバル中（フラッシュ〜フェードアウト）はサンプリングを止める
		if ( bIsTracking && !IsTrailCompletionLocked() )
		{
			UpdateTracking( DeltaTime );
		}
	}

	// 保留タイマー等はチャージ終了後も進める必要があるため常に呼ぶ
	for ( const TObjectPtr<USlidePassiveSubModule>& Sub : SubModules )
	{
		if ( Sub ) Sub->OnUpdate( DeltaTime );
	}

	UpdateTrailVFX( DeltaTime );
}

void USlidePassivePlayerModule::StartTracking()
{
	bIsTracking = true;
	bPendingReleaseFlash = false;
	ResetSampling();
	NotifySubModulesTrajectoryReset();
	SpawnTrailVFX();

	for ( const TObjectPtr<USlidePassiveSubModule>& Sub : SubModules )
	{
		if ( Sub ) Sub->OnTrackingStarted();
	}
}

void USlidePassivePlayerModule::StopTracking( bool bChargeReleased )
{
	bIsTracking = false;
	ResetSampling();

	for ( const TObjectPtr<USlidePassiveSubModule>& Sub : SubModules )
	{
		if ( Sub ) Sub->OnTrackingStopped( bChargeReleased );
	}

	// 解除時まとめモードで図形完成があった場合は、解除がわかりやすいようここで 1 回だけ完成演出を出す。
	// bIsTracking は既に false なので演出後は再スポーンせず破棄される
	if ( bPendingReleaseFlash )
	{
		bPendingReleaseFlash = false;
		if ( UseNewTrailColorScheme() )
		{
			BeginTrailReleaseWhiteFade();	// 赤 → 白 → フェードアウト
		}
		else
		{
			BeginTrailFlash();				// 赤フラッシュ → 通常色 → フェードアウト
		}
	}
	else if ( !IsTrailCompletionLocked() )
	{
		BeginTrailFadeOut();	// 印未完成（既にフラッシュ中なら上書きしない）
	}
}

void USlidePassivePlayerModule::ResetSampling()
{
	LastHeading = FVector::ZeroVector;
	bHasLastHeading = false;
	bHasFirstSample = false;
}

void USlidePassivePlayerModule::NotifySubModulesTrajectoryReset()
{
	for ( const TObjectPtr<USlidePassiveSubModule>& Sub : SubModules )
	{
		if ( Sub ) Sub->OnTrajectoryReset();
	}
}

void USlidePassivePlayerModule::UpdateTracking( float DeltaTime )
{
	if ( !OwnerCharacter || !OwnerCharacter->PlayerParamData ) return;
	const UTidePlayerParamDataAsset* Params = OwnerCharacter->PlayerParamData;

	FVector Foot = OwnerCharacter->GetActorLocation();	// 足元（地面）基準
	if ( const UCapsuleComponent* Capsule = OwnerCharacter->GetCapsuleComponent() )
	{
		Foot.Z -= Capsule->GetScaledCapsuleHalfHeight();
	}

	if ( !bHasFirstSample )
	{
		bHasFirstSample = true;
		LastSampleLoc = Foot;
		DispatchSampleToSubModules( Foot, 0.0f );
		return;
	}

	// 一定距離進むごとにサンプリング
	if ( FVector::Dist2D( Foot, LastSampleLoc ) < Params->SlidePassiveSampleDistance )
	{
		return;
	}

	FVector Heading = Foot - LastSampleLoc;
	Heading.Z = 0.0f;
	Heading = Heading.GetSafeNormal();

	float SignedTurnDeltaDeg = 0.0f;
	if ( !Heading.IsNearlyZero() )
	{
		if ( bHasLastHeading )
		{
			// 符号付き角度差＝左右どちらに回ったか
			const float Cross = LastHeading.X * Heading.Y - LastHeading.Y * Heading.X;
			const float Dot = FVector::DotProduct( LastHeading, Heading );
			SignedTurnDeltaDeg = FMath::RadiansToDegrees( FMath::Atan2( Cross, Dot ) );
		}
		LastHeading = Heading;
		bHasLastHeading = true;
	}

	LastSampleLoc = Foot;
	DispatchSampleToSubModules( Foot, SignedTurnDeltaDeg );
}

void USlidePassivePlayerModule::DispatchSampleToSubModules( const FVector& Foot, float SignedTurnDeltaDeg )
{
	for ( const TObjectPtr<USlidePassiveSubModule>& Sub : SubModules )
	{
		if ( !Sub || !Sub->IsEnabled() ) continue;

		// 猶予中の検出を許さないサブモジュール（突風）へは配信しない
		if ( bInChargeDashGrace && !Sub->CanDetectDuringChargeDashGrace() ) continue;

		Sub->OnSampleAdded( Foot, SignedTurnDeltaDeg );
	}
}

// ─── トレイルリボン管理（共有） ────────────────────────────────────────────────

namespace
{
	// スライド中はこの巨大 LifeTime で全セグメントを「死なせず・ずっと表示」にする。終了時は LifeTime は触らず、
	// Color.A を 1→0 にしてリボン全体を一様にフェードアウトさせてから破棄する
	constexpr float TrailHoldLifeTime           = 9999.0f;
	constexpr float TrailEndFadeDuration        = 0.4f;
	constexpr float TrailFlashToRedDuration     = 0.15f;
	constexpr float TrailFlashReturnDuration    = 0.4f;

	// 印完成時：明るく光らせる → 通常色と「今の明るさの半分」を行き来してチカチカし、チャージ解除まで持続する
	constexpr float TrailCompletionBrightenDuration  = 0.4f;
	constexpr float TrailCompletionFlickerFrequency  = 6.0f;	// rad/s。小さいほどゆっくりフェード
	constexpr float TrailCompletionBrightnessScale   = 100.0f;	// ブルームで発光させるピーク倍率

	constexpr float TrailFootUpOffset           = 10.0f;	// 地面からわずかに浮かせる（cm）

	// 新色演出：緑→赤→白フェードの各遷移時間（秒）
	constexpr float TrailNewToRedDuration       = 0.2f;
	constexpr float TrailNewToWhiteDuration     = 0.15f;
	constexpr float TrailNewWhiteFadeDuration   = 0.4f;
	constexpr float TrailNewWhiteBrightness     = 4.0f;	// 発光させるため白も持ち上げる。Alpha は別管理

	const FLinearColor TrailDefaultColor = FLinearColor( 0.4f, 1.0f, 0.1f, 1.0f );	// 黄緑（通常時）
}

bool USlidePassivePlayerModule::UseNewTrailColorScheme() const
{
	const UTidePlayerParamDataAsset* Params = OwnerCharacter ? OwnerCharacter->PlayerParamData : nullptr;
	return Params && Params->bSlidePassiveNewTrailColor && Params->bSlidePassiveDeferOnRelease;
}

void USlidePassivePlayerModule::SpawnTrailVFX()
{
	if ( !OwnerCharacter || !OwnerCharacter->NiagaraSystemDataAsset ) return;

	// 連続スライド対応：前のトレイルが残っていれば即時破棄
	if ( TrailVFX )
	{
		TrailVFX->DestroyComponent();
		TrailVFX = nullptr;
	}

	UNiagaraSystem* Sys = OwnerCharacter->NiagaraSystemDataAsset->GetNiagaraSystem( PlayerNiagaraTags::PASSIVE_TRAIL );
	if ( !Sys ) return;

	float FootOffsetZ = 0.0f;
	if ( const UCapsuleComponent* Capsule = OwnerCharacter->GetCapsuleComponent() )
	{
		FootOffsetZ = -Capsule->GetScaledCapsuleHalfHeight() + TrailFootUpOffset;
	}

	TrailVFX = UNiagaraFunctionLibrary::SpawnSystemAttached(
		Sys,
		OwnerCharacter->GetRootComponent(),
		NAME_None,
		FVector( 0.0f, 0.0f, FootOffsetZ ),
		FRotator::ZeroRotator,
		EAttachLocation::KeepRelativeOffset,
		false,	// bAutoDestroy — 寿命はこちらで管理する
		true	// bAutoActivate
	);

	if ( TrailVFX )
	{
		TrailVFX->SetFloatParameter( TEXT( "LifeTime" ), TrailHoldLifeTime );
		TrailVFX->SetColorParameter( TEXT( "Color" ), TrailDefaultColor );
	}

	TrailColorState = ETrailColorState::Idle;
	TrailColorTimer = 0.0f;
}

void USlidePassivePlayerModule::DebugSpawnTornadoInFront( bool bIsLarge, float ForwardDistance )
{
	for ( USlidePassiveSubModule* Sub : SubModules )
	{
		if ( USlidePassiveTornadoSubModule* Tornado = Cast<USlidePassiveTornadoSubModule>( Sub ) )
		{
			Tornado->DebugSpawnTornadoInFront( bIsLarge, ForwardDistance );
			return;
		}
	}
}

void USlidePassivePlayerModule::NotifyFigureCompleted()
{
	const UTidePlayerParamDataAsset* Params = OwnerCharacter ? OwnerCharacter->PlayerParamData : nullptr;
	const bool bDeferToRelease = Params && Params->bSlidePassiveDeferOnRelease;

	if ( bDeferToRelease )
	{
		// 完成演出は出さずリボンを継続させ、解除時にまとめて出すため予約する
		// （この遅延仕様は竜巻・突風など SlidePassive 全体に共通）
		bPendingReleaseFlash = true;

		if ( UseNewTrailColorScheme() )
		{
			BeginTrailCompletionRed();	// 完成した瞬間に赤へ。解除まで持続
		}
		else
		{
			// 明るく光らせてから通常色と半分の明るさを行き来してチカチカさせる。
			// 消えも再スポーンもせずリボンは継続するので、印表示と併せて完成が分かる
			BeginTrailCompletionGlow();
		}
	}
	else
	{
		BeginTrailFlash();	// 通常モードは完成ごとに即フラッシュ（インターバル込み）
	}
}

void USlidePassivePlayerModule::BeginTrailFlash()
{
	if ( !TrailVFX ) return;

	TrailColorState = ETrailColorState::FlashToRed;
	TrailColorTimer = 0.0f;
}

void USlidePassivePlayerModule::BeginTrailFadeOut()
{
	// User.LifeTime を下げて尾（古い側）から OUT→kill で畳んで消す
	StartTrailEndFade();
}

void USlidePassivePlayerModule::BeginTrailCompletionGlow()
{
	if ( !TrailVFX ) return;

	// 解除のまとめ演出中は上書きしない（見せ場が優先）
	if ( IsTrailCompletionLocked() )
	{
		return;
	}

	TrailColorState = ETrailColorState::CompletionBrighten;
	TrailColorTimer = 0.0f;
}

void USlidePassivePlayerModule::BeginTrailCompletionRed()
{
	if ( !TrailVFX ) return;

	// 複数図形を連続完成しても赤を保つ
	if ( TrailColorState == ETrailColorState::NewToRed || TrailColorState == ETrailColorState::NewHoldRed )
	{
		return;
	}

	TrailColorState = ETrailColorState::NewToRed;
	TrailColorTimer = 0.0f;
}

void USlidePassivePlayerModule::BeginTrailReleaseWhiteFade()
{
	if ( !TrailVFX ) return;

	// bIsTracking は既に false なので、フェード後は再スポーンせず破棄される
	TrailColorState = ETrailColorState::NewToWhite;
	TrailColorTimer = 0.0f;
}

bool USlidePassivePlayerModule::IsTrailCompletionLocked() const
{
	return TrailColorState == ETrailColorState::FlashToRed
		|| TrailColorState == ETrailColorState::FlashReturn
		|| TrailColorState == ETrailColorState::CompletionCooldown;
}

float USlidePassivePlayerModule::GetTrailCompletionCooldownTime() const
{
	const UTidePlayerParamDataAsset* Params = OwnerCharacter ? OwnerCharacter->PlayerParamData : nullptr;
	return Params ? FMath::Max( 0.0f, Params->SlidePassiveTrailCompletionCooldown ) : 0.0f;
}

void USlidePassivePlayerModule::BeginTrailCompletionCooldown()
{
	TrailColorState = ETrailColorState::CompletionCooldown;
	TrailColorTimer = 0.0f;

	// 0 なら間を置かず再スポーン
	if ( GetTrailCompletionCooldownTime() <= 0.0f )
	{
		EndTrailCompletionCooldown();
	}
}

void USlidePassivePlayerModule::EndTrailCompletionCooldown()
{
	TrailColorState = ETrailColorState::Idle;
	TrailColorTimer = 0.0f;

	// クールダウン中にスライドが終わっていたら再スポーンしない
	if ( !bIsTracking ) return;

	ResetSampling();
	NotifySubModulesTrajectoryReset();
	SpawnTrailVFX();
}

void USlidePassivePlayerModule::UpdateTrailVFX( float DeltaTime )
{
	// 切り離した旧トレイルは現行トレイルの有無に関わらず消し進める
	UpdateFadingTrailVFX( DeltaTime );

	// 完成後のトレイル非表示クールダウン。この間 TrailVFX は無いので下の TrailVFX ガードより先に進める
	if ( TrailColorState == ETrailColorState::CompletionCooldown )
	{
		TrailColorTimer += DeltaTime;
		if ( TrailColorTimer >= GetTrailCompletionCooldownTime() )
		{
			EndTrailCompletionCooldown();
		}
		return;
	}

	if ( !TrailVFX ) return;

	TrailColorTimer += DeltaTime;

	switch ( TrailColorState )
	{
	case ETrailColorState::FlashToRed:
	{
		const float T = FMath::Clamp( TrailColorTimer / TrailFlashToRedDuration, 0.0f, 1.0f );
		SetTrailColor( FLinearColor::LerpUsingHSV( TrailDefaultColor, FLinearColor::Red, T ) );
		if ( TrailColorTimer >= TrailFlashToRedDuration )
		{
			TrailColorState = ETrailColorState::FlashReturn;
			TrailColorTimer = 0.0f;
		}
		break;
	}
	case ETrailColorState::FlashReturn:
	{
		const float T = FMath::Clamp( TrailColorTimer / TrailFlashReturnDuration, 0.0f, 1.0f );
		SetTrailColor( FLinearColor::LerpUsingHSV( FLinearColor::Red, TrailDefaultColor, T ) );
		if ( TrailColorTimer >= TrailFlashReturnDuration )
		{
			if ( bIsTracking )
			{
				// 旧トレイルは切り離してフェードアウトさせ（即破棄だとパッと消えて見える）、
				// クールダウンを挟んでから新トレイルを開始する
				DetachTrailForFadeOut();
				BeginTrailCompletionCooldown();
			}
			else
			{
				StartTrailEndFade();	// 解除（スライド終了）：尾から畳んで消す
			}
		}
		break;
	}
	case ETrailColorState::CompletionBrighten:
	{
		// RGB を上げて発光させる（Alpha は保つ）
		const float T = FMath::Clamp( TrailColorTimer / TrailCompletionBrightenDuration, 0.0f, 1.0f );
		const FLinearColor BrightColor( TrailDefaultColor.R * TrailCompletionBrightnessScale,
			TrailDefaultColor.G * TrailCompletionBrightnessScale,
			TrailDefaultColor.B * TrailCompletionBrightnessScale, 1.0f );
		FLinearColor C = FMath::Lerp( TrailDefaultColor, BrightColor, T );
		C.A = 1.0f;
		SetTrailColor( C );
		if ( TrailColorTimer >= TrailCompletionBrightenDuration )
		{
			TrailColorState = ETrailColorState::CompletionFlicker;
			TrailColorTimer = 0.0f;
		}
		break;
	}
	case ETrailColorState::CompletionFlicker:
	{
		// 通常色と「今の明るさの半分」を行き来。収束させず、解除または次の印完成まで持続する
		const float Osc = 0.5f * ( 1.0f + FMath::Sin( TrailColorTimer * TrailCompletionFlickerFrequency ) );	// 0..1
		const float HalfScale = TrailCompletionBrightnessScale * 0.5f;
		const float Factor = FMath::Lerp( 1.0f, HalfScale, Osc );
		FLinearColor C( TrailDefaultColor.R * Factor, TrailDefaultColor.G * Factor, TrailDefaultColor.B * Factor, 1.0f );
		SetTrailColor( C );
		break;
	}
	case ETrailColorState::EndFade:
	{
		// User.LifeTime は触らず（＝尾から畳まず）Color.A を 1→0 へ落とし、リボン全体を一様に薄くする
		const float A = FMath::Clamp( 1.0f - TrailColorTimer / TrailEndFadeDuration, 0.0f, 1.0f );
		FLinearColor FadeColor = TrailDefaultColor;
		FadeColor.A = A;
		SetTrailColor( FadeColor );
		if ( TrailColorTimer >= TrailEndFadeDuration )
		{
			TrailVFX->DestroyComponent();
			TrailVFX = nullptr;
			TrailColorState = ETrailColorState::Idle;
			TrailColorTimer = 0.0f;
		}
		break;
	}
	case ETrailColorState::NewToRed:
	{
		// 緑 → 赤。遷移しきったら NewHoldRed で持続
		const float T = FMath::Clamp( TrailColorTimer / TrailNewToRedDuration, 0.0f, 1.0f );
		SetTrailColor( FLinearColor::LerpUsingHSV( TrailDefaultColor, FLinearColor::Red, T ) );
		if ( TrailColorTimer >= TrailNewToRedDuration )
		{
			TrailColorState = ETrailColorState::NewHoldRed;
			TrailColorTimer = 0.0f;
		}
		break;
	}
	case ETrailColorState::NewHoldRed:
	{
		// 解除まで赤を持続。サンプリングは止めないので続けて図形を描ける
		SetTrailColor( FLinearColor::Red );
		break;
	}
	case ETrailColorState::NewToWhite:
	{
		// 赤 → 白（発光のため少し持ち上げる）
		const float T = FMath::Clamp( TrailColorTimer / TrailNewToWhiteDuration, 0.0f, 1.0f );
		const FLinearColor White( TrailNewWhiteBrightness, TrailNewWhiteBrightness, TrailNewWhiteBrightness, 1.0f );
		FLinearColor C = FMath::Lerp( FLinearColor::Red, White, T );
		C.A = 1.0f;
		SetTrailColor( C );
		if ( TrailColorTimer >= TrailNewToWhiteDuration )
		{
			TrailColorState = ETrailColorState::NewWhiteFade;
			TrailColorTimer = 0.0f;
		}
		break;
	}
	case ETrailColorState::NewWhiteFade:
	{
		// 白のまま Alpha 1→0 でフェードアウトし、完了したら破棄する
		const float A = FMath::Clamp( 1.0f - TrailColorTimer / TrailNewWhiteFadeDuration, 0.0f, 1.0f );
		FLinearColor C( TrailNewWhiteBrightness, TrailNewWhiteBrightness, TrailNewWhiteBrightness, A );
		SetTrailColor( C );
		if ( TrailColorTimer >= TrailNewWhiteFadeDuration )
		{
			TrailVFX->DestroyComponent();
			TrailVFX = nullptr;
			TrailColorState = ETrailColorState::Idle;
			TrailColorTimer = 0.0f;
		}
		break;
	}
	default:
		break;
	}
}

void USlidePassivePlayerModule::DetachTrailForFadeOut()
{
	if ( !TrailVFX ) return;

	// 発生だけ止める（既存セグメントは巨大 LifeTime のまま残るのでリボンは消えない）。
	// アタッチしたままだと新トレイルと重なって足元を追ってくるため、その場へ切り離す
	TrailVFX->Deactivate();
	TrailVFX->DetachFromComponent( FDetachmentTransformRules::KeepWorldTransform );

	FSlidePassiveFadingTrail& Fading = FadingTrails.AddDefaulted_GetRef();
	Fading.VFX = TrailVFX;
	Fading.Timer = 0.0f;

	TrailVFX = nullptr;
}

void USlidePassivePlayerModule::UpdateFadingTrailVFX( float DeltaTime )
{
	for ( int32 Index = FadingTrails.Num() - 1; Index >= 0; --Index )
	{
		FSlidePassiveFadingTrail& Fading = FadingTrails[Index];
		if ( !Fading.VFX )
		{
			FadingTrails.RemoveAt( Index );
			continue;
		}

		Fading.Timer += DeltaTime;

		FLinearColor C = TrailDefaultColor;
		C.A = FMath::Clamp( 1.0f - Fading.Timer / TrailEndFadeDuration, 0.0f, 1.0f );
		Fading.VFX->SetColorParameter( TEXT( "Color" ), C );

		if ( Fading.Timer >= TrailEndFadeDuration )
		{
			Fading.VFX->DestroyComponent();
			FadingTrails.RemoveAt( Index );
		}
	}
}

void USlidePassivePlayerModule::StartTrailEndFade()
{
	if ( !TrailVFX )
	{
		TrailColorState = ETrailColorState::Idle;
		TrailColorTimer = 0.0f;
		return;
	}

	// User.LifeTime は巨大固定のまま（尾から畳まない）。EndFade で Color.A を落として全体を薄くする
	TrailColorState = ETrailColorState::EndFade;
	TrailColorTimer = 0.0f;
}

void USlidePassivePlayerModule::SetTrailColor( FLinearColor Color )
{
	if ( TrailVFX )
	{
		TrailVFX->SetColorParameter( TEXT( "Color" ), Color );
	}
}

#if !UE_BUILD_SHIPPING
void USlidePassivePlayerModule::DrawDebugImGui()
{
	ImGui::Indent();

	ImGui::Text( "記録中: %s", bIsTracking ? "Yes" : "No" );
	ImGui::Text( "チャージダッシュ猶予中: %s", bInChargeDashGrace ? "Yes（突風は成立しない）" : "No" );

	{
		const char* StateName = "Idle";
		switch ( TrailColorState )
		{
		case ETrailColorState::FlashToRed:         StateName = "FlashToRed"; break;
		case ETrailColorState::FlashReturn:        StateName = "FlashReturn"; break;
		case ETrailColorState::CompletionCooldown: StateName = "CompletionCooldown(非表示)"; break;
		case ETrailColorState::CompletionBrighten: StateName = "CompletionBrighten"; break;
		case ETrailColorState::CompletionFlicker:  StateName = "CompletionFlicker"; break;
		case ETrailColorState::EndFade:            StateName = "EndFade"; break;
		case ETrailColorState::NewToRed:           StateName = "NewToRed(緑→赤)"; break;
		case ETrailColorState::NewHoldRed:         StateName = "NewHoldRed(赤持続)"; break;
		case ETrailColorState::NewToWhite:         StateName = "NewToWhite(赤→白)"; break;
		case ETrailColorState::NewWhiteFade:       StateName = "NewWhiteFade(白フェード)"; break;
		default: break;
		}
		ImGui::Text( "新色演出(緑→赤→白): %s", UseNewTrailColorScheme() ? "ON" : "OFF" );
		ImGui::Text( "トレイル色状態: %s", StateName );
	}

	for ( const TObjectPtr<USlidePassiveSubModule>& Sub : SubModules )
	{
		if ( Sub ) Sub->DrawDebugImGui();
	}

	ImGui::Unindent();

	// サブモジュールが下に増えてもパネル下端で見切れないように余白を確保する
	ImGui::Dummy( ImVec2( 0.0f, 24.0f ) );
}
#endif
