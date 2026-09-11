// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Perception/AIPerceptionTypes.h"
#include "EnemyThreatComponent.generated.h"

class UEnemyPerceptionComponent;

/**
 * ターゲット選定・記憶。TargetActorの唯一の書き手であり、実体を保持する。
 *
 * 検知ゲージ:
 *  - 未交戦のあいだ、交戦圏(OuterVolume)内にプレイヤーがいる間リニアに上昇し、
 *    満タンで交戦状態へ入る
 *  - スポナー配下は視認を条件にしない。壁越し・背後でも在圏だけで溜まる
 *    (視野に入らないと開戦しない配置事故を避ける)。手置き敵は従来どおり視認ベース
 *  - 圏外に出ている間はリニアに減衰する
 *  - 交戦中は満タンで固定。降りるのはエンカウント停止(交戦圏離脱)のみ
 *  - 速度はDAのFEnemyPerceptionSettings(ApplyDetectionSettings)で敵タイプごとに指定する
 *
 * エンカウント型のターゲット保持を採る:
 *  - 取得：スポナー配下は交戦圏内のプレイヤー、手置きは視覚で感知した候補をセット
 *    (いずれも交戦ゲートbEngagedが条件)
 *  - 保持：一度取得したら視覚喪失(物陰・扇外れ)ではクリアしない。一本道アクション向けに、
 *    戦闘に入ったらエリア内にいる限り継続する。ステルス的な最終地点捜索は行わない。
 *  - 解除：交戦圏(OuterVolume)離脱(UEnemyAIState_ReturnToOriginのClearTarget)
 *    またはエンカウント停止のみ
 *  - テリトリーフィルタ：交戦圏(半径>0)外の候補は視野内でも取得しない
 *
 * エンカウント/テリトリー (交戦フラグ・home原点・半径) も同居させる。
 */
UCLASS()
class PRJ_TIDE_P0_API UEnemyThreatComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	UEnemyThreatComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// 交戦の有効/無効。スポナーのエンカウント制御からAIController経由で、
	// および検知ゲージ満タンで自身から呼ばれる
	// trueで現在の検知状態から再導出、falseでTargetActorを即クリア(ゲージもリセット)
	void SetEngaged(bool bInEngaged);

	// 検知ゲージの速度をDAから適用する。OnPossessから呼ぶ
	void ApplyDetectionSettings(float InFillSeconds, float InDecaySeconds);

	// 検知ゲージの現在値 (0..1)。デバッグ表示用
	float GetDetectionGauge() const { return DetectionGauge; }

	// テリトリーを設定する (スポナーのスポーン時に呼ばれる)
	// 徘徊圏=InnerVolume / 交戦圏=OuterVolume。交戦圏は徘徊圏未満にはならない
	void SetTerritory(const FVector& Origin, float InPatrolRadius, float InCombatRadius);

	// 現在の検知状態からTargetActorを再導出する (帰還・再交戦で使用)
	void RefreshTargetActor();

	// TargetActorを明示的にクリアする (帰還開始時など)
	void ClearTarget();

	// 現在のターゲットを返す
	AActor* GetTargetActor() const { return TargetActor; }

	// テリトリーの中央を返す
	const FVector& GetPatrolOrigin() const { return HomeOrigin; }
	// 徘徊圏の半径を返す (0=テリトリー無し)
	float GetPatrolRadius() const { return TerritoryRadius; }
	// 交戦圏の半径を返す (0=テリトリー無し)。交戦の維持・帰還判定はこちらを使う
	float GetCombatRadius() const { return CombatRadius; }

	// 交戦ゲート。スポナー配下でなければ既定trueのまま
	bool IsEngaged() const { return bEngaged; }

	// 現在視認中のアクター数 (デバッグ表示用)
	int32 GetPerceivedActorCount() const;

private:

	UFUNCTION()
	void OnPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus);

	void SetTargetActor(AActor* InTarget);

	// 検知ゲージ更新 + ターゲット取得の定期ポーリング
	void PollDetection();

	// 取得専用の定期チェック。ターゲット未所持時のみ検知候補を拾う
	// 「視野に入りっぱなしでテリトリー境界を越える」ケースはAIPerceptionが新規イベントを出さな
	// いため、イベント経路だけでは取り逃す。
	// これを低頻度ポーリングで補完する(所持中は早期リターン)
	void TryAcquireTarget();

	// 検知ゲージの上昇/減衰。満タンで自身を交戦状態へ入れる
	void UpdateDetectionGauge(float DeltaSeconds);

	// 検知候補を1体返す (無ければnullptr)
	// スポナー配下は交戦圏内のプレイヤー、手置き敵は視認中の候補
	AActor* FindDetectionCandidate() const;

	// 視認中かつ交戦圏内の候補を1体返す (無ければnullptr)
	// 感知OFFデバッグの判定もここに集約する
	AActor* FindPerceivedActorInTerritory() const;

	// HomeOriginからRadius以内にいるプレイヤーを返す (無ければnullptr)。視認は問わない
	// 感知OFFデバッグの判定もここに集約する
	AActor* FindPlayerWithinRadius(float Radius) const;

	// スポナー配下か (テリトリーが設定されているか)
	bool HasTerritory() const { return CombatRadius > 0.0f; }

	// 候補が交戦圏内か。テリトリー未設定 (半径0=スポナー非所属) の場合は常にtrue
	// 判定を帰還判定(IsOutsidePatrolArea)と同じ交戦圏で揃えないと、
	// 境界の内外で検知↔帰還が高速に往復する
	bool IsWithinTerritory(const AActor* Actor) const;

	UPROPERTY(Transient)
	TObjectPtr<UEnemyPerceptionComponent> Perception = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<AActor> TargetActor = nullptr;

	FTimerHandle AcquireTimerHandle;

	// ポーリング間隔。ゲージの刻みもこの粒度になる (AIPerceptionの更新間隔と同程度)
	static constexpr float PollInterval = 0.1f;

	bool bEngaged = true;

	// 検知ゲージ (0..1)。交戦中は1.0固定なので、既定はbEngagedと揃えて満タン
	// (手置き敵は交戦扱いのまま動かない。スポナー配下は生成時のSetEngaged(false)で0になる)
	float DetectionGauge = 1.0f;
	// DAから注入。0で「即時検知」「減衰しない」
	float DetectionFillSeconds = 0.0f;
	float DetectionDecaySeconds = 0.0f;

	// テリトリー (スポナーがSetTerritoryで設定)。半径0=テリトリー無し
	FVector HomeOrigin = FVector::ZeroVector;
	// 徘徊圏 (InnerVolume)。非交戦時の行動範囲・帰還先のサンプル範囲
	float TerritoryRadius = 0.0f;
	// 交戦圏 (OuterVolume)。交戦中はここまで追跡でき、出たら帰還する
	float CombatRadius = 0.0f;

};
