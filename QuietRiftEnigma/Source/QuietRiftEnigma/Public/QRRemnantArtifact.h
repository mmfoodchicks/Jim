#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "QRTypes.h"
#include "QRRemnantArtifact.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnRemnantArtifactCollected,
	class AQRRemnantArtifact*, Artifact, AActor*, Collector);

// A physical Progenitor artifact found near Remnant sites.
//
// Lore: when a Remnant structure stirs, fragments of its automated systems shed loose —
// crystalline data shards, depleted power cells, broken signal-spire components. Some
// are inert curios; others still carry charge or stored data. Collecting them feeds the
// Remnant research family directly and, for rare types, unlocks codex entries that hint
// at what the Progenitors were before they vanished.
//
// Place by hand for hand-crafted set pieces, or spawn procedurally near AQRRemnantStructure
// actors with a higher density when their wake state is Active or Hostile.
UCLASS(BlueprintType, Blueprintable)
class QUIETRIFTENIGMA_API AQRRemnantArtifact : public AActor
{
	GENERATED_BODY()

public:
	AQRRemnantArtifact();

	// ── Identity ─────────────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Artifact")
	FName ArtifactId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Artifact")
	EQRRemnantArtifactType ArtifactType = EQRRemnantArtifactType::DataShard;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Artifact")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Artifact", meta = (MultiLine = "true"))
	FText FlavorText;

	// ── Yield ─────────────────────────────────
	// Research points awarded to the colony's Research component on collection.
	// Memory Cores grant the most; Data Shards the least.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Artifact", meta = (ClampMin = "0"))
	float ResearchPointsOnCollect = 25.0f;

	// Always granted into the Remnant research family.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Artifact")
	EQRResearchFamily ResearchFamily = EQRResearchFamily::Remnant;

	// Carry weight in kg when held in inventory.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Artifact", meta = (ClampMin = "0"))
	float WeightKg = 1.0f;

	// Higher = rarer at procedural spawn. 1.0 = common, 0.05 = Memory Core rarity.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Artifact", meta = (ClampMin = "0", ClampMax = "1"))
	float SpawnRarityWeight = 1.0f;

	// ── Codex ─────────────────────────────────
	// Codex entry to mark "Sampled" or "Known" on collection.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Artifact")
	FName CodexEntryId;

	// If true, collecting this artifact unlocks the linked codex entry directly to "Known"
	// rather than just "Sampled". Reserved for Memory Cores.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Artifact")
	bool bUnlocksCodexDirectly = false;

	// ── State ─────────────────────────────────
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Artifact")
	bool bCollected = false;

	// ── Events ───────────────────────────────
	UPROPERTY(BlueprintAssignable, Category = "Artifact|Events")
	FOnRemnantArtifactCollected OnCollected;

	// ── Interface ────────────────────────────
	// Called when a player or NPC collects this artifact. Fires OnCollected and disables
	// further interaction. Returns the research yield granted.
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Artifact")
	float Collect(AActor* Collector);

	UFUNCTION(BlueprintPure, Category = "Artifact")
	bool CanBeCollected() const { return !bCollected; }

	// Blueprint hook for visual/audio feedback when collected.
	UFUNCTION(BlueprintNativeEvent, Category = "Artifact")
	void OnArtifactCollected(AActor* Collector);
	virtual void OnArtifactCollected_Implementation(AActor* /*Collector*/) {}

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
