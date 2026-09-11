// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PRJ_TIDE_P0/AI/States/EnemyAIStateBase.h"
#include "EnemyAIState_React.generated.h"

/**
 * 被弾リアクション・ガード・崖落下など「行動できない」状態。
 * BrainComponent::StopLogic / RestartLogicの置き換え。
 *
 * この状態は何もしない。リアクションの再生自体はUHitReactionComponentや
 * 各所のモンタージュ再生が持っており、ここは「その間AIが動かない」ことだけを表す。
 *
 * 入場条件はAEnemyCharacter::IsReacting() ただ1つ。
 * BT時代のように9種の理由が1つのスイッチを奪い合うことがないので、
 * 「再開が保証される場合だけ止める」ための事前予測 (WillReact) も要らない。
 */
UCLASS()
class PRJ_TIDE_P0_API UEnemyAIState_React : public UEnemyAIStateBase
{
	GENERATED_BODY()

public:

	virtual void Enter(UEnemyBrainComponent& Brain) override;

	// 6種類の理由が1つの状態に畳まれているので、どれで止まっているかを出す
	virtual FString GetDebugText() const override;

private:

	UPROPERTY(Transient)
	TObjectPtr<UEnemyBrainComponent> OwningBrain = nullptr;

};
