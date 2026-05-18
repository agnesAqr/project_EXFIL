// Copyright Project EXFIL. All Rights Reserved.

#include "InventoryTestFixture.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "CoreMinimal.h"
#include "Data/ItemDataSubsystem.h"
#include "Engine/DataTable.h"
#include "Engine/GameInstance.h"
#include "GameFramework/Actor.h"
#include "Inventory/InventoryComponent.h"
#include "UObject/UObjectGlobals.h"

FInventoryTestFixture::FInventoryTestFixture(int32 InGridWidth, int32 InGridHeight)
{
	OwnerActor = NewObject<AActor>(GetTransientPackage());
	check(OwnerActor);
	check(OwnerActor->HasAuthority());

	Inventory = NewObject<UInventoryComponent>(OwnerActor, TEXT("InventoryComponent"));
	check(Inventory);
	OwnerActor->AddInstanceComponent(Inventory);
	check(Inventory->GetOwner() == OwnerActor);
	Inventory->GridWidth = InGridWidth;
	Inventory->GridHeight = InGridHeight;

	TestItemTable = NewObject<UDataTable>(GetTransientPackage());
	check(TestItemTable);
	TestItemTable->RowStruct = FItemData::StaticStruct();

	TestGameInstance = NewObject<UGameInstance>(GetTransientPackage());
	check(TestGameInstance);

	ItemSub = NewObject<UItemDataSubsystem>(TestGameInstance);
	check(ItemSub);
	ItemSub->SetTablesForTests(TestItemTable, nullptr);

	Inventory->SetCachedItemSubForTests(ItemSub);
	Inventory->BootstrapForTests();
}

FInventoryTestFixture::~FInventoryTestFixture()
{
	if (Inventory)
	{
		if (Inventory->IsRegistered())
		{
			Inventory->UnregisterComponent();
		}
		Inventory = nullptr;
	}

	OwnerActor = nullptr;
	TestGameInstance = nullptr;
	ItemSub = nullptr;
	TestItemTable = nullptr;

	CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
}

void FInventoryTestFixture::RegisterTestItem(FName ItemDataID, FItemSize Size,
	EItemType Type, int32 MaxStackCount)
{
	check(TestItemTable);

	FItemData Row;
	Row.DisplayName = FText::FromName(ItemDataID);
	Row.Description = FText::FromString(TEXT("Inventory unit test item"));
	Row.ItemType = Type;
	Row.SizeWidth = Size.Width;
	Row.SizeHeight = Size.Height;
	Row.MaxStackCount = FMath::Max(1, MaxStackCount);
	TestItemTable->AddRow(ItemDataID, Row);
}

#endif // WITH_DEV_AUTOMATION_TESTS
