// Copyright Project EXFIL. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Net/Serialization/FastArraySerializer.h"
#include "EXFILInventoryTypes.generated.h"

USTRUCT(BlueprintType)
struct PROJECT_EXFIL_API FItemSize
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 Width = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 Height = 1;

	FItemSize() = default;
	FItemSize(int32 InWidth, int32 InHeight)
		: Width(InWidth), Height(InHeight) {}

	
	FItemSize GetRotated() const { return FItemSize(Height, Width); }
	bool IsSquare() const { return Width == Height; }

	bool operator==(const FItemSize& Other) const
	{
		return Width == Other.Width && Height == Other.Height;
	}
};

USTRUCT(BlueprintType)
struct PROJECT_EXFIL_API FInventoryItemInstance : public FFastArraySerializerItem
{
	GENERATED_BODY()

	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FGuid InstanceID;

	
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName ItemDataID;

	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FIntPoint RootPosition = FIntPoint::ZeroValue;

	
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FItemSize ItemSize;

	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	bool bIsRotated = false;

	
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 StackCount = 1;

	
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 MaxStackCount = 1;

	
	FItemSize GetEffectiveSize() const
	{
		return (bIsRotated && !ItemSize.IsSquare()) ? ItemSize.GetRotated() : ItemSize;
	}

	bool IsValid() const { return InstanceID.IsValid(); }

	bool operator==(const FInventoryItemInstance& Other) const
	{
		return InstanceID == Other.InstanceID;
	}
};

USTRUCT(BlueprintType)
struct PROJECT_EXFIL_API FInventorySlot
{
	GENERATED_BODY()

	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FGuid OccupyingItemID;

	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	bool bIsRootSlot = false;

	bool IsEmpty() const { return !OccupyingItemID.IsValid(); }

	void Clear()
	{
		OccupyingItemID.Invalidate();
		bIsRootSlot = false;
	}

	void Occupy(const FGuid& ItemID, bool bIsRoot)
	{
		OccupyingItemID = ItemID;
		bIsRootSlot = bIsRoot;
	}
};
