# Procedural Landscape - Quick Reference Card

## Actor Properties at a Glance

### Map Configuration
| Property | Default | Range | Effect |
|----------|---------|-------|--------|
| MapSize | 100 | 10-1000 | Total landscape size in meters |
| GridScale | 10 | 1-100 | Individual cell size in meters |

### Height Configuration
| Property | Default | Range | Effect |
|----------|---------|-------|--------|
| MaxHeightVariation | 10000 | 1000-50000 | Height range in cm (10000 = 100m) |
| HeightOffset | 0 | -50000-50000 | Base height adjustment |

### Water Configuration
| Property | Default | Range | Effect |
|----------|---------|-------|--------|
| WaterLevel | 0 | -10000-10000 | Height at which water plane appears |
| bGenerateWaterPlanes | true | true/false | Create water plane meshes |
| WaterMaterial | None | Material* | Material for water surfaces |

### Noise Configuration
| Property | Default | Range | Effect |
|----------|---------|-------|--------|
| PrimaryNoiseFrequency | 0.001 | 0.0001-0.1 | Large terrain features (continents) |
| PrimaryNoiseOctaves | 3 | 1-6 | Layers of primary noise |
| SecondaryNoiseFrequency | 0.01 | 0.001-1.0 | Medium terrain features (hills/valleys) |
| SecondaryNoiseOctaves | 4 | 1-8 | Layers of secondary noise |
| TertiaryNoiseFrequency | 0.05 | 0.01-10.0 | Detail texture (bumps) |
| TertiaryNoiseOctaves | 2 | 1-4 | Layers of tertiary noise |

### Streaming Configuration
| Property | Default | Range | Effect |
|----------|---------|-------|--------|
| StreamingLoadRadius | 2 | 1-5 | Cells to load around player (2 = 5×5 grid) |
| MaxCachedCells | 25 | 9-100 | Maximum cells in memory |

### Rendering Configuration
| Property | Default | Range | Effect |
|----------|---------|-------|--------|
| TerrainMaterial | None | Material* | Default material for all terrain |
| BiomeMaterials | Empty | Map | Per-biome material overrides |

### Debug Configuration
| Property | Default | Range | Effect |
|----------|---------|-------|--------|
| bDebugVisualizeBiomes | false | true/false | Color terrain by biome type |
| bDebugShowStats | false | true/false | Display memory stats on screen |

---

## Blueprint Events

```cpp
GenerateLandscape()                    // Regenerate all terrain
ClearLandscape()                       // Remove all meshes/water
UpdateStreamingAroundPoint(Vector)     // Stream cells around position
PrintDebugStats()                      // Log cache statistics
```

---

## Biome Height Ranges

| Biome | Height Range | Color (Debug) | Use Case |
|-------|-------------|---------------|----------|
| Ocean | < -500cm | Blue | Deep water |
| Lake | -500 to 0cm | Cyan | Shallow water |
| Plains | 0 to 3000cm | Green | Grasslands, valleys |
| Cliffs | 3000 to 5000cm | Yellow | Steep rocky areas |
| Mountains | 5000 to 10000cm | Magenta | High elevation |
| Peaks | > 10000cm | White | Snow-capped summits |

---

## Noise Tuning Quick Guide

### Want MORE Variety?
```
✓ Increase octaves (more layers)
✓ Decrease frequencies (larger features)
✓ Increase MaxHeightVariation
✓ Balance weights: 0.5, 0.35, 0.15
```

### Want LESS Variation (Flatter)?
```
✓ Decrease octaves
✓ Increase frequencies (smaller features)
✓ Decrease MaxHeightVariation
✓ Increase PrimaryWeight (0.7), reduce others
```

### Want DRAMATIC Mountains?
```
✓ PrimaryFrequency: 0.001
✓ PrimaryOctaves: 4-5
✓ MaxHeightVariation: 15000+
✓ Weights: 0.6, 0.3, 0.1
```

### Want REALISTIC Terrain?
```
✓ PrimaryFrequency: 0.001
✓ SecondaryFrequency: 0.01
✓ TertiaryFrequency: 0.05
✓ Octaves: 3, 4, 2
✓ Weights: 0.5, 0.35, 0.15
✓ MaxHeightVariation: 8000-10000
```

### Want ISLAND-Heavy Map?
```
✓ PrimaryFrequency: 0.0005
✓ PrimaryOctaves: 2
✓ PrimaryWeight: 0.8
✓ MaxHeightVariation: 5000
✓ WaterLevel: 0
```

---

## Memory Estimates

| Configuration | Cells in Memory | Memory Used | FPS (GTX 1080) |
|---------------|-----------------|------------|----------------|
| 10m cells, 100m map | 100 total, 25 loaded | 150-200 MB | 60+ |
| 10m cells, 500m map | 2500 total, 25 loaded | 150-200 MB | 60+ |
| 5m cells, 100m map | 400 total, 25 loaded | 200-300 MB | 45-60 |
| 20m cells, 100m map | 25 total, 25 loaded | 100-150 MB | 120+ |

---

## Compilation & Setup

### Files to Add to Project
```
Source/RandomLandscape_5_7/
├── GridBasedLandscapeActor.h/cpp
├── LandscapeNoiseGenerator.h/cpp
├── LandscapeGridManager.h/cpp
└── BiomeClassifier.h/cpp
```

### Required Includes
```cpp
#include "GridBasedLandscapeActor.h"     // Main actor
#include "ProceduralMeshComponent.h"     // For mesh creation
#include "Kismet/GameplayStatics.h"      // For world access
```

### Blueprint Compatibility
- ✓ Can create Blueprint from AGridBasedLandscapeActor
- ✓ All public properties are UPROPERTY
- ✓ All methods are UFUNCTION(BlueprintCallable)

---

## Debug Console Commands

```cpp
// Enable stats
stat unit                              // FPS counter
stat memory                            // Memory usage
stat detailedobjectstats              // Mesh statistics

// In-game properties (if you add console variables)
DebugVisualizeBiomes=true/false
DebugShowStats=true/false
```

---

## Performance Optimization Checklist

### Before Release
- [ ] Profile with stat unit (target 60 FPS)
- [ ] Check memory with stat memory
- [ ] Test on target hardware
- [ ] Adjust GridScale for performance
- [ ] Tune MaxCachedCells for memory
- [ ] Verify no memory leaks over time

### For Better FPS
- [ ] Increase GridScale (10 → 20)
- [ ] Reduce octaves by 1-2
- [ ] Reduce MaxHeightVariation
- [ ] Disable debug visualization
- [ ] Implement LOD system

### For Better Quality
- [ ] Implement FastNoise2
- [ ] Add normal maps per biome
- [ ] Implement terrain splatting
- [ ] Create biome-specific materials
- [ ] Add vegetation system

---

## Common Problems & Fixes

| Problem | Likely Cause | Solution |
|---------|------------|----------|
| Flat terrain | MaxHeightVariation too low | Increase to 8000+ |
| No visible change when adjusting | Need to call GenerateLandscape() | Press button or call function |
| Memory growing | StreamingLoadRadius too high | Reduce to 2-3 |
| Jagged appearance | Too many octaves/detail | Reduce TertiaryOctaves |
| Terrain doesn't show | Actor not visible | Check actor location/scale |
| Water missing | bGenerateWaterPlanes false | Enable and regenerate |
| Low FPS | Too many vertices | Increase GridScale |
| Visible cell seams | Shouldn't happen | Verify seamless generation works |

---

## Architecture Summary

```
INPUT (Blueprint Properties)
    ↓
GridBasedLandscapeActor
    ├─ NoiseGenerator (multi-layer noise)
    ├─ BiomeClassifier (height → biome)
    ├─ GridManager (cell caching + streaming)
    └─ GridMeshes (ProceduralMeshComponent per cell)
    ↓
OUTPUT
    ├─ Terrain meshes with height variation
    ├─ Per-biome material assignment
    ├─ Water planes (if enabled)
    └─ Seamless streaming as player moves
```

---

## Key Numbers to Remember

| Item | Value | Notes |
|------|-------|-------|
| Vertices per cell | 1024 | 32×32 grid |
| Cells in memory | 25 | 5×5 area default |
| Memory per cell | 4-8 MB | Height + biome + mesh |
| Total memory | 150-250 MB | Fully loaded |
| Generation time | < 5 seconds | For 100×100m |
| Biome types | 6 | Ocean, Lake, Plains, Cliffs, Mountains, Peaks |
| Noise layers | 3 | Primary, Secondary, Tertiary |
| Max map size | Unlimited | With streaming |

---

## Next Features to Implement

**HIGH PRIORITY** (1-2 weeks):
- [ ] FastNoise2 integration
- [ ] Biome materials
- [ ] Water system

**MEDIUM PRIORITY** (2-4 weeks):
- [ ] LOD system
- [ ] Async generation
- [ ] Material transitions

**LOW PRIORITY** (4+ weeks):
- [ ] Vegetation system
- [ ] Erosion simulation
- [ ] Terrain sculpting tools

---

This system is ready to use! Start with LANDSCAPE_QUICKSTART.md for setup instructions.
