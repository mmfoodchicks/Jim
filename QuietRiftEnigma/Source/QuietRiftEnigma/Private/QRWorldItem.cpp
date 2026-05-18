#include "QRWorldItem.h"
#include "QRItemDefinition.h"
#include "QRItemInstance.h"
#include "QRInventoryComponent.h"
#include "QRCodexSubsystem.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Engine/AssetManager.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"

AQRWorldItem::AQRWorldItem()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	InteractionVolume = CreateDefaultSubobject<USphereComponent>(TEXT("InteractionVolume"));
	InteractionVolume->InitSphereRadius(45.0f);
	InteractionVolume->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractionVolume->SetCollisionResponseToAllChannels(ECR_Ignore);
	InteractionVolume->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	RootComponent = InteractionVolume;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(InteractionVolume);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetMobility(EComponentMobility::Movable);
}

void AQRWorldItem::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AQRWorldItem, ItemId);
	DOREPLIFETIME(AQRWorldItem, Quantity);
}

void AQRWorldItem::BeginPlay()
{
	Super::BeginPlay();

	// Designer-placed actor: pull mesh + ids from the editor def reference.
	if (!DefinitionForEditorPlacement.IsNull() && ItemId.IsNone())
	{
		const UQRItemDefinition* Def = DefinitionForEditorPlacement.LoadSynchronous();
		InitializeFrom(Def, FMath::Max(1, Quantity));
	}
}

void AQRWorldItem::InitializeFrom(const UQRItemDefinition* Definition, int32 InQuantity)
{
	if (!Definition) return;
	ItemId   = Definition->ItemId;
	Quantity = FMath::Max(1, InQuantity);

	if (Mesh)
	{
		UStaticMesh* Loaded = Definition->WorldMesh.LoadSynchronous();
		if (Loaded)
		{
			Mesh->SetStaticMesh(Loaded);

			// Clamp oversize source meshes so a dropped wildlife / world
			// item can't dwarf the player. Some Fab / placeholder meshes
			// import at 3-8 m which makes the dropped actor an unreadable
			// blue blob. Anything whose largest bounds extent exceeds
			// MaxAllowedHalfExtentCm gets scaled down uniformly.
			const FBoxSphereBounds B = Loaded->GetBounds();
			const float MaxExtent = FMath::Max3(B.BoxExtent.X, B.BoxExtent.Y, B.BoxExtent.Z);
			const float MaxAllowedHalfExtentCm = 100.0f;   // = 2 m largest dimension
			if (MaxExtent > MaxAllowedHalfExtentCm)
			{
				const float S = MaxAllowedHalfExtentCm / MaxExtent;
				Mesh->SetRelativeScale3D(FVector(S));
			}
			else
			{
				Mesh->SetRelativeScale3D(FVector(1.0f));
			}
		}
	}
}

bool AQRWorldItem::TryPickup(AActor* Picker)
{
	if (!Picker || !HasAuthority()) return false;

	UQRInventoryComponent* Inv = Picker->FindComponentByClass<UQRInventoryComponent>();
	if (!Inv) return false;

	// Resolve the definition. If we still have the editor reference it's
	// fastest to use it; otherwise fall back to the asset manager by id.
	const UQRItemDefinition* Def = DefinitionForEditorPlacement.LoadSynchronous();
	if (!Def)
	{
		// Asset-manager lookup keyed by the registered "QRItem" primary type.
		// Defined in QRItemDefinition::GetPrimaryAssetId.
		const FPrimaryAssetId AssetId(TEXT("QRItem"), ItemId);
		if (UAssetManager* AM = UAssetManager::GetIfInitialized())
		{
			const FSoftObjectPath Path = AM->GetPrimaryAssetPath(AssetId);
			Def = Cast<UQRItemDefinition>(Path.TryLoad());
		}
	}
	if (!Def) return false;

	int32 Remainder = 0;
	const EQRInventoryResult R = Inv->TryAddByDefinition(Def, Quantity, Remainder);
	if (R != EQRInventoryResult::Success && Remainder == Quantity)
	{
		// Couldn't take any of it — keep the world item around.
		return false;
	}

	// Partial pickup: leave the remainder on the ground.
	if (Remainder > 0)
	{
		Quantity = Remainder;
		return false;
	}

	// Codex: record pickup so the discovery tracker advances.
	if (UWorld* W = GetWorld())
	{
		if (UQRCodexSubsystem* Codex = W->GetSubsystem<UQRCodexSubsystem>())
		{
			Codex->Record(Def->ItemId, TEXT("Item"), Def->DisplayName,
				EQRCodexDiscoveryState::Sampled);
		}
	}

	Destroy();
	return true;
}
