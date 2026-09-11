// Copyright (c) 2026, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BoostGimmick.generated.h"

class USceneComponent;
class USphereComponent;
class UStaticMeshComponent;
class UNiagaraComponent;
class UNiagaraSystem;
class ATidePlayerCharacter;

/**
 * 加速ギミック（触れると取得する加速オブジェクト）。速度・時間の数値は本アクター側に持ち、
 * プレイヤーへそのまま渡す。見た目は BP・アセットで差し替える。
 *
 * 触れると、その瞬間の向きへ既存の速度に BoostMaxSpeed を加算して BoostDuration の間ブーストダッシュする。
 * ブースト中も方向転換は可能で、方向はプレイヤー側で解決するため本アクターは向きを渡さない。
 * 空中取得では BoostAirMaxSpeed / BoostAirDuration があればそちらを使い、BoostAirZUpSpeed で上方向速度も
 * 加算できる。BoostVelocityCap を設定すると、既に高速移動中に触れて速度が際限なく伸びるのを防げる。
 *
 * bMoveToCenterOnCollect が ON なら、加速前にプレイヤーを取得判定の中心へ移動させる。どの位置で触れても
 * 開始位置が揃うので飛距離が一定になる（高さは地上取得では維持し、空中取得のみ中心 Z へスナップして
 * 縦アークも一定化する）。移動は MovePlayerToCenterSafely のスイープで、床抜け／壁抜けを防ぐ。
 *
 * 取得すると一旦消え（メッシュ非表示＋判定オフ）、RespawnCooldown 後に復活する加速パッド運用
 * （bRespawn=false なら取得で消滅）。
 */
UCLASS()
class PRJ_TIDE_P0_API ABoostGimmick : public AActor
{
	GENERATED_BODY()

public:
	ABoostGimmick();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay( const EEndPlayReason::Type EndPlayReason ) override;
	virtual void Tick( float DeltaTime ) override;
	// AmbientEffect を AmbientVFX へ反映する（エディタ上でもプレビュー表示させる）
	virtual void OnConstruction( const FTransform& Transform ) override;

	UFUNCTION()
	void OnTriggerOverlap( UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
		bool bFromSweep, const FHitResult& SweepResult );

	// ブースト付与 → 取得 VFX → 非表示化 → 復活予約 or 消滅
	void Collect( ATidePlayerCharacter* Player );

	// 現在地→中心をカプセルスイープし、地形があれば手前で止めて床抜け／壁抜けを防ぐ。
	// bAirPickup なら高さ(Z)も中心へスナップする（地上取得は現在の高さを維持）
	void MovePlayerToCenterSafely( ATidePlayerCharacter* Player, bool bAirPickup );

	void Respawn();
	void SetActiveState( bool bActive );	// 取得中は非表示＋判定オフ

#if !UE_BUILD_SHIPPING
	void DrawDebugBoostVisual() const;	// 負荷軽減のため炎・火花演出は廃止
#endif

public:
	UPROPERTY( VisibleAnywhere, Category = "Tide|BoostGimmick" )
	TObjectPtr<USceneComponent> SceneRoot;

	// 取得判定用のオーバーラップ球
	UPROPERTY( VisibleAnywhere, Category = "Tide|BoostGimmick" )
	TObjectPtr<USphereComponent> TriggerComp;

	// 常時表示エフェクトの再生コンポーネント。再生するアセットは AmbientEffect スロットで指定する
	UPROPERTY( VisibleAnywhere, Category = "Tide|BoostGimmick" )
	TObjectPtr<UNiagaraComponent> AmbientVFX;

	// --- ブースト性能（プレイヤーへ渡す数値） ---
	// 付与する最高速度（cm/秒）。プレイヤーは取得時の自分の向きへこの速度で瞬間加速する
	UPROPERTY( EditAnywhere, Category = "Tide|BoostGimmick|Boost" )
	float BoostMaxSpeed = 3150.0f;
	// 空中で取得した場合に BoostMaxSpeed の代わりに使う最高速度（cm/秒）。0以下なら BoostMaxSpeed を
	// そのまま使う（地上取得時は常に BoostMaxSpeed を使う）
	UPROPERTY( EditAnywhere, Category = "Tide|BoostGimmick|Boost" )
	float BoostAirMaxSpeed = 0.0f;
	// ブーストの持続時間（秒）。地上で取得した場合に使う
	UPROPERTY( EditAnywhere, Category = "Tide|BoostGimmick|Boost" )
	float BoostDuration = 0.7f;
	// 空中で取得した場合に使うブーストの持続時間（秒）
	UPROPERTY( EditAnywhere, Category = "Tide|BoostGimmick|Boost" )
	float BoostAirDuration = 0.9f;
	// 加算後の水平速度の上限（cm/秒。0以下で上限なし）。高速移動中に触れたとき BoostMaxSpeed の単純加算で速度が
	// 際限なく伸びるのを防ぐため、これを超えないよう水平方向のみ縮尺してクランプする
	UPROPERTY( EditAnywhere, Category = "Tide|BoostGimmick|Boost" )
	float BoostVelocityCap = 3500.0f;
	// 空中で取得した場合に加算する上方向速度（cm/秒。0以下で加算なし。地上取得時は無視）。水平維持時間
	// （BoostDashAirGravityLockTime）が設定されている間はこの速度を維持したまま水平を保つ
	UPROPERTY( EditAnywhere, Category = "Tide|BoostGimmick|Boost" )
	float BoostAirZUpSpeed = 300.0f;
	// 取得した瞬間、加速前にプレイヤーをトリガーの中心へ移動させるか。ON ならどの位置で触れても同じ開始位置から
	// 加速するので飛距離が一定になる。高さは地上取得では維持し（床めり込み防止）、空中取得のみ中心 Z へスナップする
	UPROPERTY( EditAnywhere, Category = "Tide|BoostGimmick|Boost" )
	bool bMoveToCenterOnCollect = true;

	// --- 復活 ---
	// 取得後に復活するか（false なら取得で消滅）
	UPROPERTY( EditAnywhere, Category = "Tide|BoostGimmick|Respawn" )
	bool bRespawn = true;
	// 復活までのクールダウン（秒）
	UPROPERTY( EditAnywhere, Category = "Tide|BoostGimmick|Respawn", meta = ( EditCondition = "bRespawn" ) )
	float RespawnCooldown = 3.0f;

	// --- VFX ---
	// 常時表示するエフェクト（未設定なら出さない）。取得可能な間ずっと AmbientVFX で再生する
	UPROPERTY( EditAnywhere, Category = "Tide|BoostGimmick|VFX" )
	TObjectPtr<UNiagaraSystem> AmbientEffect;

	// AmbientEffect の大きさ（User Parameter "Scale" へ渡す）
	UPROPERTY( EditAnywhere, Category = "Tide|BoostGimmick|VFX" )
	float AmbientEffectScale = 1.0f;

	// 取得時に再生する単発エフェクト（未設定なら出さない）
	UPROPERTY( EditAnywhere, Category = "Tide|BoostGimmick|VFX" )
	TObjectPtr<UNiagaraSystem> CollectEffect;

	// CollectEffect の大きさ（User Parameter "Scale" へ渡す）
	UPROPERTY( EditAnywhere, Category = "Tide|BoostGimmick|VFX" )
	float CollectEffectScale = 1.0f;

	// --- デバッグ ---
	// ON で取得判定球を実行時に描画する
	UPROPERTY( EditAnywhere, Category = "Tide|BoostGimmick|Debug" )
	bool bDebugDraw = true;

private:
	bool  bIsActive = true;								// 取得可能（見えている）か
	FTimerHandle RespawnTimerHandle;
};
