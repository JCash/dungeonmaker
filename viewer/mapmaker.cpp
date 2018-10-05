// Reading materials
// http://www-cs-students.stanford.edu/~amitp/game-programming/polygon-map-generation/


#include <stdint.h>
#include <stdio.h> // printf

#define JC_NOISE_IMPLEMENTATION
#include "jc_noise.h"

// #define JC_MAPMAKER_NOISE_IMPLEMENTATION
// #include "jc_mapmaker_noise.h"

#include "mapmaker.h"

SVoronoiParameters::SVoronoiParameters()
{
    seed = 1337;
    generation_type = 0;
    border = 35;
    num_cells = 1024;        // for random
    num_relaxations = 20;    // for random
    hexagon_density = 26;
}

SNoiseParameters::SNoiseParameters()
{
    fbm_octaves = 5;
    fbm_frequency = 1.0f;
    fbm_lacunarity = 2.0f;
    fbm_amplitude = 1.0f;
    fbm_gain = 0.5f;
    noise_modify_type = 0;

    perturb_type    = 0; // 0 == none
    perturb1_a1     = 5.0f;
    perturb1_a2     = 1.0f;
    perturb1_scale  = 8.0f;

    perturb2_scale = 512.0f;
    perturb2_qyx = 5.2f;
    perturb2_qyy = 1.3f;
    perturb2_rxx = 1.7f;
    perturb2_rxy = 9.2f;
    perturb2_ryx = 8.3f;
    perturb2_ryy = 2.8f;

    noise_contrast_type = 0;
    contrast_exponent = 1.0f;

    use_erosion = false;
    erode_type = 0;
    erode_iterations = 0;

    // hydraulic
    erode_rain_amount = 0.15f;
    erode_solubility = 0.5f;
    erode_evaporation = 0.015f;
    erode_capacity = 0.05f;

    // thermal
    erode_thermal_talus = 8;

    apply_radial = true;
    radial_falloff = 0.20f;
};

SMapParameters::SMapParameters()
{
    width       = 512;
    height      = 512;
    seed        = 1337;
    sea_level   = 110;

    use_shading = true;
    light_dir[0]= 2;
    light_dir[1]= -1;
    light_dir[2]= 2;

    shadow_step_length = 8;
    shadow_strength = 0.15f;
    shadow_strength_sea = 0.03f;
};

SMap::SMap()
: points(0)
, voronoi(0)
, cells(0)
, num_cells(0)
{
}

uint32_t Hash(void* key, uint32_t size) // FNV Hash
{
    uint8_t* p = (uint8_t*)key;
    uint32_t h = 2166136261;
    for (uint32_t i = 0; i < size; i++)
        h = (h*16777619) ^ p[i];
    return h;
}

const float PI = 3.14159265358979323846f;
const float PI_HALF = 1.57079632679489661923f;

static SVoronoiParameters   g_VoronoiParams;
static SNoiseParameters     g_NoiseParams;
static SMapParameters       g_MapParams;
static jcn_context*         g_NoiseCtx = 0;

static jcv_diagram* g_VoronoiDiagram = 0;
static jcv_point*   g_VoronoiPoints = 0;
static int          g_VoronoiNumPoints = 0;

static uint32_t g_RandVoronoi = 0;

static SMap g_Map;

static inline float Clampf(float a, float b, float v)
{
    return v < a ? a : (v > b ? b : v);
}

static inline int Clampi(int a, int b, int v)
{
    return v < a ? a : (v > b ? b : v);
}


void UpdateParams(const SVoronoiParameters* voronoi, const SNoiseParameters* noise, const SMapParameters* map)
{
    int update_noise = noise->seed != g_NoiseParams.seed;

    g_VoronoiParams = *voronoi;
    g_NoiseParams = *noise;
    g_MapParams = *map;

    if (update_noise || g_NoiseCtx == 0) {
        if (g_NoiseCtx)
            free(g_NoiseCtx);
        g_NoiseCtx = jcn_create(JCN_TYPE_PERLIN, g_NoiseParams.seed);
    }
}


static jcn_real fbm(jcn_real x, jcn_real y)
{
    float frequency = 1.0f / (g_MapParams.width * g_NoiseParams.fbm_frequency);
    //float lacunarity = 2.0f;
    float lacunarity = g_NoiseParams.fbm_lacunarity;
    //float lacunarity = 1.85f;
    float amplitude = g_NoiseParams.fbm_amplitude;
    float gain = g_NoiseParams.fbm_gain;
    int octaves = g_NoiseParams.fbm_octaves;

    //return jcn_fbm_2d(g_NoiseCtx, octaves, amplitude, frequency, lacunarity, gain, x, y);

    jcn_real sum = 0.0f;
    jcn_real sum_amp = 0.0f;
    for(int i = 0; i < octaves; ++i)
    {
        sum += jcn_noise_2d(g_NoiseCtx, x*frequency, y*frequency) * amplitude;
        frequency *= lacunarity;
        sum_amp += amplitude;
        amplitude *= gain;
    }

    // jcn_real v = sum / sum_amp;
    // static jcn_real vmax = 0.0f;
    // static jcn_real vmin = 0.0f;
    // if (v < vmin)
    //     vmin = v;
    // if (v > vmax)
    //     vmax = v;
    // printf("sum: %f  sumamp %f  min/max: %f  %f\n", v, sum_amp, vmin, vmax);


    return sum / sum_amp;
    //return sum;
}

static float ModifyValue(float n, int modify_type)
{
    switch(modify_type)
    {
    case 2: return 1.0f - fabsf(n); // ridged
    case 1: return fabsf(n); // billow
    case 0:
    default: break;
    }
    return n;
}

// Noise generation ideas:
// Voronoi: http://web.mit.edu/cesium/Public/terrain.pdf

void GenerateNoise(float* noisef, int modify_type)
{
    for( int y = 0; y < g_MapParams.height; ++y )
    {
        for( int x = 0; x < g_MapParams.width; ++x )
        {
            jcn_real n = fbm(x, y);
            n = jcn_remap(n, -1.0f, 1.0f, 0.0f, 1.0f);

            n = ModifyValue(n, modify_type);

            noisef[y * g_MapParams.width + x] = n;
        }
    }
}

void Perturb1(int w, int h, float* noisef)
{
    int modify_type = g_NoiseParams.noise_modify_type;
    for( int y = 0; y < w; ++y )
    {
        for( int x = 0; x < h; ++x )
        {
            jcn_real qx = fbm( x, y );
            jcn_real qy = fbm( x + g_NoiseParams.perturb1_a1, y + g_NoiseParams.perturb1_a2 );
            float n = fbm(x + g_NoiseParams.perturb1_scale * qx, y + g_NoiseParams.perturb1_scale * qy );
            noisef[y*w + x] = ModifyValue(n, modify_type);
        }
    }
}


void Perturb2(int w, int h, float* noisef)
{
    int modify_type = g_NoiseParams.noise_modify_type;

    float scale = g_NoiseParams.perturb2_scale;
    float qyx = g_NoiseParams.perturb2_qyx;
    float qyy = g_NoiseParams.perturb2_qyy;
    float rxx = g_NoiseParams.perturb2_rxx;
    float rxy = g_NoiseParams.perturb2_rxy;
    float ryx = g_NoiseParams.perturb2_ryx;
    float ryy = g_NoiseParams.perturb2_ryy;
    for( int y = 0; y < w; ++y )
    {
        for( int x = 0; x < h; ++x )
        {
            float qx = fbm( x, y );
            float qy = fbm( x + qyx * scale, y + qyy * scale );

            float rx = fbm( x + 4.0f * scale * qx + rxx * scale, y + 4.0f * scale * qy + rxy * scale );
            float ry = fbm( x + 4.0f * scale * qx + ryx * scale, y + 4.0f * scale * qy + ryy * scale );

    // jcn_real qx = fbm( x, y );
    // jcn_real qy = fbm( x + 5.2f * scale, y + 1.3f * scale );

    // jcn_real rx = fbm( x + 4.0f * scale * qx + 1.7f * scale, y + 4.0f * scale * qy + 9.2f * scale );
    // jcn_real ry = fbm( x + 4.0f * scale * qx + 8.3f * scale, y + 4.0f * scale * qy + 2.8f * scale );

    // return fbm(x + 4.0f * scale * rx, y + 4.0f * scale * ry );
            float n = fbm(x + 4.0f * scale * rx, y + 4.0f * scale * ry );
            noisef[y*w + x] = ModifyValue(n, modify_type);
        }
    }
}


// jcn_real pattern2(jcn_real x, jcn_real y)
// {
//     jcn_real qx = fbm( x, y );
//     jcn_real qy = fbm( x + 5.2f * 512, y + 1.3f * 512 );

//     jcn_real rx = fbm( x + 4.0f * 512.0f * qx + 1.7f * 512, y + 4.0f * 512.0f * qy + 9.2f * 512 );
//     jcn_real ry = fbm( x + 4.0f * 512.0f * qx + 8.3f * 512, y + 4.0f * 512.0f * qy + 2.8f * 512 );

//     return fbm(x + 4.0f * 512.0f * rx, y + 4.0f * 512.0f * ry );
// }

void ContrastNoise(float* noisef, float exponent)
{
    int size = g_MapParams.width * g_MapParams.height;
    for( int i = 0; i < size; ++i)
    {
        noisef[i] = pow(noisef[i], exponent);
    }
}

static inline float Min(float a, float b)
{
    return a < b ? a : b;
}

// http://micsymposium.org/mics_2011_proceedings/mics2011_submission_30.pdf
static void ErodeHydraulic(int w, int h, float* elevation, float* sediment, float* water)
{
    int size = w * h;
    for (int it = 0; it < g_NoiseParams.erode_iterations; ++it)
    {
        for( int i = 0; i < size; ++i)
        {
            // Add water
            water[i] += g_NoiseParams.erode_rain_amount;

            // Erode
            float erode = g_NoiseParams.erode_solubility * water[i];

            if (elevation[i] < erode)
                erode = elevation[i];
            elevation[i] -= erode;
            sediment[i] += erode;

        }

        for( int y = 0; y < h; ++y)
        {
            for( int x = 0; x < w; ++x)
            {
                int i = y * w + x;

                // // Add water
                // water[i] = g_NoiseParams.erode_rain_amount;

                // // Erode
                // float erode = g_NoiseParams.erode_solubility * water[i];

                // if (elevation[i] < erode)
                //     erode = elevation[i];
                // elevation[i] -= erode;
                // sediment[i] += erode;

                // Move water
                if (water[i] > 0.0f)
                {
                    float height = elevation[i] + water[i];

                    int neighbor = i;
                    float neighbor_height = height;
                    int offsets[] = { -w-1, -w -w+1, -1, 1, w-1, w, w+1 };
                    for( int j = 0; j < 8; ++j)
                    {
                        int ii = i + offsets[j];
                        if (ii < 0 || ii >= w*h)
                            continue;

                        if (elevation[ii] + water[ii] < neighbor_height)
                        {
                            neighbor = ii;
                            neighbor_height = elevation[ii] + water[ii];
                        }
                    }

                    if (neighbor != i && neighbor_height < height)
                    {
                        float difference = height - neighbor_height;
                        if (difference > 0)
                        {
                            //float delta_water = Min(water[i], difference) * difference / difference;
                            float delta_water = Min(water[i], difference) * 1.0f / ( 1 + 1 ); // * 1.0f
                            water[i] -= delta_water;
                            water[neighbor] += delta_water;

                            float delta_sediment = sediment[i] * delta_water / water[i];
                            sediment[i] -= delta_sediment;
                            sediment[neighbor] += delta_sediment;

                            if (water[i] < 0)
                                water[i] = 0;
                            if (sediment[i] < 0)
                                sediment[i] = 0;
                        }
                    }
                }

                // Evaporate
                water[i] *= (1.0f - g_NoiseParams.erode_evaporation);

                // Move sediment back
                {
                    // If the water cannot hold this much soil, put it back into the base image
                    float m_max = g_NoiseParams.erode_capacity * water[i];
                    float amount = sediment[i];
                    if (amount < 0)
                        amount = 0;
                    sediment[i] -= amount;
                    elevation[i] += amount;
                }
            }
        }
    }
}

static void ErodeThermal(int w, int h, float* elevation)
{
    float talus = g_NoiseParams.erode_thermal_talus / w;

    for (int it = 0; it < g_NoiseParams.erode_iterations; ++it)
    {
        for( int y = 0; y < h; ++y)
        {
            for( int x = 0; x < w; ++x)
            {
                int i = y * w + x;

                // int neighbor = i;
                // float neighbor_height = elevation[i];
                // int offsets[] = { -w, -1, 1, w }; // von Neumann
                // for( int j = 0; j < 4; ++j)
                // {
                //     int ii = i + offsets[j];
                //     if (ii < 0 || ii >= w*h)
                //         continue;

                //     if (elevation[ii] < neighbor_height)
                //     {
                //         neighbor = ii;
                //         neighbor_height = elevation[ii];
                //     }
                // }

                // if( neighbor != i )
                // {
                //     float diff = elevation[i] - elevation[neighbor];
                //     if (diff > talus)
                //     {
                //         float delta = (diff - talus) / 2.0f;
                //         elevation[i] -= delta;
                //         elevation[neighbor] += delta;
                //     }
                // }


                // Reference implementation
                float dmax = 0.0f;
                float dtotal = 0.0f;
                float neighbor_height = elevation[i];
                int neighbor = i;
                //int offsets[] = { -w, -1, 1, w }; // von Neumann
                int offsets[] = { -w-1, -w+1, w-1, w+1 }; // von Neumann
                for( int j = 0; j < 4; ++j)
                {
                    int ii = i + offsets[j];
                    if (ii < 0 || ii >= w*h)
                        continue;
                    float di = elevation[i] - elevation[ii];
                    if (di > talus)
                    {
                        dtotal += di;
                        if (di > dmax)
                        {
                            dmax = di;
                            neighbor = ii;
                        }
                    }
                }

                float factor = 0.5f * (dmax - talus);
                for( int j = 0; j < 4; ++j)
                {
                    int ii = i + offsets[j];
                    if (ii < 0 || ii >= w*h)
                        continue;
                    float di = elevation[i] - elevation[ii];
                    if (di > talus)
                    {
                        float amount = factor * (di / dtotal);
                        elevation[ii] += amount;
                        elevation[i] -= amount;
                    }
                }
            }
        }
    }
}

void Erode(int w, int h, float* elevation, float* sediment, float* water)
{
    switch(g_NoiseParams.erode_type)
    {
    case 0: ErodeThermal(w, h, elevation); break;
    case 1: ErodeHydraulic(w, h, elevation, sediment, water); break;
    default: break;
    }
}

void NoiseRadial(int width, int height, float falloff, float* noisef)
{
    float halfwidth = width * 0.5f;
    float halfheight = height * 0.5f;
    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            float xx = x - halfwidth;
            float yy = y - halfheight;
            float dx = xx / halfwidth;
            float dy = yy / halfheight;
            // 1.0 in the middle, 0 at the edge
            float d = 1.0f - sqrtf(dx*dx + dy*dy);
            if (d < 0)
                d = 0;

            int i = y * width + x;
            //noisef[i] *= d;
            noisef[i] *= d / pow(d, falloff);
            //noisef[i] = d * pow(d, falloff);
            //float f = pow(d, falloff)
        }
    }
}

void Blur(int w, int h, float* noisef, float threshold)
{
    size_t size = w * h;
    float* tmp = (float*)malloc(size * sizeof(float));

    int kernelsize = 3;
    float kernel[] = {  1, 2, 1,
                        2, 4, 2,
                        1, 2, 1 };
    /*
    int kernelsize = 5;
    float kernel[] = {  1,2, 4,2,1,
                        2,4, 8,4,2,
                        4,8,16,8,4,
                        2,4, 8,4,2,
                        1,2, 4,2,1  };
                        */

    int halfkernelsize = kernelsize/2;
    // Normalize kernel
    float sum = 0;
    for( int i = 0; i < kernelsize*kernelsize; ++i )
        sum += kernel[i];
    const float oneoversum = 1.0f/sum;
    for( int i = 0; i < kernelsize*kernelsize; ++i )
        kernel[i] *= oneoversum;

    for( int y = 0; y < h; ++y )
    {
        for( int x = 0; x < w; ++x )
        {
            int index = y * w + x;
            if (noisef[index] > threshold)
            {
                tmp[index] = noisef[index];
                continue;
            }

            float sum = 0;
            for( int ky = -halfkernelsize; ky <= halfkernelsize; ++ky )
            {
                for( int kx = -halfkernelsize; kx <= halfkernelsize; ++kx )
                {
                    int xx = Clampi(0, w-1, x + kx);
                    int yy = Clampi(0, h-1, y + ky);
                    sum += kernel[(ky+halfkernelsize) * kernelsize + (kx+halfkernelsize)] * noisef[yy * w + xx];
                }
            }
            tmp[index] = sum;
        }
    }

    memcpy(noisef, tmp, size * sizeof(float));
    free(tmp);
}

void Normalize(int w, int h, float* elevation)
{
    int size = w * h;
    float max = 0.0f;
    float min = 1.0f;
    for (int i = 0; i < size; ++i)
    {
        if (elevation[i] > max)
            max = elevation[i];

        if (elevation[i] < min)
            min = elevation[i];
    }

    float f = 1.0f / max;
    for (int i = 0; i < size; ++i)
    {
        elevation[i] = jcn_remap(elevation[i], min, max, 0.0f, 1.0f);
    }
}


// VORONOI

static void relax_points(const jcv_diagram* diagram, jcv_point* points)
{
    const jcv_site* sites = jcv_diagram_get_sites(diagram);
    for( int i = 0; i < diagram->numsites; ++i )
    {
        const jcv_site* site = &sites[i];
        jcv_point sum = site->p;
        int count = 1;
        const jcv_graphedge* edge = site->edges;
        while( edge )
        {
            sum.x += edge->pos[0].x;
            sum.y += edge->pos[0].y;
            ++count;
            edge = edge->next;
        }
        points[site->index].x = sum.x / count;
        points[site->index].y = sum.y / count;
    }
}

static void GenerateVoronoiCellPoints()
{
    if (g_VoronoiPoints)
        free(g_VoronoiPoints);

    int border = 0;//g_VoronoiParams.border;
    int width = g_MapParams.width;
    int height = g_MapParams.height;
    if (g_VoronoiParams.generation_type == 0) // random
    {
        g_VoronoiNumPoints = g_VoronoiParams.num_cells;
        g_VoronoiPoints = (jcv_point*)malloc(g_VoronoiParams.num_cells * sizeof(jcv_point));

        int max_x_range = width - 2 * border;
        int max_y_range = height - 2 * border;
        for ( int i = 0; i < g_VoronoiParams.num_cells; ++i )
        {
            g_VoronoiPoints[i].x = (int)(border + rand_r(&g_RandVoronoi) % max_x_range);
            g_VoronoiPoints[i].y = (int)(border + rand_r(&g_RandVoronoi) % max_y_range);
        }
    }
    else if (g_VoronoiParams.generation_type == 1) // hexagonal
    {
        int size = (width < height ? width : height) / (g_VoronoiParams.hexagon_density * 2);
        float w = size * sqrtf(3.0f);
        float h = size * 2.0f;

        int xsteps = width / w + 1;
        int ysteps = height / (0.75f * h) + 1;

// printf("size: %d\n", size);
// printf("w, h: %f %f\n", w, h);
// printf("xs, ys: %d %d\n", xsteps, ysteps);
// printf("num: %d\n", g_VoronoiNumPoints);

        border = 0; // apply border on the cells instead

        // 2 passes: count points + allocate, fill in points
        for (int i = 0; i < 2; ++i)
        {
            int count = 0;
            for (int iy = 0; iy < ysteps; ++iy)
            {
                float y = iy * (h * 0.75f);
                if (y < border || (height - y) < border)
                    continue;
                for (int ix = 0; ix < xsteps; ++ix)
                {
                    float x = w * (iy&1 ? 0.5f : 0.0f) + ix * w;
                    if (x < border || (width - x) < border)
                        continue;

                    if (i)
                    {
                        g_VoronoiPoints[count].x = (int)x;
                        g_VoronoiPoints[count].y = (int)y;
            //printf("%d: x, y:  %f %f\n", count, x, y);
                    }
                    ++count;
                }
            }

            if (i == 0)
            {
                g_VoronoiNumPoints = count;
                g_VoronoiPoints = (jcv_point*)malloc(g_VoronoiNumPoints * sizeof(jcv_point));
            }

            //printf("%d  vs %d\n", g_VoronoiNumPoints, count);
        }

    }

}

void GenerateVoronoi()
{
    g_RandVoronoi = g_VoronoiParams.seed;
    GenerateVoronoiCellPoints();

    if (g_VoronoiDiagram == 0)
    {
        g_VoronoiDiagram = (jcv_diagram*)malloc(sizeof(jcv_diagram));
    }
    else
    {
        jcv_diagram_free(g_VoronoiDiagram);
    }

    jcv_rect rect = {0, 0, g_MapParams.width, g_MapParams.height};

    if (g_VoronoiParams.generation_type == 0) // random
    {
        for (int i = 0; i < g_VoronoiParams.num_relaxations; ++i)
        {
            memset(g_VoronoiDiagram, 0, sizeof(jcv_diagram));
            jcv_diagram_generate(g_VoronoiNumPoints, g_VoronoiPoints, &rect, g_VoronoiDiagram);
            relax_points(g_VoronoiDiagram, g_VoronoiPoints);
            jcv_diagram_free(g_VoronoiDiagram);
        }
    }

    memset(g_VoronoiDiagram, 0, sizeof(jcv_diagram));
    jcv_diagram_generate(g_VoronoiNumPoints, g_VoronoiPoints, &rect, g_VoronoiDiagram);
}

// MAP - COLORS

static inline void LightenColor(uint8_t* color, float percent)
{
    color[0] = (uint8_t)Clampf(0.0f, 255.0f, color[0] + color[0] * percent);
    color[1] = (uint8_t)Clampf(0.0f, 255.0f, color[1] + color[1] * percent);
    color[2] = (uint8_t)Clampf(0.0f, 255.0f, color[2] + color[2] * percent);
}

static inline void DarkenColor(uint8_t* color, float percent)
{
    color[0] = (uint8_t)Clampf(0.0f, 255.0f, color[0] - color[0] * percent);
    color[1] = (uint8_t)Clampf(0.0f, 255.0f, color[1] - color[1] * percent);
    color[2] = (uint8_t)Clampf(0.0f, 255.0f, color[2] - color[2] * percent);
}

static void ColorizeMapInternal(int width, int height, uint8_t* heights, int num_limits, uint8_t* height_limits, uint8_t* height_colors, uint8_t* out_colors)
{
    const float lighten_percent = 0.2f;
    size_t size = width * height;
    for( size_t i = 0; i < size; ++i )
    {
        for( int l = 0; l < num_limits; ++l )
        {
            uint8_t value = heights[i];
            if( value <= height_limits[l] )
            {
                out_colors[i*3+0] = height_colors[l*3+0];
                out_colors[i*3+1] = height_colors[l*3+1];
                out_colors[i*3+2] = height_colors[l*3+2];

                // make the height variations visible in the color
                {
                    const float maxlimit = float(height_limits[l]);
                    const float minlimit = float(l == 0 ? 0 : height_limits[l-1]);
                    const float scale = (float(value) - minlimit) / (maxlimit - minlimit);

                    LightenColor( &out_colors[i*3], lighten_percent * scale );
                }
                break;
            }
        }
    }
}

static void ShadeMap(int width, int height,
                    float light_dir_x, float light_dir_y, float sun_angle,
                    uint8_t sea_level, float strength, float strength_sea_level,
                    uint8_t* heights, uint8_t* out_colors)
{
    // currently unused
    (void)light_dir_x;
    (void)light_dir_y;

    int half_width = width / 2;
    int half_height = height / 2;
    int numsteps = g_MapParams.shadow_step_length;
    for( int y = 1; y < height; ++y )
    {
        for( int x = 1; x < width; ++x )
        {
            int current = heights[y * width + x];

            // Current dir: top left
            float step_dist = sqrtf(-1*-1 + -1*-1); // direction

            for( int n = 1; n <= numsteps && x >= n && y >= n; ++n)
            {
                int xx = x-n;
                int yy = y-n;
                int topleft = heights[yy * width + xx];

                float height_diff = topleft - current; // opposite side
                if (height_diff > 0)
                {
                    float dist = n * step_dist; // adjacent side
                    float angle = atan2f(height_diff, dist);

                    if (angle >= sun_angle)
                    {
                        // Decrease strength further away
                        float dist_factor = (numsteps - n) / float(numsteps);
                        // Lerp between angles. High difference -> higher strength
                        float angle_factor = Clampf(0.0f, 1.0f, (angle - sun_angle)/(PI_HALF - sun_angle) );
                        float s = strength;
                        if (current < sea_level)
                        {
                            int dx = x - half_width;
                            int dy = y - half_height;
                            float dist_from_center = Clampf(0.0f, 1.0f, 1.0f - sqrtf(dx*dx + dy*dy) / half_width);
                            s = strength_sea_level * dist_from_center;
                        }
                        int index = y * width * 3 + x * 3;
                        DarkenColor( &out_colors[index], Clampf(0.0f, 1.0f, s + s * angle_factor) * dist_factor);
                        break;
                    }
                }
            }
        }
    }
}

static inline float DegToRad(float degrees)
{
    return (degrees * PI) / 180.0f;
}

void ColorizeMap(uint8_t* heights, uint8_t* out_colors, int num_limits, uint8_t* height_limits, uint8_t* height_colors)
{
    ColorizeMapInternal(g_MapParams.width, g_MapParams.height, heights, num_limits, height_limits, height_colors, out_colors);

    if (g_MapParams.use_shading)
    {
        float angle = DegToRad(30.0f);
        ShadeMap(g_MapParams.width, g_MapParams.height,
                    g_MapParams.light_dir[0],
                    g_MapParams.light_dir[1],
                    angle, g_MapParams.sea_level,
                    g_MapParams.shadow_strength, g_MapParams.shadow_strength_sea,
                    heights, out_colors);
    }
}

// static inline uint32_t hash_point(int x, int y)
// {
//     return (x << 16) | y;
// }

static inline bool on_edge(float x, float y, int w, int h)
{
    if( x <= 0 || x >= w-1 )
        return true;
    if( y <= 0 || y >= h-1 )
        return true;
    return false;
}

void GenerateMap(uint8_t* noisef)
{
    g_Map.points = g_VoronoiPoints;
    g_Map.voronoi = g_VoronoiDiagram;

    if (g_Map.cells && g_Map.num_cells != g_VoronoiNumPoints)
    {
        free(g_Map.cells);
        g_Map.cells = 0;
    }

    g_Map.num_cells = g_VoronoiNumPoints;
    if (g_Map.cells == 0)
        g_Map.cells = (SCell*)malloc(sizeof(SCell) * g_Map.num_cells);
    memset(g_Map.cells, 0, sizeof(SCell) * g_Map.num_cells);

    int width = g_MapParams.width;
    int height = g_MapParams.height;
    int sea_level = g_MapParams.sea_level;
    int border = g_VoronoiParams.border;

    // Note that when getting duplicates, the number of sites may be smaller
    // which in turn leaves some cells "empty"
    const jcv_site* sites = jcv_diagram_get_sites( g_Map.voronoi );
    for (int i = 0; i < g_Map.voronoi->numsites; ++i)
    {
        const jcv_site* site = &sites[i];
        SCell& cell = g_Map.cells[site->index];
        cell.site = site;

        int x = (int)site->p.x;
        int y = (int)site->p.y;
        int noiseindex = y * width + x;

        if (x < border || (width - x) < border ||
            y < border || (height - y) < border)
        {
            cell.is_land = 0;
        }
        else
        {
            cell.is_land = 1;
        }

        cell.elevation = noisef[noiseindex];
        if (cell.elevation < sea_level)
        {
            cell.is_land = 0;
            cell.is_shallow = cell.elevation > (sea_level*2)/3;
        }

        // Check if this cell is on the border
        const jcv_graphedge* e = site->edges;
        while( e )
        {
            if (on_edge(e->pos[0].x, e->pos[0].y, width, height))
            {
                cell.is_border = 1;
                cell.is_shallow = 0;
                break;
            }

            e = e->next;
        }
    }
}

SMap* GetMap()
{
    return &g_Map;
}



