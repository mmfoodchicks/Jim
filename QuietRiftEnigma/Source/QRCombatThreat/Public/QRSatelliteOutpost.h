#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "QRTypes.h"
#include "QRSatelliteOutpost.generated.h"

class UQRFactionComponent;
class AQRVanguardColony;
class AQRRaidScheduler;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnOutpostDestroyed, AQRSatelliteOutpost*, Outpost);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnOutpostTierChanged, AQRSatelliteOutpost*, Outpost, EQRVanguardHardpointTier, NewTier);

// A Vanguard satellite outpost — one of the rings of armed positions that surround the Concordat.
// Placed by world generation or the level designer; registers with its parent AQRVanguardColony
// at BeginPlay. Difficulty tier is computed from distance to the Concordat at runtime, making
// outposts naturally harder the closer the player gets to the Rift.
UCLASS(BlueprintType, Blueprintable)
class QRCOMBATTHREAT_API AQRSatelliteOutpost : public AActor
{
	GENERATED_BODY()

public:
	AQRSatelliteOutpost();

	// ── Identity ─────────────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Outpost")
	FText OutpostName;

	// The parent Concordat colony. Set automatically by AQRVanguardColony::RegisterOutpost()
	// or assigned manually in the level for hand-crafted layouts.
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Outpost")
	TObjectPtr<AQRVanguardColony> ParentColony;

	// ── Difficulty ───────────────────────────
	// Tier computed at BeginPlay from distance to the Concordat.
	// Can be overridden in the editor for hand-tuned encounters.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Outpost")
	EQRVanguardHardpointTier HardpointTier = EQRVanguardHardpointTier::ListeningPost;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Outpost")
	bool bOverrideTierManually = false;

	// Distance thresholds (meters from the Concordat) that define each tier.
	// Defaults match a ~10km map; tune in BP_QRSatelliteOutpost for your world size.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Outpost|Tiers")
	float ListeningPostMaxDistM = 8000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Outpost|Tiers")
	float ForwardPostMaxDistM   = 5000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Outpost|Tiers")
	float HardpointMaxDistM     = 2500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Outpost|Tiers")
	float InnerSanctumMaxDistM  = 1000.0f;

	// ── State ─────────────────────────────────
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Outpost")
	bool bIsDestroyed = false;

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Outpost")
	EQRControlState ControlState = EQRControlState::FactionControlled;

	// Cached distance to parent colony (meters), set at BeginPlay
	UPROPERTY(BlueprintReadOnly, Category = "Outpost")
	float DistanceToConcordatM = 0.0f;

	// ── Faction ───────────────────────────────
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UQRFactionComponent> FactionComp;

	// ── Events ───────────────────────────────
	UPROPERTY(BlueprintAssignable, Category = "Outpost|Events")
	FOnOutpostDestroyed OnOutpostDestroyed;

	UPROPERTY(BlueprintAssignable, Category = "Outpost|Events")
	FOnOutpostTierChanged OnTierChanged;

	// ── Interface ────────────────────────────
	// Maps the current distance to Concordat to a hardpoint tier.
	UFUNCTION(BlueprintPure, Category = "Outpost")
	EQRVanguardHardpointTier ComputeTierFromDistance(float DistanceM) const;

	// Maps hardpoint tier to the equivalent raid experience tier used by the raid scheduler.
	UFUNCTION(BlueprintPure, Category = "Outpost")
	EQRRaidExperienceTier GetRaidTier() const;

	// Mark this outpost as player-controlled (captured or destroyed).
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Outpost")
	void SetDestroyed();

	// Issue a raid from this outpost toward the player colony via the given scheduler.
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Outpost")
	void IssueRaid(AQRRaidScheduler* Scheduler);

	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
