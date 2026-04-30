#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "QRSaveTypes.h"
#include "QRSaveGameSystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSaveComplete, bool, bSuccess);

// FQRGameSaveData is not BlueprintType (TMap<uint8,...> + nested non-BP structs), so
// FOnLoadComplete is a regular C++ multicast delegate — not BlueprintAssignable.
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnLoadComplete, bool /*bSuccess*/, const FQRGameSaveData& /*SaveData*/);

// UE SaveGame object that wraps the full game state
UCLASS()
class QRSAVENET_API UQRSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FQRGameSaveData GameData;

	UPROPERTY()
	FString SaveVersion = TEXT("1.0.0");
};

// Current save layout version — bump this whenever FQRGameSaveData or its nested structs change.
// MigrateToCurrentVersion() must handle every intermediate step.
static constexpr int32 QRCurrentSaveVersion = 1;

// Singleton-accessible save/load coordinator
UCLASS(BlueprintType, Blueprintable)
class QRSAVENET_API UQRSaveGameSystem : public UObject
{
	GENERATED_BODY()

public:
	// ── Events ───────────────────────────────
	UPROPERTY(BlueprintAssignable, Category = "Save")
	FOnSaveComplete OnSaveComplete;

	// Bound from C++ only — see comment on FOnLoadComplete declaration above.
	FOnLoadComplete OnLoadComplete;

	// ── Interface ────────────────────────────
	// C++ only: FQRGameSaveData isn't a BlueprintType.
	void SaveGame(const FQRGameSaveData& DataToSave, const FString& SlotName, int32 UserIndex = 0);

	UFUNCTION(BlueprintCallable, Category = "Save")
	void LoadGame(const FString& SlotName, int32 UserIndex = 0);

	UFUNCTION(BlueprintCallable, Category = "Save")
	bool DeleteSave(const FString& SlotName, int32 UserIndex = 0);

	UFUNCTION(BlueprintPure, Category = "Save")
	bool DoesSaveExist(const FString& SlotName, int32 UserIndex = 0) const;

	UFUNCTION(BlueprintPure, Category = "Save")
	TArray<FString> GetAllSaveSlots() const;

	// Apply any pending format migrations to Data so it matches QRCurrentSaveVersion.
	// Called automatically on every load; safe to call on already-current saves.
	static void MigrateToCurrentVersion(FQRGameSaveData& Data);

	// ── Async Callbacks ──────────────────────
private:
	void OnSaveToSlotComplete(const FString& SlotName, int32 UserIndex, bool bSuccess);
	void OnLoadFromSlotComplete(const FString& SlotName, int32 UserIndex, USaveGame* SaveGame);
};
