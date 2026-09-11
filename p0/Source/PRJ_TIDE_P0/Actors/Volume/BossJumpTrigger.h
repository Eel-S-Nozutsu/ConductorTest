// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/TriggerVolume.h"
#include "BossJumpTrigger.generated.h"

class ABossCharacter;

/**
 * Ph1足場ジャンプ用のトリガーボリューム。
 * PCがこのボリュームに進入すると、ボスへTargetPlatformへの足場ジャンプをキューする。
 * 坂の足場ごとに1つ手置きし、各トリガーのTargetPlatformに「次の足場アンカー」を割り当てる。
 */
UCLASS()
class PRJ_TIDE_P0_API ABossJumpTrigger : public ATriggerVolume
{
	GENERATED_BODY()

public:

	ABossJumpTrigger();

	virtual void Tick(float DeltaSeconds) override;

protected:

	virtual void NotifyActorBeginOverlap(AActor* OtherActor) override;

	// このトリガー通過時にボスが着地する足場アンカー (ATargetPoint等を割り当てる)
	UPROPERTY(EditInstanceOnly, Category = "Tide|BossJump")
	TObjectPtr<AActor> TargetPlatform = nullptr;

	// ジャンプを依頼するボス ※未設定なら初回オーバーラップ時にレベルから自動取得
	UPROPERTY(EditInstanceOnly, Category = "Tide|BossJump")
	TObjectPtr<ABossCharacter> Boss = nullptr;

	// 一度発火したら無効化する (PCの出入りによる多重発火を防ぐ)
	UPROPERTY(EditAnywhere, Category = "Tide|BossJump")
	bool bConsumeOnce = true;

	// PIE中にボリューム範囲とターゲット線をデバッグ描画する
	UPROPERTY(EditAnywhere, Category = "Tide|BossJump|Debug")
	bool bDrawDebug = true;

private:

	bool bConsumed = false;

};
