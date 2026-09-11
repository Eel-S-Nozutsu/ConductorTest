// Copyright (c) 2026, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PRJ_TIDE_P0/Composites/Player/PlayerModules/TidePlayerModule.h"

#include "GodBirdPlayerModule.generated.h"

class ATideGodBird;
class UNiagaraComponent;

// 神鳥（お供精霊）。チャージ中と神技中はプレイヤーの横に神鳥を出し、どちらでもなくなったら消す。
// 実体は GodBirdActorClass の BP を1体だけ遅延スポーンして使い回し（チャージ毎の生成/破棄はしない）、
// プレイヤーへアタッチして GodBirdSideOffset で追従させる。表示/非表示は SetVisible で切り替え、
// 待機モーションは継続する
UCLASS()
class UGodBirdPlayerModule : public UTidePlayerModule
{
	GENERATED_BODY()

public:
	virtual void Initialize( class ATidePlayerCharacter* InOwner ) override;
	virtual void OnModuleUpdate( float DeltaTime ) override;

	// 神技を除いた表示条件（チャージ系・滑空中。空中の振り下ろしは除外）を満たすか。戯れの鳥が
	// 帰り着いたあと常駐お供が出たままかを知るために使う（出ないなら消失エフェクトで締める）
	bool IsShownByPlayerAction() const;

	// 未生成なら生成する。神技「戯れ」が一閃 mover として鳥を借りるための入口
	ATideGodBird* GetOrSpawnBird();

	// 竜巻の生成演出として、神鳥を竜巻のまわりへ周回させる（開始できたら true）。周回のパラメータは
	// DA から引くので呼び出し側は中心だけ渡す＝神鳥アクターを触らせないための入口
	bool StartTornadoEscort( const FVector& TornadoCenter );

private:
	void EnsureGodBirdSpawned();	// 初回チャージ時に遅延生成し、プレイヤーへアタッチする
	void ShowGodBird();
	void HideGodBird();

	// 追従アンカー（プレイヤーに対する定位置）。滑空＝手元固定／突風バフ中＝正面／通常＝横
	enum class EGodBirdAnchor : uint8 { Side, Hand, GustFront };
	EGodBirdAnchor ComputeDesiredAnchor() const;	// 滑空 > 突風正面 > 横 の優先

	// bSnap=true で即スナップ、bVFX=true で消失→出現エフェクトを挟む（位置が飛ぶ手元用）
	void ApplyAnchor( EGodBirdAnchor Anchor, bool bSnap, bool bVFX );

	// 以下いずれもタグ未割当なら何もしない
	void SpawnGodBirdVFX( const FName& NiagaraTag );
	void StartGodBirdTrail();	// 表示中ずっと出すトレイル
	void StopGodBirdTrail();
	void StartGustSourceVFX();	// 突風バフ中、神鳥から風が出ているように見せる
	void StopGustSourceVFX();

	// GODBIRD_TRAIL が鳥の高速飛行では軌跡を引けないため、憑依の剣と同じリボンを鳥メッシュへ流用する
	void StartFrolicSwordTrail();
	void StopFrolicSwordTrail();

private:
	// 1体だけ生成して使い回す
	UPROPERTY( Transient )
	TObjectPtr<ATideGodBird> GodBirdActor;

	// 非表示時に破棄する
	UPROPERTY( Transient )
	TObjectPtr<UNiagaraComponent> TrailVFX;

	UPROPERTY( Transient )
	TObjectPtr<UNiagaraComponent> FrolicSwordTrailVFX;

	// 正面から離れたら破棄する
	UPROPERTY( Transient )
	TObjectPtr<UNiagaraComponent> GustSourceVFX;

	bool bWasVisible = false;	// 立ち上がり/立ち下がり検知用

	// 表示開始時に状態から決め、以降は変化時のみ張り替える
	EGodBirdAnchor CurrentAnchor = EGodBirdAnchor::Side;

	bool bTrailBoosted = false;	// 状態変化時のみ色を貼り直すためのエッジ検出用
};
