# Stage 2 — Uplift / Geology

> Pipeline order: **Landmass → Uplift → Hydrology → Erosion → Climate → Biomes → Refinement**

The Uplift stage turns a flat binary land-mask (from Stage 1) into a full
elevation field by running five sequential sub-stages:

```
LandMask (binary)
  │
  ├─ 1. Base Elevation   ← coastline-distance gradient × FBM noise
  ├─ 2. Mountains         ← ridged FBM for tectonic-style ridges
  ├─ 3. Hills             ← gentle FBM undulations
  ├─ 4. Volcanic Hotspots ← cone-shaped volcanoes with craters
  ├─ 5. Plateaus          ← noise-thresholded flat-topped regions
  │
  └─ Combine  →  CombinedElevation = clamp(Base + Uplift, 0, 1)
```

All elevation values are **normalised to [0, 1]**. The final conversion to
world-space metres happens later via `MaxMapHeight`.

---

## Sub-stages

### 1 — Base Elevation

Creates a smooth elevation rise from coast to interior, modulated by FBM noise.

| Step | Detail |
|------|--------|
| **BFS distance field** | 4-connected flood-fill from every ocean pixel. Each land pixel gets its shortest pixel-distance to the nearest ocean cell. |
| **Gradient** | `Gradient = clamp(distance / CoastlineGradientWidth, 0, 1)` — reaches 1.0 once far enough inland. |
| **FBM modulation** | Standard FBM evaluated at `(x · BaseNoiseFrequency, y · BaseNoiseFrequency)` with `BaseNoiseOctaves` and `BaseNoisePersistence`. Remapped from [−1, 1] to [0.2, 1.0] so coastal pixels never collapse to zero. |
| **Output** | `BaseElevation[i] = Gradient × NoiseMod` (ocean pixels = 0). |

**Settings that control this sub-stage:**

| Setting | Type | Default | Range | What it does |
|---------|------|---------|-------|--------------|
| `CoastlineGradientWidth` | int32 | 80 | 10 – 256 | Pixels inland before the gradient saturates at 1.0. Larger values → gentler coastal slopes. |
| `BaseNoiseFrequency` | float | 1.5 | 0.1 – 10.0 | Noise frequency for the base-elevation modulator. Higher → more variation over shorter distances. |
| `BaseNoiseOctaves` | int32 | 4 | 1 – 8 | FBM octave count. More octaves → finer detail in the modulation. |
| `BaseNoisePersistence` | float | 0.45 | 0.1 – 1.0 | Each successive octave's amplitude ratio. Lower → smoother; higher → rougher. |

---

### 2 — Mountains (Ridged FBM)

Adds tectonic-style mountain ridges on top of the base elevation.

| Step | Detail |
|------|--------|
| **Ridged FBM** | For each land pixel: `ridge = RidgedFBM(x · MountainRidgeFrequency, y · MountainRidgeFrequency)`. Each octave folds the noise (`1 − |noise|`) and raises it to `MountainSharpness`. |
| **Scale** | `ridge *= MountainRidgeAmplitude` |
| **Coastal fade** | Multiply by `BaseElevation[i]` so ridges taper to zero near the shore. |
| **Output** | Accumulated into `UpliftMap`. |

**Settings:**

| Setting | Type | Default | Range | What it does |
|---------|------|---------|-------|--------------|
| `MountainRidgeFrequency` | float | 2.5 | 0.5 – 10.0 | Feature frequency. Higher → denser, narrower ridges. |
| `MountainRidgeAmplitude` | float | 0.6 | 0.0 – 1.0 | Maximum ridge height relative to total elevation. |
| `MountainSharpness` | float | 2.0 | 0.5 – 5.0 | Power exponent applied to the folded noise. Higher → sharper, more defined peaks. |
| `MountainOctaves` | int32 | 5 | 1 – 8 | Ridged-noise octave count. More → finer ridge detail. |

---

### 3 — Hills (Rolling FBM)

Adds gentle, low-amplitude rolling terrain.

| Step | Detail |
|------|--------|
| **FBM** | Standard FBM at `(x · HillFrequency, y · HillFrequency)` with `HillOctaves`. |
| **Remap & scale** | `hill = (fbm × 0.5 + 0.5) × HillAmplitude` → range [0, HillAmplitude]. |
| **Coastal fade** | Multiplied by `BaseElevation[i]`. |
| **Accumulate** | Added to `UpliftMap`, then clamped to [0, 1]. |

Skipped entirely if `HillAmplitude == 0`.

**Settings:**

| Setting | Type | Default | Range | What it does |
|---------|------|---------|-------|--------------|
| `HillFrequency` | float | 3.0 | 0.5 – 10.0 | Feature frequency. Higher → more frequent, smaller hills. |
| `HillAmplitude` | float | 0.15 | 0.0 – 0.5 | Maximum hill height contribution. |
| `HillOctaves` | int32 | 3 | 1 – 6 | FBM octave count for hill noise. |

---

### 4 — Volcanic Hotspots

Places cone-shaped volcanoes with optional craters.

| Step | Detail |
|------|--------|
| **Placement** | `VolcanicHotspotCount` centres are picked randomly from all land pixels (seeded RNG). |
| **Cone rasterisation** | For each pixel within `VolcanicRadius` (normalised map fraction): `height = VolcanicPeakHeight × (1 − normDist)`. |
| **Crater** | If `normDist < CraterRadiusFraction`: subtract `CraterDepth × VolcanicPeakHeight × (1 − craterNorm)`, clamped ≥ 0. |
| **Accumulate** | Added to `UpliftMap`, clamped to [0, 1]. |
| **Side output** | `VolcanicCenters` — normalised (X, Y) positions used later by the Biome stage for proximity checks. |

Skipped entirely if `VolcanicHotspotCount == 0`.

**Settings:**

| Setting | Type | Default | Range | What it does |
|---------|------|---------|-------|--------------|
| `VolcanicHotspotCount` | int32 | 2 | 0 – 10 | Number of volcanoes placed on land. |
| `VolcanicRadius` | float | 0.08 | 0.02 – 0.3 | Influence radius as a fraction of the map. 0.08 ≈ 8 % of map width. |
| `VolcanicPeakHeight` | float | 0.85 | 0.1 – 1.0 | Peak elevation of a volcano (normalised). |
| `CraterDepth` | float | 0.3 | 0.0 – 0.8 | How deep the crater is as a fraction of peak height. |
| `CraterRadiusFraction` | float | 0.2 | 0.0 – 0.5 | Crater bowl radius as a fraction of `VolcanicRadius`. |

---

### 5 — Plateaus

Identifies zones to flatten into elevated mesas.

| Step | Detail |
|------|--------|
| **Zone detection** | FBM noise at `PlateauNoiseFrequency`, 3 octaves. Remapped to [0, 1]. Only pixels above `PlateauThreshold` qualify. |
| **Elevation filter** | Skipped if current elevation (Base + Uplift) < 0.15 or > 0.8 — avoids flattening coastlines or peaks. |
| **Plateau strength** | `strength = clamp((noise − threshold) / (1 − threshold), 0, 1)`. Stored in `PlateauMap`. |
| **Flatten** | `UpliftMap[i] = lerp(UpliftMap[i], desiredUplift, strength × PlateauFlatness)` where `desiredUplift = max(PlateauElevation − BaseElevation[i], 0)`. |

**Settings:**

| Setting | Type | Default | Range | What it does |
|---------|------|---------|-------|--------------|
| `PlateauNoiseFrequency` | float | 1.8 | 0.5 – 8.0 | Noise frequency for plateau-zone detection. |
| `PlateauThreshold` | float | 0.55 | 0.0 – 1.0 | Noise threshold that triggers a plateau. Higher → fewer, more isolated plateaus. |
| `PlateauFlatness` | float | 0.7 | 0.0 – 1.0 | How aggressively the terrain is flattened (0 = no effect, 1 = perfectly flat). |
| `PlateauElevation` | float | 0.45 | 0.1 – 0.8 | Target elevation for plateau tops (normalised). |

---

## Global setting

| Setting | Type | Default | Range | What it does |
|---------|------|---------|-------|--------------|
| `Seed` | int32 | 0 | 0 – ∞ | Per-stage seed override. **0** = automatically derived from the global world seed via `DeriveSeed(GlobalSeed, 2)`. |

Each sub-stage internally offsets its own noise seed (+100, +200, +300, +400)
so the same parent seed still produces uncorrelated layers.

---

## Outputs

| Name | Type | Range | Consumed by |
|------|------|-------|-------------|
| `BaseElevation` | `TArray<float>` | [0, 1] | Combine step, Plateaus |
| `UpliftMap` | `TArray<float>` | [0, 1] | Combine step, debug texture |
| `CombinedElevation` | `TArray<float>` | [0, 1] | Hydrology (Stage 3), Erosion (Stage 4) |
| `PlateauMap` | `TArray<float>` | [0, 1] | Biome assignment (Stage 6), debug texture |
| `VolcanicCenters` | `TArray<FVector2D>` | [0, 1] | Biome assignment (volcano-proximity check) |

All arrays are `Resolution × Resolution` pixels. Ocean pixels are always 0.

---

## Debug textures

The four debug textures shown in the editor under **Debug | 2-Uplift** are
read-only grayscale images (one per output layer):

| Editor label | Source array | What you see |
|--------------|-------------|--------------|
| **Debug Base Elevation** | `BaseElevation` | Coastline gradient modulated by noise — bright = high inland, dark = near coast. |
| **Debug Uplift Map** | `UpliftMap` | Mountain ridges, hills, and volcano cones — bright = high uplift. |
| **Debug Combined Elevation** | `CombinedElevation` | Final merged elevation before hydrology/erosion — the "geology" result. |
| **Debug Plateau Map** | `PlateauMap` | White where plateaus flatten the terrain; black elsewhere. |

These correspond to the four thumbnails visible in the screenshot:

```
Debug Base Elevation      →  soft, noisy gradient
Debug Uplift Map          →  mountain ridges and volcanoes
Debug Combined Elevation  →  sum of the above two
Debug Plateau Map         →  isolated white blobs = plateau zones
```

---

## Editor layout

In the Details panel the Uplift stage appears as a collapsible group:

```
▸ 2 - Uplift/Geology
    UpliftSettings
        Seed
        ▸ Base Elevation
            CoastlineGradientWidth
            BaseNoiseFrequency
            BaseNoiseOctaves
            BaseNoisePersistence
        ▸ Mountains
            MountainRidgeFrequency
            MountainRidgeAmplitude
            MountainSharpness
            MountainOctaves
        ▸ Hills
            HillFrequency
            HillAmplitude
            HillOctaves
        ▸ Plateaus
            PlateauNoiseFrequency
            PlateauThreshold
            PlateauFlatness
            PlateauElevation
        ▸ Volcanic
            VolcanicHotspotCount
            VolcanicRadius
            VolcanicPeakHeight
            CraterDepth
            CraterRadiusFraction
    [ Generate Uplift ]          ← runs only this stage
    Debug_BaseElevation          ← read-only texture
    Debug_UpliftMap
    Debug_CombinedElevation
    Debug_PlateauMap
```

---

## Quick-tuning recipes

| Goal | Settings to adjust |
|------|--------------------|
| **Gentle rolling hills, no mountains** | `MountainRidgeAmplitude = 0`, `HillAmplitude = 0.3`, `VolcanicHotspotCount = 0` |
| **Dramatic alpine ridges** | `MountainRidgeAmplitude = 0.8`, `MountainSharpness = 3.5`, `MountainOctaves = 6` |
| **Mesa / canyon-country** | `PlateauThreshold = 0.35`, `PlateauFlatness = 0.9`, `PlateauElevation = 0.5` |
| **Volcanic island** | `VolcanicHotspotCount = 1`, `VolcanicRadius = 0.15`, `CraterDepth = 0.4` |
| **Flat coastal plain** | `CoastlineGradientWidth = 200`, `BaseNoisePersistence = 0.2`, `HillAmplitude = 0.05` |

---

## Source files

| File | Role |
|------|------|
| `Source/RandomLandscape_5_7/MapGeneration/UpliftGenerator.h` | Class declaration, output getters |
| `Source/RandomLandscape_5_7/MapGeneration/UpliftGenerator.cpp` | Sub-stage implementations |
| `Source/RandomLandscape_5_7/MapGeneration/WorldGenTypes.h` | `FUpliftSettings` struct (lines 151-253) |
| `Source/RandomLandscape_5_7/MapGeneration/WorldGenerationActor.h` | Debug texture properties, `UpliftSettings` UPROPERTY |
| `Source/RandomLandscape_5_7/MapGeneration/NoiseUtility.h` | `WorldNoise::FBM`, `WorldNoise::RidgedFBM` |
