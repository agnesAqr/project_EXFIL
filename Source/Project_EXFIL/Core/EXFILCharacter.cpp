// Copyright Project EXFIL. All Rights Reserved.

#include "EXFILCharacter.h"
#include "InventoryComponent.h"

AEXFILCharacter::AEXFILCharacter()
{
	InventoryComponent = CreateDefaultSubobject<UInventoryComponent>(TEXT("InventoryComponent"));
}
