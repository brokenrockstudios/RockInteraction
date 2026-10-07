// Copyright Broken Rock Studios LLC. All Rights Reserved.

#pragma once

#include "AIController.h"
#include "CoreMinimal.h"
#include "Components/ActorTestSpawner.h"
#include "GameFramework/Pawn.h"
#include "RockInteractionTestTypes.h"
#include "UObject/StrongObjectPtr.h"

/** A point 100 units ahead of the origin and off the +X axis by this many degrees, as seen from the origin. */
inline FVector Ahead(double OffAxisDegrees, double Distance = 100.0)
{
	return FVector(Distance, Distance * FMath::Tan(FMath::DegreesToRadians(OffAxisDegrees)), 0.0);
}

/**
 * A transient game world plus helpers to build interaction scenes: interactables at known positions, and a pawn with an
 * interactor component that looks along +X from the origin (eye height 0).
 *
 * Declare it as a TEST_CLASS member. CQTest also constructs test classes at module load, so nothing here touches the
 * engine in a constructor; the spawner creates its world lazily on the first spawn.
 *
 * The test world never ticks and never begins play. Drive BeginPlay with BeginPlay(Actor) and scoring with
 * URockTestInteractorComponent::ScorePass(). No game instance is created: nothing in these tests needs one, and creating
 * one would start every project game instance subsystem.
 */
class FRockInteractionFixture
{
public:
	FActorTestSpawner Spawner;

	ARockTestInteractable& SpawnInteractable(const FVector& Location)
	{
		ARockTestInteractable& Interactable = Spawner.SpawnActor<ARockTestInteractable>();
		Interactable.SetActorLocation(Location);
		return Interactable;
	}

	/** A pawn at the origin that is not possessed. The interactor component is registered and the pawn has begun play. */
	APawn& SpawnPawn()
	{
		APawn& Pawn = Spawner.SpawnActor<APawn>();
		Pawn.BaseEyeHeight = 0.f;
		return Pawn;
	}

	URockTestInteractorComponent& AddInteractor(APawn& Pawn)
	{
		URockTestInteractorComponent* Component = NewObject<URockTestInteractorComponent>(&Pawn);
		Component->RegisterComponent();
		return *Component;
	}

	/** Possesses the pawn with an AI controller facing +X. Local, authoritative and standalone, so the interactor starts its line trace scan. */
	AAIController& Possess(APawn& Pawn)
	{
		AAIController& Controller = Spawner.SpawnActor<AAIController>();
		Controller.Possess(&Pawn);
		Pawn.BaseEyeHeight = 0.f; // possession recalculates it
		Controller.SetControlRotation(FRotator::ZeroRotator);
		return Controller;
	}

	/** A pawn with an interactor, begun play, possessed: ready for ScorePass(). */
	URockTestInteractorComponent& SpawnReadyInteractor(APawn*& OutPawn)
	{
		OutPawn = &SpawnPawn();
		URockTestInteractorComponent& Interactor = AddInteractor(*OutPawn);
		BeginPlay(*OutPawn);
		Possess(*OutPawn);
		return Interactor;
	}

	/** The test world never begins play, so BeginPlay is dispatched by hand. */
	static void BeginPlay(AActor& Actor)
	{
		Actor.DispatchBeginPlay();
	}

	template <class T>
	T* MakeObject()
	{
		T* Object = NewObject<T>(GetTransientPackage());
		KeepAlive.Add(TStrongObjectPtr<UObject>(Object));
		return Object;
	}

	static TScriptInterface<IRockInteractableTarget> AsTarget(AActor& Actor)
	{
		TScriptInterface<IRockInteractableTarget> Target;
		Target.SetObject(&Actor);
		Target.SetInterface(Cast<IRockInteractableTarget>(&Actor));
		return Target;
	}

	/** A point at a world location with a tag, as an interaction point. */
	static FRockInteractionPoint MakePoint(const FVector& Location, const FName& Tag, ERockInteractionPointRole Role = ERockInteractionPointRole::Interaction, USceneComponent* Source = nullptr)
	{
		FRockInteractionPoint Point;
		Point.WorldLocation = Location;
		Point.PointTag = RockInteractionTestTags::Get(Tag);
		Point.Role = Role;
		Point.SourceComponent = Source;
		return Point;
	}

	static FRockInteractionOption MakeOption(const FName& Tag)
	{
		FRockInteractionOption Option;
		Option.OptionTag = RockInteractionTestTags::Get(Tag);
		return Option;
	}

	/** A scan context looking from the origin along +X with the default 3 degree threshold. */
	static FInteractionScanContext MakeScan(float ThresholdDegrees = 3.f)
	{
		FInteractionScanContext Scan;
		Scan.ViewOrigin = FVector::ZeroVector;
		Scan.ViewDirection = FVector::ForwardVector;
		Scan.LookAtThresholdCos = FMath::Cos(FMath::DegreesToRadians(ThresholdDegrees));
		return Scan;
	}

private:
	TArray<TStrongObjectPtr<UObject>> KeepAlive;
};
