// Copyright (c) 2026, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.

#include "TargetingUtils.h"

#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "GameFramework/Actor.h"

#include "PRJ_TIDE_P0/Actors/Character/Enemy/EnemyCharacter.h"
#include "PRJ_TIDE_P0/Components/Targeting/LockOnTargetComponent.h"

namespace
{
	// 扇内の候補を「正面優先（整列度がほぼ同じときだけ近い方）」で 1 つ選び続ける保持枠
	struct FFanCandidate
	{
		ULockOnTargetComponent*	Comp = nullptr;
		float					Dot = -1.0f;
		float					DistSq = 0.0f;

		void TryUpdate( ULockOnTargetComponent* InComp, float InDot, float InDistSq, float DotTolerance )
		{
			if ( Comp == nullptr )
			{
				Comp = InComp;
				Dot = InDot;
				DistSq = InDistSq;
				return;
			}

			if ( InDot > Dot + DotTolerance )	// 明確に正面寄り
			{
				Comp = InComp;
				Dot = InDot;
				DistSq = InDistSq;
			}
			else if ( InDot >= Dot - DotTolerance && InDistSq < DistSq )	// 整列度が同等なら近い方
			{
				Comp = InComp;
				Dot = FMath::Max( Dot, InDot );
				DistSq = InDistSq;
			}
		}
	};
}

namespace TargetingUtil
{
	bool IsTargetableActor( const AActor* Actor, const AActor* IgnoreActor )
	{
		if ( Actor == nullptr || Actor == IgnoreActor )
		{
			return false;
		}

		const ULockOnTargetComponent* TargetComp = Actor->FindComponentByClass<ULockOnTargetComponent>();
		return TargetComp != nullptr && TargetComp->bIsTargetable;
	}

	ULockOnTargetComponent* FindBestTargetInFan(
		const UWorld* World,
		const FVector& Origin,
		const FVector& DefaultDir,
		float MaxDist,
		float HalfAngleRad,
		const AActor* IgnoreActor,
		float ApexBackOffset,
		float MaxHeightDiff )
	{
		if ( !World )
		{
			return nullptr;
		}

		// Origin から DefaultDir の逆方向へ下げると、横にいる敵の角度が小さくなり拾いやすくなる
		// （距離・向きは Origin 基準のまま）
		const FVector ApexOrigin = Origin - DefaultDir * ApexBackOffset;

		TArray<FOverlapResult> Overlaps;
		FCollisionQueryParams Params;
		if ( IgnoreActor )
		{
			Params.AddIgnoredActor( IgnoreActor );
		}

		// チャンネル応答ではなく種別で集めるので、Pawn 応答を Ignore にしている巨大敵も漏れなく拾える。
		// 壊れ物も吸着対象にできるよう WorldDynamic も対象にする
		FCollisionObjectQueryParams ObjParams;
		ObjParams.AddObjectTypesToQuery( ECC_Pawn );
		ObjParams.AddObjectTypesToQuery( ECC_WorldDynamic );

		// 巨大敵はカプセルが狙う部位（足など）から離れており MaxDist の球ではアクター自体を取りこぼす。
		// 収集は広い球で行い、実際の吸着レンジは各部位の距離でゲートする
		constexpr float GatherRadiusMargin = 5000.0f;
		const float GatherRadius = MaxDist + GatherRadiusMargin;

		World->OverlapMultiByObjectType(
			Overlaps, Origin, FQuat::Identity, ObjParams, FCollisionShape::MakeSphere( GatherRadius ), Params );

		const float CosHalfAngle = FMath::Cos( HalfAngleRad );

		// 真後ろガードは全角90°未満のときだけ有効。それ以上は「広く・背面も拾う」意図とみなしてマスクする
		const bool bApplyBackGuard = ( HalfAngleRad < FMath::DegreesToRadians( 45.0f ) );

		// 照準方向との整列度（内積）が大きいほど優先し、この差の内なら近い方を選ぶ
		constexpr float FrontalDotTolerance = 0.02f;

		// 手前の壊れ物に吸着を奪われないよう、敵とそれ以外を別枠で選んで距離では逆転させない
		FFanCandidate BestEnemy;
		FFanCandidate BestOther;

		const float MaxDistSq = FMath::Square( MaxDist );

		for ( const FOverlapResult& Result : Overlaps )
		{
			AActor* Actor = Result.GetActor();
			// ここでアクター単位の足切りはしない（IsTargetableActor は最初の1部位だけ見るため、頭が
			// 非対象だと同じアクターの足まで巻き添えで弾かれる）。部位ごとの判定は下のループで行う
			if ( !Actor || Actor == IgnoreActor ) continue;

			// 敵優先の判定はアクター単位（部位はすべて同じ扱い）
			const bool bIsEnemy = Actor->IsA<AEnemyCharacter>();

			TArray<ULockOnTargetComponent*> TargetComps;
			Actor->GetComponents<ULockOnTargetComponent>( TargetComps );

			for ( ULockOnTargetComponent* Comp : TargetComps )
			{
				if ( !Comp || !Comp->bIsTargetable ) continue;

				const FVector TargetLoc = Comp->GetTargetLocation();

				const FVector ToTargetFromOrigin = TargetLoc - Origin;

				// 頂点を後方シフトすると扇の裏側が背面へ伸びて背後の敵を拾うため、実キャラ位置から見て
				// 後方の対象を除外する（真横＝Dot 0 は許容）
				if ( bApplyBackGuard && FVector::DotProduct( DefaultDir, ToTargetFromOrigin ) < 0.0f ) continue;

				// 距離・角度はどちらも水平判定なので、これが唯一の上下の足切り。無いと崖上／崖下の敵にも
				// 水平距離だけで吸着してしまう。0 で無制限
				if ( MaxHeightDiff > 0.0f && FMath::Abs( ToTargetFromOrigin.Z ) > MaxHeightDiff ) continue;

				// 収集の球は広げてあるので、ここで部位距離により本当のレンジへ絞る。判定は水平距離で
				// 行い（角度判定が 2D なのと一貫）、高さ方向は上の MaxHeightDiff で制限する
				const float DistSq = ToTargetFromOrigin.SizeSquared2D();
				if ( DistSq > MaxDistSq ) continue;	// 部位が吸着距離（水平）の外

				// 角度は ApexOrigin から測り、扇は水平（Yaw）基準なので 2D で正規化する
				// （3D だと対象が上にあるほど内積が下がり、狭い扇から外れて吸着できなくなる）
				const FVector DirForAngle = ( TargetLoc - ApexOrigin ).GetSafeNormal2D();
				if ( DirForAngle.IsNearlyZero() ) continue;	// 真上／真下（水平成分なし）は扇で狙えないので除外

				const float Dot = FVector::DotProduct( DefaultDir, DirForAngle );
				if ( Dot < CosHalfAngle ) continue;	// 扇形（吸着角度）の外

				FFanCandidate& Candidate = bIsEnemy ? BestEnemy : BestOther;
				Candidate.TryUpdate( Comp, Dot, DistSq, FrontalDotTolerance );
			}
		}

		// 敵が居れば必ず敵。居なければそれ以外から選ぶ
		return BestEnemy.Comp != nullptr ? BestEnemy.Comp : BestOther.Comp;
	}
}
