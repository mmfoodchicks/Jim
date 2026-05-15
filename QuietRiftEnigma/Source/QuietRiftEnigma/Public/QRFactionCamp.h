#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "QRFactionCamp.generated.h"

class UStaticMeshComponent;
class USphereComponent;
class UQRCampSimComponent;
class UQRLeaderComponent;
class UQRFactionComponent;


/**
 * One enemy AI camp — Master GDD §30 "factions operate under the same
 * pressures as the player." Hosts:
 *
 *   • UQRCampSimComponent  : population / military / resources sim
 *                            (ticks game-hours via GameMode)
 *   • UQRLeaderComponent   : LeadershipAptitude drives raid quality
 *   • UQRFactionComponent  : faction membership + reputation
 *
 * The worldgen pipeline auto-spawns one of these at each
 * FactionSatellite POI placement so the world starts with several
 * autonomous camps that grow + raid on their own schedule.
 *
 * The Concordat itself stays as AQRVanguardColony (mega-faction
 * special-cased); these are the regular AI camps that surround it.
 */
UCLASS(BlueprintType, Blueprintable)
class QUIETRIFTENIGMA_API AQRFactionCamp : public AActor
{
	GENERATED_BODY()

public:
	AQRFactionCamp();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "QR|Camp")
	TObjectPtr<USphereComponent> AreaSphere;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "QR|Camp")
	TObjectPtr<UStaticMeshComponent> CampMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "QR|Camp")
	TObjectPtr<UQRCampSimComponent> Sim;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "QR|Camp")
	TObjectPtr<UQRLeaderComponent> Leader;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "QR|Camp")
	TObjectPtr<UQRFactionComponent> Faction;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|Camp")
	FText DisplayName;

	// NPC actor class spawned for each raid-party member. Defaults to
	// AQRNPCActor; designer can swap to a faction-themed soldier.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|Camp")
	TSubclassOf<class AQRNPCActor> RaiderClass;

	// Drops effective leadership to 0 and applies a hostility decay.
	// Call when the camp's leader NPC is killed by the player or in
	// faction-vs-faction combat. The sim then reads chaotic leadership
	// (poor decisions, rash raids) until the player kills the camp
	// off entirely or it recovers (recovery is a follow-up feature).
	UFUNCTION(BlueprintCallable, Category = "QR|Camp")
	void OnLeaderDied();

protected:
	virtual void BeginPlay() override;

private:
	UFUNCTION()
	void HandleRaidLaunched(struct FQRRaidPlan Plan);

	// Track whether the leader is still alive — sim reads this via the
	// component's accessor. Cleared by OnLeaderDied.
	bool bLeaderAlive = true;
public:
	UFUNCTION(BlueprintPure, Category = "QR|Camp")
	bool IsLeaderAlive() const { return bLeaderAlive; }
};
