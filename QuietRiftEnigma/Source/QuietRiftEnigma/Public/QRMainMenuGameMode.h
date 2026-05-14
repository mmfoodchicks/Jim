#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "QRMainMenuGameMode.generated.h"

class UQRMainMenuWidget;

/**
 * Game mode used by L_MainMenu. Spawns the main menu widget on
 * BeginPlay, sets the player to UI input mode, hides the cursor on
 * deactivation.
 *
 * Separate from AQRGameMode so its BeginPlay doesn't trigger the
 * gameplay-side autoload or activate the tutorial mission.
 */
UCLASS(BlueprintType, Blueprintable)
class QUIETRIFTENIGMA_API AQRMainMenuGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AQRMainMenuGameMode();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "QR|Menu")
	TSubclassOf<UQRMainMenuWidget> MainMenuClass;

	virtual void BeginPlay() override;

protected:
	UPROPERTY()
	TObjectPtr<UQRMainMenuWidget> MainMenu = nullptr;
};
