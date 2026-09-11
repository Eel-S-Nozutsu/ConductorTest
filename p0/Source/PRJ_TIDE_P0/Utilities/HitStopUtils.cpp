// Copyright (c) 2025, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#include "HitStopUtils.h"

#include "Engine/Engine.h"
#include "PRJ_TIDE_P0/Core/TideGameSettings.h"

namespace HitStopUtil
{
	namespace Internal
	{
		static const FName HitStopTag(TEXT("HitStopActive"));

		struct FHitStopEntry
		{
			float        PrevDilation = 1.0f;
			FTimerHandle TimerHandle;

		};

		// static TMapのためレベル破棄後も残るため、TWeakObjectPtrをキーにする
		// 生AActor*だとポインタ再利用時に別アクターと誤マッチし、
		// 古いPrevDilationを書き戻す不整合が起きうる
		static TMap<TWeakObjectPtr<AActor>, FHitStopEntry> ActiveEntries;

		static void RestoreActor(AActor* Actor)
		{
			if (!IsValid(Actor)) { return; }

			FHitStopEntry* Entry = ActiveEntries.Find(Actor);
			if (!Entry) { return; }

			if (UWorld* World = Actor->GetWorld())
			{
				World->GetTimerManager().ClearTimer(Entry->TimerHandle);
			}

			Actor->CustomTimeDilation = Entry->PrevDilation;
			Actor->Tags.Remove(HitStopTag);
			ActiveEntries.Remove(Actor);
		}
	}

	// -----------------------------------------------------------------------
	//  Public API
	// -----------------------------------------------------------------------

	void ApplyHitStop(AActor* Actor, float DurationSec, float Dilation)
	{
		if (!IsValid(Actor)) { return; }

		UWorld* World = Actor->GetWorld();
		if (!World) { return; }

		// デバッグフラグがONのときはHitStopUtil経由のヒットストップを一括で行わない
		// ここでゲートすることで、本ユーティリティを通す全経路
		// (攻撃Notify・弾・各モジュール等)をまとめて無効化できる
		// HitStopUtilを通さない独自のスロー演出(神技・とどめ等)はサポート外
		if (GEngine)
		{
			if (const UTideGameSettings* Settings = UTideGameSettings::Get())
			{
				if (Settings->bDebugFlagDisableHitStop) { return; }
			}
		}

		// 多重適用を抑制
		if (Internal::ActiveEntries.Contains(Actor)) { return; }

		const float ClampedDilation = FMath::Clamp(Dilation, 0.0f, 1.0f);

		Internal::FHitStopEntry& Entry = Internal::ActiveEntries.Add(Actor);
		Entry.PrevDilation = Actor->CustomTimeDilation;
		Actor->CustomTimeDilation = ClampedDilation;
		Actor->Tags.AddUnique(Internal::HitStopTag);

		// タイマーハンドルはエントリに保持するため、ローカル消滅の問題なし
		const FTimerDelegate Delegate = FTimerDelegate::CreateWeakLambda(Actor, [Actor]()
			{
				Internal::RestoreActor(Actor);
			});

		World->GetTimerManager().SetTimer(Entry.TimerHandle, Delegate, DurationSec, false);
	}

	void CancelHitStop(AActor* Actor)
	{
		Internal::RestoreActor(Actor);
	}

}
