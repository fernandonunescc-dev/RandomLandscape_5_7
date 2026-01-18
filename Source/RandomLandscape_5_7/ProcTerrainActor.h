#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ProceduralMeshComponent.h"
#include "ProcTerrainActor.generated.h"


/**
 * AProcTerrainActor
 *
 * Basic PoC:
 * - Builds a grid mesh (plane)
 * - Deforms Z using Perlin noise (FMath::PerlinNoise2D)
 * - Exposes parameters so you can tweak in editor / Blueprint
 */
UCLASS()
class RANDOMLANDSCAPE_5_7_API AProcTerrainActor : public AActor
{
	GENERATED_BODY()

public:
	AProcTerrainActor();

	// Called whenever the actor is constructed or a property changes in the editor.
	virtual void OnConstruction(const FTransform& Transform) override;

	// Optional runtime generation (if you want to regenerate on play too).
	virtual void BeginPlay() override;

	// Button in the Details panel (CallInEditor) + callable from Blueprint
	UFUNCTION(BlueprintCallable, CallInEditor, Category="ProcTerrain")
	void Regenerate();

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="ProcTerrain")
	TObjectPtr<UProceduralMeshComponent> ProcMesh;

	// -------------------------
	// Grid settings
	// -------------------------

	// Number of vertices along X (must be >= 2).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ProcTerrain|Grid", meta=(ClampMin="2"))
	int32 VertsX = 200;

	// Number of vertices along Y (must be >= 2).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ProcTerrain|Grid", meta=(ClampMin="2"))
	int32 VertsY = 200;

	// Spacing between vertices in Unreal units (cm). 100 = 1 meter.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ProcTerrain|Grid", meta=(ClampMin="1.0"))
	float GridSpacing = 100.0f;

	// If true, regenerate automatically when you tweak values in editor.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ProcTerrain|Debug")
	bool bRegenerateInConstruction = true;

	// -------------------------
	// Noise settings
	// -------------------------

	// Seed offset to shift sampling coordinates (simple way to "seed" UE's Perlin).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ProcTerrain|Noise")
	int32 Seed = 1337;

	// How tall the terrain can get (cm). 30000cm = 300m.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ProcTerrain|Noise", meta=(ClampMin="0.0"))
	float HeightAmplitude = 30000.0f;

	// Frequency controls feature size:
	// - Lower frequency => larger hills/continents
	// - Higher frequency => smaller bumps
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ProcTerrain|Noise", meta=(ClampMin="0.000001"))
	float Frequency = 0.002f;

	// Fractal settings (optional, but great for learning)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ProcTerrain|Noise", meta=(ClampMin="1"))
	int32 Octaves = 4;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ProcTerrain|Noise", meta=(ClampMin="1.0"))
	float Lacunarity = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ProcTerrain|Noise", meta=(ClampMin="0.0", ClampMax="1.0"))
	float Gain = 0.5f;

private:
	void GenerateMesh();

	// Samples fractal Perlin using UE's PerlinNoise2D
	// Returns approx [-1..1]
	float SampleFractalPerlin(float X, float Y) const;

	// Cached arrays (so we can reuse memory when regenerating)
	TArray<FVector> Vertices;
	TArray<int32> Triangles;
	TArray<FVector> Normals;
	TArray<FVector2D> UV0;
	TArray<FProcMeshTangent> Tangents;
};
