// BiomeTerrainGeneratorBase.cpp
// Base implementation with default Perlin fractal noise

#include "BiomeTerrainGeneratorBase.h"

TArray<float> FBiomeTerrainGeneratorBase::GenerateHeightMap(
	const FBiomeMeshSettings& MeshSettings,
	int32 Resolution,
	int32 Seed,
	float MapSizeInMeters,
	bool bTileable) const
{
	// Default implementation: Perlin fractal fBm noise
	auto Generator = CreatePerlinFractalGenerator(
		MeshSettings.NoiseOctaves,
		MeshSettings.NoisePersistence);

	if (bTileable)
	{
		return GenerateTileableNoiseGrid(Generator, Resolution, MeshSettings.NoiseFrequency, Seed, MapSizeInMeters);
	}
	return GenerateNoiseGrid(Generator, Resolution, MeshSettings.NoiseFrequency, Seed, MapSizeInMeters);
}

FastNoise::SmartNode<FastNoise::Generator> FBiomeTerrainGeneratorBase::CreatePerlinFractalGenerator(
	int32 Octaves,
	float Persistence,
	float Lacunarity)
{
	// Perlin -> FractalFBm -> Remap (matches NoiseTool setup)
	auto PerlinGen = FastNoise::New<FastNoise::Perlin>();
	
	auto FractalGen = FastNoise::New<FastNoise::FractalFBm>();
	FractalGen->SetSource(PerlinGen);
	FractalGen->SetOctaveCount(Octaves);
	FractalGen->SetGain(Persistence);
	FractalGen->SetLacunarity(Lacunarity);

	// Remap -1,1 to 0,1 (same as NoiseTool)
	auto RemapGen = FastNoise::New<FastNoise::Remap>();
	RemapGen->SetSource(FractalGen);
	RemapGen->SetFromMin(-1.0f);
	RemapGen->SetFromMax(1.0f);
	RemapGen->SetToMin(0.0f);
	RemapGen->SetToMax(1.0f);

	return RemapGen;
}

TArray<float> FBiomeTerrainGeneratorBase::GenerateNoiseGrid(
	const FastNoise::SmartNode<FastNoise::Generator>& Generator,
	int32 Resolution,
	float Frequency,
	int32 Seed,
	float MapSizeInMeters)
{
	TArray<float> NoiseGrid;
	int32 TotalSamples = Resolution * Resolution;
	NoiseGrid.SetNumUninitialized(TotalSamples);

	// Frequency is used directly as step size (same as FastNoise2 NoiseTool)
	float StepSize = Frequency;

	FastNoise::OutputMinMax MinMax = Generator->GenUniformGrid2D(
		NoiseGrid.GetData(),
		0.0f, 0.0f,
		Resolution, Resolution,
		StepSize, StepSize,
		Seed);

	// Remap node already outputs 0-1, just clamp for safety
	for (int32 i = 0; i < TotalSamples; ++i)
	{
		NoiseGrid[i] = FMath::Clamp(NoiseGrid[i], 0.0f, 1.0f);
	}

	UE_LOG(LogTemp, Log, TEXT("GenerateNoiseGrid: %dx%d, freq=%.4f, range=[%.3f, %.3f]"),
		Resolution, Resolution, Frequency, MinMax.min, MinMax.max);

	return NoiseGrid;
}

TArray<float> FBiomeTerrainGeneratorBase::GenerateTileableNoiseGrid(
	const FastNoise::SmartNode<FastNoise::Generator>& Generator,
	int32 Resolution,
	float Frequency,
	int32 Seed,
	float MapSizeInMeters)
{
	TArray<float> NoiseGrid;
	int32 TotalSamples = Resolution * Resolution;
	NoiseGrid.SetNumUninitialized(TotalSamples);
	
	// Frequency is used directly as step size
	float StepSize = Frequency;

	FastNoise::OutputMinMax MinMax = Generator->GenTileable2D(
		NoiseGrid.GetData(),
		Resolution, Resolution,
		StepSize, StepSize,
		Seed);

	// Remap node already outputs 0-1, just clamp for safety
	for (int32 i = 0; i < TotalSamples; ++i)
	{
		NoiseGrid[i] = FMath::Clamp(NoiseGrid[i], 0.0f, 1.0f);
	}

	UE_LOG(LogTemp, Log, TEXT("GenerateTileableNoiseGrid: %dx%d, freq=%.4f, range=[%.3f, %.3f]"),
		Resolution, Resolution, Frequency, MinMax.min, MinMax.max);

	return NoiseGrid;
}
