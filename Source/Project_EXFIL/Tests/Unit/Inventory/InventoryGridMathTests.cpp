// Copyright Project EXFIL. All Rights Reserved.

#include "InventoryTestFixture.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "CoreMinimal.h"
#include "Inventory/InventoryComponent.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FInventoryGridMathPositionToIndexRoundTripTest,
	"Project.EXFIL.Inventory.Unit.GridMath.PositionToIndexRoundTrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FInventoryGridMathPositionToIndexRoundTripTest::RunTest(const FString& Parameters)
{
	FInventoryTestFixture Fixture(4, 3);
	UInventoryComponent* Inventory = Fixture.GetInventory();
	if (!TestNotNull(TEXT("Inventory"), Inventory))
	{
		return false;
	}

	for (int32 Y = 0; Y < Inventory->GridHeight; ++Y)
	{
		for (int32 X = 0; X < Inventory->GridWidth; ++X)
		{
			const FIntPoint Position(X, Y);
			const int32 Index = Inventory->GridPositionToIndexForTests(Position);
			const FIntPoint RoundTrip = Inventory->IndexToGridPositionForTests(Index);
			TestTrue(
				FString::Printf(TEXT("Round-trip preserves (%d,%d)"), X, Y),
				RoundTrip == Position);
		}
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FInventoryGridMathBoundaryAcceptTest,
	"Project.EXFIL.Inventory.Unit.GridMath.IsValidGridPosition_BoundaryAccept",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FInventoryGridMathBoundaryAcceptTest::RunTest(const FString& Parameters)
{
	FInventoryTestFixture Fixture(4, 3);
	UInventoryComponent* Inventory = Fixture.GetInventory();
	if (!TestNotNull(TEXT("Inventory"), Inventory))
	{
		return false;
	}

	TestTrue(TEXT("(0,0) is valid"),
		Inventory->IsValidGridPositionForTests(FIntPoint(0, 0)));
	TestTrue(TEXT("(GridWidth-1,GridHeight-1) is valid"),
		Inventory->IsValidGridPositionForTests(
			FIntPoint(Inventory->GridWidth - 1, Inventory->GridHeight - 1)));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FInventoryGridMathBoundaryRejectTest,
	"Project.EXFIL.Inventory.Unit.GridMath.IsValidGridPosition_BoundaryReject",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FInventoryGridMathBoundaryRejectTest::RunTest(const FString& Parameters)
{
	FInventoryTestFixture Fixture(4, 3);
	UInventoryComponent* Inventory = Fixture.GetInventory();
	if (!TestNotNull(TEXT("Inventory"), Inventory))
	{
		return false;
	}

	TestFalse(TEXT("Negative X is invalid"),
		Inventory->IsValidGridPositionForTests(FIntPoint(-1, 0)));
	TestFalse(TEXT("X == GridWidth is invalid"),
		Inventory->IsValidGridPositionForTests(FIntPoint(Inventory->GridWidth, 0)));
	TestFalse(TEXT("Y == GridHeight is invalid"),
		Inventory->IsValidGridPositionForTests(FIntPoint(0, Inventory->GridHeight)));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
