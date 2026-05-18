// Copyright Project EXFIL. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Data/EXFILItemTypes.h"

class AActor;
class UDataTable;
class UGameInstance;
class UInventoryComponent;
class UItemDataSubsystem;

class FInventoryTestFixture
{
public:
	FInventoryTestFixture(int32 InGridWidth = 10, int32 InGridHeight = 20);
	~FInventoryTestFixture();

	FInventoryTestFixture(const FInventoryTestFixture&) = delete;
	FInventoryTestFixture& operator=(const FInventoryTestFixture&) = delete;

	void RegisterTestItem(FName ItemDataID, FItemSize Size,
		EItemType Type = EItemType::Material, int32 MaxStackCount = 1);

	UInventoryComponent* GetInventory() const { return Inventory; }

private:
	AActor* OwnerActor = nullptr;
	UGameInstance* TestGameInstance = nullptr;
	UInventoryComponent* Inventory = nullptr;
	UItemDataSubsystem* ItemSub = nullptr;
	UDataTable* TestItemTable = nullptr;
};

#endif // WITH_DEV_AUTOMATION_TESTS
