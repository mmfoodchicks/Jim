#include "Flora/QRFlora_SourLanternBerry.h"
#include "Net/UnrealNetwork.h"

AQRFlora_SourLanternBerry::AQRFlora_SourLanternBerry()
{
	SpeciesId      = FName("PLT_SourLanternBerry");
	DisplayName    = FText::FromString("Sour Lantern Berry");
	PreferredBiome = EQRBiomeType::ForestEdge;
	MaxHarvestCharges = 10;
	RegrowthTimeHours = 18.0f;

	// Berry drops; edibility depends on ripeness (Blueprint handles the actual swap)
	HarvestYields.Add({ FName("FOD_LANTERN_BERRY_RIPE"),   2, 5, 1.0f,  FGameplayTag() });
	HarvestYields.Add({ FName("MAT_LANTERN_SEED"),          0, 2, 0.4f,  FGameplayTag() }); // Planting
}

void AQRFlora_SourLanternBerry::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AQRFlora_SourLanternBerry, Ripeness);
}

void AQRFlora_SourLanternBerry::AdvanceRipeness(float GameHoursElapsed)
{
	// Simple state machine — tracked via accumulated hours in Blueprint tick
	// C++ just defines the states; timing managed by a timer in BP or GameMode
}
