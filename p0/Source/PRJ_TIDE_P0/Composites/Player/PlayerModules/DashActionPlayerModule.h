// Copyright (c) 2026, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "PRJ_TIDE_P0/Composites/Player/PlayerModules/TidePlayerModule.h"
#include "PRJ_TIDE_P0/Utilities/AutomaticTimer.h"

#include "DashActionPlayerModule.generated.h"

UCLASS()
class UDashActionPlayerModule : public UTidePlayerModule
{
	GENERATED_BODY()

public:
	virtual void Initialize( class ATidePlayerCharacter* InOwner ) override;
	virtual void OnModuleUpdate( float DeltaTime ) override;

	void RequestStartDash( bool bIsFromDodge = false );
	void ForceStartDash( bool bIsFromDodge = false );

	void StopDash();

	// ダッシュ中に外部（加速ギミック等）から速度を上書きされた状態から戻る際、
	// 通常ダッシュ速度への慣性減速フェーズを StartSpeed から開始する
	void StartInertiaDecayFromSpeed( float StartSpeed );

	bool IsDashing() const { return bIsDashing; }
	bool IsTurning() const;

	// 現在、旋回力ブースト期間中かどうかを判定
	bool IsInitialTurnBoostActive() const { return !InitialTurnTimer.IsFinish(); }

#if !UE_BUILD_SHIPPING
	void DrawDebugImGui();
#endif

private:
	void StartDash( bool bIsFromDodge );
	bool CanDash() const;

private:
	bool bIsDashing = false;
	FAutomaticTimer DashGraceTimer;	// ダッシュの猶予時間管理タイマー

	FAutomaticTimer DashInertiaTimer;	// 高速状態からの移行に伴う慣性減速タイマー
	float CachedInertiaStartSpeed = 0.0f;

	FAutomaticTimer InitialTurnTimer;	// ダッシュ開始時の初期旋回ブーストタイマー
};
