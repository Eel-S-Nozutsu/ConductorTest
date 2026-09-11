// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "IGimmickResettable.generated.h"

UINTERFACE( MinimalAPI, Blueprintable )
class UGimmickResettable : public UInterface
{
	GENERATED_BODY()
};

/**
 * やり直しで初期状態へ戻すギミックが実装するインターフェース。
 * AGimmickRoomVolumeがルームの出入りでまとめて呼ぶ。
 *
 * 「何が初期状態か」「戻すのに何が要るか」はギミック側の都合なので、通知だけを渡す。
 *
 * 他のインターフェース(IWindAffectable等)と違いBlueprintNativeEventにしているのは、
 * ルーム内のギミックがBP実装(BP_BoostGimmick等)でも参加できるようにするため。
 * 呼び出しはIGimmickResettable::Execute_ResetGimmick()経由（Cast<>ではBP実装を拾えない）。
 */
class IGimmickResettable
{
	GENERATED_BODY()

public:

	// 初期状態へ戻す ※既に初期状態なら何もしない(再構築のヒッチを避ける)
	UFUNCTION( BlueprintNativeEvent, Category = "Tide|Gimmick" )
	void ResetGimmick();
};
