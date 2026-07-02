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
	EQRNPCRole ColonistRole = EQRNPCRole::Unassigned;

	// Max distance (cm) the colonist will walk to claim a work post.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|Colonist",
		meta = (ClampMin = "1000", ClampMax = "30000"))
	float MaxClaimRangeCm = 8000.0f;

	// ── Job execution (5s cadence via actor TickInterval) ─────────
	// How close (cm) to AssignedWorkPost the colonist must stand before
	// the stationary jobs (farm / guard-advance) fire.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|Colonist|Job",
		meta = (ClampMin = "100", ClampMax = "5000"))
	float JobReachCm = 700.0f;

	// Seed a farmer replants into a fallow plot whose DefaultYieldItemId
	// is unset. Lattice tubers are the colony staple crop.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|Colonist|Job")
	FName FallbackSeedId = FName(TEXT("FOD_LATTICE_TUBER"));

	// Health restored per job tick by a medic treating a patient.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|Colonist|Job",
		meta = (ClampMin = "1", ClampMax = "100"))
	float MedicHealPerTick = 8.0f;

	virtual void Tick(float DeltaTime) override;

protected:
	virtual void BeginPlay() override;

private:
	// Best-fit world point for Role. Returns the actor location when no
	// matching station exists, so the brain still has a real target.
	FVector ResolveWorkPostForRole() const;

	// Role duties, fired from Tick at the actor's TickInterval while the
	// brain is in Work state. Farmer/Guard require standing at the post;
	// Medic roams to patients so it skips the at-post gate.
	void TickFarmerJob();
	void TickGuardJob();
	void TickMedicJob();

	// Deliver a harvested yield into the nearest depot's storage. Silently
	// no-ops when no depot is in claim range (colony eats it on the spot).
	void DepositToNearestDepot(FName ItemId, int32 Quantity);

	// Guard patrol waypoint cursor into the name-sorted build-piece list.
	int32 PatrolCursor = 0;
};
