// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "FlameStreamHazard.generated.h"

class UNiagaraComponent;
class UNiagaraSystem;

/**
 * 火炎放射のボックス(直線ビーム)ハザード。
 *
 * 範囲内の敵対対象(IDamageable)へ、一定間隔(DamageInterval)で通常ダメージを与えるビーム。
 * 判定形状は球や円錐ではなく「原点から前方へ伸びる直方体(幅/高さ一定のビーム)」。円錐だと遠方ほど左右に広がり、
 * 横避け/ジャンプ避けが成立しにくいため、拡散しない一定幅のボックスにしている。
 * マズルソケットへアタッチして使い、原点はソケットに追従する。方向はソケット任せ
 * (bKeepHorizontalのyaw平坦化)に加え、SetExternalYawで外部(UFlamethrowerAttackExecution)
 * からワールドヨーを直接制御できる。
 *
 * 寿命は自己破棄ではなく外部(UFlamethrowerAttackExecution)がActivate/Deactivateで制御する。
 * Niagara未設定の間はTideGameSettingsのbDebugDrawAttackHitbox(攻撃判定表示)でボックスを可視化する。
 *
 * Ownerはスポーンしたソース(敵味方判定に使う)。汎用ハザード枠 [Docs/AreaHazard.md] の一部。
 */
UCLASS()
class PRJ_TIDE_P0_API AFlameStreamHazard : public AActor
{
	GENERATED_BODY()

public:

	AFlameStreamHazard();

	// 判定開始(Tick + VFX ON)。噴射ループ開始時に呼ぶ
	void Activate();

	// 判定停止(Tick + VFX OFF、アクタは残す)。噴射ループ終了時に呼ぶ
	void Deactivate();

	// 火炎のワールドヨーを外部から直接指定する(roll/pitchは常に0)
	// 呼ばれた以降、ソケットの向きは無視してこのヨーを毎Tick適用する(位置はソケット追従のまま)
	void SetExternalYaw(float InWorldYaw);

	// ---- 到達距離 ----「伸びきった長さ」と「いまの判定の長さ」は別物なので、
	// 書き込み口を分けている。見た目(マテリアルのLength)は前者だけを見る:
	// 炎の絵は最初から伸びきった長さで出し、当たり判定だけが予兆として後から追いつく

	// 伸びきったときの到達距離を決める。見た目もここで更新される
	// 予兆の伸びを使わないなら、これ1本で判定も見た目も決まる
	void SetFinalRange(float InRange);

	// いまの判定の到達距離だけを更新する。予兆で伸ばす途中経過を渡す用で、見た目には流さない
	void SetRange(float InRange);

	float GetRange() const { return Range; }
	float GetFinalRange() const { return FinalRange; }

protected:

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	// ボックス内の敵対対象へ一定間隔で通常ダメージを与える
	void DamageTargetsInBox();

	// 判定/デバッグ描画で使うボックスの中心・半径(HalfExtent)・回転を求める
	// (原点から前方Rangeを長さ、Width/Heightを全幅/全高とする水平ボックス)
	void GetBoxGeometry(FVector& OutCenter, FVector& OutHalfExtent, FQuat& OutRot) const;

	// FinalRangeをNiagaraのユーザーパラメータへ流す
	// 流すのは伸びきりの長さだけなので、予兆で伸びている間は呼ばない
	void UpdateVFXLength();

public:

	UPROPERTY(VisibleAnywhere, Category = "Tide|FlameStream")
	TObjectPtr<USceneComponent> SceneRoot;

	// 火炎の見た目 ※今は未設定でよい
	UPROPERTY(VisibleAnywhere, Category = "Tide|FlameStream")
	TObjectPtr<UNiagaraComponent> StreamVFX;

	// ボックスの全幅。狭いほど横に避けやすい
	UPROPERTY(EditAnywhere, Category = "Tide|FlameStream", meta = (ClampMin = "0.0"))
	float Width = 150.0f;

	// ボックスの全高。低いほどジャンプで飛び越えて避けやすい
	UPROPERTY(EditAnywhere, Category = "Tide|FlameStream", meta = (ClampMin = "0.0"))
	float Height = 200.0f;

	// ---- 見た目(Niagara → マテリアル)----

	// 到達距離を渡すNiagaraのユーザーパラメータ名
	// NS側でこのfloatをマテリアルのLengthへ配線する
	// (NiagaraSystemにUser.<この名前> が必要)
	UPROPERTY(EditAnywhere, Category = "Tide|FlameStream|VFX")
	FName VFXLengthParamName = FName("Length");

	// 判定のRangeを見た目のLengthに換算する係数
	// 判定1000に対して見た目850で長さが一致するので既定0.85
	// 判定と見た目でメッシュの原点・先端の減衰が違うぶんを吸収する
	UPROPERTY(EditAnywhere, Category = "Tide|FlameStream|VFX", meta = (ClampMin = "0.0"))
	float VFXLengthScale = 0.85f;

	// ONで毎フレーム自分の向きをyawのみに平坦化する(roll無し・pitch水平固定)
	// ソケットにアタッチしていてもボーンのpitch/rollを無視し、照準の左右
	// (yaw)だけ追従させる
	UPROPERTY(EditAnywhere, Category = "Tide|FlameStream")
	bool bKeepHorizontal = true;

	// ---- ダメージ(一定間隔で通常ダメージを与える)----

	// 1ヒットのダメージ量
	UPROPERTY(EditAnywhere, Category = "Tide|FlameStream|Damage")
	float Damage = 3.0f;

	// 同じ対象へ連続ヒットさせる間隔秒数
	UPROPERTY(EditAnywhere, Category = "Tide|FlameStream|Damage")
	float DamageInterval = 1.0f;

	// ヒットリアクション種別(のけぞらせたくない場合は空)
	UPROPERTY(EditAnywhere, Category = "Tide|FlameStream|Damage")
	FGameplayTag HitReactionTag;

	// ヒットごとに対象位置へ再生するヒットエフェクト(未設定なら出さない)
	UPROPERTY(EditAnywhere, Category = "Tide|FlameStream|Damage")
	TObjectPtr<UNiagaraSystem> HitEffect;

	// ヒットごとにヒットストップをかけるか(プレイヤーのみ適用)
	UPROPERTY(EditAnywhere, Category = "Tide|FlameStream|Damage")
	bool bUseHitStop = false;

	// ヒットストップ持続秒数
	UPROPERTY(EditAnywhere, Category = "Tide|FlameStream|Damage", meta = (EditCondition = "bUseHitStop"))
	float HitStopDuration = 0.05f;

	// ヒットストップ時のTimeDilation(0で停止)
	UPROPERTY(EditAnywhere, Category = "Tide|FlameStream|Damage", meta = (EditCondition = "bUseHitStop", ClampMin = "0.0", ClampMax = "1.0"))
	float HitStopDilation = 0.1f;

private:

	// ボックスの長さ(原点から前方への到達距離)。実行中は予兆の伸びで変動するため、
	// 書き込みはSetRange/SetFinalRange経由に限る(直接代入すると見た目と食い違う)
	UPROPERTY(EditAnywhere, Category = "Tide|FlameStream")
	float Range = 1000.0f;

	// 伸びきったときの到達距離。見た目のLengthはこの値から決まる
	// BeginPlayでRangeと揃える
	float FinalRange = 1000.0f;

	bool bActive = false;

	// SetExternalYawで指定された外部ヨー。有効な間はソケットの向きを無視する
	bool bHasExternalYaw = false;
	float ExternalYaw = 0.0f;

	// 最後にNiagaraへ流したLength。同じ値を書き直さないための控え
	float PushedVFXLength = -1.0f;

	// 対象ごとの次回ヒット許可時刻(ワールド秒)。DamageIntervalで連続ヒットを律速する
	// Activateでクリア
	TMap<TWeakObjectPtr<AActor>, double> NextHitTimes;

};
