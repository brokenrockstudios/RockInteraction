// Copyright Broken Rock Studios LLC. All Rights Reserved.

#include "RockInteractionContext.h"

bool FRockInteractionContext::IsValid() const
{
	return Target.GetObject() != nullptr;
}
