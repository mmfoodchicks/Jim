#pragma once
#include "QRHarvestableBase.h"
#include "QRFlora_BlackstemReed.generated.h"

// PLT_BLACK_REED_001 — Basic fiber/thatch/reed resource
// Priority: High | Biomes: riverbank, marsh edge, ditch
UCLASS(BlueprintType, Blueprintable)
class QUIETRIFTENIGMA_API AQRFlora_BlackstemReed : public AQRHarvestableBase
{
	GENERATED_BODY()
public:
	AQRFlora_BlackstemReed();
};
