#include "QRSaveSnapshotLibrary.h"
#include "QRInventoryComponent.h"
#include "QRItemInstance.h"
#include "QRItemDefinition.h"
#include "QRSurvivalComponent.h"
#include "QRResearchComponent.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"

// ── Definition resolver ──────────────────────────────────────────

UQRItemDefinition* FQRSaveSnapshot::ResolveItemDefinition(FName ItemId)
{
	if (ItemId.IsNone()) return nullptr;

	// ItemId -> object path, built once from the asset registry. Item defs
	// are seeded into nested buckets (Items/Weapons, Items/Containers, ...)
	// so path construction by convention can't find them all.
	static TMap<FName, FSoftObjectPath> PathById;
	static bool bScanned = false;
	if (!bScanned)
	{
		bScanned = true;
		IAssetRegistry& Registry =
			FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
		TArray<FAssetData> Assets;
		Registry.GetAssetsByClass(UQRItemDefinition::StaticClass()->GetClassPathName(),
			Assets, /*bSearchSubClasses*/ true);
		for (const FAssetData& Asset : Assets)
		{
			// Asset name == ItemId by seeding convention. If they ever
			// diverge the load below still verifies via the def's ItemId.
			PathById.Add(Asset.AssetName, Asset.ToSoftObjectPath());
		}
	}

	if (const FSoftObjectPath* Path = PathById.Find(ItemId))
	{
		if (UQRItemDefinition* Def = Cast<UQRItemDefinition>(Path->TryLoad()))
		{
			return Def;
		}
	}

	// Fallback: legacy root-folder layout.
	const FString RootPath = FString::Printf(
		TEXT("/Game/QuietRift/Data/Items/%s.%s"), *ItemId.ToString(), *ItemId.ToString());
	return LoadObject<UQRItemDefinition>(nullptr, *RootPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
}

// ── Inventory ────────────────────────────────────────────────────

FQRItemSaveData FQRSaveSnapshot::CaptureOne(const UQRItemInstance* Inst, EQRSavedEquipSlot EquipSlot)
{
	FQRItemSaveData Out;
	if (!Inst || !Inst->Definition) return Out;
	Out.ItemId           = Inst->Definition->ItemId;
	Out.Quantity         = Inst->Quantity;
	Out.Durability       = Inst->Durability;
	Out.SpoilProgress    = Inst->SpoilProgress;
	Out.EdibilityState   = Inst->EdibilityState;
	Out.InstanceGuid     = Inst->InstanceGuid;
	Out.FoodOriginClass  = Inst->Definition->FoodOriginClass;
	Out.PackageIntegrity = Inst->Definition->PackageIntegrity;
	Out.bIsBulkItem      = Inst->Definition->bIsBulkItem;
	Out.ContainerKind    = Inst->ContainerKind;
	Out.GridX            = Inst->GridX;
	Out.GridY            = Inst->GridY;
	Out.bRotated         = Inst->bRotated;
	Out.EquippedSlot     = EquipSlot;
	return Out;
}

void FQRSaveSnapshot::ApplyInstanceFields(UQRItemInstance* Inst, const FQRItemSaveData& Saved)
{
	if (!Inst) return;
	Inst->Durability     = Saved.Durability;
	Inst->SpoilProgress  = Saved.SpoilProgress;
	Inst->EdibilityState = Saved.EdibilityState;
	if (Saved.InstanceGuid.IsValid())
	{
		Inst->InstanceGuid = Saved.InstanceGuid;
	}
}

void FQRSaveSnapshot::CaptureInventory(const UQRInventoryComponent* Inv, FQRInventorySaveData& Out)
{
	Out = FQRInventorySaveData();
	if (!Inv) return;

	for (const UQRItemInstance* Inst : Inv->Items)
	{
		if (!Inst || !Inst->Definition) continue;
		Out.Items.Add(CaptureOne(Inst, EQRSavedEquipSlot::None));
	}

	auto CaptureSlot = [&Out](const UQRItemInstance* Inst, EQRSavedEquipSlot Slot)
	{
		if (Inst && Inst->Definition)
		{
			Out.Items.Add(CaptureOne(Inst, Slot));
		}
	};
	CaptureSlot(Inv->EquippedHelm,        EQRSavedEquipSlot::Helm);
	CaptureSlot(Inv->EquippedChestArmour, EQRSavedEquipSlot::ChestArmour);
	CaptureSlot(Inv->EquippedLegsArmour,  EQRSavedEquipSlot::LegsArmour);
	CaptureSlot(Inv->EquippedChestRig,    EQRSavedEquipSlot::ChestRig);
	CaptureSlot(Inv->EquippedBackpack,    EQRSavedEquipSlot::Backpack);
	CaptureSlot(Inv->HandSlot,            EQRSavedEquipSlot::Hand);
	CaptureSlot(Inv->OffhandSlot,         EQRSavedEquipSlot::Offhand);

	// Legacy mirrors (pre-v2 readers + main-menu Continue preview).
	if (Inv->HandSlot && Inv->HandSlot->Definition)
	{
		Out.HandSlot     = CaptureOne(Inv->HandSlot, EQRSavedEquipSlot::Hand);
		Out.bHasHandSlot = true;
	}
	Out.HandsSlotState = Inv->HandsSlotState;
}

void FQRSaveSnapshot::ApplyInventory(UQRInventoryComponent* Inv, const FQRInventorySaveData& Data)
{
	if (!Inv) return;

	// Wipe live state. Slot pointers clear directly; equipping the restored
	// containers below recomputes capacity, so a transiently-stale bonus
	// between here and there is harmless.
	Inv->Items.Empty();
	Inv->HandSlot            = nullptr;
	Inv->OffhandSlot         = nullptr;
	Inv->EquippedHelm        = nullptr;
	Inv->EquippedChestArmour = nullptr;
	Inv->EquippedLegsArmour  = nullptr;
	Inv->EquippedChestRig    = nullptr;
	Inv->EquippedBackpack    = nullptr;
	Inv->HandsSlotState      = EQRHandsSlotState::Empty;

	// Add one saved entry as a live instance; ForceAdd bypasses capacity
	// (the save was valid when written; re-validating during restore could
	// only drop items).
	auto AddOne = [Inv](const FQRItemSaveData& Saved) -> UQRItemInstance*
	{
		UQRItemDefinition* Def = ResolveItemDefinition(Saved.ItemId);
		if (!Def)
		{
			UE_LOG(LogTemp, Warning,
				TEXT("[QRSave] No item definition for '%s' — entry dropped on load"),
				*Saved.ItemId.ToString());
			return nullptr;
		}
		UQRItemInstance* Inst = Inv->ForceAddByDefinition(Def, FMath::Max(1, Saved.Quantity));
		if (Inst) ApplyInstanceFields(Inst, Saved);
		return Inst;
	};

	// Pass 1: containers — their grids must exist before anything restores
	// into ChestRig/Backpack cells.
	// Pass 2: armour. Pass 3: hands. Pass 4: loose grid items.
	TArray<TPair<UQRItemInstance*, const FQRItemSaveData*>> Placed;
	for (const FQRItemSaveData& Saved : Data.Items)
	{
		switch (Saved.EquippedSlot)
		{
		case EQRSavedEquipSlot::ChestRig:
		case EQRSavedEquipSlot::Backpack:
			if (UQRItemInstance* Inst = AddOne(Saved))
			{
				Inv->TryEquipContainer(Inst);
			}
			break;
		default:
			break;
		}
	}
	for (const FQRItemSaveData& Saved : Data.Items)
	{
		switch (Saved.EquippedSlot)
		{
		case EQRSavedEquipSlot::Helm:
		case EQRSavedEquipSlot::ChestArmour:
		case EQRSavedEquipSlot::LegsArmour:
			if (UQRItemInstance* Inst = AddOne(Saved))
			{
				Inv->TryEquipArmour(Inst);
			}
			break;
		case EQRSavedEquipSlot::Offhand:
			if (UQRItemInstance* Inst = AddOne(Saved))
			{
				Inv->TryEquipToOffhand(Inst);
			}
			break;
		case EQRSavedEquipSlot::Hand:
			if (UQRItemInstance* Inst = AddOne(Saved))
			{
				Inv->TryEquipToHandSlot(Inst);
			}
			break;
		case EQRSavedEquipSlot::None:
			if (UQRItemInstance* Inst = AddOne(Saved))
			{
				Placed.Add(TPair<UQRItemInstance*, const FQRItemSaveData*>(Inst, &Saved));
			}
			break;
		default:
			break;
		}
	}

	// Restore saved grid placement. ForceAdd auto-placed everything at the
	// first free cell, which may squat on another item's saved cell — two
	// passes let the second attempt succeed once the squatter moved home.
	for (int32 Pass = 0; Pass < 2; ++Pass)
	{
		for (const auto& Pair : Placed)
		{
			const FQRItemSaveData& Saved = *Pair.Value;
			UQRItemInstance* Inst = Pair.Key;
			if (!Inst || Saved.ContainerKind == EQRContainerKind::None || Saved.GridX < 0)
				continue;   // v1 save or never placed — keep the auto spot
			if (Inst->ContainerKind == Saved.ContainerKind &&
				Inst->GridX == Saved.GridX && Inst->GridY == Saved.GridY &&
				Inst->bRotated == Saved.bRotated)
				continue;   // already home
			Inv->TryMoveItem(Inst, Saved.ContainerKind, Saved.GridX, Saved.GridY, Saved.bRotated);
		}
	}

	// Legacy v1 saves: no EquippedSlot data, hand slot mirrored separately.
	if (!Inv->HandSlot && Data.bHasHandSlot && !Data.HandSlot.ItemId.IsNone())
	{
		for (UQRItemInstance* Inst : Inv->Items)
		{
			if (Inst && Inst->Definition && Inst->Definition->ItemId == Data.HandSlot.ItemId)
			{
				Inv->TryEquipToHandSlot(Inst);
				break;
			}
		}
	}
	Inv->HandsSlotState = Inv->HandSlot ? EQRHandsSlotState::Occupied : EQRHandsSlotState::Empty;

	Inv->OnInventoryChanged.Broadcast();
}

// ── Survival ─────────────────────────────────────────────────────

void FQRSaveSnapshot::CaptureSurvival(const UQRSurvivalComponent* Surv, FQRSurvivorSaveData& Out)
{
	if (!Surv) return;
	Out.Health          = Surv->Health;
	Out.Hunger          = Surv->Hunger;
	Out.Thirst          = Surv->Thirst;
	Out.Fatigue         = Surv->Fatigue;
	Out.Oxygen          = Surv->Oxygen;
	Out.CoreTemperature = Surv->CoreTemperature;
	Out.bIsAlive        = !Surv->bIsDead;
	Out.ActiveInjuries  = Surv->ActiveInjuries;
}

void FQRSaveSnapshot::ApplySurvival(UQRSurvivalComponent* Surv, const FQRSurvivorSaveData& Data)
{
	if (!Surv) return;
	Surv->Health          = Data.Health;
	Surv->Hunger          = Data.Hunger;
	Surv->Thirst          = Data.Thirst;
	Surv->Fatigue         = Data.Fatigue;
	Surv->Oxygen          = Data.Oxygen;
	Surv->CoreTemperature = Data.CoreTemperature;
	Surv->bIsDead         = !Data.bIsAlive;
	Surv->ActiveInjuries  = Data.ActiveInjuries;
}

// ── Research ─────────────────────────────────────────────────────

void FQRSaveSnapshot::CaptureResearch(const UQRResearchComponent* Research, FQRResearchSaveData& Out)
{
	if (!Research) return;
	Out.TechNodeStates      = Research->TechNodeStates;
	Out.MicroResearchStates = Research->MicroResearchStates;
	Out.MicroResearchQueue  = Research->MicroResearchQueue;
	Out.CodexStates         = Research->CodexStates;
}

void FQRSaveSnapshot::ApplyResearch(UQRResearchComponent* Research, const FQRResearchSaveData& Data)
{
	if (!Research) return;
	// Empty save (new game / pre-v2 file) — don't stomp a freshly-seeded
	// tech tree with nothing.
	if (Data.TechNodeStates.Num() == 0 && Data.MicroResearchStates.Num() == 0 &&
		Data.CodexStates.Num() == 0)
	{
		return;
	}
	Research->TechNodeStates      = Data.TechNodeStates;
	Research->MicroResearchStates = Data.MicroResearchStates;
	Research->MicroResearchQueue  = Data.MicroResearchQueue;
	Research->CodexStates         = Data.CodexStates;
}
