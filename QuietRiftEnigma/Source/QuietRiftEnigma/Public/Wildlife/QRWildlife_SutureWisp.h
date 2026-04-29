#pragma once
#include "QRWildlifeBase.h"
#include "QRWildlife_SutureWisp.generated.h"

// PRD_SUTURE_WISP — Ambush bind predator; ribbon body with oil-sheen and filament tail
// Biomes: RidgeShadows, CanyonWebs | Role: Solo Ambush
// Elite variant: PRD_SUTURE_WISP_PRIME (taller, denser ribbon, apex ambush)
UCLASS(BlueprintType, Blueprintable)
class QUIETRIFTENIGMA_API AQRWildlife_SutureWisp : public AQRWildlifeBase
{
	GENERATED_BODY()
public:
	AQRWildlife_SutureWisp();

	// Prime elite flag
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SutureWisp")
	bool bIsPrimeElite = false;

	// Filament bind: wraps target, reducing move speed
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SutureWisp")
	float FilamentBindSpeedReduction = 0.65f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SutureWisp")
	float FilamentBindDuration = 4.0f;

	// Claw slash damage
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SutureWisp")
	float SlashDamage = 35.0f;

	// Ribbon body makes silhouette hard to judge — increases miss chance at range
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SutureWisp")
	float RangedMissChanceBonus = 0.2f;

	UFUNCTION(BlueprintCallable, Category = "SutureWisp")
	void TriggerFilamentBind(AActor* Target);

	virtual void OnThreatDetected_Implementation(AActor* Threat) override;
};
