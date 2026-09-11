// Copyright (c) 2026, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "SlidePassiveSubModule.generated.h"

class ATidePlayerCharacter;
class USlidePassivePlayerModule;
class UTidePlayerParamDataAsset;

// スライドパッシブ サブモジュール 基底。効果は今後増える想定なので、検出条件＋効果の単位で本クラスを継承して足す
// （現状は円→竜巻の Tornado と、S字→風まとい＋ギアMAX＋次チャージアクションのバフの Gust）。
// 親 USlidePassivePlayerModule がサンプラ＋共有トレイル＋束ねを担当して軌跡サンプル（足元位置と符号付き旋回量）を
// 配信し、サブモジュールは自前の履歴で検出して効果を発火する
UCLASS( Abstract )
class USlidePassiveSubModule : public UObject
{
	GENERATED_BODY()

public:
	// 親モジュール／キャラクターを保持する
	virtual void Initialize( USlidePassivePlayerModule* InOwnerModule, ATidePlayerCharacter* InOwner );

	// この機能が有効か（PlayerParamData の機能フラグ判定）。無効なら親はサンプル配信しても無視される
	virtual bool IsEnabled() const { return true; }

	// チャージダッシュ猶予（SlidePassiveChargeDashGraceTime）中もサンプルを受け取るか。
	// false を返すと猶予期間はサンプルが配信されず、その間は図形を完成できない（トレイルは出たまま）。
	virtual bool CanDetectDuringChargeDashGrace() const { return true; }

	// スライド（軌跡記録）が開始した
	virtual void OnTrackingStarted() {}
	// 新しい軌跡サンプルが追加された（Foot=足元位置、SignedTurnDeltaDeg=前サンプルからの符号付き旋回量）
	virtual void OnSampleAdded( const FVector& Foot, float SignedTurnDeltaDeg ) {}
	// 親がトレイル完成フラッシュ等で軌跡をリセットした。自前履歴もクリアする
	virtual void OnTrajectoryReset() {}
	// スライドが終了した（bChargeReleased=チャージ解除による終了か）
	virtual void OnTrackingStopped( bool bChargeReleased ) {}
	// 毎フレーム（保留タイマー等の進行用。記録中でなくても呼ばれる）
	virtual void OnUpdate( float DeltaTime ) {}

#if !UE_BUILD_SHIPPING
	virtual void DrawDebugImGui() {}
#endif

protected:
	const UTidePlayerParamDataAsset* GetParams() const;

	UPROPERTY( Transient )
	TObjectPtr<USlidePassivePlayerModule> OwnerModule;

	UPROPERTY( Transient )
	TObjectPtr<ATidePlayerCharacter> OwnerCharacter;
};
