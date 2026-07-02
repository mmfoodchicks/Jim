#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "QRWorldGenTypes.h"
#include "QRCrashSiteActor.generated.h"

class UStaticMeshComponent;
class USphereComponent;
class AQRWorldItem;

/**
 * Hardcoded crash-site wreck — one of seven canonical archetypes
 * (ArmoryWreck / MedBayWreck / GalleyWreck / EngineeringWreck /
 * AvionicsWreck / LuggageWreck / PowerModuleWreck). The wreck mesh
 * decorates the site; items from the loot template scatter as
 * individual AQRWorldItem actors around the wreck so the player
 * sees physical debris instead of a "search container" prompt.
 *
 * "Hardcoded" means the loot CATEGORY is fixed per archetype
 * (ArmoryWreck always yields weapon/ammo, never food). Per-instance
 * quantities + chance rolls vary against a deterministic seed so
 * the same WorldSeed produces the same wreck loot every time.
 */
UCLASS(BlueprintType, Blueprintable)
class QUIETRIFTENIGMA_API AQRCrashSiteActor : public AActor
{
	GENERATED_BODY()

public:
	AQRCrashSiteActor();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "QR|CrashSite")
	TObjectPtr<USphereComponent> ProximitySphere;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "QR|CrashSite")
	TObjectPtr<UStaticMeshComponent> WreckMesh;

	// Archetype id (ArmoryWreck / MedBayWreck / …). Set by spawner.
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "QR|CrashSite")
	FName ArchetypeId;

	// Player-readable name for HUD prompts.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|CrashSite")
	FText DisplayName;

	// Radius (cm) within which loot items scatter on Populate.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|CrashSite",
		meta = (ClampMin = "100", ClampMax = "3000"))
	float ScatterRadiusCm = 600.0f;

	// World-item actor class used when scattering loot — typically
	// AQRWorldItem. Designer can swap to a subclass with custom mesh.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "QR|CrashSite")
	TSubclassOf<AQRWorldItem> WorldItemClass;

	// Tool-gated entry. When non-None, the player needs that item id in
	// their inventory before PopulateLoot will scatter the interior
	// loot. Set by the spawner from FQRPOIPlacement::RequiredToolItemId.
	// Major hero crashes leave this NAME_None (always-accessible);
	// smaller wrecks gate behind cutters/keys/pry bars.
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "QR|CrashSite")
	FName RequiredToolItemId;

	// True after the player has interacted with the right tool and the
	// interior loot has been populated. Set by UnlockWithTool.
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "QR|CrashSite")
	bool bUnlocked = false;

	// Attempt to unlock this site using whatever's in the actor's
	// inventory. Returns true if RequiredToolItemId is NAME_None (no
	// gate) or the actor's inventory holds the required item. The
	// successful unlock is one-shot -- subsequent calls just return
	// true so re-entering the radius doesn't re-scatter loot. On the
	// FIRST successful unlock, the stashed PendingLootTemplate (set by
	// the worldgen spawner for tool-gated sites) scatters its loot.
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "QR|CrashSite")
	bool TryUnlockWithInventory(class UQRInventoryComponent* Inventory);

	// Loot held back until unlock. The spawner stashes the archetype's
	// template + deterministic seed here for tool-gated sites instead of
	// scattering immediately (an always-open site never uses these --
	// the spawner populates it directly).
	UPROPERTY()
	FQRCrashLootTemplate PendingLootTemplate;

	UPROPERTY()
	int32 PendingLootSeed = 0;

	UPROPERTY()
	bool bHasPendingLoot = false;

	// Populate this wreck by scattering AQRWorldItem actors for each
	// loot template entry that passes its SpawnChance roll. Quantities
	// roll in [MinQty, MaxQty]. Each spawned actor sits on the ground
	// via downward line trace. Idempotent — calling again clears prior
	// scattered loot first.
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "QR|CrashSite")
	void PopulateLoot(const FQRCrashLootTemplate& Template, int32 Seed);

	// Wipe scattered loot actors (used on regen).
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "QR|CrashSite")
	void ClearScatteredLoot();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	UPROPERTY(Transient)
	TArray<TWeakObjectPtr<AActor>> ScatteredLoot;
};
