# Procedural Landscape Generation System - Architecture Guide

## Overview

This is a high-performance, grid-based procedural landscape generation system for Unreal Engine 5.7 designed to create detailed terrain with multiple biome types (plains, mountains, cliffs, canyons, oceans, and lakes).

## Key Features

- **Grid-Based Generation**: Landscape divided into manageable 10m × 10m cells
- **Per-Cell Height Maps**: Each cell has its own height map (32×32 vertices = 1,024 vertices per cell)
- **Procedural Noise**: Multi-layered Perlin/Simplex noise for realistic terrain variation
- **Biome Classification**: Automatic terrain classification into 6 biome types based on height
- **Streaming System**: Loads/unloads cells based on player distance (5×5 grid area default)
- **Material Blending**: Different materials per biome type with automatic selection
- **Water System**: Dynamic ocean and lake plane generation (in progress)

## Core Components

### 1. FLandscapeNoiseGenerator (Noise Generation)
**Files**: `LandscapeNoiseGenerator.h/cpp`

Responsible for procedurally generating height maps using multi-octave noise layering.

**Key Methods**:
- `GenerateHeightMap()`: Creates height map for a single grid cell
- `GetHeightAtWorldPosition()`: Gets height at any world coordinate
- `BlendNoiseLayers()`: Blends three noise layers (primary, secondary, tertiary)

**Properties**:
- `PrimaryNoiseFrequency` (0.001): Large-scale terrain features (continents)
- `SecondaryNoiseFrequency` (0.01): Medium-scale variation (valleys, hills)
- `TertiaryNoiseFrequency` (0.05): Fine detail (surface bumps)

**How It Works**:
1. Takes cell coordinates and generates noise at multiple frequencies
2. Combines three noise layers with weighted blending
3. Returns normalized height values (-1.0 to 1.0)
4. Heights are later scaled by `MaxHeightVariation` to get actual Unreal units

### 2. FBiomeClassifier (Terrain Classification)
**Files**: `BiomeClassifier.h/cpp`

Classifies terrain into biome types based on height ranges.

**Biome Types** (Height in Unreal units/cm):
- **Ocean**: < -500cm (deep water)
- **Lake**: -500 to 0cm (shallow water)
- **Plains**: 0 to 3000cm (flat, grassy areas)
- **Cliffs**: 3000 to 5000cm (steep slopes)
- **Mountains**: 5000 to 10000cm (high terrain)
- **Peaks**: > 10000cm (highest points, snow-capped)

**Key Methods**:
- `GetBiomeAtHeight()`: Returns biome type for a given height
- `InitializeDefaultBiomes()`: Sets up default height ranges

### 3. FLandscapeGridManager (Cell Caching)
**Files**: `LandscapeGridManager.h/cpp`

Manages grid cells with caching and streaming for performance.

**Key Features**:
- LRU (Least Recently Used) cache eviction
- Keeps max 25 cells in memory by default (5×5 area)
- Tracks access time per cell for streaming

**Key Methods**:
- `GetOrCreateGridCell()`: Gets or creates a cell in cache
- `StreamCells()`: Unloads cells outside player radius
- `GetCacheStats()`: Returns memory usage info

**How It Works**:
1. Uses 2D grid coordinates converted to a single 64-bit cache key
2. Maintains `LoadedCells` map with cell data
3. When cache is full, evicts least recently used cell
4. Streaming system calls `StreamCells()` each frame to manage memory

### 4. FGridCellData (Cell Data Structure)
**Part of**: `LandscapeGridManager.h`

Stores all data for a single grid cell:
```cpp
struct FGridCellData
{
    int32 GridX, GridY;              // Grid coordinates
    TArray<float> HeightMap;          // Height values (-1 to 1)
    TArray<EBiomeType> BiomeMap;      // Biome per vertex
    float MinHeight, MaxHeight;       // Height range for cell
    bool bIsDataValid;                // Is data generated?
    double LastAccessTime;            // For LRU eviction
};
```

### 5. AGridBasedLandscapeActor (Main Actor)
**Files**: `GridBasedLandscapeActor.h/cpp`

Main actor class that orchestrates the entire system.

**Exposed Properties**:

**Map Configuration**:
- `MapSize`: Total landscape size in meters (100m default)
- `GridScale`: Individual cell size in meters (10m default)

**Height Configuration**:
- `MaxHeightVariation`: Height range in cm (10000cm = 100m default)
- `HeightOffset`: Base height offset (0 default)

**Water Configuration**:
- `WaterLevel`: Height at which water plane is placed (0 default)
- `bGenerateWaterPlanes`: Enable water generation (true default)

**Noise Configuration**:
- All noise generator properties exposed for tweaking

**Debug Options**:
- `bDebugVisualizeBiomes`: Color terrain by biome type
- `bDebugShowStats`: Display memory stats on screen

**Key Methods**:
- `GenerateLandscape()`: Generate all visible cells
- `UpdateStreamingAroundPoint()`: Stream cells around a position
- `PrintDebugStats()`: Display cache statistics

## Generation Pipeline

### 1. Initialization (BeginPlay/OnConstruction)
```
Initialize GridManager with MapSize & GridScale
        ↓
GenerateLandscape()
        ↓
Update Noise Settings from properties
```

### 2. Landscape Generation
```
For each grid cell (0 to GridCountX/Y):
    ├─ Get or create cell data from GridManager
    ├─ Generate height map via NoiseGenerator
    ├─ Convert normalized heights to actual heights (cm)
    ├─ Classify biome for each vertex
    ├─ Create mesh component for cell
    ├─ Build vertices/triangles from height map
    ├─ Determine dominant biome
    └─ Apply biome-specific material
```

### 3. Streaming (Every 0.5 seconds)
```
Get player camera position
    ↓
Convert to grid coordinates
    ↓
Load cells in radius (default 2 = 5×5 grid)
    ↓
Unload cells outside radius (via GridManager.StreamCells)
```

## Performance Considerations

### Memory Usage
- **Per Cell**: ~4-8 MB (1024 float height map + biome map + mesh)
- **25 Cells Max**: ~100-200 MB (fully loaded world)
- **Streaming**: Only loads cells within radius, keeps memory constant

### LOD Strategy (Future Implementation)
For StarCraft Remastered level detail:

1. **High Detail Near Camera**:
   - Full 32×32 vertex grid per cell
   - Normal-mapped surfaces
   - Per-vertex biome classification

2. **Medium Detail Mid-Range**:
   - 16×16 simplified grid
   - Reduced draw calls

3. **Low Detail Far Away**:
   - 8×8 or merged cells
   - Single draw call per region
   - Merged meshes

### Optimization Techniques

1. **Height Map Caching**: Generate once, cache in memory
2. **Asynchronous Generation**: Generate in background thread (future)
3. **Mesh Pooling**: Reuse mesh components when possible
4. **Material Instancing**: Use dynamic material instances to vary per biome
5. **Frustum Culling**: Only render visible cells

## Water System (In Progress)

**Implementation Plan**:
1. Scan all cell vertices for heights below water level
2. For ocean areas: Place large water plane actors
3. For lakes: Create water plane per lake area
4. Use water-specific material with wave animation
5. Separate from terrain mesh for proper layering

## Noise Tuning Guide

### For Different Terrain Styles

**Realistic Terrain**:
```
PrimaryFrequency: 0.001 (continents)
PrimaryOctaves: 3
SecondaryFrequency: 0.01 (variation)
SecondaryOctaves: 4
TertiaryFrequency: 0.05 (detail)
TertiaryOctaves: 2
```

**More Dramatic Mountains**:
```
Increase PrimaryOctaves to 4
Increase MaxHeightVariation to 15000
Increase Secondary/TertiaryWeights
```

**Flatter Terrain**:
```
Reduce PrimaryFrequency to 0.0005
Reduce MaxHeightVariation to 5000
Increase PrimaryWeight, decrease Secondary/Tertiary
```

## Integration Notes

### With Existing Code
- Refactored from `AProceduralLandscapeActor` (old flat plane)
- Still in same project, can coexist with original
- No dependencies on original system

### Future Enhancements

1. **FastNoise2 Integration**: Replace placeholder noise with FastNoise2 library
2. **Water Simulation**: Add dynamic water with wave physics
3. **Vegetation System**: Add trees, grass based on biome/slope
4. **Erosion Simulation**: Apply erosion algorithm to heightmaps
5. **Texture Splatting**: Blend multiple textures per biome
6. **Runtime Generation**: Generate terrain while game is running
7. **Save/Load**: Serialize generated heightmaps to disk

## Testing Checklist

- [ ] Basic landscape generation (flat test)
- [ ] Height variation (check min/max heights)
- [ ] Biome classification (verify height ranges)
- [ ] Streaming behavior (monitor memory as camera moves)
- [ ] Material application (check biome colors with debug mode)
- [ ] Water plane generation (ocean/lake placement)
- [ ] Performance profiling (FPS with varying grid sizes)
- [ ] Edge cases (edges of map, transitions between cells)

## File Structure
```
Source/RandomLandscape_5_7/
├── LandscapeNoiseGenerator.h/cpp        (Noise generation)
├── BiomeClassifier.h/cpp                (Biome classification)
├── LandscapeGridManager.h/cpp           (Cell caching/streaming)
└── GridBasedLandscapeActor.h/cpp        (Main actor)
```

## References
- Perlin Noise: https://en.wikipedia.org/wiki/Perlin_noise
- FastNoise2: https://github.com/Auburn/FastNoise2
- UE5 Procedural Generation: https://docs.unrealengine.com/5.0/en-US/procedural-content-generation-in-unreal-engine/
