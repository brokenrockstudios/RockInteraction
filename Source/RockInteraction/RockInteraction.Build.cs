// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class RockInteraction : ModuleRules
{
	public RockInteraction(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			[
				"Core",
				"CoreUObject",
				"Engine",
				"GameplayTags",
				"GameplayAbilities"
				//"GameplayTasks"
			]
		);

		PrivateDependencyModuleNames.AddRange([]);
	}
}