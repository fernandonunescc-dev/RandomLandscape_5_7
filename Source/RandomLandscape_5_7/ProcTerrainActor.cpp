// Fill out your copyright notice in the Description page of Project Settings.


#include "ProcTerrainActor.h"

AProcTerrainActor::AProcTerrainActor()
{
	PrimaryActorTick.bCanEverTick = false;

	ProcMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("ProcMesh"));
	SetRootComponent(ProcMesh);

	// For PoC: Complex collision is simplest, but can be heavy on dense meshes.
	ProcMesh->bUseComplexAsSimpleCollision = true;
}

void AProcTerrainActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	if (bRegenerateInConstruction)
	{
		GenerateMesh();
	}
}

void AProcTerrainActor::BeginPlay()
{
	Super::BeginPlay();

	// If you want it to always generate at runtime as well, uncomment:
	// GenerateMesh();
}

void AProcTerrainActor::Regenerate()
{
	GenerateMesh();
}

float AProcTerrainActor::SampleFractalPerlin(float X, float Y) const
{
	// UE's PerlinNoise2D is deterministic from input coordinates only.
	// To add a "seed", we just offset the coordinates by a seed-derived amount.
	const float SeedOffsetX = (float)(Seed % 100000) * 0.01f;
	const float SeedOffsetY = (float)((Seed / 7) % 100000) * 0.01f;

	float Sum = 0.0f;
	float Amp = 1.0f;
	float Freq = Frequency;
	float AmpSum = 0.0f;

	for (int32 i = 0; i < Octaves; i++)
	{
		// Convert world-space sample coords into noise-space coords.
		// Larger Freq => sample changes faster => smaller features.
		const float NX = (X + SeedOffsetX) * Freq;
		const float NY = (Y + SeedOffsetY) * Freq;

		const float N = FMath::PerlinNoise2D(FVector2D(NX, NY)); // ~[-1..1]

		Sum += N * Amp;
		AmpSum += Amp;

		Amp *= Gain;         // reduces amplitude each octave
		Freq *= Lacunarity;  // increases frequency each octave
	}

	// Normalize so output stays roughly in [-1..1]
	return (AmpSum > 0.0f) ? (Sum / AmpSum) : 0.0f;
}

void AProcTerrainActor::GenerateMesh()
{
	// Sanity clamps (avoid crashes if someone sets weird values)
	VertsX = FMath::Max(VertsX, 2);
	VertsY = FMath::Max(VertsY, 2);
	GridSpacing = FMath::Max(GridSpacing, 1.0f);

	const int32 NumVerts = VertsX * VertsY;
	const int32 NumQuadsX = VertsX - 1;
	const int32 NumQuadsY = VertsY - 1;

	// Pre-size arrays (important for performance)
	Vertices.SetNumUninitialized(NumVerts);
	Normals.SetNumUninitialized(NumVerts);
	UV0.SetNumUninitialized(NumVerts);
	Tangents.SetNumUninitialized(NumVerts);

	Triangles.Reset();
	Triangles.Reserve(NumQuadsX * NumQuadsY * 6);

	// We’ll center the grid around the actor origin
	const float HalfWidth  = (VertsX - 1) * GridSpacing * 0.5f;
	const float HalfHeight = (VertsY - 1) * GridSpacing * 0.5f;
	
	TArray<float> Heights;
	Heights.SetNumUninitialized(NumVerts);

	TArray<FColor> VertexColors;
	VertexColors.SetNumUninitialized(NumVerts);

	float MinH = TNumericLimits<float>::Max();
	float MaxH = TNumericLimits<float>::Lowest();

	// -------------------------
	// 1) Build vertices (with noise-based Z)
	// -------------------------
	for (int32 y = 0; y < VertsY; y++)
	{
		for (int32 x = 0; x < VertsX; x++)
		{
			const int32 Idx = x + y * VertsX;

			// Position on plane (X/Y)
			const float PX = (x * GridSpacing) - HalfWidth;
			const float PY = (y * GridSpacing) - HalfHeight;

			// Sample noise using these coordinates
			const float NoiseVal = SampleFractalPerlin(PX, PY); // ~[-1..1]

			// Convert noise to height (cm)
			const float PZ = NoiseVal * HeightAmplitude;

			Vertices[Idx] = FVector(PX, PY, PZ);

			// Simple UVs across the plane (0..1)
			UV0[Idx] = FVector2D((float)x / (VertsX - 1), (float)y / (VertsY - 1));

			// Temporarily set normals up; we’ll recompute properly after triangles
			Normals[Idx] = FVector::UpVector;

			// Tangent points along +X direction
			Tangents[Idx] = FProcMeshTangent(1.0f, 0.0f, 0.0f);
			
			
			Heights[Idx] = PZ;
			MinH = FMath::Min(MinH, PZ);
			MaxH = FMath::Max(MaxH, PZ);
		}
	}
	
	const float Range = FMath::Max(MaxH - MinH, 1.0f); // avoid divide-by-zero
	for (int32 i = 0; i < NumVerts; i++)
	{
		const float T = FMath::Clamp((Heights[i] - MinH) / Range, 0.0f, 1.0f);
		const uint8 C = (uint8)FMath::RoundToInt(T * 255.0f);
		VertexColors[i] = FColor(C, C, C, 255);
	}


	// -------------------------
	// 2) Build triangles (two triangles per quad)
	// -------------------------
	for (int32 y = 0; y < NumQuadsY; y++)
	{
		for (int32 x = 0; x < NumQuadsX; x++)
		{
			const int32 I0 = (x)     + (y)     * VertsX;
			const int32 I1 = (x + 1) + (y)     * VertsX;
			const int32 I2 = (x)     + (y + 1) * VertsX;
			const int32 I3 = (x + 1) + (y + 1) * VertsX;

			// Winding order matters (clockwise vs counter-clockwise)
			// This order typically makes the top face visible.
			Triangles.Add(I0);
			Triangles.Add(I2);
			Triangles.Add(I1);

			Triangles.Add(I1);
			Triangles.Add(I2);
			Triangles.Add(I3);
		}
	}

	// -------------------------
	// 3) Recompute normals (simple method)
	// -------------------------
	// Reset normals
	for (int32 i = 0; i < NumVerts; i++)
	{
		Normals[i] = FVector::ZeroVector;
	}

	// Accumulate triangle normals into vertex normals
	for (int32 i = 0; i < Triangles.Num(); i += 3)
	{
		const int32 A = Triangles[i];
		const int32 B = Triangles[i + 1];
		const int32 C = Triangles[i + 2];

		const FVector& VA = Vertices[A];
		const FVector& VB = Vertices[B];
		const FVector& VC = Vertices[C];

		const FVector Edge1 = VB - VA;
		const FVector Edge2 = VC - VA;

		const FVector TriNormal = FVector::CrossProduct(Edge1, Edge2).GetSafeNormal();

		Normals[A] += TriNormal;
		Normals[B] += TriNormal;
		Normals[C] += TriNormal;
	}

	// Normalize vertex normals
	for (int32 i = 0; i < NumVerts; i++)
	{
		Normals[i].Normalize();
	}

	VertexColors[0] = FColor::Black;
	// -------------------------
	// 4) Push mesh section to ProceduralMeshComponent
	// -------------------------
	ProcMesh->ClearAllMeshSections();

	ProcMesh->CreateMeshSection(
		0,
		Vertices,
		Triangles,
		Normals,
		UV0,
		VertexColors,
		Tangents,
		/*bCreateCollision*/ true
	);
}
