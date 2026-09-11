// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PRJ_TIDE_P0/Data/Director/ContentDirectorPhaseRow.h"
#include "PRJ_TIDE_P0/Data/Director/DirectorActorPhaseRow.h"
#include "PRJ_TIDE_P0/Director/DirectorObjectBase.h"
#include "ContentDirector.generated.h"

class UContentDirector;
class UContentDirectorModule;
class UDirectorCondition;
struct FContentDirectorRow;
struct FDirectorActorRow;

DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnContentPhaseChanged, UContentDirector* /*Director*/, FName /*OldPhase*/, FName /*NewPhase*/);

/**
 * 1コンテンツ(修練場1つなど)の進行役。
 * 現フェーズを保持し、DT_ContentDirectorPhaseの記述に従って遷移する。
 * 遊びの中身は持たず、それはUContentDirectorModuleの仕事。
 */
UCLASS(BlueprintType)
class PRJ_TIDE_P0_API UContentDirector : public UDirectorObjectBase
{
	GENERATED_BODY()

public:

	// UMapDirectorから呼ぶ
	void StartDirector(FName InContentId, const FContentDirectorRow& Row);
	void StopDirector();
	void TickDirector(float DeltaSeconds, float EvaluateInterval);

	UFUNCTION(BlueprintPure, Category = "Tide|Director")
	FName GetCurrentPhase() const { return CurrentPhase; }

	UFUNCTION(BlueprintPure, Category = "Tide|Director")
	FName GetContentId() const { return ContentId; }

	// 条件を待たずにフェーズを移す。Module/Action/デバッグからの明示要求
	// ※要求は次のTickで処理する(遷移処理中の再入で配列を壊さないため)
	UFUNCTION(BlueprintCallable, Category = "Tide|Director")
	void RequestPhase(FName NextPhase);

	// 同じフェーズへ入り直す。やり直しはこれで表す
	UFUNCTION(BlueprintCallable, Category = "Tide|Director")
	void RestartPhase();

	UFUNCTION(BlueprintCallable, Category = "Tide|Director", meta = (DeterminesOutputType = "ModuleClass"))
	UContentDirectorModule* FindModuleByClass(TSubclassOf<UContentDirectorModule> ModuleClass) const;

	template <typename T>
	T* FindModule() const
	{
		return Cast<T>(FindModuleByClass(T::StaticClass()));
	}

	// --- 管理アクター ---

	// ActorIdの実体を返す。生成済みならその実体、レベル配置なら札(UDirectorIdComponent)で引いたもの
	UFUNCTION(BlueprintCallable, Category = "Tide|Director")
	AActor* FindManagedActor(FName ActorId);

	UFUNCTION(BlueprintCallable, Category = "Tide|Director")
	TArray<AActor*> GetGroupActors(FName GroupId);

	// グループのうち生存している数。StatusComponentを持たないアクターは存在だけで生存扱い
	UFUNCTION(BlueprintCallable, Category = "Tide|Director")
	int32 CountAliveInGroup(FName GroupId);

	FOnContentPhaseChanged OnPhaseChanged;

	// --- デバッグ用アクセサ ---

	// このコンテンツに定義されている全フェーズ(DTの記述順)
	TArray<FName> GetDefinedPhases() const;

	// 現フェーズの出口を「条件の説明, 遷移先, 現在の真偽」で返す
	void GetActiveTransitionsDebug(TArray<TTuple<FString, FName, bool>>& OutRows) const;

	const TArray<TObjectPtr<UContentDirectorModule>>& GetModules() const { return Modules; }

private:

	void EnterPhase(FName NewPhase);
	void EvaluateTransitions();

	// 現フェーズの出口ぶんの条件を作る/捨てる。DTのTransitionsと同じ順に並べる
	void BuildConditions(const FContentDirectorPhaseRow& PhaseRow);
	void ClearConditions();
	const FContentDirectorPhaseRow* FindPhaseRow(FName Phase) const;

	// 起動時に参照の辻褄を検査してログに出す。別コンテンツの表を差した行もここで拾う
	void ValidateTables(FName InitialPhaseName) const;

	void ApplyActorsForPhase(FName Phase);
	void ApplyPresence(FName ActorId, const FDirectorActorRow& ActorRow, EDirectorActorPresence Presence, float HealthRatio);
	const FDirectorActorPhaseRow* FindActorPhaseRow(FName ActorId, FName Phase) const;
	void ForEachActorRow(TFunctionRef<void(FName, const FDirectorActorRow&)> Visitor) const;

	AActor* ResolvePlacedActor(FName ActorId);
	void ScanPlacedActors();
	AActor* EnsureSpawned(FName ActorId, const FDirectorActorRow& Row);
	void DestroySpawned(FName ActorId);
	void DestroyAllSpawned();

	UPROPERTY()
	TArray<TObjectPtr<UContentDirectorModule>> Modules;

	// 現フェーズの条件の実体。添字はDTのTransitionsに対応し、条件未設定の要素はnullptr
	UPROPERTY()
	TArray<TObjectPtr<UDirectorCondition>> PhaseConditions;

	// ソフト参照のままだとGCで落ちるので実体を保持する
	UPROPERTY()
	TObjectPtr<UDataTable> PhaseTable;

	UPROPERTY()
	TObjectPtr<UDataTable> ActorTable;

	UPROPERTY()
	TObjectPtr<UDataTable> ActorPhaseTable;

	// 札で引いたレベル配置アクター。走査は必要になってから一度だけ行う
	TMap<FName, TWeakObjectPtr<AActor>> PlacedActors;

	// ディレクターが生成した実体。片付けの責任もこちらが持つ
	TMap<FName, TWeakObjectPtr<AActor>> SpawnedActors;

	FName ContentId;
	FName CurrentPhase;
	FName PendingPhase;
	float EvaluateAccumulator = 0.0f;
	bool  bStarted            = false;
	bool  bPhasePending       = false;

};
