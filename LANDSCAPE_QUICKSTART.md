# Procedural Landscape Generation - Quick Start Guide

## Setup Instructions

### 1. Add Actor to Level

1. Open your level in Unreal Editor
2. In the **Content Browser**, navigate to the C++ Classes folder
3. Search for or find **AGridBasedLandscapeActor**
4. Drag it into the level to spawn an instance
5. Select it in the outliner

### 2. Configure Basic Settings

In the **Details** panel, set these properties:

**Map Configuration**:
- `MapSize`: 100 (100 meters)
- `GridScale`: 10 (10 meter cells)

**Height Configuration**:
- `MaxHeightVariation`: 10000 (100 meters of height range)
- `HeightOffset`: 0 (sea level)

**Water Configuration**:
- `WaterLevel`: 0 (places water at sea level)
- `bGenerateWaterPlanes`: true (generate water)

### 3. Generate Landscape

Click the **Generate Landscape** button in the Details panel (or call from Blueprint).

You should see the terrain spawn with height variation!

## Customizing Terrain

### Example 1: Mountainous Terrain

```
MapSize: 200
GridScale: 10
MaxHeightVariation: 15000          (More dramatic heights)
PrimaryNoiseFrequency: 0.001
PrimaryNoiseOctaves: 4              (More octaves = more detail)
SecondaryNoiseFrequency: 0.01
SecondaryNoiseOctaves: 5
TertiaryNoiseFrequency: 0.05
TertiaryNoiseOctaves: 3
PrimaryWeight: 0.6                  (Emphasize large features)
SecondaryWeight: 0.3
TertiaryWeight: 0.1
```

### Example 2: Flat Plains with Islands

```
MapSize: 200
MaxHeightVariation: 5000            (Less variation)
PrimaryNoiseFrequency: 0.0005       (Larger features)
PrimaryNoiseOctaves: 2
SecondaryNoiseFrequency: 0.005
SecondaryNoiseOctaves: 3
TertiaryNoiseFrequency: 0.05
TertiaryNoiseOctaves: 1
PrimaryWeight: 0.8                  (Emphasize continents)
SecondaryWeight: 0.15
TertiaryWeight: 0.05
WaterLevel: 0                        (Create islands)
```

### Example 3: Detailed, Realistic Terrain

```
MapSize: 100
GridScale: 5                         (Finer cells for detail)
MaxHeightVariation: 8000
PrimaryNoiseFrequency: 0.001
PrimaryNoiseOctaves: 3
SecondaryNoiseFrequency: 0.01
SecondaryNoiseOctaves: 4
TertiaryNoiseFrequency: 0.1         (More detail)
TertiaryNoiseOctaves: 3
PrimaryWeight: 0.5
SecondaryWeight: 0.35
TertiaryWeight: 0.15
```

## Debug Visualization

### Enable Height Visualization

1. Check **bDebugVisualizeBiomes** in actor properties
2. The terrain will be colored by biome type:
   - **Blue**: Ocean (height < -500cm)
   - **Cyan**: Lake (-500 to 0cm)
   - **Green**: Plains (0 to 3000cm)
   - **Yellow**: Cliffs (3000 to 5000cm)
   - **Magenta**: Mountains (5000 to 10000cm)
   - **White**: Peaks (> 10000cm)

### Show Memory Stats

1. Check **bDebugShowStats**
2. On-screen display shows: "Landscape Stats - Cells: X/25"
3. Monitor memory usage as camera moves
4. See cells load/unload in real-time

## Understanding Biome Ranges

The system automatically classifies terrain into these height ranges (in Unreal units/cm):

```
Height Distribution:
    10000+ cm │ [Peaks] Snow-capped mountains
             │
     5000 cm │ [Mountains] High terrain with cliffs
             │
     3000 cm │ [Cliffs] Steep rocky slopes
             │
        0 cm │ [Plains] Grasslands and valleys (Sea Level)
             │
      -500 cm│ [Lakes] Shallow water
             │
      -FLT   │ [Ocean] Deep water
```

These ranges are defined in `FBiomeClassifier::InitializeDefaultBiomes()`. You can modify them by:

1. Editing the biome height ranges in **BiomeClassifier.cpp**
2. Recompiling the project
3. Regenerating the landscape

## Performance Tips

### For Better Performance

1. **Reduce GridScale** (e.g., 20 instead of 10)
   - Fewer vertices per cell
   - Simpler meshes
   - Trade-off: Lower detail

2. **Reduce MapSize**
   - Fewer cells total
   - Faster generation

3. **Reduce MaxHeightVariation**
   - Smoother, less dramatic terrain

4. **Reduce Noise Octaves**
   - Fewer detail layers
   - Faster generation

5. **Disable WaterPlanes** if not needed
   - Saves memory
   - Faster generation

### For Better Quality

1. **Increase Octaves** in noise settings
   - More detailed terrain

2. **Decrease GridScale** (e.g., 5 instead of 10)
   - More vertices per cell
   - Smoother surfaces

3. **Increase MaxHeightVariation**
   - More dramatic terrain features

4. **Fine-tune noise frequencies**
   - Adjust for desired terrain characteristics

## Streaming Behavior

The system automatically manages memory by streaming cells:

```
┌─────────────────────┐
│                     │
│    Loaded Cells     │  (5×5 grid area by default)
│   (In Memory)       │
│                     │
└─────────────────────┘
        ↓ Player moves
┌─────────────────────┐
│  (new cells load)   │
│  (old cells unload) │
└─────────────────────┘
```

**Streaming Radius**: Controls how many cells away to load
- Default: 2 (loads 5×5 grid = 25 cells)
- Increase for smoother camera movement
- Decrease to save memory

## Common Issues & Solutions

### Issue: Flat Terrain (No Height Variation)
**Solution**: 
- Increase `MaxHeightVariation` 
- Check noise frequencies aren't too small
- Verify `HeightOffset` isn't too high

### Issue: Too Much Jagged Detail
**Solution**:
- Reduce `TertiaryNoiseOctaves`
- Reduce `TertiaryWeight`
- Increase `GridScale` (fewer cells)

### Issue: Memory Growing Too Much
**Solution**:
- Check `MaxCachedCells` in GridManager (defaults to 25)
- Reduce `StreamingLoadRadius`
- Close other applications

### Issue: Terrain Looks Blocky/Low Detail
**Solution**:
- Increase octaves in noise settings
- Decrease `GridScale` for finer cells
- Reduce `GridScale` to 5 for higher detail

### Issue: Generation Takes Too Long
**Solution**:
- Reduce `MapSize`
- Increase `GridScale` (larger cells = fewer of them)
- Reduce `PrimaryNoiseOctaves`

## Next Steps

### Implementing FastNoise2 Integration
Replace the placeholder noise in `FLandscapeNoiseGenerator::GetNoise()` with actual FastNoise2:

```cpp
// TODO: Integrate FastNoise2 library
// Current implementation uses simple sine-based pseudo-noise
// Replace with FastNoise2::CreateNoise() for better quality
```

### Adding Materials

1. Create a material for each biome
2. Assign in the **BiomeMaterials** map:
   - Key: "Ocean", "Lake", "Plains", "Cliffs", "Mountains", "Peaks"
   - Value: Your material asset

### Adding Water Physics
The water plane framework is ready; implement:
- Wave simulation
- Water volume for swimming
- Splash effects

### Adding Vegetation
Use the biome information to place:
- Grass decals on plains
- Trees on cliffs
- Snow on peaks

## Blueprint Usage

Create a Blueprint from **AGridBasedLandscapeActor** to:
- Set default values for your terrain type
- Add events for generation completion
- Integrate with level loading systems

Example Blueprint Event:
```
Event Begin Play
  → Generate Landscape
  → (wait for meshes to appear)
  → Spawn Player
```

## Performance Profiling

Use Unreal's built-in tools:

1. **Stat Unit**: Shows FPS
   - Console: `stat unit`

2. **Stat Memory**: Shows memory usage
   - Console: `stat memory`

3. **Stat DetailedObjectStats**: Shows mesh stats
   - Console: `stat detailedobjectstats`

## Support

For questions or issues:
1. Check the **LANDSCAPE_ARCHITECTURE.md** for technical details
2. Review the code comments in the .cpp files
3. Enable debug visualization to understand what's happening
4. Check console for error messages

Happy terrain generation!
