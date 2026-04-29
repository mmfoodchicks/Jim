#pragma once
#include "QRWildlifeBase.h"
#include "QRWildlife_TrenchDiggers.generated.h"

// PRD_TRENCH_DIGGERS — Siege-capable digger; rotating mineral tooth collars threaten base walls
// Biomes: BasaltShelf, CraterFloors | Role: Coordinated Breach
// Elite variant: PRD_TRENCH_DIGGERS_SAPPER (larger tooth collars, heavier soil eruptions)
UCLASS(BlueprintType, Blueprintable)
class QUIETRIFTENIGMA_API AQRWildlife_TrenchDiggers : public AQRWildlifeBase
{
	GENERATED_BODY()
public:
	AQRWildlife_TrenchDiggers();

	// Sapper elite flag
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TrenchDigger")
	bool bIsSapperElite = false;

	// Wall/building damage per dig cycle
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TrenchDigger")
	float WallDamagePerCycle = 45.0f;

	// Time per dig cycle in seconds
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TrenchDigger")
	float DigCycleSeconds = 3.0f;

	// Pack coordination: shares dig target among nearby diggers
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TrenchDigger")
	float PackCoordinationRadius = 1200.0f;

	// Whether currently underground (tunneling state)
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "TrenchDigger")
	bool bIsTunneling = false;

	UFUNCTION(BlueprintCallable, Category = "TrenchDigger")
	void StartDigging(AActor* TargetStructure);

	UFUNCTION(BlueprintCallable, Category = "TrenchDigger")
	void SurfaceErupt();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
