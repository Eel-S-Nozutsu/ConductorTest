// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/TriggerVolume.h"
#include "GimmickRoomVolume.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FGimmickRoomPlayerEvent);

/**
 * ギミックルーム(ワープ先のミニゲーム部屋)全体を囲むトリガーボリューム。
 * 在室の開始・終了を1点で捉え、
 *   - 在室中のリスタート地点を「祠の前」へ上書き (URestartComponentの上書き層)
 *   - 入室時にルーム内ギミックを初期状態へ戻す (IGimmickResettable)
 * を行う。
 *
 * ワープそのものは既存のBP_Teleportに任せる。プレイヤーがワープで出入りすれば
 * このボリュームのOverlapが必ず切り替わるので、テレポート側に手を入れる必要はない。
 *
 * ARestartTriggerVolumeとの棲み分け:
 * ルームの地点をARestartTriggerVolume(ProgressOrderで前進のみ)で与えると、退室後も
 * 本線の復帰点がルーム内に居座る(もしくは順番が小さくて採用されない)。
 * 寄り道であるルームの地点は在室スコープの上書きとして持ち、退室で捨てる。
 */
UCLASS()
class PRJ_TIDE_P0_API AGimmickRoomVolume : public ATriggerVolume
{
	GENERATED_BODY()

public:

	// 入室・退室の通知。IGimmickResettableを実装できないBPギミックはここから拾う
	UPROPERTY(BlueprintAssignable, Category = "Tide|GimmickRoom")
	FGimmickRoomPlayerEvent OnPlayerEnterRoom;

	UPROPERTY(BlueprintAssignable, Category = "Tide|GimmickRoom")
	FGimmickRoomPlayerEvent OnPlayerExitRoom;

protected:

	virtual void BeginPlay() override;
	virtual void NotifyActorBeginOverlap(AActor* OtherActor) override;
	virtual void NotifyActorEndOverlap(AActor* OtherActor) override;

	// 在室中の復帰地点(祠の前)。ここに指したアクターのTransformへ復帰する ※ATargetPoint推奨
	//
	// ソフト参照なのはボリュームと別レベル(サブレベル)のアクターを指せるようにするため。
	// ハード参照(TObjectPtr)はクロスレベル参照が許されず(FObjectProperty::AllowCrossLevel は
	// CPF_InstancedReference のみ true)、指定してもレベル保存でNoneに戻る
	// (LogProperty: Illegal TEXT reference to a private object in external package)
	UPROPERTY(EditInstanceOnly, Category = "Tide|GimmickRoom", meta = (DisplayName = "リスタート地点"))
	TSoftObjectPtr<AActor> RestartTargetActor;

	// 初期状態へ戻すルーム内ギミック (IGimmickResettableの実装アクター。BP実装も可)
	UPROPERTY(EditInstanceOnly, Category = "Tide|GimmickRoom", meta = (DisplayName = "リセットするギミック"))
	TArray<TSoftObjectPtr<AActor>> ResetTargets;

	// 入室時にギミックを戻す ※やり直しの起点を「入りなおし」に揃える既定
	UPROPERTY(EditAnywhere, Category = "Tide|GimmickRoom")
	bool bResetGimmicksOnEnter = true;

	// 退室時にも戻す ※メッシュ再構築のヒッチを入室(ワープ直後)から退室側へ寄せたいとき
	UPROPERTY(EditAnywhere, Category = "Tide|GimmickRoom")
	bool bResetGimmicksOnExit = false;

private:

	// 復帰地点を解決してキャッシュする。祠が別レベルにある場合、そのレベルが
	// 読み込まれている間にしか解決できないのでBeginPlayと入室時の両方で試みる
	bool TryCacheRestartPoint();

	void ResetGimmicks();

	FVector CachedRestartLocation = FVector::ZeroVector;
	FRotator CachedRestartRotation = FRotator::ZeroRotator;
	bool bRestartPointCached = false;

};
