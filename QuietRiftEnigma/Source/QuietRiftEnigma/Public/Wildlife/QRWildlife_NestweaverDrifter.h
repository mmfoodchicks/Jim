#pragma once
#include "QRWildlifeBase.h"
#include "QRWildlife_NestweaverDrifter.generated.h"

// ANI_NESTWEAVER_DRIFTER — Egg-producing husbandry animal; gliding membrane fins
// Biomes: CanyonWebs, MossFields | Role: Husbandry/Livestock
UCLASS(BlueprintType, Blueprintable)
class QUIETRIFTENIGMA_API AQRWildlife_NestweaverDrifter : public AQRWildlifeBase
{
	GENERATED_BODY()
public:
	AQRWildlife_NestweaverDrifter();

	// Whether this drifter is penned and producing eggs
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "NestweaverDrifter")
	bool bIsPenned = false;

	// Hours (game time) until next egg is ready
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "NestweaverDrifter")
	float HoursUntilNextEgg = 12.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NestweaverDrifter")
	float EggLayIntervalHours = 8.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NestweaverDrifter")
	FName EggItemId = "ITEM_NESTWEAVER_EGG";

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "NestweaverDrifter")
	bool CollectEgg(TArray<FQRWildlifeDrop>& OutEggs);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
