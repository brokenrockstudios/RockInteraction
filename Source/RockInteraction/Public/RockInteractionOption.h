// Copyright Broken Rock Studios LLC. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "StructUtils/InstancedStruct.h"
#include "RockInteractionOption.generated.h"

class UGameplayAbility;

// Maybe just have something like
// if (Component->GetOptionCount() == 1)
// 	Component->TriggerInteraction(0); // instant
// else
// 	OpenSelectionUI(); // show options, defer TriggerInteraction

/**
 * Base struct for game-specific interaction option extensions.
 *
 * Inherit from this to attach game-layer data to an option without
 * modifying FRockInteractionOption itself. For example:
 *
 *   USTRUCT()
 *   struct FFenInteractionOptionData : public FRockInteractionOptionData
 *   {
 *       GENERATED_BODY()
 *       FGameplayAbilitySpecHandle AbilityHandle;
 *   };
 */
USTRUCT(BlueprintType)
struct ROCKINTERACTION_API FRockInteractionOptionData
{
	GENERATED_BODY()
};

USTRUCT(BlueprintType)
struct ROCKINTERACTION_API FRockInteractionOption
{
	GENERATED_BODY()
	/**
	 * Stable identity for this option. Used by the game layer to determine
	 * what ability or behavior to invoke when the player confirms this option.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FGameplayTag OptionTag;

	/** Primary display text for this option (e.g. "Open", "Pick Lock"). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FText Text;

	/** Optional secondary display text (e.g., a hint or condition description). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FText SubText;

	UPROPERTY(EditAnywhere, Category = "Interaction")
	TSubclassOf<UGameplayAbility> InteractionAbility = nullptr;

	// Game layer extension - ability spec handle, item requirements, etc.
	UPROPERTY(EditAnywhere, meta = (ExcludeBaseStruct, BaseStruct = "/Script/RockInteraction.RockInteractionOptionData", ShowOnlyInnerProperties))
	FInstancedStruct OptionData;

	bool operator==(const FRockInteractionOption& Other) const
	{
		return OptionTag == Other.OptionTag;
	}
};

// ----------------------------------------------------------------

/**
 * Output container populated by IRockInteractableTarget::GatherInteractionOptions.
 *
 * Passed into GatherInteractionOptions and returned to the interaction subsystem.
 * The subsystem reads AvailableOptions to determine what to present to the player.
 *
 * Intentionally, a struct rather than a builder. Target back-reference lives on
 * FRockInteractionContext, so no scope-stamping is needed here.
 */
USTRUCT(BlueprintType)
struct FRockInteractionOptions
{
	GENERATED_BODY()
public:
	/**
	 * Array of available options that can be started.
	 * Add to this array for any interaction that you would like to be presented
	 * as an available option in response to this query.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = General)
	TArray<FRockInteractionOption> AvailableOptions;

	/* Resets the values of this query results to be empty.	*/
	void Reset()
	{
		AvailableOptions.Reset();
	}

	bool IsEmpty() const
	{
		return AvailableOptions.IsEmpty();
	}

	void AddOption(const FRockInteractionOption& Option)
	{
		AvailableOptions.Add(Option);
	}
};
