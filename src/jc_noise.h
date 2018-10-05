/*
*/

#ifndef JC_NOISE_H
#define JC_NOISE_H


#ifdef __cplusplus
extern "C" {
#endif

#ifndef JCN_REAL_TYPE
    #define JCN_REAL_TYPE float
#endif

#ifndef JCN_RAND
    #include <stdlib.h>
    #define JCN_SRAND srand
    #define JCN_RAND rand
#endif

#ifndef JCN_MEMCPY
    #include <string.h>
    #define JCN_MEMCPY memcpy
#endif


#ifndef JCN_SQRT
    #include <math.h>
    #define JCN_SQRT sqrt
#endif

#define JCN_FUNDEF extern

typedef JCN_REAL_TYPE jcn_real;

enum jcn_type
{
    JCN_TYPE_PERLIN,
    JCN_TYPE_SIMPLEX,
};

typedef struct _jcn_context
{
    enum jcn_type type;
} jcn_context;


JCN_FUNDEF jcn_context* jcn_create(enum jcn_type type, unsigned int seed);
JCN_FUNDEF void         jcn_destroy(jcn_context* ctx);
JCN_FUNDEF jcn_real     jcn_noise_1d(jcn_context* ctx, jcn_real x);
JCN_FUNDEF jcn_real     jcn_noise_2d(jcn_context* ctx, jcn_real x, jcn_real y);
JCN_FUNDEF jcn_real     jcn_noise_3d(jcn_context* ctx, jcn_real x, jcn_real y, jcn_real z);
//JCN_FUNDEF jcn_real     jcn_fbm_1d(jcn_context* ctx, int octaves, jcn_real x);
//JCN_FUNDEF jcn_real     jcn_fbm_2d(jcn_context* ctx, int octaves, jcn_real x, jcn_real y);
JCN_FUNDEF jcn_real     jcn_fbm_2d(jcn_context* ctx, int octaves, jcn_real amplitude, jcn_real frequency, jcn_real lacunarity, jcn_real gain, jcn_real x, jcn_real y);
//JCN_FUNDEF jcn_real     jcn_fbm_3d(jcn_context* ctx, int octaves, jcn_real x, jcn_real y, jcn_real z);

JCN_FUNDEF jcn_real     jcn_map(jcn_real x, jcn_real low, jcn_real high);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // JC_NOISE_H

#ifdef JC_NOISE_IMPLEMENTATION

#ifdef __cplusplus
extern "C" {
#endif

unsigned char g_jcn_perm[512] = {0};

static inline int jcn_hash_1(int x) {
    return g_jcn_perm[x];
}
static inline int jcn_hash_2(int x, int y) {
    return g_jcn_perm[g_jcn_perm[x] + y];
}
static inline int jcn_hash_3(int x, int y, int z) {
    return g_jcn_perm[g_jcn_perm[g_jcn_perm[x] + y] +z];
}
static inline jcn_real jcn_fade(jcn_real t) {
    return t * t * t * (t * (t * 6 - 15) + 10);
}

// Credit: https://www.scratchapixel.com/code.php?id=57&origin=/lessons/procedural-generation-virtual-worlds/perlin-noise-part-2
// perm: [0..255]
static jcn_real jcn_grad_dot_v(unsigned int perm, jcn_real x, jcn_real y, jcn_real z)
{
    switch (perm & 15) {
    case  0: return  x + y; // (1,1,0)
    case  1: return -x + y; // (-1,1,0)
    case  2: return  x - y; // (1,-1,0)
    case  3: return -x - y; // (-1,-1,0)
    case  4: return  x + z; // (1,0,1)
    case  5: return -x + z; // (-1,0,1)
    case  6: return  x - z; // (1,0,-1)
    case  7: return -x - z; // (-1,0,-1)
    case  8: return  y + z; // (0,1,1),
    case  9: return -y + z; // (0,-1,1),
    case 10: return  y - z; // (0,1,-1),
    case 11: return -y - z; // (0,-1,-1)
    case 12: return  y + x; // (1,1,0)
    case 13: return -x + y; // (-1,1,0)
    case 14: return -y + z; // (0,-1,1)
    case 15: return -y - z; // (0,-1,-1)
    }
    return 0;
}

static jcn_real jcn_grad(int hash, jcn_real x, jcn_real y, jcn_real z) {
    int h = hash & 15;                          // CONVERT LO 4 BITS OF HASH CODE
    jcn_real u = h<8 ? x : y,                   // INTO 12 GRADIENT DIRECTIONS.
         v = h<4 ? y : h==12||h==14 ? x : z;
    return ((h&1) == 0 ? u : -u) + ((h&2) == 0 ? v : -v);
}

static inline jcn_real jcn_lerp(jcn_real t, jcn_real a, jcn_real b) {
    return a + t * (b - a);
}

// References:
// http://weber.itn.liu.se/~stegu/simplexnoise/simplexnoise.pdf
// https://github.com/SRombauts/SimplexNoise/tree/master/src
// https://gist.github.com/Slipyx/2372043

struct jcn_simplex_data {
    double grad3[12*3];
    uint8_t p[256];
    uint8_t perm[512];
    uint8_t permMod12[512];
};

static void jc_noise_simplex_init(struct jcn_simplex_data* data)
{

    const uint8_t perm[256] = {
        151,160,137,91,90,15,131,13,201,95,96,53,194,233,7,225,140,36,103,30,69,
        142,8,99,37,240,21,10,23,190,6,148,247,120,234,75,0,26,197,62,94,252,219,
        203,117,35,11,32,57,177,33,88,237,149,56,87,174,20,125,136,171,168,68,175,
        74,165,71,134,139,48,27,166,77,146,158,231,83,111,229,122,60,211,133,230,
        220,105,92,41,55,46,245,40,244,102,143,54,65,25,63,161,1,216,80,73,209,76,
        132,187,208,89,18,169,200,196,135,130,116,188,159,86,164,100,109,198,173,
        186,3,64,52,217,226,250,124,123,5,202,38,147,118,126,255,82,85,212,207,206,
        59,227,47,16,58,17,182,189,28,42,223,183,170,213,119,248,152,2,44,154,163,
        70,221,153,101,155,167,43,172,9,129,22,39,253,19,98,108,110,79,113,224,232,
        178,185,112,104,218,246,97,228,251,34,242,193,238,210,144,12,191,179,162,
        241,81,51,145,235,249,14,239,107,49,192,214,31,181,199,106,157,184,84,204,
        176,115,121,50,45,127,4,150,254,138,236,205,93,222,114,67,29,24,72,243,141,
        128,195,78,66,215,61,156,180
    };

    for ( uint16_t i = 0; i < 512; ++i ) {
        data->perm[i] = perm[i & 255];
        data->permMod12[i] = (uint8_t)(data->perm[i] % 12);
    }

    const double grad3[12*3] = {
        1,1,0,-1,1,0,1,-1,0,-1,-1,0,1,0,1,-1,0,1,1,0,-1,-1,0,-1,0,1,1,0,-1,1,0,1,-1,0,-1,-1
    };
    JCN_MEMCPY(data->grad3, grad3, sizeof(grad3));
}

static int32_t jcn_fast_floor(jcn_real x)
{
    int xi = (int)x;
    return x < 0 ? xi - 1 : xi;
}

static double _jc_noise_simplex_dot( double ax, double ay, double bx, double by )
{
    return ax * bx + ay * by;
}

static float _jc_noise_simplex_grad2D(int32_t hash, float x, float y) {
    const int32_t h = hash & 0x3F;  // Convert low 3 bits of hash code
    const float u = h < 4 ? x : y;  // into 8 simple gradient directions,
    const float v = h < 4 ? y : x;
    return ((h & 1) ? -u : u) + ((h & 2) ? -2.0f * v : 2.0f * v); // and compute the dot product with (x,y).
}

static double jc_noise_simplex_noise(struct jcn_simplex_data* data, double xin, double yin)
{
    const double F2 = 0.5 * (JCN_SQRT( 3.0 ) - 1.0);
    const double G2 = (3.0 - JCN_SQRT( 3.0 )) / 6.0;

    double s = (xin + yin) * F2;
    int32_t i = jcn_fast_floor( xin + s );
    int32_t j = jcn_fast_floor( yin + s );
    double t = (i + j) * G2;
    double x0 = xin - (i - t);
    double y0 = yin - (j - t);
    uint8_t i1 = 0, j1 = 1;
    if ( x0 > y0 ) {
        i1 = 1;
        j1 = 0;
    }
    double x1 = x0 - i1 + G2;
    double y1 = y0 - j1 + G2;
    double x2 = x0 - 1.0 + 2.0 * G2;
    double y2 = y0 - 1.0 + 2.0 * G2;
    uint8_t ii = i & 255;
    uint8_t jj = j & 255;
    uint8_t gi0 = data->permMod12[ii + data->perm[jj]];
    uint8_t gi1 = data->permMod12[ii + i1 + data->perm[jj + j1]];
    uint8_t gi2 = data->permMod12[ii + 1 + data->perm[jj + 1]];
    double n0 = 0.0;
    double t0 = 0.5 - x0 * x0 - y0 * y0;
    if ( t0 >= 0.0 ) {
        t0 *= t0;
        n0 = t0 * t0 * _jc_noise_simplex_grad2D(gi0, x0, y0 );
    }
    double n1 = 0.0;
    double t1 = 0.5 - x1 * x1 - y1 * y1;
    if ( t1 >= 0.0 ) {
        t1 *= t1;
        n1 = t1 * t1 * _jc_noise_simplex_grad2D(gi1, x1, y1 );
    }
    double n2 = 0.0;
    double t2 = 0.5 - x2 * x2 - y2 * y2;
    if ( t2 >= 0.0 ) {
        t2 *= t2;
        n2 = t2 * t2 * _jc_noise_simplex_grad2D(gi2, x2, y2 );
    }
    return 70.0f * (n0 + n1 + n2);
}


// References Perlin noise:
// https://mrl.nyu.edu/~perlin/noise/
// https://www.scratchapixel.com/lessons/procedural-generation-virtual-worlds/perlin-noise-part-2/improved-perlin-noise
// https://github.com/Reputeless/PerlinNoise/blob/master/PerlinNoise.hpp
// https://gist.github.com/nowl/828013
//


struct jcn_perlin_data {
    double grad3[12*3];
    uint8_t p[256];
    uint8_t perm[512];
    uint8_t permMod12[512];
};

// Noise with derivatives:
// http://www.iquilezles.org/www/articles/morenoise/morenoise.htm

jcn_real jcn_perlin_noise_2d(jcn_real x, jcn_real y)
{
    int xi0 = jcn_fast_floor(x) & 255;
    int yi0 = jcn_fast_floor(y) & 255;
    int xi1 = (xi0 + 1) & 255;
    int yi1 = (yi0 + 1) & 255;
    x -= jcn_fast_floor(x);
    y -= jcn_fast_floor(y);
    jcn_real u = jcn_fade(x);
    jcn_real v = jcn_fade(y);
    // jcn_real x0 = x, x1 = x - 1.0f;
    // jcn_real y0 = y, y1 = y - 1.0f;
    // jcn_real out = jcn_lerp(v,  jcn_lerp(u, jcn_grad_dot_v(jcn_hash_2(xi0, yi0), x0, y0, 0 ),
    //                                         jcn_grad_dot_v(jcn_hash_2(xi1, yi0), x1, y0, 0 )),
    //                             jcn_lerp(u, jcn_grad_dot_v(jcn_hash_2(xi0, yi1), x0, y1, 0 ),
    //                                         jcn_grad_dot_v(jcn_hash_2(xi1, yi1), x1, y1, 0 )));

    jcn_real out = jcn_lerp(v,  jcn_lerp(u, jcn_grad_dot_v(jcn_hash_2(xi0, yi0), x  , y  , 0 ),
                                            jcn_grad_dot_v(jcn_hash_2(xi1, yi0), x-1, y  , 0 )),
                                jcn_lerp(u, jcn_grad_dot_v(jcn_hash_2(xi0, yi1), x  , y-1, 0 ),
                                            jcn_grad_dot_v(jcn_hash_2(xi1, yi1), x-1, y-1, 0 )));

    // jcn_real out = jcn_lerp(v,  jcn_lerp(u, jcn_grad(jcn_hash_2(xi0, yi0), x0, y0, 0 ),
    //                                         jcn_grad(jcn_hash_2(xi1, yi0), x1, y0, 0 )),
    //                             jcn_lerp(u, jcn_grad(jcn_hash_2(xi0, yi1), x0, y1, 0 ),
    //                                         jcn_grad(jcn_hash_2(xi1, yi1), x1, y1, 0 )));

    //printf("%f, %f: %f\n", x+xi0, y+yi0, out*0.5 + 0.5f);

    return out;
}


///////////////////////////////////////////////////////////////////////////

jcn_context* jcn_create(enum jcn_type type, unsigned int seed)
{
    JCN_SRAND(seed);

    for( int i = 0; i < 256; ++i )
    {
        g_jcn_perm[i] = i;
    }
    for( int i = 0; i < 256; ++i )
    {
        // shuffle the values
        int i2 = JCN_RAND() & 255;
        unsigned int v = g_jcn_perm[i];
        g_jcn_perm[i] = g_jcn_perm[i2];
        g_jcn_perm[i2] = v;
    }
    // Extend the values
    for( int i = 0; i < 256; ++i )
    {
        g_jcn_perm[i+256] = g_jcn_perm[i];
    }

    jcn_context* ctx = (jcn_context*)malloc(sizeof(jcn_context));
    ctx->type = type;

    switch(type) {
    case JCN_TYPE_PERLIN:
        break;
    case JCN_TYPE_SIMPLEX:
        break;
    }

    return ctx;
}

void jcn_destroy(jcn_context* ctx)
{
    free(ctx);
}


jcn_real jcn_noise_2d(jcn_context* ctx, jcn_real x, jcn_real y)
{
    switch(ctx->type)
    {
    case JCN_TYPE_PERLIN: return jcn_perlin_noise_2d(x, y);
    case JCN_TYPE_SIMPLEX: return 0;
    }
}


// fbm et.al
// fbm
// https://code.google.com/archive/p/fractalterraingeneration/wikis/Fractional_Brownian_Motion.wiki
// http://www.iquilezles.org/www/articles/warp/warp.htm
// https://youtu.be/SePDzis8HqY?t=1667

// Analytical derivative
// https://youtu.be/SePDzis8HqY?t=1823

jcn_real jcn_fbm_2d(jcn_context* ctx, int octaves, jcn_real amplitude, jcn_real frequency, jcn_real lacunarity, jcn_real gain, jcn_real x, jcn_real y)
{
    jcn_real sum = 0.0f;
    jcn_real sum_amp = 0.0f;
    for(int i = 0; i < octaves; ++i)
    {
        sum += jcn_noise_2d(ctx, x*frequency, y*frequency) * amplitude;
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
}

jcn_real jcn_remap(jcn_real value, jcn_real low1, jcn_real high1, jcn_real low2, jcn_real high2)
{
    return low2 + (value - low1) * (high2 - low2) / (high1 - low1);
}

#ifdef __cplusplus
} // extern "C"
#endif

#endif // JC_NOISE_IMPLEMENTATION
