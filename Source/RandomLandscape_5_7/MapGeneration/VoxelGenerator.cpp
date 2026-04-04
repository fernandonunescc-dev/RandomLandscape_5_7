// VoxelGenerator.cpp
// 3D voxel terrain generator implementation using FastNoise2.

#include "VoxelGenerator.h"
#include "FastNoise2Noise.h"
#include "Async/ParallelFor.h"

// ----------------------------------------------------------------
// Initialise
// ----------------------------------------------------------------
void UVoxelGenerator::Initialize(const FVoxelGenerationSettings& InSettings, int32 InSeed)
{
	Settings = InSettings;
	Seed = (InSeed == 0) ? FMath::RandRange(1, 0x7FFFFFFF) : InSeed;

	TotalVoxelsX = Settings.WorldChunksX * Settings.ChunkResolution;
	TotalVoxelsY = Settings.WorldChunksY * Settings.ChunkResolution;
	TotalVoxelsZ = Settings.WorldChunksZ * Settings.ChunkResolution;
}

// ----------------------------------------------------------------
// Generate
// ----------------------------------------------------------------
bool UVoxelGenerator::Generate()
{
	if (TotalVoxelsX <= 0 || TotalVoxelsY <= 0 || TotalVoxelsZ <= 0)
	{
		UE_LOG(LogTemp, Error, TEXT("UVoxelGenerator: Invalid world dimensions"));
		return false;
	}

	// Step 1: Generate 2D heightfield
	GenerateHeightfield();

	// Step 2: Allocate and fill chunks
	const int32 NumChunks = Settings.WorldChunksX * Settings.WorldChunksY * Settings.WorldChunksZ;
	Chunks.SetNum(NumChunks);

	for (int32 CZ = 0; CZ < Settings.WorldChunksZ; ++CZ)
	{
		for (int32 CY = 0; CY < Settings.WorldChunksY; ++CY)
		{
			for (int32 CX = 0; CX < Settings.WorldChunksX; ++CX)
			{
				const int32 Idx = ChunkIndex(CX, CY, CZ);
				Chunks[Idx].Init(FIntVector(CX, CY, CZ), Settings.ChunkResolution);
			}
		}
	}

	// Step 3: Fill density + materials in parallel per chunk
	ParallelFor(NumChunks, [this](int32 Idx)
	{
		FillChunkDensity(Chunks[Idx]);
		AssignChunkMaterials(Chunks[Idx]);
		ClassifyChunk(Chunks[Idx]);
	});

	UE_LOG(LogTemp, Log, TEXT("UVoxelGenerator: Generated %d chunks (%dx%dx%d voxels), Seed=%d"),
		NumChunks, TotalVoxelsX, TotalVoxelsY, TotalVoxelsZ, Seed);

	return true;
}

// ----------------------------------------------------------------
// Heightfield generation via FastNoise2
// ----------------------------------------------------------------
void UVoxelGenerator::GenerateHeightfield()
{
	const int32 SizeX = TotalVoxelsX;
	const int32 SizeY = TotalVoxelsY;

	// Build the terrain FN2 node graph (use domain warp for organic terrain if configured)
	auto TerrainFBM = (Settings.DomainWarpAmplitude > 0.0f)
		? FN2::MakeWarpedFBM(Settings.TerrainOctaves, Settings.TerrainGain, Settings.DomainWarpAmplitude)
		: FN2::MakeFBM(Settings.TerrainOctaves, Settings.TerrainGain);

	// Generate base heightfield
	FN2::GenGrid2D(TerrainFBM, Heightfield, 0, 0, SizeX, SizeY, Settings.TerrainFrequency, Seed);

	// Add ridged mountain noise if enabled
	TArray<float> MountainNoise;
	if (Settings.bEnableMountains && Settings.MountainAmplitude > 0.0f)
	{
		auto MountainGen = FN2::MakeRidged(Settings.MountainOctaves, 0.5f);
		FN2::GenGrid2D(MountainGen, MountainNoise, 0, 0, SizeX, SizeY, Settings.MountainFrequency, Seed + 1000);
	}

	// Combine into final heightfield [0,1]
	const float BaseHeight = Settings.BaseTerrainHeight;
	const float Amplitude = Settings.TerrainAmplitude;
	const float MountainAmp = Settings.MountainAmplitude;

	for (int32 i = 0; i < Heightfield.Num(); ++i)
	{
		float H = BaseHeight + Heightfield[i] * Amplitude;

		if (MountainNoise.Num() > 0)
		{
			// MountainNoise is in [-1,1], remap to [0,1] for additive blend
			const float M = (MountainNoise[i] + 1.0f) * 0.5f;
			H += M * MountainAmp;
		}

		Heightfield[i] = FMath::Clamp(H, 0.0f, 1.0f);
	}

	UE_LOG(LogTemp, Log, TEXT("UVoxelGenerator: Heightfield generated (%dx%d)"), SizeX, SizeY);
}

// ----------------------------------------------------------------
// Fill chunk density
// ----------------------------------------------------------------
void UVoxelGenerator::FillChunkDensity(FVoxelChunk& Chunk)
{
	const int32 Res = Chunk.Resolution;
	const int32 BaseVX = Chunk.ChunkCoord.X * Res;
	const int32 BaseVY = Chunk.ChunkCoord.Y * Res;
	const int32 BaseVZ = Chunk.ChunkCoord.Z * Res;

	// Padded size: Res+1 to include boundary voxels from neighboring chunks
	const int32 Padded = Res + 1;

	// Pre-generate 3D noise for this chunk if 3D features enabled.
	// Used for both overhang shaping and cave carving.
	TArray<float> Noise3D;
	if (Settings.bEnable3DFeatures && (Settings.CaveStrength > 0.0f || Settings.OverhangStrength > 0.0f))
	{
		auto NoiseGen = FN2::MakeFBM(Settings.CaveOctaves, 0.5f);
		FN2::GenGrid3D(NoiseGen, Noise3D,
			BaseVX, BaseVY, BaseVZ,
			Padded, Padded, Padded,
			Settings.CaveFrequency, Seed + 5000);
	}

	// Maximum cave carving magnitude in voxels — scales with chunk resolution, not total
	// world height.  This prevents deep-underground chunks from being over-carved.
	const float CaveCarveScale = Settings.CaveStrength * static_cast<float>(Res);

	// Overhang shaping scale: how strongly 3D noise reshapes the surface
	const float OverhangScale = Settings.OverhangStrength * static_cast<float>(Res);

	// Fill density for (Res+1)³ voxels (includes 1-voxel padding for seamless MC)
	for (int32 Z = 0; Z < Padded; ++Z)
	{
		for (int32 Y = 0; Y < Padded; ++Y)
		{
			for (int32 X = 0; X < Padded; ++X)
			{
				const int32 WorldVX = BaseVX + X;
				const int32 WorldVY = BaseVY + Y;
				const int32 WorldVZ = BaseVZ + Z;

				// Look up heightfield (clamped)
				const int32 HX = FMath::Clamp(WorldVX, 0, TotalVoxelsX - 1);
				const int32 HY = FMath::Clamp(WorldVY, 0, TotalVoxelsY - 1);
				const float SurfaceHeight = Heightfield[HY * TotalVoxelsX + HX];

				// Normalised height of this voxel
				const float NormZ = static_cast<float>(WorldVZ) / static_cast<float>(TotalVoxelsZ);

				// Base density: positive below surface, negative above
				float Density = (SurfaceHeight - NormZ) * static_cast<float>(TotalVoxelsZ);

				if (Noise3D.Num() > 0)
				{
					const int32 LocalIdx = Z * Padded * Padded + Y * Padded + X;
					const float NoiseVal = Noise3D[LocalIdx];

					// --- 3D terrain shaping (overhangs, undercuts, cliff faces) ---
					// Blend bidirectional 3D noise near the surface.  Positive noise
					// adds material above the heightfield → overhangs; negative noise
					// removes material below it → undercuts.  Fades out with distance
					// from the surface so deep underground stays solid.
					if (Settings.OverhangStrength > 0.0f)
					{
						const float DistFromSurface = FMath::Abs(SurfaceHeight - NormZ);
						if (DistFromSurface < Settings.OverhangRange)
						{
							const float Blend = 1.0f - (DistFromSurface / Settings.OverhangRange);
							Density += NoiseVal * OverhangScale * Blend;
						}
					}

					// --- Cave carving (deep underground only) ---
					const float DepthBelowSurface = SurfaceHeight - NormZ;
					if (Settings.CaveStrength > 0.0f && DepthBelowSurface > Settings.CaveMinDepth)
					{
						if (NoiseVal > Settings.CaveThreshold)
						{
							const float CarveAmount = (NoiseVal - Settings.CaveThreshold) * CaveCarveScale;
							Density -= CarveAmount;
						}
					}
				}

				Chunk.SetDensity(X, Y, Z, Density);
			}
		}
	}
}

// ----------------------------------------------------------------
// Assign materials
// ----------------------------------------------------------------
void UVoxelGenerator::AssignChunkMaterials(FVoxelChunk& Chunk)
{
	const int32 Res = Chunk.Resolution;
	const int32 Padded = Res + 1;
	const int32 BaseVX = Chunk.ChunkCoord.X * Res;
	const int32 BaseVY = Chunk.ChunkCoord.Y * Res;
	const int32 BaseVZ = Chunk.ChunkCoord.Z * Res;

	for (int32 Z = 0; Z < Padded; ++Z)
	{
		for (int32 Y = 0; Y < Padded; ++Y)
		{
			for (int32 X = 0; X < Padded; ++X)
			{
				const float D = Chunk.GetDensity(X, Y, Z);

				if (D <= 0.0f)
				{
					// Air or water
					const int32 WorldVZ = BaseVZ + Z;
					const float NormZ = static_cast<float>(WorldVZ) / static_cast<float>(TotalVoxelsZ);

					if (Settings.bEnableWater && NormZ < Settings.SeaLevel)
					{
						Chunk.SetMaterial(X, Y, Z, EVoxelMaterial::Water);
					}
					else
					{
						Chunk.SetMaterial(X, Y, Z, EVoxelMaterial::Air);
					}
					continue;
				}

				// Solid voxel — determine material by context
				const int32 WorldVX = BaseVX + X;
				const int32 WorldVY = BaseVY + Y;
				const int32 WorldVZ = BaseVZ + Z;
				const float NormZ = static_cast<float>(WorldVZ) / static_cast<float>(TotalVoxelsZ);

				// Look up heightfield for this column
				const int32 HX = FMath::Clamp(WorldVX, 0, TotalVoxelsX - 1);
				const int32 HY = FMath::Clamp(WorldVY, 0, TotalVoxelsY - 1);
				const float SurfaceHeight = Heightfield[HY * TotalVoxelsX + HX];

				// Depth below surface in voxels
				const float DepthVoxels = (SurfaceHeight - NormZ) * static_cast<float>(TotalVoxelsZ);

				// Bedrock at the very bottom
				if (WorldVZ <= 1)
				{
					Chunk.SetMaterial(X, Y, Z, EVoxelMaterial::Bedrock);
					continue;
				}

				// Snow above snow line
				if (NormZ > Settings.SnowLineHeight)
				{
					if (DepthVoxels <= static_cast<float>(Settings.GrassLayerDepth))
					{
						Chunk.SetMaterial(X, Y, Z, EVoxelMaterial::Snow);
					}
					else
					{
						Chunk.SetMaterial(X, Y, Z, EVoxelMaterial::Stone);
					}
					continue;
				}

				// Beach sand near sea level
				if (NormZ < Settings.BeachHeight && NormZ >= Settings.SeaLevel - 0.05f)
				{
					if (DepthVoxels <= static_cast<float>(Settings.DirtLayerDepth))
					{
						Chunk.SetMaterial(X, Y, Z, EVoxelMaterial::Sand);
					}
					else
					{
						Chunk.SetMaterial(X, Y, Z, EVoxelMaterial::Stone);
					}
					continue;
				}

				// Normal terrain layering: grass → dirt → stone
				if (DepthVoxels <= static_cast<float>(Settings.GrassLayerDepth))
				{
					Chunk.SetMaterial(X, Y, Z, EVoxelMaterial::Grass);
				}
				else if (DepthVoxels <= static_cast<float>(Settings.GrassLayerDepth + Settings.DirtLayerDepth))
				{
					Chunk.SetMaterial(X, Y, Z, EVoxelMaterial::Dirt);
				}
				else
				{
					Chunk.SetMaterial(X, Y, Z, EVoxelMaterial::Stone);
				}
			}
		}
	}
}

// ----------------------------------------------------------------
// Classify chunk as empty/solid
// ----------------------------------------------------------------
void UVoxelGenerator::ClassifyChunk(FVoxelChunk& Chunk)
{
	bool bAllNegative = true;
	bool bAllPositive = true;

	for (const float D : Chunk.Density)
	{
		if (D > 0.0f) bAllNegative = false;
		if (D <= 0.0f) bAllPositive = false;

		if (!bAllNegative && !bAllPositive) break;
	}

	Chunk.bIsEmpty = bAllNegative;
	Chunk.bIsSolid = bAllPositive;
}

// ----------------------------------------------------------------
// Accessors
// ----------------------------------------------------------------
FIntVector UVoxelGenerator::GetWorldVoxelDimensions() const
{
	return FIntVector(TotalVoxelsX, TotalVoxelsY, TotalVoxelsZ);
}

FVector UVoxelGenerator::GetWorldSizeCm() const
{
	return FVector(
		static_cast<float>(TotalVoxelsX) * Settings.VoxelSizeCm,
		static_cast<float>(TotalVoxelsY) * Settings.VoxelSizeCm,
		static_cast<float>(TotalVoxelsZ) * Settings.VoxelSizeCm);
}

const FVoxelChunk* UVoxelGenerator::GetChunkAtVoxel(int32 VX, int32 VY, int32 VZ) const
{
	const int32 CX = VX / Settings.ChunkResolution;
	const int32 CY = VY / Settings.ChunkResolution;
	const int32 CZ = VZ / Settings.ChunkResolution;

	if (CX < 0 || CX >= Settings.WorldChunksX ||
		CY < 0 || CY >= Settings.WorldChunksY ||
		CZ < 0 || CZ >= Settings.WorldChunksZ)
	{
		return nullptr;
	}

	return &Chunks[ChunkIndex(CX, CY, CZ)];
}

int32 UVoxelGenerator::ChunkIndex(int32 CX, int32 CY, int32 CZ) const
{
	return CZ * Settings.WorldChunksY * Settings.WorldChunksX
		 + CY * Settings.WorldChunksX
		 + CX;
}
