// Copyright (c) 2026, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "PRJ_TIDE_P0/Composites/Player/PlayerModules/TidePlayerModule.h"
#include "LaunchActionLockPlayerModule.generated.h"

// 打ち上げ（ジャンプパッド等で LaunchCharacter された）中の「全アクション封印」モジュール。開始時に行動不能タグ
// State.Common.Disable を付与し、上昇区間のうち LaunchActionLockAscentRate ぶんが経過した時点で解除する。
// Disable は全アクションを一括で弾くが、移動（空中制御）だけは IsLaunchActionLocked() を見て例外的に許可する。
// 解除は上昇速度の割合で計る：打ち上げ直後の最大上昇速度を基準に Vz が (1 - Rate) 倍以下へ落ちた瞬間に解除する
// （Vz はほぼ線形に減るので速度割合＝時間割合。Rate=1 で頂点まで、0.5 で上昇の前半だけ封印できる）。
// 封印解除とは別に「打ち上げの上昇中か（IsAscending）」を頂点まで持つ。Rate<1 では封印が上昇の途中で解けるため、
// チャージだけはこちらを見て頂点まで抑える（上昇中に溜めへ入られると滑空・空中アクションを奪ってしまう）
UCLASS()
class PRJ_TIDE_P0_API ULaunchActionLockPlayerModule : public UTidePlayerModule
{
	GENERATED_BODY()

public:
	virtual void Initialize( ATidePlayerCharacter* InOwner ) override;
	virtual void OnModuleUpdate( float DeltaTime ) override;

	// ジャンプパッド等の打ち上げギミックから呼ばれる。封印を開始する（頂点で自動解除）
	void BeginLaunchLock();

	// 着地時の保険解除（天井衝突・頂点前着地に対応）。キャラクターの Landed() から呼ぶ
	void OnLanded();

	bool IsLocked() const { return bIsLocked; }
	// 打ち上げの上昇中か（頂点／着地／保険タイムアウトまで）。封印がレート地点で解けた後も続く
	bool IsAscending() const { return bIsAscending; }

private:
	// 封印を解除する。bExecuteDeferredCharge=true のとき、封印中に後回しへ回したチャージ発動を実行する
	// （頂点手前のレート地点／着地での解除は true、死亡での解除は false）。
	// まだ上昇中ならチャージ関連は実行せず、上昇の終わり（EndLaunchAscent）まで持ち越す
	void EndLaunchLock( bool bExecuteDeferredCharge );
	// 上昇の終わり（頂点・着地・保険タイムアウト）。持ち越したチャージ発動／長押し再チャージをここで解決する
	void EndLaunchAscent( bool bExecuteDeferredCharge );
	const class UTidePlayerParamDataAsset* GetPlayerParams() const;

private:
	bool  bIsLocked = false;
	// 打ち上げの上昇中か。封印より長く（頂点まで）持ち、チャージの抑止に使う
	bool  bIsAscending = false;
	// 上昇中に解除されたぶんのチャージ発動／再チャージを持ち越しているか
	bool  bPendingChargeResolve = false;
	// 一度でも上昇（Vz > しきい値）を観測したか。打ち上げ直前（Launch 適用前の Vz<=0）での誤解除を防ぐ
	bool  bObservedAscent = false;
	// 打ち上げ直後に観測した最大上昇速度（cm/s）。レート解除のしきい値 (1-Rate)*PeakAscentVz の基準
	float PeakAscentVz = 0.0f;
	// 保険タイムアウト用の経過時間（秒）
	float ElapsedTime = 0.0f;
};
