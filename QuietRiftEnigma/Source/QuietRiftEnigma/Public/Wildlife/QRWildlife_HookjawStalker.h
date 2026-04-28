#pragma once
#include "QRWildlifeBase.h"
#include "QRWildlife_HookjawStalker.generated.h"

// ANM_HOOK_STALKER_001 — First serious predator; stealth/noise threat
// Biomes: dense forest, ravine, abandoned structure
UCLASS(BlueprintType, Blueprintable)
class QUIETRIFTENIGMA_API AQRWildlife_HookjawStalker : public AQRWildlifeBase
{
	GENERATED_BODY()
public:
	AQRWildlife_HookjawStalker();

	// Stalker becomes aggressive if player noise exceeds this value
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stalker")
	float AggroNoiseThreshold = 0.6f;

	// True while actively tracking a target
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Stalker")
	bool bIsStalking = false;

	// Damage per bite attack
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stalker")
	float BiteDamage = 40.0f;

	// Whether the stalker is part of an ambush-configured pack
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stalker")
	bool bPackHunter = false;

	virtual void OnThreatDetected_Implementation(AActor* Threat) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
