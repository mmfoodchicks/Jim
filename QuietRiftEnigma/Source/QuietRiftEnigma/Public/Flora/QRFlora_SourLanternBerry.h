#pragma once
#include "QRHarvestableBase.h"
#include "QRFlora_SourLanternBerry.generated.h"

// Ripeness state affects edibility: Unripe = toxic, Ripe = safe, Overripe = sick risk
UENUM(BlueprintType)
enum class EBerryRipeness : uint8
{
	Unripe   UMETA(DisplayName = "Unripe"),
	Ripe     UMETA(DisplayName = "Ripe"),
	Overripe UMETA(DisplayName = "Overripe"),
};

// PLT_SOUR_BERRY_001 — Early food source with ripeness risk; animal lure
// Priority: High | Biomes: forest edge, shaded clearing
UCLASS(BlueprintType, Blueprintable)
class QUIETRIFTENIGMA_API AQRFlora_SourLanternBerry : public AQRHarvestableBase
{
	GENERATED_BODY()
public:
	AQRFlora_SourLanternBerry();

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Berry")
	EBerryRipeness Ripeness = EBerryRipeness::Unripe;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Berry")
	float HoursToRipen = 8.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Berry")
	float HoursToOverripen = 24.0f;

	void AdvanceRipeness(float GameHoursElapsed);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
