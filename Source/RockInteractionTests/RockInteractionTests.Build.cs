// Copyright Broken Rock Studios LLC. All Rights Reserved.

using UnrealBuildTool;

// Automation tests for RockInteraction. Developer module: not built for Shipping, so CQTest never leaks into a shipped target.
public class RockInteractionTests : ModuleRules
{
	public RockInteractionTests(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"AIModule",
				"GameplayTags",
				"GameplayAbilities",
				"CQTest",
				"RockInteraction",
			}
		);
	}
}
