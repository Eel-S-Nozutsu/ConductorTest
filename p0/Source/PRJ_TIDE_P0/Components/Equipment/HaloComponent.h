// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PRJ_TIDE_P0/Utilities/HaloShake.h"
#include "HaloComponent.generated.h"

struct FDamageInfo;
class UEnemyDataAsset;
class AGeometryCollectionActor;
class UEnemyBattleComponent;
class USkeletalMeshComponent;
class UMaterialInstanceDynamic;

// 背面待機光輪を後ろから被弾したときの結果
enum class EHaloBackHitResult : uint8
{
	None,	// 背面非活性/後方でない ※光輪に影響なし
	Crack,	// ヒビのみ (光輪は生存)
	Break,	// 破壊
};

/**
 * 光輪ガードシステム ガード状態管理/接触ダメージ/ビジュアルをまとめて管理
 * AEnemyCharacter専用
 */
UCLASS(ClassGroup = (Equipment), meta = (BlueprintSpawnableComponent))
class PRJ_TIDE_P0_API UHaloComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	UHaloComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// AEnemyCharacter::BeginPlayから呼ぶ。依存オブジェクトをキャッシュする
	void Initialize(const UEnemyDataAsset* InData, USkeletalMeshComponent* InSkelMesh,
		UEnemyBattleComponent* InBattle);
	// "Halo" コンポーネントを探してDMIを生成しマテリアルに差し替える
	void InitializeDMI();
	// Overlapデリゲートをバインドし、バリア球を動的に生成する
	void BindHaloOverlap(FName HaloComponentName = FName("Halo"));
	// 光輪を指定ソケットへアタッチする。AnimNotifyState_HaloAttachから呼ばれる経
	// 路用
	USceneComponent* AttachHaloToSocket(FName SocketName, FName HaloComponentName, float Scale = 1.0f);
	// 攻撃用に光輪をソケットへアタッチする。進行中のガード解除ディザを中止して
	// RestoreHaloToStateSocketによる割り込みを防ぐ
	void AttachHaloForAttack(FName SocketName, FName HaloComponentName);
	// 現在のガード状態に合わせたソケットへ光輪を戻す
	void RestoreHaloToStateSocket(FName HaloComponentName = FName("Halo"));
	// ガード開始。TAG_State_Enemy_Guardを付与してソケットを切り替える
	void BeginGuard(FName HaloComponentName = FName("Halo"));
	// ガード自動解除タイマーを起動する
	void StartGuardAutoRelease();
	// バリア球のコリジョンを有効/無効にする
	void SetHaloBarrierActive(bool bEnable);
	// 光輪タッチダメージを有効化する。有効化時に既に接触中のアクターにも発火する
	void SetDeployed(bool bActive);
	// 光輪技で光輪を飛ばしている間true。破壊と合わせて「光輪が手元に無い＝無防備」を決める
	void SetHaloThrown(bool bThrown);
	// 光輪が手元に無い (投擲中or破壊〜再生待ち) か
	bool IsHaloAway() const { return bHaloThrown || bHaloBroken; }
	// 光輪技が中断され光輪を飛ばしたままなら、技クールを回してから戻す
	// 既に戻っている / 破壊されている場合は何もしない (破壊側が再生を握る)
	void StartThrowRegenIfAway();
	// ガード中のヒット処理。ひびあり・ガード解除に応じてOnGuardPenetratedを呼ぶ
	// bSuppressReaction=trueでガードを割るだけに留める
	// 呼び出し側が後段でリアクションを通す場合、ここでも起こすと二重に走るため
	void HandleGuardHit(const FDamageInfo& DamageInfo, bool bSuppressReaction = false);
	// 竜巻に巻き込まれる際にガードの光輪を破壊する。ダメージ・ヒットリアクションは伴わない
	// WindCenterから外側へ破片を散らす。実際にガードを破壊したらtrue
	bool BreakGuardForWind(const FVector& WindCenter);
	// 死亡時など外部からタイマーをすべてクリアする
	void ClearAllTimers();
	// ガード突破時にAEnemyCharacter側でSuper::ReceiveDamageを呼ぶための
	// コールバック
	TFunction<void(const FDamageInfo&)> OnGuardPenetrated;
	// HitReactionのSetActorRotationより前にフォワードを保存する
	// (EnemyCharacter::ReceiveDamageから呼ぶ)
	void CaptureForwardSnapshot(const FVector& Forward) { ForwardSnapshot = Forward; }
	// 背中待機中に後ろから攻撃されたときの結果を返す。弱攻撃(ギア無し/壱/弍)=ヒビ /
	// ギア参・極 もしくは ヒビ中の再ヒット=破壊。破壊時のみ光輪を砕き背面待機を解除する
	EHaloBackHitResult HandleBackHaloHit(const FDamageInfo& DamageInfo);
	// 攻撃時の明るさを切り替える (true=明るめ:攻撃false=暗め:ガード・通常)
	void SetHaloAttackGlow(bool bBright);
	// 光輪を使った攻撃の区間中true。共通仕様として攻撃 (発光) 中は光輪を破壊させない
	// 部位光輪のbPartHaloSuppressedと対を成す
	// 発光 (SetHaloAttackGlow) とは別管理にして、
	// 復帰アーマーの発光 (SetRecoveryArmor) では破壊不可にしない
	void SetHaloAttackSuppressed(bool bSuppressed) { bHaloAttackSuppressed = bSuppressed; }
	// 光輪の発光強度を直接書き込む (HaloAttackGlowParamNameを流用)
	// ガードチャージのフリッカー演出などから毎フレーム呼ぶ。bHaloAttackGlow状態は変えない
	void SetHaloGlowIntensity(float Intensity);
	// ガードを即時終了する (チャージ暴発後など外部から終わらせる用)
	// 被弾リアクション由来の中断中は、リアクション側が抑止を握るためbRestartAI=falseで呼ぶ
	void EndGuard(bool bRestartAI = true);
	// ふっとび復帰中の光輪アーマー。有効中は発光＋接触ダメージ判定を有効化し、
	// SAタグの有無に依らず後方攻撃による破壊を無効化する
	void SetRecoveryArmor(bool bActive);

private:

	const UEnemyDataAsset* Data = nullptr;

	UPROPERTY()
	TObjectPtr<USkeletalMeshComponent> SkelMesh;

	UPROPERTY()
	TObjectPtr<UEnemyBattleComponent> BattleComponent;

	UPROPERTY()
	TObjectPtr<UPrimitiveComponent> HaloMeshComp;

	UPROPERTY()
	TObjectPtr<UPrimitiveComponent> HaloBarrierComp;

	// 接触ダメージ用のオーバーラップ球。薄いリングメッシュに依存せず、接触ゾーンを
	// 確実に拾う。ブロックはせずPawnとOverlapするだけ。モンタージュ拡縮の影響を
	// 受けないよう絶対スケールで保持する
	UPROPERTY()
	TObjectPtr<UPrimitiveComponent> HaloTouchVolumeComp;

	// 光輪メッシュの全マテリアルスロットのMID (破片と同様に全スロット化)
	// 反映はSetHaloScalarで一括
	UPROPERTY()
	TArray<TObjectPtr<UMaterialInstanceDynamic>> HaloDMIs;

	FVector ForwardSnapshot = FVector::ForwardVector;
	bool    bBackHaloActive = false;

	// 現在の明るさ状態 (true=明るめ:攻撃false=暗め:ガード・通常)。破壊破片の色に流用する
	bool    bHaloAttackGlow = false;

	// 光輪攻撃中は破壊不可 (発光中は壊れない共通仕様)
	// SetHaloAttackSuppressedで駆動
	bool    bHaloAttackSuppressed = false;

	// 光輪が手元に無い理由。どちらかが立っている間は無防備
	// (TAG_State_Enemy_HaloAway)
	// 破壊は技クールを無効化する ＝ 投擲が終わっても破壊中はタグが残り、再生で初めて解ける
	bool bHaloThrown = false;
	bool bHaloBroken = false;

	// このコンポーネントがHaloAwayタグの参照カウントを1つ握っているか
	bool bHaloAwayTagApplied = false;

	bool bHaloDamageCooling     = false;
	bool bHaloDeployed          = false;

	// 接触ダメージのウィンドウ。発火からHaloTouchDamageDuration秒の間、
	// 毎フレーム範囲内を取り直して当てる (1体につき1回)
	// 1Fスナップショットだと発火Fにたまたま範囲外にいただけで無傷になり、
	// 予告 (フリッカー/VFX) と結果が食い違うため
	bool  bHaloTouchWindowActive   = false;
	float HaloTouchWindowRemaining = 0.0f;
	// このウィンドウで既にダメージを与えた相手 (毎フレーム多段ヒットさせないため)
	TSet<TWeakObjectPtr<AActor>> HaloTouchDamagedActors;
	bool bHaloCracked           = false;
	bool bHaloTouchFlickerState = false;
	bool bHaloDithering        = false;
	bool bHaloDitherOut         = true;
	float HaloDitherElapsed     = 0.0f;

	FTimerHandle HaloCooldownTimerHandle;
	FTimerHandle HaloVfxTimerHandle;
	FTimerHandle HaloTouchTimerHandle;
	FTimerHandle HaloFlickerTimerHandle;
	FTimerHandle GuardCheckTimerHandle;
	FTimerHandle GuardMaxDurationTimerHandle;
	FTimerHandle HaloBreakReformTimerHandle;
	FTimerHandle HaloBreakFadeTimerHandle;
	FTimerHandle HaloBreakScatterTimerHandle;
	FTimerHandle HaloThrowRegenTimerHandle;

	bool  bHaloBreakFading     = false;
	float HaloBreakFadeElapsed = 0.0f;

	// 胴体フレネル (光輪が健在な間だけ点灯)
	// CurrentをTargetへ補間してON/OFFをフェードさせる
	bool  bBodyFresnelFading = false;
	float FresnelCurrent     = 0.0f;
	float FresnelTarget      = 0.0f;

	// ヒットしたが壊れなかったときのガード光輪の3軸ブレ
	FHaloShakeState HaloShakeState;

	FVector CachedHaloBreakImpulseDir = FVector::ZeroVector;
	FVector CachedHaloBreakCenter     = FVector::ZeroVector;
	float   CachedHaloBreakInstigatorDistance = -1.0f;

	TWeakObjectPtr<AGeometryCollectionActor> ActiveHaloBreakGCA;

	UPROPERTY()
	TObjectPtr<class UFieldSystemComponent> HaloBreakFieldComp;

	void CheckGuardRelease();

	void OnGuardAutoReleaseExpired();

	// ガード成立時に立てた行動抑止が宙に浮かないための保険
	// ガード突破/解除でガードタグを消す経路から呼ぶ
	void ClearGuardReaction();

	// bHaloThrown / bHaloBrokenから無防備タグを同期する
	void UpdateHaloAwayTag();

	void StartHaloDither();
	void StartHaloBreak(const FVector& ImpulseDir, float InstigatorDistance = -1.0f);

	// 胴体フレネルをON/OFFする (光輪健在=ON / 無防備=OFF)。光輪を持たない敵は常に無効
	// bImmediate=trueでフェードせず即反映 (初期化用)
	void SetFresnelActive(bool bOn, bool bImmediate = false);
	void ApplyHaloBreakScatterForce();
	void OnHaloBreakFadeStart();
	void OnHaloBreakReformed();

	// 技クール明け。飛ばしたままの光輪を手元へ戻す
	void OnHaloThrowRegen();

	bool IsBackHaloAttackFromBehind(const FDamageInfo& DamageInfo) const;

	// 操作対象の光輪プリミティブを解決する。AEnemyCharacterがDA (HaloMesh)
	// から構成した光輪メッシュを返す。メッシュ未構成の敵 (BackHalo運用など光輪を持たない敵)
	// はnullptr
	UPrimitiveComponent* ResolveHaloPrimitive() const;

	// 背中光輪を常にヒビ状態に保つか (bHaloBackAlwaysCracked)
	// ヒビ解除の各所で参照し、常時ヒビの敵は無傷へ戻さない (初期化・再生時もヒビを張り直す)
	bool ShouldBackHaloStayCracked() const;

	// bHaloCrackedとマテリアルのヒビ表現を同期する
	// 常時ヒビ敵はfalseを渡してもヒビを維持する
	void SetHaloCracked(bool bCracked);

	// 光輪メッシュの全MIDへスカラーパラメータを流す (全スロット化対応)
	void SetHaloScalar(FName Param, float Value);
	// 光輪のMIDが生成済みか (パラメータ反映の前提)
	bool HasHaloDMI() const { return !HaloDMIs.IsEmpty(); }

	UFUNCTION()
	void OnHaloOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
		bool bFromSweep, const FHitResult& SweepResult);

	void OnHaloVfxFired();
	// 接触ダメージ発火。ダメージウィンドウを開く
	// (実ダメージはApplyHaloTouchDamageが毎フレーム行う)
	void OnHaloTouchFired();
	// ウィンドウを閉じて再受付クールダウンへ入る
	void CloseHaloTouchWindow();
	// 範囲内の敵対Damageableへダメージを与える
	// ウィンドウ中は毎フレーム呼ばれるが1体につき1回だけ当てる
	void ApplyHaloTouchDamage();
	void StartHaloTouchFlicker(bool bStart);
	void OnHaloFlickerTick();
	void ResetHaloCooldown();

#if !UE_BUILD_SHIPPING
	void DrawHaloDebug() const;
#endif

};
