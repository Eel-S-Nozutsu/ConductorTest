// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EnvironmentQuery/EnvQueryContext.h"
#include "EnvQueryContext_AttackTarget.generated.h"

/**
 * EQSの「攻撃対象」コンテキスト。BP_EnvQueryContext_AttackTargetのC++置き換え。
 *
 * BP版はAIControllerのBlackboardを直接読んでいたため、Blackboardを生成しない
 * ステートマシン運用ではターゲットを返せずクエリが失敗していた
 * (＝ストレイフの移動先が決まらず棒立ちになる)。
 * ターゲットの実体を持つUEnemyThreatComponentから引く形に統一する。
 */
UCLASS()
class PRJ_TIDE_P0_API UEnvQueryContext_AttackTarget : public UEnvQueryContext
{
	GENERATED_BODY()

public:

	virtual void ProvideContext(FEnvQueryInstance& QueryInstance,
		FEnvQueryContextData& ContextData) const override;

};
