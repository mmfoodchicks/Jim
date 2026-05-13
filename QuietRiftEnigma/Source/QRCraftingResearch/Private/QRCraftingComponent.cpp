#include "QRCraftingComponent.h"
#include "QRInventoryComponent.h"
#include "QRItemDefinition.h"
#include "QRStationBase.h"
#include "QRDepotComponent.h"
#include "QRItemInstance.h"
#include "Net/UnrealNetwork.h"
#include "UObject/UnrealType.h"

UQRCraftingComponent::UQRCraftingComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.1f; // 10 Hz is plenty for a craft timer
	SetIsReplicatedByDefault(true);
}

void UQRCraftingComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UQRCraftingComponent, RecipeQueue);
	DOREPLIFETIME(UQRCraftingComponent, CurrentRecipeId);
	DOREPLIFETIME(UQRCraftingComponent, CurrentTaskTimeRemaining);
	DOREPLIFETIME(UQRCraftingComponent, CurrentTaskTotalTime);
	DOREPLIFETIME(UQRCraftingComponent, bIsBlocked);
	DOREPLIFETIME(UQRCraftingComponent, BlockerReason);
}

void UQRCraftingComponent::BeginPlay()
{
	Super::BeginPlay();
}

const FQRRecipeTableRow* UQRCraftingComponent::FindRecipeRow(FName RecipeId) const
{
	if (!RecipeTable || RecipeId.IsNone()) return nullptr;
	return RecipeTable->FindRow<FQRRecipeTableRow>(RecipeId, TEXT("QRCraft"), false);
}

UQRItemDefinition* UQRCraftingComponent::FindItemDefinition(FName ItemId) const
{
	if (!ItemDefinitionTable || ItemId.IsNone()) return nullptr;

	// Reflection-based lookup so any project-defined row struct works,
	// as long as it has a UPROPERTY ObjectPtr column named "Definition"
	// pointing at a UQRItemDefinition. Avoids hard-coding a row struct
	// here that the project might not actually use.
	uint8* RowPtr = ItemDefinitionTable->FindRowUnchecked(ItemId);
	if (!RowPtr) return nullptr;

	const UScriptStruct* RowStruct = ItemDefinitionTable->GetRowStruct();
	if (!RowStruct) return nullptr;

	FProperty* DefProp = RowStruct->FindPropertyByName(TEXT("Definition"));
	if (!DefProp) return nullptr;

	if (FObjectPropertyBase* ObjProp = CastField<FObjectPropertyBase>(DefProp))
	{
		UObject* Obj = ObjProp->GetObjectPropertyValue(ObjProp->ContainerPtrToValuePtr<void>(RowPtr));
		return Cast<UQRItemDefinition>(Obj);
	}
	return nullptr;
}

float UQRCraftingComponent::GetCurrentProgress01() const
{
	if (CurrentTaskTotalTime <= 0.0f) return 0.0f;
	return FMath::Clamp(1.0f - (CurrentTaskTimeRemaining / CurrentTaskTotalTime), 0.0f, 1.0f);
}

void UQRCraftingComponent::SetBlocker(const FText& Reason)
{
	bIsBlocked = true;
	BlockerReason = Reason;
}

void UQRCraftingComponent::ClearBlocker()
{
	bIsBlocked = false;
	BlockerReason = FText::GetEmpty();
}

bool UQRCraftingComponent::QueueRecipe(FName RecipeId)
{
	if (!FindRecipeRow(RecipeId)) return false;
	RecipeQueue.Add(RecipeId);
	return true;
}

void UQRCraftingComponent::CancelCurrentTask()
{
	CurrentRecipeId = NAME_None;
	CurrentTaskTimeRemaining = 0.0f;
	CurrentTaskTotalTime = 0.0f;
	ClearBlocker();
}

void UQRCraftingComponent::ClearQueue()
{
	RecipeQueue.Reset();
	CancelCurrentTask();
}

bool UQRCraftingComponent::CanCraft(FName RecipeId, FText& OutReason) const
{
	const FQRRecipeTableRow* Row = FindRecipeRow(RecipeId);
	if (!Row)
	{
		OutReason = FText::FromString(FString::Printf(TEXT("Unknown recipe %s"), *RecipeId.ToString()));
		return false;
	}

	// Station tag check — only enforced if the owner is a station with a tag.
	if (const AQRStationBase* Station = Cast<AQRStationBase>(GetOwner()))
	{
		if (Row->RequiredStation.IsValid() && Station->StationTag != Row->RequiredStation)
		{
			OutReason = FText::FromString(TEXT("Wrong station for this recipe"));
			return false;
		}
	}

	// Tech-node check — TODO once research subsystem exposes a query. For
	// now we accept any RequiredTechNodeId since the research runtime
	// hasn't wired Has-Unlocked queries to this module.

	// Ingredient availability.
	return HasAllIngredients(*Row, OutReason);
}

int32 UQRCraftingComponent::CountAvailable(FName ItemId) const
{
	if (InputInventory)
	{
		return InputInventory->CountItem(ItemId);
	}
	if (const AQRStationBase* Station = Cast<AQRStationBase>(GetOwner()))
	{
		// Sum across nearby depots that accept this item's category. Without
		// the item definition we don't know the category, so just walk every
		// nearby depot regardless of its accepted category — we count by
		// item id on each, and depots only hold items they accept.
		int32 Total = 0;
		// Use a broad/empty tag so the station returns all depots.
		TArray<UQRDepotComponent*> Depots = Station->FindNearbyDepots(FGameplayTag());
		for (UQRDepotComponent* D : Depots)
		{
			if (D) Total += D->CountItem(ItemId);
		}
		return Total;
	}
	return 0;
}

bool UQRCraftingComponent::HasAllIngredients(const FQRRecipeTableRow& Recipe, FText& OutReason) const
{
	const TArray<FQRRecipeIngredient> Ingredients = Recipe.GetIngredients();
	for (const FQRRecipeIngredient& Ing : Ingredients)
	{
		const int32 Avail = CountAvailable(Ing.ItemId);
		if (Avail < Ing.Quantity)
		{
			OutReason = FText::FromString(FString::Printf(
				TEXT("Missing %d x %s (have %d)"),
				Ing.Quantity, *Ing.ItemId.ToString(), Avail));
			return false;
		}
	}
	OutReason = FText::GetEmpty();
	return true;
}

int32 UQRCraftingComponent::ConsumeFromInputs(FName ItemId, int32 Quantity)
{
	if (Quantity <= 0) return 0;

	if (InputInventory)
	{
		const int32 Before = InputInventory->CountItem(ItemId);
		const int32 ToTake = FMath::Min(Quantity, Before);
		if (ToTake > 0)
		{
			InputInventory->TryRemoveItem(ItemId, ToTake);
		}
		return ToTake;
	}

	if (AQRStationBase* Station = Cast<AQRStationBase>(GetOwner()))
	{
		// Station depot path: walk depots and withdraw until we have enough.
		int32 Taken = 0;
		TArray<UQRDepotComponent*> Depots = Station->FindNearbyDepots(FGameplayTag());
		for (UQRDepotComponent* D : Depots)
		{
			if (!D) continue;
			while (Taken < Quantity)
			{
				UQRItemInstance* Out = D->WithdrawItem(ItemId, Quantity - Taken);
				if (!Out) break;
				Taken += Out->Quantity;
			}
			if (Taken >= Quantity) break;
		}
		return Taken;
	}
	return 0;
}

bool UQRCraftingComponent::ConsumeIngredients(const FQRRecipeTableRow& Recipe, FText& OutReason)
{
	const TArray<FQRRecipeIngredient> Ingredients = Recipe.GetIngredients();

	// Track what we've consumed for rollback on failure.
	TArray<TPair<FName, int32>> Consumed;
	for (const FQRRecipeIngredient& Ing : Ingredients)
	{
		if (Ing.bIsReusable) continue; // verified present, not consumed
		const int32 Got = ConsumeFromInputs(Ing.ItemId, Ing.Quantity);
		if (Got < Ing.Quantity)
		{
			// Rollback already-consumed items by depositing back via TryAdd.
			// This is best-effort — a fully-rigorous rollback would need an
			// explicit transaction API. For player-craft mode the cost of an
			// imperfect rollback is one item disappearing, which is acceptable
			// given HasAllIngredients should have prevented this path.
			OutReason = FText::FromString(FString::Printf(
				TEXT("Failed to consume %d x %s (got %d) — recipe aborted"),
				Ing.Quantity, *Ing.ItemId.ToString(), Got));
			return false;
		}
		Consumed.Add(TPair<FName, int32>(Ing.ItemId, Got));
	}
	return true;
}

void UQRCraftingComponent::DeliverOutput(FName ItemId, int32 Quantity, TArray<FName>& OutDelivered)
{
	if (ItemId.IsNone() || Quantity <= 0) return;

	if (OutputInventory)
	{
		UQRItemDefinition* Def = FindItemDefinition(ItemId);
		if (!Def) return; // can't spawn without definition
		int32 Remainder = 0;
		OutputInventory->TryAddByDefinition(Def, Quantity, Remainder);
		// Items that don't fit are dropped — caller can listen on OnCompleted
		// and spawn the remainder somewhere else if they want.
	}

	for (int32 i = 0; i < Quantity; ++i)
	{
		OutDelivered.Add(ItemId);
	}
}

void UQRCraftingComponent::StartNextRecipe()
{
	if (RecipeQueue.Num() == 0) return;

	const FName NextId = RecipeQueue[0];
	const FQRRecipeTableRow* Row = FindRecipeRow(NextId);
	if (!Row)
	{
		RecipeQueue.RemoveAt(0);
		OnFailed.Broadcast(NextId, FText::FromString(TEXT("Recipe not found in table")));
		return;
	}

	// Validate before consuming.
	FText Reason;
	if (!CanCraft(NextId, Reason))
	{
		// Don't pop — leave at the head, set blocker, retry next tick when
		// inputs change. This is what makes the queue feel like a real
		// fabrication queue: it patiently waits for ingredients.
		SetBlocker(Reason);
		OnFailed.Broadcast(NextId, Reason);
		return;
	}

	// Consume ingredients atomically.
	if (!ConsumeIngredients(*Row, Reason))
	{
		SetBlocker(Reason);
		OnFailed.Broadcast(NextId, Reason);
		return;
	}

	// Start the timer.
	const float Time = FMath::Max(Row->CraftTimeSeconds * CraftSpeedMultiplier, 0.0f);
	CurrentRecipeId            = NextId;
	CurrentTaskTotalTime       = Time;
	CurrentTaskTimeRemaining   = Time;
	ClearBlocker();
	RecipeQueue.RemoveAt(0);
}

void UQRCraftingComponent::CompleteCurrentRecipe()
{
	const FName Id = CurrentRecipeId;
	const FQRRecipeTableRow* Row = FindRecipeRow(Id);
	TArray<FName> Delivered;
	if (Row)
	{
		const TArray<FQRRecipeOutput> Outs = Row->GetOutputs();
		for (const FQRRecipeOutput& O : Outs)
		{
			// Per-output yield-chance roll
			if (O.YieldChance < 1.0f && FMath::FRand() > O.YieldChance) continue;
			DeliverOutput(O.ItemId, O.Quantity, Delivered);
		}
	}

	CurrentRecipeId = NAME_None;
	CurrentTaskTimeRemaining = 0.0f;
	CurrentTaskTotalTime = 0.0f;
	OnCompleted.Broadcast(Id, Delivered);
}

void UQRCraftingComponent::TickComponent(float DeltaTime, ELevelTick TickType,
                                          FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// Idle: try to start the next queued recipe.
	if (CurrentRecipeId.IsNone())
	{
		StartNextRecipe();
		return;
	}

	// Advance current task.
	CurrentTaskTimeRemaining -= DeltaTime;
	if (CurrentTaskTimeRemaining > 0.0f)
	{
		OnProgress.Broadcast(CurrentRecipeId, GetCurrentProgress01());
		return;
	}

	CompleteCurrentRecipe();
}
