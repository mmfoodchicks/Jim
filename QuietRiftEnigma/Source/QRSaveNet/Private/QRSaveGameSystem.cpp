#include "QRSaveGameSystem.h"
#include "Kismet/GameplayStatics.h"

void UQRSaveGameSystem::SaveGame(const FQRGameSaveData& DataToSave, const FString& SlotName, int32 UserIndex)
{
	UQRSaveGame* SaveObj = Cast<UQRSaveGame>(UGameplayStatics::CreateSaveGameObject(UQRSaveGame::StaticClass()));
	if (!SaveObj) { OnSaveComplete.Broadcast(false); return; }

	SaveObj->GameData = DataToSave;
	SaveObj->GameData.SaveSlotName = SlotName;
	SaveObj->GameData.SaveTimestamp = FDateTime::Now();

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
	OnLoadComplete.Broadcast(true, QRSave->GameData);
}
