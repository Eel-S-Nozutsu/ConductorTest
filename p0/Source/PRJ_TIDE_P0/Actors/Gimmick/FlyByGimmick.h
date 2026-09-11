// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FlyByGimmick.generated.h"

class USceneComponent;
class USphereComponent;
class USplineComponent;
class USkeletalMeshComponent;
class UAnimSequenceBase;
class UNiagaraSystem;
class ATidePlayerCharacter;

/**
 * 飛翔演出ギミック(コリジョン接触で1度だけ起動する)。
 *
 * プレイヤーがTriggerCompに触れると、PathSplineの始点からPerformerMeshを表示し、
 * スプラインに沿ってFlightSpeedで飛翔させながらMoveAnimationをループ再生する。
 * 終端到達でEndEffectを再生し、自身をDestroyする。
 * 見た目はPerformerMeshコンポーネントにスケルタルメッシュを、動きはMoveAnimationに
 * AnimSequence(回転込みのモーション)を指定するだけでよく、キャラBPの作成は不要。
 * ※ Montageはスロット付きAnimBPが要るため素のメッシュ再生には使えない。AnimSequenceを指定すること。
 * 起動は1度きり(bTriggered)。PathSplineがそのまま生成地点(始点)と飛翔経路を兼ねる。
 */
UCLASS()
class PRJ_TIDE_P0_API AFlyByGimmick : public AActor
{
	GENERATED_BODY()

public:

	AFlyByGimmick();

protected:

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaTime) override;
	// 配置時プレビュー用にPerformerMeshをスプライン始点へスナップする
	virtual void OnConstruction(const FTransform& Transform) override;

	UFUNCTION()
	void OnTriggerOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
		bool bFromSweep, const FHitResult& SweepResult);

	// 演出を起動する(メッシュ表示 → 飛翔開始)。多重起動はbTriggeredで抑止
	void Trigger();

	// スプライン終端に到達した際の処理(終端VFX再生 → Destroy)
	void FinishFlight();

	// PerformerMeshをスプライン上の指定距離へ配置する
	// (bOrientToPathなら進行方向へ向ける)
	void PlaceMeshAtDistance(float Distance);

public:

	UPROPERTY(VisibleAnywhere, Category = "Tide|FlyByGimmick")
	TObjectPtr<USceneComponent> SceneRoot;

	// 起動判定用のオーバーラップ球
	UPROPERTY(VisibleAnywhere, Category = "Tide|FlyByGimmick")
	TObjectPtr<USphereComponent> TriggerComp;

	// 生成地点(始点)と飛翔経路を兼ねるスプライン
	UPROPERTY(VisibleAnywhere, Category = "Tide|FlyByGimmick")
	TObjectPtr<USplineComponent> PathSpline;

	// 飛翔する見た目。このコンポーネントにスケルタルメッシュを指定する(キャラBPは不要)
	UPROPERTY(VisibleAnywhere, Category = "Tide|FlyByGimmick")
	TObjectPtr<USkeletalMeshComponent> PerformerMesh;

	// 飛翔中ループ再生するモーション(AnimSequence。回転込みで持たせる。未設定なら再生しない)
	UPROPERTY(EditAnywhere, Category = "Tide|FlyByGimmick|Motion")
	TObjectPtr<UAnimSequenceBase> MoveAnimation;

	// ONならATidePlayerCharacterが触れた時のみ起動する
	UPROPERTY(EditAnywhere, Category = "Tide|FlyByGimmick|Spawn")
	bool bTriggerByPlayerOnly = true;

	// 飛翔速度 ※スプラインに沿って毎秒進む距離
	UPROPERTY(EditAnywhere, Category = "Tide|FlyByGimmick|Flight", meta = (ClampMin = "0.0"))
	float FlightSpeed = 800.0f;

	// ONで進行方向(スプライン接線)へメッシュを向ける
	UPROPERTY(EditAnywhere, Category = "Tide|FlyByGimmick|Flight")
	bool bOrientToPath = true;

	// 終端到達時に再生する単発エフェクト(未設定なら出さない)
	UPROPERTY(EditAnywhere, Category = "Tide|FlyByGimmick|VFX")
	TObjectPtr<UNiagaraSystem> EndEffect;

	// EndEffectの大きさ(User Parameter "Scale" へ渡す)
	UPROPERTY(EditAnywhere, Category = "Tide|FlyByGimmick|VFX", meta = (ClampMin = "0.0"))
	float EndEffectScale = 1.0f;

private:

	float DistanceAlong = 0.0f;		// スプライン始点からの累積距離
	bool  bTriggered = false;		// 起動済み(1度きり)
	bool  bFlying = false;			// 飛翔中か

};
