// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

class AEnemyCharacter;
struct FDamageInfo;

namespace TideCombatUtil
{
	/**
	 * AttackerがTargetと敵対関係にあるか返す。
	 * どちらかがNoTeamの場合はtrue (当てる) として扱う。
	 */
	bool IsHostileTo(AActor* Attacker, AActor* Target);

	/**
	 * このダメージが神技 (UGodActionPlayerModule) 由来か返す。
	 * Instigatorが神技の発動専有中のPC (ATidePlayerCharacter::IsGodActionExecuting) かで判定する。
	 * 構え選択中(通常アクション自由)は対象外＝構え中の通常攻撃を神技ダメージに誤分類しない。
	 * 傘モジュールなので今後追加される神技もまとめて対象になる。
	 */
	bool IsGodActionDamage(const FDamageInfo& DamageInfo);

	/**
	 * このダメージが光輪を無条件で一撃破壊するか返す。
	 * 神技 (UGodActionPlayerModule) と、bBreaksHaloUnconditionallyを立てたスライドパッシブ
	 * (突風・竜巻) が該当する。ギア・背後条件・ヒビ状態・通常SAを無視して破壊する。
	 * 完全無敵 (SuperArmor_Invincible) だけは意図的な無敵フレームなので尊重する (呼び出し側で判定)。
	 */
	bool IsHaloBreakerDamage(const FDamageInfo& DamageInfo);

	// 等速直線で移動する標的への迎撃時間を解く
	// 発射点から見た相対位置Dと標的速度V、弾速Sに対し、
	// |D + V t|=Stを満たす最小の正のtを返す。解が無ければ-1
	float SolveInterceptTime(const FVector& D, const FVector& V, float S);

	// プロト: 実行中の攻撃エントリ(FAttackEntry::Damage)を別アクタ駆動の弾へ流す
	// CurrentAttackDamageが負 (攻撃非実行中/未設定)のときは弾BPの既定値を尊重する
	void InjectAttackDamage(AEnemyCharacter* Enemy, float& OutDamage);

	// 実行中の攻撃エントリ(FAttackEntry::HitReactionTag)を弾
	// /ハザードのリアクションタグへ流す。タグが空 (攻撃非実行中/未設定)
	// のときは弾・ハザード自前の既定タグを尊重する
	void InjectAttackHitReactionTag(AEnemyCharacter* Enemy, FGameplayTag& OutTag);

	// Enemyのターゲット ※なければnullptr
	AActor* GetTarget(AEnemyCharacter* Enemy);

}
