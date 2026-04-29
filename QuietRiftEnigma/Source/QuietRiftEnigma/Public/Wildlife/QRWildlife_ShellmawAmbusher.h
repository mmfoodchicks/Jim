#pragma once
#include "QRWildlifeBase.h"
#include "QRWildlife_ShellmawAmbusher.generated.h"

// PRD_SHELLMAW_AMBUSHER — Buried ambush beast; hides as mineral hump then opens trapdoor maw
// Biomes: WetBasins, CraterWalls | Role: Buried Ambush / Jump-scare predator
UCLASS(BlueprintType, Blueprintable)
class QUIETRIFTENIGMA_API AQRWildlife_ShellmawAmbusher : public AQRWildlifeBase
{
	GENERATED_BODY()
public:
	AQRWildlife_ShellmawAmbusher();

	// Whether the ambusher is currently buried (disguised as terrain)
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "ShellmawAmbusher")
	bool bIsBuried = true;

	// Detection radius while buried (very small — hard to spot)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ShellmawAmbusher")
	float BuriedDetectionRadius = 180.0f;

	// Detection radius while surfaced
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ShellmawAmbusher")
	float SurfacedDetectionRadius = 800.0f;

	// Maw snap damage on emerge
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ShellmawAmbusher")
	float MawSnapDamage = 90.0f;

	// Grab duration (target is held, taking periodic bleed)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ShellmawAmbusher")
	float GrabDurationSeconds = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ShellmawAmbusher")
	float GrabBleedPerSecond = 12.0f;

	// Cooldown before reburying after surfacing
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ShellmawAmbusher")
	float ReburyCooldownSeconds = 20.0f;

	UFUNCTION(BlueprintCallable, Category = "ShellmawAmbusher")
	void Emerge();

	UFUNCTION(BlueprintCallable, Category = "ShellmawAmbusher")
	void Rebury();

	UFUNCTION(BlueprintCallable, Category = "ShellmawAmbusher")
	void TriggerMawSnap(AActor* Target);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void OnThreatDetected_Implementation(AActor* Threat) override;
};
