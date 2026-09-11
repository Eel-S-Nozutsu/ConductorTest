// Copyright (c) 2026, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.
#include "FinisherPlayerModule.h"

#include "Misc/App.h"
#include "Kismet/GameplayStatics.h"

#include "PRJ_TIDE_P0/Actors/Character/Player/TidePlayerCharacter.h"
#include "PRJ_TIDE_P0/Data/Player/TidePlayerParamDataAsset.h"
#include "PRJ_TIDE_P0/Utilities/HitStopUtils.h"

void UFinisherPlayerModule::OnModuleUpdate( float DeltaTime )
{
	if ( !bIsActive ) return;

	// スロー中は DeltaTime がダイレーションの影響を受けるため、実時間（FApp::GetDeltaTime）で減算する
	RemainingRealTime -= FApp::GetDeltaTime();
	if ( RemainingRealTime <= 0.0f )
	{
		ResetTimeDilation();
	}
}

void UFinisherPlayerModule::TriggerFinisher()
{
	if ( !OwnerCharacter || !OwnerCharacter->PlayerParamData ) return;

	// 神技演出中はグローバルタイムダイレーションを取り合うため、とどめスローは抑制する
	if ( OwnerCharacter->IsGodActionActive() ) return;

	UWorld* World = GetWorld();
	if ( !World ) return;

	const UTidePlayerParamDataAsset* Params = OwnerCharacter->PlayerParamData;
	const float GlobalDilation = Params->FinisherGlobalTimeDilation;
	const float PlayerScale = Params->FinisherPlayerTimeScale;
	const float Duration = Params->FinisherSlowMotionDuration;

	// ワールド全体を遅くする
	UGameplayStatics::SetGlobalTimeDilation( World, GlobalDilation );

	// プレイヤーだけ等速側へ補正（ゼロ除算防止）
	if ( GlobalDilation > 0.0f )
	{
		OwnerCharacter->SetCustomTimeDilation( PlayerScale / GlobalDilation );
	}

	// 連続キル時は残り時間をリフレッシュするだけ
	RemainingRealTime = Duration;
	bIsActive = true;
}

void UFinisherPlayerModule::ResetTimeDilation()
{
	if ( UWorld* World = GetWorld() )
	{
		UGameplayStatics::SetGlobalTimeDilation( World, 1.0f );
	}

	if ( OwnerCharacter )
	{
		// 攻撃ヒットストップは復帰先としてフィニッシュ補正値（=高速）を記録しているため、後から復帰すると等速に戻らない。
		// 先に解除（保留タイマーも破棄）してから 1.0 を設定し、確実に等速へ戻す
		HitStopUtil::CancelHitStop( OwnerCharacter );
		OwnerCharacter->SetCustomTimeDilation( 1.0f );
	}

	bIsActive = false;
	RemainingRealTime = 0.0f;
}
