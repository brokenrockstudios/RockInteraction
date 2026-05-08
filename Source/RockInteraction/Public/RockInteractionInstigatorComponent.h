// Copyright Broken Rock Studios LLC. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RockInteractionContext.h"
#include "RockInteractionOption.h"
#include "Components/ActorComponent.h"
#include "RockInteractionInstigatorComponent.generated.h"

UCLASS(ClassGroup=Interaction, meta=(BlueprintSpawnableComponent))
class ROCKINTERACTION_API URockInteractionInstigatorComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    // --- Config ---
    UPROPERTY(EditDefaultsOnly, Category="Interaction")
    float ScanRange = 250.f;

    UPROPERTY(EditDefaultsOnly, Category="Interaction")
    float ScanRate = 0.05f; // 20hz

    UPROPERTY(EditDefaultsOnly, Category="Interaction")
    float LookAtThresholdDegrees = 3.0f; // Within 3 degrees of center
    
    UPROPERTY(EditDefaultsOnly, Category="Interaction")
    TEnumAsByte<ECollisionChannel> ScanChannel = ECC_GameTraceChannel1;
    
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
    
    void AddPersistentCandidate(const TScriptInterface<IRockInteractableTarget>& Target);
    void RemovePersistentCandidate(const TScriptInterface<IRockInteractableTarget>& Target);
    
    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
protected:
    URockInteractionInstigatorComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
    
    
    void StartSphereScan();
    void TickSphereScan();
    void UpdateCandidates(TArray<FOverlapResult>& Overlaps);
    
    virtual void OnCandidateEntered(const TScriptInterface<IRockInteractableTarget>& Target);
    virtual void OnCandidateExited(const TScriptInterface<IRockInteractableTarget>& Target);
    virtual void OnCandidatesUpdated(const TArray<TScriptInterface<IRockInteractableTarget>>& NewCandidates);
    
    void ScoreAndSelectFocused();
private:
    // Returns view origin + direction from controller
    bool GetViewPoint(FVector& OutOrigin, FVector& OutDirection) const;

    void SetFocusedTarget(const TScriptInterface<IRockInteractableTarget>& NewTarget);
    void OnFocusedTargetStateChanged();
    void ClearFocus();

    FTimerHandle ScanTimerHandle;

    // Candidate list from sphere overlap
protected:
    // If you consistently go over 10 candidate targets, consider switching to TSet instead?
    TArray<TScriptInterface<IRockInteractableTarget>> Candidates;
    TArray<TScriptInterface<IRockInteractableTarget>> PreviousCandidates;
    TArray<TScriptInterface<IRockInteractableTarget>> PersistentCandidates;
    
private:
    // Result of scoring
    FRockInteractionContext CurrentContext;
    FRockInteractionOptions CurrentOptions;
    bool bHasFocus = false;
    
    void DrawInteractionPointDebug(const UWorld* World, const FVector& Location, float LookAtDotProduct);
    
};