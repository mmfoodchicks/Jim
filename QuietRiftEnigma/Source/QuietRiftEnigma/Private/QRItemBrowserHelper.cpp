#include "QRItemBrowserHelper.h"
#include "QRItemDefinition.h"
#include "Engine/AssetManager.h"

TArray<UQRItemDefinition*> UQRItemBrowserHelper::GetAllItemDefinitions()
{
	TArray<UQRItemDefinition*> Result;
	UAssetManager* AM = UAssetManager::GetIfInitialized();
	if (!AM) return Result;

	TArray<FPrimaryAssetId> Ids;
	AM->GetPrimaryAssetIdList(FPrimaryAssetType(TEXT("QRItem")), Ids);

	Result.Reserve(Ids.Num());
	for (const FPrimaryAssetId& Id : Ids)
	{
		const FSoftObjectPath Path = AM->GetPrimaryAssetPath(Id);
		if (UQRItemDefinition* Def = Cast<UQRItemDefinition>(Path.TryLoad()))
		{
			Result.Add(Def);
		}
	}
	return Result;
}

TArray<UQRItemDefinition*> UQRItemBrowserHelper::SearchItems(
	const TArray<UQRItemDefinition*>& Source, const FString& Filter)
{
	if (Filter.IsEmpty()) return Source;

	TArray<UQRItemDefinition*> Out;
	Out.Reserve(Source.Num());
	for (UQRItemDefinition* Def : Source)
	{
		if (!Def) continue;
		if (Def->DisplayName.ToString().Contains(Filter, ESearchCase::IgnoreCase) ||
			Def->ItemId.ToString().Contains(Filter, ESearchCase::IgnoreCase))
		{
			Out.Add(Def);
		}
	}
	return Out;
}

UQRItemDefinition* UQRItemBrowserHelper::LoadDefinitionById(FName ItemId)
{
	UAssetManager* AM = UAssetManager::GetIfInitialized();
	if (!AM) return nullptr;
	const FPrimaryAssetId Id(TEXT("QRItem"), ItemId);
	const FSoftObjectPath Path = AM->GetPrimaryAssetPath(Id);
	return Cast<UQRItemDefinition>(Path.TryLoad());
}
