// VoxelTypes.h
// Data structures and settings for the voxel terrain generation pipeline.
// Defines chunk layout, material types, and generation parameters.

#pragma once

#include "CoreMinimal.h"
#include "VoxelTypes.generated.h"

// ==================== Voxel Material ====================

/** Material type stored per voxel. */
UENUM(BlueprintType)
enum class EVoxelMaterial : uint8
{
	Air       UMETA(DisplayName = "Air"),
	Stone     UMETA(DisplayName = "Stone"),
	Dirt      UMETA(DisplayName = "Dirt"),
	Grass     UMETA(DisplayName = "Grass"),
	Sand      UMETA(DisplayName = "Sand"),
	Snow      UMETA(DisplayName = "Snow"),
	Ice       UMETA(DisplayName = "Ice"),
	Water     UMETA(DisplayName = "Water"),
	Lava      UMETA(DisplayName = "Lava"),
	Bedrock   UMETA(DisplayName = "Bedrock")
};

// ==================== Voxel Chunk ====================

/**
 * A single chunk of the voxel world.
 * Stores a 3D density field and material assignments.
 * Density > 0 = solid, density <= 0 = air.
 */
USTRUCT(BlueprintType)
struct RANDOMLANDSCAPE_5_7_API FVoxelChunk
{
	GENERATED_BODY()

	/** Chunk coordinates in chunk-space (not world-space). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FIntVector ChunkCoord = FIntVector::ZeroValue;

	/** Voxels per axis in this chunk. */
	int32 Resolution = 0;

	/** Flat array of density values: Index = Z * P*P + Y * P + X, where P = Resolution + 1.
	 *  Stores (Resolution+1)³ values to include a 1-voxel padding layer for seamless
	 *  Marching Cubes extraction across chunk boundaries.
	 *  Positive = solid, Negative/zero = air. */
	TArray<float> Density;

	/** Flat array of material IDs per voxel (same indexing as Density). */
	TArray<uint8> Materials;

	/** Is this chunk fully empty (all air)? Skip meshing. */
	bool bIsEmpty = true;

	/** Is this chunk fully solid? Can skip interior faces. */
	bool bIsSolid = false;

	void Init(FIntVector InCoord, int32 InResolution)
	{
		ChunkCoord = InCoord;
		Resolution = InResolution;
		const int32 Padded = Resolution + 1;
		const int32 Total = Padded * Padded * Padded;
		Density.SetNumZeroed(Total);
		Materials.SetNumZeroed(Total);
		bIsEmpty = true;
		bIsSolid = false;
	}

	FORCEINLINE int32 Index(int32 X, int32 Y, int32 Z) const
	{
		const int32 P = Resolution + 1;
		return Z * P * P + Y * P + X;
	}

	FORCEINLINE float GetDensity(int32 X, int32 Y, int32 Z) const
	{
		return Density[Index(X, Y, Z)];
	}

	FORCEINLINE void SetDensity(int32 X, int32 Y, int32 Z, float Value)
	{
		Density[Index(X, Y, Z)] = Value;
	}

	FORCEINLINE EVoxelMaterial GetMaterial(int32 X, int32 Y, int32 Z) const
	{
		return static_cast<EVoxelMaterial>(Materials[Index(X, Y, Z)]);
	}

	FORCEINLINE void SetMaterial(int32 X, int32 Y, int32 Z, EVoxelMaterial Mat)
	{
		Materials[Index(X, Y, Z)] = static_cast<uint8>(Mat);
	}
};

// ==================== Voxel Generation Settings ====================

/** Settings for the 3D voxel terrain generator. */
USTRUCT(BlueprintType)
struct RANDOMLANDSCAPE_5_7_API FVoxelGenerationSettings
{
	GENERATED_BODY()

	// --- World Dimensions ---

	/** Number of chunks along each horizontal axis. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|World", meta = (ClampMin = "1", ClampMax = "64"))
	int32 WorldChunksX = 4;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|World", meta = (ClampMin = "1", ClampMax = "64"))
	int32 WorldChunksY = 4;

	/** Number of chunks along the vertical axis. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|World", meta = (ClampMin = "1", ClampMax = "32"))
	int32 WorldChunksZ = 4;

	/** Voxels per axis per chunk (e.g., 32 means 32×32×32 voxels per chunk). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|World", meta = (ClampMin = "8", ClampMax = "64"))
	int32 ChunkResolution = 32;

	/** World-space size of each voxel in centimeters. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|World", meta = (ClampMin = "10.0", ClampMax = "500.0"))
	float VoxelSizeCm = 100.0f;

	// --- Terrain Shape ---

	/** Base terrain height as a fraction of total world height [0,1]. Terrain surface sits around this level. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Terrain", meta = (ClampMin = "0.1", ClampMax = "0.9"))
	float BaseTerrainHeight = 0.5f;

	/** Height variation amplitude. Higher = more dramatic hills/mountains. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Terrain", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float TerrainAmplitude = 0.3f;

	/** Primary frequency for the terrain heightfield noise. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Terrain", meta = (ClampMin = "0.001", ClampMax = "0.1"))
	float TerrainFrequency = 0.008f;

	/** FBM octave count for the terrain heightfield. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Terrain", meta = (ClampMin = "1", ClampMax = "10"))
	int32 TerrainOctaves = 6;

	/** FBM gain (persistence) for terrain heightfield. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Terrain", meta = (ClampMin = "0.1", ClampMax = "0.9"))
	float TerrainGain = 0.5f;

	/** Domain warp amplitude for organic-looking terrain (0 = no warp). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Terrain", meta = (ClampMin = "0.0", ClampMax = "100.0"))
	float DomainWarpAmplitude = 30.0f;

	// --- 3D Features (Caves, Overhangs) ---

	/** Enable 3D noise features (caves, overhangs, arches). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|3D Features")
	bool bEnable3DFeatures = true;

	/** Strength of 3D noise carving. Higher = more caves/overhangs. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|3D Features", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float CaveStrength = 0.5f;

	/** Frequency for 3D cave/overhang noise. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|3D Features", meta = (ClampMin = "0.001", ClampMax = "0.1"))
	float CaveFrequency = 0.015f;

	/** FBM octave count for 3D features. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|3D Features", meta = (ClampMin = "1", ClampMax = "8"))
	int32 CaveOctaves = 4;

	/** Threshold below which cave noise carves out terrain. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|3D Features", meta = (ClampMin = "-1.0", ClampMax = "1.0"))
	float CaveThreshold = -0.3f;

	/** Minimum depth (fraction of world height) below which caves can form.
	 *  Prevents surface-level cave openings from being too common. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|3D Features", meta = (ClampMin = "0.0", ClampMax = "0.5"))
	float CaveMinDepth = 0.05f;

	// --- Mountains (Ridged Noise) ---

	/** Enable ridged-noise mountain ridges on top of base terrain. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Mountains")
	bool bEnableMountains = true;

	/** Amplitude of mountain ridges relative to world height. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Mountains", meta = (ClampMin = "0.0", ClampMax = "0.5"))
	float MountainAmplitude = 0.15f;

	/** Frequency of mountain ridge noise. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Mountains", meta = (ClampMin = "0.001", ClampMax = "0.05"))
	float MountainFrequency = 0.005f;

	/** Octaves for mountain ridged noise. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Mountains", meta = (ClampMin = "1", ClampMax = "8"))
	int32 MountainOctaves = 5;

	// --- Sea Level ---

	/** Sea level as a fraction of total world height. Voxels below this in air get filled with water. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Water", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SeaLevel = 0.35f;

	/** Whether to fill below sea level with water voxels. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Water")
	bool bEnableWater = true;

	// --- Material Layers ---

	/** Depth of grass/surface layer in voxels. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Materials", meta = (ClampMin = "0", ClampMax = "8"))
	int32 GrassLayerDepth = 1;

	/** Depth of dirt layer below grass in voxels. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Materials", meta = (ClampMin = "0", ClampMax = "16"))
	int32 DirtLayerDepth = 4;

	/** Height (fraction of world) above which surface becomes snow. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Materials", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SnowLineHeight = 0.75f;

	/** Height (fraction of world) below which beach sand appears near sea level. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Materials", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float BeachHeight = 0.38f;
};
