// CaveGenerator.h
// Stage 8: Cave Generation — 3D noise-based cave carving beneath the terrain surface.
//
// Samples 3D FBM noise in a volumetric grid below the heightmap surface to
// determine where caves exist. The density field is then converted to triangle
// meshes using the Marching Cubes algorithm. Cave meshes are added as additional
// ProceduralMeshComponent sections alongside the heightmap terrain (section 0).

#pragma once

#include "CoreMinimal.h"
#include "WorldGenTypes.h"
#include "CaveGenerator.generated.h"

/** Output mesh data for a single cave mesh section. */
struct FCaveMeshData
{
TArray<FVector> Vertices;
TArray<int32> Triangles;
TArray<FVector> Normals;
TArray<FVector2D> UVs;
TArray<FColor> VertexColors;
};

/**
 * Stage 8: Cave generator.
 *
 * Generates underground cave meshes by:
 *   1. Building a 3D density field (ResX x ResY x DepthLayers) below the terrain
 *   2. Marking voxels as air where 3D FBM noise exceeds the cave threshold
 *   3. Running Marching Cubes on the density field to extract iso-surface meshes
 *   4. Converting results to FCaveMeshData for the mesh builder
 */
UCLASS(BlueprintType)
class RANDOMLANDSCAPE_5_7_API UCaveGenerator : public UObject
{
GENERATED_BODY()

public:

/**
 * Store settings and derive the actual seed.
 *
 * @param InSettings         Cave configuration
 * @param GlobalSeed         Pipeline-wide seed used when Settings.Seed == 0
 * @param TextureResolution  Width/height of the terrain data arrays
 */
void Initialize(const FCaveSettings& InSettings, int32 GlobalSeed, int32 TextureResolution);

/**
 * Generate cave density field and extract mesh data.
 *
 * @param FinalElevation  Normalised elevation [0,1] from Stage 7
 * @param LandMask        Binary mask (1 = land, 0 = ocean) from Stage 1
 * @param WorldSizeCm     World dimensions in centimetres
 * @param MaxHeightCm     Maximum terrain height in centimetres
 * @return true on success
 */
bool Generate(const TArray<float>& FinalElevation, const TArray<uint8>& LandMask,
float WorldSizeCm, float MaxHeightCm);

/** Resulting cave mesh data. */
const FCaveMeshData& GetCaveMesh() const { return CaveMesh; }

/** Whether any cave triangles were generated. */
bool HasCaves() const { return CaveMesh.Triangles.Num() > 0; }

/** 2D cave presence map (TerrainResolution^2), 0-1 indicating fraction of depth that is cave. */
const TArray<float>& GetCavePresenceMap() const { return CavePresenceMap; }

/** The actual seed used for generation. */
int32 GetSeed() const { return ActualSeed; }

private:

//--------------------------------------------------------------------------
// Configuration
//--------------------------------------------------------------------------
FCaveSettings Settings;
int32 ActualSeed = 0;
int32 TerrainResolution = 0;

//--------------------------------------------------------------------------
// Output
//--------------------------------------------------------------------------
FCaveMeshData CaveMesh;
TArray<float> CavePresenceMap;

//--------------------------------------------------------------------------
// 3D density field
//--------------------------------------------------------------------------
TArray<float> DensityField;
int32 CaveResX = 0;
int32 CaveResY = 0;
int32 CaveResZ = 0;

/** Get density value at (x, y, z) in the 3D grid. */
FORCEINLINE float GetDensity(int32 X, int32 Y, int32 Z) const
{
return DensityField[X + Y * CaveResX + Z * CaveResX * CaveResY];
}

/** Set density value at (x, y, z) in the 3D grid. */
FORCEINLINE void SetDensity(int32 X, int32 Y, int32 Z, float Value)
{
DensityField[X + Y * CaveResX + Z * CaveResX * CaveResY] = Value;
}

//--------------------------------------------------------------------------
// Internal pipeline
//--------------------------------------------------------------------------

/** Build the 3D density field from surface elevation + 3D noise. */
void BuildDensityField(const TArray<float>& FinalElevation, const TArray<uint8>& LandMask,
float WorldSizeCm, float MaxHeightCm);

/** Run Marching Cubes on the density field to produce cave mesh geometry. */
void ExtractMeshMarchingCubes(float WorldSizeCm, float MaxHeightCm,
const TArray<float>& FinalElevation);

/** Build the 2D cave presence debug map. */
void BuildCavePresenceMap();

//--------------------------------------------------------------------------
// Marching Cubes tables
//--------------------------------------------------------------------------
static const int32 EdgeTable[256];
static const int32 TriTable[256][16];
};
