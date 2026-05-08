# Rock Interaction

A high-performance interaction framework plugin for Unreal Engine. Designed as a simple core for an interaction system.

## Overview
RockInteraction provides a complete interaction pipeline driven by a component on the Pawn. It handles candidate detection, lookat scoring, focus selection, and option gathering.

* Chose to use Struct over UObject where performance matters.
* Clean seperation and highly extendable


## Features

* URockInteractionInstigatorComponent: owns the full pipeline. Sphere overlap, per frame scoring, and focus selection.
* FRockInteractionPoint: Lightweight struct describing interaction and visibility probe points via optional socket conventions (IX_ and IX_VP) prefixes)
* URockInteractionLibrary: Helper library for automated socket scanning helpers for static mesh, skeletal meshes, and tagged components
* Two-Phase visibility pipeline (ScoreAndSelectFocused) for efficient candidate evaluation
  * Fast Path: Line trace with early out on immediate hits. Exits immediately if the trace hits a valid component with assosciated InteractionPoint (0-1 on actor or no more than 1 per component)
  * Fallback: LookatPercentage scoring when no immediate hit is found, with optional per-point override thresholds for activation and focus
  * Helper IX_VP Visibility probes participate in LookAtScoring and resolve back to their assosciated Interaction Point, such as for unusual shaped objects (e.g. long thin sword)
 

## Installation

1. Copy the RockInteraction folder into your project's Plugins/ directory
2. Add "RockInteraction" to your .uproject plugin list
3. Add the module to your Build.cs dependencies as needed
4. Regenerate project files and build



## Example extension and implementation

### Example integration

You can either add custom authored points directly such as below, or leverage the Library to auto collect from meshes or components
```cpp
bool AFenPortableRadio::GatherInteractionPoints(const FRockInteractionQuery& Context, TArray<FRockInteractionPoint>& OutPoints) const
{
    OutPoints.Add({MeshComponent->GetSocketLocation(TEXT("IX_0")), FenGameplayTags::Interact_Verb_Play, MeshComponent, TEXT("IX_0")});
    OutPoints.Add({MeshComponent->GetSocketLocation(TEXT("IX_1")), FenGameplayTags::Interact_Verb_Stop, MeshComponent, TEXT("IX_1")});
    return true;
}
void AFenPortableRadio::OnInteractionBegin(const FRockInteractionContext& Context)
{
    IRockInteractableTarget::OnInteractionBegin(Context);
    // Dispatch on Context.Point.PointTag (Interact_Verb_Play, Stop, etc.)
    // Alternatively leverage a unique GA per point
}
```

### Example Extension of core

```cpp
void UFenInteractionInstigatorComponent::OnCandidateEntered(const TScriptInterface<IRockInteractableTarget>& Target)
{
    // Gather abilities from target, ref-count grant via ASC on authority
    auto OptionData = Option.OptionData.GetPtr<FFenInteractionOptionData>();
		if (OptionData && OptionData->InteractionAbility)
		{
			OutAbilities.Add(OptionData->InteractionAbility);
		}
}

USTRUCT()
struct FFenInteractionOptionData : public FRockInteractionOptionData
{
    GENERATED_BODY()
    UPROPERTY(EditDefaultsOnly)
    TSubclassOf<UGameplayAbility> InteractionAbility;
};
```

In my project's use case, I give the user in proximity the required abilities ahead of the interaction, to eliminate possibly latency related issues
We ref count and track modified time for optional revocation of abilities at a later point.
```cpp
void UFenInteractionInstigatorComponent::OnCandidateEntered(const TScriptInterface<IRockInteractableTarget>& Target)
{
	UAbilitySystemComponent* ASC = GetCachedASC();
	if (!ASC || ASC->GetOwnerRole() != ROLE_Authority) { return; }

	TArray<TSubclassOf<UGameplayAbility>> RequiredAbilities;
	Target->GatherInteractionAbilities(RequiredAbilities);

	for (const TSubclassOf<UGameplayAbility>& AbilityClass : RequiredAbilities)
	{
		if (!AbilityClass) { continue; }
		FObjectKey ClassKey(AbilityClass);
		
		if (FRockGrantedAbilityEntry* Existing = GrantedAbilities.Find(ClassKey))
		{
			Existing->RefCount++;
			Existing->LastModifiedTime = GetWorld()->GetTimeSeconds();
		}
		else
		{
			FGameplayAbilitySpec Spec(AbilityClass, 1, INDEX_NONE, ASC->GetOwnerActor());
			FRockGrantedAbilityEntry& Entry = GrantedAbilities.Add(ClassKey);
			Entry.Handle = ASC->GiveAbility(Spec);
			Entry.RefCount = 1;
			Entry.LastModifiedTime = GetWorld()->GetTimeSeconds();
		}
	}
}
```

## Contributing

Specific feature requests are also welcome. If there's something you need for your project, ask and I'll consider it.

Contributions, questions, and feedback are welcome. Feel free to open an issue or PR or contact via alternative means





