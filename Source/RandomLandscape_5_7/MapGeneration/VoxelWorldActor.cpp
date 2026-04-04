// VoxelWorldActor.cpp
// Voxel world generation pipeline orchestrator implementation.

#include "VoxelWorldActor.h"
#include "VoxelGenerator.h"
#include "MarchingCubes.h"
#include "Engine/Texture2D.h"

// ----------------------------------------------------------------
// Constructor
// ----------------------------------------------------------------
AVoxelWorldActor::AVoxelWorldActor()
{
	PrimaryActorTick.bCanEverTick = false;

	VoxelRoot = CreateDefaultSubobject<USceneComponent>(TEXT("VoxelRoot"));
	RootComponent = VoxelRoot;
}

// ----------------------------------------------------------------
// Generate
// ----------------------------------------------------------------
void AVoxelWorldActor::GenerateVoxelWorld()
{
	ClearAll();

	UVoxelGenerator* Generator = NewObject<UVoxelGenerator>(this);
	if (!Generator)
	{
		UE_LOG(LogTemp, Error, TEXT("AVoxelWorldActor: Failed to create UVoxelGenerator"));
		return;
	}

	Generator->Initialize(VoxelSettings, GlobalSeed);

	const double StartTime = FPlatformTime::Seconds();

	if (!Generator->Generate())
	{
		UE_LOG(LogTemp, Error, TEXT("AVoxelWorldActor: Voxel generation failed"));
		return;
	}

	const double GenTime = FPlatformTime::Seconds() - StartTime;
	UE_LOG(LogTemp, Log, TEXT("AVoxelWorldActor: Voxel generation took %.3f seconds"), GenTime);

	// Update debug stats
	WorldSizeCm = Generator->GetWorldSizeCm();

	// Build meshes
	BuildChunkMeshes(Generator);

	// Create debug textures
	CreateDebugTextures(Generator);
}

// ----------------------------------------------------------------
// Build chunk meshes via Marching Cubes
// ----------------------------------------------------------------
void AVoxelWorldActor::BuildChunkMeshes(UVoxelGenerator* Generator)
{
	const TArray<FVoxelChunk>& Chunks = Generator->GetChunks();
	const float VoxelSize = VoxelSettings.VoxelSizeCm;

	TotalChunks = Chunks.Num();
	EmptyChunks = 0;
	SolidChunks = 0;
	MeshedChunks = 0;
	TotalVertices = 0;
	TotalTriangles = 0;

	const double MeshStartTime = FPlatformTime::Seconds();

	for (int32 i = 0; i < Chunks.Num(); ++i)
	{
		const FVoxelChunk& Chunk = Chunks[i];

		if (Chunk.bIsEmpty)
		{
			++EmptyChunks;
			continue;
		}
		if (Chunk.bIsSolid)
		{
			++SolidChunks;
			continue;
		}

		// Extract mesh via Marching Cubes
		FMarchingCubes::FMeshData MeshData;
		FMarchingCubes::Extract(Chunk, VoxelSize, IsoLevel, MeshData);

		if (MeshData.Vertices.Num() == 0)
		{
			continue;
		}

		// Create a ProceduralMeshComponent for this chunk
		UProceduralMeshComponent* ChunkMesh = NewObject<UProceduralMeshComponent>(this,
			*FString::Printf(TEXT("ChunkMesh_%d_%d_%d"), Chunk.ChunkCoord.X, Chunk.ChunkCoord.Y, Chunk.ChunkCoord.Z));

		if (!ChunkMesh)
		{
			continue;
		}

		ChunkMesh->SetupAttachment(VoxelRoot);
		ChunkMesh->RegisterComponent();
		ChunkMesh->bUseComplexAsSimpleCollision = true;

		TArray<FProcMeshTangent> Tangents;
		ChunkMesh->CreateMeshSection(0,
			MeshData.Vertices, MeshData.Triangles, MeshData.Normals,
			MeshData.UVs, MeshData.VertexColors, Tangents, true);

		if (VoxelMaterial)
		{
			ChunkMesh->SetMaterial(0, VoxelMaterial);
		}

		ChunkMeshes.Add(ChunkMesh);

		TotalVertices += MeshData.Vertices.Num();
		TotalTriangles += MeshData.Triangles.Num() / 3;
		++MeshedChunks;
	}

	const double MeshTime = FPlatformTime::Seconds() - MeshStartTime;

	UE_LOG(LogTemp, Log,
		TEXT("AVoxelWorldActor: Meshing complete — %d chunks (%d meshed, %d empty, %d solid), %d verts, %d tris, %.3fs"),
		TotalChunks, MeshedChunks, EmptyChunks, SolidChunks, TotalVertices, TotalTriangles, MeshTime);
}

// ----------------------------------------------------------------
// Debug textures
// ----------------------------------------------------------------
void AVoxelWorldActor::CreateDebugTextures(UVoxelGenerator* Generator)
{
	const FIntVector Dims = Generator->GetWorldVoxelDimensions();
	const int32 SizeX = Dims.X;
	const int32 SizeY = Dims.Y;
	const int32 SizeZ = Dims.Z;

	// Heightfield preview (top-down grayscale)
	{
		TArray<FColor> Pixels;
		Pixels.SetNumUninitialized(SizeX * SizeY);

		// Access chunks to read heightfield indirectly via density at each column
		// We'll sample the density at the midpoint Z for a quick preview
		for (int32 Y = 0; Y < SizeY; ++Y)
		{
			for (int32 X = 0; X < SizeX; ++X)
			{
				// Find the highest solid voxel in this column
				float MaxSolidHeight = 0.0f;
				for (int32 Z = SizeZ - 1; Z >= 0; --Z)
				{
					const FVoxelChunk* Chunk = Generator->GetChunkAtVoxel(X, Y, Z);
					if (Chunk)
					{
						const int32 LX = X % VoxelSettings.ChunkResolution;
						const int32 LY = Y % VoxelSettings.ChunkResolution;
						const int32 LZ = Z % VoxelSettings.ChunkResolution;
						if (Chunk->GetDensity(LX, LY, LZ) > 0.0f)
						{
							MaxSolidHeight = static_cast<float>(Z) / static_cast<float>(SizeZ);
							break;
						}
					}
				}

				const uint8 Val = static_cast<uint8>(FMath::Clamp(MaxSolidHeight * 255.0f, 0.0f, 255.0f));
				Pixels[Y * SizeX + X] = FColor(Val, Val, Val, 255);
			}
		}

		Debug_Heightfield = CreateColorTexture(FMath::Max(SizeX, SizeY), Pixels);
	}

	// Cross-section at mid-Z
	{
		const int32 MidZ = SizeZ / 2;
		TArray<FColor> Pixels;
		Pixels.SetNumUninitialized(SizeX * SizeY);

		for (int32 Y = 0; Y < SizeY; ++Y)
		{
			for (int32 X = 0; X < SizeX; ++X)
			{
				const FVoxelChunk* Chunk = Generator->GetChunkAtVoxel(X, Y, MidZ);
				FColor C(0, 0, 0, 255);
				if (Chunk)
				{
					const int32 LX = X % VoxelSettings.ChunkResolution;
					const int32 LY = Y % VoxelSettings.ChunkResolution;
					const int32 LZ = MidZ % VoxelSettings.ChunkResolution;
					const float D = Chunk->GetDensity(LX, LY, LZ);

					if (D > 0.0f)
					{
						const EVoxelMaterial Mat = Chunk->GetMaterial(LX, LY, LZ);
						switch (Mat)
						{
						case EVoxelMaterial::Grass:   C = FColor(80, 160, 60, 255);  break;
						case EVoxelMaterial::Dirt:    C = FColor(140, 100, 60, 255);  break;
						case EVoxelMaterial::Stone:   C = FColor(130, 130, 130, 255); break;
						case EVoxelMaterial::Sand:    C = FColor(210, 200, 140, 255); break;
						case EVoxelMaterial::Snow:    C = FColor(240, 245, 255, 255); break;
						case EVoxelMaterial::Water:   C = FColor(30, 80, 200, 255);   break;
						case EVoxelMaterial::Bedrock: C = FColor(50, 50, 50, 255);    break;
						default:                     C = FColor(128, 128, 128, 255); break;
						}
					}
					else if (D <= 0.0f && Chunk->GetMaterial(LX, LY, LZ) == EVoxelMaterial::Water)
					{
						C = FColor(30, 80, 200, 128);
					}
				}

				Pixels[Y * SizeX + X] = C;
			}
		}

		Debug_CrossSection = CreateColorTexture(FMath::Max(SizeX, SizeY), Pixels);
	}
}

UTexture2D* AVoxelWorldActor::CreateColorTexture(int32 Res, const TArray<FColor>& Pixels)
{
	if (Res <= 0 || Pixels.Num() < Res * Res)
	{
		return nullptr;
	}

	UTexture2D* Texture = UTexture2D::CreateTransient(Res, Res, PF_B8G8R8A8);
	if (!Texture)
	{
		return nullptr;
	}

	Texture->MipGenSettings = TMGS_NoMipmaps;
	Texture->Filter = TF_Nearest;
	Texture->SRGB = false;
	Texture->CompressionSettings = TC_VectorDisplacementmap;

	FTexturePlatformData* PlatformData = Texture->GetPlatformData();
	if (!PlatformData || PlatformData->Mips.Num() == 0)
	{
		return nullptr;
	}

	FTexture2DMipMap& Mip = PlatformData->Mips[0];
	void* Data = Mip.BulkData.Lock(LOCK_READ_WRITE);
	const int32 CopySize = FMath::Min(Pixels.Num(), Res * Res) * static_cast<int32>(sizeof(FColor));
	FMemory::Memcpy(Data, Pixels.GetData(), CopySize);
	Mip.BulkData.Unlock();

	Texture->UpdateResource();

	return Texture;
}

// ----------------------------------------------------------------
// Clear all
// ----------------------------------------------------------------
void AVoxelWorldActor::ClearAll()
{
	for (UProceduralMeshComponent* Mesh : ChunkMeshes)
	{
		if (Mesh)
		{
			Mesh->ClearAllMeshSections();
			Mesh->DestroyComponent();
		}
	}
	ChunkMeshes.Empty();

	Debug_Heightfield = nullptr;
	Debug_CrossSection = nullptr;

	TotalChunks = 0;
	EmptyChunks = 0;
	SolidChunks = 0;
	MeshedChunks = 0;
	TotalVertices = 0;
	TotalTriangles = 0;
	WorldSizeCm = FVector::ZeroVector;

	UE_LOG(LogTemp, Log, TEXT("AVoxelWorldActor: Cleared all voxel data"));
}

// ----------------------------------------------------------------
// Randomize
// ----------------------------------------------------------------
void AVoxelWorldActor::Randomize()
{
	GlobalSeed = FMath::RandRange(1, 0x7FFFFFFF);
	UE_LOG(LogTemp, Log, TEXT("AVoxelWorldActor: Randomized seed to %d"), GlobalSeed);
	GenerateVoxelWorld();
}

// ----------------------------------------------------------------
// Auto-regeneration
// ----------------------------------------------------------------
#if WITH_EDITOR
void AVoxelWorldActor::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (!bAutoRegenerate) return;
	if (PropertyChangedEvent.ChangeType == EPropertyChangeType::Interactive) return;

	const FName MemberName = PropertyChangedEvent.MemberProperty
		? PropertyChangedEvent.MemberProperty->GetFName()
		: NAME_None;

	if (MemberName == NAME_None) return;

	// Regenerate on any relevant property change
	if (MemberName == GET_MEMBER_NAME_CHECKED(AVoxelWorldActor, GlobalSeed)
		|| MemberName == GET_MEMBER_NAME_CHECKED(AVoxelWorldActor, VoxelSettings)
		|| MemberName == GET_MEMBER_NAME_CHECKED(AVoxelWorldActor, IsoLevel)
		|| MemberName == GET_MEMBER_NAME_CHECKED(AVoxelWorldActor, VoxelMaterial))
	{
		GenerateVoxelWorld();
	}
}
#endif
