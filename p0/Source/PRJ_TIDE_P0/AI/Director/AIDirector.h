// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "AIDirector.generated.h"

class AEnemyCharacter;

/**
 * 全敵が共有するグローバル調停役
 * - AttackTokenプールによる同時攻撃数の制限
 * - EQSスロット占有情報の提供
 */
UCLASS()
class PRJ_TIDE_P0_API UAIDirector : public UWorldSubsystem
{
	GENERATED_BODY()

public:

	// --- AttackToken ---

	// 攻撃したい意思を距離つきで登録する (TryPickAttackから毎tick呼ぶ)
	// トークンは先着順ではなく、入札した敵のうち最もターゲットに近い敵に与えられる
	void SubmitAttackBid(AEnemyCharacter* Bidder, float DistToTarget);

	// トークン取得を試みる。同時攻撃数の上限に達している場合と、より近い入札者がいる場合はfalse
	// HoldTimeが正ならその秒数で自動解放
	// (FAttackEntry::TokenHoldTime)。-1は攻撃終了まで保持
	bool TryAcquireAttackToken(AEnemyCharacter* Requester, float HoldTime = -1.0f);

	// トークンを返却 (RecordAttackEnd経由で呼ぶ)
	void ReleaseAttackToken(AEnemyCharacter* Requester);

	// 取得可能かどうかのペーク (状態変更なし。TryPickAttack用)
	bool HasTokenAvailable(const AEnemyCharacter* Requester) const;

	// 有効な入札者の中でRequesterが最もターゲットに近いか。入札していない場合はfalse
	bool IsClosestBidder(const AEnemyCharacter* Requester) const;

	// --- 戦闘参加者 ---

	// 戦闘ループへの参加を申告する (UEnemyBrainComponentが毎ティック同期する)
	// 「いまPCと戦っている敵」の唯一の真実源。重複登録は無害
	void RegisterCombatant(AEnemyCharacter* Enemy);

	// 戦闘ループからの離脱を申告する (ターゲット喪失・死亡・破棄)
	void UnregisterCombatant(AEnemyCharacter* Enemy);

	// --- スロット占有 ---

	// EQSで選んだ移動先を登録 (UEnemyAIState_Strafeから呼ぶ)
	void RegisterDesiredPosition(AEnemyCharacter* Enemy, FVector Position);

	// 登録済みの移動先を解除 (帰還/非戦闘遷移時に呼ぶ)
	void UnregisterDesiredPosition(AEnemyCharacter* Enemy);

	// 指定地点の占有スコアを返す (0=完全占有, 1=空き)
	// Radius>0でその半径を使う。0以下なら既定のSlotRadiusを使う
	float GetOccupancyScore(FVector Position, AEnemyCharacter* Requester, float Radius = -1.0f) const;

	// --- レーン調停 ---

	// Requester以外の戦闘中の敵が「最終的にいる場所」を返す。EQSの角度テストが参照する
	// 移動中の敵は現在位置ではなくDesiredPositionを返す
	// (数秒後に鉢合わせる方位を選ばないため)。攻撃中の敵は移動しないので現在位置を返す
	TArray<FVector> GetLaneAnchors(const AEnemyCharacter* Requester) const;

	// Requester以外の戦闘ループ中の敵の「現在位置」を返す。EQSの近接テストが参照する
	// GetLaneAnchorsと違い移動先ではなく実位置なので、点密度に依らず団子を避けられる
	TArray<FVector> GetCombatantLocations(const AEnemyCharacter* Requester) const;

	// Requester以外の戦闘参加者を返す (現在位置版の実体側)。協調攻撃の募集元
	TArray<AEnemyCharacter*> GetCombatants(const AEnemyCharacter* Requester) const;

	// --- 協調攻撃 (フォーメーション) ---

	// CoopをLeaderの協力者として予約し、担当スロット位置と着地後の向きを保持する
	// 既に別リーダーに予約済みならfalse (取り合い防止)
	bool ReserveCoordinator(AEnemyCharacter* Coop, AEnemyCharacter* Leader, FVector SlotLocation,
		float SlotYaw = 0.0f);

	// Coopが現在いずれかのリーダーに予約されているか
	bool IsCoordinationReserved(AEnemyCharacter* Coop) const;

	// Coopの担当スロット位置を取得する。予約が無ければfalse
	bool GetCoordinationSlot(AEnemyCharacter* Coop, FVector& OutSlot) const;

	// Coopの着地後に向かせる方位(yaw)を取得する。予約が無ければfalse
	bool GetCoordinationSlotYaw(AEnemyCharacter* Coop, float& OutSlotYaw) const;

	// Coopを指名したリーダーを返す。予約が無ければnullptr
	AEnemyCharacter* GetCoordinationLeader(AEnemyCharacter* Coop) const;

	// Coopがスロットへ着地してスタンバイに入ったことを記録する
	// 同時に予約を延長する: 着地後は噴射〜後隙まで予約を保持したいが、募集時の寿命は
	// 「指名したのに動かない個体」を拾い直すための短い値なので、そのままだと途中で失効する
	void MarkCoordinatorArrived(AEnemyCharacter* Coop, float ExtraLifetime = 20.0f);

	// Leaderが予約している協力者を返す (有効な予約のみ)
	TArray<AEnemyCharacter*> GetCoordinators(const AEnemyCharacter* Leader) const;

	// Leaderの協力者が全員スタンバイに入ったか。予約が0件でもtrue
	bool AreAllCoordinatorsArrived(const AEnemyCharacter* Leader) const;

	// Coopがスタンバイに入っているか。着地しなかった個体を発射順から外すのに使う
	bool IsCoordinatorArrived(AEnemyCharacter* Coop) const;

	// スタンバイ終了時点からCoopが噴射を始めるまでの秒数。負値 = 未確定
	void SetCoordinatorFireDelay(AEnemyCharacter* Coop, float Delay);
	bool GetCoordinatorFireDelay(AEnemyCharacter* Coop, float& OutDelay) const;

	// Coopの予約を解除する (協力者攻撃の終了/中断時に呼ぶ)
	void ReleaseCoordinator(AEnemyCharacter* Coop);

	// --- デバッグ用アクセサ ---
	int32 GetMaxSimultaneousAttackers() const { return MaxSimultaneousAttackers; }
	int32 GetCurrentAttackerCount() const;
	TArray<AEnemyCharacter*> GetActiveAttackers() const;
	TArray<TPair<AEnemyCharacter*, FVector>> GetDesiredPositions() const;
	TArray<TPair<AEnemyCharacter*, float>> GetActiveBids() const;

	// 敵種/拠点ごとに振れるようにしておくと演出の幅が出る
	int32 MaxSimultaneousAttackers = 1;
	float SlotRadius = 150.0f;

	// 入札の有効期間秒数。TryPickAttackが呼ばれなくなった敵を候補から外すための足切り
	// 抽選サービスのtick間隔より十分長くする
	float BidLifetime = 1.0f;

	// 近さで競り勝つのに必要な差。ほぼ等距離の2体が毎tick入れ替わるのを防ぐ
	float BidDistanceMargin = 50.0f;

private:

	struct FAttackBid
	{
		float DistToTarget = 0.0f;
		float SubmitTime = 0.0f;

	};

	struct FActiveAttacker
	{
		TWeakObjectPtr<AEnemyCharacter> Enemy;

		// この時刻を過ぎたら枠を明け渡す。0以下なら攻撃終了まで保持し続ける
		float ExpireTime = 0.0f;

	};

	// 期限切れの入札を除去する
	void PruneBids();

	// 死亡・保持時間切れの攻撃者を枠から外す
	void PruneAttackers();

	// 破棄済みの参加者を除去する。Unregisterを通らずに消えた敵 (レベル遷移など) の後始末
	void PruneCombatants();

	// 協調攻撃の予約1件 (どのリーダーの指名か + 担当スロット位置 + 失効時刻)
	struct FCoordinationSlot
	{
		TWeakObjectPtr<AEnemyCharacter> Leader;
		FVector SlotLocation = FVector::ZeroVector;
		// この時刻を過ぎたら予約は無効。協力者が攻撃を開始しないまま放置された時の再募集漏れ対策
		float ExpireTime = 0.0f;

		// 着地後に向かせる方位。背中合わせのようにリーダーと別方向を向かせる隊形で使う
		float SlotYaw = 0.0f;

		// スロットへ着地してスタンバイに入ったか
		bool bArrived = false;

		// スタンバイ終了からこの秒数後に噴射を始める。負値 = リーダー未確定
		float FireDelay = -1.0f;

	};

	// 戦闘ループに参加中の敵。移動予約や入札からの導出ではなく明示登録なので、
	// ストレイフ中か直近に抽選を回したかといった内部事情に左右されない
	TSet<TWeakObjectPtr<AEnemyCharacter>> Combatants;

	TArray<FActiveAttacker> ActiveAttackers;
	TMap<TWeakObjectPtr<AEnemyCharacter>, FVector> DesiredPositions;
	TMap<TWeakObjectPtr<AEnemyCharacter>, FAttackBid> AttackBids;
	TMap<TWeakObjectPtr<AEnemyCharacter>, FCoordinationSlot> CoordinationReservations;

};
