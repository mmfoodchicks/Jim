#include "QRGameState.h"
#include "QRColonyStateComponent.h"
#include "QRResearchComponent.h"
#include "QRWeatherComponent.h"

AQRGameState::AQRGameState()
{
	ColonyState = CreateDefaultSubobject<UQRColonyStateComponent>(TEXT("ColonyState"));
	Research    = CreateDefaultSubobject<UQRResearchComponent>(TEXT("Research"));
	Weather     = CreateDefaultSubobject<UQRWeatherComponent>(TEXT("Weather"));

	// Replication note: SetIsReplicatedByDefault is protected (component-
	// internal), so per-component replication is each class's own call in
	// its constructor. Colony + research already declare replicated props;
	// weather reaches clients through its broadcast events. Single-player
	// behavior is unaffected either way.
}
