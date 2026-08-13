#include "QRCraftingComponent.h"
#include "QRInventoryComponent.h"
#include "QRItemDefinition.h"
#include "QRStationBase.h"
#include "QRDepotComponent.h"
#include "QRItemInstance.h"
#include "QRResearchComponent.h"
#include "GameFramework/GameStateBase.h"
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

UQRInventoryComponent* UQRCraftingComponent::GetOwnerInventory() const
{
	if (AActor* O = GetOwner())
	{
		return O->FindComponentByClass<UQRInventoryComponent>();
	}
	return nullptr;
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
	// Refund the in-flight recipe's consumed ingredients — cancelling
	// used to silently destroy them (they were consumed at start).
	if (!CurrentRecipeId.IsNone())
	{
		if (const FQRRecipeTableRow* Row = FindRecipeRow(CurrentRecipeId))
		{
			for (const FQRRecipeIngredient& Ing : Row->GetIngredients())
			{
				if (!Ing.bIsReusable)
				{
					DepositToInputs(Ing.ItemId, Ing.Quantity);
				}
			}
		}
	}
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

	// Tech-node gate. The research component lives on the GameState. If no
	// research component exists (bare dev maps), crafting stays open so test
	// maps don't brick — with one, locked means locked.
	if (!Row->RequiredTechNodeId.IsNone())
	{
		const AGameStateBase* GS = GetWorld() ? GetWorld()->GetGameState() : nullptr;
		const UQRResearchComponent* Research = GS
			? GS->FindComponentByClass<UQRResearchComponent>() : nullptr;
		if (Research && !Research->IsTechUnlocked(Row->RequiredTechNodeId))
		{
			OutReason = FText::FromString(FString::Printf(
				TEXT("Requires research: %s"), *Row->RequiredTechNodeId.ToString()));
			return false;
		}
	}

	// Ingredient availability.
	return HasAllIngredients(*Row, OutReason);
}

int32 UQRCraftingComponent::CountAvailable(FName ItemId) const
{
	// Sum every reachable source: the bound input inventory (the
	// interacting player's pocket), the OWNER's own inventory (bench
	// Storage — where haulers deliver), and station depots. The old
	// InputInventory-only early return meant hauler-delivered stock at
	// the bench was invisible to CanCraft.
	int32 Total = 0;
	if (InputInventory)
	{
		Total += InputInventory->CountItem(ItemId);
	}
	if (UQRInventoryComponent* OwnerInv = GetOwnerInventory())
	{
		if (OwnerInv != InputInventory) Total += OwnerInv->CountItem(ItemId);
	}
	if (const AQRStationBase* Station = Cast<AQRStationBase>(GetOwner()))
	{
		// Broad/empty tag so the station returns all depots; depots only
		// hold items they accept, so counting by id is safe.
		TArray<UQRDepotComponent*> Depots = Station->FindNearbyDepots(FGameplayTag());
		for (UQRDepotComponent* D : Depots)
		{
			if (D) Total += D->CountItem(ItemId);
		}
	}
	return Total;
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

	// Drain sources in the same order CountAvailable sums them:
	// player pocket → owner (bench) storage → station depots.
	int32 Taken = 0;
	auto TakeFromInv = [&](UQRInventoryComponent* Inv)
	{
		if (!Inv || Taken >= Quantity) return;
		const int32 Avail  = Inv->CountItem(ItemId);
		const int32 ToTake = FMath::Min(Quantity - Taken, Avail);
		if (ToTake > 0 && Inv->TryRemoveItem(ItemId, ToTake))
		{
			Taken += ToTake;
		}
	};
	TakeFromInv(InputInventory);
	if (UQRInventoryComponent* OwnerInv = GetOwnerInventory())
	{
		if (OwnerInv != InputInventory) TakeFromInv(OwnerInv);
	}
	if (Taken >= Quantity) return Taken;

	if (AQRStationBase* Station = Cast<AQRStationBase>(GetOwner()))
	{
		// Station depot path: walk depots and withdraw the remainder.
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
	}
	return Taken;
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
			// Roll back: everything consumed so far goes back, including the
			// partial take of THIS ingredient. Without this, a mid-recipe
			// failure (co-op race, depot withdrawn between check and consume)
			// silently ate the earlier ingredients.
			if (Got > 0) DepositToInputs(Ing.ItemId, Got);
			for (const TPair<FName, int32>& Pair : Consumed)
			{
				DepositToInputs(Pair.Key, Pair.Value);
			}
			OutReason = FText::FromString(FString::Printf(
				TEXT("Failed to consume %d x %s (got %d) — recipe aborted, ingredients returned"),
				Ing.Quantity, *Ing.ItemId.ToString(), Got));
			return false;
		}
		Consumed.Add(TPair<FName, int32>(Ing.ItemId, Got));
	}
	return true;
}

void UQRCraftingComponent::DepositToInputs(FName ItemId, int32 Quantity)
{
	if (ItemId.IsNone() || Quantity <= 0) return;

	UQRItemDefinition* Def = FindItemDefinition(ItemId);
	if (!Def)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[QRCraft] Rollback couldn't resolve definition for %d x %s — lost"),
			Quantity, *ItemId.ToString());
		return;
	}

	if (InputInventory)
	{
		int32 Remainder = 0;
		InputInventory->TryAddByDefinition(Def, Quantity, Remainder);
		if (Remainder > 0)
		{
			UE_LOG(LogTemp, Warning,
				TEXT("[QRCraft] Rollback couldn't fit %d x %s back into the input inventory"),
				Remainder, *ItemId.ToString());
		}
		return;
	}

	if (AQRStationBase* Station = Cast<AQRStationBase>(GetOwner()))
	{
		TArray<UQRDepotComponent*> Depots = Station->FindNearbyDepots(FGameplayTag());
		for (UQRDepotComponent* D : Depots)
		{
			if (!D) continue;
			UQRItemInstance* Inst = NewObject<UQRItemInstance>(this);
			Inst->Initialize(Def, Quantity);
			if (D->DepositItem(Inst)) return;
		}
		UE_LOG(LogTemp, Warning,
			TEXT("[QRCraft] Rollback found no depot to take back %d x %s"),
			Quantity, *ItemId.ToString());
	}
}

FName UQRCraftingComponent::FindFirstMissingIngredient(const FQRRecipeTableRow& Recipe, int32& OutMissingQty) const
{
	OutMissingQty = 0;
	const TArray<FQRRecipeIngredient> Ingredients = Recipe.GetIngredients();
	for (const FQRRecipeIngredient& Ing : Ingredients)
	{
		const int32 Avail = CountAvailable(Ing.ItemId);
		if (Avail < Ing.Quantity)
		{
			OutMissingQty = Ing.Quantity - Avail;
			return Ing.ItemId;
		}
	}
	return NAME_None;
}

FName UQRCraftingComponent::GetCurrentDemandItem(int32& OutMissingQty) const
{
	OutMissingQty = 0;
	// Head of the queue is what the station is trying to make next; the
	// in-flight recipe already consumed its inputs.
	const FName HeadId = RecipeQueue.Num() > 0 ? RecipeQueue[0] : NAME_None;
	const FQRRecipeTableRow* Row = FindRecipeRow(HeadId);
	if (!Row) return NAME_None;
	return FindFirstMissingIngredient(*Row, OutMissingQty);
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
