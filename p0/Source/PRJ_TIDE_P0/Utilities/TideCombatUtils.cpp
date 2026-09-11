// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#include "PRJ_TIDE_P0/Utilities/TideCombatUtils.h"

#include "PRJ_TIDE_P0/Actors/Character/Enemy/EnemyCharacter.h"
#include "PRJ_TIDE_P0/Actors/Character/Player/TidePlayerCharacter.h"
#include "PRJ_TIDE_P0/Data/Combat/DamageInfo.h"

#include "AIController.h"
#include "GenericTeamAgentInterface.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Controller.h"

namespace
{
	FGenericTeamId GetTeamId(AActor* Actor)
	{
		if (const APawn* Pawn = Cast<APawn>(Actor))
		{
			if (const IGenericTeamAgentInterface* Agent =
				Cast<IGenericTeamAgentInterface>(Pawn->GetController()))
			{
				return Agent->GetGenericTeamId();
			}
		}
		if (const IGenericTeamAgentInterface* Agent =
			Cast<IGenericTeamAgentInterface>(Actor))
		{
			return Agent->GetGenericTeamId();
		}
		return FGenericTeamId::NoTeam;
	}
}

namespace TideCombatUtil
{
	bool IsHostileTo(AActor* Attacker, AActor* Target)
	{
		if (!Attacker || !Target) return false;

		const FGenericTeamId AttackerTeam = GetTeamId(Attacker);
		const FGenericTeamId TargetTeam = GetTeamId(Target);

		// どちらかがNoTeamなら当てる
		if (AttackerTeam == FGenericTeamId::NoTeam || TargetTeam == FGenericTeamId::NoTeam)
			return true;

		return AttackerTeam != TargetTeam;
	}

	bool IsGodActionDamage(const FDamageInfo& DamageInfo)
	{
		// 神技の発動専有中(憑依の一閃)の攻撃だけを神技ダメージ扱いにする
		// 構え選択中は通常攻撃を自由に出せるため、IsGodActionActive(構え含む)で見ると
		// 構え中の通常攻撃まで神技ダメージ(一撃破壊・SA貫通等)に誤分類してしまう
		const ATidePlayerCharacter* PC = Cast<ATidePlayerCharacter>(DamageInfo.Instigator.Get());
		return PC && PC->IsGodActionExecuting();
	}

	bool IsHaloBreakerDamage(const FDamageInfo& DamageInfo)
	{
		return DamageInfo.bBreaksHaloUnconditionally || IsGodActionDamage(DamageInfo);
	}

	float SolveInterceptTime(const FVector& D, const FVector& V, float S)
	{
		const float a = V.SizeSquared() - S * S;
		const float b = 2.0f * FVector::DotProduct(D, V);
		const float c = D.SizeSquared();

		auto SmallestPositive = [](float x, float y) -> float
			{
				const float Lo = FMath::Min(x, y);
				const float Hi = FMath::Max(x, y);
				if (Lo > KINDA_SMALL_NUMBER) return Lo;
				if (Hi > KINDA_SMALL_NUMBER) return Hi;
				return -1.0f;
			};

		// a≈0 (標的速度≒弾速): 線形b t + c = 0に退化する
		if (FMath::Abs(a) < KINDA_SMALL_NUMBER)
		{
			if (FMath::Abs(b) < KINDA_SMALL_NUMBER) return -1.0f;
			const float t = -c / b;
			return t > KINDA_SMALL_NUMBER ? t : -1.0f;
		}

		const float Disc = b * b - 4.0f * a * c;
		if (Disc < 0.0f) return -1.0f;
		const float Sq = FMath::Sqrt(Disc);
		return SmallestPositive((-b - Sq) / (2.0f * a), (-b + Sq) / (2.0f * a));
	}

	void InjectAttackDamage(AEnemyCharacter* Enemy, float& OutDamage)
	{
		const float D = Enemy ? Enemy->GetCurrentAttackDamage() : -1.0f;
		if (D >= 0.0f) OutDamage = D;
	}

	void InjectAttackHitReactionTag(AEnemyCharacter* Enemy, FGameplayTag& OutTag)
	{
		const FGameplayTag T = Enemy ? Enemy->GetCurrentAttackHitReactionTag() : FGameplayTag();
		if (T.IsValid()) OutTag = T;
	}

	AActor* GetTarget(AEnemyCharacter* Enemy)
	{
		// ターゲットの実体はUEnemyThreatComponentが持つ
		// BBはBT運用中のミラーでしかないので、直接読むとステートマシン運用
		// (Blackboard非生成) で常にnullptrになる
		return Enemy ? Enemy->GetTargetActor() : nullptr;
	}

}
