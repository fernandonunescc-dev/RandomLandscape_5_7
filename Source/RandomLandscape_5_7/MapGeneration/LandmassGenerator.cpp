// LandmassGenerator.cpp
// Implementation of landmass texture generation using FBM noise
// Version: 01.26.2026.21.45

#include "LandmassGenerator.h"
#include "Engine/Texture2D.h"
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

	UE_LOG(LogTemp, Log, TEXT("LandmassGenerator initialized - Resolution: %d, Seed: %d, LandCoverage: %.1f%%"),
		Settings.TextureResolution, Settings.Seed, Settings.LandCoveragePercent);
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
//   Pass 1: Compute GetContinentMask() for every pixel (continuous 0-1 values)
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

	// ===== PASS 1: Compute continent mask value for every pixel =====
	// These are continuous values from 0.0 (definitely ocean) to 1.0 (definitely land)
	TArray<float> MaskValues;
	MaskValues.SetNum(TotalPixels);

	for (int32 Y = 0; Y < Height; ++Y)
	{
		for (int32 X = 0; X < Width; ++X)
		{
			// Normalize pixel coordinates to 0-1 range for noise sampling
			// Using (Width-1) so edges are symmetric: X=0 -> 0.0, X=Width-1 -> 1.0
			float NormX = (Width > 1) ? static_cast<float>(X) / static_cast<float>(Width - 1) : 0.5f;
			float NormY = (Height > 1) ? static_cast<float>(Y) / static_cast<float>(Height - 1) : 0.5f;

			int32 Index = Y * Width + X;
			MaskValues[Index] = GetContinentMask(NormX, NormY);
		}
	}

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
	//   1. First mark all pixels strictly GREATER than threshold as land.
	//   2. Collect pixels exactly EQUAL to threshold.
	//   3. Add only as many equal-pixels as needed to reach TargetLandPixels (deterministic order).
	
	TArray<int32> EqualThresholdIndices; // Indices of pixels exactly at threshold
	int32 StrictlyGreaterCount = 0;
	
	// Pass 2a: Mark strictly greater, collect equal
	for (int32 Index = 0; Index < TotalPixels; ++Index)
	{
		float Value = MaskValues[Index];
		if (Value > LandThreshold)
		{
			LandMask[Index] = 1;
			++StrictlyGreaterCount;
		}
		else if (Value == LandThreshold)
		{
			LandMask[Index] = 0; // Tentatively ocean; may promote below
			EqualThresholdIndices.Add(Index);
		}
		else
		{
			LandMask[Index] = 0;
		}
	}
	
	// Pass 2b: Promote exactly as many equal-threshold pixels as needed
	int32 NeededFromEqual = TargetLandPixels - StrictlyGreaterCount;
	NeededFromEqual = FMath::Clamp(NeededFromEqual, 0, EqualThresholdIndices.Num());
	
	for (int32 i = 0; i < NeededFromEqual; ++i)
	{
		// Deterministic: use stable index order (no shuffle needed for reproducibility)
		LandMask[EqualThresholdIndices[i]] = 1;
	}
	
	// Pass 2c: Build LandPixels array from final mask
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
// GetContinentMask: Computes how "land-like" a point is.
// 
// Combines multiple factors to create organic continent shapes:
//   1. Distance from center: land more likely near texture center
//   2. Coast noise: irregular coastline details (medium frequency)
//   3. Shape noise: large-scale continent shape variation (low frequency)
//   4. Detail noise: fine bumps and indentations (high frequency)
//   5. Edge falloff: ensures land doesn't touch texture borders
// 
// Returns 0.0 (definitely ocean) to 1.0 (definitely land).
// The actual land/ocean threshold is determined by GenerateLandMask()
// based on target land coverage percentage.
//------------------------------------------------------------------------------
float ULandmassGenerator::GetContinentMask(float NormX, float NormY) const
{
	// ===== Center-based base shape =====
	// Transform to centered coordinates: (0,0) at center, ±0.5 at edges
	float CX = NormX - 0.5f;
	float CY = NormY - 0.5f;

	// Distance from center, scaled so corners are at distance 1.0
	// This creates a radial gradient: high in center, low at edges
	float DistFromCenter = FMath::Sqrt(CX * CX + CY * CY) * 2.0f;

	// ===== Layered noise for organic coastlines =====
	
	// Medium-frequency noise for irregular coastline shape
	// (No offset needed—coast noise uses base coordinates directly)
	float NoiseScale = 3.0f;
	float CoastNoise = FBM(NormX * NoiseScale, NormY * NoiseScale, 4, 0.5f) * 0.4f;

	// Low-frequency noise for overall continent shape variation
	// Uses seed-derived ShapeNoiseOffset to decorrelate from coast noise
	float ShapeNoise = FBM(
		NormX * 1.5f + ShapeNoiseOffset.X, 
		NormY * 1.5f + ShapeNoiseOffset.Y, 
		3, 0.6f) * 0.3f;

	// Combine: start with inverted distance (1 at center, 0 at corners) + noise
	float ContinentValue = 1.0f - DistFromCenter + CoastNoise + ShapeNoise;

	// High-frequency noise for fine coastal details (bays, peninsulas)
	// Uses seed-derived DetailNoiseOffset to decorrelate from other layers
	float DetailNoise = FBM(
		NormX * 8.0f + DetailNoiseOffset.X, 
		NormY * 8.0f + DetailNoiseOffset.Y, 
		2, 0.5f) * 0.15f;
	ContinentValue += DetailNoise;

	// ===== Edge falloff: prevent land from touching texture borders =====
	// This is important for tiling or to ensure clean edges
	
	// Calculate distance from each edge (0 at edge, 0.5 at center)
	float DistFromLeft = NormX;
	float DistFromRight = 1.0f - NormX;
	float DistFromTop = NormY;
	float DistFromBottom = 1.0f - NormY;

	// Use the minimum distance to any edge
	float MinEdgeDist = FMath::Min(FMath::Min(DistFromLeft, DistFromRight), 
	                              FMath::Min(DistFromTop, DistFromBottom));

	// Add noise to the falloff zone for irregular (non-rectangular) borders
	// Uses seed-derived EdgeNoiseOffset to decorrelate from shape/detail layers
	float EdgeNoise = FBM(
		NormX * 6.0f + EdgeNoiseOffset.X, 
		NormY * 6.0f + EdgeNoiseOffset.Y, 
		3, 0.5f) * 0.5f + 0.5f;

	// Noisy padding zone: varies between 2.5% and 7.5% of texture width
	float NoisyPadding = 0.05f * (0.5f + EdgeNoise);

	// Smooth falloff: 0 at edge, 1 when past the padding zone
	float EdgeFalloff = FMath::Clamp(MinEdgeDist / FMath::Max(NoisyPadding, 0.001f), 0.0f, 1.0f);

	// Apply smoothstep curve for gradual transition (avoids harsh cutoff)
	EdgeFalloff = EdgeFalloff * EdgeFalloff * (3.0f - 2.0f * EdgeFalloff);

	// Apply edge falloff to continent value
	ContinentValue *= EdgeFalloff;

	// Clamp to valid range
	return FMath::Clamp(ContinentValue, 0.0f, 1.0f);
}
