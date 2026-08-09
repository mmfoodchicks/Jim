#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "QRGameState.generated.h"

class UQRColonyStateComponent;
class UQRResearchComponent;
class UQRWeatherComponent;

/**
 * Game state that actually CARRIES the three shared world components
 * everything else looks up on the GameState (the GameMode's tick,
 * crafting's research gate, the camp sim's weather check, the cheat
 * manager). Until this class existed nothing ever created them, so
 * weather, research, and colony aggregation were permanently null.
 *
 * AQRGameMode sets GameStateClass to this in its constructor; a BP
 * subclass can swap in its own GameState, and the GameMode's BeginPlay
 * fallback-creates any of the three that are missing.
 */
UCLASS(BlueprintType, Blueprintable)
class QUIETRIFTENIGMA_API AQRGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	AQRGameState();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "QR")
	TObjectPtr<UQRColonyStateComponent> ColonyState;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "QR")
	TObjectPtr<UQRResearchComponent> Research;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "QR")
	TObjectPtr<UQRWeatherComponent> Weather;
};
