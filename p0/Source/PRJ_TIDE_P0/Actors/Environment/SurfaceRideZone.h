// Copyright (c) 2026, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SurfaceRideZone.generated.h"

class UBoxComponent;

/**
 * 面沿い移動モード（スケボー的な壁面走行）を許可するエリア。
 *
 * 面沿いモードは重力方向を床法線へ向けて「その面を床にする」ため、無制限に効かせると
 * なんでもない壁を登れてしまう。そこでハーフパイプ・トンネル等の意図した場所にこのエリアを置き、
 * **中に居る間だけ開始を許可**する（PlayerParamData.bSurfaceRideRequireZone が ON のとき）。
 *
 * 判定はオーバーラップのイン／アウトのみで Tick を持たない。プレイヤー側はエリアの重複数を
 * カウントするので、複数のエリアを重ねて長いトンネルを覆っても正しく動く。
 *
 * エリアを出たら面沿いは解除される。急斜面（＝壁）の上で出た場合は溜まった沿面速度が射出になるため、
 * チャージごと終了して速度を削る（HandleSurfaceRideZoneExitRelease。削る割合は PlayerParamData の
 * SurfaceRideZoneExit 系）。緩い床で出た場合は射出の恐れが無いので速度もチャージも触らない。
 * 接地・空中のどちらで出ても同じ扱いになる（空中は乗っていた面の傾斜で急斜面かを判定する）。
 *
 * エリア単位で挙動を差し替えるフラグも持つ（トンネルの操作系と、上り坂アシスト＝物理準拠をやめて
 * 上りでも減速させない「嘘」）。数値は PlayerParamData 側で、エリアは ON/OFF だけを持つ。
 *
 * 詳細は [Docs/PlayerArchitecture.md] の「面沿い移動モード」を参照。
 */
UCLASS()
class PRJ_TIDE_P0_API ASurfaceRideZone : public AActor
{
	GENERATED_BODY()

public:
	ASurfaceRideZone();

protected:
	virtual void BeginPlay() override;
	// インゲーム中のエリア可視化と、開始時から重なっていた場合の取りこぼし解決に使う
	virtual void Tick( float DeltaTime ) override;

	// プレイヤーの在籍状態を設定する。同じ状態への再設定は無視する（イベントと開始時検知の二重登録防止）
	void SetPlayerInside( class ATidePlayerCharacter* Player, bool bInside );

	// 開始時点でエリアに重なっているプレイヤーを拾う。取れたら true
	bool ResolveInitialOverlap();

	UFUNCTION()
	void OnAreaBeginOverlap( UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult );

	UFUNCTION()
	void OnAreaEndOverlap( UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex );

public:
	// エリア形状。エディタで引き伸ばしてトンネル・ハーフパイプを覆う
	UPROPERTY( VisibleAnywhere, Category = "Tide|SurfaceRideZone" )
	TObjectPtr<UBoxComponent> AreaBox;

	// 【検証・原案】トンネル挙動。ON のエリアでは、面沿いモード中にプレイヤーが 90°以上の張り付き
	// （上方向が下を向く＝天井側）になったとき左右移動を反転させる。天井で操作が鏡像に見えるのを打ち消す狙い
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "Tide|SurfaceRideZone|Tunnel" )
	bool bInvertLateralInput = false;

	// 【検証】上記と同条件（90°以上の張り付き）で、上下（前後）移動入力を反転させる。左右反転とは独立
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "Tide|SurfaceRideZone|Tunnel" )
	bool bInvertForwardInput = false;

	// 【検証・B案】ON のエリアでは面沿い中の左右入力を「画面基準」にする。右ベクトルをカメラ右から作るため、天井側へ
	// 回り込んで上下が反転しても左右が鏡像にならない（OFF なら面の上方向基準＝天井側で鏡像に見える）。
	// ON だと鏡像自体が起きないので bInvertLateralInput の併用は逆に鏡像化する（二者択一で試す想定）
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "Tide|SurfaceRideZone|Tunnel" )
	bool bScreenRelativeInput = false;

	// 【検証・A案】ON のエリアでは面沿い中にカメラの上方向をプレイヤーの上方向（重力方向）へロール追従させる
	// （画面の上＝プレイヤーの上になるので 1周する筒でも操作と画面が一致する）。入力リマップとは別軸なので単体で試す想定
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "Tide|SurfaceRideZone|Tunnel" )
	bool bCameraRollFollowUp = false;

	// 【検証・方式1】ON のエリアでは面沿い中カメラを進行方向の背後へ自動追従させ、フリールックを解除する。前が常に
	// 画面奥になりフレームが安定する（バンクを含む）。移動入力もこのカメラ向き基準になり、方式2との併用でレース式トンネルになる
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "Tide|SurfaceRideZone|Tunnel" )
	bool bChaseCameraFollowTravel = false;

	// 【検証・方式2】ON のエリアでは、面沿いモード中の移動入力をチューブ基準にする。
	// 前入力＝現在の進行方向へ加速／左右入力＝筒の円周を回り込む。カメラに依存せず操作が安定する（セミオンレール）
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "Tide|SurfaceRideZone|Tunnel" )
	bool bTubeRelativeInput = false;

	// ON のエリアでは面沿い中の「戻る力」と登り加速を DA のアシスト値（SurfaceRideAssist*）へ差し替え、上り坂でも
	// ほとんど減速せず登り成分に比例して少し伸びるようにする（OFF なら物理準拠で |g|×sin で滑り落ちる）
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "Tide|SurfaceRideZone|Assist" )
	bool bUphillAssist = false;

	// ON でインゲーム中もエリア形状を描画する（プレイヤー在籍中は緑、それ以外は水色）
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "Tide|SurfaceRideZone|Debug" )
	bool bDebugDraw = false;

	// プレイヤーが今このエリアに入っているか（デバッグ描画の色分けに使う）
	bool IsPlayerInside() const { return bPlayerInside; }

private:
	bool bPlayerInside = false;

	// 開始時オーバーラップの解決が済んだか。プレイヤーの生成順に依存しないよう、
	// BeginPlay で取れなければ取れるまで Tick で再試行する
	bool bInitialOverlapResolved = false;
};
