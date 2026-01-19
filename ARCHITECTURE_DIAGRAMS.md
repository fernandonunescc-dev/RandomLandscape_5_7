# System Design & Architecture Diagrams

## System Architecture Overview

```
┌─────────────────────────────────────────────────────────┐
│         AGridBasedLandscapeActor (Main Actor)           │
│                                                          │
│  ┌─────────────────┬─────────────────┬────────────────┐ │
│  │  Configuration  │  Internal State │  Components    │ │
│  ├─────────────────┼─────────────────┼────────────────┤ │
│  │ • MapSize       │ • GridManager   │ • RootScene    │ │
│  │ • GridScale     │ • NoiseGen      │ • GridMeshes   │ │
│  │ • MaxHeight     │ • BiomeClassif  │ • WaterPlanes  │ │
│  │ • WaterLevel    │                 │                │ │
│  └─────────────────┴─────────────────┴────────────────┘ │
└─────────────────────────────────────────────────────────┘
         │                    │                    │
         ▼                    ▼                    ▼
    ┌──────────────┐   ┌──────────────┐   ┌──────────────┐
    │   Noise      │   │  Biome       │   │   Grid       │
    │   Generator  │   │  Classifier  │   │   Manager    │
    │              │   │              │   │              │
    │ • Multi-     │   │ • 6 Biomes   │   │ • Cell Cache │
    │   layer      │   │ • Height     │   │ • Streaming  │
    │   noise      │   │   ranges     │   │ • LRU evict  │
    │ • Frequencies│   │ • Per-vertex │   │              │
    │ • Octaves    │   │   classify   │   │              │
    └──────────────┘   └──────────────┘   └──────────────┘
```

## Data Flow Diagram

```
┌─────────────────────────────────────────────────────────────────┐
│                    GENERATION PIPELINE                          │
└─────────────────────────────────────────────────────────────────┘

1. INITIALIZATION
   OnConstruction() / BeginPlay()
           │
           ▼
   Initialize GridManager with MapSize & GridScale
           │
           ▼
   ┌──────────────────────────────────────────┐
   │ GenerateLandscape()                      │
   │  For each grid cell (0..GridCountX/Y):  │
   │   ├─ GetOrCreateGridCell()               │
   │   ├─ Generate height map (noise)         │
   │   ├─ Convert to actual heights (cm)      │
   │   ├─ Classify each vertex's biome       │
   │   ├─ Find dominant biome for cell       │
   │   ├─ Create mesh from height map        │
   │   └─ Apply biome material               │
   └──────────────────────────────────────────┘
           │
           ▼

2. STREAMING (Every 0.5s)
   Tick()
    │
    ├─ Get player camera position
    │   │
    │   ▼
    ├─ Convert to grid coordinates
    │   │
    │   ▼
    └─ StreamCells(centerX, centerY, radius)
        │
        ├─ Mark cells in radius as active
        │
        ├─ Unload cells outside radius
        │   │
        │   ▼
        │   Cells unloaded from memory
        │
        └─ Memory stays constant (~150-250 MB)
```

## Grid Cell Generation Pipeline

```
GRID CELL (GridX, GridY)
         │
         ├─ Get grid bounds
         │
         ├─ Get or create FGridCellData
         │   │
         │   ├─ GridX, GridY
         │   ├─ HeightMap [1024 floats]
         │   ├─ BiomeMap [1024 enums]
         │   ├─ MinHeight, MaxHeight
         │   └─ bIsDataValid flag
         │
         ▼

NOISE GENERATION
         │
         ├─ For each vertex in cell:
         │   │
         │   ├─ Get world position
         │   │
         │   ├─ BlendNoiseLayers():
         │   │   ├─ Primary noise (0.001 freq)
         │   │   ├─ Secondary noise (0.01 freq)
         │   │   ├─ Tertiary noise (0.05 freq)
         │   │   └─ Blend with weights
         │   │
         │   └─ Store normalized height (-1..1)
         │
         ▼

HEIGHT CONVERSION & BIOME CLASSIFICATION
         │
         ├─ For each normalized height:
         │   │
         │   ├─ ActualHeight = (normalized * MaxHeightVar) + HeightOffset
         │   │
         │   ├─ BiomeType = ClassifyByHeight(ActualHeight)
         │   │   ├─ < -500cm → Ocean
         │   │   ├─ -500..0cm → Lake
         │   │   ├─ 0..3000cm → Plains
         │   │   ├─ 3000..5000cm → Cliffs
         │   │   ├─ 5000..10000cm → Mountains
         │   │   └─ > 10000cm → Peaks
         │   │
         │   └─ Store biome type
         │
         ▼

MESH CREATION
         │
         ├─ Get or create ProceduralMeshComponent
         │
         ├─ Build vertex array from height map
         │   ├─ Position.X = CellOriginX + (X * Spacing)
         │   ├─ Position.Y = CellOriginY + (Y * Spacing)
         │   └─ Position.Z = ActualHeight from height map
         │
         ├─ Build triangle indices
         │   └─ 2 triangles per quad (32×32 grid = 2046 tris)
         │
         ├─ Build UV coordinates (0..1 range)
         │
         └─ CreateMeshSection(Vertices, Triangles, UVs)
                    │
                    ▼

MATERIAL APPLICATION
         │
         ├─ Determine dominant biome
         │   └─ Count biome types, pick most common
         │
         └─ SetMaterial(0, BiomeMaterial[dominantBiome])
```

## Memory Layout

```
LOADED CELLS CACHE (Max 25 cells = ~5×5 grid)

┌─────────────────────────────────────────────────┐
│          TMap<int64, FGridCellData>             │
├─────────────────────────────────────────────────┤
│ Key: (GridX << 32) | GridY                      │
├─────────────────────────────────────────────────┤
│                                                  │
│  FGridCellData Cell_0_0                         │
│  ├─ GridX: 0                                    │
│  ├─ GridY: 0                                    │
│  ├─ HeightMap [1024 floats] = ~4 KB             │
│  ├─ BiomeMap [1024 enums] = ~1 KB               │
│  ├─ MinHeight: float = 8 B                      │
│  ├─ MaxHeight: float = 8 B                      │
│  ├─ bIsDataValid: bool = 1 B                    │
│  ├─ LastAccessTime: double = 8 B                │
│  └─ ProceduralMeshComponent Data = ~4 MB        │
│     TOTAL per cell: ~4-8 MB                     │
│                                                  │
│  ... (24 more cells) ...                        │
│                                                  │
└─────────────────────────────────────────────────┘

TOTAL: 25 cells × 6 MB avg = 150 MB
       (can be 100-250 MB depending on mesh complexity)
```

## Noise Generation Blending

```
NOISE LAYERS VISUALIZATION

Layer 1: Primary (Frequency 0.001, 3 Octaves)
┌──────────────────────────────────────┐
│    ▁▂▃▄▅▆▇█▆▅▄▃▂▁                   │ Large continents
│  ▂▃▄▅▆▇█▅▄▃▂▁▂▃▄▅▆▇█▆▅▄▃▂▁         │
│ ▃▄▅▆▇█▁▂▃▄▅▆▇█▆▅▄▃▂▁▂▃▄▅▆▇█▆▅▄    │
└──────────────────────────────────────┘
          Weight: 0.5 (50%)

Layer 2: Secondary (Frequency 0.01, 4 Octaves)
┌──────────────────────────────────────┐
│ ▕▏▁▂▃▂▁▕▏▁▂▃▂▁▕▏▁▂▃▂▁▕▏▁▂▃▂▁       │ Medium valleys/hills
│ ▂▃▄▃▂▁▂▃▄▃▂▁▂▃▄▃▂▁▂▃▄▃▂▁▂▃▄▃▂▁     │
│ ▃▄▅▄▃▂▃▄▅▄▃▂▃▄▅▄▃▂▃▄▅▄▃▂▃▄▅▄▃▂▃   │
└──────────────────────────────────────┘
          Weight: 0.35 (35%)

Layer 3: Tertiary (Frequency 0.05, 2 Octaves)
┌──────────────────────────────────────┐
│▕▏▕▏▕▏▕▏▕▏▕▏▕▏▕▏▕▏▕▏▕▏▕▏▕▏▕▏▕▏▕▏ │ Fine detail bumps
│▕▏▕▏▕▏▕▏▕▏▕▏▕▏▕▏▕▏▕▏▕▏▕▏▕▏▕▏▕▏▕▏ │
└──────────────────────────────────────┘
          Weight: 0.15 (15%)

BLENDING (50% + 35% + 15%)
┌──────────────────────────────────────┐
│    ▂▃▅▆▇█▇▅▄▂▁                      │ Combined result
│  ▂▄▆█▇▅▃▁▂▄▆█▇▅▄▃▂▁▂▄▆█▇▅▄▃▂     │
│ ▃▅▇█▇▅▂▁▂▄▆█▇▅▃▁▂▄▆█▇▅▃▁▂▄▆█     │
└──────────────────────────────────────┘

Result: Complex terrain with varied features at multiple scales
```

## Biome Height Distribution

```
HEIGHT RANGES (in Unreal units/cm)

 10000 ┌─────────────────────┐
       │     PEAKS [6]       │  Snow-capped mountains
       │   (White in debug)  │  Height > 10000cm
  5000 ├─────────────────────┤
       │   MOUNTAINS [5]     │  High terrain with cliffs
       │  (Magenta in debug) │  5000-10000cm
  3000 ├─────────────────────┤
       │    CLIFFS [4]       │  Steep rocky slopes
       │  (Yellow in debug)  │  3000-5000cm
     0 ├─────────────────────┤ ← SEA LEVEL (WaterLevel)
       │    PLAINS [3]       │  Grasslands and valleys
       │   (Green in debug)  │  0-3000cm
  -500 ├─────────────────────┤
       │    LAKES [2]        │  Shallow water
       │   (Cyan in debug)   │  -500-0cm
       │                     │
       │   OCEANS [1]        │  Deep water
       │   (Blue in debug)   │  < -500cm
       └─────────────────────┘

CONFIGURATION:
• WaterLevel: Controls where water plane appears (0 = sea level)
• MaxHeightVariation: Controls total height range (10000cm = 100m typical)
• HeightOffset: Shifts all heights up/down
```

## Streaming & Memory Management

```
CAMERA MOVEMENT SIMULATION

Step 1: Camera at Grid (2, 2), StreamRadius = 2
┌───────────────┐
│ . . . . . . . │
│ . L L L L . . │
│ . L L L L . . │
│ . L L C L . . │  C = Camera
│ . L L L L . . │  L = Loaded cells (5×5)
│ . L L L L . . │  . = Not loaded
│ . . . . . . . │
└───────────────┘
Memory: 25 cells loaded

Step 2: Camera moves to Grid (4, 2)
┌───────────────┐
│ . . . . . . . │
│ . . L L L L . │
│ . . L L L L . │
│ . . L C L L . │
│ . . L L L L . │
│ . . L L L L . │
│ . . . . . . . │
└───────────────┘
- 12 new cells load
- 12 old cells unload
Memory: Still ~25 cells

Step 3: Camera at edge Grid (6, 2)
┌───────────────┐
│ . . . . . . . │
│ . . . L L L . │
│ . . . L L L . │
│ . . . L C L . │
│ . . . L L L . │
│ . . . L L L . │
│ . . . . . . . │
└───────────────┘
Memory: Constant at 25 cells max

RESULT: Seamless terrain with constant memory usage
```

## Cell Boundary Seamlessness

```
ADJACENT CELL SHARING (Height Map Seams)

Cell (0,0)          Cell (1,0)
┌─────────┐        ┌─────────┐
│ H H H H │H│H H H H│
│ H H H H │H│H H H H│
│ H H H H │H│H H H H│
│ H H H H │H│H H H H│  ← Shared edge vertices
└─────────┘        └─────────┘

Cell (0,0)          Cell (0,1)
┌─────────┐
│ H H H H │
│ H H H H │
│ H H H H │
│ H H H H │  ← Shared edge vertices
├─────────┤
│ H H H H │
│ H H H H │
│ H H H H │
│ H H H H │
└─────────┘

SEAMLESS GENERATION:
• Each cell generates its own heights
• Edges use same noise values (deterministic)
• Same coordinates produce same noise
• No seams or gaps between cells
```

## Comparison: Original vs. New System

```
ORIGINAL SYSTEM (AProceduralLandscapeActor)
┌──────────────────────────────────────────┐
│ Single flat plane mesh                   │
│ • No height variation                    │
│ • No biome types                         │
│ • Fixed grid layout                      │
│ • High memory for large maps            │
│ • All vertices always rendered           │
│ • No streaming                           │
│ • No water system                        │
└──────────────────────────────────────────┘

NEW SYSTEM (AGridBasedLandscapeActor)
┌──────────────────────────────────────────┐
│ Grid-based procedural generation        │
│ • Multi-layer noise for variation       │
│ • 6 biome types with height ranges      │
│ • Per-cell mesh components              │
│ • Efficient memory management            │
│ • LOD-ready architecture                │
│ • Automatic streaming                   │
│ • Water system framework                │
│ • Blueprint configurable               │
└──────────────────────────────────────────┘

BENEFITS:
✓ 10-50x more height variation
✓ Visual biome diversity
✓ Constant memory usage (streaming)
✓ Scalable to huge maps
✓ Framework for water/vegetation
✓ Easy to configure and tweak
✓ Ready for performance optimization (LOD)
```

---

This architecture is designed to be:
- **Scalable**: Grows to multi-km maps with streaming
- **Performant**: Constant memory, efficient rendering
- **Extensible**: Framework for water, vegetation, erosion
- **Flexible**: Fully configurable via editor properties
- **Beautiful**: StarCraft Remastered-level detail target
