// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RestartComponent.generated.h"

class ATidePlayerCharacter;

/**
 * 進行制御 (P0仮実装): リスタート地点の管理と、死亡時の復帰処理を行うコンポーネント。
 *
 * - ARestartTriggerVolumeが進入時にSetRestartPoint() を呼び、最後に入ったトリガーの
 *   地点を現在のリスタート地点として保持する。
 * - 死亡担当が死亡時にRequestRestart() を呼ぶと、暗転 → リスタート地点へテレポート →
 *   HP全回復 → フェードイン を行う。
 *
 * リスタート地点は2層で持つ。本線のチェックポイント(トリガー・ProgressOrderで前進のみ)と、
 * その上に載る一時的な上書き(AGimmickRoomVolume等の局所スコープ)。
 * 上書きを本線に混ぜないのは、ギミックルームのような寄り道の地点をProgressOrderに載せると
 * 「前進のみ」の規則で退室後も本線の復帰点が戻らなくなるため。
 */
UCLASS()
class PRJ_TIDE_P0_API URestartComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	URestartComponent();

	virtual void BeginPlay() override;

	// リスタート地点を更新 ※ARestartTriggerVolumeの進入時に呼ばれる
	// InOrderが現在の進行順より小さい(古い)場合は更新しない = 前進のみ
	void SetRestartPoint(const FVector& InLocation, const FRotator& InRotation, int32 InOrder);

	// 一時的なリスタート地点を上書きする ※在室中だけ有効にしたいギミックルーム等が使う
	// InScopeは上書きの持ち主。破棄・ストリームアウトで上書きも自動失効する
	void PushRestartOverride(const FVector& InLocation, const FRotator& InRotation, const AActor* InScope);

	// InScopeが積んだ上書きを解除して本線のチェックポイントへ戻す
	// 別スコープが上書きを持っている場合は何もしない (退室順の入れ違いで他室の地点を消さないため)
	void ClearRestartOverride(const AActor* InScope);

	// 【死亡担当向け 接続点】死亡時にこれを呼ぶ
	// 暗転 → 現在のリスタート地点へテレポート → HP全回復 → フェードイン を実行する
	UFUNCTION(BlueprintCallable, Category = "Restart")
	void RequestRestart();

private:

	void ExecuteRestart();

	// 暗転(フェードアウト)にかける秒数
	UPROPERTY(EditAnywhere, Category = "Restart", meta = (ClampMin = "0.0"))
	float FadeOutDuration = 0.4f;

	// テレポート後のフェードインにかける秒数
	UPROPERTY(EditAnywhere, Category = "Restart", meta = (ClampMin = "0.0"))
	float FadeInDuration = 0.4f;

	// 現在のリスタート地点。初期値はBeginPlay時のスポーン地点
	FVector RestartLocation = FVector::ZeroVector;
	FRotator RestartRotation = FRotator::ZeroRotator;

	// 現在採用しているリスタート地点の進行順。スポーン地点は最小値にして
	// どのトリガー(どんなProgressOrderでも)でも上書きできるようにする
	int32 CurrentOrder = MIN_int32;

	// 一時的な上書きのリスタート地点。有効な間はチェックポイントより優先する
	FVector OverrideLocation = FVector::ZeroVector;
	FRotator OverrideRotation = FRotator::ZeroRotator;

	// 上書きを積んだスコープ。有効性そのものが上書きの有無を兼ねるので、
	// スコープ側の破棄・ストリームアウトで上書きが取り残されることがない
	TWeakObjectPtr<const AActor> OverrideScope;

	bool bIsRestarting = false;
	FTimerHandle RestartTimerHandle;

};
