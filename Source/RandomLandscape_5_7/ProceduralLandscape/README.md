# Procedural Landscape System

A simple procedural terrain generation system for Unreal Engine 5 using fractal noise and ProceduralMeshComponent.

## Quick Start

1. **Create Actor**: Drag `ProceduralLandscapeActor` into your level
2. **Configure**: Set map size, height, and noise settings in Details panel
3. **Play**: Terrain generates automatically!

## Features

- **Procedural Mesh**: Creates terrain using ProceduralMeshComponent for runtime generation
- **Fractal Noise**: Generates realistic terrain using configurable noise parameters
- **Async Generation**: Background thread support for runtime generation
- **Auto-Regeneration**: Automatically updates when settings change in editor
- **Chunked Generation**: Large terrains are split into chunks for better performance

## Configuration

### Map Settings

| Property | Description | Default |
|----------|-------------|---------|
| `MapSizeMeters` | Total map size (X by X meters) | 500 |
| `MaxHeightMeters` | Maximum terrain height | 50 |
| `Resolution` | Detail level (1-100) | 50 |

### Noise Settings

| Property | Description | Default | Range |
|----------|-------------|---------|-------|
| `TerrainSeed` | Random seed (same seed = same terrain) | 1337 | Any integer |
| `NoiseFrequency` | Zoom level - lower = larger features | 0.003 | 0.0001 - 0.1 |
| `NoiseOctaves` | Detail layers - more = more detail | 5 | 1 - 10 |
| `NoiseLacunarity` | Frequency multiplier between octaves | 2.0 | 1.5 - 3.0 |
| `NoiseGain` | Amplitude multiplier between octaves | 0.5 | 0.3 - 0.7 |
| `HeightExponent` | Height distribution (1=uniform, higher=flatter with rare peaks) | 1.0 | 0.5 - 10.0 |

### Material Settings

| Property | Description |
|----------|-------------|
| `TerrainMaterial` | Material to apply to the terrain mesh |

### Generation Settings

| Property | Description | Default |
|----------|-------------|---------|
| `bUseAsyncGeneration` | Use background threads | true |
| `bAutoRegenerateInEditor` | Auto-update on setting changes | true |
| `bDebugShowStats` | Show generation stats on screen | false |

## Noise Frequency Guide

The `NoiseFrequency` has the biggest visual impact:

| Frequency | Result |
|-----------|--------|
| 0.001 | Very large, rolling hills |
| 0.003 | Medium-sized terrain features |
| 0.005 | Smaller hills |
| 0.01 | Very small, bumpy terrain |

## Resolution Guide

| Resolution | Quality | Use Case |
|------------|---------|----------|
| 1-25 | Low | Large maps, better performance |
| 25-50 | Medium | Standard gameplay |
| 50-75 | High | Detailed terrain |
| 75-100 | Ultra | Maximum detail |

## Files

| File | Purpose |
|------|---------|
| `ProceduralLandscapeActor.h/cpp` | Main actor you place in level |
| `LandscapeNoiseService.h/cpp` | Fractal noise generation |
| `LandscapeMeshBuilder.h/cpp` | Creates mesh from height data |
| `LandscapeAsyncGenerator.h/cpp` | Background thread support |
| `LandscapeTypes.h` | Shared data structures |

## How It Works

1. **Noise Generation**: Fractal noise creates a height value for each vertex
2. **Height Mapping**: Heights are scaled to the configured range
3. **Mesh Building**: Vertices, triangles, normals, and UVs are computed
4. **Chunking**: Large terrains are split into chunks for async generation
5. **Material**: Apply your own material to customize the look!

## Tips

- **Change the seed** to get completely different terrain
- **Lower frequency** = larger, smoother hills
- **More octaves** = more small-scale detail
- **Higher resolution** = more vertices but slower

## Example Configurations

### Rolling Plains
```
NoiseFrequency: 0.002
NoiseOctaves: 4
MaxHeightMeters: 30
HeightExponent: 1.5
```

### Mountain Range
```
NoiseFrequency: 0.003
NoiseOctaves: 6
MaxHeightMeters: 150
HeightExponent: 1.0
```

### Gentle Hills
```
NoiseFrequency: 0.001
NoiseOctaves: 3
MaxHeightMeters: 20
HeightExponent: 2.0
```

## Blueprint Functions

| Function | Description |
|----------|-------------|
| `RegenerateLandscape()` | Regenerate the entire landscape |
| `ClearLandscape()` | Remove all generated meshes |
| `GetMapSizeMeters()` | Get the total map size in meters |
| `GetTotalTriangleCount()` | Get total triangle count |
