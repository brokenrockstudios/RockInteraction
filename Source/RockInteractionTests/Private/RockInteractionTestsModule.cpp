// Copyright Broken Rock Studios LLC. All Rights Reserved.

#include "RockInteractionTestTypes.h"

#include "GameplayTagsManager.h"
#include "Modules/ModuleManager.h"

namespace RockInteractionTestTags
{
	const FName PointA = TEXT("Test.RockInteraction.Point.A");
	const FName PointB = TEXT("Test.RockInteraction.Point.B");
	const FName OptionA = TEXT("Test.RockInteraction.Option.A");
	const FName OptionB = TEXT("Test.RockInteraction.Option.B");

	FGameplayTag Get(const FName& Name)
	{
		return UGameplayTagsManager::Get().RequestGameplayTag(Name);
	}
}

// FNativeGameplayTag ensures when defined outside a Runtime module, so the test tags use the legacy AddNativeGameplayTag path.
class FRockInteractionTestsModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		UGameplayTagsManager& Manager = UGameplayTagsManager::Get();
		const FString Comment = TEXT("RockInteractionTests");
		Manager.AddNativeGameplayTag(RockInteractionTestTags::PointA, Comment);
		Manager.AddNativeGameplayTag(RockInteractionTestTags::PointB, Comment);
		Manager.AddNativeGameplayTag(RockInteractionTestTags::OptionA, Comment);
		Manager.AddNativeGameplayTag(RockInteractionTestTags::OptionB, Comment);
	}
};

IMPLEMENT_MODULE(FRockInteractionTestsModule, RockInteractionTests)
