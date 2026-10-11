// Copyright Broken Rock Studios LLC. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NativeGameplayTags.h"

#define TAG_EXTERN(Name) UE_DECLARE_GAMEPLAY_TAG_EXTERN(Name)

namespace RockInteractionGameplayTags
{
	TAG_EXTERN(Interact_Verb_Activate);
}

#undef TAG_EXTERN
