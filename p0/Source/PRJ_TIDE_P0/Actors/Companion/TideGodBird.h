// Copyright (c) 2026, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TideGodBird.generated.h"

class USceneComponent;
class USkeletalMeshComponent;
class UAnimMontage;
class UAnimMontageListDataAsset;

/**
 * 神鳥（プレイヤーのお供精霊）。生成・表示制御・追従設定は UGodBirdPlayerModule が行い、
 * 本アクターは「見た目＋浮遊＋補間追従」を自己完結で持つ。
 *
 * 構成：Root＝補間追従で動かす本体、BirdMesh はその子で浮遊はそのローカルオフセットへ乗せる
 * （追従＝Root、浮遊＝子メッシュ、で干渉しない）。プレイヤーへは剛体アタッチしない。
 */
UCLASS()
class PRJ_TIDE_P0_API ATideGodBird : public AActor
{
	GENERATED_BODY()

public:
	ATideGodBird();

	void SetVisible( bool bVisible );

	// 追従対象と、対象ローカル基準のオフセット/向きを設定する。InSocketName 指定でメッシュソケット基準
	// （NAME_None でアクター原点基準）。bInStickToTarget は補間・浮遊をやめ毎フレームスナップする
	// 「引っ付く」モード、bSnapToTargetNow=false は現在地から徐々に寄せる（滑空終了→通常追従の復帰用）
	void SetFollowTarget( AActor* InLeader, const FVector& InLocalOffset, const FRotator& InRotationOffset, const FName& InSocketName = NAME_None, bool bInStickToTarget = false, bool bSnapToTargetNow = true );

	// true の間は自前の補間追従・浮遊を止め、位置・向きを外部（一閃 mover）が直接動かせるようにする
	void SetExternallyDriven( bool bDriven ) { bExternallyDriven = bDriven; }
	bool IsExternallyDriven() const { return bExternallyDriven; }

	// 位置は従来どおり対象へ寄せつつ、向きだけ InBaseWorldFacing＋メッシュ補正へ寄せる（構え中の用途）
	void SetFacingOverride( bool bEnabled, const FRotator& InBaseWorldFacing );

	// メッシュ補正を掛けてアクター向きを即時に設定する。外部駆動中に一閃側から締めの向きを与えるために使う
	void SetActorFacingWithModelOffset( const FRotator& InBaseWorldFacing );

	// 以下 3 つの起動関数に共通：InModelRotationOffset はメッシュ補正、
	// InStartScale（>0）を指定すると ST モーションの進捗に合わせて拡大する

	// 神技「導き」の突進（都度スポーンした専用インスタンス用）。InTargetLocation へ前進し、
	// 到達か InMaxDuration 経過で自壊する（追従・浮遊はしない）
	void ActivateGuidanceCharge( const FVector& InTargetLocation, float InSpeed, float InMaxDuration, float InScale, const FRotator& InModelRotationOffset = FRotator::ZeroRotator, float InStartScale = -1.0f );

	// 神技「戯れ」の自律攻撃。外部駆動モードにする（移動・寿命・攻撃は UGodFrolicSubModule が持つ）
	void ActivateFrolicAttack( float InScale, const FRotator& InModelRotationOffset = FRotator::ZeroRotator, float InStartScale = -1.0f );

	// SlidePassive 竜巻エスコート（お供本体を一時的に借りるだけで自壊はしない）。InCenter へ飛んでいき半径
	// InRadius の円を InTurns 周ぶん回って戻る。高さは InStartHeight → InHeight へ補間する（＝上がる螺旋。
	// 同値なら水平周回）。戻りきると自動で通常追従へ復帰する
	void ActivateTornadoEscort( const FVector& InCenter, float InRadius, float InHeight, float InStartHeight, float InSpeed, float InAngularSpeedDeg, float InTurns );

	// 表示維持・割り込み判定にモジュールが使う
	bool IsPlayingSlidePassiveCinematic() const { return EscortPhase != EEscortPhase::None; }
	void CancelSlidePassiveCinematic() { EscortPhase = EEscortPhase::None; }	// 滑空／神技による強制回収用

	USkeletalMeshComponent* GetBirdMesh() const { return BirdMesh; }

	// --- モーション（AnimBP 待機の上にモンタージュを単発で差し込む。プレイヤーと同じ運用）---

	// ラベル（GodBirdAnimTags）から BirdAnimMontageDataAsset を引く。未設定/未登録なら nullptr
	UAnimMontage* GetAnimMontage( const FName& MontageName ) const;

	// 戻り値は再生長（秒）で、再生できなければ 0。RateScale はプレイヤー側と同じく戻り値の補正に反映する
	float PlayAnimMontage( const FName& MontageName, float InPlayRate = 1.0f, FName StartSectionName = NAME_None );
	float PlayAnimMontage( UAnimMontage* AnimMontage, float InPlayRate = 1.0f, FName StartSectionName = NAME_None );
	void StopAnimMontage( UAnimMontage* AnimMontage = nullptr );	// nullptr なら現在再生中のものを停止

	// ST を再生し、流れ終わったら LP へ自動で繋ぐ（LP 側はモンタージュ内でループ設定する前提）。
	// 繋ぎは Tick でポーリングして判定し、片方が未登録なら取れた方だけ再生する
	void PlayMontageSequence( const FName& StartLabel, const FName& LoopLabel );

	// true の間は神技挙動（戯れの飛び回り・導きの突進）を保留してスポーン位置で ST を流す。
	// Loop へ移行すると false＝挙動開始の合図。ST モンタージュ未登録なら常に false
	bool IsStartMotionActive() const { return bStartMotionActive; }

protected:
	virtual void Tick( float DeltaTime ) override;

	void UpdateGuidanceCharge( float DeltaTime );	// 目標へ前進し、到達／寿命で自壊する
	void UpdateTornadoEscort( float DeltaTime );	// 竜巻へ飛ぶ→周回→戻る。戻りきると通常追従へ復帰

	// InStartScale>0 かつ開始モーション中なら開始スケールへ縮め、以降 ST 進捗で InTargetScale まで拡大する
	void SetupStartMotionScale( float InTargetScale, float InStartScale );
	void UpdateStartMotionScale();

	// メッシュ下端が地面より下へ行かないよう Z を持ち上げて返す（地面沿いは水平スキム移動になる）。
	// 初回呼び出しで地面クリアランスを一度だけ確定・キャッシュするため非 const
	FVector ClampGuidanceAboveGround( const FVector& DesiredLocation );

	void UpdateMontageSequence();		// ST が流れ終わったら LP へ移行する
	void UpdateFollow( float DeltaTime );	// Root を目標へ VInterp/RInterp で寄せる
	void UpdateFloat( float DeltaTime );	// BirdMesh のローカルオフセットを軸ごとの sin で揺らす
	bool GetDesiredFollowTransform( FVector& OutLocation, FRotator& OutRotation ) const;

public:
	// 補間追従で動かす本体。BirdMesh の親
	UPROPERTY( VisibleAnywhere, Category = "Tide|GodBird" )
	TObjectPtr<USceneComponent> Root;

	// 神鳥の見た目（スケルタルメッシュ＋待機モーション）。Root の子。浮遊はこのローカルオフセットに乗る
	UPROPERTY( VisibleAnywhere, Category = "Tide|GodBird" )
	TObjectPtr<USkeletalMeshComponent> BirdMesh;

	// 神鳥専用のモンタージュリスト（プレイヤーの AnimMontageDataAsset と同じ仕組みだが、スケルトン・モーション語彙が
	// 別物のためプレイヤーとは別アセットを割り当てる）。ラベルは GodBirdAnimTags を使う。BP サブクラスで割り当てる
	UPROPERTY( EditAnywhere, Category = "Tide|GodBird|Animation" )
	TObjectPtr<UAnimMontageListDataAsset> BirdAnimMontageDataAsset;

	// === ふわふわ浮遊（軸ごとに独立。周期をずらして直線的でない自然な揺れにする） ===

	// 上下（Z）浮遊の振幅（cm）。0 で無効
	UPROPERTY( EditAnywhere, Category = "Tide|GodBird|Float" )
	float FloatAmplitude = 20.0f;

	// 上下（Z）浮遊の周期（秒／1往復）。0 以下で無効
	UPROPERTY( EditAnywhere, Category = "Tide|GodBird|Float" )
	float FloatPeriod = 2.0f;

	// 左右（Y）浮遊の振幅（cm）。0 で無効
	UPROPERTY( EditAnywhere, Category = "Tide|GodBird|Float" )
	float FloatAmplitudeY = 12.0f;

	// 左右（Y）浮遊の周期（秒／1往復）。0 以下で無効
	UPROPERTY( EditAnywhere, Category = "Tide|GodBird|Float" )
	float FloatPeriodY = 3.1f;

	// 前後（X）浮遊の振幅（cm）。0 で無効
	UPROPERTY( EditAnywhere, Category = "Tide|GodBird|Float" )
	float FloatAmplitudeX = 10.0f;

	// 前後（X）浮遊の周期（秒／1往復）。0 以下で無効
	UPROPERTY( EditAnywhere, Category = "Tide|GodBird|Float" )
	float FloatPeriodX = 2.7f;

	// === 補間追従（若干ずれてついてくる） ===

	// 位置の補間速度（大きいほど速く追いつく＝ずれが小さい。小さいほど遅れて大きくずれる）
	UPROPERTY( EditAnywhere, Category = "Tide|GodBird|Follow" )
	float FollowLocationInterpSpeed = 10.0f;

	// 向きの補間速度（大きいほど速く向きが揃う）。下の機首上下傾きも同じ補間で滑らかに寄せる
	UPROPERTY( EditAnywhere, Category = "Tide|GodBird|Follow" )
	float FollowRotationInterpSpeed = 6.0f;

	// 追従中、プレイヤーの上昇/下降に合わせて機首を上下へ傾けるか（true で有効）
	UPROPERTY( EditAnywhere, Category = "Tide|GodBird|Follow" )
	bool bFollowVerticalPitch = true;

	// 垂直速度（cm/秒）あたりの機首傾き角（度）。上昇で機首上げ・下降で機首下げ。
	// 例：0.05 なら垂直速度 700cm/秒で 35 度（下の最大角に到達）
	UPROPERTY( EditAnywhere, Category = "Tide|GodBird|Follow", meta = ( EditCondition = "bFollowVerticalPitch" ) )
	float FollowVerticalPitchPerSpeed = 0.05f;

	// 機首傾きの最大角（度、±）。上昇/下降が速くてもこの角度でクランプする
	UPROPERTY( EditAnywhere, Category = "Tide|GodBird|Follow", meta = ( ClampMin = "0.0", EditCondition = "bFollowVerticalPitch" ) )
	float FollowVerticalPitchMax = 35.0f;

	// 導き突進中の向き補間速度（大きいほど速く進行方向を向く）。地面スキムへ切り替わる際のガクつきを抑える
	UPROPERTY( EditAnywhere, Category = "Tide|GodBird|Guidance" )
	float GuidanceFacingInterpSpeed = 10.0f;

	// 導き突進中の地面クリアランス（cm。地面からこの高さにメッシュ下端を保つ）。
	// 0以下なら発動時のメッシュ下端から自動算出する（毎フレーム再計算はせず一度だけ確定して跳ねを防ぐ）
	UPROPERTY( EditAnywhere, Category = "Tide|GodBird|Guidance", meta = ( ClampMin = "0.0" ) )
	float GuidanceGroundClearance = 150.0f;

private:
	// ST→LP シーケンス。ST が流れ終わったら LP を再生して両方クリアする（LP 中は SeqLoopMontage=nullptr）
	UPROPERTY( Transient )
	TObjectPtr<UAnimMontage> SeqStartMontage;
	UPROPERTY( Transient )
	TObjectPtr<UAnimMontage> SeqLoopMontage;

	bool bStartMotionActive = false;	// ST 再生中は神技挙動を保留し、Loop 移行で false（挙動開始）

	// ST 中に開始→最終スケールへ補間し、ST 終了で最終スケールへ確定して無効化する
	bool bStartScaleGrow = false;
	float StartScaleFrom = 1.0f;
	float StartScaleTo = 1.0f;

	float FloatElapsed = 0.0f;	// 浮遊の位相

	// --- 追従対象と、対象ローカル基準のオフセット/向き ---
	TWeakObjectPtr<AActor> FollowLeader;
	FVector FollowLocalOffset = FVector::ZeroVector;
	FRotator FollowRotationOffset = FRotator::ZeroRotator;
	FName FollowSocketName = NAME_None;	// NAME_None ならアクター原点基準
	bool bStickToTarget = false;		// 滑空中の手元固定。補間・ふわふわを止め毎フレームスナップする
	bool bExternallyDriven = false;		// 一閃 mover が位置を直接動かす間は補間追従・浮遊を停止する

	// 構え中にカメラ方向へ向ける。位置は従来追従のまま、向きだけ差し替える
	bool bUseFacingOverride = false;
	FRotator FacingOverrideBase = FRotator::ZeroRotator;

	// --- 導きの突進（true の間は追従・浮遊をせず、目標へ前進して寿命/到達で自壊する）---
	bool bGuidanceCharging = false;
	FVector GuidanceTargetLocation = FVector::ZeroVector;
	float GuidanceSpeed = 0.0f;
	float GuidanceMaxDuration = 0.0f;
	float GuidanceElapsed = 0.0f;

	// 発動時に一度だけ確定し、突進中は一定に保つ（毎フレーム bounds から取ると
	// 待機アニメ・回頭で上下に跳ねたり一瞬潜ったりするため）
	float CachedGuidanceClearance = 0.0f;

	// --- SlidePassive 竜巻エスコート演出。None で通常追従へ復帰 ---
	enum class EEscortPhase : uint8 { None, FlyTo, Orbit, Return };
	EEscortPhase EscortPhase = EEscortPhase::None;
	FVector EscortCenter = FVector::ZeroVector;	// 竜巻中心（地面）
	float EscortRadius = 0.0f;					// 周回半径
	float EscortHeight = 0.0f;					// 周回し終わりの高さ（中心からの +Z）
	float EscortStartHeight = 0.0f;				// 周回し始めの高さ（中心からの +Z）
	float EscortSpeed = 0.0f;					// 飛行速度（向かう／戻る）
	float EscortAngularSpeedDeg = 0.0f;			// 周回角速度（度/秒）
	float EscortAngleDeg = 0.0f;				// 現在の周回角度（度）
	float EscortTurnsTotalDeg = 0.0f;			// 周回の総角度（度）。上昇の進行率算出に使う
	float EscortTurnsRemainingDeg = 0.0f;		// 残りの周回角度（度）。0 で戻りへ

	// 進行方向へ機首を向ける（メッシュ補正込み・RInterp）。飛行・周回で共用
	void FaceTravelDirection( const FVector& Dir, float DeltaTime, float InterpSpeed );
	FVector EscortOrbitPoint( float AngleDeg ) const;	// 中心 + 半径方向 + 現在の周回高さ
	float CurrentEscortOrbitHeight() const;				// 開始高さ→終了高さへ上がっていく
};
