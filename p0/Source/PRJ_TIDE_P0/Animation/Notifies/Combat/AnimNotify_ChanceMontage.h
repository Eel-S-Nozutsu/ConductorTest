// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_ChanceMontage.generated.h"

class UAnimMontage;

/**
 * 指定フレームで確率抽選し、成功したらMontageを再生する汎用Notify。
 * 同スロットで再生中のモンタージュを中断して差し替える用途。
 * 例: のけぞり後ろ(AS_stagger_b)の特定Fで確率で振り向き(AS_stagger_b_turn)へ遷移する。
 *
 * AI停止/再開はモンタージュ側の終了コールバックに委ねる。中断された元モンタージュは
 * bInterrupted扱いでAI再開せず、遷移先の自然終了で再開される
 * (AEnemyCharacter::OnReactionMontageEnded)。差し替えたいのでMontageは元と同スロットにすること。
 */
UCLASS(meta = (DisplayName = "Chance Montage", Category = "Combat"))
class PRJ_TIDE_P0_API UAnimNotify_ChanceMontage : public UAnimNotify
{
	GENERATED_BODY()

public:

	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;

	// 抽選に成功したとき再生するモンタージュ ※元と同スロットに置くこと
	UPROPERTY(EditAnywhere)
	TObjectPtr<UAnimMontage> Montage;

	// 再生する確率 [0,1]
	UPROPERTY(EditAnywhere, meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Chance = 0.5f;

	// 再生レート
	UPROPERTY(EditAnywhere, meta = (ClampMin = "0.0"))
	float PlayRate = 1.0f;

};
