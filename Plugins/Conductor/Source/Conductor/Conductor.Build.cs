// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Conductor : ModuleRules
{
	public Conductor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange([
			"Core",
			"CoreUObject",
			"Engine",
			"DeveloperSettings"
			]);
	}
}
