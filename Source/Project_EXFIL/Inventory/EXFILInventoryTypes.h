// Copyright Project EXFIL. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EXFILInventoryTypes.generated.h"

/**
 * FItemSize — 아이템이 그리드에서 차지하는 크기 (Width × Height)
 */
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

	/** 회전 시 Width/Height 스왑된 사이즈 반환 */
	FItemSize GetRotated() const { return FItemSize(Height, Width); }

	bool operator==(const FItemSize& Other) const
	{
		return Width == Other.Width && Height == Other.Height;
	}
};

/**
 * FInventoryItemInstance — 인벤토리에 존재하는 아이템 런타임 인스턴스
 */
USTRUCT(BlueprintType)
struct PROJECT_EXFIL_API FInventoryItemInstance
{
	GENERATED_BODY()

	/** 고유 인스턴스 ID (같은 종류 아이템도 각각 다른 ID) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FGuid InstanceID;

	/** 아이템 정의 ID (Day 3에서 DataTable RowName으로 연결) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName ItemDataID;

	/** 그리드 내 루트 위치 (좌상단 기준) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FIntPoint RootPosition = FIntPoint::ZeroValue;

	/** 그리드 점유 크기 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FItemSize ItemSize;

	/** 회전 여부 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	bool bIsRotated = false;

	/** 스택 수량 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 StackCount = 1;

	/** 최대 스택 수량 (Day 3에서 DataTable에서 로드) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 MaxStackCount = 1;

	/** 현재 적용 중인 크기 (회전 반영) */
	FItemSize GetEffectiveSize() const
	{
		return bIsRotated ? ItemSize.GetRotated() : ItemSize;
	}

	bool IsValid() const { return InstanceID.IsValid(); }

	bool operator==(const FInventoryItemInstance& Other) const
	{
		return InstanceID == Other.InstanceID;
	}
};

/**
 * FInventorySlot — 그리드의 개별 셀
 */
USTRUCT(BlueprintType)
struct PROJECT_EXFIL_API FInventorySlot
{
	GENERATED_BODY()

	/** 이 슬롯을 점유한 아이템의 InstanceID (Invalid = 빈 슬롯) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FGuid OccupyingItemID;

	/** 이 슬롯이 아이템의 루트(좌상단) 위치인지 */
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
