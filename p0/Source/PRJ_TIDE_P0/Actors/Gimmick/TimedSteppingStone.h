// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TimedSteppingStone.generated.h"

class UStaticMeshComponent;
class USplineComponent;

UENUM(BlueprintType)
enum class ESteppingStoneState : uint8
{
	Floating,
	Sinking,
	Sunken,
	Rising,
	// 派生(朽ちた浮石)用の前段。基底は使わない(既定はFloating開始)
	// 末尾追加でシリアライズ値は不変
	Entangled,	// ツタ拘束・不動。囲いトリガ待ち
	Freeing,	// ツタ破壊演出中
	Emerging,	// 指定位置へ浮上中
};

/**
 * 時限性の飛び石。PLが乗るとSinkDelay秒後に沈み、離れてRiseDelay秒後に元の位置へ浮き上がる。
 * PathSplineに点を置きMoveSpeed>0にすると、スプラインに沿ってパス移動する(移動中に乗っても同じく沈む)。
 *
 * StoneMeshがRoot。StaticMeshエディタでSimple Collisionを設定すること。ActorのScaleが石のサイズになる。
 */
UCLASS()
class PRJ_TIDE_P0_API ATimedSteppingStone : public AActor
{
	GENERATED_BODY()

public:

	ATimedSteppingStone();

protected:

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void NotifyActorBeginOverlap(AActor* OtherActor) override;

public:

	// Root。BlockAll+GenerateOverlapEventsで
	// 物理コリジョンと奈落検知を兼ねる
	UPROPERTY(VisibleAnywhere, Category = "Tide|Gimmick")
	TObjectPtr<UStaticMeshComponent> StoneMesh;

	// 移動パス。点を置いてMoveSpeed>0にするとこれに沿って動く
	UPROPERTY(VisibleAnywhere, Category = "Tide|Gimmick")
	TObjectPtr<USplineComponent> PathSpline;

	// パス移動の速度。0で移動しない
	UPROPERTY(EditAnywhere, Category = "Tide|Gimmick")
	float MoveSpeed = 0.0f;

	// PLが乗ったら沈むか。falseにすると沈まず、
	// MoveSpeed>0なら「乗っても落ちない移動床」になる
	UPROPERTY(EditAnywhere, Category = "Tide|Gimmick")
	bool bSinkOnPlayer = true;

	// PLが乗ってから沈み始めるまでの秒数
	UPROPERTY(EditAnywhere, Category = "Tide|Gimmick")
	float SinkDelay = 2.0f;

	// 沈む速度
	UPROPERTY(EditAnywhere, Category = "Tide|Gimmick")
	float SinkSpeed = 1000.0f;

	// PLが離れてから浮き上がるまでの待機秒数
	UPROPERTY(EditAnywhere, Category = "Tide|Gimmick")
	float RiseDelay = 3.0f;

	// 浮き上がりにかける秒数。時間ベース補間で戻す※速度指定だとfloat
	// /double誤差で到達判定が詰まる
	UPROPERTY(EditAnywhere, Category = "Tide|Gimmick")
	float RiseDuration = 0.5f;

	// 沈み始める何秒前から振動するか
	UPROPERTY(EditAnywhere, Category = "Tide|Gimmick")
	float ShakeDuration = 1.0f;

	// 振動の周波数 (rad/s)
	UPROPERTY(EditAnywhere, Category = "Tide|Gimmick")
	float ShakeFrequency = 50.0f;

	// 振動の振幅
	UPROPERTY(EditAnywhere, Category = "Tide|Gimmick")
	float ShakeAmplitude = 0.8f;

protected:

	// Stateが前段(Entangled/Freeing/Emerging)の間、
	// 基底Tickの通常機械をスキップして毎フレーム呼ぶフック
	// 派生(朽ちた浮石)がツタ破壊・浮上を差し込む。既定はno-op
	virtual void TickPreSettled(float DeltaTime) {}

	// 前段完了時に派生が呼ぶ。指定ワールド位置を定位置(OriginalLocation)に据え、
	// 通常挙動(Floating)へ委譲する
	void SettleAt(const FVector& WorldLocation);

	// 状態。既定Floating＝通常浮石。派生はEntangledから開始する
	ESteppingStoneState State = ESteppingStoneState::Floating;

private:

	void OnPlayerLanded();
	void OnPlayerLeft();
	void OnSinkTimerFired();
	void OnShakeTimerFired();
	void OnRiseTimerFired();
	void EnterSunken(bool bFromAbyss = false);
	void StopShake();

	bool IsSplineActive() const;
	// 沈み/浮き/振動中は呼ばない(基準を凍結)
	void AdvanceAlongSpline(float DeltaTime);

	// パス移動で動く「その時の基準位置」。沈み/浮き/振動はこれを基準にする
	FVector OriginalLocation = FVector::ZeroVector;
	bool bWasPlayerOnStone = false;
	bool bIsShaking = false;
	float ShakeElapsedTime = 0.0f;

	// 浮き上がりの時間ベース補間用
	// 開始時の高さからRiseDuration秒かけてOriginalLocation.Zへ戻す
	float RiseElapsedTime = 0.0f;
	double RiseStartZ = 0.0;

	// スプライン走査
	float SplineDistance = 0.0f;
	int32 SplineDir = 1;

	TWeakObjectPtr<class ATidePlayerCharacter> CachedPlayer;

	FTimerHandle SinkTimerHandle;
	FTimerHandle ShakeTimerHandle;
	FTimerHandle RiseTimerHandle;

};
