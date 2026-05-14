#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "QRItemBrowserHelper.generated.h"

class UQRItemDefinition;

/**
 * Helpers for the creative-mode item browser widget.
 *
 * Setup: Project Settings → Asset Manager → "Primary Asset Types To Scan"
 * must contain an entry for "QRItem" mapped to UQRItemDefinition with a
 * scan path that covers wherever the definition data assets live (e.g.
 * /Game/Data/Items). UQRItemDefinition's GetPrimaryAssetId returns
 * FPrimaryAssetId("QRItem", ItemId) so the manager keys them correctly.
 *
 * GetAllItemDefinitions performs a synchronous load of every entry. The
 * creative browser is opened once on Tab and the result can be cached on
 * the widget — repeated SearchItems calls just filter the cached list.
 */
UCLASS()
class QUIETRIFTENIGMA_API UQRItemBrowserHelper : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// Load and return every UQRItemDefinition known to the asset manager.
	// Synchronous load — call once when the browser opens, then filter in
	// memory via SearchItems.
	UFUNCTION(BlueprintCallable, Category = "QR|Item Browser")
	static TArray<UQRItemDefinition*> GetAllItemDefinitions();

	// Filter a list by a query string. Match is case-insensitive substring
	// against DisplayName (preferred) and ItemId (fallback). Empty filter
	// returns the input list unmodified.
	UFUNCTION(BlueprintCallable, Category = "QR|Item Browser")
	static TArray<UQRItemDefinition*> SearchItems(const TArray<UQRItemDefinition*>& Source,
		const FString& Filter);

	// Convenience: load a single definition by ItemId.
	UFUNCTION(BlueprintCallable, Category = "QR|Item Browser")
	static UQRItemDefinition* LoadDefinitionById(FName ItemId);
};
