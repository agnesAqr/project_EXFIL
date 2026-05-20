// Copyright Project EXFIL. All Rights Reserved.

#include "CoreMinimal.h"
#include "EXFILInventoryTypes.h"
#include "HAL/PlatformTime.h"
#include "Math/RandomStream.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace UE::ProjectEXFIL::InventoryBenchmark
{
enum class EOccupancyPattern : uint8
{
	Empty,
	Sparse25,
	Fragmented50,
	Dense75,
	NearFull,
	WorstNoFit
};

struct FBenchmarkFixture
{
	int32 GridWidth = 0;
	int32 GridHeight = 0;
	TArray<FInventorySlot> GridSlots;
	TArray<uint16> RowBitmap;
	TArray<FIntPoint> TimedProbePositions;
	TArray<FIntPoint> CorrectnessProbePositions;

	int32 GridPositionToIndex(FIntPoint Position) const
	{
		return Position.Y * GridWidth + Position.X;
	}
};

struct FBenchmarkCase
{
	const TCHAR* PatternName = TEXT("");
	int32 GridWidth = 0;
	int32 GridHeight = 0;
	FItemSize ItemSize;
	EOccupancyPattern Pattern = EOccupancyPattern::Empty;
	int32 Seed = 0;
	int32 FindIterations = 0;
	int32 FreeIterations = 0;
};

struct FMeasuredMs
{
	double BeforeMs = 0.0;
	double AfterMs = 0.0;

	double GetSpeedup() const
	{
		return AfterMs > 0.0 ? BeforeMs / AfterMs : 0.0;
	}
};

static volatile int32 GBenchmarkSink = 0;

constexpr int32 FindBenchmarkIterations = 100000;

const FGuid OccupiedSlotID(0x13572468, 0x24681357, 0xABCDEF01, 0x10203040);

const TCHAR* PatternToString(EOccupancyPattern Pattern)
{
	switch (Pattern)
	{
	case EOccupancyPattern::Empty:
		return TEXT("Empty");
	case EOccupancyPattern::Sparse25:
		return TEXT("Sparse25");
	case EOccupancyPattern::Fragmented50:
		return TEXT("Fragmented50");
	case EOccupancyPattern::Dense75:
		return TEXT("Dense75");
	case EOccupancyPattern::NearFull:
		return TEXT("NearFull");
	case EOccupancyPattern::WorstNoFit:
		return TEXT("WorstNoFit");
	default:
		return TEXT("Unknown");
	}
}

void SetOccupied(FBenchmarkFixture& Fixture, int32 Col, int32 Row, bool bOccupied)
{
	if (Col < 0 || Row < 0 || Col >= Fixture.GridWidth || Row >= Fixture.GridHeight)
	{
		return;
	}

	const int32 Index = Fixture.GridPositionToIndex(FIntPoint(Col, Row));
	if (!Fixture.GridSlots.IsValidIndex(Index) || !Fixture.RowBitmap.IsValidIndex(Row))
	{
		return;
	}

	if (bOccupied)
	{
		Fixture.GridSlots[Index].Occupy(OccupiedSlotID, false);
		Fixture.RowBitmap[Row] |= static_cast<uint16>(1U << Col);
	}
	else
	{
		Fixture.GridSlots[Index].Clear();
		Fixture.RowBitmap[Row] &= static_cast<uint16>(~(1U << Col));
	}
}

float GetRandomOccupancyRate(EOccupancyPattern Pattern)
{
	switch (Pattern)
	{
	case EOccupancyPattern::Sparse25:
		return 0.25f;
	case EOccupancyPattern::Fragmented50:
		return 0.50f;
	case EOccupancyPattern::Dense75:
		return 0.75f;
	default:
		return 0.0f;
	}
}

void PopulateRandom(FBenchmarkFixture& Fixture, EOccupancyPattern Pattern, int32 Seed)
{
	FRandomStream Stream(Seed);
	const float OccupancyRate = GetRandomOccupancyRate(Pattern);
	for (int32 Y = 0; Y < Fixture.GridHeight; ++Y)
	{
		for (int32 X = 0; X < Fixture.GridWidth; ++X)
		{
			SetOccupied(Fixture, X, Y, Stream.FRand() < OccupancyRate);
		}
	}
}

void PopulateNearFull(FBenchmarkFixture& Fixture, FItemSize Size)
{
	for (int32 Y = 0; Y < Fixture.GridHeight; ++Y)
	{
		for (int32 X = 0; X < Fixture.GridWidth; ++X)
		{
			SetOccupied(Fixture, X, Y, true);
		}
	}

	const FIntPoint LastFit(
		Fixture.GridWidth - Size.Width,
		Fixture.GridHeight - Size.Height);
	for (int32 Y = LastFit.Y; Y < LastFit.Y + Size.Height; ++Y)
	{
		for (int32 X = LastFit.X; X < LastFit.X + Size.Width; ++X)
		{
			SetOccupied(Fixture, X, Y, false);
		}
	}
}

void PopulateWorstNoFit(FBenchmarkFixture& Fixture, FItemSize Size)
{
	for (int32 Y = 0; Y <= Fixture.GridHeight - Size.Height; ++Y)
	{
		for (int32 X = 0; X <= Fixture.GridWidth - Size.Width; ++X)
		{
			SetOccupied(Fixture, X + Size.Width - 1, Y + Size.Height - 1, true);
		}
	}
}

void BuildProbePositions(FBenchmarkFixture& Fixture, FItemSize Size)
{
	for (int32 Y = 0; Y <= Fixture.GridHeight - Size.Height; ++Y)
	{
		for (int32 X = 0; X <= Fixture.GridWidth - Size.Width; ++X)
		{
			Fixture.TimedProbePositions.Add(FIntPoint(X, Y));
			Fixture.CorrectnessProbePositions.Add(FIntPoint(X, Y));
		}
	}

	Fixture.CorrectnessProbePositions.Add(FIntPoint(-1, 0));
	Fixture.CorrectnessProbePositions.Add(FIntPoint(0, -1));
	Fixture.CorrectnessProbePositions.Add(FIntPoint(Fixture.GridWidth, 0));
	Fixture.CorrectnessProbePositions.Add(FIntPoint(0, Fixture.GridHeight));
	Fixture.CorrectnessProbePositions.Add(FIntPoint(Fixture.GridWidth - Size.Width + 1, 0));
	Fixture.CorrectnessProbePositions.Add(FIntPoint(0, Fixture.GridHeight - Size.Height + 1));
}

FBenchmarkFixture BuildFixture(const FBenchmarkCase& BenchmarkCase)
{
	FBenchmarkFixture Fixture;
	Fixture.GridWidth = BenchmarkCase.GridWidth;
	Fixture.GridHeight = BenchmarkCase.GridHeight;
	Fixture.GridSlots.SetNum(BenchmarkCase.GridWidth * BenchmarkCase.GridHeight);
	for (FInventorySlot& Slot : Fixture.GridSlots)
	{
		Slot.Clear();
	}
	Fixture.RowBitmap.Init(0, BenchmarkCase.GridHeight);

	switch (BenchmarkCase.Pattern)
	{
	case EOccupancyPattern::Empty:
		break;
	case EOccupancyPattern::Sparse25:
	case EOccupancyPattern::Fragmented50:
	case EOccupancyPattern::Dense75:
		PopulateRandom(Fixture, BenchmarkCase.Pattern, BenchmarkCase.Seed);
		break;
	case EOccupancyPattern::NearFull:
		PopulateNearFull(Fixture, BenchmarkCase.ItemSize);
		break;
	case EOccupancyPattern::WorstNoFit:
		PopulateWorstNoFit(Fixture, BenchmarkCase.ItemSize);
		break;
	default:
		break;
	}

	BuildProbePositions(Fixture, BenchmarkCase.ItemSize);
	return Fixture;
}

// Before source: pre-RowBitmap InventoryComponent implementation, restored from
// commit 57fe371^ where AreSlotsFree scanned GridSlots cell-by-cell.
bool AreSlotsFree_Before(const FBenchmarkFixture& Fixture, FIntPoint Position, FItemSize Size)
{
	if (Position.X < 0 || Position.Y < 0)
	{
		return false;
	}

	if (Position.X + Size.Width > Fixture.GridWidth
		|| Position.Y + Size.Height > Fixture.GridHeight)
	{
		return false;
	}

	for (int32 Y = Position.Y; Y < Position.Y + Size.Height; ++Y)
	{
		for (int32 X = Position.X; X < Position.X + Size.Width; ++X)
		{
			const int32 Index = Fixture.GridPositionToIndex(FIntPoint(X, Y));
			if (!Fixture.GridSlots[Index].IsEmpty())
			{
				return false;
			}
		}
	}

	return true;
}

bool FindFirstAvailableSlot_Before(
	const FBenchmarkFixture& Fixture, FItemSize Size, FIntPoint& OutPosition)
{
	for (int32 Y = 0; Y <= Fixture.GridHeight - Size.Height; ++Y)
	{
		for (int32 X = 0; X <= Fixture.GridWidth - Size.Width; ++X)
		{
			if (AreSlotsFree_Before(Fixture, FIntPoint(X, Y), Size))
			{
				OutPosition = FIntPoint(X, Y);
				return true;
			}
		}
	}

	return false;
}

// After source: current InventoryComponent bitmap path, mirrored from
// AreSlotsFree_Internal without the ignore-instance branch.
bool AreSlotsFree_After(const FBenchmarkFixture& Fixture, FIntPoint Position, FItemSize Size)
{
	if (Size.Width <= 0 || Size.Height <= 0
		|| Position.X < 0 || Position.Y < 0
		|| Position.X + Size.Width > Fixture.GridWidth
		|| Position.Y + Size.Height > Fixture.GridHeight)
	{
		return false;
	}

	const uint16 PlacementMask = static_cast<uint16>(
		((static_cast<uint32>(1) << Size.Width) - 1U) << Position.X);

	for (int32 Y = Position.Y; Y < Position.Y + Size.Height; ++Y)
	{
		if (!Fixture.RowBitmap.IsValidIndex(Y))
		{
			return false;
		}

		if ((Fixture.RowBitmap[Y] & PlacementMask) != 0)
		{
			return false;
		}
	}

	return true;
}

// After source: current InventoryComponent::FindFirstAvailableSlot bitmap path.
bool FindFirstAvailableSlot_After(
	const FBenchmarkFixture& Fixture, FItemSize Size, FIntPoint& OutPosition)
{
	const int32 W = Size.Width;
	const int32 H = Size.Height;
	if (W <= 0 || H <= 0 || W > Fixture.GridWidth || H > Fixture.GridHeight)
	{
		return false;
	}

	const uint16 BaseMask = static_cast<uint16>((static_cast<uint32>(1) << W) - 1U);

	for (int32 Y = 0; Y <= Fixture.GridHeight - H; ++Y)
	{
		uint16 Merged = 0;
		for (int32 DY = 0; DY < H; ++DY)
		{
			Merged |= Fixture.RowBitmap[Y + DY];
		}

		for (int32 X = 0; X <= Fixture.GridWidth - W; ++X)
		{
			if ((Merged & static_cast<uint16>(BaseMask << X)) == 0)
			{
				OutPosition = FIntPoint(X, Y);
				return true;
			}
		}
	}

	return false;
}

double MeasureBestMs(TFunctionRef<int32()> Work, int32 Iterations)
{
	const int32 WarmupIterations = FMath::Min(Iterations / 10, 10000);
	for (int32 i = 0; i < WarmupIterations; ++i)
	{
		GBenchmarkSink += Work();
	}

	double BestMs = TNumericLimits<double>::Max();
	for (int32 Trial = 0; Trial < 3; ++Trial)
	{
		const double StartSeconds = FPlatformTime::Seconds();
		int32 LocalSink = 0;
		for (int32 i = 0; i < Iterations; ++i)
		{
			LocalSink += Work();
		}
		const double ElapsedMs = (FPlatformTime::Seconds() - StartSeconds) * 1000.0;
		GBenchmarkSink += LocalSink;
		BestMs = FMath::Min(BestMs, ElapsedMs);
	}

	return BestMs;
}

int32 ResultToSink(bool bResult, FIntPoint Position)
{
	if (!bResult)
	{
		return 7;
	}

	return ((Position.X + 1) * 17) + ((Position.Y + 1) * 31);
}

FMeasuredMs MeasureFindFirstAvailableSlot(
	const FBenchmarkFixture& Fixture, FItemSize Size, int32 Iterations)
{
	FMeasuredMs Result;
	Result.BeforeMs = MeasureBestMs(
		[&Fixture, Size]()
		{
			FIntPoint Position(INDEX_NONE, INDEX_NONE);
			const bool bFound = FindFirstAvailableSlot_Before(Fixture, Size, Position);
			return ResultToSink(bFound, Position);
		},
		Iterations);

	Result.AfterMs = MeasureBestMs(
		[&Fixture, Size]()
		{
			FIntPoint Position(INDEX_NONE, INDEX_NONE);
			const bool bFound = FindFirstAvailableSlot_After(Fixture, Size, Position);
			return ResultToSink(bFound, Position);
		},
		Iterations);

	return Result;
}

FMeasuredMs MeasureAreSlotsFree(
	const FBenchmarkFixture& Fixture, FItemSize Size, int32 Iterations)
{
	FMeasuredMs Result;
	int32 BeforeProbeIndex = 0;
	Result.BeforeMs = MeasureBestMs(
		[&Fixture, Size, &BeforeProbeIndex]()
		{
			const FIntPoint Position =
				Fixture.TimedProbePositions[BeforeProbeIndex % Fixture.TimedProbePositions.Num()];
			++BeforeProbeIndex;
			return AreSlotsFree_Before(Fixture, Position, Size) ? 1 : 0;
		},
		Iterations);

	int32 AfterProbeIndex = 0;
	Result.AfterMs = MeasureBestMs(
		[&Fixture, Size, &AfterProbeIndex]()
		{
			const FIntPoint Position =
				Fixture.TimedProbePositions[AfterProbeIndex % Fixture.TimedProbePositions.Num()];
			++AfterProbeIndex;
			return AreSlotsFree_After(Fixture, Position, Size) ? 1 : 0;
		},
		Iterations);

	return Result;
}

bool VerifyCorrectness(
	FAutomationTestBase& Test, const FBenchmarkFixture& Fixture, const FBenchmarkCase& BenchmarkCase)
{
	bool bPassed = true;
	for (const FIntPoint& Position : Fixture.CorrectnessProbePositions)
	{
		const bool bBefore = AreSlotsFree_Before(Fixture, Position, BenchmarkCase.ItemSize);
		const bool bAfter = AreSlotsFree_After(Fixture, Position, BenchmarkCase.ItemSize);
		if (bBefore != bAfter)
		{
			Test.AddError(FString::Printf(
				TEXT("AreSlotsFree mismatch. Pattern=%s Grid=%dx%d Size=%dx%d Pos=(%d,%d) Before=%s After=%s"),
				BenchmarkCase.PatternName,
				BenchmarkCase.GridWidth,
				BenchmarkCase.GridHeight,
				BenchmarkCase.ItemSize.Width,
				BenchmarkCase.ItemSize.Height,
				Position.X,
				Position.Y,
				bBefore ? TEXT("true") : TEXT("false"),
				bAfter ? TEXT("true") : TEXT("false")));
			bPassed = false;
		}
	}

	FIntPoint BeforePosition(INDEX_NONE, INDEX_NONE);
	FIntPoint AfterPosition(INDEX_NONE, INDEX_NONE);
	const bool bBeforeFound = FindFirstAvailableSlot_Before(
		Fixture, BenchmarkCase.ItemSize, BeforePosition);
	const bool bAfterFound = FindFirstAvailableSlot_After(
		Fixture, BenchmarkCase.ItemSize, AfterPosition);

	if (bBeforeFound != bAfterFound || (bBeforeFound && BeforePosition != AfterPosition))
	{
		Test.AddError(FString::Printf(
			TEXT("FindFirstAvailableSlot mismatch. Pattern=%s Grid=%dx%d Size=%dx%d Before=%s (%d,%d) After=%s (%d,%d)"),
			BenchmarkCase.PatternName,
			BenchmarkCase.GridWidth,
			BenchmarkCase.GridHeight,
			BenchmarkCase.ItemSize.Width,
			BenchmarkCase.ItemSize.Height,
			bBeforeFound ? TEXT("true") : TEXT("false"),
			BeforePosition.X,
			BeforePosition.Y,
			bAfterFound ? TEXT("true") : TEXT("false"),
			AfterPosition.X,
			AfterPosition.Y));
		bPassed = false;
	}

	return bPassed;
}

int32 GetFreeIterations(bool bMainGrid)
{
	return bMainGrid ? 1000000 : 500000;
}

void AddCasesForGrid(
	TArray<FBenchmarkCase>& Cases,
	int32 GridWidth,
	int32 GridHeight,
	bool bMainGrid,
	int32& SeedCounter)
{
	const FItemSize ItemSizes[] =
	{
		FItemSize(1, 1),
		FItemSize(2, 1),
		FItemSize(2, 3),
		FItemSize(4, 2),
		FItemSize(1, 4)
	};
	const EOccupancyPattern Patterns[] =
	{
		EOccupancyPattern::Empty,
		EOccupancyPattern::Sparse25,
		EOccupancyPattern::Fragmented50,
		EOccupancyPattern::Dense75,
		EOccupancyPattern::NearFull,
		EOccupancyPattern::WorstNoFit
	};

	for (const FItemSize& ItemSize : ItemSizes)
	{
		for (EOccupancyPattern Pattern : Patterns)
		{
			Cases.Add(FBenchmarkCase{
				PatternToString(Pattern),
				GridWidth,
				GridHeight,
				ItemSize,
				Pattern,
				SeedCounter++,
				FindBenchmarkIterations,
				GetFreeIterations(bMainGrid)
			});
		}
	}
}

TArray<FBenchmarkCase> BuildBenchmarkCases()
{
	TArray<FBenchmarkCase> Cases;
	int32 SeedCounter = 3579;
	AddCasesForGrid(Cases, 10, 20, true, SeedCounter);
	AddCasesForGrid(Cases, 10, 10, false, SeedCounter);
	AddCasesForGrid(Cases, 16, 20, false, SeedCounter);
	return Cases;
}
} // namespace UE::ProjectEXFIL::InventoryBenchmark

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FInventoryGridBitmapBenchmarkTest,
	"Project.EXFIL.Inventory.GridBitmapBenchmark",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FInventoryGridBitmapBenchmarkTest::RunTest(const FString& Parameters)
{
	using namespace UE::ProjectEXFIL::InventoryBenchmark;

	const TArray<FBenchmarkCase> Cases = BuildBenchmarkCases();
	AddInfo(FString::Printf(
		TEXT("Running GridBitmapBenchmark. Cases=%d, Sink=%d"),
		Cases.Num(),
		static_cast<int32>(GBenchmarkSink)));

	for (const FBenchmarkCase& BenchmarkCase : Cases)
	{
		const FBenchmarkFixture Fixture = BuildFixture(BenchmarkCase);
		if (Fixture.TimedProbePositions.IsEmpty())
		{
			AddError(FString::Printf(
				TEXT("No valid timed probe positions. Grid=%dx%d Size=%dx%d"),
				BenchmarkCase.GridWidth,
				BenchmarkCase.GridHeight,
				BenchmarkCase.ItemSize.Width,
				BenchmarkCase.ItemSize.Height));
			continue;
		}

		if (!VerifyCorrectness(*this, Fixture, BenchmarkCase))
		{
			continue;
		}

		const FMeasuredMs FindMs = MeasureFindFirstAvailableSlot(
			Fixture, BenchmarkCase.ItemSize, BenchmarkCase.FindIterations);
		AddInfo(FString::Printf(
			TEXT("[GridBitmapBenchmark] Func=FindFirstAvailableSlot Pattern=%s Grid=%dx%d Item=%dx%d Iter=%d Before=%.4fms After=%.4fms Speedup=%.2fx"),
			BenchmarkCase.PatternName,
			BenchmarkCase.GridWidth,
			BenchmarkCase.GridHeight,
			BenchmarkCase.ItemSize.Width,
			BenchmarkCase.ItemSize.Height,
			BenchmarkCase.FindIterations,
			FindMs.BeforeMs,
			FindMs.AfterMs,
			FindMs.GetSpeedup()));

		const FMeasuredMs FreeMs = MeasureAreSlotsFree(
			Fixture, BenchmarkCase.ItemSize, BenchmarkCase.FreeIterations);
		AddInfo(FString::Printf(
			TEXT("[GridBitmapBenchmark] Func=AreSlotsFree Pattern=%s Grid=%dx%d Item=%dx%d Iter=%d Before=%.4fms After=%.4fms Speedup=%.2fx"),
			BenchmarkCase.PatternName,
			BenchmarkCase.GridWidth,
			BenchmarkCase.GridHeight,
			BenchmarkCase.ItemSize.Width,
			BenchmarkCase.ItemSize.Height,
			BenchmarkCase.FreeIterations,
			FreeMs.BeforeMs,
			FreeMs.AfterMs,
			FreeMs.GetSpeedup()));
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
