// BiomeDataGenerationActor.cpp
// Manual editor-only actor for Landmass + Biome + Heightmap preview generation

#include "BiomeDataGenerationActor.h"
#include "Engine/Texture2D.h"

ABiomeDataGenerationActor::ABiomeDataGenerationActor()
{
	PrimaryActorTick.bCanEverTick = false;

	// Initialize output pointers
	LandmassPreviewTexture = nullptr;
	BiomePreviewTexture = nullptr;

	// Create terrain mesh component directly on this actor
	TerrainMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("TerrainMesh"));
	RootComponent = TerrainMesh;
	TerrainMesh->bUseComplexAsSimpleCollision = true;
}

void ABiomeDataGenerationActor::GenerateLandmass()
{
	// Create generator instance
	ULandmassGenerator* Generator = NewObject<ULandmassGenerator>(this);
	if (!Generator)
	{
		UE_LOG(LogTemp, Error, TEXT("ABiomeDataGenerationActor: Failed to create ULandmassGenerator"));
		return;
	}

	// Initialize and generate
	Generator->Initialize(LandmassSettings);
	
	if (!Generator->Generate())
	{
		UE_LOG(LogTemp, Error, TEXT("ABiomeDataGenerationActor: Landmass generation failed"));
		return;
	}

	// Cache results
	CachedLandMask = Generator->GetLandMask();
	ActualLandmassSeedUsed = Generator->GetSeed();
	TextureResolutionUsed = Generator->GetTextureResolution();

	// Get preview texture from generator
	LandmassPreviewTexture = Generator->GetPreviewTexture();

	UE_LOG(LogTemp, Log, TEXT("ABiomeDataGenerationActor: Landmass generated - Seed: %d, Resolution: %d, LandPixels: %d, MapType: %d, MapSize: %d"),
		ActualLandmassSeedUsed, TextureResolutionUsed, CachedLandMask.Num(),
		static_cast<int32>(LandmassSettings.MapType), static_cast<int32>(LandmassSettings.MapSize));
}

void ABiomeDataGenerationActor::GenerateBiomes()
{
	// Check prerequisite
	if (CachedLandMask.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("ABiomeDataGenerationActor: GenerateLandmass first - no land mask available"));
		return;
	}

	// Sync resolution from landmass settings
	BiomeLayoutSettings.TextureResolution = LandmassSettings.TextureResolution;

	// Create generator instance
	UBiomeMapGenerator* Generator = NewObject<UBiomeMapGenerator>(this);
	if (!Generator)
	{
		UE_LOG(LogTemp, Error, TEXT("ABiomeDataGenerationActor: Failed to create UBiomeMapGenerator"));
		return;
	}

	// Initialize with our settings
	Generator->Initialize(BiomeLayoutSettings);

	// Generate biome map
	if (!Generator->Generate(CachedLandMask))
	{
		UE_LOG(LogTemp, Error, TEXT("ABiomeDataGenerationActor: Biome generation failed"));
		return;
	}

	// Cache results
	CachedBiomeMap = Generator->GetBiomeMap();
	ActualBiomeSeedUsed = Generator->GetBiomeSeed();

	// Build preview texture
	BuildBiomePreviewTexture();

	UE_LOG(LogTemp, Log, TEXT("ABiomeDataGenerationActor: Biomes generated - Seed: %d"), ActualBiomeSeedUsed);
}

void ABiomeDataGenerationActor::GenerateHeightmaps()
{
	// Check prerequisites
	if (CachedLandMask.Num() == 0 || CachedBiomeMap.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("ABiomeDataGenerationActor: Generate Landmass and Biomes first"));
		return;
	}

	// Sync resolution from landmass settings
	HeightmapSettings.TextureResolution = LandmassSettings.TextureResolution;

	// Create generator instance
	UHeightmapGenerator* Generator = NewObject<UHeightmapGenerator>(this);
	if (!Generator)
	{
		UE_LOG(LogTemp, Error, TEXT("ABiomeDataGenerationActor: Failed to create UHeightmapGenerator"));
		return;
	}

	// Initialize and generate
	Generator->Initialize(HeightmapSettings);

	if (!Generator->Generate(CachedBiomeMap, CachedLandMask, BiomeLayoutSettings.Layers))
	{
		UE_LOG(LogTemp, Error, TEXT("ABiomeDataGenerationActor: Heightmap generation failed"));
		return;
	}

	// Store results
	HeightmapResults = Generator->GetResults();
	ActualHeightmapSeedUsed = Generator->GetSeed();

	UE_LOG(LogTemp, Log, TEXT("ABiomeDataGenerationActor: Heightmaps generated - Seed: %d, Biomes: %d"),
		ActualHeightmapSeedUsed, HeightmapResults.Num());
}

void ABiomeDataGenerationActor::GenerateAll()
{
	GenerateLandmass();

	// Only continue if landmass was generated successfully
	if (CachedLandMask.Num() > 0)
	{
		GenerateBiomes();

		// Only continue if biomes were generated successfully
		if (CachedBiomeMap.Num() > 0)
		{
			GenerateHeightmaps();
		}
	}
}

void ABiomeDataGenerationActor::ClearGeneratedData()
{
	LandmassPreviewTexture = nullptr;
	BiomePreviewTexture = nullptr;
	HeightmapResults.Empty();
	CachedLandMask.Empty();
	CachedBiomeMap.Empty();
	ActualLandmassSeedUsed = 0;
	ActualBiomeSeedUsed = 0;
	ActualHeightmapSeedUsed = 0;
	TextureResolutionUsed = 0;

	// Clear the terrain mesh
	if (TerrainMesh)
	{
		TerrainMesh->ClearAllMeshSections();
	}

	UE_LOG(LogTemp, Log, TEXT("ABiomeDataGenerationActor: All generated data cleared"));
}

void ABiomeDataGenerationActor::GenerateMesh()
{
	// Check prerequisites
	if (CachedLandMask.Num() == 0 || CachedBiomeMap.Num() == 0 || HeightmapResults.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("ABiomeDataGenerationActor: Generate Landmass, Biomes, and Heightmaps first before generating mesh"));
		return;
	}

	const int32 Resolution = LandmassSettings.TextureResolution;
	const float WorldSizeCm = LandmassSettings.GetWorldSizeCm();
	const float MaxMapHeight = HeightmapSettings.MaxMapHeight;

	BuildTerrainMesh(
		CachedBiomeMap,
		CachedLandMask,
		HeightmapResults,
		BiomeLayoutSettings.Layers,
		BiomeLayoutSettings.OceanColor,
		Resolution,
		WorldSizeCm,
		MaxMapHeight);

	UE_LOG(LogTemp, Log, TEXT("ABiomeDataGenerationActor: Mesh generated - Resolution: %d, WorldSize: %.0f cm, MaxHeight: %.0f cm"),
		Resolution, WorldSizeCm, MaxMapHeight);
}

void ABiomeDataGenerationActor::GenerateSingleBiomeMesh()
{
	// Check prerequisites
	if (CachedLandMask.Num() == 0 || CachedBiomeMap.Num() == 0 || HeightmapResults.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("ABiomeDataGenerationActor: Generate Landmass, Biomes, and Heightmaps first before generating single biome mesh"));
		return;
	}

	// Find the heightmap result for the selected biome
	const FBiomeHeightmapResult* FoundResult = nullptr;
	for (const FBiomeHeightmapResult& Result : HeightmapResults)
	{
		if (Result.BiomeType == DebugBiomeFilter)
		{
			FoundResult = &Result;
			break;
		}
	}

	if (!FoundResult)
	{
		UE_LOG(LogTemp, Warning, TEXT("ABiomeDataGenerationActor: No heightmap found for biome type %d"), static_cast<int32>(DebugBiomeFilter));
		return;
	}

	// Build single-element array with only the selected biome's heightmap
	TArray<FBiomeHeightmapResult> SingleResult;
	SingleResult.Add(*FoundResult);

	const int32 Resolution = LandmassSettings.TextureResolution;
	const float WorldSizeCm = LandmassSettings.GetWorldSizeCm();
	const float MaxMapHeight = HeightmapSettings.MaxMapHeight;

	BuildTerrainMesh(
		CachedBiomeMap,
		CachedLandMask,
		SingleResult,
		BiomeLayoutSettings.Layers,
		BiomeLayoutSettings.OceanColor,
		Resolution,
		WorldSizeCm,
		MaxMapHeight);

	UE_LOG(LogTemp, Log, TEXT("ABiomeDataGenerationActor: Single biome mesh generated for '%s' - Resolution: %d, HeightScale: %.2f, MaxHeight: %.0f cm"),
		*FoundResult->DisplayName, Resolution, FoundResult->HeightScale, MaxMapHeight);
}

void ABiomeDataGenerationActor::BuildBiomePreviewTexture()
{
	const int32 Resolution = BiomeLayoutSettings.TextureResolution;
	const int32 TotalPixels = Resolution * Resolution;

	if (CachedBiomeMap.Num() != TotalPixels || CachedLandMask.Num() != TotalPixels)
	{
		UE_LOG(LogTemp, Error, TEXT("ABiomeDataGenerationActor: Data size mismatch for biome preview"));
		return;
	}

	// Build pixel data
	TArray<FColor> PixelData;
	PixelData.SetNumUninitialized(TotalPixels);

	// Convert ocean color once
	const FColor OceanColor = BiomeLayoutSettings.OceanColor.ToFColor(false);

	for (int32 i = 0; i < TotalPixels; ++i)
	{
		if (CachedLandMask[i] == 0)
		{
			// Ocean pixel
			PixelData[i] = OceanColor;
		}
		else
		{
			// Land pixel - get biome color
			const EBiomeType BiomeType = static_cast<EBiomeType>(CachedBiomeMap[i]);
			const FLinearColor BiomeColor = BiomeLayoutSettings.GetBiomeColor(BiomeType);
			PixelData[i] = BiomeColor.ToFColor(false);
		}
	}

	// Create texture
	BiomePreviewTexture = CreatePreviewTexture(Resolution, PixelData);
}

UTexture2D* ABiomeDataGenerationActor::CreatePreviewTexture(int32 Resolution, const TArray<FColor>& PixelData)
{
	if (Resolution <= 0 || PixelData.Num() != Resolution * Resolution)
	{
		return nullptr;
	}

	// Create transient texture
	UTexture2D* Texture = UTexture2D::CreateTransient(Resolution, Resolution, PF_B8G8R8A8);
	if (!Texture)
	{
		UE_LOG(LogTemp, Error, TEXT("ABiomeDataGenerationActor: Failed to create transient texture"));
		return nullptr;
	}

	// Configure texture settings
	Texture->MipGenSettings = TMGS_NoMipmaps;
	Texture->Filter = TF_Nearest;
	Texture->SRGB = false;
	Texture->CompressionSettings = TC_VectorDisplacementmap; // No compression

	// Lock and write pixel data
	FTexture2DMipMap& Mip = Texture->GetPlatformData()->Mips[0];
	void* Data = Mip.BulkData.Lock(LOCK_READ_WRITE);
	FMemory::Memcpy(Data, PixelData.GetData(), PixelData.Num() * sizeof(FColor));
	Mip.BulkData.Unlock();

	// Update GPU resource
	Texture->UpdateResource();

	return Texture;
}

// ------------------------------------------------------------
// Read grayscale pixel data from a heightmap texture
// ------------------------------------------------------------
bool ABiomeDataGenerationActor::ReadHeightmapTexture(UTexture2D* Texture, int32 Resolution, TArray<float>& OutHeights) const
{
	if (!Texture)
	{
		return false;
	}

	const int32 TotalPixels = Resolution * Resolution;
	OutHeights.SetNumZeroed(TotalPixels);

	FTexturePlatformData* PlatformData = Texture->GetPlatformData();
	if (!PlatformData || PlatformData->Mips.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("ABiomeDataGenerationActor: Texture has no platform data or mips"));
		return false;
	}

	FTexture2DMipMap& Mip = PlatformData->Mips[0];
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
// Build the terrain mesh directly on this actor's TerrainMesh component
// ------------------------------------------------------------
void ABiomeDataGenerationActor::BuildTerrainMesh(
	const TArray<int32>& BiomeMap,
	const TArray<uint8>& LandMask,
	const TArray<FBiomeHeightmapResult>& InHeightmapResults,
	const TArray<FBiomeLayerSettings>& BiomeLayers,
	const FLinearColor& OceanColor,
	int32 Resolution,
	float WorldSizeCm,
	float MaxMapHeight)
{
	const int32 TotalPixels = Resolution * Resolution;

	if (BiomeMap.Num() != TotalPixels || LandMask.Num() != TotalPixels)
	{
		UE_LOG(LogTemp, Error, TEXT("ABiomeDataGenerationActor: Data size mismatch - expected %d, BiomeMap=%d, LandMask=%d"),
			TotalPixels, BiomeMap.Num(), LandMask.Num());
		return;
	}

	// ---- Step 1: Read all heightmap textures into float arrays ----
	TMap<EBiomeType, TArray<float>> BiomeHeightData;
	for (const FBiomeHeightmapResult& Result : InHeightmapResults)
	{
		TArray<float> Heights;
		if (ReadHeightmapTexture(Result.HeightmapTexture, Resolution, Heights))
		{
			BiomeHeightData.Add(Result.BiomeType, MoveTemp(Heights));
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("ABiomeDataGenerationActor: Failed to read heightmap for biome %s"), *Result.DisplayName);
		}
	}

	// ---- Step 2: Composite heights per pixel ----
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

	// ---- Step 2b: Smooth composited heightmap ----
	// Apply box blur passes to remove sharp cliffs between biomes.
	// Each pass averages each pixel with its 8 neighbors.
	const int32 SmoothPasses = HeightmapSettings.SmoothingPasses;
	if (SmoothPasses > 0)
	{
		TArray<float> TempHeights;
		TempHeights.SetNumZeroed(TotalPixels);

		for (int32 Pass = 0; Pass < SmoothPasses; ++Pass)
		{
			for (int32 Y = 0; Y < Resolution; ++Y)
			{
				for (int32 X = 0; X < Resolution; ++X)
				{
					float Sum = 0.0f;
					float Count = 0.0f;

					for (int32 DY = -1; DY <= 1; ++DY)
					{
						for (int32 DX = -1; DX <= 1; ++DX)
						{
							const int32 NX = X + DX;
							const int32 NY = Y + DY;
							if (NX >= 0 && NX < Resolution && NY >= 0 && NY < Resolution)
							{
								Sum += CompositeHeights[NY * Resolution + NX];
								Count += 1.0f;
							}
						}
					}

					TempHeights[Y * Resolution + X] = Sum / Count;
				}
			}

			// Swap buffers for next pass
			Swap(CompositeHeights, TempHeights);
		}
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

	for (int32 Y = 0; Y < Resolution; ++Y)
	{
		for (int32 X = 0; X < Resolution; ++X)
		{
			const int32 Index = Y * Resolution + X;

			const float WorldX = static_cast<float>(X) * CellSize;
			const float WorldY = static_cast<float>(Y) * CellSize;
			const float WorldZ = CompositeHeights[Index] * MaxMapHeight;

			Vertices[Index] = FVector(WorldX, WorldY, WorldZ);

			UVs[Index] = FVector2D(
				static_cast<float>(X) / static_cast<float>(Resolution - 1),
				static_cast<float>(Y) / static_cast<float>(Resolution - 1));

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

			Triangles[TriIdx++] = TopLeft;
			Triangles[TriIdx++] = BottomLeft;
			Triangles[TriIdx++] = TopRight;

			Triangles[TriIdx++] = TopRight;
			Triangles[TriIdx++] = BottomLeft;
			Triangles[TriIdx++] = BottomRight;
		}
	}

	// ---- Step 5: Compute normals ----
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

	for (int32 i = 0; i < VertexCount; ++i)
	{
		Normals[i] = Normals[i].GetSafeNormal();
	}

	// ---- Step 6: Create the mesh section ----
	if (!TerrainMesh)
	{
		UE_LOG(LogTemp, Error, TEXT("ABiomeDataGenerationActor: TerrainMesh component is null"));
		return;
	}

	TerrainMesh->ClearAllMeshSections();

	TArray<FProcMeshTangent> Tangents;
	TerrainMesh->CreateMeshSection(0, Vertices, Triangles, Normals, UVs, VertexColors, Tangents, true);

	UE_LOG(LogTemp, Log, TEXT("ABiomeDataGenerationActor: Terrain mesh built - %d vertices, %d triangles, WorldSize=%.0f, MaxHeight=%.0f"),
		VertexCount, TriangleCount, WorldSizeCm, MaxMapHeight);
}
