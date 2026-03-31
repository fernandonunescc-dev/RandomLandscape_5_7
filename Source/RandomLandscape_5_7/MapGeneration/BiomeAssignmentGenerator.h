#pragma once

#include "CoreMinimal.h"
#include "BiomeTypes.h"
#include "WorldGenTypes.h"
#include "BiomeAssignmentGenerator.generated.h"

/**
 * Stage 6: Biome Assignment — rule-based classification from climate + terrain.
 *
 * Takes elevation, temperature, moisture, precipitation, land mask, river/lake
 * maps, and volcanic centers from earlier stages and produces:
 *   1. BiomeMap           — per-pixel EBiomeType encoded as int32 (legacy)
 *   2. TerrainArchetypeMap — per-pixel ETerrainArchetype encoded as int32
 *   3. SurfaceOverlayMap  — per-pixel ESurfaceOverlay encoded as int32
 *   4. GeneratedFeatureMap — per-pixel EGeneratedFeature encoded as int32
 *   5. SlopeMap           — per-pixel slope magnitude [0,1]
 *   6. WaterDistMap       — per-pixel BFS distance to nearest river/lake (pixels)
 *   7. BiomeBlendWeights  — per-pixel array of 8 floats (one per EBiomeType)
 *
 * Three-tier terrain taxonomy:
 *   Terrain archetypes (shape):    Plains, Hills, Desert, Mountains, Plateaus, Canyons
 *   Generated features (water):    Rivers, Lakes, Waterfalls
 *   Surface overlays (content):    Forest, Grassland, Snow, Wetlands, DesertScrub
 *
 * Legacy biome classification priority (unchanged for backward compatibility):
 *   1. Ocean        (LandMask == 0)
 *   2. Volcanic     (near volcanic center AND Elevation > 0.3)
 *   3. Mountain     (Elevation > threshold OR slope > steep-slope threshold)
 *   4. Ice          (Temperature < threshold AND Moisture > threshold)
 *   5. Snow         (Temperature < threshold)
 *   6. Desert       (Temperature > threshold AND effective moisture < threshold)
 *   7. Forest       (Moisture > threshold AND Temperature > threshold AND Precipitation > threshold)
 *   8. Land         (default grassland)
 */
UCLASS(BlueprintType)
class RANDOMLANDSCAPE_5_7_API UBiomeAssignmentGenerator : public UObject
{
	GENERATED_BODY()

public:

	/**
	 * Store settings and allocate output buffers.
	 *
	 * @param InSettings        Biome classification thresholds
	 * @param TextureResolution Width/height of the square map in pixels
	 */
	void Initialize(const FBiomeAssignmentSettings& InSettings, int32 TextureResolution);

	/**
	 * Classify every pixel into a biome type, compute slope, water distance,
	 * and biome blending weights.  Also produces terrain archetype, surface
	 * overlay, and generated feature maps.
	 *
	 * @param Elevation       Per-pixel elevation [0,1]
	 * @param Temperature     Per-pixel temperature [0,1]
	 * @param Moisture        Per-pixel moisture [0,1]
	 * @param Precipitation   Per-pixel precipitation [0,1]
	 * @param LandMask        Binary mask (1 = land, 0 = ocean)
	 * @param RiverMap        Normalized river strength [0,1] (>0 = river)
	 * @param LakeMap         Binary mask (1 = lake, 0 = not lake)
	 * @param VolcanicCenters Volcanic hotspot positions in normalized [0,1] space
	 * @param PlateauMap      Per-pixel plateau strength [0,1]
	 * @param CanyonMask      Binary mask (1 = canyon, 0 = not canyon)
	 * @param WaterfallMap    Binary mask (1 = waterfall, 0 = not waterfall)
	 * @param SeaLevel        Configurable sea level as normalized elevation [0,1]
	 * @return true on success
	 */
	bool Generate(const TArray<float>& Elevation, const TArray<float>& Temperature,
		const TArray<float>& Moisture, const TArray<float>& Precipitation,
		const TArray<uint8>& LandMask, const TArray<float>& RiverMap,
		const TArray<uint8>& LakeMap, const TArray<FVector2D>& VolcanicCenters,
		const TArray<float>& PlateauMap, const TArray<uint8>& CanyonMask,
		const TArray<uint8>& WaterfallMap, float SeaLevel);

	/** Biome map — stores static_cast<int32>(EBiomeType) per pixel. */
	const TArray<int32>& GetBiomeMap() const { return BiomeMap; }

	/** Terrain archetype map — stores static_cast<int32>(ETerrainArchetype) per pixel. */
	const TArray<int32>& GetTerrainArchetypeMap() const { return TerrainArchetypeMap; }

	/** Surface overlay map — stores static_cast<int32>(ESurfaceOverlay) per pixel. */
	const TArray<int32>& GetSurfaceOverlayMap() const { return SurfaceOverlayMap; }

	/** Generated feature map — stores static_cast<int32>(EGeneratedFeature) per pixel. */
	const TArray<int32>& GetGeneratedFeatureMap() const { return GeneratedFeatureMap; }

	/** Slope magnitude per pixel [0,1].  Computed from 8-connected elevation differences. */
	const TArray<float>& GetSlopeMap() const { return SlopeMap; }

	/** BFS distance to nearest river/lake per pixel (in pixels). */
	const TArray<float>& GetWaterDistMap() const { return WaterDistMap; }

	/** Per-pixel biome blending weights (8 floats per pixel, indexed by EBiomeType). */
	const TArray<float>& GetBiomeBlendWeights() const { return BiomeBlendWeights; }

private:

	FBiomeAssignmentSettings Settings;
	int32 Resolution = 0;

	TArray<int32> BiomeMap;
	TArray<int32> TerrainArchetypeMap;
	TArray<int32> SurfaceOverlayMap;
	TArray<int32> GeneratedFeatureMap;
	TArray<float> SlopeMap;
	TArray<float> WaterDistMap;

	/** Flat array: pixel i → [i*8 .. i*8+7], one weight per EBiomeType. */
	TArray<float> BiomeBlendWeights;

	/** Number of distinct biome types (must match EBiomeType count). */
	static constexpr int32 NumBiomeTypes = 8;

	// --- helpers ---
	void ComputeSlopeMap(const TArray<float>& Elevation);
	void ComputeWaterDistMap(const TArray<float>& RiverMap, const TArray<uint8>& LakeMap);
	void ComputeBiomeBlendWeights();

	/** If bEnableBiomeTargets, override threshold-based assignment with
	    score-ranked selection to achieve target coverage per biome. */
	void ApplyBiomeTargets(const TArray<float>& Elevation,
		const TArray<float>& Temperature,
		const TArray<float>& EffectiveMoisture,
		const TArray<float>& Precipitation,
		const TArray<uint8>& LandMask);

	/** Flood-fill connected components; absorb patches smaller than
	    MinBiomeClusterSize into the dominant neighbouring biome. */
	void RemoveSmallClusters();
};
