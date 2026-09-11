// Copyright (c) 2026, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class AActor;
class UWorld;
class ULockOnTargetComponent;

namespace TargetingUtil
{
	/**
	 * アクターが有効なロックオン対象か（bIsTargetable な ULockOnTargetComponent を持つか）を判定する。
	 *
	 * @param Actor       判定対象（nullptr / IgnoreActor は false）
	 * @param IgnoreActor 除外するアクター（通常は自分）
	 */
	bool IsTargetableActor( const AActor* Actor, const AActor* IgnoreActor );

	/**
	 * Origin から DefaultDir を中心とする扇形（半径 MaxDist・半角 HalfAngleRad）の内側で、
	 * 角度条件を満たすうち最も近い bIsTargetable な ULockOnTargetComponent を返す（攻撃の吸着用）。
	 *
	 * - ECC_Pawn でスフィアオーバーラップして候補を集める
	 * - 1 アクターが複数の ULockOnTargetComponent（部位）を持つ場合も全て評価する
	 * - 敵（AEnemyCharacter 派生）とそれ以外（壊れ物など）が同時に扇へ入った場合は、
	 *   距離に依らず必ず敵を返す（敵の中で最も角度が近いもの）。敵が居ないときだけそれ以外を返す
	 * - 距離・角度はどちらも水平（2D）判定。上下は MaxHeightDiff の高さ差ゲートだけで切る
	 * - 該当が無ければ nullptr
	 *
	 * 扇の頂点（角度を測る原点）だけを ApexBackOffset ぶん DefaultDir の逆方向へ下げられる。
	 * 頂点を後ろに置くと真横の敵の角度が小さくなり、横にいる敵を拾いやすくなる。
	 * 探索半径・距離・最終的な向きは Origin（実キャラ位置）基準のまま（頂点シフトは角度判定にのみ作用）。
	 * なお頂点を下げても実キャラ位置から見て後方の対象は拾わない（真後ろ拾いガード。真横は許容）。
	 * ただしホーミング全角が90°以上（HalfAngleRad >= 45°）のときは広角設定を尊重してこのガードを外す。
	 *
	 * @param World         検索に使うワールド
	 * @param Origin        探索の原点（通常はキャラ位置）
	 * @param DefaultDir    扇の中心方向（正規化済み想定）
	 * @param MaxDist       探索半径
	 * @param HalfAngleRad  扇の半角（ラジアン）
	 * @param IgnoreActor   探索から除外するアクター（通常は自分）
	 * @param ApexBackOffset 角度判定に使う扇の頂点を Origin から後方へ下げる量（cm。0 で従来どおり）
	 * @param MaxHeightDiff Origin と対象部位の高さ（Z）差の上限（cm。0 で無制限＝従来どおり）。
	 *                      距離・角度はどちらも水平判定なので、これが唯一の上下の足切りになる
	 */
	ULockOnTargetComponent* FindBestTargetInFan(
		const UWorld* World,
		const FVector& Origin,
		const FVector& DefaultDir,
		float MaxDist,
		float HalfAngleRad,
		const AActor* IgnoreActor,
		float ApexBackOffset = 0.0f,
		float MaxHeightDiff = 0.0f );
}
