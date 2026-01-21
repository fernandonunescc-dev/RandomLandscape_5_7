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
	int32 VerticesPerSide,
	const FIslandEdgeConfig& EdgeConfig,
	const FChunkEdgeFlags& EdgeFlags)
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
			float Height = SectionData.HeightMap[Index];
			
			const float WorldX = SectionWorldX + X * VertexSpacing;
			const float WorldY = SectionWorldY + Y * VertexSpacing;

			// Apply edge falloff if island mode is enabled
			if (EdgeConfig.bEnabled)
			{
				Height = ApplyEdgeFalloff(Height, WorldX, WorldY, EdgeConfig);
			}

			SectionData.MinHeight = FMath::Min(SectionData.MinHeight, Height);
			SectionData.MaxHeight = FMath::Max(SectionData.MaxHeight, Height);

			SectionData.Vertices[Index] = FVector(WorldX, WorldY, Height);
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

	// -------------------------------------------------------------------------
	// Add Edge Skirt (if this chunk is on an edge and island mode is enabled)
	// -------------------------------------------------------------------------
	if (EdgeConfig.bEnabled && EdgeFlags.HasAnyEdge())
	{
		AddEdgeSkirt(SectionData, SectionWorldX, SectionWorldY, SectionSizeUU, 
			VerticesPerSide, EdgeConfig, EdgeFlags);
	}

	SectionData.bIsGenerated = true;
}

// ============================================================================
// Edge Falloff
// ============================================================================

float FLandscapeMeshBuilder::ApplyEdgeFalloff(
	float OriginalHeight,
	float WorldX,
	float WorldY,
	const FIslandEdgeConfig& Config)
{
	// =========================================================================
	// RADIAL ISLAND FALLOFF (Somewhat Circular)
	// =========================================================================
	// Creates a roughly circular island by:
	// 1. Calculating radial distance from center
	// 2. Applying smooth falloff from center to edge
	// 3. Edges always drop to minimum height (-1 equivalent)
	// =========================================================================
	
	const float HalfMapSize = Config.MapSizeUU / 2.0f;
	
	// Calculate radial distance from map center
	const float DeltaX = WorldX - Config.MapCenter.X;
	const float DeltaY = WorldY - Config.MapCenter.Y;
	const float RadialDistance = FMath::Sqrt(DeltaX * DeltaX + DeltaY * DeltaY);
	
	// The island radius - use inscribed circle (HalfMapSize) as the edge
	// FalloffDistance controls how much of the map is in the falloff zone
	const float IslandRadius = HalfMapSize;
	
	// Where the falloff starts (from center outward)
	// IslandShapeStrength controls the "core" size: 
	// - Higher values = larger flat center, steeper edges
	// - Lower values = falloff starts closer to center, gentler overall
	const float CoreRadius = IslandRadius * (1.0f - Config.IslandShapeStrength);
	const float FalloffRange = IslandRadius - CoreRadius;
	
	// Calculate the island factor (1.0 at center, 0.0 at edge)
	float IslandFactor = 1.0f;
	
	if (RadialDistance > CoreRadius && FalloffRange > 0.0f)
	{
		// How far through the falloff zone (0 = at core edge, 1 = at island edge)
		float FalloffProgress = (RadialDistance - CoreRadius) / FalloffRange;
		FalloffProgress = FMath::Clamp(FalloffProgress, 0.0f, 1.0f);
		
		// Apply smoothstep for natural curve (S-shaped transition)
		float SmoothProgress = FalloffProgress * FalloffProgress * (3.0f - 2.0f * FalloffProgress);
		
		// Apply exponent to control cliff steepness
		// Higher exponent = terrain stays high longer, then drops sharply
		// Lower exponent = more gradual, linear-ish falloff
		SmoothProgress = FMath::Pow(SmoothProgress, Config.FalloffExponent);
		
		// Island factor goes from 1 (at core) to 0 (at edge)
		IslandFactor = 1.0f - SmoothProgress;
	}
	else if (RadialDistance >= IslandRadius)
	{
		// Beyond island edge - force to minimum
		IslandFactor = 0.0f;
	}
	
	// Calculate the minimum height (what edges drop to)
	const float MinHeight = Config.SeaLevel - Config.SkirtDepth * 0.5f;
	
	// Blend between original height and minimum based on island factor
	// At center (factor=1): full original height
	// At edge (factor=0): minimum height (below sea level)
	float ModifiedHeight = FMath::Lerp(MinHeight, OriginalHeight, IslandFactor);
	
	return ModifiedHeight;
}

// ============================================================================
// Edge Skirt Generation
// ============================================================================

void FLandscapeMeshBuilder::AddEdgeSkirt(
	FLandscapeSectionData& SectionData,
	float SectionWorldX,
	float SectionWorldY,
	float SectionSizeUU,
	int32 VerticesPerSide,
	const FIslandEdgeConfig& EdgeConfig,
	const FChunkEdgeFlags& EdgeFlags)
{
	const float VertexSpacing = SectionSizeUU / (VerticesPerSide - 1);
	const float SkirtBottom = EdgeConfig.SeaLevel - EdgeConfig.SkirtDepth;
	const int32 BaseVertexCount = SectionData.Vertices.Num();
	
	// Track new vertices and triangles for skirts
	TArray<FVector> SkirtVertices;
	TArray<int32> SkirtTriangles;
	TArray<FVector2D> SkirtUVs;
	TArray<FVector> SkirtNormals;

	// Helper lambda to add a skirt edge
	auto AddSkirtEdge = [&](int32 StartIdx, int32 EndIdx, int32 Step, const FVector& OutwardNormal)
	{
		int32 PrevTopIdx = -1;
		int32 PrevBottomIdx = -1;

		for (int32 Idx = StartIdx; Idx != EndIdx + Step; Idx += Step)
		{
			const FVector& TopVertex = SectionData.Vertices[Idx];
			
			// Create bottom vertex (directly below at skirt depth)
			FVector BottomVertex = TopVertex;
			BottomVertex.Z = SkirtBottom;

			int32 NewTopIdx = BaseVertexCount + SkirtVertices.Num();
			SkirtVertices.Add(TopVertex);
			SkirtUVs.Add(FVector2D(0.0f, 0.0f));
			SkirtNormals.Add(OutwardNormal);

			int32 NewBottomIdx = BaseVertexCount + SkirtVertices.Num();
			SkirtVertices.Add(BottomVertex);
			SkirtUVs.Add(FVector2D(0.0f, 1.0f));
			SkirtNormals.Add(OutwardNormal);

			// Create triangles connecting to previous vertices
			if (PrevTopIdx >= 0)
			{
				// Triangle 1: PrevTop, NewTop, PrevBottom
				SkirtTriangles.Add(PrevTopIdx);
				SkirtTriangles.Add(NewTopIdx);
				SkirtTriangles.Add(PrevBottomIdx);

				// Triangle 2: NewTop, NewBottom, PrevBottom
				SkirtTriangles.Add(NewTopIdx);
				SkirtTriangles.Add(NewBottomIdx);
				SkirtTriangles.Add(PrevBottomIdx);
			}

			PrevTopIdx = NewTopIdx;
			PrevBottomIdx = NewBottomIdx;
		}
	};

	// Add skirts for each edge that's at the map boundary
	
	// Left edge (X = 0, varying Y)
	if (EdgeFlags.bIsLeftEdge)
	{
		for (int32 Y = 0; Y < VerticesPerSide - 1; ++Y)
		{
			const int32 TopIdx = Y * VerticesPerSide;
			const int32 NextTopIdx = (Y + 1) * VerticesPerSide;
			
			const FVector& V0 = SectionData.Vertices[TopIdx];
			const FVector& V1 = SectionData.Vertices[NextTopIdx];
			FVector V0Bottom = V0; V0Bottom.Z = SkirtBottom;
			FVector V1Bottom = V1; V1Bottom.Z = SkirtBottom;
			
			int32 BaseIdx = BaseVertexCount + SkirtVertices.Num();
			SkirtVertices.Add(V0);
			SkirtVertices.Add(V0Bottom);
			SkirtVertices.Add(V1);
			SkirtVertices.Add(V1Bottom);
			
			FVector Normal(-1.0f, 0.0f, 0.0f);
			SkirtNormals.Add(Normal);
			SkirtNormals.Add(Normal);
			SkirtNormals.Add(Normal);
			SkirtNormals.Add(Normal);
			
			SkirtUVs.Add(FVector2D(0, 0));
			SkirtUVs.Add(FVector2D(0, 1));
			SkirtUVs.Add(FVector2D(1, 0));
			SkirtUVs.Add(FVector2D(1, 1));
			
			// Two triangles for the quad (winding for outward-facing)
			SkirtTriangles.Add(BaseIdx + 0);
			SkirtTriangles.Add(BaseIdx + 1);
			SkirtTriangles.Add(BaseIdx + 2);
			
			SkirtTriangles.Add(BaseIdx + 2);
			SkirtTriangles.Add(BaseIdx + 1);
			SkirtTriangles.Add(BaseIdx + 3);
		}
	}

	// Right edge (X = max, varying Y)
	if (EdgeFlags.bIsRightEdge)
	{
		for (int32 Y = 0; Y < VerticesPerSide - 1; ++Y)
		{
			const int32 TopIdx = Y * VerticesPerSide + (VerticesPerSide - 1);
			const int32 NextTopIdx = (Y + 1) * VerticesPerSide + (VerticesPerSide - 1);
			
			const FVector& V0 = SectionData.Vertices[TopIdx];
			const FVector& V1 = SectionData.Vertices[NextTopIdx];
			FVector V0Bottom = V0; V0Bottom.Z = SkirtBottom;
			FVector V1Bottom = V1; V1Bottom.Z = SkirtBottom;
			
			int32 BaseIdx = BaseVertexCount + SkirtVertices.Num();
			SkirtVertices.Add(V0);
			SkirtVertices.Add(V0Bottom);
			SkirtVertices.Add(V1);
			SkirtVertices.Add(V1Bottom);
			
			FVector Normal(1.0f, 0.0f, 0.0f);
			SkirtNormals.Add(Normal);
			SkirtNormals.Add(Normal);
			SkirtNormals.Add(Normal);
			SkirtNormals.Add(Normal);
			
			SkirtUVs.Add(FVector2D(0, 0));
			SkirtUVs.Add(FVector2D(0, 1));
			SkirtUVs.Add(FVector2D(1, 0));
			SkirtUVs.Add(FVector2D(1, 1));
			
			// Two triangles (opposite winding from left edge)
			SkirtTriangles.Add(BaseIdx + 0);
			SkirtTriangles.Add(BaseIdx + 2);
			SkirtTriangles.Add(BaseIdx + 1);
			
			SkirtTriangles.Add(BaseIdx + 2);
			SkirtTriangles.Add(BaseIdx + 3);
			SkirtTriangles.Add(BaseIdx + 1);
		}
	}

	// Bottom edge (Y = 0, varying X)
	if (EdgeFlags.bIsBottomEdge)
	{
		for (int32 X = 0; X < VerticesPerSide - 1; ++X)
		{
			const int32 TopIdx = X;
			const int32 NextTopIdx = X + 1;
			
			const FVector& V0 = SectionData.Vertices[TopIdx];
			const FVector& V1 = SectionData.Vertices[NextTopIdx];
			FVector V0Bottom = V0; V0Bottom.Z = SkirtBottom;
			FVector V1Bottom = V1; V1Bottom.Z = SkirtBottom;
			
			int32 BaseIdx = BaseVertexCount + SkirtVertices.Num();
			SkirtVertices.Add(V0);
			SkirtVertices.Add(V0Bottom);
			SkirtVertices.Add(V1);
			SkirtVertices.Add(V1Bottom);
			
			FVector Normal(0.0f, -1.0f, 0.0f);
			SkirtNormals.Add(Normal);
			SkirtNormals.Add(Normal);
			SkirtNormals.Add(Normal);
			SkirtNormals.Add(Normal);
			
			SkirtUVs.Add(FVector2D(0, 0));
			SkirtUVs.Add(FVector2D(0, 1));
			SkirtUVs.Add(FVector2D(1, 0));
			SkirtUVs.Add(FVector2D(1, 1));
			
			// Two triangles
			SkirtTriangles.Add(BaseIdx + 0);
			SkirtTriangles.Add(BaseIdx + 2);
			SkirtTriangles.Add(BaseIdx + 1);
			
			SkirtTriangles.Add(BaseIdx + 2);
			SkirtTriangles.Add(BaseIdx + 3);
			SkirtTriangles.Add(BaseIdx + 1);
		}
	}

	// Top edge (Y = max, varying X)
	if (EdgeFlags.bIsTopEdge)
	{
		const int32 TopRow = (VerticesPerSide - 1) * VerticesPerSide;
		for (int32 X = 0; X < VerticesPerSide - 1; ++X)
		{
			const int32 TopIdx = TopRow + X;
			const int32 NextTopIdx = TopRow + X + 1;
			
			const FVector& V0 = SectionData.Vertices[TopIdx];
			const FVector& V1 = SectionData.Vertices[NextTopIdx];
			FVector V0Bottom = V0; V0Bottom.Z = SkirtBottom;
			FVector V1Bottom = V1; V1Bottom.Z = SkirtBottom;
			
			int32 BaseIdx = BaseVertexCount + SkirtVertices.Num();
			SkirtVertices.Add(V0);
			SkirtVertices.Add(V0Bottom);
			SkirtVertices.Add(V1);
			SkirtVertices.Add(V1Bottom);
			
			FVector Normal(0.0f, 1.0f, 0.0f);
			SkirtNormals.Add(Normal);
			SkirtNormals.Add(Normal);
			SkirtNormals.Add(Normal);
			SkirtNormals.Add(Normal);
			
			SkirtUVs.Add(FVector2D(0, 0));
			SkirtUVs.Add(FVector2D(0, 1));
			SkirtUVs.Add(FVector2D(1, 0));
			SkirtUVs.Add(FVector2D(1, 1));
			
			// Two triangles (opposite winding from bottom edge)
			SkirtTriangles.Add(BaseIdx + 0);
			SkirtTriangles.Add(BaseIdx + 1);
			SkirtTriangles.Add(BaseIdx + 2);
			
			SkirtTriangles.Add(BaseIdx + 2);
			SkirtTriangles.Add(BaseIdx + 1);
			SkirtTriangles.Add(BaseIdx + 3);
		}
	}

	// Append skirt data to section
	SectionData.Vertices.Append(SkirtVertices);
	SectionData.Triangles.Append(SkirtTriangles);
	SectionData.UVs.Append(SkirtUVs);
	SectionData.Normals.Append(SkirtNormals);
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
