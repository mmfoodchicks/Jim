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
