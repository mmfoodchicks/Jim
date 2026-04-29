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

	// ── Disease Transmission ─────────────────
	// Sphere radius (cm) for shared-meal infection spread
	UPROPERTY(Config, EditAnywhere, Category = "Disease", meta = (ClampMin = "50"))
	float DiseaseSpreadRadiusCm = 300.0f;

	// Probability per nearby survivor of catching a spoiled-food infection
	UPROPERTY(Config, EditAnywhere, Category = "Disease", meta = (ClampMin = "0", ClampMax = "1"))
	float SpoiledFoodSpreadChance = 0.30f;

	// Probability per nearby survivor of catching a contaminated-water infection
	UPROPERTY(Config, EditAnywhere, Category = "Disease", meta = (ClampMin = "0", ClampMax = "1"))
	float ContaminatedWaterSpreadChance = 0.20f;

	// ── Weather Events ────────────────────────
	// Minimum real-hours between consecutive weather events
	UPROPERTY(Config, EditAnywhere, Category = "Weather", meta = (ClampMin = "1"))
	float WeatherEventMinIntervalHours = 48.0f;

	// Maximum real-hours between consecutive weather events
	UPROPERTY(Config, EditAnywhere, Category = "Weather", meta = (ClampMin = "1"))
	float WeatherEventMaxIntervalHours = 168.0f;

	// Default duration (hours) of a triggered weather event
	UPROPERTY(Config, EditAnywhere, Category = "Weather", meta = (ClampMin = "0.5"))
	float WeatherEventDefaultDurationHours = 4.0f;

	// ── Scent / Detection ────────────────────
	// Natural scent decay per real-second (0 = no decay)
	UPROPERTY(Config, EditAnywhere, Category = "Scent", meta = (ClampMin = "0", ClampMax = "1"))
	float ScentDecayRatePerSecond = 0.01f;

	// Scent intensity added per kg of raw meat carried
	UPROPERTY(Config, EditAnywhere, Category = "Scent", meta = (ClampMin = "0"))
	float MeatScentIntensityPerKg = 0.20f;

	// Maximum predator detection-radius multiplier at full scent (1.0 = no bonus)
	UPROPERTY(Config, EditAnywhere, Category = "Scent", meta = (ClampMin = "1", ClampMax = "10"))
	float MaxPredatorDetectionRadiusMult = 3.0f;

	// ── Mentorship / Lineage ─────────────────
	// Fraction of mentor's skill level inherited by a replacement NPC (die-and-replace)
	UPROPERTY(Config, EditAnywhere, Category = "Mentorship", meta = (ClampMin = "0", ClampMax = "1"))
	float MentorshipInheritanceFraction = 0.33f;

	// Extra XP fraction granted when training under a mentor (apprenticeship bonus)
	UPROPERTY(Config, EditAnywhere, Category = "Mentorship", meta = (ClampMin = "0", ClampMax = "1"))
	float MentorshipXPBoostFraction = 0.15f;

	// ── Combat / Weapon ───────────────────────
	// Fouling rate multiplier during dust storms
	UPROPERTY(Config, EditAnywhere, Category = "Combat", meta = (ClampMin = "1"))
	float DustStormFoulingMultiplier = 2.0f;

	// Acid rain damage per real-second to unprotected structures
	UPROPERTY(Config, EditAnywhere, Category = "Combat", meta = (ClampMin = "0"))
	float AcidRainStructureDamagePerSecond = 0.002f;
};
