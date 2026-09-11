// Copyright (c) 2026, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "IGroundPullAffectable.generated.h"

/**
 * 地面の吸い込み（アリジゴクの渦など）が対象へ渡す影響パラメータ。
 * IWindAffectable（空気の流れ）とは別物：本インターフェースは「地面が沈み込んで引き込む」系の
 * 環境ギミック専用。受け手（IGroundPullAffectable）が、自分の都合に合わせて解釈する。
 */
struct FGroundPullInfluence
{
	// 引き込み中心（水平座標。Z は無視される）
	FVector Center = FVector::ZeroVector;
	// 水平の引き込み＋渦巻き合成速度（cm/秒）。受け手が状態別の抵抗倍率を掛け、
	// 「位置オフセット（速度×DeltaTime）」として押されることを想定する（IWindAffectable の水平風と同じ流儀）
	FVector PullVelocity = FVector::ZeroVector;
	// 引き込み源（アリジゴクの渦アクター等）
	TWeakObjectPtr<AActor> Source;
};

UINTERFACE( MinimalAPI )
class UGroundPullAffectable : public UInterface
{
	GENERATED_BODY()
};

/**
 * 地面の吸い込み（アリジゴクの渦など）の影響を受ける対象が実装するインターフェース。
 *
 * IWindAffectable と同じく「風源（本インターフェースでは引き込み源）は影響パラメータだけを渡し、
 * 具体的な挙動は各実装に委ねる」という思想を踏襲する。ただし物理現象としては空気の流れ（Wind）とは
 * 別物（地面の砂が沈み込む効果）なので、意図的にインターフェースを分けている。
 *
 * 複数の引き込み源に同時に巻き込まれる場合があるため、実装側は Enter/Exit を参照カウントで扱うのが望ましい。
 */
class IGroundPullAffectable
{
	GENERATED_BODY()

public:
	// 引き込みに巻き込まれ始めた（範囲に入った）瞬間
	virtual void OnGroundPullEnter( const FGroundPullInfluence& Pull ) {}
	// 引き込みの影響を毎フレーム受ける
	virtual void OnGroundPullTick( const FGroundPullInfluence& Pull, float DeltaTime ) {}
	// 引き込みから外れた／引き込み源が消えた
	virtual void OnGroundPullExit() {}
};
