// LandmassGenerator.cpp
// Implementation of landmass texture generation using FBM noise
// Version: 01.26.2026.23.36

#include "LandmassGenerator.h"
#include "Engine/Texture2D.h"
#include "Async/ParallelFor.h"  // ParallelFor for multi-threaded pixel processing
#include <algorithm> // std::nth_element

ULandmassGenerator::ULandmassGenerator()
{
	PreviewTexture = nullptr;
}

//------------------------------------------------------------------------------
// Initialize: Store settings and prepare the random stream for deterministic generation.
// The seed ensures reproducible results - same seed = same landmass shape.
//------------------------------------------------------------------------------
void ULandmassGenerator::Initialize(const FLandmassSettings& InSettings)
{
	Settings = InSettings;

	// Auto-seed if Seed == 0: generate unpredictable seed from platform time
	// Store back into Settings so GetSeed() returns the actual seed used
	if (Settings.Seed == 0)
	{
		Settings.Seed = static_cast<int32>(FPlatformTime::Cycles() & 0x7FFFFFFF);
		if (Settings.Seed == 0) Settings.Seed = 1; // Ensure non-zero after auto-seed
	}

	// The landscape is generated as a unified noise field.
	// Land coverage and ocean level determine whether the result
	// looks like a single island or an archipelago.

	// Compute minimum edge padding in normalised 0-1 space from world-metres setting.
	// WorldSize is in cm; convert MinEdgeMarginMeters to cm then normalise.
	const float WorldSizeCm = Settings.GetWorldSizeCm();
	MinEdgePaddingNorm = (Settings.MinEdgeMarginMeters * 100.0f) / WorldSizeCm;
	MinEdgePaddingNorm = FMath::Clamp(MinEdgePaddingNorm, 0.0f, 0.45f);

	// Initialize random stream with seed for deterministic noise generation
	RandomStream.Initialize(Settings.Seed);

	// === Generate seed-derived noise offsets ===
	// Each noise layer gets a unique 2D offset so layers are decorrelated.
	// Range [-500, 500] provides sufficient spatial separation in noise space.
	// These replace the hard-coded +50/+100/+200 offsets for better variety per seed.
	
	ShapeNoiseOffset = FVector2D(
		RandomStream.FRandRange(-500.0f, 500.0f),
		RandomStream.FRandRange(-500.0f, 500.0f)
	);
	
	DetailNoiseOffset = FVector2D(
		RandomStream.FRandRange(-500.0f, 500.0f),
		RandomStream.FRandRange(-500.0f, 500.0f)
	);
	
	EdgeNoiseOffset = FVector2D(
		RandomStream.FRandRange(-500.0f, 500.0f),
		RandomStream.FRandRange(-500.0f, 500.0f)
	);

	// === Generate domain warp offsets ===
	// Two separate 2D offsets ensure X and Y warp components are decorrelated.
	// This prevents the warp field from producing diagonal-biased distortions.
	
	WarpOffsetX = FVector2D(
		RandomStream.FRandRange(-500.0f, 500.0f),
		RandomStream.FRandRange(-500.0f, 500.0f)
	);
	
	WarpOffsetY = FVector2D(
		RandomStream.FRandRange(-500.0f, 500.0f),
		RandomStream.FRandRange(-500.0f, 500.0f)
	);

	// === Generate island peaks for multi-island archipelago patterns ===
	// Instead of a single centre bias, we scatter 4-6 peaks across the map.
	// Peak 0 is a large "main island" near the centre; subsequent peaks are
	// smaller satellites at random positions within the circular land zone.
	// This naturally produces archipelago-like layouts with distinct islands.
	
	IslandPeaks.Reset();
	const float LandRadiusNorm = 0.5f - MinEdgePaddingNorm;
	const int32 NumPeaks = RandomStream.RandRange(4, 6);
	
	for (int32 i = 0; i < NumPeaks; ++i)
	{
		FIslandPeak Peak;
		
		if (i == 0)
		{
			// Main island: large, near centre (slight random offset)
			Peak.Position = FVector2D(
				0.5f + RandomStream.FRandRange(-0.08f, 0.08f),
				0.5f + RandomStream.FRandRange(-0.08f, 0.08f)
			);
			Peak.Strength = RandomStream.FRandRange(0.28f, 0.35f);
			Peak.Radius = RandomStream.FRandRange(0.22f, 0.30f);
		}
		else
		{
			// Satellite islands: smaller, scattered across the land zone
			// Use polar coordinates from centre for good distribution
			float Angle = RandomStream.FRandRange(0.0f, 2.0f * PI);
			float Dist = RandomStream.FRandRange(0.12f, LandRadiusNorm * 0.85f);
			Peak.Position = FVector2D(
				0.5f + FMath::Cos(Angle) * Dist,
				0.5f + FMath::Sin(Angle) * Dist
			);
			Peak.Strength = RandomStream.FRandRange(0.15f, 0.25f);
			Peak.Radius = RandomStream.FRandRange(0.10f, 0.18f);
		}
		
		IslandPeaks.Add(Peak);
	}

	UE_LOG(LogTemp, Log, TEXT("LandmassGenerator initialized - Resolution: %d, Seed: %d, LandCoverage: %.1f%%, MapSize: %d, MaxHeight: %.0f, DomainWarp: %s"),
		Settings.TextureResolution, Settings.Seed, Settings.LandCoveragePercent,
		static_cast<int32>(Settings.MapSize), Settings.MaxMapHeight,
		Settings.bEnableDomainWarp ? TEXT("ON") : TEXT("OFF"));
}

//------------------------------------------------------------------------------
// Generate: Main entry point - runs the complete landmass generation pipeline.
// Returns true if at least some land was generated.
//------------------------------------------------------------------------------
bool ULandmassGenerator::Generate()
{
	UE_LOG(LogTemp, Log, TEXT("LandmassGenerator::Generate - Generating land mask"));

	// Step 1: Create the binary land/ocean mask from noise
	GenerateLandMask();
	
	// Step 2: Convert mask to a visual texture for preview/debugging
	GeneratePreviewTexture();

	return LandMask.Num() > 0;
}

//------------------------------------------------------------------------------
// GenerateLandMask: Creates the binary land/ocean classification.
// 
// Two-pass algorithm:
//   Pass 1: Compute GetIslandMask() for every pixel (continuous 0-1 values)
//   Pass 2: Sort values and find threshold that achieves exact land coverage %
//           Then apply threshold to create binary mask
// 
// This percentile-based thresholding guarantees the requested land coverage
// regardless of how the noise values are distributed.
//------------------------------------------------------------------------------
void ULandmassGenerator::GenerateLandMask()
{
	const int32 Width = Settings.TextureResolution;
	const int32 Height = Settings.TextureResolution;
	const int32 TotalPixels = Width * Height;

	// Clear any previous generation data
	LandMask.Empty();
	LandMask.SetNum(TotalPixels);
	LandPixels.Empty();

	// ===== PASS 1: Compute island mask value for every pixel =====
	// These are continuous values from 0.0 (definitely ocean) to 1.0 (definitely land)
	// PARALLELIZED: Each pixel is independent; MaskValues[Index] written by one thread only.
	TArray<float> MaskValues;
	MaskValues.SetNum(TotalPixels);

	// Use ParallelFor for large resolutions (>=1024²) to distribute work across cores.
	// GetIslandMask() is the expensive call (multiple FBM evaluations per pixel).
	ParallelFor(TotalPixels, [&](int32 Index)
	{
		int32 X = Index % Width;
		int32 Y = Index / Width;
		
		// Normalize pixel coordinates to 0-1 range for noise sampling
		// Using (Width-1) so edges are symmetric: X=0 -> 0.0, X=Width-1 -> 1.0
		float NormX = (Width > 1) ? static_cast<float>(X) / static_cast<float>(Width - 1) : 0.5f;
		float NormY = (Height > 1) ? static_cast<float>(Y) / static_cast<float>(Height - 1) : 0.5f;

		// Compute island mask for every pixel — a unified noise landscape
		MaskValues[Index] = GetIslandMask(NormX, NormY);
	});

	// ===== Calculate adaptive threshold for exact land coverage =====
	// Using std::nth_element (introselect) for O(n) average complexity instead of O(n log n) sort.
	// IMPORTANT: nth_element permutes the array, so we must work on a COPY.
	// MaskValues must remain pixel-aligned for Pass 2 threshold comparison.
	
	// Calculate target land pixel count (clamped to valid range)
	float TargetLandPercent = FMath::Clamp(Settings.LandCoveragePercent, 5.0f, 80.0f) / 100.0f;

	int32 TargetLandPixels = FMath::RoundToInt(TotalPixels * TargetLandPercent);
	TargetLandPixels = FMath::Clamp(TargetLandPixels, 1, TotalPixels - 1);

	// nth_element partitions so that element at index K is in sorted position.
	// For K-th largest, we use index (TotalPixels - TargetLandPixels).
	int32 ThresholdIndex = FMath::Clamp(TotalPixels - TargetLandPixels, 0, TotalPixels - 1);
	
	// Copy to scratch array—nth_element will permute this, not the original
	TArray<float> ScratchValues = MaskValues;
	std::nth_element(ScratchValues.GetData(), ScratchValues.GetData() + ThresholdIndex, ScratchValues.GetData() + ScratchValues.Num());
	float LandThreshold = ScratchValues[ThresholdIndex];

	UE_LOG(LogTemp, Log, TEXT("Land coverage target: %.1f%% (%d pixels), threshold: %.3f"),
		Settings.LandCoveragePercent, TargetLandPixels, LandThreshold);

	// ===== PASS 2: Apply threshold to create EXACT land coverage =====
	// Problem: using >= threshold may overshoot if many pixels equal threshold.
	// Solution: 
	//   1. First mark all pixels strictly GREATER than threshold as land (parallelized).
	//   2. Collect pixels exactly EQUAL to threshold (sequential for determinism).
	//   3. Add only as many equal-pixels as needed to reach TargetLandPixels.
	//   4. Build LandPixels by scanning LandMask (sequential, avoids thread-local merge).
	
	// Pass 2a: PARALLELIZED classification into LandMask
	// Each thread writes to a unique LandMask[Index], no data race.
	// We use a simple encoding: 0=ocean, 1=land(>threshold), 2=equal(==threshold)
	ParallelFor(TotalPixels, [&](int32 Index)
	{
		float Value = MaskValues[Index];
		if (Value > LandThreshold)
		{
			LandMask[Index] = 1; // Definitely land
		}
		else if (Value == LandThreshold)
		{
			LandMask[Index] = 2; // Equal to threshold (tentative, may become land or ocean)
		}
		else
		{
			LandMask[Index] = 0; // Definitely ocean
		}
	});
	
	// Pass 2b: SEQUENTIAL scan to count and collect equal-threshold pixels
	// This must be sequential to ensure deterministic index order for tie-breaking.
	TArray<int32> EqualThresholdIndices;
	int32 StrictlyGreaterCount = 0;
	
	for (int32 Index = 0; Index < TotalPixels; ++Index)
	{
		if (LandMask[Index] == 1)
		{
			++StrictlyGreaterCount;
		}
		else if (LandMask[Index] == 2)
		{
			EqualThresholdIndices.Add(Index);
			LandMask[Index] = 0; // Tentatively ocean; may promote below
		}
	}
	
	// Pass 2c: Promote exactly as many equal-threshold pixels as needed
	int32 NeededFromEqual = TargetLandPixels - StrictlyGreaterCount;
	NeededFromEqual = FMath::Clamp(NeededFromEqual, 0, EqualThresholdIndices.Num());
	
	for (int32 i = 0; i < NeededFromEqual; ++i)
	{
		// Deterministic: use stable index order (no shuffle needed for reproducibility)
		LandMask[EqualThresholdIndices[i]] = 1;
	}
	
	// Pass 2d: Build LandPixels array by SEQUENTIAL scan of final LandMask
	// This avoids thread-local buffers and merge complexity while being fast (simple scan).
	for (int32 Y = 0; Y < Height; ++Y)
	{
		for (int32 X = 0; X < Width; ++X)
		{
			int32 Index = Y * Width + X;
			if (LandMask[Index] != 0)
			{
				LandPixels.Add(FIntPoint(X, Y));
			}
		}
	}

	// ===== PASS 3 (Optional): Keep only the largest connected landmass =====
	// Uses 4-way adjacency (up/down/left/right) for connectivity.
	// 4-way chosen over 8-way for stricter landmass separation (diagonal pixels are not connected).
	// Tie-break rule: if two components have equal size, keep the one with the lowest starting index.
	
	if (Settings.bKeepOnlyLargestLandmass && LandPixels.Num() > 0)
	{
		// Visited array: 0 = unvisited, 1 = visited
		TArray<uint8> Visited;
		Visited.SetNumZeroed(TotalPixels);
		
		// Track largest component
		int32 LargestComponentStartIndex = -1;
		int32 LargestComponentSize = 0;
		
		// BFS queue for flood-fill
		TArray<int32> Queue;
		Queue.Reserve(TotalPixels / 4); // Reasonable pre-allocation
		
		// 4-way adjacency offsets: right, left, down, up
		const int32 DX[4] = { 1, -1, 0, 0 };
		const int32 DY[4] = { 0, 0, 1, -1 };
		
		// Scan all pixels in index order (ensures deterministic tie-break: lowest index wins)
		for (int32 StartIndex = 0; StartIndex < TotalPixels; ++StartIndex)
		{
			// Skip if not land or already visited
			if (LandMask[StartIndex] == 0 || Visited[StartIndex] != 0)
			{
				continue;
			}
			
			// BFS flood-fill to find this connected component
			Queue.Reset();
			Queue.Add(StartIndex);
			Visited[StartIndex] = 1;
			int32 ComponentSize = 0;
			
			while (Queue.Num() > 0)
			{
				int32 CurrentIndex = Queue.Pop(EAllowShrinking::No); // Pop from end (LIFO for cache locality)
				++ComponentSize;
				
				int32 CX = CurrentIndex % Width;
				int32 CY = CurrentIndex / Width;
				
				// Check 4-way neighbors
				for (int32 Dir = 0; Dir < 4; ++Dir)
				{
					int32 NX = CX + DX[Dir];
					int32 NY = CY + DY[Dir];
					
					// Bounds check
					if (NX < 0 || NX >= Width || NY < 0 || NY >= Height)
					{
						continue;
					}
					
					int32 NeighborIndex = NY * Width + NX;
					
					// Skip if not land or already visited
					if (LandMask[NeighborIndex] == 0 || Visited[NeighborIndex] != 0)
					{
						continue;
					}
					
					Visited[NeighborIndex] = 1;
					Queue.Add(NeighborIndex);
				}
			}
			
			// Update largest component (tie-break: first encountered wins due to index order scan)
			if (ComponentSize > LargestComponentSize)
			{
				LargestComponentSize = ComponentSize;
				LargestComponentStartIndex = StartIndex;
			}
		}
		
		// If we found a largest component, re-flood to mark it and clear everything else
		if (LargestComponentStartIndex >= 0)
		{
			// Reset visited for second pass
			FMemory::Memzero(Visited.GetData(), TotalPixels);
			
			// Flood-fill again from the largest component's start
			Queue.Reset();
			Queue.Add(LargestComponentStartIndex);
			Visited[LargestComponentStartIndex] = 1;
			
			while (Queue.Num() > 0)
			{
				int32 CurrentIndex = Queue.Pop(EAllowShrinking::No);
				
				int32 CX = CurrentIndex % Width;
				int32 CY = CurrentIndex / Width;
				
				for (int32 Dir = 0; Dir < 4; ++Dir)
				{
					int32 NX = CX + DX[Dir];
					int32 NY = CY + DY[Dir];
					
					if (NX < 0 || NX >= Width || NY < 0 || NY >= Height)
					{
						continue;
					}
					
					int32 NeighborIndex = NY * Width + NX;
					
					if (LandMask[NeighborIndex] == 0 || Visited[NeighborIndex] != 0)
					{
						continue;
					}
					
					Visited[NeighborIndex] = 1;
					Queue.Add(NeighborIndex);
				}
			}
			
			// Now Visited marks the largest component. Update LandMask and rebuild LandPixels.
			LandPixels.Reset();
			
			for (int32 Y = 0; Y < Height; ++Y)
			{
				for (int32 X = 0; X < Width; ++X)
				{
					int32 Index = Y * Width + X;
					if (LandMask[Index] != 0)
					{
						if (Visited[Index] != 0)
						{
							// Keep as land (part of largest component)
							LandPixels.Add(FIntPoint(X, Y));
						}
						else
						{
							// Remove (island not part of largest component)
							LandMask[Index] = 0;
						}
					}
				}
			}
			
			UE_LOG(LogTemp, Log, TEXT("Kept largest landmass: %d pixels (removed %d island pixels)"),
				LargestComponentSize, TargetLandPixels - LargestComponentSize);
		}
	}

	// ===== PASS 4 (Optional): Fill enclosed ocean holes (lakes) =====
	// Flood-fill ocean from all border pixels using 4-way adjacency.
	// Any ocean pixel NOT reached is an enclosed hole and gets converted to land.
	// This ensures the final landmass has no interior lakes/holes.
	
	if (Settings.bFillEnclosedHoles)
	{
		// VisitedOcean: tracks ocean pixels reachable from border
		TArray<uint8> VisitedOcean;
		VisitedOcean.SetNumZeroed(TotalPixels);
		
		// BFS queue for flood-fill
		TArray<int32> Queue;
		Queue.Reserve(TotalPixels / 4);
		
		// 4-way adjacency offsets
		const int32 DX[4] = { 1, -1, 0, 0 };
		const int32 DY[4] = { 0, 0, 1, -1 };
		
		// Seed BFS from all border ocean pixels (top, bottom, left, right edges)
		// Top and bottom rows
		for (int32 X = 0; X < Width; ++X)
		{
			int32 TopIndex = X; // Y=0
			int32 BottomIndex = (Height - 1) * Width + X; // Y=Height-1
			
			if (LandMask[TopIndex] == 0 && VisitedOcean[TopIndex] == 0)
			{
				VisitedOcean[TopIndex] = 1;
				Queue.Add(TopIndex);
			}
			if (LandMask[BottomIndex] == 0 && VisitedOcean[BottomIndex] == 0)
			{
				VisitedOcean[BottomIndex] = 1;
				Queue.Add(BottomIndex);
			}
		}
		
		// Left and right columns (excluding corners already processed)
		for (int32 Y = 1; Y < Height - 1; ++Y)
		{
			int32 LeftIndex = Y * Width; // X=0
			int32 RightIndex = Y * Width + (Width - 1); // X=Width-1
			
			if (LandMask[LeftIndex] == 0 && VisitedOcean[LeftIndex] == 0)
			{
				VisitedOcean[LeftIndex] = 1;
				Queue.Add(LeftIndex);
			}
			if (LandMask[RightIndex] == 0 && VisitedOcean[RightIndex] == 0)
			{
				VisitedOcean[RightIndex] = 1;
				Queue.Add(RightIndex);
			}
		}
		
		// BFS flood-fill from all border ocean pixels
		while (Queue.Num() > 0)
		{
			int32 CurrentIndex = Queue.Pop(EAllowShrinking::No);
			
			int32 CX = CurrentIndex % Width;
			int32 CY = CurrentIndex / Width;
			
			for (int32 Dir = 0; Dir < 4; ++Dir)
			{
				int32 NX = CX + DX[Dir];
				int32 NY = CY + DY[Dir];
				
				if (NX < 0 || NX >= Width || NY < 0 || NY >= Height)
				{
					continue;
				}
				
				int32 NeighborIndex = NY * Width + NX;
				
				// Only visit unvisited ocean pixels
				if (LandMask[NeighborIndex] == 0 && VisitedOcean[NeighborIndex] == 0)
				{
					VisitedOcean[NeighborIndex] = 1;
					Queue.Add(NeighborIndex);
				}
			}
		}
		
		// Convert any ocean pixel NOT visited (enclosed hole) to land
		int32 HolePixelsFilled = 0;
		for (int32 Index = 0; Index < TotalPixels; ++Index)
		{
			if (LandMask[Index] == 0 && VisitedOcean[Index] == 0)
			{
				LandMask[Index] = 1;
				++HolePixelsFilled;
			}
		}
		
		// Rebuild LandPixels if we filled any holes
		if (HolePixelsFilled > 0)
		{
			LandPixels.Reset();
			for (int32 Y = 0; Y < Height; ++Y)
			{
				for (int32 X = 0; X < Width; ++X)
				{
					int32 Index = Y * Width + X;
					if (LandMask[Index] != 0)
					{
						LandPixels.Add(FIntPoint(X, Y));
					}
				}
			}
			
			UE_LOG(LogTemp, Log, TEXT("Filled %d enclosed hole pixels (lakes)"), HolePixelsFilled);
		}
	}

	// ===== PASS 5: Circular edge margin enforcement =====
	// Enforces a circular land zone: all land outside a noise-modulated circle
	// centred on the map is eroded to ocean. This creates a "round map" appearance
	// with guaranteed ocean at all edges and corners.
	if (MinEdgePaddingNorm > 0.0f)
	{
		int32 EdgePixelsCleared = 0;
		const float InvWidthMinus1 = (Width > 1) ? 1.0f / static_cast<float>(Width - 1) : 0.0f;
		const float InvHeightMinus1 = (Height > 1) ? 1.0f / static_cast<float>(Height - 1) : 0.0f;

		// Circular land zone radius (from map centre)
		const float LandRadiusNorm = 0.5f - MinEdgePaddingNorm;

		// Soft transition zone width for organic erosion at the boundary
		constexpr float SoftMarginFraction = 1.5f; // transition extends this × margin inward
		const float SoftTransitionWidth = MinEdgePaddingNorm * SoftMarginFraction;

		for (int32 Y = 0; Y < Height; ++Y)
		{
			const float NormY = static_cast<float>(Y) * InvHeightMinus1;

			for (int32 X = 0; X < Width; ++X)
			{
				const int32 Index = Y * Width + X;
				if (LandMask[Index] == 0)
				{
					continue; // Already ocean, skip
				}

				const float NormX = static_cast<float>(X) * InvWidthMinus1;

				// Circular distance from map centre
				const float CDX = NormX - 0.5f;
				const float CDY = NormY - 0.5f;
				const float DistFromCenter = FMath::Sqrt(CDX * CDX + CDY * CDY);

				// How far inside the land circle boundary are we?
				const float MarginDist = LandRadiusNorm - DistFromCenter;

				// Hard guarantee: outside the land circle → always ocean
				if (MarginDist <= 0.0f)
				{
					LandMask[Index] = 0;
					++EdgePixelsCleared;
					continue;
				}

				// Soft transition zone: noise-modulated erosion for organic coastlines
				if (MarginDist < SoftTransitionWidth)
				{
					// Normalised position within the transition band: 0 at hard boundary, 1 deep inside
					const float TransitionT = MarginDist / SoftTransitionWidth;

					// Sample noise for organic erosion pattern
					const float ErodeNoise = FBM(
						NormX * 8.0f + EdgeNoiseOffset.X + 300.0f,
						NormY * 8.0f + EdgeNoiseOffset.Y + 300.0f,
						3, 0.5f) * 0.5f + 0.5f; // Remap to [0, 1]

					// Combine transition distance with noise: land survives if TransitionT > noise threshold
					// This creates irregular, natural-looking coastlines at the circular boundary
					constexpr float ErodeNoiseScale = 0.85f;
					constexpr float ErodeMinThreshold = 0.05f;
					const float SurvivalThreshold = ErodeNoise * ErodeNoiseScale + ErodeMinThreshold;
					if (TransitionT < SurvivalThreshold)
					{
						LandMask[Index] = 0;
						++EdgePixelsCleared;
					}
				}
			}
		}

		// Rebuild LandPixels if we cleared anything
		if (EdgePixelsCleared > 0)
		{
			LandPixels.Reset();
			for (int32 Y = 0; Y < Height; ++Y)
			{
				for (int32 X = 0; X < Width; ++X)
				{
					const int32 Index = Y * Width + X;
					if (LandMask[Index] != 0)
					{
						LandPixels.Add(FIntPoint(X, Y));
					}
				}
			}

			UE_LOG(LogTemp, Log, TEXT("Smooth edge erosion: cleared %d land pixels within %.1fm margin"),
				EdgePixelsCleared, Settings.MinEdgeMarginMeters);
		}
	}

	UE_LOG(LogTemp, Log, TEXT("Land mask generated: %d land pixels out of %d total (%.1f%%)"),
		LandPixels.Num(), TotalPixels,
		(float)LandPixels.Num() / (float)TotalPixels * 100.0f);
}

//------------------------------------------------------------------------------
// GeneratePreviewTexture: Creates a visual black/white texture from LandMask.
// 
// Output format:
//   - White (255,255,255) = Land
//   - Black (0,0,0) = Ocean
//   - BGRA8 format, no mipmaps, nearest filtering for crisp pixel edges
// 
// This texture is primarily for visualization/debugging. The actual land/ocean
// data is in the LandMask array.
//------------------------------------------------------------------------------
void ULandmassGenerator::GeneratePreviewTexture()
{
	const int32 Width = Settings.TextureResolution;
	const int32 Height = Settings.TextureResolution;

	// Create a transient texture (not saved to disk, exists only in memory)
	PreviewTexture = UTexture2D::CreateTransient(Width, Height, PF_B8G8R8A8);
	if (!PreviewTexture)
	{
		UE_LOG(LogTemp, Error, TEXT("LandmassGenerator::GeneratePreviewTexture - Failed to create texture"));
		return;
	}

	// Configure texture settings for crisp, pixel-perfect display
	PreviewTexture->MipGenSettings = TMGS_NoMipmaps;  // No blurring between mip levels
	PreviewTexture->SRGB = false;                      // Linear color space
	PreviewTexture->Filter = TF_Nearest;              // Crisp pixel edges, no interpolation

	// Lock texture memory for direct pixel writing
	FTexture2DMipMap& Mip = PreviewTexture->GetPlatformData()->Mips[0];
	void* TextureData = Mip.BulkData.Lock(LOCK_READ_WRITE);
	uint8* Pixels = static_cast<uint8*>(TextureData);

	// Write pixels: BGRA format (4 bytes per pixel)
	for (int32 Y = 0; Y < Height; ++Y)
	{
		for (int32 X = 0; X < Width; ++X)
		{
			const int32 PixelIndex = (Y * Width + X) * 4;  // 4 bytes per pixel
			const int32 MapIndex = Y * Width + X;

			bool bIsLand = (MapIndex < LandMask.Num()) ? (LandMask[MapIndex] != 0) : false;

			// White = land (255), Black = ocean (0)
			uint8 ColorValue = bIsLand ? 255 : 0;

			Pixels[PixelIndex + 0] = ColorValue; // Blue
			Pixels[PixelIndex + 1] = ColorValue; // Green
			Pixels[PixelIndex + 2] = ColorValue; // Red
			Pixels[PixelIndex + 3] = 255;        // Alpha (fully opaque)
		}
	}

	// Unlock and upload to GPU
	Mip.BulkData.Unlock();
	PreviewTexture->UpdateResource();

	UE_LOG(LogTemp, Log, TEXT("LandmassGenerator::GeneratePreviewTexture - Created %dx%d texture"),
		Width, Height);
}

//------------------------------------------------------------------------------
// Noise2D: Basic 2D value noise with smooth interpolation.
// 
// Algorithm:
//   1. Offset input coordinates by seed-derived values for unique patterns per seed
//   2. Find the integer grid cell containing (X, Y)
//   3. Compute pseudo-random values at the 4 corners using a hash function
//   4. Bilinearly interpolate using smoothstep for continuous, smooth noise
// 
// Returns values approximately in the range [-1, 1].
//------------------------------------------------------------------------------
float ULandmassGenerator::Noise2D(float X, float Y) const
{
	// Offset by seed to get different patterns for different seeds
	// Using irrational-ish multipliers to avoid obvious repetition
	X += (Settings.Seed % 10000) * 0.37f;
	Y += (Settings.Seed % 10000) * 0.53f;

	// Find integer grid cell and fractional position within cell
	int32 Xi = FMath::FloorToInt(X);
	int32 Yi = FMath::FloorToInt(Y);
	float Xf = X - Xi;  // 0 to 1 within cell
	float Yf = Y - Yi;

	// Smoothstep interpolation weights (Hermite curve: 3t² - 2t³)
	// This eliminates visible grid artifacts compared to linear interpolation
	float U = Xf * Xf * (3.0f - 2.0f * Xf);
	float V = Yf * Yf * (3.0f - 2.0f * Yf);

	// Hash function: maps integer grid coordinates to pseudo-random float
	// Uses large primes for good distribution and includes seed for variation
	// NOTE: All arithmetic in uint32 to avoid signed overflow UB
	int32 SeedHash = Settings.Seed;
	auto Hash = [SeedHash](int32 X, int32 Y) -> float
	{
		uint32 N = static_cast<uint32>(X) + static_cast<uint32>(Y) * 57u + static_cast<uint32>(SeedHash);
		N = (N << 13) ^ N;
		N = N * (N * N * 15731u + 789221u) + 1376312589u;
		return 1.0f - static_cast<float>(N & 0x7fffffffu) / 1073741824.0f;
	};

	// Get pseudo-random values at the 4 corners of this grid cell
	float A = Hash(Xi, Yi);         // Bottom-left
	float B = Hash(Xi + 1, Yi);     // Bottom-right
	float C = Hash(Xi, Yi + 1);     // Top-left
	float D = Hash(Xi + 1, Yi + 1); // Top-right

	// Bilinear interpolation: first horizontal, then vertical
	float AB = FMath::Lerp(A, B, U);  // Bottom edge
	float CD = FMath::Lerp(C, D, U);  // Top edge

	return FMath::Lerp(AB, CD, V);    // Final interpolated value
}

//------------------------------------------------------------------------------
// FBM (Fractal Brownian Motion): Layered noise for natural-looking patterns.
// 
// Combines multiple "octaves" of noise at increasing frequencies and
// decreasing amplitudes. This creates rich, multi-scale detail similar
// to natural phenomena like coastlines, clouds, and terrain.
// 
// Parameters:
//   Octaves     - Number of noise layers (4-6 typical for terrain)
//   Persistence - How quickly amplitude decreases (0.5 = halves each octave)
// 
// Each octave: frequency doubles, amplitude *= persistence
//------------------------------------------------------------------------------
float ULandmassGenerator::FBM(float X, float Y, int32 Octaves, float Persistence) const
{
	float Total = 0.0f;
	float Amplitude = 1.0f;
	float Frequency = 1.0f;
	float MaxValue = 0.0f;  // For normalization

	for (int32 i = 0; i < Octaves; ++i)
	{
		// Sample noise at current frequency, scale by current amplitude
		Total += Noise2D(X * Frequency, Y * Frequency) * Amplitude;
		
		// Track maximum possible value for normalization
		MaxValue += Amplitude;
		
		// Next octave: higher frequency (finer detail), lower amplitude
		Amplitude *= Persistence;
		Frequency *= 2.0f;
	}

	// Normalize to approximately [-1, 1] range
	return Total / MaxValue;
}

//------------------------------------------------------------------------------
// GetIslandMask: Computes how "land-like" a point is.
// 
// Generates a noise-based landscape where the shape emerges from layered
// FBM noise with a gentle centre preference.  The noise field can produce
// multiple peaks — whether the final result is a single island or an
// archipelago depends on the LandCoveragePercent threshold chosen later.
// 
// Components:
//   1. Primary terrain noise: large-scale landmass shapes (low frequency)
//   2. Secondary terrain noise: medium ridges and valleys
//   3. Coast noise: irregular coastline details (medium-high frequency)
//   4. Detail noise: fine bumps and indentations (high frequency)
//   5. Centre bias: gentle preference for land near the map centre
//   6. Edge falloff: enforces circular ocean ring at map boundary
// 
// If domain warping is enabled, coordinates are displaced before noise sampling
// to create large-scale bends, peninsulas, and bays for more realistic coastlines.
// 
// Returns 0.0 (definitely ocean) to ~1.0 (definitely land).
// The actual land/ocean threshold is determined by GenerateLandMask()
// based on target land coverage percentage.
//------------------------------------------------------------------------------
float ULandmassGenerator::GetIslandMask(float NormX, float NormY) const
{
	// ===== Domain Warping (optional) =====
	// Displaces sampling coordinates using a low-frequency noise field.
	// This creates large-scale bends and irregular coastline features.
	// The warp is applied before all other noise sampling to affect the entire shape.
	
	float SampleX = NormX;
	float SampleY = NormY;
	
	if (Settings.bEnableDomainWarp)
	{
		// Compute warp displacement using FBM noise.
		// X and Y components use different offsets to decorrelate and avoid diagonal bias.
		float WarpX = FBM(
			NormX * Settings.WarpFrequency + WarpOffsetX.X,
			NormY * Settings.WarpFrequency + WarpOffsetX.Y,
			Settings.WarpOctaves,
			Settings.WarpPersistence
		);
		
		float WarpY = FBM(
			NormX * Settings.WarpFrequency + WarpOffsetY.X,
			NormY * Settings.WarpFrequency + WarpOffsetY.Y,
			Settings.WarpOctaves,
			Settings.WarpPersistence
		);
		
		// Apply warp: displace coordinates by noise * amplitude
		// FBM returns ~[-1,1], so displacement range is [-Amplitude, +Amplitude]
		SampleX = NormX + WarpX * Settings.WarpAmplitude;
		SampleY = NormY + WarpY * Settings.WarpAmplitude;
		
		// Note: We intentionally do NOT clamp SampleX/SampleY to [0,1].
		// Allowing slight out-of-bounds sampling creates natural edge variation.
		// The edge falloff logic below still uses original NormX/NormY for border enforcement.
	}

	// ===== Noise-driven terrain field =====
	// The landscape is built from layered noise combined with multiple island
	// peaks (seed-derived positions in Initialize).  Each peak creates a bias
	// that nucleates an island; noise adds organic shape.  The adaptive threshold
	// then determines how much land survives, naturally creating archipelago
	// patterns with a main island and several satellites.

	// Primary terrain noise: large-scale shape variation that gives each
	// island an organic, non-circular outline.  Amplitude 0.5 adds significant
	// irregularity without overwhelming the multi-peak bias (0.15-0.35).
	float PrimaryNoise = FBM(
		SampleX * 3.0f + ShapeNoiseOffset.X,
		SampleY * 3.0f + ShapeNoiseOffset.Y,
		4, 0.55f) * 0.5f;

	// Secondary terrain noise: medium-scale ridges, isthmuses, and satellite islands.
	// Higher frequency (5.5) creates small sub-peaks that become tiny islets
	// when threshold cuts through.
	float SecondaryNoise = FBM(
		SampleX * 5.5f + DetailNoiseOffset.X,
		SampleY * 5.5f + DetailNoiseOffset.Y,
		3, 0.5f) * 0.25f;

	// Coast noise: higher-frequency coastline irregularity.
	float CoastNoise = FBM(
		SampleX * 8.0f + ShapeNoiseOffset.X + 100.0f,
		SampleY * 8.0f + ShapeNoiseOffset.Y + 100.0f,
		2, 0.5f) * 0.15f;

	// Detail noise: fine-scale coastal indentations.
	float DetailNoise = FBM(
		SampleX * 14.0f + DetailNoiseOffset.X + 200.0f,
		SampleY * 14.0f + DetailNoiseOffset.Y + 200.0f,
		2, 0.5f) * 0.08f;

	// ===== Multi-peak island bias =====
	// Instead of a single centre dome, multiple peaks at seed-derived positions
	// create distinct island nucleation points.  Each peak adds a radial bias
	// that falls off with distance, creating separate "islands" in the noise field.
	// The noise can still split or merge peaks depending on the threshold,
	// producing varied archipelago layouts per seed.
	float PeakBias = 0.0f;
	for (const FIslandPeak& Peak : IslandPeaks)
	{
		float DX = NormX - Peak.Position.X;
		float DY = NormY - Peak.Position.Y;
		float Dist = FMath::Sqrt(DX * DX + DY * DY);
		float NormDist = FMath::Clamp(Dist / Peak.Radius, 0.0f, 1.0f);
		// Smooth cosine falloff: 1 at centre, 0 at radius
		float Falloff = 0.5f * (1.0f + FMath::Cos(NormDist * PI));
		PeakBias += Peak.Strength * Falloff;
	}

	// Combine all layers.  Multi-peak bias creates distinct island centres;
	// noise adds organic shape variation and can merge or split peaks.
	float IslandValue = PeakBias + PrimaryNoise + SecondaryNoise + CoastNoise + DetailNoise;

	// ===== Circular edge falloff: enforce round land zone =====
	// Uses ORIGINAL coordinates (NormX, NormY) to enforce a circular boundary.
	// Land is constrained to a circle inscribed in the map; corners are always ocean.
	// This creates a "round map" appearance with a guaranteed ocean ring.

	// Radial distance from map centre (0 at centre, 0.5 at mid-edge, ~0.707 at corner)
	float RadDistX = NormX - 0.5f;
	float RadDistY = NormY - 0.5f;
	float RadialDist = FMath::Sqrt(RadDistX * RadDistX + RadDistY * RadDistY);

	// Land zone radius: half the map minus the ocean margin
	float LandRadius = 0.5f - MinEdgePaddingNorm;

	// Add noise for organic, non-perfectly-circular coastline at the boundary
	// Edge noise uses WARPED coordinates for consistency with other noise layers
	float EdgeNoise = FBM(
		SampleX * 6.0f + EdgeNoiseOffset.X,
		SampleY * 6.0f + EdgeNoiseOffset.Y,
		3, 0.5f) * 0.5f + 0.5f;

	// Noise modulates the land radius slightly for organic coastline shape
	float NoisyRadius = LandRadius * (0.88f + 0.12f * EdgeNoise);

	// Transition width for smooth falloff (proportional to margin, minimum 6%
	// so that even small margins like 30 m produce a natural fade zone)
	constexpr float MinTransitionWidth = 0.06f; // 6% of map width
	float TransitionWidth = FMath::Max(MinTransitionWidth, MinEdgePaddingNorm * 0.5f);

	// Smooth falloff: 1 inside the land circle, 0 outside
	float EdgeFalloff = FMath::Clamp((NoisyRadius - RadialDist) / TransitionWidth, 0.0f, 1.0f);

	// Apply smoothstep curve for gradual transition (avoids harsh cutoff)
	EdgeFalloff = EdgeFalloff * EdgeFalloff * (3.0f - 2.0f * EdgeFalloff);

	// Apply edge falloff — multiplies the noise field to zero near edges
	IslandValue *= EdgeFalloff;

	// Clamp to valid range (noise can push slightly negative)
	return FMath::Clamp(IslandValue, 0.0f, 1.0f);
}
