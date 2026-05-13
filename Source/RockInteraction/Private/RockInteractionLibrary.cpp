// Copyright Broken Rock Studios LLC. All Rights Reserved.


#include "RockInteractionLibrary.h"

#include "GameplayTagsManager.h"
#include "RockInteractionGameplayTags.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshSocket.h"

static const TCHAR* IX_Prefix = TEXT("IX_");
static const TCHAR* IX_VP_Prefix = TEXT("IX_VP_");

int32 URockInteractionLibrary::AppendPointsFromStaticMesh(TArray<FRockInteractionPoint>& OutPoints, UStaticMeshComponent* Mesh)
{
	if (!Mesh) { return 0; }
	const UStaticMesh* StaticMesh = Mesh->GetStaticMesh();
	if (!StaticMesh) { return 0; }

	const FTransform ComponentTransform = Mesh->GetComponentTransform();
	int32 AddedCount = 0;

	for (const UStaticMeshSocket* Socket : StaticMesh->Sockets)
	{
		if (!Socket) { continue; }
		FString SocketNameStr = Socket->SocketName.ToString();
		if (!SocketNameStr.StartsWith(IX_Prefix)) { continue; }

		// Compose directly from socket relative transform. No per-name lookup
		FRockInteractionPoint& Point = OutPoints.AddDefaulted_GetRef();
		Point.WorldLocation = ComponentTransform.TransformPosition(Socket->RelativeLocation);
		// The caller should populate/override this
		Point.PointTag = RockInteractionGameplayTags::Interact_Verb_Activate;
		Point.SourceComponent = Mesh;
		Point.SocketName = Socket->SocketName;
		Point.Role = SocketNameStr.StartsWith(IX_VP_Prefix) ? ERockInteractionPointRole::Visibility : ERockInteractionPointRole::Interaction;
		++AddedCount;
	}

	return AddedCount;
}

bool URockInteractionLibrary::RefreshPointsFromStaticMesh(TArray<FRockInteractionPoint>& InOutPoints, UStaticMeshComponent* Mesh, int32 StartIndex, int32 Count)
{
	if (!Mesh) { return false; }

	const UStaticMesh* StaticMesh = Mesh->GetStaticMesh();
	if (!StaticMesh) { return false; }

	const int32 EndIndex = (Count == INDEX_NONE) ? InOutPoints.Num() : FMath::Min(StartIndex + Count, InOutPoints.Num());

	// Build a socket name -> socket lookup once for the whole refresh
	// TMap would be overkill for typical socket counts (1-5), linear scan is fine
	const FTransform ComponentTransform = Mesh->GetComponentTransform();
	const TArray<UStaticMeshSocket*>& Sockets = StaticMesh->Sockets;

	int32 PointIndex = StartIndex;
	int32 SocketIndex = 0;

	while (PointIndex < EndIndex && SocketIndex < Sockets.Num())
	{
		const UStaticMeshSocket* Socket = Sockets[SocketIndex];
		if (!Socket)
		{
			++SocketIndex;
			continue;
		}

		if (Socket->SocketName != InOutPoints[PointIndex].SocketName)
		{
			// Socket was filtered out by prefix during Append. Advance socket only
			++SocketIndex;
			continue;
		}

		// Names match - update transform and advance both
		InOutPoints[PointIndex].WorldLocation = ComponentTransform.TransformPosition(Socket->RelativeLocation);

		++PointIndex;
		++SocketIndex;
	}

	// If we didn't consume all expected points, something is misaligned
	if (PointIndex != EndIndex)
	{
		UE_LOG(
			LogRockInteraction, Warning,
			TEXT("RefreshPointsFromStaticMesh: point/socket mismatch on %s - expected to reach index %d, stopped at %d. Full re-gather needed."),
			*GetNameSafe(Mesh), EndIndex, PointIndex);
		return false;
	}

	return true;
}

int32 URockInteractionLibrary::AppendPointsFromSkeletalMesh(TArray<FRockInteractionPoint>& OutPoints, USkeletalMeshComponent* Mesh)
{
	if (!Mesh) { return 0; }

	const USkeletalMesh* SkeletalMesh = Mesh->GetSkeletalMeshAsset();
	if (!SkeletalMesh) { return 0; }

	int32 AddedCount = 0;

	// GetMeshOnlySocketList returns a const ref to the asset socket array. No allocation
	for (const USkeletalMeshSocket* Socket : SkeletalMesh->GetMeshOnlySocketList())
	{
		if (!Socket) { continue; }
		FString SocketNameStr = Socket->SocketName.ToString();
		if (!SocketNameStr.StartsWith(IX_Prefix)) { continue; }

		FRockInteractionPoint& Point = OutPoints.AddDefaulted_GetRef();
		// GetBoneTransform is O(bone count) but unavoidable for skeletal. bone index
		// lookup is the cheapest path available without caching the skeleton hierarchy
		Point.WorldLocation = Socket->GetSocketLocation(Mesh);
		// The caller should populate/override this
		Point.PointTag = RockInteractionGameplayTags::Interact_Verb_Activate;
		Point.SourceComponent = Mesh;
		Point.SocketName = Socket->SocketName;
		Point.Role = SocketNameStr.StartsWith(IX_VP_Prefix) ? ERockInteractionPointRole::Visibility : ERockInteractionPointRole::Interaction;

		++AddedCount;
	}

	return AddedCount;
}

bool URockInteractionLibrary::RefreshPointsFromSkeletalMesh(
	TArray<FRockInteractionPoint>& InOutPoints,
	USkeletalMeshComponent* Mesh, int32 StartIndex,
	int32 Count)
{
	if (!Mesh) { return false; }
	const USkeletalMesh* SkeletalMesh = Mesh->GetSkeletalMeshAsset();
	if (!SkeletalMesh) { return false; }

	const int32 EndIndex = (Count == INDEX_NONE) ? InOutPoints.Num() : FMath::Min(StartIndex + Count, InOutPoints.Num());
	const TArray<USkeletalMeshSocket*>& Sockets = SkeletalMesh->GetMeshOnlySocketList();
	int32 PointIndex = StartIndex;
	int32 SocketIndex = 0;


	while (PointIndex < EndIndex && SocketIndex < Sockets.Num())
	{
		const USkeletalMeshSocket* Socket = Sockets[SocketIndex];
		if (!Socket)
		{
			++SocketIndex;
			continue;
		}

		if (Socket->SocketName != InOutPoints[PointIndex].SocketName)
		{
			// Socket was filtered out by prefix during Append - advance socket only
			++SocketIndex;
			continue;
		}

		// Names match - update transform and advance both
		InOutPoints[PointIndex].WorldLocation = Socket->GetSocketLocation(Mesh);

		++PointIndex;
		++SocketIndex;
	}

	if (PointIndex != EndIndex)
	{
		UE_LOG(
			LogRockInteraction, Warning,
			TEXT("RefreshPointsFromSkeletalMesh: point/socket mismatch on %s - expected to reach index %d, stopped at %d. Full re-gather needed."),
			*GetNameSafe(Mesh), EndIndex, PointIndex);
		return false;
	}

	return true;
}

int32 URockInteractionLibrary::AppendPointsFromTaggedComponents(TArray<FRockInteractionPoint>& OutPoints, USceneComponent* Root, bool bRecursive)
{
	if (!Root) { return 0; }

	int32 AddedCount = 0;

	auto TryAddComponent = [&](USceneComponent* Comp) -> bool
	{
		if (!Comp) { return false; }

		FName MatchedTag = NAME_None;
		for (const FName& Tag : Comp->ComponentTags)
		{
			if (Tag.ToString().StartsWith(IX_Prefix))
			{
				MatchedTag = Tag;
				break;
			}
		}
		if (MatchedTag == NAME_None) { return false; }

		FRockInteractionPoint& Point = OutPoints.AddDefaulted_GetRef();
		Point.WorldLocation = Comp->GetComponentLocation();
		Point.PointTag = RockInteractionGameplayTags::Interact_Verb_Activate;
		Point.SourceComponent = Comp;
		Point.Role = MatchedTag.ToString().StartsWith(IX_VP_Prefix)
			? ERockInteractionPointRole::Visibility
			: ERockInteractionPointRole::Interaction;
		return true;
	};

	if (TryAddComponent(Root)) { ++AddedCount; }

	TArray<USceneComponent*> Candidates;
	Root->GetChildrenComponents(bRecursive, Candidates);
	for (USceneComponent* Comp : Candidates)
	{
		if (TryAddComponent(Comp)) { ++AddedCount; }
	}

	return AddedCount;
}


bool URockInteractionLibrary::RefreshPointsFromTaggedComponents(
	TArray<FRockInteractionPoint>& InOutPoints, USceneComponent* Root,
	bool bRecursive,
	int32 StartIndex,
	int32 Count)
{
	if (!Root) { return false; }

	const int32 EndIndex = (Count == INDEX_NONE)
		? InOutPoints.Num()
		: FMath::Min(StartIndex + Count, InOutPoints.Num());
	const int32 ExpectedCount = EndIndex - StartIndex;
	if (ExpectedCount <= 0) { return true; }

	// Small-N: TMap allocates, but for N <= ~8 a TInlineAllocator-backed array
	// and linear scan is faster. Pick based on your typical point count
	// For now, TMap is fine and clearer
	// e.g. TArray<TPair<USceneComponent*, int32>, TInlineAllocator<8>>
	// auto Found = ComponentToPoint.FindByPredicate([Comp](const auto& P) { return P.Key == Comp; });
	TMap<USceneComponent*, int32 /*PointIndex*/> ComponentToPoint;
	ComponentToPoint.Reserve(ExpectedCount);

	for (int32 i = StartIndex; i < EndIndex; ++i)
	{
		if (USceneComponent* Comp = InOutPoints[i].SourceComponent.Get())
		{
			ComponentToPoint.Add(Comp, i);
		}
	}
	int32 RefreshedCount = 0;

	auto TryRefreshComponent = [&](USceneComponent* Comp)
	{
		if (!Comp) { return; }
		const int32* FoundIdx = ComponentToPoint.Find(Comp);
		if (!FoundIdx) { return; }

		InOutPoints[*FoundIdx].WorldLocation = Comp->GetComponentLocation();
		++RefreshedCount;
	};

	TryRefreshComponent(Root);

	TArray<USceneComponent*> Candidates;
	Root->GetChildrenComponents(bRecursive, Candidates);
	for (USceneComponent* Comp : Candidates)
	{
		TryRefreshComponent(Comp);
	}

	if (RefreshedCount != ExpectedCount)
	{
		UE_LOG(
			LogRockInteraction, Warning,
			TEXT("RefreshPointsFromTaggedComponents: refreshed %d / %d on %s. Full re-gather needed."),
			RefreshedCount, ExpectedCount, *GetNameSafe(Root));
		return false;
	}

	return true;
}

AActor* URockInteractionLibrary::GetCandidateActor(const TScriptInterface<IRockInteractableTarget>& Candidate)
{
	UObject* Obj = Candidate.GetObject();
	if (AActor* Actor = Cast<AActor>(Obj)) return Actor;
	if (UActorComponent* Comp = Cast<UActorComponent>(Obj)) return Comp->GetOwner();
	return nullptr;
}
