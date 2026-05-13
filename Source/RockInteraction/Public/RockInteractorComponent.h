// Copyright Broken Rock Studios LLC. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RockInteractionContext.h"
#include "RockInteractionOption.h"
#include "Components/ActorComponent.h"
#include "RockInteractorComponent.generated.h"


USTRUCT()
struct FRockInteractorSecondaryTick : public FTickFunction
{
	GENERATED_USTRUCT_BODY()

	// TODO: Move this whole thing to RockCore and make it a generic secondary tick function
	// That could be used by any number of systems?
	// UPROPERTY()
	//TScriptInterface<IRockSecondaryTickTarget> Target;

	UPROPERTY()
	TObjectPtr<class URockInteractorComponent> Target;

	virtual void ExecuteTick(float DeltaTime, ELevelTick TickType, ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent) override;
	virtual FString DiagnosticMessage() override { return TEXT("FRockInteractorSecondaryTick"); }
	virtual FName DiagnosticContext(bool bDetailed) override { return FName(TEXT("FRockInteractorSecondaryTick")); }
};

template <>
struct TStructOpsTypeTraits<FRockInteractorSecondaryTick> : public TStructOpsTypeTraitsBase2<FRockInteractorSecondaryTick>
{
	enum { WithCopy = false };
};


UCLASS(ClassGroup=Interaction, meta=(BlueprintSpawnableComponent))
class ROCKINTERACTION_API URockInteractorComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	// --- Config ---
	UPROPERTY(EditDefaultsOnly, Category="Interaction")
	float ScanRange = 250.f;

	/** Rate at which the sphere overlap scan runs to maintain the candidate list.
	*  Only active in DirectHitWithLookAt mode. Default 0.05 (~20hz). */
	UPROPERTY(EditDefaultsOnly, Category="Interaction", meta=(ClampMin="-1.0", ToolTip="Sphere scanning interval in seconds. 0 = every frame, -1 = disabled. Default 0.05 (20hz)."))
	float SphereScanRate = 0.05f;

	/** Rate at which interaction focus is scored and updated.
	*  0 = every frame, -1 = disabled. Default 0.01 (~100hz).
	*  Runs a line trace at a specified rate; lower values reduce CPU cost in dense scenes. */
	UPROPERTY(EditDefaultsOnly, Category="Interaction", meta=(ClampMin="-1.0", ToolTip="Focus scoring interval in seconds. 0 = every frame, -1 = disabled. Default 0.01 (100hz)."))
	float LineTraceScanRate = 0.01f;

	UPROPERTY(EditDefaultsOnly, Category="Interaction")
	float LookAtThresholdDegrees = 3.0f; // Within 3 degrees of center

	UPROPERTY(EditDefaultsOnly, Category="Interaction")
	TEnumAsByte<ECollisionChannel> ScanChannel = ECC_GameTraceChannel1;

	// Tags that you pass along during GatherInteractionPoints and GatherInteractionOptions. Such as "IsInVehicle". Otherwise, you could query the Instigator's ASC directly
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction")
	FGameplayTagContainer QueryInteractionTags;

	UPROPERTY(EditAnywhere, Category = "Interaction|Scan")
	ERockInteractorScanMode ScanMode = ERockInteractorScanMode::DirectHitWithLookAt;

	// UPROPERTY(EditDefaultsOnly, Category = "Interaction|Scan")
	// bool bEnableSphereScan = true;

	// Turning these off would slight perf+, but would disable pre-granting abilities on proximity 
	UPROPERTY(EditDefaultsOnly, Category = "Interaction|Scan")
	bool bEnableCandidateEnterEvents = true;

	// Turning these off would slight perf+, but would disable revoking abilities on leaving proximity
	UPROPERTY(EditDefaultsOnly, Category = "Interaction|Scan")
	bool bEnableCandidateExitEvents = true;


	// --- Delegates ---
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnFocusChanged, const FRockInteractionContext&, Context);

	UPROPERTY(BlueprintAssignable)
	FOnFocusChanged OnFocusChanged;

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnFocusOptionsChanged, const FRockInteractionOptions&, Options);

	UPROPERTY(BlueprintAssignable)
	FOnFocusOptionsChanged OnOptionsChanged;

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnInteractionTriggered, const FRockInteractionContext&, Context, const FRockInteractionOption&, Option);

	UPROPERTY(BlueprintAssignable)
	FOnInteractionTriggered OnInteractionTriggered;

	// --- Queries ---
	UFUNCTION(BlueprintCallable)
	bool HasFocus() const;
	UFUNCTION(BlueprintCallable)
	int32 GetOptionCount() const;

	UFUNCTION(BlueprintCallable)
	const FRockInteractionContext& GetFocusedContext() const;
	UFUNCTION(BlueprintCallable)
	const FRockInteractionOptions& GetFocusedOptions() const;

	// --- Actions ---
	// Called by input binding or GA_Interact
	UFUNCTION(BlueprintCallable)
	void TriggerInteraction(int32 OptionIndex = 0);

	UFUNCTION()
	void OnControllerChanged(APawn* Pawn, AController* OldController, AController* NewController);

	void AddPersistentCandidate(const FRockInteractionCandidateEntry& CandidateEntry);
	void RemovePersistentCandidate(const FRockInteractionCandidateEntry& CandidateEntry);

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	void SecondaryTickComponent(float DeltaTime, ELevelTick TickType);
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
protected:
	URockInteractorComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	void StartScans();
	void TickSphereScan();

	void OnScanComplete(const FTraceHandle& Handle, FOverlapDatum& Datum);
	void UpdateCandidates(TArray<FOverlapResult>& Overlaps);

	virtual void OnCandidateEntered(const TScriptInterface<IRockInteractableTarget>& Target);
	virtual void OnCandidateExited(const TScriptInterface<IRockInteractableTarget>& Target);
	virtual void OnCandidatesUpdated(const TArray<FRockInteractionCandidateEntry>& NewCandidates);
	virtual void SetFocusedTarget(const TScriptInterface<IRockInteractableTarget>& NewTarget);
	virtual void ClearFocus();

	void TickLineTrace();
	bool TryResolveDirectHit(const FInteractionScanContext& ScanCtx, const FRockInteractionQuery& Query, TScriptInterface<IRockInteractableTarget>& OutTarget, FRockInteractionPoint& OutPoint) const;
	bool ResolvePointsFromTarget(const TScriptInterface<IRockInteractableTarget>& Candidate, const FInteractionScanContext& ScanCtx, const FRockInteractionQuery& Query, TScriptInterface<IRockInteractableTarget>& OutTarget, FRockInteractionPoint& OutPoint) const;
	bool ScoreCandidatesByLookAt(const FInteractionScanContext& ScanCtx, const FRockInteractionQuery& Query, TScriptInterface<IRockInteractableTarget>& OutTarget, FRockInteractionPoint& OutPoint) const;
	void ResolveVisibilityProxy(const FRockInteractionQuery& Query, TScriptInterface<IRockInteractableTarget>& Target, FRockInteractionPoint& InOutPoint) const;
private:
	// Returns view origin + direction from controller
	bool GetViewPoint(FVector& OutOrigin, FVector& OutDirection) const;
	void OnFocusedTargetStateChanged();
protected:
	// Candidate list from sphere overlap
	// If you consistently go over 10 candidate targets, consider switching to TSet instead?
	TArray<FRockInteractionCandidateEntry> Candidates;
	TArray<FRockInteractionCandidateEntry> PersistentCandidates;
private:
	// Result of scoring
	FRockInteractionContext CurrentContext;
	FRockInteractionOptions CurrentOptions;
	bool bHasFocus = false;

	//FTimerHandle SphereScanTimerHandle;
	//FTimerHandle LineTraceScanTimerHandle;
	FTraceHandle PendingSphereScanHandle;
	FOverlapDelegate SphereScanDelegate;

	bool bSphereScanActive = false;
	bool bLineTraceScanActive = false;

	UPROPERTY()
	FRockInteractorSecondaryTick SecondaryTickFunction;

	void DrawInteractionPointDebug(const UWorld* World, const FVector& Location, float LookAtDotProduct) const;
};
