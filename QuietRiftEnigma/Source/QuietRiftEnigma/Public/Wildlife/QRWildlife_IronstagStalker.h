#pragma once
#include "QRWildlifeBase.h"
#include "QRWildlife_IronstagStalker.generated.h"

// PRD_IRONSTAG_STALKER — Late-mid apex predator; ferric antler blades + static chest plate
// Biomes: MagneticRidges, HighRims | Role: Solo Territorial
// Visual: Regal, rare, landmark silhouette with iron-red/black/silver spark palette
UCLASS(BlueprintType, Blueprintable)
class QUIETRIFTENIGMA_API AQRWildlife_IronstagStalker : public AQRWildlifeBase
{
	GENERATED_BODY()
public:
	AQRWildlife_IronstagStalker();

	// Territory radius — other Stalkers will not spawn within this range
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "IronstagStalker")
	float TerritoryRadius = 5000.0f;

	// Antler blade sweep attack damage
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "IronstagStalker")
	float AntlerSwipeDamage = 75.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "IronstagStalker")
	float AntlerSwipeRadius = 250.0f;

	// Storm static discharge: stuns targets near chest plate during rain/magnetic events
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "IronstagStalker")
	float StaticDischargeRadius = 350.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "IronstagStalker")
	float StaticDischargeDamage = 40.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "IronstagStalker")
	float StaticDischargeStunDuration = 2.5f;

	// Whether a magnetic storm event is active (boosts static discharge)
	UPROPERTY(BlueprintReadOnly, Category = "IronstagStalker")
	bool bMagneticStormActive = false;

	UFUNCTION(BlueprintCallable, Category = "IronstagStalker")
	void TriggerAntlerSwipe();

	UFUNCTION(BlueprintCallable, Category = "IronstagStalker")
	void TriggerStaticDischarge();

	virtual void OnThreatDetected_Implementation(AActor* Threat) override;
};
