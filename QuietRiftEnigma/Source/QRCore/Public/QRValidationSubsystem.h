#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "QRValidationSubsystem.generated.h"

// Runs data-integrity checks on startup (dev/editor builds only).
// Validates that gameplay tags are registered, critical config values are sane,
// and flags common authoring errors so they surface in the log before a play session.
UCLASS()
class QRCORE_API UQRValidationSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	// Returns false if any validation failed — useful for automation tests.
	UFUNCTION(BlueprintCallable, Category = "QR|Validation")
	bool RunAllValidations();

private:
	// Verify that all gameplay tags declared in QRGameplayTags.h are registered
	// (catches typos in the tag string table and missing UE_DEFINE entries).
	bool ValidateGameplayTags() const;

	// Check that UQRCoreSettings values won't cause divide-by-zero or infinite loops.
	bool ValidateSettings() const;

	// Log a uniform warning so entries are easy to grep in the output log.
	static void WarnInvalid(const FString& Context, const FString& Detail);
};
