#pragma once
#include "QRWildlifeBase.h"
#include "QRWildlife_RidgeCourser.generated.h"

// ANI_RIDGE_COURSER — Primary fast mount; long-bodied six-limbed runner with vane tail
// Biomes: WindPlains, BasaltShelf, RidgeShadows | Role: Mount
// Elite variant: ANI_RIDGE_COURSER_STORMLINE (alias: Gale)
UCLASS(BlueprintType, Blueprintable)
class QUIETRIFTENIGMA_API AQRWildlife_RidgeCourser : public AQRWildlifeBase
{
	GENERATED_BODY()
public:
	AQRWildlife_RidgeCourser();

	// Whether this specimen has been tamed for mounting
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "RidgeCourser")
	bool bIsTamed = false;

	// Rider character reference when mounted
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "RidgeCourser")
	TObjectPtr<AActor> MountedRider;

	// Stormline elite variant flag (longer vanes, harder sprint posture)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RidgeCourser")
	bool bIsStormlineElite = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RidgeCourser")
	float MountedSpeedMultiplier = 2.2f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RidgeCourser")
	float TamingDifficultyScore = 0.6f;

	UFUNCTION(BlueprintCallable, Category = "RidgeCourser")
	bool TryMount(AActor* Rider);

	UFUNCTION(BlueprintCallable, Category = "RidgeCourser")
	void Dismount();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
