#include "QRValidationSubsystem.h"
#include "QRGameplayTags.h"
#include "QRCoreSettings.h"
#include "NativeGameplayTags.h"

void UQRValidationSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

#if !UE_BUILD_SHIPPING
	RunAllValidations();
#endif
}

bool UQRValidationSubsystem::RunAllValidations()
{
	bool bAllPassed = true;
	bAllPassed &= ValidateGameplayTags();
	bAllPassed &= ValidateSettings();

	if (bAllPassed)
	{
		UE_LOG(LogTemp, Log, TEXT("[QRValidation] All startup checks passed."));
	}
	return bAllPassed;
}

bool UQRValidationSubsystem::ValidateGameplayTags() const
{
	bool bPassed = true;

	// Spot-check the most critical tags that systems call by name at runtime.
	// A missing tag causes a fatal-assert in editor when bErrorIfNotFound=true (the UE default).
	const TArray<FName> RequiredTagNames =
	{
		"Status.Injury.Bleeding",
		"Status.Injury.Infection",
		"Status.Injury.Toxin",
		"Status.Need.Starving",
		"Status.Need.Dehydrated",
		"Status.Condition.Hypothermia",
		"Ammo.Dirty",
		"Tool.Knife",
		"Tool.Axe",
		"Weather.AcidRain",
		"Scent.Meat",
	};

	for (const FName& TagName : RequiredTagNames)
	{
		FGameplayTag Tag = FGameplayTag::RequestGameplayTag(TagName, /*bErrorIfNotFound=*/false);
		if (!Tag.IsValid())
		{
			WarnInvalid(TEXT("GameplayTag"), FString::Printf(TEXT("'%s' is not registered — add UE_DEFINE_GAMEPLAY_TAG in QRGameplayTags.cpp"), *TagName.ToString()));
			bPassed = false;
		}
	}

	return bPassed;
}

bool UQRValidationSubsystem::ValidateSettings() const
{
	bool bPassed = true;
	const UQRCoreSettings* S = GetDefault<UQRCoreSettings>();
	if (!S) return true;

	if (S->SimulationTickRate <= 0.0f)
	{
		WarnInvalid(TEXT("QRCoreSettings"), TEXT("SimulationTickRate must be > 0 to avoid divide-by-zero in time conversions."));
		bPassed = false;
	}
	if (S->DayLengthSeconds <= 0.0f)
	{
		WarnInvalid(TEXT("QRCoreSettings"), TEXT("DayLengthSeconds must be > 0 to avoid divide-by-zero in day-progress calculations."));
		bPassed = false;
	}
	if (S->BaseDailyCalorieNeed <= 0.0f)
	{
		WarnInvalid(TEXT("QRCoreSettings"), TEXT("BaseDailyCalorieNeed must be > 0; ConsumeFood normalizes calories against this value."));
		bPassed = false;
	}
	if (S->WeatherEventMinIntervalHours > S->WeatherEventMaxIntervalHours)
	{
		WarnInvalid(TEXT("QRCoreSettings"), TEXT("WeatherEventMinIntervalHours exceeds Max — weather events will never trigger."));
		bPassed = false;
	}
	if (S->MentorshipInheritanceFraction < 0.0f || S->MentorshipInheritanceFraction > 1.0f)
	{
		WarnInvalid(TEXT("QRCoreSettings"), TEXT("MentorshipInheritanceFraction must be in [0..1]."));
		bPassed = false;
	}

	return bPassed;
}

void UQRValidationSubsystem::WarnInvalid(const FString& Context, const FString& Detail)
{
	UE_LOG(LogTemp, Warning, TEXT("[QRValidation][%s] %s"), *Context, *Detail);
}
