// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/KismetSystemLibrary.h"

DECLARE_LOG_CATEGORY_EXTERN(LogConductor, Log, All);

#define UE_SCREEN_LOG_ERROR(WorldContextObject, Format, ...)                                                          \
	{                                                                                                                 \
		FString ErrorMessage = FString::Printf(Format, ##__VA_ARGS__);                                                \
		UE_LOG(LogConductor, Error, TEXT("%s"), *ErrorMessage);                                                       \
		if (GEngine)                                                                                                  \
		{                                                                                                             \
			UKismetSystemLibrary::PrintString(WorldContextObject, ErrorMessage, true, true, FLinearColor::Red, 2.0f); \
		}                                                                                                             \
	}

#define UE_SCREEN_LOG_WARNING(WorldContextObject, Format, ...)                                                             \
	{                                                                                                                      \
		FString WarningMessage = FString::Printf(Format, ##__VA_ARGS__);                                                   \
		UE_LOG(LogConductor, Warning, TEXT("%s"), *WarningMessage);                                                        \
		if (GEngine)                                                                                                       \
		{                                                                                                                  \
			UKismetSystemLibrary::PrintString(WorldContextObject, WarningMessage, true, true, FLinearColor::Yellow, 2.0f); \
		}                                                                                                                  \
	}
