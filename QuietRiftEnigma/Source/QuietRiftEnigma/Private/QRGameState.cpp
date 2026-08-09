#include "QRGameState.h"
#include "QRColonyStateComponent.h"
#include "QRResearchComponent.h"
#include "QRWeatherComponent.h"

AQRGameState::AQRGameState()
{
	ColonyState = CreateDefaultSubobject<UQRColonyStateComponent>(TEXT("ColonyState"));
	Research    = CreateDefaultSubobject<UQRResearchComponent>(TEXT("Research"));
	Weather     = CreateDefaultSubobject<UQRWeatherComponent>(TEXT("Weather"));

	// Colony + research already declare replicated props; weather state
	// changes reach clients through its own broadcast events.
	ColonyState->SetIsReplicatedByDefault(true);
	Research->SetIsReplicatedByDefault(true);
	Weather->SetIsReplicatedByDefault(true);
}
