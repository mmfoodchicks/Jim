#pragma once
#include "QRWildlifeBase.h"
#include "QRWildlife_VaultbackDray.generated.h"

// ANI_VAULTBACK_DRAY — Primary heavy transport mount; dorsal vault depression with harness ribs
// Biomes: CraterFloors, BasaltShelf | Role: Heavy Mount / Cargo
// Elite variant: ANI_VAULTBACK_DRAY_BASTION (alias: Stonevault)
UCLASS(BlueprintType, Blueprintable)
class QUIETRIFTENIGMA_API AQRWildlife_VaultbackDray : public AQRWildlifeBase
{
	GENERATED_BODY()
public:
	AQRWildlife_VaultbackDray();

	// Current cargo weight loaded into dorsal vault (kg)
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "VaultbackDray")
	float CurrentCargoWeightKg = 0.0f;

	// Maximum cargo capacity (kg)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VaultbackDray")
	float MaxCargoWeightKg = 500.0f;

	// Bastion elite flag (deeper vaults, heavier armor, siege beast)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VaultbackDray")
	bool bIsBastionElite = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VaultbackDray")
	bool bIsTamed = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VaultbackDray")
	float TamingDifficultyScore = 0.4f;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "VaultbackDray")
	bool LoadCargo(float WeightKg);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "VaultbackDray")
	void UnloadCargo();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
