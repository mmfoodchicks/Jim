#include "QRGameMode.h"
#include "GameFramework/GameStateBase.h"
#include "QRColonyStateComponent.h"
#include "QRResearchComponent.h"
#include "QRSaveGameSystem.h"
#include "QRCharacter.h"
#include "Kismet/GameplayStatics.h"

AQRGameMode::AQRGameMode()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 1.0f;

	DefaultPawnClass = AQRCharacter::StaticClass();

	SaveSystem = CreateDefaultSubobject<UQRSaveGameSystem>(TEXT("SaveSystem"));
}

void AQRGameMode::BeginPlay()
{
	Super::BeginPlay();

	// Colony state and research components live on the GameState actor
	if (AGameStateBase* GS = GetGameState<AGameStateBase>())
	{
		ColonyState = GS->FindComponentByClass<UQRColonyStateComponent>();
		Research    = GS->FindComponentByClass<UQRResearchComponent>();
	}

	// Activate tutorial mission
	ActivateMission(FName("MQ_000"));
}

void AQRGameMode::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	float PrevTime    = WorldTimeSeconds;
	WorldTimeSeconds += DeltaTime;

	float DayProgress = FMath::Fmod(WorldTimeSeconds, DayLengthRealSeconds) / DayLengthRealSeconds;
	bool bWasNight    = bIsNight;
	bIsNight          = DayProgress > 0.5f; // Night is second half of cycle

	// Day rollover
	int32 NewDay = FMath::FloorToInt(WorldTimeSeconds / DayLengthRealSeconds) + 1;
	if (NewDay != DayNumber)
	{
		DayNumber = NewDay;
		OnDayStarted(DayNumber);
	}

	// Night transition
	if (bIsNight != bWasNight)
	{
		if (bIsNight) OnNightStarted();
	}
}

float AQRGameMode::GetDayProgress() const
{
	return FMath::Fmod(WorldTimeSeconds, DayLengthRealSeconds) / DayLengthRealSeconds;
}

void AQRGameMode::CompleteMission(FName MissionId)
{
	if (CompletedMissionIds.Contains(MissionId)) return;

	ActiveMissionIds.Remove(MissionId);
	CompletedMissionIds.Add(MissionId);
	OnMissionCompleted(MissionId);

	// Auto-unlock next mission in sequence (handled in Blueprint subclass for flexibility)
}

void AQRGameMode::ActivateMission(FName MissionId)
{
	if (ActiveMissionIds.Contains(MissionId) || CompletedMissionIds.Contains(MissionId)) return;
	ActiveMissionIds.Add(MissionId);
}

bool AQRGameMode::IsMissionComplete(FName MissionId) const
{
	return CompletedMissionIds.Contains(MissionId);
}

void AQRGameMode::QuickSave()
{
	if (!SaveSystem) return;
	FQRGameSaveData Data;
	Data.WorldSeed        = 0; // Blueprint fills from world generator
	Data.WorldTimeSeconds = WorldTimeSeconds;
	Data.DayNumber        = DayNumber;
	Data.CompletedMissionIds = CompletedMissionIds;
	Data.ActiveMissionIds    = ActiveMissionIds;
	if (ColonyState) Data.ColonyMorale = ColonyState->ColonyMorale;

	SaveSystem->SaveGame(Data, TEXT("QuickSave"), 0);
}

void AQRGameMode::QuickLoad()
{
	if (!SaveSystem) return;
	SaveSystem->LoadGame(TEXT("QuickSave"), 0);
}

void AQRGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);
	// New player joined — in listen-server co-op, sync their initial state
}
