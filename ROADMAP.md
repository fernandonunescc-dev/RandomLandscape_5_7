# Procedural Landscape - Implementation Roadmap

## Phase 1: Foundation ✓ (COMPLETED)

### Core Systems Implemented
- [x] **FLandscapeNoiseGenerator**: Multi-layer noise generation
  - Placeholder noise function (uses sine-based pseudo-noise)
  - Ready for FastNoise2 integration
  - Supports 3 noise layers with configurable frequencies/octaves

- [x] **FBiomeClassifier**: Height-based terrain classification
  - 6 biome types (Ocean, Lake, Plains, Cliffs, Mountains, Peaks)
  - Customizable height ranges
  - Per-vertex biome classification

- [x] **FLandscapeGridManager**: Efficient cell caching
  - LRU cache with configurable size (25 cells default)
  - Grid cell data structure with height/biome maps
  - Streaming support for memory management

- [x] **AGridBasedLandscapeActor**: Main actor orchestrating system
  - Blueprint-exposed configuration properties
  - Automatic landscape generation from properties
  - Per-cell mesh creation with ProceduralMeshComponent

### Current Status
**Ready for Testing**: Basic landscape generation works. Terrain is generated with height variation and biome classification. Memory management with streaming is functional.

---

## Phase 2: Quality Enhancement (NEXT)

### FastNoise2 Integration
**Priority**: HIGH
**Effort**: Medium (2-4 hours)
**Impact**: Significantly better noise quality

1. **Integrate FastNoise2 Library**
   - Already in project at: `/ThirdParty/FastNoise2/`
   - Update `RandomLandscape_5_7.Build.cs` to include FastNoise2
   - Link against FastNoise2 library

2. **Replace Noise Implementation**
   - Modify `FLandscapeNoiseGenerator::GetNoise()`
   - Use `FastNoise::CreateNoise()` for Perlin/Simplex
   - Support noise seamless tiling across cells

3. **Testing**
   - Compare quality with placeholder noise
   - Benchmark performance impact
   - Tune noise settings for optimal results

### Material & Biome System
**Priority**: HIGH
**Effort**: Medium (2-3 hours)
**Impact**: Visual variety and polish

1. **Create Biome Materials**
   - Base material with normal maps
   - Per-biome material instances (Ocean, Lake, Plains, Cliffs, Mountains, Peaks)
   - Implement slope-based material blending

2. **Material Assignment**
   - Map biome type to material in actor
   - Handle material transitions at biome boundaries
   - Support dynamic material parameters

3. **Visual Polish**
   - Add detail normal maps
   - Implement triplanar projection
   - Height-based color variation

### Water System Implementation
**Priority**: MEDIUM
**Effort**: High (4-6 hours)
**Impact**: Critical for ocean/lake distinction

1. **Water Plane Generation**
   - Identify continuous water regions
   - Generate water plane meshes
   - Position at correct water level

2. **Water Material**
   - Create water shader with wave animation
   - Reflection/refraction (screen-space or planar)
   - Foam at shorelines

3. **Water Interaction**
   - Basic water volumes for level design
   - Optional: buoyancy for actors

---

## Phase 3: Performance Optimization

### LOD (Level of Detail) System
**Priority**: HIGH
**Effort**: High (6-8 hours)
**Impact**: Enables larger maps with better FPS

1. **Multi-LOD Generation**
   - LOD0: Full 32×32 vertex grid per cell
   - LOD1: 16×16 simplified (50% vertices)
   - LOD2: 8×8 simplified (25% vertices)
   - LOD3: Merged cells (2×2 cells = single mesh)

2. **Distance-Based LOD Selection**
   - Automatic LOD based on distance to camera
   - Smooth transitions between LODs
   - Frustum culling

3. **Mesh Instancing**
   - Instance similar cells (same biome, similar height)
   - Reduce draw calls significantly

### Asynchronous Generation
**Priority**: MEDIUM
**Effort**: High (6-8 hours)
**Impact**: Smooth gameplay during generation

1. **Background Thread Processing**
   - Use FAsyncTask for noise generation
   - Queue cell generation requests
   - Stream results back to main thread

2. **Progressive Streaming**
   - Generate nearby cells first
   - Show placeholder geometry while generating
   - Smooth pop-in using LOD blending

### Memory Optimization
**Priority**: MEDIUM
**Effort**: Medium (3-4 hours)
**Impact**: Support larger maps with less RAM

1. **Height Map Compression**
   - Store heightmaps at 16-bit instead of 32-bit floats
   - Compress inactive cells to disk
   - Streaming from disk cache

2. **Mesh Pooling**
   - Reuse ProceduralMeshComponent instances
   - Reduce memory fragmentation

---

## Phase 4: Advanced Features

### Terrain Shaping Tools
**Priority**: LOW (Nice to have)
**Effort**: High (8-10 hours)
**Impact**: Manual terrain adjustment for level design

1. **Brush System**
   - Raise/lower terrain with painting
   - Smooth/flatten tools
   - Real-time mesh updates

2. **Sculpting Tools**
   - Carve canyons
   - Create valleys
   - Place cliffs/overhangs

### Vegetation System
**Priority**: LOW (Nice to have)
**Effort**: High (8-12 hours)
**Impact**: Biome-specific visual richness

1. **Vegetation Placement**
   - Use biome type to determine vegetation
   - Use slope for placement rules
   - Density based on terrain

2. **Vegetation Types**
   - Trees (mountains, cliffs)
   - Grass (plains)
   - Water plants (lakes)
   - Snow coverage (peaks)

### Erosion Simulation
**Priority**: LOW (Nice to have)
**Effort**: Very High (12-16 hours)
**Impact**: More realistic terrain

1. **Thermal Erosion**
   - Smooth steep slopes
   - Create natural cliff edges

2. **Hydraulic Erosion**
   - Water flow simulation
   - Valley/canyon creation
   - Realistic sediment distribution

### Runtime Terrain Modification
**Priority**: LOW (Future)
**Effort**: High (10-12 hours)
**Impact**: Dynamic terrain changes

1. **Deformation**
   - Explosion craters
   - Building placement deformation
   - Terrain damage

2. **Streaming Updates**
   - Modify heightmaps
   - Update meshes dynamically
   - Cascade to dependent cells

---

## Development Checklist

### Phase 1 Tests
- [ ] Generate landscape without errors
- [ ] Verify height variation (check min/max)
- [ ] Check biome classification (visualize with debug mode)
- [ ] Test streaming (move camera, monitor memory)
- [ ] Verify different noise configurations produce different results
- [ ] Check cell boundaries are seamless

### Phase 2 Tests
- [ ] FastNoise2 integration compiles
- [ ] Noise quality improvement visible
- [ ] Materials apply correctly per biome
- [ ] Water planes appear at correct height
- [ ] Smooth transitions between biomes

### Phase 3 Tests
- [ ] LOD switches work correctly
- [ ] No visible "popping" of geometry
- [ ] Async generation doesn't stutter
- [ ] FPS improvement with larger maps
- [ ] Memory usage stays constant

### Phase 4 Tests
- [ ] Brush tools respond smoothly
- [ ] Vegetation placement looks natural
- [ ] Erosion creates realistic features
- [ ] Runtime modifications update correctly

---

## Code Structure Planning

### Current Files
```
Source/RandomLandscape_5_7/
├── GridBasedLandscapeActor.h/cpp          (Main orchestrator)
├── LandscapeNoiseGenerator.h/cpp          (Noise generation)
├── BiomeClassifier.h/cpp                  (Terrain classification)
└── LandscapeGridManager.h/cpp             (Cell management)
```

### To Be Added (Phase 2+)
```
├── LandscapeWaterManager.h/cpp            (Water system)
├── BiomeMaterialManager.h/cpp             (Material handling)
├── LandscapeLODManager.h/cpp              (LOD system)
├── LandscapeAsyncGenerator.h/cpp          (Async generation)
├── LandscapeSculptureTools.h/cpp          (Terrain editing)
├── LandscapeVegetationSystem.h/cpp        (Vegetation)
└── LandscapeErosionSimulator.h/cpp        (Erosion)
```

---

## Performance Targets

### Phase 1 (Current)
- FPS: 30-60 on GTX 1080
- Memory: 150-250 MB (25 cells loaded)
- Generation time: < 5 seconds for 100×100m map

### Phase 2 (After Materials)
- FPS: 30-60 on GTX 1080
- Memory: 200-300 MB
- Generation time: < 5 seconds

### Phase 3 (After LOD)
- FPS: 60+ on GTX 1080
- Memory: 150-250 MB (fewer vertices)
- Support: 500×500m maps at 60 FPS

### Phase 4 (Full Featured)
- FPS: 60+ with all features
- Memory: Managed with streaming
- Support: Unlimited map size

---

## Priority Recommendations

**For Quick Demo**:
1. Finish Phase 1 (now)
2. Add basic materials (Phase 2)
3. Simple water planes

**For Production Quality**:
1. Complete Phase 1-3
2. FastNoise2 integration
3. LOD system
4. Proper water system

**For Open-World Game**:
1. All of above
2. Async generation
3. Vegetation system
4. Runtime modification

---

## Next Immediate Steps

1. **Test Current Implementation**
   - Create a test level
   - Spawn GridBasedLandscapeActor
   - Verify landscape generates
   - Check streaming works

2. **FastNoise2 Integration**
   - Set up build dependencies
   - Implement proper noise function
   - Compare quality

3. **Material System**
   - Create biome materials
   - Implement material assignment
   - Test biome transitions

4. **Water System**
   - Implement water plane generation
   - Create water material
   - Test ocean/lake placement
