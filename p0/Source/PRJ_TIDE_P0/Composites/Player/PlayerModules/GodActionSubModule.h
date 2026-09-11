// Copyright (c) 2026, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "GodActionSubModule.generated.h"

class ATidePlayerCharacter;
class UGodActionPlayerModule;
class UTidePlayerParamDataAsset;
class UAnimMontage;

// 神技サブモジュール 基底。神技は種類が増える想定なので、1 神技 = 本クラスを継承した 1 サブモジュールで足す
// （現状は一閃の UGodSlashSubModule）。傘モジュール UGodActionPlayerModule が共通のゲージを持ち、毎フレーム更新と
// デバッグ描画を橋渡しする（ゲージの照会・消費は OwnerModule 経由）。本基底はライフサイクルの足場のみを持つ
UCLASS( Abstract )
class UGodActionSubModule : public UObject
{
	GENERATED_BODY()

public:
	// 親モジュール／キャラクターを保持する
	virtual void Initialize( UGodActionPlayerModule* InOwnerModule, ATidePlayerCharacter* InOwner );

	// この神技が有効か（機能フラグ判定）。無効なら傘は更新・入力を配らない
	virtual bool IsEnabled() const { return true; }

	// 毎フレーム更新（傘の OnModuleUpdate から呼ばれる）
	virtual void OnModuleUpdate( float DeltaTime ) {}

#if !UE_BUILD_SHIPPING
	virtual void DrawDebugImGui() {}
#endif

protected:
	const UTidePlayerParamDataAsset* GetParams() const;

	// OwnerCharacter 経由の名前引きモンタージュ再生（UTidePlayerModule と同流儀）
	float PlayAnimMontage( const FName& MontageName, float InPlayRate = 1.0f, FName StartSectionName = NAME_None );

	UPROPERTY( Transient )
	TObjectPtr<UGodActionPlayerModule> OwnerModule;

	UPROPERTY( Transient )
	TObjectPtr<ATidePlayerCharacter> OwnerCharacter;
};
