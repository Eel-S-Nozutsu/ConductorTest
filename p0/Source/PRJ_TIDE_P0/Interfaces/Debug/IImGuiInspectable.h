// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "IImGuiInspectable.generated.h"

UINTERFACE(MinimalAPI, NotBlueprintable)
class UImGuiInspectable : public UInterface
{
	GENERATED_BODY()

};

/**
 * 右クリックインスペクターにカスタム情報を表示するインターフェース
 * DrawImGuiInspector()に表示したいImGui描画を実装する
 * 名前・クラス・Transformはシステム側が自動で表示するため実装不要
 */
class PRJ_TIDE_P0_API IImGuiInspectable
{
	GENERATED_BODY()

public:

	virtual void DrawImGuiInspector() = 0;

};
