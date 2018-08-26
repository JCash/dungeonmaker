#pragma once

#include <stdint.h>
#include "jc_voronoi.h"

struct Point2
{
    float x, y;
};

struct SVoronoiParameters
{
    int seed;
    int num_cells;
    int generation_type; // 0 random, 1 square, 2 hexagonal
    int border;
    int num_relaxations;

    int hexagon_density;

    SVoronoiParameters();
};

struct SNoiseParameters
{
    int     seed;

    int     fbm_octaves;
    float   fbm_frequency;
    float   fbm_lacunarity;
    float   fbm_amplitude;
    float   fbm_gain;

    int     noise_modify_type;
    int     noise_contrast_type;

    float   contrast_exponent;

    bool    use_erosion;
    int     erode_type;         //
    int     erode_iterations;
    float   erode_rain_amount;  // how much rain falls per iteration
    float   erode_solubility;   // how much soil is eroded by one unit of water
    float   erode_evaporation;  // what percentage of the water evaporates each iteration
    float   erode_capacity;     // how much sediment one unit of water can hold
    float   erode_thermal_talus;

    bool    apply_radial;
    float   radial_falloff;
    SNoiseParameters();
};

struct SMapParameters
{
    int     width;
    int     height;
    int     seed;
    int     sea_level;

    uint8_t  colors[8*3];
    uint8_t  limits[8];

    bool    use_shading;
    float   light_dir[3];
    int     shadow_step_length;
    float   shadow_strength;
    float   shadow_strength_sea;

    SMapParameters();
};

uint32_t Hash(void* p, uint32_t size);

void UpdateParams(const SVoronoiParameters* voronoi, const SNoiseParameters* noise, const SMapParameters* map);

// VORONOI GENERATION

void GenerateVoronoi();

// NOISE GENERATION

void GenerateNoise(float* noisef, int modify_type);
void ContrastNoise(float* noisef, float contrast_exponent);
void Erode(int w, int h, float* elevation, float* sediment, float* water);
void NoiseRadial(int w, int h, float falloff, float* noisef);
void Blur(int w, int h, float* noisef, float threshold);
void Normalize(int w, int h, float* elevation);

// MAP GENERATION

struct SCell
{
    const jcv_site* site; // Neighborhood data
    uint32_t        elevation : 8;
    uint32_t        is_border : 1;  // The edge around the map
    uint32_t        is_land : 1;    // if not land, water
    uint32_t        is_shallow : 1; // If not shallow, deep water
};

struct SMap
{
    jcv_point*      points;     // The centers of each cell
    jcv_diagram*    voronoi;    // The voronoi diragram between the cells
    SCell*          cells;      // Each cell has the same index as the center point
    int             num_cells;

    SMap();
};

void GenerateMap(uint8_t* noisef);

void ColorizeMap(uint8_t* heights, uint8_t* out_colors, int num_limits, uint8_t* height_limits, uint8_t* height_colors);

SMap* GetMap();
