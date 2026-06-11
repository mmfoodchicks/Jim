#include "QRSaveGameSystem.h"
#include "Kismet/GameplayStatics.h"

void UQRSaveGameSystem::MigrateToCurrentVersion(FQRGameSaveData& Data)
{
	if (Data.SaveVersion < 1)
	{
		// v0 → v1: FoodOriginClass, PackageIntegrity, bIsBulkItem added to FQRItemSaveData;
		// FQRWeaponSaveData, FQRLeaderSaveData, and HandsSlotState added to higher structs.
		// Struct defaults already cover every new field — no data patching needed.
		Data.SaveVersion = 1;
	}
	if (Data.SaveVersion < 2)
	{
		// v1 → v2: grid placement + EquippedSlot on FQRItemSaveData; armour/
		// offhand/hand entries folded into Items; ActiveInjuries, Oxygen,
		// CoreTemperature on FQRSurvivorSaveData; ResearchData actually
		// populated. Struct defaults cover the new fields, and
		// FQRSaveSnapshot::ApplyInventory falls back to the legacy
		// HandSlot/bHasHandSlot mirror when no EquippedSlot entries exist —
		// no data patching needed.
		Data.SaveVersion = 2;
	}
}

void UQRSaveGameSystem::SaveGame(const FQRGameSaveData& DataToSave, const FString& SlotName, int32 UserIndex)
{
	UQRSaveGame* SaveObj = Cast<UQRSaveGame>(UGameplayStatics::CreateSaveGameObject(UQRSaveGame::StaticClass()));
	if (!SaveObj) { OnSaveComplete.Broadcast(false); return; }

	SaveObj->GameData = DataToSave;
	SaveObj->GameData.SaveVersion    = QRCurrentSaveVersion;
	SaveObj->GameData.SaveSlotName   = SlotName;
	SaveObj->GameData.SaveTimestamp  = FDateTime::Now();

	FAsyncSaveGameToSlotDelegate Delegate;
	Delegate.BindUObject(this, &UQRSaveGameSystem::OnSaveToSlotComplete);
	UGameplayStatics::AsyncSaveGameToSlot(SaveObj, SlotName, UserIndex, Delegate);
}

void UQRSaveGameSystem::LoadGame(const FString& SlotName, int32 UserIndex)
{
	if (!DoesSaveExist(SlotName, UserIndex))
	{
		OnLoadComplete.Broadcast(false, FQRGameSaveData());
		return;
	}

	FAsyncLoadGameFromSlotDelegate Delegate;
	Delegate.BindUObject(this, &UQRSaveGameSystem::OnLoadFromSlotComplete);
	UGameplayStatics::AsyncLoadGameFromSlot(SlotName, UserIndex, Delegate);
}

bool UQRSaveGameSystem::DeleteSave(const FString& SlotName, int32 UserIndex)
{
	return UGameplayStatics::DeleteGameInSlot(SlotName, UserIndex);
}

bool UQRSaveGameSystem::DoesSaveExist(const FString& SlotName, int32 UserIndex) const
{
	return UGameplayStatics::DoesSaveGameExist(SlotName, UserIndex);
}

TArray<FString> UQRSaveGameSystem::GetAllSaveSlots() const
{
	// UE doesn't natively enumerate slots without a custom save directory query
	// Typically handled via platform file system or a registry approach
	return TArray<FString>();
}

void UQRSaveGameSystem::OnSaveToSlotComplete(const FString& SlotName, int32 UserIndex, bool bSuccess)
{
	OnSaveComplete.Broadcast(bSuccess);
}

void UQRSaveGameSystem::OnLoadFromSlotComplete(const FString& SlotName, int32 UserIndex, USaveGame* SaveGame)
{
	UQRSaveGame* QRSave = Cast<UQRSaveGame>(SaveGame);
	if (!QRSave)
	{
		OnLoadComplete.Broadcast(false, FQRGameSaveData());
		return;
	}

	MigrateToCurrentVersion(QRSave->GameData);
	OnLoadComplete.Broadcast(true, QRSave->GameData);
}
