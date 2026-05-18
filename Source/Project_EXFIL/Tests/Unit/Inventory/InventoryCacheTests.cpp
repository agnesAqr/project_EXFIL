// Copyright Project EXFIL. All Rights Reserved.

#include "InventoryTestFixture.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "CoreMinimal.h"
#include "Inventory/InventoryComponent.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FInventoryCacheAddIncrementsCountTest,
	"Project.EXFIL.Inventory.Unit.Cache.AddIncrementsCount",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FInventoryCacheAddIncrementsCountTest::RunTest(const FString& Parameters)
{
	static const FName StackableItemID(TEXT("Stackable1x1"));

	FInventoryTestFixture Fixture(4, 4);
	Fixture.RegisterTestItem(StackableItemID, FItemSize(1, 1), EItemType::Material, 10);
	UInventoryComponent* Inventory = Fixture.GetInventory();
	if (!TestNotNull(TEXT("Inventory"), Inventory))
	{
		return false;
	}

	TestTrue(TEXT("Adding stackable item succeeds"),
		Inventory->AddItemByID_Internal(StackableItemID, 3));
	TestEqual(TEXT("Cached count increments"),
		Inventory->GetItemCountByID_Cached(StackableItemID), 3);
	TestEqual(TEXT("Cached count matches scan count"),
		Inventory->GetItemCountByID_Cached(StackableItemID),
		Inventory->GetItemCountByID(StackableItemID));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FInventoryCacheRemoveDecrementsCountAndKeepsIndexMapTest,
	"Project.EXFIL.Inventory.Unit.Cache.RemoveDecrementsCountAndKeepsIndexMap",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FInventoryCacheRemoveDecrementsCountAndKeepsIndexMapTest::RunTest(const FString& Parameters)
{
	static const FName TestItemID(TEXT("Test1x1"));

	FInventoryTestFixture Fixture(4, 4);
	Fixture.RegisterTestItem(TestItemID, FItemSize(1, 1));
	UInventoryComponent* Inventory = Fixture.GetInventory();
	if (!TestNotNull(TEXT("Inventory"), Inventory))
	{
		return false;
	}

	TestTrue(TEXT("First item add succeeds"),
		Inventory->AddItemByIDAt_Internal(TestItemID, FIntPoint(0, 0)));
	TestTrue(TEXT("Second item add succeeds"),
		Inventory->AddItemByIDAt_Internal(TestItemID, FIntPoint(1, 0)));

	const TArray<FInventoryItemInstance> ItemsBeforeRemove = Inventory->GetAllItems();
	TestEqual(TEXT("Two items exist before remove"), ItemsBeforeRemove.Num(), 2);
	if (ItemsBeforeRemove.Num() != 2)
	{
		return false;
	}

	const FGuid RemovedID = ItemsBeforeRemove[0].InstanceID;
	const FGuid RemainingID = ItemsBeforeRemove[1].InstanceID;
	TestTrue(TEXT("Remove succeeds"), Inventory->RemoveItem_Internal(RemovedID));
	TestEqual(TEXT("Cached count decrements"),
		Inventory->GetItemCountByID_Cached(TestItemID), 1);

	FInventoryItemInstance RemainingItem;
	TestTrue(TEXT("Remaining swapped item remains indexed"),
		Inventory->GetItemByID(RemainingID, RemainingItem));
	TestTrue(TEXT("Remaining item ID matches"),
		RemainingItem.InstanceID == RemainingID);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FInventoryCacheStackOverflowCreatesSecondStackTest,
	"Project.EXFIL.Inventory.Unit.Cache.StackOverflowCreatesSecondStack",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FInventoryCacheStackOverflowCreatesSecondStackTest::RunTest(const FString& Parameters)
{
	static const FName StackableItemID(TEXT("Stackable1x1"));

	FInventoryTestFixture Fixture(4, 4);
	Fixture.RegisterTestItem(StackableItemID, FItemSize(1, 1), EItemType::Material, 10);
	UInventoryComponent* Inventory = Fixture.GetInventory();
	if (!TestNotNull(TEXT("Inventory"), Inventory))
	{
		return false;
	}

	TestTrue(TEXT("Adding overflow stack succeeds"),
		Inventory->AddItemByID_Internal(StackableItemID, 15));

	const TArray<FInventoryItemInstance> Items = Inventory->GetAllItems();
	TestEqual(TEXT("Overflow creates two stacks"), Items.Num(), 2);
	TestEqual(TEXT("Cached count includes both stacks"),
		Inventory->GetItemCountByID_Cached(StackableItemID), 15);

	TArray<int32> StackCounts;
	for (const FInventoryItemInstance& Item : Items)
	{
		StackCounts.Add(Item.StackCount);
	}
	StackCounts.Sort();

	if (StackCounts.Num() == 2)
	{
		TestEqual(TEXT("Overflow stack remainder is 5"), StackCounts[0], 5);
		TestEqual(TEXT("Full stack is 10"), StackCounts[1], 10);
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
