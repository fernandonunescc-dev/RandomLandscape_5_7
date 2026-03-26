// LandscapeMeshActor.cpp
// Procedural mesh actor that builds terrain geometry from biome heightmap data

#include "LandscapeMeshActor.h"
#include "Engine/Texture2D.h"

ALandscapeMeshActor::ALandscapeMeshActor()
{
	PrimaryActorTick.bCanEverTick = false;

	TerrainMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("TerrainMesh"));
	RootComponent = TerrainMesh;

	// Enable collision for the generated mesh
	TerrainMesh->bUseComplexAsSimpleCollision = true;
}

// ------------------------------------------------------------
// Read grayscale pixel data from a heightmap texture
// ------------------------------------------------------------
bool ALandscapeMeshActor::ReadHeightmapTexture(UTexture2D* Texture, int32 Resolution, TArray<float>& OutHeights) const
{
	if (!Texture)
	{
		return false;
	}

	const int32 TotalPixels = Resolution * Resolution;
	OutHeights.SetNumZeroed(TotalPixels);

	FTexture2DMipMap& Mip = Texture->GetPlatformData()->Mips[0];
	const void* Data = Mip.BulkData.LockReadOnly();
	if (!Data)
	{
		Mip.BulkData.Unlock();
		return false;
	}

	const uint8* Pixels = static_cast<const uint8*>(Data);
	for (int32 i = 0; i < TotalPixels; ++i)
	{
		// BGRA8 format - read R channel (index 2) for grayscale value
		const int32 PixelIdx = i * 4;
		OutHeights[i] = static_cast<float>(Pixels[PixelIdx + 2]) / 255.0f;
	}

	Mip.BulkData.Unlock();
	return true;
}

// ------------------------------------------------------------
// Build the terrain mesh
// ------------------------------------------------------------
void ALandscapeMeshActor::BuildMesh(
	const TArray<int32>& BiomeMap,
	const TArray<uint8>& LandMask,
	const TArray<FBiomeHeightmapResult>& HeightmapResults,
	const TArray<FBiomeLayerSettings>& BiomeLayers,
	const FLinearColor& OceanColor,
	int32 Resolution,
	float WorldSizeCm,
	float MaxMapHeight)
{
	const int32 TotalPixels = Resolution * Resolution;

	if (BiomeMap.Num() != TotalPixels || LandMask.Num() != TotalPixels)
	{
		UE_LOG(LogTemp, Error, TEXT("LandscapeMeshActor: Data size mismatch - expected %d, BiomeMap=%d, LandMask=%d"),
			TotalPixels, BiomeMap.Num(), LandMask.Num());
		return;
	}

	// ---- Step 1: Read all heightmap textures into float arrays ----
	// Build a map from EBiomeType -> float height array
	TMap<EBiomeType, TArray<float>> BiomeHeightData;
	for (const FBiomeHeightmapResult& Result : HeightmapResults)
	{
		TArray<float> Heights;
		if (ReadHeightmapTexture(Result.HeightmapTexture, Resolution, Heights))
		{
			BiomeHeightData.Add(Result.BiomeType, MoveTemp(Heights));
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("LandscapeMeshActor: Failed to read heightmap for biome %s"), *Result.DisplayName);
		}
	}

	// ---- Step 2: Composite heights per pixel ----
	// For each pixel, take the maximum height across all biome heightmaps
	// (each biome heightmap has non-zero values only within its region + blend zone)
	TArray<float> CompositeHeights;
	CompositeHeights.SetNumZeroed(TotalPixels);

	for (int32 i = 0; i < TotalPixels; ++i)
	{
		float MaxH = 0.0f;
		for (const auto& Pair : BiomeHeightData)
		{
			MaxH = FMath::Max(MaxH, Pair.Value[i]);
		}
		CompositeHeights[i] = MaxH;
	}

	// ---- Step 3: Build color lookup from biome layers ----
	TMap<EBiomeType, FColor> BiomeColorMap;
	for (const FBiomeLayerSettings& Layer : BiomeLayers)
	{
		BiomeColorMap.Add(Layer.BiomeType, Layer.Color.ToFColor(false));
	}
	const FColor OceanFColor = OceanColor.ToFColor(false);

	// ---- Step 4: Generate mesh data ----
	const int32 VertexCount = TotalPixels;
	const int32 TriangleCount = (Resolution - 1) * (Resolution - 1) * 2;

	TArray<FVector> Vertices;
	TArray<int32> Triangles;
	TArray<FVector> Normals;
	TArray<FVector2D> UVs;
	TArray<FColor> VertexColors;

	Vertices.SetNumUninitialized(VertexCount);
	Normals.SetNumZeroed(VertexCount);
	UVs.SetNumUninitialized(VertexCount);
	VertexColors.SetNumUninitialized(VertexCount);
	Triangles.SetNumUninitialized(TriangleCount * 3);

	const float CellSize = WorldSizeCm / static_cast<float>(Resolution - 1);

	// Generate vertices, UVs, and colors
	for (int32 Y = 0; Y < Resolution; ++Y)
	{
		for (int32 X = 0; X < Resolution; ++X)
		{
			const int32 Index = Y * Resolution + X;

			// Position: X/Y on the ground plane, Z from composite height
			const float WorldX = static_cast<float>(X) * CellSize;
			const float WorldY = static_cast<float>(Y) * CellSize;
			const float WorldZ = CompositeHeights[Index] * MaxMapHeight;

			Vertices[Index] = FVector(WorldX, WorldY, WorldZ);

			// UV coordinates
			UVs[Index] = FVector2D(
				static_cast<float>(X) / static_cast<float>(Resolution - 1),
				static_cast<float>(Y) / static_cast<float>(Resolution - 1));

			// Vertex color from biome type
			if (LandMask[Index] == 0)
			{
				VertexColors[Index] = OceanFColor;
			}
			else
			{
				const EBiomeType BiomeType = static_cast<EBiomeType>(BiomeMap[Index]);
				const FColor* FoundColor = BiomeColorMap.Find(BiomeType);
				VertexColors[Index] = FoundColor ? *FoundColor : FColor::White;
			}
		}
	}

	// Generate triangle indices (two triangles per grid cell)
	int32 TriIdx = 0;
	for (int32 Y = 0; Y < Resolution - 1; ++Y)
	{
		for (int32 X = 0; X < Resolution - 1; ++X)
		{
			const int32 TopLeft = Y * Resolution + X;
			const int32 TopRight = TopLeft + 1;
			const int32 BottomLeft = (Y + 1) * Resolution + X;
			const int32 BottomRight = BottomLeft + 1;

			// Triangle 1 (top-left, bottom-left, top-right)
			Triangles[TriIdx++] = TopLeft;
			Triangles[TriIdx++] = BottomLeft;
			Triangles[TriIdx++] = TopRight;

			// Triangle 2 (top-right, bottom-left, bottom-right)
			Triangles[TriIdx++] = TopRight;
			Triangles[TriIdx++] = BottomLeft;
			Triangles[TriIdx++] = BottomRight;
		}
	}

	// ---- Step 5: Compute normals ----
	// Accumulate face normals into vertices
	for (int32 i = 0; i < TriangleCount; ++i)
	{
		const int32 I0 = Triangles[i * 3 + 0];
		const int32 I1 = Triangles[i * 3 + 1];
		const int32 I2 = Triangles[i * 3 + 2];

		const FVector Edge1 = Vertices[I1] - Vertices[I0];
		const FVector Edge2 = Vertices[I2] - Vertices[I0];
		const FVector FaceNormal = FVector::CrossProduct(Edge1, Edge2);

		Normals[I0] += FaceNormal;
		Normals[I1] += FaceNormal;
		Normals[I2] += FaceNormal;
	}

	// Normalize
	for (int32 i = 0; i < VertexCount; ++i)
	{
		Normals[i] = Normals[i].GetSafeNormal();
	}

	// ---- Step 6: Create the mesh section ----
	ClearMesh();

	TArray<FProcMeshTangent> Tangents; // Empty - will be auto-computed
	TerrainMesh->CreateMeshSection(0, Vertices, Triangles, Normals, UVs, VertexColors, Tangents, true);

	UE_LOG(LogTemp, Log, TEXT("LandscapeMeshActor: Mesh generated - %d vertices, %d triangles, WorldSize=%.0f, MaxHeight=%.0f"),
		VertexCount, TriangleCount, WorldSizeCm, MaxMapHeight);
}

// ------------------------------------------------------------
// Clear mesh
// ------------------------------------------------------------
void ALandscapeMeshActor::ClearMesh()
{
	if (TerrainMesh)
	{
		TerrainMesh->ClearAllMeshSections();
	}
}
