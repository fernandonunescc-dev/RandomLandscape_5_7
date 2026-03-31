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

	// Adjust land coverage based on MapType
	switch (Settings.MapType)
	{
	case EMapType::Island:
		// Single landmass surrounded by ocean - keep user setting
		Settings.bKeepOnlyLargestLandmass = true;
		break;
	case EMapType::Archipelago:
		// Multiple islands - don't keep only largest, let all islands through
		Settings.bKeepOnlyLargestLandmass = false;
		break;
	}

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

	// === Generate island centers for Archipelago mode ===
	if (Settings.MapType == EMapType::Archipelago)
	{
		IslandCenters.Reset();
		IslandRadii.Reset();
		const int32 NumIslands = FMath::Max(2, Settings.IslandCount);

		for (int32 i = 0; i < NumIslands; ++i)
		{
			// Place islands within the inner 70% of the map to avoid edge clipping
			FVector2D Center(
				RandomStream.FRandRange(0.15f, 0.85f),
				RandomStream.FRandRange(0.15f, 0.85f)
			);
			IslandCenters.Add(Center);

			// Random radius for each island (larger = bigger island)
			float Radius = RandomStream.FRandRange(0.06f, 0.18f);
			IslandRadii.Add(Radius);
		}
	}

	UE_LOG(LogTemp, Log, TEXT("LandmassGenerator initialized - Resolution: %d, Seed: %d, LandCoverage: %.1f%%, MapType: %d, MapSize: %d, MaxHeight: %.0f, DomainWarp: %s"),
		Settings.TextureResolution, Settings.Seed, Settings.LandCoveragePercent,
		static_cast<int32>(Settings.MapType), static_cast<int32>(Settings.MapSize), Settings.MaxMapHeight,
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

		// Use the appropriate mask function based on MapType
		if (Settings.MapType == EMapType::Archipelago)
		{
			MaskValues[Index] = GetArchipelagoMask(NormX, NormY);
		}
		else
		{
			MaskValues[Index] = GetIslandMask(NormX, NormY);
		}
	});

	// ===== Calculate adaptive threshold for exact land coverage =====
	// Using std::nth_element (introselect) for O(n) average complexity instead of O(n log n) sort.
	// IMPORTANT: nth_element permutes the array, so we must work on a COPY.
	// MaskValues must remain pixel-aligned for Pass 2 threshold comparison.
	
	// Calculate target land pixel count (clamped to valid range)
	float TargetLandPercent = FMath::Clamp(Settings.LandCoveragePercent, 5.0f, 75.0f) / 100.0f;
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
// Combines multiple factors to create organic island shapes:
//   1. Distance from centre: land more likely near texture centre
//   2. Coast noise: irregular coastline details (medium frequency)
//   3. Shape noise: large-scale island shape variation (low frequency)
//   4. Detail noise: fine bumps and indentations (high frequency)
//   5. Edge falloff: enforces minimum margin from texture borders
// 
// If domain warping is enabled, coordinates are displaced before noise sampling
// to create large-scale bends, peninsulas, and bays for more realistic coastlines.
// 
// Returns 0.0 (definitely ocean) to 1.0 (definitely land).
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

	// ===== Center-based base shape =====
	// Transform to centered coordinates: (0,0) at center, ±0.5 at edges
	// Use ORIGINAL coordinates for centre distance to maintain island centering
	float CX = NormX - 0.5f;
	float CY = NormY - 0.5f;

	// Distance from center, scaled so corners are at distance 1.0
	// This creates a radial gradient: high in center, low at edges
	float DistFromCenter = FMath::Sqrt(CX * CX + CY * CY) * 2.0f;

	// ===== Layered noise for organic coastlines =====
	// All FBM calls use warped coordinates (SampleX, SampleY) for consistent deformation
	
	// Medium-frequency noise for irregular coastline shape
	float NoiseScale = 3.0f;
	float CoastNoise = FBM(SampleX * NoiseScale, SampleY * NoiseScale, 4, 0.5f) * 0.4f;

	// Low-frequency noise for overall island shape variation
	// Uses seed-derived ShapeNoiseOffset to decorrelate from coast noise
	float ShapeNoise = FBM(
		SampleX * 1.5f + ShapeNoiseOffset.X, 
		SampleY * 1.5f + ShapeNoiseOffset.Y, 
		3, 0.6f) * 0.3f;

	// Combine: start with inverted distance (1 at centre, 0 at corners) + noise
	float IslandValue = 1.0f - DistFromCenter + CoastNoise + ShapeNoise;

	// High-frequency noise for fine coastal details (bays, peninsulas)
	// Uses seed-derived DetailNoiseOffset to decorrelate from other layers
	float DetailNoise = FBM(
		SampleX * 8.0f + DetailNoiseOffset.X, 
		SampleY * 8.0f + DetailNoiseOffset.Y, 
		2, 0.5f) * 0.15f;
	IslandValue += DetailNoise;

	// ===== Edge falloff: enforce minimum margin from texture borders =====
	// Uses ORIGINAL coordinates (NormX, NormY) to enforce border padding
	// regardless of warp displacement—land must not touch texture edges.
	// MinEdgePaddingNorm is derived from MinEdgeMarginMeters in Initialize().
	
	// Calculate distance from each edge (0 at edge, 0.5 at centre)
	float DistFromLeft = NormX;
	float DistFromRight = 1.0f - NormX;
	float DistFromTop = NormY;
	float DistFromBottom = 1.0f - NormY;

	// Use the minimum distance to any edge
	float MinEdgeDist = FMath::Min(FMath::Min(DistFromLeft, DistFromRight), 
	                              FMath::Min(DistFromTop, DistFromBottom));

	// Add noise to the falloff zone for irregular (non-rectangular) borders
	// Edge noise uses WARPED coordinates for consistency with other noise layers
	float EdgeNoise = FBM(
		SampleX * 6.0f + EdgeNoiseOffset.X, 
		SampleY * 6.0f + EdgeNoiseOffset.Y, 
		3, 0.5f) * 0.5f + 0.5f;

	// Noisy padding zone: base varies between 2.5% and 7.5%, but never below the
	// configured minimum edge margin (MinEdgePaddingNorm, default 20 m).
	float NoisyPadding = FMath::Max(MinEdgePaddingNorm, 0.05f * (0.5f + EdgeNoise));

	// Smooth falloff: 0 at edge, 1 when past the padding zone
	float EdgeFalloff = FMath::Clamp(MinEdgeDist / FMath::Max(NoisyPadding, 0.001f), 0.0f, 1.0f);

	// Apply smoothstep curve for gradual transition (avoids harsh cutoff)
	EdgeFalloff = EdgeFalloff * EdgeFalloff * (3.0f - 2.0f * EdgeFalloff);

	// Apply edge falloff to island value
	IslandValue *= EdgeFalloff;

	// Clamp to valid range
	return FMath::Clamp(IslandValue, 0.0f, 1.0f);
}

//------------------------------------------------------------------------------
// GetArchipelagoMask: Generates multiple island shapes for Archipelago mode.
//
// Each island has its own center and radius (generated in Initialize).
// Uses the same noise techniques as GetIslandMask but applied per-island
// with a smooth maximum combination to create natural multi-island layouts.
//------------------------------------------------------------------------------
float ULandmassGenerator::GetArchipelagoMask(float NormX, float NormY) const
{
	float MaxIslandValue = 0.0f;

	for (int32 i = 0; i < IslandCenters.Num(); ++i)
	{
		const FVector2D& Center = IslandCenters[i];
		const float Radius = IslandRadii[i];

		// Distance from this island's center
		float DX = NormX - Center.X;
		float DY = NormY - Center.Y;
		float Dist = FMath::Sqrt(DX * DX + DY * DY);

		// Normalize distance by island radius
		float NormDist = Dist / FMath::Max(Radius, 0.01f);

		if (NormDist > 2.0f)
		{
			continue; // Too far from this island, skip for performance
		}

		// Base island shape: smooth falloff from center
		float IslandValue = FMath::Max(0.0f, 1.0f - NormDist);

		// Apply domain warping for organic coastlines
		float SampleX = NormX;
		float SampleY = NormY;

		if (Settings.bEnableDomainWarp)
		{
			float WarpX = FBM(
				NormX * Settings.WarpFrequency + WarpOffsetX.X + (float)i * 37.0f,
				NormY * Settings.WarpFrequency + WarpOffsetX.Y,
				Settings.WarpOctaves, Settings.WarpPersistence);
			float WarpY = FBM(
				NormX * Settings.WarpFrequency + WarpOffsetY.X,
				NormY * Settings.WarpFrequency + WarpOffsetY.Y + (float)i * 53.0f,
				Settings.WarpOctaves, Settings.WarpPersistence);

			SampleX = NormX + WarpX * Settings.WarpAmplitude;
			SampleY = NormY + WarpY * Settings.WarpAmplitude;
		}

		// Add noise for irregular coastlines (each island gets a unique offset)
		float CoastNoise = FBM(
			SampleX * 4.0f + ShapeNoiseOffset.X + (float)i * 100.0f,
			SampleY * 4.0f + ShapeNoiseOffset.Y,
			3, 0.5f) * 0.35f;

		float DetailNoise = FBM(
			SampleX * 10.0f + DetailNoiseOffset.X + (float)i * 200.0f,
			SampleY * 10.0f + DetailNoiseOffset.Y,
			2, 0.5f) * 0.1f;

		IslandValue += CoastNoise + DetailNoise;
		IslandValue = FMath::Max(0.0f, IslandValue);

		// Take the maximum across all islands (smooth union)
		MaxIslandValue = FMath::Max(MaxIslandValue, IslandValue);
	}

	// Edge falloff to enforce minimum margin from texture borders
	float DistFromLeft = NormX;
	float DistFromRight = 1.0f - NormX;
	float DistFromTop = NormY;
	float DistFromBottom = 1.0f - NormY;
	float MinEdgeDist = FMath::Min(FMath::Min(DistFromLeft, DistFromRight),
	                              FMath::Min(DistFromTop, DistFromBottom));
	// Use whichever is larger: the fixed 5% base padding or the metres-based minimum
	float Padding = FMath::Max(MinEdgePaddingNorm, 0.05f);
	float EdgeFalloff = FMath::Clamp(MinEdgeDist / Padding, 0.0f, 1.0f);
	EdgeFalloff = EdgeFalloff * EdgeFalloff * (3.0f - 2.0f * EdgeFalloff);

	MaxIslandValue *= EdgeFalloff;

	return FMath::Clamp(MaxIslandValue, 0.0f, 1.0f);
}
