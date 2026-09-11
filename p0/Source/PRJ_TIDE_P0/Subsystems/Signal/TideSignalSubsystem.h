// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "TideSignalSubsystem.generated.h"

// シグナル発火時に呼ばれる内部デリゲート(C++受信者向け)
DECLARE_MULTICAST_DELEGATE(FTideSignalDelegate);

/**
 * 名前(FName)をキーにしたレベル内シグナルバス。
 * スポナー全滅などの発火側と、扉などの受信側を直接参照させずに疎結合で繋ぐ。
 *
 * 発火済みの名前はラッチ保持され、後から登録した受信側にも取りこぼしなく通知される
 * (扉BPのBeginPlayがスポナー発火より後でも開扉できる)。
 */
UCLASS()
class PRJ_TIDE_P0_API UTideSignalSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:

	// シグナルを発火。発火済みとしてラッチ保持し、登録済みの受信側へ通知する
	UFUNCTION(BlueprintCallable, Category = "Tide|Signal")
	void BroadcastSignal(FName SignalName);

	// 発火済みシグナルをリセット(None指定で全クリア)※リスタート時などに使う
	UFUNCTION(BlueprintCallable, Category = "Tide|Signal")
	void ResetSignal(FName SignalName);

	// 指定シグナルが既に発火済みか
	UFUNCTION(BlueprintCallable, Category = "Tide|Signal")
	bool HasSignalFired(FName SignalName) const { return FiredSignals.Contains(SignalName); }

	// シグナルを購読登録。bFireIfAlreadyBroadcast=trueなら既に発火済みの場合は
	// 即座にCallbackを呼ぶ。解除には返り値のハンドルをRemoveListenerへ渡す
	FDelegateHandle ListenForSignal(FName SignalName, FTideSignalDelegate::FDelegate Callback, bool bFireIfAlreadyBroadcast = true);

	// 購読を解除する
	void RemoveListener(FName SignalName, FDelegateHandle Handle);

private:

	// 名前ごとの受信デリゲート
	TMap<FName, FTideSignalDelegate> SignalDelegates;

	// 既に発火済みの名前
	TSet<FName> FiredSignals;

};
