// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "StateTreeSchema.h"
#include "StateTreeExecutionTypes.h"
#include "ConductorStateTreeSchema.generated.h"

class UContentConductor;
struct FStateTreeExecutionContext;

/**
 * ContentConductor用 StateTreeスキーマ
 */
UCLASS(BlueprintType, EditInlineNew, CollapseCategories, meta = (DisplayName = "Conductor Content", CommonSchema))
class CONDUCTOR_API UConductorStateTreeSchema : public UStateTreeSchema
{
	GENERATED_BODY()

public:
	UConductorStateTreeSchema();

	static const FName ContextName_Conductor;

	// ホストがコンテキストを設定 揃っていればtrue
	static bool SetContextRequirements(
		UContentConductor& Conductor, FStateTreeExecutionContext& Context, bool bLogErrors = false);

protected:
	virtual bool IsStructAllowed(const UScriptStruct* InScriptStruct) const override;
	virtual bool IsClassAllowed(const UClass* InClass) const override;
	virtual bool IsExternalItemAllowed(const UStruct& InStruct) const override;

	virtual TConstArrayView<FStateTreeExternalDataDesc> GetContextDataDescs() const override;

	UPROPERTY()
	TArray<FStateTreeExternalDataDesc> ContextDataDescs;
};
