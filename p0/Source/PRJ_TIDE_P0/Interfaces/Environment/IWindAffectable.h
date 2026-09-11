// Copyright (c) 2026, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "IWindAffectable.generated.h"

/**
 * 風（竜巻・吸引など）が対象へ渡す影響パラメータ。
 * 受け手（IWindAffectable）が、自分の都合に合わせて巻き上げ・引き込みを解釈する。
 */
struct FWindInfluence
{
	// 風の中心（水平方向の引き込み先）
	FVector Center = FVector::ZeroVector;
	// 上昇速度（cm/秒）
	float LiftSpeed = 0.0f;
	// 中心へ寄せる水平引き込み速度（cm/秒）
	float PullSpeed = 0.0f;
	// この引き込み（PullSpeed）をプレイヤーへ適用するか。竜巻はプレイヤーを引き込まないため false を渡す
	// （敵の引き込み・敵弾の引き寄せは既定 true のまま）。PL 側のみ参照する
	bool bPullAffectsPlayer = true;
	// 中心の高さからの最大持ち上げ高さ（cm）。0 以下で無制限
	float MaxLiftHeight = 0.0f;
	// 風の柱（範囲ボリューム）の上端のワールド Z。受け手が「頂上」として使える物理的な天井。
	// MaxLiftHeight と違い常に有限かつ中心より高い値が入る（吸い上げ上限が無制限でも範囲の高さで決まる）
	float ColumnTopZ = 0.0f;
	// 風の柱（範囲ボリューム）の水平半径（cm）。受け手が旋回半径などの基準に使える
	float ColumnRadius = 0.0f;
	// 風の中でジャンプしたときのジャンプ力倍率（PL のジャンプ強化用。1.0 で等倍）
	float JumpBoostMultiplier = 1.0f;
	// 水平方向の風（押し出し速度 cm/秒）。受け手が状態別の抵抗倍率を掛け、速度ではなく位置オフセットとして押し出すことで
	// 本人の Velocity を汚さない（ダッシュの速度上書きに消されず、速度向きを見るターン検出も誤爆しない）。竜巻はゼロ
	FVector HorizontalWindVelocity = FVector::ZeroVector;
	// 風源（竜巻アクター等）
	TWeakObjectPtr<AActor> Source;
};

UINTERFACE( MinimalAPI )
class UWindAffectable : public UInterface
{
	GENERATED_BODY()
};

/**
 * 風（竜巻・吸引・突風など）の影響を受ける対象が実装するインターフェース。
 *
 * ダメージは IDamageable が担うので、本インターフェースは「巻き上げ・引き込み等の移動的な影響」だけを担当する。
 * 同じ風でも対象ごとに反応が異なる（敵＝巻き上げて滞空、PL＝ジャンプ強化 など）ため、
 * 風源は影響パラメータ（FWindInfluence）だけを渡し、具体的な挙動は各実装に委ねる。
 *
 * 複数の風源に同時に巻き込まれる場合があるため、実装側は Enter/Exit を参照カウントで扱うのが望ましい。
 */
class IWindAffectable
{
	GENERATED_BODY()

public:
	// 風に巻き込まれ始めた（範囲に入った）瞬間
	virtual void OnWindEnter( const FWindInfluence& Wind ) {}
	// 風の影響を毎フレーム受ける
	virtual void OnWindTick( const FWindInfluence& Wind, float DeltaTime ) {}
	// 風から外れた／風が消えた
	virtual void OnWindExit() {}

	// 向かい風ゾーンのエリア内（無効エリアは除く）に入った/出た瞬間。遮蔽を無視した静的なエリア判定なので、実際に
	// 押し出しを受けているか（OnWindEnter/Exit）とは独立して発火する。複数ゾーンに同時に入りうるので参照カウントで扱う
	virtual void OnWindZoneAreaEnter() {}
	virtual void OnWindZoneAreaExit() {}
};
