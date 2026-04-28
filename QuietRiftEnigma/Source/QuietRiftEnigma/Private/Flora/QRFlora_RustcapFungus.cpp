#include "Flora/QRFlora_RustcapFungus.h"
#include "Net/UnrealNetwork.h"

AQRFlora_RustcapFungus::AQRFlora_RustcapFungus()
{
	SpeciesId      = FName("PLT_RustcapFungus");
	DisplayName    = FText::FromString("Rustcap Fungus");
	PreferredBiome = EQRBiomeType::Cave;
	MaxHarvestCharges = 4;
	RegrowthTimeHours = 36.0f;

	HarvestYields.Add({ FName("FOD_RUSTCAP_CAP"),       2, 5, 1.0f,  FGameplayTag() });
	HarvestYields.Add({ FName("MAT_FUNGAL_SPORE"),       1, 3, 0.7f,  FGameplayTag() }); // Medicine ingredient
	HarvestYields.Add({ FName("MAT_MUSHROOM_MYCELIUM"),  0, 2, 0.5f,  FGameplayTag() });
}

void AQRFlora_RustcapFungus::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority())
		bIsToxicVariant = FMath::FRand() < ToxicVariantChance;
}

void AQRFlora_RustcapFungus::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AQRFlora_RustcapFungus, bIsToxicVariant);
}
