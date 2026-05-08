// Copyright Broken Rock Studios LLC. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "RockInteractableInstigator.generated.h"

// This class does not need to be modified.
UINTERFACE()
class URockInteractableInstigator : public UInterface
{
	GENERATED_BODY()
};

/**
 * Interface that represents a single "Instigator" of an interaction.
 * 
 * Instigators are objects which are allowed to interact with targets. They
 * can attempt to begin interacting with a given set of targets.
 */
class ROCKINTERACTION_API IRockInteractableInstigator
{
	GENERATED_BODY()

public:
protected:
	// virtual void OnAttemptToBeginInteractions(const TArray<TScriptInterface<IInteractionTarget>>& TargetsToInteractWith) = 0;
};
