# Biome Map System - User Guide

## Overview

The Biome Map System provides a way to:
1. **Know where each biome is** - Query biome at any world position
2. **See biome distribution** - Get percentage coverage and minimap texture
3. **Spawn content correctly** - Built-in rules prevent spawning inappropriate content (no mountains in desert!)
4. **Generate spawn points** - Get valid positions for biome-specific content

---

## Quick Start

### In Blueprints

```
// Get biome at player location
Biome = LandscapeActor->GetBiomeAtPosition(PlayerLocation)

// Check if you can spawn trees here
CanSpawn = LandscapeActor->CanSpawnCategoryAtPosition(Location, ESpawnableCategory::Trees)

// Get 50 tree spawn points in Plains biome
SpawnPoints = LandscapeActor->GetSpawnPointsForBiome(ELandscapeBiome::Plains, ESpawnableCategory::Trees, 50, 5.0)

// Get biome coverage statistics
Stats = LandscapeActor->GetBiomeStatistics()
PlainsPercent = Stats.Percentages[ELandscapeBiome::Plains]  // e.g., 35.2%

// Generate minimap texture
MinimapTexture = LandscapeActor->GenerateBiomeMapTexture()
```

### In C++

```cpp
// Get the biome map system
UBiomeMapSystem* BiomeMap = LandscapeActor->GetBiomeMapSystem();

// Query biome at position
ELandscapeBiome Biome = BiomeMap->GetBiomeAtPosition(WorldPosition);

// Check if cacti can spawn here (will return true only in Desert!)
bool bCanSpawn = BiomeMap->CanSpawnAtPosition(Position, ESpawnableCategory::Cacti);

// Get spawn density multiplier (0.0-1.0)
float Density = BiomeMap->GetSpawnDensityAtPosition(Position, ESpawnableCategory::Trees);

// Generate spawn points with custom request
FBiomeSpawnRequest Request;
Request.TargetBiome = ELandscapeBiome::Mountains;
Request.Category = ESpawnableCategory::OreDeposits;
Request.Count = 100;
Request.MinSpacing = 20.0f;  // 20 meters apart
TArray<FBiomeSpawnPoint> Points = BiomeMap->GenerateSpawnPoints(Request);
```

---

## Spawnable Categories

The system defines categories of content that can be placed:

| Category | Description | Best Biomes |
|----------|-------------|-------------|
| **Trees** | Living trees | Plains |
| **Bushes** | Shrubs, hedges | Plains, sparse elsewhere |
| **Grass** | Ground cover | Plains |
| **Flowers** | Decorative plants | Plains |
| **Cacti** | Desert plants | **Desert only!** |
| **DeadTrees** | Dead/barren trees | Desert, Canyon |
| **Rocks** | Small rocks | All biomes |
| **Boulders** | Large rocks | Mountains, Canyon |
| **Cliffs** | Cliff formations | Mountains, Canyon |
| **Crystals** | Crystal deposits | Mountains |
| **Ruins** | Ancient structures | All land biomes |
| **Camps** | Small settlements | Plains, Desert, Canyon |
| **Villages** | Larger settlements | Plains |
| **Towers** | Watchtowers, spires | Mountains |
| **Caves** | Cave entrances | Mountains, Canyon |
| **OreDeposits** | Mineable resources | Mountains (dense), Canyon |
| **WaterSources** | Springs, wells | Plains, rare in Desert |
| **Chests** | Treasure | All biomes (rare) |
| **PassiveCreatures** | Wildlife | Plains (dense), others sparse |
| **HostileCreatures** | Enemies | Canyon (dense), Mountains |
| **NPCs** | Characters | Plains |
| **Landmarks** | Special locations | All biomes |
| **POIs** | Points of interest | All biomes |

---

## Default Spawn Rules Per Biome

### Ocean 🌊
- **Allowed**: Rocks, Ruins (sunken), Chests (treasure), Fish, Sharks
- **NOT Allowed**: Trees, Grass, Villages, NPCs (underwater!)

### Plains 🌿
- **Dense**: Grass, Bushes, Wildlife
- **Normal**: Trees, Flowers, Villages, Camps, NPCs
- **Sparse**: Rocks, Water sources, Ruins
- **NOT Allowed**: Cacti, Cliffs, Crystals, Caves

### Desert 🏜️
- **Normal**: Cacti (exclusive!), Rocks, Ruins, Hostile creatures
- **Sparse**: Dead trees, Bandits, Ore, Chests
- **Rare**: Bushes, Water (oases!)
- **NOT Allowed**: Trees, Grass, Flowers, Villages

### Mountains ⛰️
- **Very Dense**: Rocks
- **Dense**: Boulders, Ore deposits
- **Normal**: Cliffs, Caves, Hostile creatures, Landmarks
- **Sparse**: Trees (lower elevations), Crystals
- **NOT Allowed**: Cacti, Flowers, Villages

### Canyon 🏜️
- **Very Dense**: Rocks
- **Dense**: Boulders, Cliffs, Caves, Hostile creatures
- **Normal**: Ruins, Ore, Chests
- **Sparse**: Dead trees, Crystals, Water (rivers)
- **NOT Allowed**: Trees, Grass, Flowers, Villages, NPCs

---

## Spawn Density Levels

| Level | Multiplier | Meaning |
|-------|------------|---------|
| None | 0.0 | Never spawns |
| Rare | 0.1 | Very few instances |
| Sparse | 0.25 | Occasional |
| Normal | 0.5 | Standard amount |
| Dense | 0.75 | Many instances |
| VeryDense | 1.0 | Packed |

---

## Customizing Rules

You can modify spawn rules at runtime:

```cpp
// Get the biome map system
UBiomeMapSystem* BiomeMap = LandscapeActor->GetBiomeMapSystem();

// Get current rules for Desert
FBiomeSpawnRules DesertRules = BiomeMap->GetBiomeRules(ELandscapeBiome::Desert);

// Allow trees in desert (sparse oases)
DesertRules.AllowedSpawnables.Add(ESpawnableCategory::Trees, ESpawnDensity::Rare);

// Increase hostile creature density
DesertRules.AllowedSpawnables.Add(ESpawnableCategory::HostileCreatures, ESpawnDensity::Dense);

// Apply modified rules
BiomeMap->SetBiomeRules(ELandscapeBiome::Desert, DesertRules);
```

---

## Biome Map Texture (Minimap)

Generate a texture for UI display:

```cpp
UTexture2D* MinimapTexture = LandscapeActor->GenerateBiomeMapTexture();
```

Default colors:
- **Ocean**: Deep Blue (0.0, 0.2, 0.6)
- **Plains**: Green (0.4, 0.7, 0.3)
- **Desert**: Sandy Yellow (0.9, 0.8, 0.4)
- **Mountains**: Gray (0.5, 0.5, 0.55)
- **Canyon**: Reddish Brown (0.7, 0.4, 0.3)

---

## Height & Slope Constraints

Each biome has height and slope limits:

| Biome | Min Height | Max Height | Max Slope |
|-------|------------|------------|-----------|
| Ocean | -1000m | 0m | 60° |
| Plains | 0m | 100m | 30° |
| Desert | 0m | 150m | 45° |
| Mountains | 50m | 500m | 70° |
| Canyon | -50m | 200m | 80° |

These affect what can spawn where - steep cliffs won't have grass!

---

## Statistics

Get coverage information:

```cpp
FBiomeStatistics Stats = LandscapeActor->GetBiomeStatistics();

// Total cells in the map
int32 Total = Stats.TotalCells;

// Per-biome data
for (auto& Pair : Stats.Percentages)
{
    ELandscapeBiome Biome = Pair.Key;
    float Percent = Pair.Value;
    int32 CellCount = Stats.CellCounts[Biome];
    
    UE_LOG(LogTemp, Log, TEXT("%s: %.1f%% (%d cells)"), 
        *GetBiomeName(Biome), Percent, CellCount);
}
```

---

## Advanced: Spawn Point Generation

Full control over spawn point generation:

```cpp
FBiomeSpawnRequest Request;
Request.TargetBiome = ELandscapeBiome::Plains;
Request.Category = ESpawnableCategory::Trees;
Request.Count = 1000;
Request.MinSpacing = 10.0f;  // 10 meters minimum between trees
Request.Seed = 12345;        // For reproducible results
Request.bOnlyBoundaries = false;  // Set true for edge-only spawning
Request.bAvoidBoundaries = true;  // Avoid biome edges

TArray<FBiomeSpawnPoint> Points = BiomeMap->GenerateSpawnPoints(Request);

for (const FBiomeSpawnPoint& Point : Points)
{
    FVector Position = Point.Position;      // World position
    float Height = Point.Height;            // Terrain height at this point
    float Slope = Point.SlopeAngle;         // Slope in degrees
    bool bIsBoundary = Point.bIsBoundary;   // Near biome edge?
    
    // Spawn your content here!
    SpawnTree(Position);
}
```

---

## Performance Notes

- **Map Resolution**: Default 100x100 grid (10,000 cells)
- **Memory**: ~1-2 MB for biome map data
- **Generation Time**: < 100ms after landscape generation
- **Query Time**: O(1) for position lookups

Increase `MapResolution` on the BiomeMapSystem for finer detail (at cost of memory).
