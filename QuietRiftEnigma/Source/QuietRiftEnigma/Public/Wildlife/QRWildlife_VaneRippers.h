#pragma once
#include "QRWildlifeBase.h"
#include "QRWildlife_VaneRippers.generated.h"

// PRD_VANE_RIPPERS — Pack flank predators; knife-thin with dorsal vanes
// Biomes: WindPlains, BasaltShelf | Role: Pack Flank
// Elite variant: PRD_VANE_RIPPERS_GALEPACK (larger vanes, higher speed, more organized)
UCLASS(BlueprintType, Blueprintable)
class QUIETRIFTENIGMA_API AQRWildlife_VaneRippers : public AQRWildlifeBase
{
	GENERATED_BODY()
public:
	AQRWildlife_VaneRippers();

	// Galepack elite flag
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VaneRippers")
	bool bIsGalepackElite = false;

	// Maximum pack members for coordinated flank
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VaneRippers")
	int32 MaxPackSize = 6;

	// Flanking bonus damage when attacking from sides/rear
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VaneRippers")
	float FlankDamageMultiplier = 1.5f;

	// Wind-aided speed boost when vanes align with wind direction
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VaneRippers")
	float WindSpeedBonus = 250.0f;

	// Ripper slash damage
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VaneRippers")
	float SlashDamage = 28.0f;

	virtual void OnThreatDetected_Implementation(AActor* Threat) override;
};
