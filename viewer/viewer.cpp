
#if defined(__APPLE__)
    #define SOKOL_METAL
#elif defined(WIN32)
    #define SOKOL_D3D11
#elif defined(__EMSCRIPTEN__)
    #define SOKOL_GLES2
#else
    #error "No GFX Backend Specified"
#endif

#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_time.h"

#include "imgui.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#define HANDMADE_MATH_IMPLEMENTATION
#include "HandmadeMath.h"

#include "mapmaker.h"

#define JC_VORONOI_IMPLEMENTATION
#include "jc_voronoi.h"

extern void imgui_sokol_event(const sapp_event* event);
extern void imgui_setup();
extern void imgui_teardown();

static void draw_voronoi(uint8_t* pixels, const SMap* map);

static SVoronoiParameters   g_VoronoiParams;
static SNoiseParameters     g_NoiseParams;
static SMapParameters       g_MapParams;
static uint32_t g_VoronoiParamsHash = 0;
static uint32_t g_NoiseParamsHash = 0;
static uint32_t g_MapParamsHash = 0;


int image_area_width = 512;
int image_area_height = 512;
int imgui_width = 256;

float* noisef = 0;
float* sediment = 0;
float* water = 0;

uint8_t* heights = 0;
uint8_t* colors = 0;
uint8_t* pixels = 0;

uint8_t height_limits[] = {
    55, 96, 116, 165, 190, 220, 255
};

uint8_t height_colors[] = { 8, 134, 255,   // deep water
                        77, 171, 255,   // shallow water
                        252, 225, 146,  // beach
                        149, 215, 1,    // plains,
                        181, 159, 122,  // mountain low
                        128, 126, 129,  // mountain high
                        255, 255, 255   // snow
};

uint64_t last_time = 0;

typedef struct {
    hmm_mat4 mvp;
} vs_params_t;

hmm_mat4 view_proj;
sg_pass_action pass_action;
sg_draw_state draw_state = {0};

float rx = 0.0f;
float ry = 0.0f;
int update_count = 0;

extern const char *vs_src, *fs_src;

static void init(void) {
    sg_desc desc = (sg_desc){
        .mtl_device = sapp_metal_get_device(),
        .mtl_renderpass_descriptor_cb = sapp_metal_get_renderpass_descriptor,
        .mtl_drawable_cb = sapp_metal_get_drawable,
        .d3d11_device = sapp_d3d11_get_device(),
        .d3d11_device_context = sapp_d3d11_get_device_context(),
        .d3d11_render_target_view_cb = sapp_d3d11_get_render_target_view,
        .d3d11_depth_stencil_view_cb = sapp_d3d11_get_depth_stencil_view
    };
    sg_setup(&desc);

    pass_action = (sg_pass_action) {
        .colors[0] = { .action=SG_ACTION_CLEAR, .val={0.34f, 0.34f, 0.34f, 1.0f} }
    };

    float vertices[] = {
         1.0f,  1.0f,  1.0f,   1.0f, 1.0f, 1.0f, 1.0f,     1.0f, 1.0f,
         1.0f, -1.0f,  1.0f,   1.0f, 1.0f, 1.0f, 1.0f,     1.0f, 0.0f,
        -1.0f, -1.0f,  1.0f,   1.0f, 1.0f, 1.0f, 1.0f,     0.0f, 0.0f,
        -1.0f,  1.0f,  1.0f,   1.0f, 1.0f, 1.0f, 1.0f,     0.0f, 1.0f,
    };
    uint16_t indices[] = { 0, 1, 2,  0, 2, 3 };

    sg_buffer_desc vertexbuf_desc = (sg_buffer_desc){
        .size = sizeof(vertices),
        .content = vertices,
    };
    sg_buffer vbuf = sg_make_buffer(&vertexbuf_desc);

    sg_buffer_desc indexbuf_desc = (sg_buffer_desc){
        .type = SG_BUFFERTYPE_INDEXBUFFER,
        .size = sizeof(indices),
        .content = indices,
    };
    sg_buffer ibuf = sg_make_buffer(&indexbuf_desc);

    sg_shader_desc shader_desc = (sg_shader_desc){
        .vs.uniform_blocks[0] = {
            .size = sizeof(vs_params_t),
            .uniforms = {
                [0] = { .name="mvp", .type=SG_UNIFORMTYPE_MAT4 }
            }
        },
        .fs.images[0] = { .name="tex", .type=SG_IMAGETYPE_2D },
        .vs.source = vs_src,
        .fs.source = fs_src
    };
    sg_shader shd = sg_make_shader(&shader_desc);

    sg_pipeline_desc pipeline_desc = (sg_pipeline_desc){
        .layout = {
            .attrs = {
                [0] = { .name="position",   .format=SG_VERTEXFORMAT_FLOAT3 },
                [1] = { .name="color0",     .format=SG_VERTEXFORMAT_FLOAT4 },
                [2] = { .name="texcoord0",  .format=SG_VERTEXFORMAT_FLOAT2 }
            },
        },
        .shader = shd,
        .index_type = SG_INDEXTYPE_UINT16,
        .depth_stencil = {
            .depth_compare_func = SG_COMPAREFUNC_LESS_EQUAL,
            .depth_write_enabled = true
        },
        .rasterizer.cull_mode = SG_CULLMODE_BACK,
        //.rasterizer.sample_count = 4
    };
    sg_pipeline pip = sg_make_pipeline(&pipeline_desc);

    sg_image_desc img_desc = (sg_image_desc){
        .width = g_MapParams.width,
        .height = g_MapParams.height,
        .pixel_format = SG_PIXELFORMAT_RGBA8,
        .usage = SG_USAGE_STREAM,
        .min_filter = SG_FILTER_LINEAR,
        .mag_filter = SG_FILTER_LINEAR,
        .wrap_u = SG_WRAP_CLAMP_TO_EDGE,
        .wrap_v = SG_WRAP_CLAMP_TO_EDGE
    };
    sg_image img = sg_make_image(&img_desc);

    /* setup the draw state */
    draw_state = (sg_draw_state) {
        .pipeline = pip,
        .vertex_buffers[0] = vbuf,
        .index_buffer = ibuf,
        .fs_images[0] = img
    };

    stm_setup();
    imgui_setup();

    uint32_t size = g_MapParams.width * g_MapParams.height;
    pixels = (uint8_t*)malloc(size*4);
    memset(pixels, 0xFF, size*4);

    colors = (uint8_t*)malloc(size*3);
    memset(colors, 0xFF, size*3);

    heights = (uint8_t*)malloc(size);
    memset(heights, 0, size);

    noisef = (float*)malloc(size*sizeof(float));
    memset(noisef, 0, size*sizeof(float));

    sediment = (float*)malloc(size*sizeof(float));
    memset(sediment, 0, size*sizeof(float));
    water = (float*)malloc(size*sizeof(float));
    memset(water, 0, size*sizeof(float));

    memcpy(g_MapParams.colors, height_colors, sizeof(height_colors));
    memcpy(g_MapParams.limits, height_limits, sizeof(height_limits));

    srand(time(0));

    // Init settings
    g_NoiseParams.seed = 771;
    g_NoiseParams.fbm_octaves = 8;
    g_NoiseParams.fbm_frequency = 0.158f;
    g_NoiseParams.fbm_lacunarity = 1.582f;
    g_NoiseParams.fbm_amplitude = 1.0f;
    g_NoiseParams.fbm_gain = 0.831f;

    g_NoiseParams.noise_modify_type = 0;
    g_NoiseParams.noise_contrast_type = 2;



    // g_NoiseParams.contrast_exponent = 2.746f;
    // g_NoiseParams.fbm_frequency = 1.5f;
    g_NoiseParams.erode_iterations = 100;
    g_NoiseParams.erode_thermal_talus = 0.68f;

    g_VoronoiParams.generation_type = 1;
}


static void frame(void) {
    int w = sapp_width();
    int h = sapp_height();

    static bool show_voronoi = false;
    static bool show_noise = false;
    static bool show_sediment = false;
    static bool show_water = false;
    static int noise_type = 0;

    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2(w, h);
    io.DeltaTime = (float) stm_sec(stm_laptime(&last_time));
    ImGui::NewFrame();

    ImGui::SetNextWindowPos(ImVec2((w * 2) / 3, 0));
    ImGui::SetNextWindowSize(ImVec2((w * 1) / 3, h));
    ImGui::Begin("Config", 0);

    ImGui::SetWindowSize(ImVec2(imgui_width, image_area_height));

    if (ImGui::CollapsingHeader("Noise"))
    {
        ImGui::Checkbox("Show Noise", &show_noise);

        ImGui::InputInt("seed", &g_NoiseParams.seed, 1, 127);
        if (ImGui::SmallButton("Random")) {
            g_NoiseParams.seed = rand() & 0xFFFF;
        }

        ImGui::Combo("Noise Type", &noise_type, "Perlin\0Simplex (not implemented)\0");

        if (ImGui::CollapsingHeader("fBm")) {
            ImGui::Text("fBm");
            ImGui::InputInt("octaves", &g_NoiseParams.fbm_octaves);
            ImGui::SliderFloat("frequency", &g_NoiseParams.fbm_frequency, 0.01f, 1.5f);
            ImGui::SliderFloat("lacunarity", &g_NoiseParams.fbm_lacunarity, 1.0f, 3.0f);
            ImGui::SliderFloat("amplitude", &g_NoiseParams.fbm_amplitude, 0.01f, 10.0f);
            ImGui::SliderFloat("gain", &g_NoiseParams.fbm_gain, 0.01f, 2.0f);
        }

        ImGui::Separator();

        ImGui::Combo("Perturb Type", &g_NoiseParams.perturb_type, "None\0Perturb 1\0Perturb 2\0");
        if (g_NoiseParams.perturb_type == 1)
        {
            ImGui::SliderFloat("scale", &g_NoiseParams.perturb1_scale, 0.01f, 256.0f);
            ImGui::SliderFloat("a1", &g_NoiseParams.perturb1_a1, 0.01f, 10.0f);
            ImGui::SliderFloat("a2", &g_NoiseParams.perturb1_a2, 0.01f, 10.0f);
        }
        else if (g_NoiseParams.perturb_type == 2)
        {
            ImGui::SliderFloat("scale", &g_NoiseParams.perturb2_scale, 0.01f, 256.0f);
            ImGui::SliderFloat("qyx", &g_NoiseParams.perturb2_qyx, 0.01f, 10.0f);
            ImGui::SliderFloat("qyy", &g_NoiseParams.perturb2_qyy, 0.01f, 10.0f);
            ImGui::SliderFloat("rxx", &g_NoiseParams.perturb2_rxx, 0.01f, 10.0f);
            ImGui::SliderFloat("rxy", &g_NoiseParams.perturb2_rxy, 0.01f, 10.0f);
            ImGui::SliderFloat("ryx", &g_NoiseParams.perturb2_ryx, 0.01f, 10.0f);
            ImGui::SliderFloat("ryy", &g_NoiseParams.perturb2_ryy, 0.01f, 10.0f);
        }

        ImGui::Separator();
        ImGui::Text("Processing");

        ImGui::Checkbox("Use Radial Falloff", &g_NoiseParams.apply_radial);
        ImGui::SliderFloat("Radial Falloff", &g_NoiseParams.radial_falloff, 0.001f, 1.0f);

        ImGui::Combo("Modification Type", &g_NoiseParams.noise_modify_type, "None\0Billow\0Ridge\0");

        ImGui::SliderFloat("Power", &g_NoiseParams.contrast_exponent, 0.01f, 10.0f);
    }

    if (ImGui::CollapsingHeader("Erosion")) {
        ImGui::Checkbox("Use Erosion", &g_NoiseParams.use_erosion);

        ImGui::Combo("Erosion Type", &g_NoiseParams.erode_type, "Thermal\0Hydraulic\0");
        ImGui::InputInt("Iterations", &g_NoiseParams.erode_iterations);

        if (g_NoiseParams.erode_type == 0)
        {
            ImGui::SliderFloat("Talus", &g_NoiseParams.erode_thermal_talus, 0.0f, 16.0f);
        }
        else if (g_NoiseParams.erode_type == 1)
        {
            ImGui::SliderFloat("Rain Amount", &g_NoiseParams.erode_rain_amount, 0.0f, 5.0f);
            ImGui::SliderFloat("Solubility", &g_NoiseParams.erode_solubility, 0.0f, 1.0f);
            ImGui::SliderFloat("Evaporation", &g_NoiseParams.erode_evaporation, 0.0f, 1.0f);
            ImGui::SliderFloat("Capacity", &g_NoiseParams.erode_capacity, 0.0f, 5.0f);
            ImGui::Checkbox("Show Sediment", &show_sediment);
            ImGui::Checkbox("Show Water", &show_water);
        }
    }

    if (ImGui::CollapsingHeader("Voronoi"))
    {
        ImGui::Checkbox("Show Cells", &show_voronoi);

        ImGui::InputInt("seed", &g_VoronoiParams.seed, 1, 127);
        if (ImGui::SmallButton("Random")) {
            g_VoronoiParams.seed = rand() & 0xFFFF;
        }

        ImGui::SliderInt("Border", &g_VoronoiParams.border, 0, g_MapParams.width/2);

        ImGui::Combo("Cell Type", &g_VoronoiParams.generation_type, "Random\0Hexagonal\0");

        if (g_VoronoiParams.generation_type == 0) // random
        {
            ImGui::SliderInt("Num Cells", &g_VoronoiParams.num_cells, 1, 16*1024);
            ImGui::SliderInt("Relax", &g_VoronoiParams.num_relaxations, 0, 100);
        }
        else if(g_VoronoiParams.generation_type == 1) // hexagonal
        {
            ImGui::SliderInt("Density", &g_VoronoiParams.hexagon_density, 1, 128);
        }
    }

    if (ImGui::CollapsingHeader("Map")) {

        ImGui::SliderInt("Sea Level", &g_MapParams.sea_level, 0, 255);

        if (ImGui::CollapsingHeader("Colors")) {
            ImGui::Text("Elevation limits and their colors");

            size_t num_limits = sizeof(height_limits)/sizeof(height_limits[0]);
            for( int i = 0; i < num_limits; ++i)
            {
                char name[64];

                snprintf(name, sizeof(name), "Color %d", i);
                float c[3] = { g_MapParams.colors[i*3+0]/255.0f, g_MapParams.colors[i*3+1]/255.0f, g_MapParams.colors[i*3+2]/255.0f};
                if (ImGui::ColorEdit3(name, c, ImGuiColorEditFlags_NoInputs|ImGuiColorEditFlags_NoLabel)) {
                    g_MapParams.colors[i*3+0] = (uint8_t)(c[0] * 255.0f);
                    g_MapParams.colors[i*3+1] = (uint8_t)(c[1] * 255.0f);
                    g_MapParams.colors[i*3+2] = (uint8_t)(c[2] * 255.0f);
                }

                ImGui::SameLine();

                snprintf(name, sizeof(name), "%d", i);

                int limit = g_MapParams.limits[i];
                int prev = i == 0 ? 0 : g_MapParams.limits[i-1];
                int next = i == num_limits-1 ? 255 : g_MapParams.limits[i+1];
                if (ImGui::SliderInt(name, &limit, prev, next)) {
                    g_MapParams.limits[i] = (uint8_t)limit;
                }
            }
        }
        ImGui::Checkbox("Use Shading", &g_MapParams.use_shading);
        ImGui::SliderInt("Shadow Step Length", &g_MapParams.shadow_step_length, 1, 16);
        ImGui::SliderFloat("Shadow Strength", &g_MapParams.shadow_strength, 0.0f, 1.0f);
        ImGui::SliderFloat("Shadow Strength Sea", &g_MapParams.shadow_strength_sea, 0.0f, 1.0f);
    }

    ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);

    ImGui::End();

    /////////////////////////////////////////
    // Check for updates
    uint32_t prev_voronoi_param_hash = g_VoronoiParamsHash;
    uint32_t prev_noise_param_hash = g_NoiseParamsHash;
    uint32_t prev_map_param_hash = g_MapParamsHash;

    g_VoronoiParamsHash = Hash((void*)&g_VoronoiParams, sizeof(g_VoronoiParams));
    g_NoiseParamsHash = Hash((void*)&g_NoiseParams, sizeof(g_NoiseParams));
    g_MapParamsHash = Hash((void*)&g_MapParams, sizeof(g_MapParams));
    bool update_voronoi = prev_voronoi_param_hash != g_VoronoiParamsHash;
    bool update_noise = prev_noise_param_hash != g_NoiseParamsHash;
    bool update_map = update_noise || update_voronoi || prev_map_param_hash != g_MapParamsHash;

    if (update_map)
    {
        UpdateParams(&g_VoronoiParams, &g_NoiseParams, &g_MapParams);
    }

    if (update_voronoi)
    {
        GenerateVoronoi();
    }

    if (update_noise)
    {
        int size = g_MapParams.width*g_MapParams.height;
        memset(sediment, 0, size*sizeof(float));
        memset(water, 0, size*sizeof(float));

        if (g_NoiseParams.perturb_type == 0)
            GenerateNoise(noisef, g_NoiseParams.noise_modify_type);
        else if(g_NoiseParams.perturb_type == 1)
            Perturb1(g_MapParams.width, g_MapParams.height, noisef);
        else if(g_NoiseParams.perturb_type == 2)
            Perturb2(g_MapParams.width, g_MapParams.height, noisef);

        if (g_NoiseParams.perturb_type != 0)
        {
            Blur(g_MapParams.width, g_MapParams.height, noisef, 255.0f);
            Blur(g_MapParams.width, g_MapParams.height, noisef, 255.0f);
            Blur(g_MapParams.width, g_MapParams.height, noisef, 255.0f);
            Blur(g_MapParams.width, g_MapParams.height, noisef, 255.0f);
        }

        ContrastNoise(noisef, g_NoiseParams.contrast_exponent);
        if (g_NoiseParams.use_erosion)
            Erode(g_MapParams.width, g_MapParams.height, noisef, sediment, water);

        Normalize(g_MapParams.width, g_MapParams.height, noisef);

        if (g_NoiseParams.apply_radial)
            NoiseRadial(g_MapParams.width, g_MapParams.height, g_NoiseParams.radial_falloff, noisef);

        Normalize(g_MapParams.width, g_MapParams.height, noisef);
    }

    if (update_map)
    {
        for( int i = 0; i < g_MapParams.width * g_MapParams.height; ++i )
            heights[i] = (uint8_t)255.0f * noisef[i];

        GenerateMap(heights);
        ColorizeMap(heights, colors, sizeof(height_limits)/sizeof(height_limits[0]), g_MapParams.limits, g_MapParams.colors);
    }

    if (show_water) {
        for(int i = 0; i < g_MapParams.width * g_MapParams.height; ++i) {
            pixels[i*4 + 0] = (uint8_t)(255.0f * water[i]);
            pixels[i*4 + 1] = (uint8_t)(255.0f * water[i]);
            pixels[i*4 + 2] = (uint8_t)(255.0f * water[i]);
            pixels[i*4 + 3] = 0xFF;
        }
    }
    else if (show_sediment) {
        for(int i = 0; i < g_MapParams.width * g_MapParams.height; ++i) {
            pixels[i*4 + 0] = (uint8_t)(255.0f * sediment[i]);
            pixels[i*4 + 1] = (uint8_t)(255.0f * sediment[i]);
            pixels[i*4 + 2] = (uint8_t)(255.0f * sediment[i]);
            pixels[i*4 + 3] = 0xFF;
        }
    }
    else if (show_noise) {
        for(int i = 0; i < g_MapParams.width * g_MapParams.height; ++i) {
            pixels[i*4 + 0] = heights[i];
            pixels[i*4 + 1] = heights[i];
            pixels[i*4 + 2] = heights[i];
            pixels[i*4 + 3] = 0xFF;
        }
    }
    else if (show_voronoi) {
        SMap* map = GetMap();
        draw_voronoi(pixels, map);
    }
    else
    {
        for(int i = 0; i < g_MapParams.width * g_MapParams.height; ++i) {
            pixels[i*4 + 0] = colors[i*3 + 0];
            pixels[i*4 + 1] = colors[i*3 + 1];
            pixels[i*4 + 2] = colors[i*3 + 2];
            pixels[i*4 + 3] = 0xFF;
        }
    }

    sg_image_content img_content = (sg_image_content){
        .subimage[0][0] = {
            .ptr = pixels,
            .size = g_MapParams.width*g_MapParams.height*4
        }
    };
    sg_update_image(draw_state.fs_images[0], &img_content);

    hmm_mat4 proj = HMM_Orthographic(-1, 2, -1, 1, 0.01f, 5.0f);
    hmm_mat4 view = HMM_LookAt(HMM_Vec3(0.0f, 0.0f, 4.0f), HMM_Vec3(0.0f, 0.0f, 0.0f), HMM_Vec3(0.0f, 1.0f, 0.0f));
    hmm_mat4 view_proj = HMM_MultiplyMat4(proj, view);

    hmm_mat4 identity = HMM_Mat4d(1);
    vs_params_t vs_params;
    vs_params.mvp = HMM_MultiplyMat4(view_proj, identity);

    sg_begin_default_pass(&pass_action, w, h);
    sg_apply_draw_state(&draw_state);
    sg_apply_uniform_block(SG_SHADERSTAGE_VS, 0, &vs_params, sizeof(vs_params));
    sg_draw(0, 6, 1);

    ImGui::Render();
    sg_end_pass();
    sg_commit();
}

static void cleanup(void) {
    imgui_teardown();
    sg_shutdown();
    free(pixels);
    free(heights);
    free(noisef);
}

static void log_msg(const char* msg) {
    printf("%s\n", msg);
    fflush(stdout);
}


void event(const sapp_event* event) {
    imgui_sokol_event(event);

    if (event->type == SAPP_EVENTTYPE_KEY_DOWN) {
        if (event->key_code == SAPP_KEYCODE_ESCAPE) {
            exit(0);
        }
    }
    else if(event->type == SAPP_EVENTTYPE_RESIZED) {
        //printf("RESIZED: %d, %d\n", event->framebuffer_width, event->framebuffer_height);
    }
}

sapp_desc sokol_main(int argc, char* argv[]) {
    sapp_desc desc;
    desc.init_cb = init;
    desc.frame_cb = frame;
    desc.cleanup_cb = cleanup;
    desc.event_cb = event;
    desc.width = image_area_width + imgui_width;
    desc.height = image_area_height;
    desc.window_title = "MapMaker";
    return desc;
}


#if defined(SOKOL_METAL)
const char* vs_src =
    "#include <metal_stdlib>\n"
    "using namespace metal;\n"
    "struct params_t {\n"
    "  float4x4 mvp;\n"
    "};\n"
    "struct vs_in {\n"
    "  float4 position [[attribute(0)]];\n"
    "  float4 color [[attribute(1)]];\n"
    "  float2 uv [[attribute(2)]];\n"
    "};\n"
    "struct vs_out {\n"
    "  float4 pos [[position]];\n"
    "  float4 color;\n"
    "  float2 uv;\n"
    "};\n"
    "vertex vs_out _main(vs_in in [[stage_in]], constant params_t& params [[buffer(0)]]) {\n"
    "  vs_out out;\n"
    "  out.pos = params.mvp * in.position;\n"
    "  out.color = float4(in.color.xyz, 1.0);\n"
    "  out.uv = in.uv;\n"
    "  return out;\n"
    "}\n";
const char* fs_src =
    "#include <metal_stdlib>\n"
    "using namespace metal;\n"
    "struct fs_in {\n"
    "  float4 color;\n"
    "  float2 uv;\n"
    "};\n"
    "fragment float4 _main(fs_in in [[stage_in]], texture2d<float> tex [[texture(0)]], sampler smp [[sampler(0)]]) {\n"
    "  return float4(tex.sample(smp, in.uv).xyz, 1.0) * in.color;\n"
    "};\n";

#elif defined(SOKOL_GLES2)
const char* vs_src =
    "uniform mat4 mvp;\n"
    "attribute vec4 position;\n"
    "attribute vec4 color0;\n"
    "attribute vec2 texcoord0;\n"
    "varying vec2 uv;"
    "varying vec4 color;"
    "void main() {\n"
    "  gl_Position = mvp * position;\n"
    "  uv = texcoord0;\n"
    "  color = color0;\n"
    "}\n";
const char* fs_src =
    "precision mediump float;\n"
    "uniform sampler2D tex;\n"
    "varying vec4 color;\n"
    "varying vec2 uv;\n"
    "void main() {\n"
    "  gl_FragColor = texture2D(tex, uv) * color;\n"
    "}\n";
#elif defined(SOKOL_D3D11)
const char* vs_src =
    "cbuffer params {\n"
    "  float2 disp_size;\n"
    "};\n"
    "struct vs_in {\n"
    "  float2 pos: POSITION;\n"
    "  float2 uv: TEXCOORD0;\n"
    "  float4 color: COLOR0;\n"
    "};\n"
    "struct vs_out {\n"
    "  float2 uv: TEXCOORD0;\n"
    "  float4 color: COLOR0;\n"
    "  float4 pos: SV_Position;\n"
    "};\n"
    "vs_out main(vs_in inp) {\n"
    "  vs_out outp;\n"
    "  outp.pos = float4(((inp.pos/disp_size)-0.5)*float2(2.0,-2.0), 0.5, 1.0);\n"
    "  outp.uv = inp.uv;\n"
    "  outp.color = inp.color;\n"
    "  return outp;\n"
    "}\n";
const char* fs_src =
    "Texture2D<float4> tex: register(t0);\n"
    "sampler smp: register(s0);\n"
    "float4 main(float2 uv: TEXCOORD0, float4 color: COLOR0): SV_Target0 {\n"
    "  return tex.Sample(smp, uv) * color;\n"
    "}\n";
#else
    #error "No shader implemented yet for this platform"
#endif


// temp test
static void draw_point(int x, int y, uint8_t* image, int width, int height, int nchannels, const uint8_t* color)
{
    if( x < 0 || y < 0 || x > (width-1) || y > (height-1) )
        return;
    int index = y * width * nchannels + x * nchannels;
    for( int i = 0; i < nchannels; ++i )
    {
        image[index+i] = color[i];
    }
}

// http://members.chello.at/~easyfilter/bresenham.html
static void draw_line(int x0, int y0, int x1, int y1, uint8_t* image, int width, int height, int nchannels, const uint8_t* color)
{
    int dx =  abs(x1-x0), sx = x0<x1 ? 1 : -1;
    int dy = -abs(y1-y0), sy = y0<y1 ? 1 : -1;
    int err = dx+dy, e2; // error value e_xy

    for(;;)
    {  // loop
        draw_point(x0,y0, image, width, height, nchannels, color);
        if (x0==x1 && y0==y1) break;
        e2 = 2*err;
        if (e2 >= dy) { err += dy; x0 += sx; } // e_xy+e_x > 0
        if (e2 <= dx) { err += dx; y0 += sy; } // e_xy+e_y < 0
    }
}

static inline int orient2d(const jcv_point* a, const jcv_point* b, const jcv_point* c)
{
    return ((int)b->x - (int)a->x)*((int)c->y - (int)a->y) - ((int)b->y - (int)a->y)*((int)c->x - (int)a->x);
}

static inline int min2(int a, int b)
{
    return (a < b) ? a : b;
}

static inline int max2(int a, int b)
{
    return (a > b) ? a : b;
}

static inline int min3(int a, int b, int c)
{
    return min2(a, min2(b, c));
}
static inline int max3(int a, int b, int c)
{
    return max2(a, max2(b, c));
}

static void draw_triangle(const jcv_point* v0, const jcv_point* v1, const jcv_point* v2, uint8_t* image, int width, int height, int nchannels, const uint8_t* color)
{
    int area = orient2d(v0, v1, v2);
    if( area == 0 )
        return;

    // Compute triangle bounding box
    int minX = min3((int)v0->x, (int)v1->x, (int)v2->x);
    int minY = min3((int)v0->y, (int)v1->y, (int)v2->y);
    int maxX = max3((int)v0->x, (int)v1->x, (int)v2->x);
    int maxY = max3((int)v0->y, (int)v1->y, (int)v2->y);

    // Clip against screen bounds
    minX = max2(minX, 0);
    minY = max2(minY, 0);
    maxX = min2(maxX, width - 1);
    maxY = min2(maxY, height - 1);

    // Rasterize
    jcv_point p;
    for (p.y = minY; p.y <= maxY; p.y++) {
        for (p.x = minX; p.x <= maxX; p.x++) {
            // Determine barycentric coordinates
            int w0 = orient2d(v1, v2, &p);
            int w1 = orient2d(v2, v0, &p);
            int w2 = orient2d(v0, v1, &p);

            // If p is on or inside all edges, render pixel.
            if (w0 >= 0 && w1 >= 0 && w2 >= 0)
            {
                draw_point((int)p.x, (int)p.y, image, width, height, nchannels, color);
            }
        }
    }
}

static void draw_voronoi(uint8_t* pixels, const SMap* map)
{
    uint8_t colorline[] = { 200, 200, 200, 255 };
    uint8_t colorwhite[] = { 255, 255, 255, 255 };
    uint8_t colorvertex[] = { 80, 200, 127, 255 };
    int num_channels = 4;
    uint8_t color_beach[] = {height_colors[6], height_colors[7], height_colors[8], 255};
    uint8_t color_water_shallow[] = {height_colors[3], height_colors[4], height_colors[5], 255};
    uint8_t color_water_deep[] = {height_colors[0], height_colors[1], height_colors[2], 255};

    int width = g_MapParams.width;
    int height = g_MapParams.height;
    memset(pixels, 255, width*height*num_channels );

    jcv_point* points = map->points;
    for( int i = 0; i < map->num_cells; ++i )
    {
        const SCell& cell = map->cells[i];
        if (cell.site == 0)
            continue;

        jcv_point p = points[i];

        const jcv_graphedge* e = cell.site->edges;
        while( e )
        {
            const jcv_point& p0 = e->pos[0];
            const jcv_point& p1 = e->pos[1];

            const uint8_t* color = color_water_shallow;
            if(cell.is_land)
            {
                color = color_beach;
            }
            else
            {
                if (cell.is_shallow)
                    color = color_water_shallow;
                else
                    color = color_water_deep;
            }

            draw_triangle( &p, &p0, &p1, pixels, width, height, num_channels, color);
            draw_line(p0.x, p0.y, p1.x, p1.y, pixels, width, height, num_channels, colorline);

            //printf("s: %f, %f:  e: %f, %f -> %f, %f\n", p.x, p.y, p0.x, p0.y, p1.x, p1.y);

            e = e->next;
        }

        draw_point( p.x, p.y, pixels, width, height, num_channels, colorwhite);
    }
}
