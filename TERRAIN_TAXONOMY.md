# Terrain Classification System — Three-Tier Taxonomy

## 1. Revised Terrain Taxonomy

The terrain classification system uses a **three-tier taxonomy** that separates terrain shape, water features, and surface content into independent layers:

### Tier 1: Terrain Archetypes (Shape)

Determined by elevation, slope, geological masks (plateau/canyon), and aridity.
These describe the **structural geometry** of the terrain.

| Archetype  | Classification Rule |
|------------|-------------------|
| **Plains** | Default low-elevation, low-slope land |
| **Hills** | Low/mid elevation with slope above `HillSlopeThreshold` |
| **Desert** | Hot + dry terrain (`Temperature > DesertTempThreshold` AND `Moisture < DesertMoistureThreshold`) |
| **Mountains** | `Elevation > MountainElevationThreshold` OR `Slope > SteepSlopeThreshold` |
| **Plateaus** | `PlateauMap[pixel] >= PlateauArchetypeThreshold` (from Stage 2 noise mask) |
| **Canyons** | `CanyonMask[pixel] != 0` (from Stage 4 river incision) |

Classification priority: Canyons > Plateaus > Mountains > Desert > Hills > Plains

### Tier 2: Generated Features (Water/Hydrology)

Outputs of the hydrology simulation (Stage 3). Layered on top of any archetype.
**Lakes are explicitly NOT a terrain archetype.**

| Feature     | Source |
|-------------|--------|
| **River**   | `RiverMap > 0` (flow-accumulation threshold) |
| **Lake**    | `LakeMap != 0` (local-minimum drainage + BFS expansion) |
| **Waterfall** | `WaterfallMap != 0` (steep river drop detection) |

### Tier 3: Surface / Content Overlays

Determined by climate (temperature, moisture, precipitation) and water proximity.
Any terrain archetype can host any surface overlay.

| Overlay        | Classification Rule |
|----------------|-------------------|
| **Snow**       | `Temperature < SnowTemperatureThreshold` |
| **Wetlands**   | `EffectiveMoisture >= WetlandsMoistureThreshold` AND `WaterDist < WetlandsMaxWaterDistance` |
| **Forest**     | `Moisture > ForestMoistureThreshold` AND `Temperature > ForestTempThreshold` AND `Precipitation > ForestPrecipThreshold` |
| **Desert Scrub** | `Temperature > DesertScrubTempThreshold` AND `DesertMoistureThreshold <= Moisture < DesertScrubMoistureMax` |
| **Grassland**  | Default surface overlay for land |

Classification priority: Snow > Wetlands > Forest > DesertScrub > Grassland

### Enum Definitions (BiomeTypes.h)

```cpp
enum class ETerrainArchetype : uint8
{
    Ocean, Plains, Hills, Desert, Mountains, Plateaus, Canyons
};

enum class EGeneratedFeature : uint8
{
    None, River, Lake, Waterfall
};

enum class ESurfaceOverlay : uint8
{
    None, Forest, Grassland, Snow, Wetlands, DesertScrub
};
```

The legacy `EBiomeType` enum (Ocean, Land, Forest, Desert, Snow, Ice, Mountain, Volcanic) is retained for backward compatibility with Stage 7 (Terrain Refinement) and mesh coloring.

---

## 2. Updated Generation Pipeline

```
Stage 1: Landmass        → Binary land/ocean mask
Stage 2: Uplift/Geology  → Base elevation + mountains + hills + plateaus + volcanoes
                            Outputs: BaseElevation, UpliftMap, CombinedElevation, PlateauMap
Stage 3: Hydrology       → Rivers, lakes, waterfalls from drainage simulation
                            Outputs: RiverMap, LakeMap, WaterfallMap, FlowAccumulation
Stage 4: Erosion         → River incision (canyons) + thermal erosion
                            Outputs: ErodedElevation, CanyonMask, ErosionDeltaMap
Stage 5: Climate         → Temperature, moisture, precipitation from terrain shape
Stage 6: Biomes          → Three-tier classification:
                              ├─ TerrainArchetypeMap  (shape)
                              ├─ GeneratedFeatureMap  (water)
                              ├─ SurfaceOverlayMap    (content)
                              └─ BiomeMap             (legacy compat)
                            Also: SlopeMap, WaterDistMap, BiomeBlendWeights
Stage 7: Refinement      → Biome-local detail noise on macro terrain
Stage 8: Mesh            → Procedural mesh from final elevation + biome colors
```

Key change: Stage 6 now accepts `PlateauMap`, `CanyonMask`, `WaterfallMap`, and `SeaLevel` as additional inputs, and produces three new output maps alongside the legacy `BiomeMap`.

---

## 3. Height / Sea-Level Rules

### Configurable Sea Level

A new `SeaLevel` parameter (normalized `[0, 0.5]`, default `0.0`) defines the elevation threshold between "below sea level" and "above sea level" for classification purposes.

| Parameter | Type | Range | Default | Purpose |
|-----------|------|-------|---------|---------|
| `SeaLevel` | float | 0.0–0.5 | 0.0 | Normalized elevation threshold for classification |
| `OceanLevel` | float | 0–100 m | 15 m | Mesh depth of ocean vertices below Z=0 |
| `MaxMapHeight` | float | 10–2000 m | 500 m | Maximum land elevation in meters |

### Height Rules

- **All pipeline stages (1-7)** work with normalized elevation `[0, 1]`
- **Ocean pixels** (LandMask == 0) are always at the bottom of the range
- **Land pixels** range from `0.0` (coastline) to `1.0` (highest peak)
- **Sea level** does NOT create negative elevations — it's a classification threshold
- **Canyons** are carved by subtracting from positive elevation, clamped to `>= 0`
- **Mesh conversion**: `WorldZ = Elevation * MaxMapHeight * 100` (cm) for land, `WorldZ = -OceanLevel * 100` (cm) for ocean

### Canyon Generation (no negative elevation required)

Canyons are generated via river incision in Stage 4:
1. Each river pixel gets `Incision = RiverMap[i] * RiverIncisionStrength`
2. Incision deepens in high-uplift areas: `Incision *= (1 + UpliftMap[i] * CanyonDepthMultiplier)`
3. BFS expansion creates valley profile with linear taper over `CanyonWidth` pixels
4. Final elevation: `max(0, OldElevation - IncisionDepth)` — always non-negative
5. CanyonMask marks all pixels where elevation was actually removed

---

## 4. How Each Terrain Archetype Influences Terrain Shape

| Archetype | Shape Generation (Stage 2) | Shape Influence |
|-----------|---------------------------|-----------------|
| **Plains** | Low base elevation from coastline gradient + gentle FBM noise | Broad, flat areas with subtle rolling. Driven by `BaseNoiseFrequency` and `BaseNoisePersistence`. |
| **Hills** | Dedicated hill noise layer: `HillFrequency`, `HillAmplitude`, `HillOctaves` | Moderate elevation variation, rounded profiles. FBM noise scaled by base elevation to fade near coasts. |
| **Desert** | Same elevation as surrounding terrain — Desert is a climate archetype | Terrain shape comes from underlying plains/hills. Surface character from dune noise in Stage 7 (`DuneFrequency`, `DuneHeight`). |
| **Mountains** | Ridged FBM: `MountainRidgeFrequency`, `MountainRidgeAmplitude`, `MountainSharpness` | Sharp peaks and ridges. Multiplied by base elevation so ridges fade coastward. Extra ridged detail noise in Stage 7. |
| **Plateaus** | FBM noise identifies regions, then flattens UpliftMap toward `PlateauElevation` | Flat tops with sharp edges. `PlateauFlatness` controls how flat; only applies in mid-elevation band (0.15–0.8). |
| **Canyons** | River incision in Stage 4 carves into existing terrain | V/U-shaped valleys. Depth proportional to flow strength × uplift. `CanyonWidth` controls valley width via BFS. `CanyonWallSteepness` protects walls from thermal erosion. |

---

## 5. How Generated Features Depend on Terrain

| Feature | Terrain Dependencies | Generation Method |
|---------|---------------------|-------------------|
| **Rivers** | Flow direction from D8 steepest-descent on combined elevation. Higher terrain = more drainage area downstream. | Stage 3: D8 flow → topological-sort accumulation → threshold rivers. `FlowJitter` adds natural meandering. |
| **Lakes** | Form at local elevation minima (no downhill neighbor) with sufficient upstream flow. More common in basins between mountains/plateaus. | Stage 3: Local minima with `FlowAccumulation > LakeThreshold × MaxAccum`. BFS expansion for visual area. |
| **Waterfalls** | Occur where rivers cross steep elevation drops (cliff edges, canyon rims, plateau edges). | Stage 3: River pixels where downstream elevation drop ≥ `WaterfallMinElevationDrop`. Naturally occur at plateau/canyon boundaries. |

### Archetype–Feature Interactions

- **Mountains** → drive river origin points (high accumulation at mountain bases)
- **Plateaus** → create waterfall sites at plateau edges where rivers spill over
- **Canyons** → deepened by river incision (rivers create canyons, canyons ARE rivers)
- **Plains** → rivers widen and meander in flat terrain (higher accumulation values)
- **Desert** → rivers dry up (high threshold filters out low-flow channels in arid areas)

---

## 6. Unreal-Friendly Implementation Plan

### Data Structures

All three classification layers are stored as parallel per-pixel `TArray<int32>` maps, same resolution as the elevation grid:

```cpp
// In WorldGenerationActor.h (cached pipeline data):
TArray<int32> CachedTerrainArchetypeMap;  // ETerrainArchetype per pixel
TArray<int32> CachedSurfaceOverlayMap;    // ESurfaceOverlay per pixel
TArray<int32> CachedGeneratedFeatureMap;  // EGeneratedFeature per pixel
TArray<int32> CachedBiomeMap;             // EBiomeType per pixel (legacy)
```

### Debug Visualization

Three new debug textures in the editor Details panel (Stage 6 group):
- `Debug_TerrainArchetypeMap` — color-coded by archetype
- `Debug_SurfaceOverlayMap` — color-coded by surface overlay
- `Debug_GeneratedFeatureMap` — highlights rivers (blue), lakes (dark blue), waterfalls (light blue)

### Editor Integration

- New `SeaLevel` parameter exposed in the Global Settings group
- New classification threshold properties in `FBiomeAssignmentSettings`:
  - `HillSlopeThreshold`, `PlateauArchetypeThreshold`
  - `WetlandsMoistureThreshold`, `WetlandsMaxWaterDistance`
  - `DesertScrubTemperatureThreshold`, `DesertScrubMoistureMax`
- `SeaLevel` changes trigger `RegenerateFromStage(6)` (biome reclassification only)
- `OceanLevel` / `MaxMapHeight` changes trigger `GenerateMesh()` only (no data regeneration)

### Downstream Usage

The three-tier maps can be queried by downstream systems:

```cpp
// Example: get terrain info at a pixel
ETerrainArchetype Archetype = static_cast<ETerrainArchetype>(CachedTerrainArchetypeMap[PixelIndex]);
ESurfaceOverlay   Overlay   = static_cast<ESurfaceOverlay>(CachedSurfaceOverlayMap[PixelIndex]);
EGeneratedFeature Feature   = static_cast<EGeneratedFeature>(CachedGeneratedFeatureMap[PixelIndex]);

// Example: check if pixel is a canyon with forest surface
bool bForestCanyon = (Archetype == ETerrainArchetype::Canyons)
                  && (Overlay == ESurfaceOverlay::Forest);

// Example: check if pixel has water feature
bool bHasWater = (Feature != EGeneratedFeature::None);
```

### Backward Compatibility

The legacy `EBiomeType` enum and `BiomeMap` are fully preserved. Stage 7 (Terrain Refinement) and mesh coloring continue to use `EBiomeType` without modification. The new three-tier maps are additive — they provide richer classification data alongside the existing system.
