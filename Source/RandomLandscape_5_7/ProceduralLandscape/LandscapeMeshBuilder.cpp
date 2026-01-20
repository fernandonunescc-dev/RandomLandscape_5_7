// Copyright Fernando Araujo. All Rights Reserved.

#include "LandscapeMeshBuilder.h"

// ============================================================================
// Main Mesh Building
// ============================================================================

void FLandscapeMeshBuilder::BuildSectionMesh(
	FLandscapeSectionData& SectionData,
	float SectionWorldX,
	float SectionWorldY,
	float SectionSizeUU,
	int32 VerticesPerSide)
{
	const int32 TotalVertices = VerticesPerSide * VerticesPerSide;
	const float VertexSpacing = SectionSizeUU / (VerticesPerSide - 1);

	// Validate height map
	if (SectionData.HeightMap.Num() != TotalVertices)
	{
		UE_LOG(LogTemp, Error, TEXT("LandscapeMeshBuilder: Height map size mismatch. Expected %d, got %d"),
			TotalVertices, SectionData.HeightMap.Num());
		return;
	}

	// -------------------------------------------------------------------------
	// Generate Vertices
	// -------------------------------------------------------------------------
	SectionData.Vertices.Empty();
	SectionData.Vertices.SetNumUninitialized(TotalVertices);

	SectionData.MinHeight = FLT_MAX;
	SectionData.MaxHeight = -FLT_MAX;

	for (int32 Y = 0; Y < VerticesPerSide; ++Y)
	{
		for (int32 X = 0; X < VerticesPerSide; ++X)
		{
			const int32 Index = Y * VerticesPerSide + X;
			const float Height = SectionData.HeightMap[Index];

			SectionData.MinHeight = FMath::Min(SectionData.MinHeight, Height);
			SectionData.MaxHeight = FMath::Max(SectionData.MaxHeight, Height);

			SectionData.Vertices[Index] = FVector(
				SectionWorldX + X * VertexSpacing,
				SectionWorldY + Y * VertexSpacing,
				Height
			);
		}
	}

	// -------------------------------------------------------------------------
	// Generate Triangles
	// -------------------------------------------------------------------------
	GenerateTriangleIndices(VerticesPerSide, SectionData.Triangles);

	// -------------------------------------------------------------------------
	// Generate UVs
	// -------------------------------------------------------------------------
	GenerateUVs(VerticesPerSide, SectionData.UVs);

	// -------------------------------------------------------------------------
	// Generate Normals
	// -------------------------------------------------------------------------
	CalculateNormals(SectionData.Vertices, VerticesPerSide, SectionData.Normals);

	SectionData.bIsGenerated = true;
}

// ============================================================================
// Normals Calculation
// ============================================================================

void FLandscapeMeshBuilder::CalculateNormals(
	const TArray<FVector>& Vertices,
	int32 VerticesPerSide,
	TArray<FVector>& OutNormals)
{
	const int32 TotalVertices = VerticesPerSide * VerticesPerSide;
	OutNormals.Empty();
	OutNormals.SetNumUninitialized(TotalVertices);

	// Use central differencing for smooth normals
	for (int32 Y = 0; Y < VerticesPerSide; ++Y)
	{
		for (int32 X = 0; X < VerticesPerSide; ++X)
		{
			const int32 Index = Y * VerticesPerSide + X;

			// Get neighboring indices with clamping
			const int32 Left = Y * VerticesPerSide + FMath::Max(0, X - 1);
			const int32 Right = Y * VerticesPerSide + FMath::Min(VerticesPerSide - 1, X + 1);
			const int32 Down = FMath::Max(0, Y - 1) * VerticesPerSide + X;
			const int32 Up = FMath::Min(VerticesPerSide - 1, Y + 1) * VerticesPerSide + X;

			// Calculate tangent vectors
			const FVector TangentX = Vertices[Right] - Vertices[Left];
			const FVector TangentY = Vertices[Up] - Vertices[Down];

			// Cross product for normal (note: order matters for winding)
			FVector Normal = FVector::CrossProduct(TangentY, TangentX);
			Normal.Normalize();

			// Ensure normal points up (Z positive)
			if (Normal.Z < 0.0f)
			{
				Normal = -Normal;
			}

			OutNormals[Index] = Normal;
		}
	}
}

// ============================================================================
// Triangle Index Generation
// ============================================================================

void FLandscapeMeshBuilder::GenerateTriangleIndices(
	int32 VerticesPerSide,
	TArray<int32>& OutTriangles)
{
	const int32 QuadsPerSide = VerticesPerSide - 1;
	const int32 TotalTriangles = QuadsPerSide * QuadsPerSide * 2;
	
	OutTriangles.Empty();
	OutTriangles.Reserve(TotalTriangles * 3);

	for (int32 Y = 0; Y < QuadsPerSide; ++Y)
	{
		for (int32 X = 0; X < QuadsPerSide; ++X)
		{
			// Quad corners
			const int32 BottomLeft = Y * VerticesPerSide + X;
			const int32 BottomRight = BottomLeft + 1;
			const int32 TopLeft = BottomLeft + VerticesPerSide;
			const int32 TopRight = TopLeft + 1;

			// First triangle (bottom-left, top-left, bottom-right)
			OutTriangles.Add(BottomLeft);
			OutTriangles.Add(TopLeft);
			OutTriangles.Add(BottomRight);

			// Second triangle (bottom-right, top-left, top-right)
			OutTriangles.Add(BottomRight);
			OutTriangles.Add(TopLeft);
			OutTriangles.Add(TopRight);
		}
	}
}

// ============================================================================
// UV Generation
// ============================================================================

void FLandscapeMeshBuilder::GenerateUVs(
	int32 VerticesPerSide,
	TArray<FVector2D>& OutUVs)
{
	const int32 TotalVertices = VerticesPerSide * VerticesPerSide;
	OutUVs.Empty();
	OutUVs.SetNumUninitialized(TotalVertices);

	const float UVStep = 1.0f / (VerticesPerSide - 1);

	for (int32 Y = 0; Y < VerticesPerSide; ++Y)
	{
		for (int32 X = 0; X < VerticesPerSide; ++X)
		{
			const int32 Index = Y * VerticesPerSide + X;
			OutUVs[Index] = FVector2D(X * UVStep, Y * UVStep);
		}
	}
}

