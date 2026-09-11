// Copyright (c) 2026, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "PRJ_TIDE_P0/Composites/Player/PlayerModules/GodActionSubModule.h"

#include "GodGuidanceSubModule.generated.h"

class ATideGodBird;

// 神技「導き」。前方の到達点（前方距離＋真下トレースで着地補正）へ向けて巨大な鳥を都度スポーンして
// 突進させ（常駐のお供とは別インスタンス）、突進中は鳥の現在地を中心に範囲ダメージを与える。
// 鳥は到達／最大持続で自壊し、ロックオンは使わない。突進の移動・寿命は ATideGodBird 側が持ち、
// 本サブモジュールは発動・範囲ダメージ・ゲージ消費・状態管理を担う
UCLASS()
class UGodGuidanceSubModule : public UGodActionSubModule
{
	GENERATED_BODY()

public:
	virtual bool IsEnabled() const override { return true; }
	virtual void OnModuleUpdate( float DeltaTime ) override;

	// ゲージ満タン確認は傘側で済ませてから呼ぶ。クラス未設定などで発動できなければゲージも消費しない
	void Execute();

	// 突進の進行中か（鳥・範囲ダメージ・ギミック配置が動いている間）
	bool IsActive() const { return bActive; }

	// 発動モーション（ST→ED）の再生中か。突進の寿命とは独立し、ED が終わると false になる。
	// 傘はこれが false になったら空中滞空を解除してプレイヤーを落下・自由操作へ戻す
	bool IsPlayerCasting() const { return bActive && !bCastEnded; }

#if !UE_BUILD_SHIPPING
	virtual void DrawDebugImGui() override;

	// 構え中に、突進で通り抜ける範囲を黄緑の Box で描画する（プレイヤー現在地→到達点を軸に、
	// 範囲ダメージ半径ぶんの太さを持つ帯）。傘から呼ぶ
	void DrawRangePreview() const;
#endif

private:
	FVector GetAimForward2D() const;	// カメラ Yaw の水平前方。2D 発射時
	FVector GetAimForward3D() const;	// カメラのピッチ＋ヨー方向そのまま。3D 発射時

	// 3D 時は照準方向そのまま／2D 時は水平前方＋真下トレースで着地補正
	FVector ComputeDestination() const;

	void ApplyChargeDamage();	// 鳥の現在地中心。同一対象は再アーム間隔ごと

	// 前回設置点から一定間隔ごとに加速ギミックを配置する（中心＋左右の3列）
	void DropBoostGimmicksAlongPath();

	// 1個ぶんのスポーン（地面への最低高さ確保も行う）。3列配置の各列で呼ぶ
	void SpawnBoostGimmickAt( class UWorld* World, const class UTidePlayerParamDataAsset* Params, FVector DropLoc, const FRotator& Rotation );

	void EndGuidance();	// 残っている鳥があれば破棄する

private:
	bool bActive = false;
	float Elapsed = 0.0f;

	TWeakObjectPtr<ATideGodBird> GuidanceBird;	// 都度スポーン。自壊で消える
	TMap<TWeakObjectPtr<AActor>, double> LastDamageTimes;	// 同一対象への再アーム管理

	FVector BoostPathOrigin = FVector::ZeroVector;	// 加速ギミック配置の原点＝発動位置
	float NextBoostDropDistance = 0.0f;	// 次に置く原点からの距離（先頭＝StartOffset、以降 Interval ごと）

	// 発動モーション（他の神技の攻撃モーションを流用。ループ無し）。
	// Execute で ATK_ST → 終わったら ATK_ED → ED が終わったら bCastEnded（プレイヤー解放）
	float MontageTimer = 0.0f;	// 現在段の残り再生時間
	bool bCastInEnd = false;
	bool bCastEnded = false;
};
