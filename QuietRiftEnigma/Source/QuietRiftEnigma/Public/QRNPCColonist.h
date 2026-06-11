#pragma once

#include "CoreMinimal.h"
#include "QRNPCActor.h"
#include "QRTypes.h"
#include "QRNPCColonist.generated.h"


/**
 * Subclass of AQRNPCActor that knows its role. Designer drops a
 * Farmer / Engineer / Hunter / Cook / Medic / Guard variant in the
 * level (or qr_spawn_starter_village creates a mix), and the brain's
 * AssignedWorkPost auto-snaps to the nearest matching station:
 *
 *   Farmer   → nearest AQRFarmPlotActor
 *   Engineer → nearest workbench AQRStationBase
 *   Hunter   → nothing yet (planned: nearest watchpost / fauna spawn ring)
 *   Cook     → nearest cooking station
 *   Medic    → nearest infirmary tag
 *   Guard    → nearest fortification piece (build tag)
 *
 * Resolution runs on BeginPlay so dropping one anywhere in the level
 * sends the colonist to the right job without per-NPC wiring. The
 * lookup falls back to spawn location, which keeps the brain busy
 * wandering even when no matching station exists.
 */
UCLASS(BlueprintType, Blueprintable)
class QUIETRIFTENIGMA_API AQRNPCColonist : public AQRNPCActor
{
	GENERATED_BODY()

public:
	AQRNPCColonist();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|Colonist")
	EQRNPCRole Role = EQRNPCRole::Unassigned;

	// Max distance (cm) the colonist will walk to claim a work post.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|Colonist",
		meta = (ClampMin = "1000", ClampMax = "30000"))
	float MaxClaimRangeCm = 8000.0f;

protected:
	virtual void BeginPlay() override;

private:
	// Best-fit world point for Role. Returns the actor location when no
	// matching station exists, so the brain still has a real target.
	FVector ResolveWorkPostForRole() const;
};
