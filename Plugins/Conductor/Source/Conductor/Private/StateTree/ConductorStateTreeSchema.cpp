// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#include "StateTree/ConductorStateTreeSchema.h"

#include "ContentConductor.h"
#include "ConductorLog.h"

#include "StateTreeConditionBase.h"
#include "StateTreeConsiderationBase.h"
#include "StateTreeEvaluatorBase.h"
#include "StateTreeExecutionContext.h"
#include "StateTreePropertyFunctionBase.h"
#include "StateTreeTaskBase.h"

const FName UConductorStateTreeSchema::ContextName_Conductor = FName(TEXT("Conductor"));

UConductorStateTreeSchema::UConductorStateTreeSchema()
	// Guidはアセットのバインドが差すIDになるので、後から変えると既存アセットの線が切れる
	: ContextDataDescs({ { ContextName_Conductor, UContentConductor::StaticClass(), FGuid(0x7C4A1E20, 0x38F04B96, 0xA1D25C07, 0x6E930B44) } })
{
}

bool UConductorStateTreeSchema::IsStructAllowed(const UScriptStruct* InScriptStruct) const
{
	return InScriptStruct->IsChildOf(FStateTreeConditionCommonBase::StaticStruct())
		|| InScriptStruct->IsChildOf(FStateTreeEvaluatorCommonBase::StaticStruct())
		|| InScriptStruct->IsChildOf(FStateTreeTaskCommonBase::StaticStruct())
		|| InScriptStruct->IsChildOf(FStateTreeConsiderationCommonBase::StaticStruct())
		|| InScriptStruct->IsChildOf(FStateTreePropertyFunctionCommonBase::StaticStruct());
}

bool UConductorStateTreeSchema::IsClassAllowed(const UClass* InClass) const
{
	return IsChildOfBlueprintBase(InClass);
}

bool UConductorStateTreeSchema::IsExternalItemAllowed(const UStruct& InStruct) const
{
	// 外部データの収集コールバックを繋いでいないので、今は受け付けない
	return false;
}

TConstArrayView<FStateTreeExternalDataDesc> UConductorStateTreeSchema::GetContextDataDescs() const
{
	return ContextDataDescs;
}

bool UConductorStateTreeSchema::SetContextRequirements(UContentConductor& Conductor, FStateTreeExecutionContext& Context, bool bLogErrors)
{
	if (!Context.IsValid()) return false;

	Context.SetContextDataByName(ContextName_Conductor, FStateTreeDataView(&Conductor));

	const bool bResult = Context.AreContextDataViewsValid();
	if (!bResult && bLogErrors)
	{
		UE_LOG(LogConductor, Error, TEXT("[Conductor] %s: StateTreeのコンテキストが揃わない (スキーマがConductor Contentか確認)"), *Conductor.GetContentId().ToString());
	}

	return bResult;
}
