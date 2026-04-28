#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "QRCoreSettings.generated.h"

UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Quiet Rift Core Settings"))
class QRCORE_API UQRCoreSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UQRCoreSettings();

	// World simulation tick rate in seconds (default 1.0s for colony sim)
	UPROPERTY(Config, EditAnywhere, Category = "Simulation", meta = (ClampMin = "0.1", ClampMax = "5.0"))
	float SimulationTickRate = 1.0f;

	// Day length in real seconds
	UPROPERTY(Config, EditAnywhere, Category = "World")
	float DayLengthSeconds = 1200.0f;

	// Night risk multiplier for raids
	UPROPERTY(Config, EditAnywhere, Category = "Threat")
	float NightRaidRiskMultiplier = 2.5f;

	// Base calories per game-day a survivor needs
	UPROPERTY(Config, EditAnywhere, Category = "Survival")
	float BaseDailyCalorieNeed = 2200.0f;

	// Base water ml per game-day
	UPROPERTY(Config, EditAnywhere, Category = "Survival")
	float BaseDailyWaterNeedML = 2500.0f;

	// Max stack sizes
	UPROPERTY(Config, EditAnywhere, Category = "Items")
	int32 DefaultMaxStackSize = 50;

	// Depot pull radius (world units)
	UPROPERTY(Config, EditAnywhere, Category = "Logistics")
	float DefaultDepotPullRadiusMeters = 25.0f;

	// Research micro-research multiplicative stack cap per family
	UPROPERTY(Config, EditAnywhere, Category = "Research")
	int32 MicroResearchMaxStacksPerFamily = 10;

	// Maximum morale value
	UPROPERTY(Config, EditAnywhere, Category = "Colony")
	float MaxMoraleValue = 100.0f;

	// Seed for world generation (0 = random)
	UPROPERTY(Config, EditAnywhere, Category = "WorldGen")
	int32 WorldSeed = 0;
};
