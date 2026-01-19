# Code Examples & Integration Guide

## Blueprint Usage Examples

### Example 1: Basic Landscape Generation

**Blueprint Event Graph**:
```
Event BeginPlay
  │
  ├─ Cast to GridBasedLandscapeActor
  │
  ├─ Set MapSize = 200
  │
  ├─ Set GridScale = 10
  │
  ├─ Set MaxHeightVariation = 10000
  │
  ├─ Set WaterLevel = 0
  │
  └─ Call GenerateLandscape()
```

**In C++**:
```cpp
void AMyLevelManager::SetupLandscape()
{
    AGridBasedLandscapeActor* Landscape = GetWorld()->SpawnActor<AGridBasedLandscapeActor>();
    
    Landscape->MapSize = 200.0f;
    Landscape->GridScale = 10.0f;
    Landscape->MaxHeightVariation = 10000.0f;
    Landscape->WaterLevel = 0.0f;
    
    Landscape->GenerateLandscape();
}
```

---

## Advanced Examples

### Example 2: Procedural Material Assignment

**Override in Blueprint or C++**:
```cpp
void AMyLandscapeManager::AssignBiomeMaterials()
{
    AGridBasedLandscapeActor* Landscape = GetLandscapeActor();
    
    // Create or load materials
    UMaterialInterface* OceanMat = LoadObject<UMaterialInterface>(nullptr, 
        TEXT("/Game/Materials/M_Ocean"));
    UMaterialInterface* PlainsKMat = LoadObject<UMaterialInterface>(nullptr,
        TEXT("/Game/Materials/M_Plains"));
    UMaterialInterface* MountainMat = LoadObject<UMaterialInterface>(nullptr,
        TEXT("/Game/Materials/M_Mountain"));
    
    // Assign to landscape (if you add this functionality)
    // Landscape->BiomeMaterials.Add("Ocean", OceanMat);
    // Landscape->BiomeMaterials.Add("Plains", PlainsMat);
    // Landscape->BiomeMaterials.Add("Mountains", MountainMat);
}
```

### Example 3: Dynamic Height Queries

**Getting height at world position**:
```cpp
float AMyCharacter::GetTerrainHeightBelow()
{
    // Get landscape actor
    AGridBasedLandscapeActor* Landscape = Cast<AGridBasedLandscapeActor>(
        UGameplayStatics::GetActorOfClass(GetWorld(), AGridBasedLandscapeActor::StaticClass())
    );
    
    if (!Landscape)
        return 0.0f;
    
    // Get height at character position
    FVector CharPos = GetActorLocation();
    float HeightAtPos = Landscape->NoiseGenerator.GetHeightAtWorldPosition(
        CharPos.X, CharPos.Y
    );
    
    return HeightAtPos;
}
```

### Example 4: Streaming Control

**Manual streaming around specific point**:
```cpp
void AMyGameMode::StreamAroundInterestPoint(const FVector& Point)
{
    AGridBasedLandscapeActor* Landscape = GetLandscape();
    
    // Update streaming radius based on gameplay
    Landscape->StreamingLoadRadius = 3;  // 7×7 grid
    
    // Stream cells around point
    Landscape->UpdateStreamingAroundPoint(Point);
}
```

### Example 5: Regeneration with Different Settings

**Generate multiple terrain variations**:
```cpp
void ATerrainTestMap::GenerateTerrainVariations()
{
    AGridBasedLandscapeActor* Landscape = GetLandscape();
    
    // Variation 1: Mountainous
    Landscape->MaxHeightVariation = 15000.0f;
    Landscape->PrimaryNoiseOctaves = 4;
    Landscape->GenerateLandscape();
    
    // Wait for generation...
    GetWorld()->GetTimerManager().SetTimer(
        FTimerHandle(),
        [this, Landscape]() {
            // Variation 2: Flatter
            Landscape->MaxHeightVariation = 5000.0f;
            Landscape->PrimaryNoiseOctaves = 2;
            Landscape->ClearLandscape();
            Landscape->GenerateLandscape();
        },
        5.0f,
        false
    );
}
```

---

## Integration with Game Systems

### Example 6: AI Navigation Integration

**Getting walkable terrain for AI**:
```cpp
bool AMyAIController::IsTerrainWalkable(const FVector& Location)
{
    AGridBasedLandscapeActor* Landscape = GetLandscape();
    
    // Get terrain height
    float Height = Landscape->NoiseGenerator.GetHeightAtWorldPosition(
        Location.X, Location.Y
    );
    
    // Get biome type at this height
    EBiomeType Biome = Landscape->BiomeClassifier.GetBiomeAtHeight(Height);
    
    // Define walkable biomes
    switch (Biome)
    {
        case EBiomeType::Plains:
        case EBiomeType::Cliffs:  // Can climb cliffs
            return true;
            
        case EBiomeType::Ocean:
        case EBiomeType::Lake:
            return false;  // Can't walk on water
            
        default:
            return true;
    }
}
```

### Example 7: Environmental Effects Based on Terrain

**Apply different effects by biome**:
```cpp
void AMyWeatherSystem::ApplyTerrainEffects()
{
    AGridBasedLandscapeActor* Landscape = GetLandscape();
    
    for (float Y = -5000; Y < 5000; Y += 1000)
    {
        for (float X = -5000; X < 5000; X += 1000)
        {
            // Get height at this position
            float Height = Landscape->NoiseGenerator.GetHeightAtWorldPosition(X, Y);
            EBiomeType Biome = Landscape->BiomeClassifier.GetBiomeAtHeight(Height);
            
            // Apply effects
            switch (Biome)
            {
                case EBiomeType::Peaks:
                    SpawnSnow(FVector(X, Y, Height));
                    break;
                    
                case EBiomeType::Ocean:
                case EBiomeType::Lake:
                    SpawnWaterMist(FVector(X, Y, Height));
                    break;
                    
                case EBiomeType::Plains:
                    SpawnGrass(FVector(X, Y, Height));
                    break;
            }
        }
    }
}
```

### Example 8: Player Spawning on Terrain

**Spawn player on valid terrain**:
```cpp
void AMyGameMode::SpawnPlayerOnTerrain(APlayerController* PC)
{
    AGridBasedLandscapeActor* Landscape = GetLandscape();
    
    // Find valid spawn point
    FVector SpawnLocation = FVector(0, 0, 0);
    bool bFoundSpawn = false;
    
    for (int i = 0; i < 100 && !bFoundSpawn; i++)
    {
        float RandomX = FMath::RandRange(-5000.0f, 5000.0f);
        float RandomY = FMath::RandRange(-5000.0f, 5000.0f);
        
        float Height = Landscape->NoiseGenerator.GetHeightAtWorldPosition(
            RandomX, RandomY
        );
        
        EBiomeType Biome = Landscape->BiomeClassifier.GetBiomeAtHeight(Height);
        
        // Spawn on plains or cliffs, avoid water
        if (Biome == EBiomeType::Plains || Biome == EBiomeType::Cliffs)
        {
            SpawnLocation = FVector(RandomX, RandomY, Height + 100.0f);
            bFoundSpawn = true;
        }
    }
    
    APawn* PlayerPawn = GetWorld()->SpawnActor<APawn>(DefaultPawnClass,
        SpawnLocation, FRotator::ZeroRotator);
    PC->Possess(PlayerPawn);
}
```

---

## Custom Extensions

### Example 9: Custom Noise Modifier

**Extending with additional noise layers**:
```cpp
class FExtendedNoiseGenerator : public FLandscapeNoiseGenerator
{
public:
    // Add custom noise layer (e.g., cavern systems)
    float GenerateCavernNoise(float X, float Y)
    {
        // Custom noise for underground features
        return GetNoise(X, Y, 0.01f, 3);  // Different frequency
    }
    
    // Blend with original
    float GetHeightWithCaverns(float X, float Y)
    {
        float MainHeight = BlendNoiseLayers(X, Y);
        float CavernNoise = GenerateCavernNoise(X, Y);
        
        return FMath::Lerp(MainHeight, CavernNoise, 0.3f);
    }
};
```

### Example 10: Biome-Specific Density Maps

**Using height for vegetation density**:
```cpp
class FVegetationDensityCalculator
{
public:
    float GetGrassDensity(const FVector& Location, AGridBasedLandscapeActor* Landscape)
    {
        float Height = Landscape->NoiseGenerator.GetHeightAtWorldPosition(
            Location.X, Location.Y
        );
        
        EBiomeType Biome = Landscape->BiomeClassifier.GetBiomeAtHeight(Height);
        
        switch (Biome)
        {
            case EBiomeType::Plains:
                return 1.0f;  // Full density
            case EBiomeType::Cliffs:
                return 0.3f;  // Sparse
            case EBiomeType::Mountains:
                return 0.5f;  // Medium
            case EBiomeType::Peaks:
                return 0.1f;  // Very sparse
            default:
                return 0.0f;
        }
    }
};
```

---

## Debugging & Testing

### Example 11: Debug Visualization

**Enable various debug modes**:
```cpp
void ADebugLandscapeViewer::VisualizeLandscape()
{
    AGridBasedLandscapeActor* Landscape = GetLandscape();
    
    // Enable biome visualization
    Landscape->bDebugVisualizeBiomes = true;
    
    // Enable stats
    Landscape->bDebugShowStats = true;
    
    // Regenerate with debug colors
    Landscape->GenerateLandscape();
}
```

### Example 12: Performance Profiling

**Measure generation performance**:
```cpp
void APerformanceTester::ProfileGeneration()
{
    AGridBasedLandscapeActor* Landscape = GetLandscape();
    
    double StartTime = FPlatformTime::Seconds();
    
    Landscape->GenerateLandscape();
    
    double ElapsedTime = FPlatformTime::Seconds() - StartTime;
    
    UE_LOG(LogTemp, Warning, TEXT("Generation took %.2f seconds"), ElapsedTime);
    
    int32 CellsLoaded, CellsMax;
    Landscape->GridManager.GetCacheStats(CellsLoaded, CellsMax);
    
    UE_LOG(LogTemp, Warning, TEXT("Cells loaded: %d/%d"), CellsLoaded, CellsMax);
}
```

### Example 13: Noise Parameter Testing

**Test different noise configurations**:
```cpp
void ANoiseParameterTester::TestConfigurations()
{
    AGridBasedLandscapeActor* Landscape = GetLandscape();
    
    // Configuration array
    struct FNoiseConfig {
        float PrimaryFreq;
        int32 PrimaryOctaves;
        FString Name;
    };
    
    TArray<FNoiseConfig> Configs = {
        {0.001f, 3, TEXT("Realistic")},
        {0.001f, 4, TEXT("Detailed")},
        {0.0005f, 2, TEXT("Islands")},
        {0.002f, 2, TEXT("Bumpy")}
    };
    
    for (const auto& Config : Configs)
    {
        Landscape->PrimaryNoiseFrequency = Config.PrimaryFreq;
        Landscape->PrimaryNoiseOctaves = Config.PrimaryOctaves;
        
        Landscape->ClearLandscape();
        Landscape->GenerateLandscape();
        
        UE_LOG(LogTemp, Warning, TEXT("Generated: %s"), *Config.Name);
    }
}
```

---

## Data Query Patterns

### Example 14: Biome Statistics

**Analyze terrain composition**:
```cpp
void ATerrainAnalyzer::AnalyzeBiomeDistribution()
{
    AGridBasedLandscapeActor* Landscape = GetLandscape();
    
    TMap<EBiomeType, int32> BiomeCount;
    
    // Sample the landscape
    for (float Y = -5000; Y < 5000; Y += 100)
    {
        for (float X = -5000; X < 5000; X += 100)
        {
            float Height = Landscape->NoiseGenerator.GetHeightAtWorldPosition(X, Y);
            EBiomeType Biome = Landscape->BiomeClassifier.GetBiomeAtHeight(Height);
            
            BiomeCount.FindOrAdd(Biome)++;
        }
    }
    
    // Log results
    for (const auto& Pair : BiomeCount)
    {
        FString BiomeName = TEXT("Unknown");
        switch (Pair.Key)
        {
            case EBiomeType::Ocean:
                BiomeName = TEXT("Ocean");
                break;
            case EBiomeType::Plains:
                BiomeName = TEXT("Plains");
                break;
            // ... etc
        }
        
        UE_LOG(LogTemp, Warning, TEXT("%s: %d cells"), *BiomeName, Pair.Value);
    }
}
```

### Example 15: Height Range Analysis

**Find terrain extremes**:
```cpp
void ATerrainAnalyzer::FindHeightExtremes()
{
    AGridBasedLandscapeActor* Landscape = GetLandscape();
    
    float MinHeight = FLT_MAX;
    float MaxHeight = -FLT_MAX;
    FVector MinPos, MaxPos;
    
    // Sample the landscape
    for (float Y = -5000; Y < 5000; Y += 500)
    {
        for (float X = -5000; X < 5000; X += 500)
        {
            float Height = Landscape->NoiseGenerator.GetHeightAtWorldPosition(X, Y);
            
            if (Height < MinHeight)
            {
                MinHeight = Height;
                MinPos = FVector(X, Y, Height);
            }
            
            if (Height > MaxHeight)
            {
                MaxHeight = Height;
                MaxPos = FVector(X, Y, Height);
            }
        }
    }
    
    UE_LOG(LogTemp, Warning, TEXT("Min height: %.0f at (%.0f, %.0f)"),
        MinHeight, MinPos.X, MinPos.Y);
    UE_LOG(LogTemp, Warning, TEXT("Max height: %.0f at (%.0f, %.0f)"),
        MaxHeight, MaxPos.X, MaxPos.Y);
    UE_LOG(LogTemp, Warning, TEXT("Range: %.0f units"), MaxHeight - MinHeight);
}
```

---

## Common Integration Points

### When to Use Which Method

| Need | Method | Example |
|------|--------|---------|
| Setup landscape | `GenerateLandscape()` | Level initialization |
| Remove landscape | `ClearLandscape()` | Level cleanup |
| Query height | `NoiseGenerator.GetHeightAtWorldPosition()` | AI pathfinding |
| Query biome | `BiomeClassifier.GetBiomeAtHeight()` | Environmental effects |
| Update streaming | `UpdateStreamingAroundPoint()` | Every frame (auto) |
| Debug | `bDebugVisualizeBiomes`, `bDebugShowStats` | Development |

---

## Performance Considerations

### Querying Height (High Frequency)
```cpp
// GOOD: Cache result
float CachedHeight = Landscape->NoiseGenerator.GetHeightAtWorldPosition(X, Y);
for (int i = 0; i < 100; i++)
{
    UseHeight(CachedHeight);  // Use cached value
}

// BAD: Call every iteration
for (int i = 0; i < 100; i++)
{
    float Height = Landscape->NoiseGenerator.GetHeightAtWorldPosition(X, Y);  // Expensive!
    UseHeight(Height);
}
```

### Biome Queries
```cpp
// GOOD: Query once
EBiomeType Biome = Landscape->BiomeClassifier.GetBiomeAtHeight(Height);
if (Biome == EBiomeType::Plains || Biome == EBiomeType::Cliffs) { }

// LESS EFFICIENT: Multiple queries
if (Landscape->BiomeClassifier.GetBiomeAtHeight(Height) == EBiomeType::Plains) { }
if (Landscape->BiomeClassifier.GetBiomeAtHeight(Height) == EBiomeType::Cliffs) { }
```

---

This guide provides practical examples for integrating the procedural landscape system with your gameplay systems!
