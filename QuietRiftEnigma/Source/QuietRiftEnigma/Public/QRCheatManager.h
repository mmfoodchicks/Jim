#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CheatManager.h"
#include "QRCheatManager.generated.h"

// Debug / QA cheat manager.
// Exec functions are invoked from the UE console (~ key) or via -ExecCmds on the command line.
// None of these compile in shipping builds (wrapped by WITH_EDITOR || !UE_BUILD_SHIPPING).
//
// Usage examples:
//   qr.GiveItem stone_axe 3
//   qr.SetMorale 90
//   qr.ForceRaid
//   qr.UnlockTech T1_Metalwork
//   qr.SetWeather AcidRain 0.8
UCLASS()
class QUIETRIFTENIGMA_API UQRCheatManager : public UCheatManager
{
	GENERATED_BODY()

public:
	// Give ItemId to the local player's inventory (Quantity defaults to 1)
	UFUNCTION(Exec)
	void QRGiveItem(const FString& ItemId, int32 Quantity = 1);

	// Set the colony morale index to Value [0..100]
	UFUNCTION(Exec)
	void QRSetMorale(float Value);

	// Immediately trigger a raid event at maximum threat
	UFUNCTION(Exec)
	void QRForceRaid();

	// Unlock a tech node by ID
	UFUNCTION(Exec)
	void QRUnlockTech(const FString& TechNodeId);

	// Set local player's health to Value
	UFUNCTION(Exec)
	void QRSetHealth(float Value);

	// Remove all active injuries from the local player
	UFUNCTION(Exec)
	void QRKillAllInjuries();

	// Trigger a weather event: QRSetWeather AcidRain [Intensity=0.5] [DurationHours=4]
	UFUNCTION(Exec)
	void QRSetWeather(const FString& WeatherType, float Intensity = 0.5f, float DurationHours = 4.0f);

	// Clear any active weather event
	UFUNCTION(Exec)
	void QRClearWeather();

	// Reset scent intensity on the local pawn to zero
	UFUNCTION(Exec)
	void QRClearScent();

	// Print a summary of the local player's current vitals to the screen and log
	UFUNCTION(Exec)
	void QRPrintVitals();
};
