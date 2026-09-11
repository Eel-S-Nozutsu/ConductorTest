// Copyright (c) 2026, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "PRJ_TIDE_P0/Composites/Player/PlayerModules/TidePlayerModule.h" // ※環境に合わせてベースクラスを指定してください
#include "FallActionPlayerModule.generated.h"

struct FHitResult;

UCLASS()
class PRJ_TIDE_P0_API UFallActionPlayerModule : public UTidePlayerModule
{
	GENERATED_BODY()

public:
	virtual void Initialize( ATidePlayerCharacter* InOwner ) override;
	virtual void OnModuleUpdate( float DeltaTime ) override;

	void OnLanded( const FHitResult& Hit );

	// Debug
	void DrawDebugImGui();

private:
	void StartFallTracking();
	void StopFallTracking();
	void ProcessFallDamage();

	// 落下（下降）中に重力を徐々に強めるランプ処理。下降が続くほど GravityScale を上げて落下を加速させる
	void UpdateFallGravityRamp( float DeltaTime );
	// 重力ランプを適用してよい状態か（下降中かつ重力を自前制御する特殊アクション中でない）
	bool ShouldApplyFallGravityRamp() const;

	const class UTidePlayerParamDataAsset* GetPlayerParams() const;

private:
	bool  bIsTrackingFall;
	float MaxFallZ;

	// 重力ランプ用：下降を続けている時間（秒）と、GravityScale を書き換え中かどうか
	float FallDescentTime = 0.0f;
	bool  bGravityRampApplied = false;

	// Debug: 直近の着地判定結果（NoFallDamage タグの判定確認用）
	FString LastLandedActorName;
	FString LastLandedComponentName;
	FString LastSoftLandingSourceName;
	bool    bLastLandingWasSoft = false;
};
