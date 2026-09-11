// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ConductorTest : ModuleRules
{
	public ConductorTest(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange([
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
            "GameplayTags",
			"DeveloperSettings"
			]);

		PrivateDependencyModuleNames.AddRange([
			]);

		PublicIncludePaths.Add(Target.ProjectFile.GetFileNameWithoutExtension());
	}
}
