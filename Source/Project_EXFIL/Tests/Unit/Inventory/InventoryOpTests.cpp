// Copyright Project EXFIL. All Rights Reserved.

#include "InventoryTestFixture.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "CoreMinimal.h"
#include "Inventory/InventoryComponent.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FInventoryOpAddThenRemoveLeavesEmptyTest,
	"Project.EXFIL.Inventory.Unit.Op.AddThenRemove_LeavesEmpty",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FInventoryOpAddThenRemoveLeavesEmptyTest::RunTest(const FString& Parameters)
{
	static const FName TestItemID(TEXT("Test1x1"));

	FInventoryTestFixture Fixture(4, 4);
	Fixture.RegisterTestItem(TestItemID, FItemSize(1, 1));
	UInventoryComponent* Inventory = Fixture.GetInventory();
	if (!TestNotNull(TEXT("Inventory"), Inventory))
	{
		return false;
	}

	TestTrue(TEXT("Add succeeds"), Inventory->AddItemByID_Internal(TestItemID));
	const TArray<FInventoryItemInstance> Items = Inventory->GetAllItems();
	TestEqual(TEXT("One item was added"), Items.Num(), 1);
	if (Items.Num() != 1)
	{
		return false;
	}

	TestTrue(TEXT("Remove succeeds"),
		Inventory->RemoveItem_Internal(Items[0].InstanceID));
	TestTrue(TEXT("Inventory is empty"), Inventory->IsEmpty());
	TestTrue(TEXT("RowBitmap is clear"), Inventory->IsRowBitmapClearForTests());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FInventoryOpAddItemAtBlockedReturnsFalseTest,
	"Project.EXFIL.Inventory.Unit.Op.AddItemAtBlocked_ReturnsFalse",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FInventoryOpAddItemAtBlockedReturnsFalseTest::RunTest(const FString& Parameters)
{
	static const FName TestItemID(TEXT("Test1x1"));

	FInventoryTestFixture Fixture(4, 4);
	Fixture.RegisterTestItem(TestItemID, FItemSize(1, 1));
	UInventoryComponent* Inventory = Fixture.GetInventory();
	if (!TestNotNull(TEXT("Inventory"), Inventory))
	{
		return false;
	}

	TestTrue(TEXT("First add succeeds"),
		Inventory->AddItemByIDAt_Internal(TestItemID, FIntPoint(0, 0)));
	AddExpectedErrorPlain(
		TEXT("AddItemAt_Internal: Cannot place item 'Test1x1'"),
		EAutomationExpectedErrorFlags::Contains);
	TestFalse(TEXT("Second add at occupied position fails"),
		Inventory->AddItemByIDAt_Internal(TestItemID, FIntPoint(0, 0)));
	TestEqual(TEXT("Only one item exists"), Inventory->GetAllItems().Num(), 1);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FInventoryOpMoveItemUpdatesPositionTest,
	"Project.EXFIL.Inventory.Unit.Op.MoveItem_UpdatesPosition",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FInventoryOpMoveItemUpdatesPositionTest::RunTest(const FString& Parameters)
{
	static const FName TestItemID(TEXT("Test1x1"));

	FInventoryTestFixture Fixture(5, 5);
	Fixture.RegisterTestItem(TestItemID, FItemSize(1, 1));
	UInventoryComponent* Inventory = Fixture.GetInventory();
	if (!TestNotNull(TEXT("Inventory"), Inventory))
	{
		return false;
	}

	TestTrue(TEXT("Add succeeds"),
		Inventory->AddItemByIDAt_Internal(TestItemID, FIntPoint(0, 0)));
	const TArray<FInventoryItemInstance> Items = Inventory->GetAllItems();
	TestEqual(TEXT("One item was added"), Items.Num(), 1);
	if (Items.Num() != 1)
	{
		return false;
	}

	const FGuid ItemID = Items[0].InstanceID;
	TestTrue(TEXT("Move succeeds"),
		Inventory->MoveItem_Internal(ItemID, FIntPoint(3, 3)));
	TestTrue(TEXT("Old position is free"),
		Inventory->CanPlaceItemAt(FIntPoint(0, 0), FItemSize(1, 1)));
	TestFalse(TEXT("New position is occupied"),
		Inventory->CanPlaceItemAt(FIntPoint(3, 3), FItemSize(1, 1)));

	FInventoryItemInstance MovedItem;
	TestTrue(TEXT("Moved item remains indexed"),
		Inventory->GetItemByID(ItemID, MovedItem));
	TestTrue(TEXT("Moved item root updates"),
		MovedItem.RootPosition == FIntPoint(3, 3));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FInventoryOpAddAutoRotatesWhenOnlyRotatedFitsTest,
	"Project.EXFIL.Inventory.Unit.Op.AddAutoRotatesWhenOnlyRotatedFits",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FInventoryOpAddAutoRotatesWhenOnlyRotatedFitsTest::RunTest(const FString& Parameters)
{
	static const FName BlockerItemID(TEXT("Test1x1"));
	static const FName WideItemID(TEXT("Wide2x1"));

	FInventoryTestFixture Fixture(2, 3);
	Fixture.RegisterTestItem(BlockerItemID, FItemSize(1, 1));
	Fixture.RegisterTestItem(WideItemID, FItemSize(2, 1));
	UInventoryComponent* Inventory = Fixture.GetInventory();
	if (!TestNotNull(TEXT("Inventory"), Inventory))
	{
		return false;
	}

	TestTrue(TEXT("Blocker at (0,0) succeeds"),
		Inventory->AddItemByIDAt_Internal(BlockerItemID, FIntPoint(0, 0)));
	TestTrue(TEXT("Blocker at (1,1) succeeds"),
		Inventory->AddItemByIDAt_Internal(BlockerItemID, FIntPoint(1, 1)));
	TestTrue(TEXT("Blocker at (1,2) succeeds"),
		Inventory->AddItemByIDAt_Internal(BlockerItemID, FIntPoint(1, 2)));

	TestTrue(TEXT("Wide item auto-rotates into only fitting vertical slot"),
		Inventory->AddItemByID_Internal(WideItemID));

	bool bFoundWideItem = false;
	for (const FInventoryItemInstance& Item : Inventory->GetAllItems())
	{
		if (Item.ItemDataID != WideItemID)
		{
			continue;
		}

		bFoundWideItem = true;
		TestTrue(TEXT("Wide item is rotated"), Item.bIsRotated);
		TestTrue(TEXT("Wide item root is the vertical opening"),
			Item.RootPosition == FIntPoint(0, 1));
		TestTrue(TEXT("Wide item effective size is 1x2"),
			Item.GetEffectiveSize() == FItemSize(1, 2));
	}

	TestTrue(TEXT("Wide item exists"), bFoundWideItem);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
