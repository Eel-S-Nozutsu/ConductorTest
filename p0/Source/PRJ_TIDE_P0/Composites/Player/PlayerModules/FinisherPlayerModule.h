// Copyright (c) 2026, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "PRJ_TIDE_P0/Composites/Player/PlayerModules/TidePlayerModule.h"

#include "FinisherPlayerModule.generated.h"

// とどめ（フィニッシュ）演出モジュール。致死ヒットを与えた瞬間にワールド全体のスローモーションを起動する
// （プレイヤーだけは等速側へ補正。Dodge/God の演出と同流儀）
UCLASS()
class UFinisherPlayerModule : public UTidePlayerModule
{
	GENERATED_BODY()

public:
	virtual void OnModuleUpdate( float DeltaTime ) override;

	// とどめスローを起動する。発動中に再度呼ばれた場合は復帰時間をリフレッシュするだけ。
	void TriggerFinisher();

	bool IsFinisherActive() const { return bIsActive; }

private:
	void ResetTimeDilation();

private:
	bool bIsActive = false;
	// 復帰までの残り「実時間」（秒）。グローバル/カスタムのタイムダイレーションに影響されない
	// FApp::GetDeltaTime() で減算するため、スロー中でも実時間どおりにカウントダウンできる。
	float RemainingRealTime = 0.0f;
};
