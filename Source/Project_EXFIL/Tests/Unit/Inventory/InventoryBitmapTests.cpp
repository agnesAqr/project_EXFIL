// Copyright Project EXFIL. All Rights Reserved.

#include "InventoryTestFixture.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "CoreMinimal.h"
#include "Inventory/InventoryComponent.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FInventoryBitmapEmptyGridAlwaysFreeTest,
	"Project.EXFIL.Inventory.Unit.Bitmap.EmptyGridAlwaysFree",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FInventoryBitmapEmptyGridAlwaysFreeTest::RunTest(const FString& Parameters)
{
	FInventoryTestFixture Fixture(4, 4);
	UInventoryComponent* Inventory = Fixture.GetInventory();
	if (!TestNotNull(TEXT("Inventory"), Inventory))
	{
		return false;
	}

	TestTrue(TEXT("1x1 fits at origin"),
		Inventory->CanPlaceItemAt(FIntPoint(0, 0), FItemSize(1, 1)));
	TestTrue(TEXT("2x2 fits in empty grid"),
		Inventory->CanPlaceItemAt(FIntPoint(2, 2), FItemSize(2, 2)));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FInventoryBitmapOccupiedSlotsBlockOverlapTest,
	"Project.EXFIL.Inventory.Unit.Bitmap.OccupiedSlotsBlockOverlap",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FInventoryBitmapOccupiedSlotsBlockOverlapTest::RunTest(const FString& Parameters)
{
	static const FName TestItemID(TEXT("Test1x1"));

	FInventoryTestFixture Fixture(4, 4);
	Fixture.RegisterTestItem(TestItemID, FItemSize(1, 1));
	UInventoryComponent* Inventory = Fixture.GetInventory();
	if (!TestNotNull(TEXT("Inventory"), Inventory))
	{
		return false;
	}

	TestTrue(TEXT("Initial add succeeds"),
		Inventory->AddItemByIDAt_Internal(TestItemID, FIntPoint(1, 1)));
	TestFalse(TEXT("Occupied slot blocks overlap"),
		Inventory->CanPlaceItemAt(FIntPoint(1, 1), FItemSize(1, 1)));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
