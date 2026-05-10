// Copyright Broken Rock Studios LLC. All Rights Reserved.


#include "RockInteractionTypes.h"

DEFINE_LOG_CATEGORY(LogRockInteraction)

APawn* FRockInteractionQuery::GetInstigatorPawn() const
{
	return Cast<APawn>(Instigator.Get());
};
