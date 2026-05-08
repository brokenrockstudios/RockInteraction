// Copyright Broken Rock Studios LLC. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RockInteractionTypes.h"
#include "Misc/CoreMiscDefines.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "RockInteractionLibrary.generated.h"

/**
 * Stateless helpers for populating FRockInteractionPoint arrays from common
 * component configurations. Intended for use inside GatherInteractionPoints
 * implementations to avoid boilerplate.
 *
 * All functions APPEND to OutPoints. They do not reset the array.
 * Each Append function returns the number of points added by that specific call.
 * Each Refresh function returns false if the mesh/component topology has changed
 * and a full re-gather is required, true if all transforms updated cleanly.
 */
UCLASS()
class ROCKINTERACTION_API URockInteractionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

	/**
	 * Appends one FRockInteractionPoint per socket on a UStaticMeshComponent whose
	 * name starts with SocketPrefix. Iterates UStaticMesh::Sockets directly. O(N).
	 *
	 * PointTag is left empty; caller is responsible for post-processing if tag mapping is needed.
	 *
	 * @param OutPoints     Array to append into. Not reset.
	 * @param Mesh          Component to query. No-op if null or has no mesh asset.
	 * @param SocketPrefix  Only sockets whose FName starts with this prefix are included.
	 *                      Pass NAME_None to include all sockets.
	 * @return              Number of points appended.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rock|Interaction")
	static int32 AppendPointsFromStaticMesh(UPARAM(ref) TArray<FRockInteractionPoint>& OutPoints, UStaticMeshComponent* Mesh);
	
	/**
	 * Refreshes WorldTransform for all points in InOutPoints whose SourceComponent
	 * matches Mesh. Does not add or remove points. The array structure must already
	 * be valid from a prior AppendPointsFromStaticMesh call.
	 *
	 * @param InOutPoints   Existing point array to update in place.
	 * @param Mesh          Component to refresh transforms from.
	 * @param StartIndex    Index of the first point to refresh.
	 * @param Count         Number of points to refresh. Pass -1 to refresh all points
	 *                      from StartIndex to the end of the array.
	 * @return              True if all transforms updated cleanly. False if any matched
	 *                      point has a SocketName not found on the current mesh asset,
	 *                      indicating the caller should trigger a full re-gather.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rock|Interaction")
	static bool RefreshPointsFromStaticMesh(UPARAM(ref) TArray<FRockInteractionPoint>& InOutPoints, UStaticMeshComponent* Mesh, int32 StartIndex, int32 Count = -1);

	/**
	 * Appends one FRockInteractionPoint per socket on a USkeletalMeshComponent whose
	 * name starts with SocketPrefix. Iterates USkeletalMesh::GetMeshOnlySocketList()
	 * directly O(N). Socket scan + O(bone count) bone transform per socket.
	 *
	 * PointTag is left empty; caller is responsible for post-processing if tag mapping is needed.
	 *
	 * @param OutPoints     Array to append into. Not reset.
	 * @param Mesh          Component to query. No-op if null or has no mesh asset.
	 * @return              Number of points appended.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rock|Interaction")
	static int32 AppendPointsFromSkeletalMesh(UPARAM(ref) TArray<FRockInteractionPoint>& OutPoints, USkeletalMeshComponent* Mesh);

	/**
	 * Refreshes WorldTransform for all points in InOutPoints whose SourceComponent
	 * matches Mesh. Does not add or remove points. The array structure must already
	 * be valid from a prior AppendPointsFromSkeletalMesh call.
	 *
	 * @param InOutPoints   Existing point array to update in place.
	 * @param Mesh          Component to refresh transforms from.
	 * @param StartIndex    Index of the first point to refresh.
	 * @param Count         Number of points to refresh. Pass -1 to refresh all points
	 *                      from StartIndex to the end of the array.
	 * @return              True if all transforms updated cleanly. False if any matched
	 *                      point has a SocketName not found on the current mesh asset,
	 *                      indicating the caller should trigger a full re-gather.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rock|Interaction")
	static bool RefreshPointsFromSkeletalMesh(UPARAM(ref) TArray<FRockInteractionPoint>& InOutPoints, USkeletalMeshComponent* Mesh, int32 StartIndex, int32 Count);

	/**
	 * Appends one FRockInteractionPoint per USceneComponent (including UArrowComponent)
	 * that has at least one ComponentTag whose string starts with TagPrefix.
	 *
	 * The first matching tag is stored as PointTag via FGameplayTag::RequestGameplayTag.
	 * SourceComponent is set to the matched component; SocketName is left NAME_None.
	 *
	 * @param OutPoints     Array to append into. Not reset.
	 * @param Root          Starting component. No-op if null.
	 * @param bRecursive    If true, walks the full component subtree.
	 *                      If false, only checks direct children of Root.
	 * @return              Number of points appended.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rock|Interaction")
	static int32 AppendPointsFromTaggedComponents(UPARAM(ref) TArray<FRockInteractionPoint>& OutPoints, USceneComponent* Root, bool bRecursive = true);

	/**
	 * Refreshes WorldTransform for all points in InOutPoints whose SourceComponent is
	 * still present in the component hierarchy rooted at Root. Does not add or remove
	 * points. The array structure must already be valid from a prior
	 * AppendPointsFromTaggedComponents call.
	 *
	 * @param InOutPoints   Existing point array to update in place.
	 * @param Root          Starting component. Returns false immediately if null.
	 * @param bRecursive    Must match the value used during the original append.
	 * @param StartIndex    Index of the first point to refresh.
	 * @param Count         Number of points to refresh. Pass -1 to refresh all points
	 *                      from StartIndex to the end of the array.
	 * @return              True if all transforms updated cleanly. False if any matched
	 *                      SourceComponent is no longer found in the hierarchy, indicating
	 *                      the caller should trigger a full re-gather.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rock|Interaction")
	static bool RefreshPointsFromTaggedComponents(UPARAM(ref) TArray<FRockInteractionPoint>& InOutPoints, USceneComponent* Root, bool bRecursive, int32 StartIndex, int32 Count);
private:
};
