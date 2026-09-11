// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class PRJ_TIDE_P0 : ModuleRules
{
	public PRJ_TIDE_P0(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange([
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"NavigationSystem",
			"GameplayTags",
			"DeveloperSettings",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"UMG",
			"Slate",
			"ImGui",
			"Niagara",
			"GeometryCollectionEngine",
			"FieldSystemEngine",
			"AnimGraphRuntime",
			"PhysicsCore",
			"ProceduralMeshComponent",
			"MotionWarping"
		] );

		PrivateDependencyModuleNames.AddRange([
			"ApplicationCore",
			"AssetRegistry"
		]);

		PublicIncludePaths.AddRange([
			"PRJ_TIDE_P0",
			"PRJ_TIDE_P0/ThirdParty/sml/include",
		]);

		// エディタビルド時のみの依存
		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.AddRange([
				"UnrealEd"
			]);
		}
	}
}
