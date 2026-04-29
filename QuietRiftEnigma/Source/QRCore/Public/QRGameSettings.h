#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "QRGameSettings.generated.h"

// Centralised game-balance tuning knobs for designers.
// Appears in Project Settings → QuietRift → Game Balance.
// All values are .ini-persisted (Config = Game) and hot-reloadable in editor.
//
// Rule: any "magic number" that a designer should be able to tweak without recompiling
// belongs here rather than being buried in a component constructor or CPP file.
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "QR Game Balance Settings"))
class QRCORE_API UQRGameSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UQRGameSettings();

	// ── Survival Vitals ──────────────────────
	UPROPERTY(Config, EditAnywhere, Category = "Survival|Vitals", meta = (ClampMin = "1"))
	float MaxHealth = 100.0f;

	UPROPERTY(Config, EditAnywhere, Category = "Survival|Vitals", meta = (ClampMin = "1"))
	float MaxHunger = 100.0f;

	UPROPERTY(Config, EditAnywhere, Category = "Survival|Vitals", meta = (ClampMin = "1"))
	float MaxThirst = 100.0f;

	UPROPERTY(Config, EditAnywhere, Category = "Survival|Vitals", meta = (ClampMin = "1"))
	float MaxFatigue = 100.0f;

	// ── Drain Rates (per real-second) ────────
	UPROPERTY(Config, EditAnywhere, Category = "Survival|Drain", meta = (ClampMin = "0"))
	float HungerDrainPerSecond = 0.01f;

	UPROPERTY(Config, EditAnywhere, Category = "Survival|Drain", meta = (ClampMin = "0"))
	float ThirstDrainPerSecond = 0.015f;

	UPROPERTY(Config, EditAnywhere, Category = "Survival|Drain", meta = (ClampMin = "0"))
	float FatigueDrainPerSecond = 0.005f;

	// ── Damage from Unmet Needs (per real-second) ──
	UPROPERTY(Config, EditAnywhere, Category = "Survival|Damage", meta = (ClampMin = "0"))
	float StarvationDamagePerSecond = 0.2f;

	UPROPERTY(Config, EditAnywhere, Category = "Survival|Damage", meta = (ClampMin = "0"))
	float DehydrationDamagePerSecond = 0.4f;

	UPROPERTY(Config, EditAnywhere, Category = "Survival|Damage", meta = (ClampMin = "0"))
	float ExhaustionDamagePerSecond = 0.05f;

	// DPS per degree Celsius below 35°C
	UPROPERTY(Config, EditAnywhere, Category = "Survival|Damage", meta = (ClampMin = "0"))
	float HypothermiaDamagePerDegreeBelowThreshold = 0.1f;

	// ── Encumbrance ───────────────────────────
	// Base carry weight independent of STR (kg)
	UPROPERTY(Config, EditAnywhere, Category = "Encumbrance", meta = (ClampMin = "1"))
	float BaseCarryWeightKg = 20.0f;

	// Extra carry weight per point of STR (kg)
	UPROPERTY(Config, EditAnywhere, Category = "Encumbrance", meta = (ClampMin = "0"))
	float CarryWeightPerSTR = 6.0f;

	// Ratio of MaxCarry at which sprint is blocked [0..1]
	UPROPERTY(Config, EditAnywhere, Category = "Encumbrance", meta = (ClampMin = "0", ClampMax = "1"))
	float SprintEncumbranceRatio = 0.85f;

	// ── Morale & Leadership ───────────────────
	UPROPERTY(Config, EditAnywhere, Category = "Leadership", meta = (ClampMin = "1"))
	float MaxMoraleIndex = 100.0f;

	// Hours before unresolved blocker starts escalating
	UPROPERTY(Config, EditAnywhere, Category = "Leadership", meta = (ClampMin = "0"))
	float DefaultGuidanceDelayHours = 6.0f;

	// IssueEscalationScore threshold that triggers QuestIssued state
	UPROPERTY(Config, EditAnywhere, Category = "Leadership", meta = (ClampMin = "1"))
	float QuestIssuedEscalationThreshold = 100.0f;

	// ── Weapons & Fouling ─────────────────────
	UPROPERTY(Config, EditAnywhere, Category = "Weapons", meta = (ClampMin = "0", ClampMax = "1"))
	float FoulingPerShot = 0.005f;

	UPROPERTY(Config, EditAnywhere, Category = "Weapons", meta = (ClampMin = "1"))
	float DirtyAmmoFoulingMultiplier = 5.0f;

	UPROPERTY(Config, EditAnywhere, Category = "Weapons", meta = (ClampMin = "1"))
	float SuppressorFoulingMultiplier = 1.5f;

	// Fouling threshold above which a malfunction roll occurs
	UPROPERTY(Config, EditAnywhere, Category = "Weapons", meta = (ClampMin = "0", ClampMax = "1"))
	float MalfunctionFoulingThreshold = 0.8f;

	UPROPERTY(Config, EditAnywhere, Category = "Weapons", meta = (ClampMin = "0"))
	float MalfunctionClearSeconds = 4.0f;

	// ── Food Safety ───────────────────────────
	// Minimum PackageIntegrity to retain EarthSealed / ShipRation SafeKnown exemption
	UPROPERTY(Config, EditAnywhere, Category = "Food", meta = (ClampMin = "0", ClampMax = "1"))
	float MinPackageIntegrityForSafe = 0.5f;

	// Calorie base used to normalise hunger gain from food (should match BaseDailyCalorieNeed)
	UPROPERTY(Config, EditAnywhere, Category = "Food", meta = (ClampMin = "1"))
	float CalorieNormalisationBase = 2200.0f;

	// ── Spoil & Storage ───────────────────────
	// StorageDeficitMod bounds (must satisfy Min < Max)
	UPROPERTY(Config, EditAnywhere, Category = "Logistics", meta = (ClampMin = "0", ClampMax = "1"))
	float StorageDeficitModMin = 0.5f;

	UPROPERTY(Config, EditAnywhere, Category = "Logistics", meta = (ClampMin = "1"))
	float StorageDeficitModMax = 3.0f;
};
