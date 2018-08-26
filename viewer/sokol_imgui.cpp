#include "imgui.h"

#if defined(__APPLE__)
    #define SOKOL_METAL
#elif defined(WIN32)
    #define SOKOL_D3D11
#elif defined(__EMSCRIPTEN__)
    #define SOKOL_GLES2
#else
    #error "No GFX Backend Specified"
#endif

#define SOKOL_IMPL

#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_time.h"
#include <stdio.h>

const int MaxVertices = (1<<16);
const int MaxIndices = MaxVertices * 3;

extern const char* vs_src_imgui;
extern const char* fs_src_imgui;

static sg_draw_state draw_state = { };
ImDrawVert vertices[MaxVertices];
uint16_t indices[MaxIndices];

typedef struct {
    ImVec2 disp_size;
} vs_params_t;


void imgui_sokol_event(const sapp_event* e) {
    switch (e->type) {
        case SAPP_EVENTTYPE_KEY_DOWN:
            ImGui::GetIO().KeysDown[e->key_code] = true;
            break;
        case SAPP_EVENTTYPE_KEY_UP:
            ImGui::GetIO().KeysDown[e->key_code] = false;
            break;
        case SAPP_EVENTTYPE_CHAR:
            ImGui::GetIO().AddInputCharacter(e->char_code);
            break;
        case SAPP_EVENTTYPE_MOUSE_DOWN:
            ImGui::GetIO().MouseDown[e->mouse_button] = true;
            break;
        case SAPP_EVENTTYPE_MOUSE_UP:
            ImGui::GetIO().MouseDown[e->mouse_button] = false;
            break;
        case SAPP_EVENTTYPE_MOUSE_SCROLL:
            ImGui::GetIO().MouseWheel = 0.25f * e->scroll_y;
            break;
        case SAPP_EVENTTYPE_MOUSE_MOVE:
            ImGui::GetIO().MousePos = ImVec2(e->mouse_x, e->mouse_y);
            break;
        default:
            break;
    }
}

static void imgui_draw_cb(ImDrawData* draw_data);

void imgui_setup() {
        // setup the imgui environment
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = 0;
    io.RenderDrawListsFn = imgui_draw_cb;
    io.Fonts->AddFontDefault();
    io.KeyMap[ImGuiKey_Tab] = SAPP_KEYCODE_TAB;
    io.KeyMap[ImGuiKey_LeftArrow] = SAPP_KEYCODE_LEFT;
    io.KeyMap[ImGuiKey_RightArrow] = SAPP_KEYCODE_RIGHT;
    io.KeyMap[ImGuiKey_DownArrow] = SAPP_KEYCODE_DOWN;
    io.KeyMap[ImGuiKey_UpArrow] = SAPP_KEYCODE_UP;
    io.KeyMap[ImGuiKey_Home] = SAPP_KEYCODE_HOME;
    io.KeyMap[ImGuiKey_End] = SAPP_KEYCODE_END;
    io.KeyMap[ImGuiKey_Delete] = SAPP_KEYCODE_DELETE;
    io.KeyMap[ImGuiKey_Backspace] = SAPP_KEYCODE_BACKSPACE;
    io.KeyMap[ImGuiKey_Enter] = SAPP_KEYCODE_ENTER;
    io.KeyMap[ImGuiKey_Escape] = SAPP_KEYCODE_ESCAPE;
    // io.KeyMap[ImGuiKey_A] = 0x00;
    // io.KeyMap[ImGuiKey_C] = 0x08;
    // io.KeyMap[ImGuiKey_V] = 0x09;
    // io.KeyMap[ImGuiKey_X] = 0x07;
    // io.KeyMap[ImGuiKey_Y] = 0x10;
    // io.KeyMap[ImGuiKey_Z] = 0x06;

    // // OSX => ImGui input forwarding
    // osx_mouse_pos([] (float x, float y) { ImGui::GetIO().MousePos = ImVec2(x, y); });
    // osx_mouse_btn_down([] (int btn)     { ImGui::GetIO().MouseDown[btn] = true; });
    // osx_mouse_btn_up([] (int btn)       { ImGui::GetIO().MouseDown[btn] = false; });
    // osx_mouse_wheel([] (float v)        { ImGui::GetIO().MouseWheel = 0.25f * v; });
    // osx_key_down([] (int key)           { if (key < 512) ImGui::GetIO().KeysDown[key] = true; });
    // osx_key_up([] (int key)             { if (key < 512) ImGui::GetIO().KeysDown[key] = false; });
    // osx_char([] (wchar_t c)             { ImGui::GetIO().AddInputCharacter(c); });

    // dynamic vertex- and index-buffers for ImGui-generated geometry
    sg_buffer_desc vbuf_desc = {
        .usage = SG_USAGE_STREAM,
        .size = sizeof(vertices)
    };
    draw_state.vertex_buffers[0] = sg_make_buffer(&vbuf_desc);
    sg_buffer_desc ibuf_desc = {
        .type = SG_BUFFERTYPE_INDEXBUFFER,
        .usage = SG_USAGE_STREAM,
        .size = sizeof(indices)
    };
    draw_state.index_buffer = sg_make_buffer(&ibuf_desc);

    // font texture for ImGui's default font
    unsigned char* font_pixels;
    int font_width, font_height;
    io.Fonts->GetTexDataAsRGBA32(&font_pixels, &font_width, &font_height);
    sg_image_desc img_desc = {
        .width = font_width,
        .height = font_height,
        .pixel_format = SG_PIXELFORMAT_RGBA8,
        .wrap_u = SG_WRAP_CLAMP_TO_EDGE,
        .wrap_v = SG_WRAP_CLAMP_TO_EDGE,
        .min_filter = SG_FILTER_LINEAR,
        .mag_filter = SG_FILTER_LINEAR,
        .content.subimage[0][0] = {
            .ptr = font_pixels,
            .size = font_width * font_height * 4
        }
    };
    draw_state.fs_images[0] = sg_make_image(&img_desc);

    // shader object for imgui renering
    sg_shader_desc shd_desc = {
        .vs.uniform_blocks[0].size = sizeof(vs_params_t),
        .vs.uniform_blocks[0].uniforms[0].name = "disp_size",
        .vs.uniform_blocks[0].uniforms[0].type = SG_UNIFORMTYPE_FLOAT2,
        .vs.source = vs_src_imgui,
        .fs.images[0].type = SG_IMAGETYPE_2D,
        .fs.images[0].name = "tex",
        .fs.source = fs_src_imgui,
    };
    sg_shader shd = sg_make_shader(&shd_desc);

    // pipeline object for imgui rendering
    sg_pipeline_desc pip_desc = {
        .layout = {
            .buffers[0].stride = sizeof(ImDrawVert),
            .attrs = {
                [0] = { .offset=IM_OFFSETOF(ImDrawVert, pos), .format=SG_VERTEXFORMAT_FLOAT2 },
                [1] = { .offset=IM_OFFSETOF(ImDrawVert, uv), .format=SG_VERTEXFORMAT_FLOAT2 },
                [2] = { .offset=IM_OFFSETOF(ImDrawVert, col), .format=SG_VERTEXFORMAT_UBYTE4N }
            }
        },
        .shader = shd,
        .index_type = SG_INDEXTYPE_UINT16,
        .blend = {
            .enabled = true,
            .src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA,
            .dst_factor_rgb = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
            .color_write_mask = SG_COLORMASK_RGB
        }
    };
    draw_state.pipeline = sg_make_pipeline(&pip_desc);
}

static void imgui_draw_cb(ImDrawData* draw_data) {
    if (draw_data->CmdListsCount == 0) {
        return;
    }

    // copy vertices and indices
    int num_vertices = 0;
    int num_indices = 0;
    int num_cmdlists = 0;
    for (; num_cmdlists < draw_data->CmdListsCount; num_cmdlists++) {
        const ImDrawList* cl = draw_data->CmdLists[num_cmdlists];
        const int cl_num_vertices = cl->VtxBuffer.size();
        const int cl_num_indices = cl->IdxBuffer.size();

        // overflow check
        if ((num_vertices + cl_num_vertices) > MaxVertices) {
            break;
        }
        if ((num_indices + cl_num_indices) > MaxIndices) {
            break;
        }

        // copy vertices
        memcpy(&vertices[num_vertices], &cl->VtxBuffer.front(), cl_num_vertices*sizeof(ImDrawVert));

        // copy indices, need to rebase to start of global vertex buffer
        const ImDrawIdx* src_index_ptr = &cl->IdxBuffer.front();
        const uint16_t base_vertex_index = num_vertices;
        for (int i = 0; i < cl_num_indices; i++) {
            indices[num_indices++] = src_index_ptr[i] + base_vertex_index;
        }
        num_vertices += cl_num_vertices;
    }

    // update vertex and index buffers
    const int vertex_data_size = num_vertices * sizeof(ImDrawVert);
    const int index_data_size = num_indices * sizeof(uint16_t);
    sg_update_buffer(draw_state.vertex_buffers[0], vertices, vertex_data_size);
    sg_update_buffer(draw_state.index_buffer, indices, index_data_size);

    // render the command list
    vs_params_t vs_params;
    vs_params.disp_size = ImGui::GetIO().DisplaySize;
    sg_apply_draw_state(&draw_state);
    sg_apply_uniform_block(SG_SHADERSTAGE_VS, 0, &vs_params, sizeof(vs_params));
    int base_element = 0;
    for (int cl_index = 0; cl_index < num_cmdlists; cl_index++) {
        const ImDrawList* cmd_list = draw_data->CmdLists[cl_index];
        //for (const ImDrawCmd& cmd : cmd_list->CmdBuffer) {
        for (size_t i = 0; i < cmd_list->CmdBuffer.size(); ++i) {
            const ImDrawCmd& cmd = cmd_list->CmdBuffer[i];
            if (cmd.UserCallback) {
                cmd.UserCallback(cmd_list, &cmd);
            }
            else {
                const int sx = (int) cmd.ClipRect.x;
                const int sy = (int) cmd.ClipRect.y;
                const int sw = (int) (cmd.ClipRect.z - cmd.ClipRect.x);
                const int sh = (int) (cmd.ClipRect.w - cmd.ClipRect.y);
                sg_apply_scissor_rect(sx, sy, sw, sh, true);
                sg_draw(base_element, cmd.ElemCount, 1);
            }
            base_element += cmd.ElemCount;
        }
    }
}

void imgui_teardown() {
    ImGui::DestroyContext();
}

#if defined(SOKOL_GLCORE33)
const char* vs_src_imgui =
    "#version 330\n"
    "uniform vec2 disp_size;\n"
    "in vec2 position;\n"
    "in vec2 texcoord0;\n"
    "in vec4 color0;\n"
    "out vec2 uv;\n"
    "out vec4 color;\n"
    "void main() {\n"
    "    gl_Position = vec4(((position/disp_size)-0.5)*vec2(2.0,-2.0), 0.5, 1.0);\n"
    "    uv = texcoord0;\n"
    "    color = color0;\n"
    "}\n";
const char* fs_src_imgui =
    "#version 330\n"
    "uniform sampler2D tex;\n"
    "in vec2 uv;\n"
    "in vec4 color;\n"
    "out vec4 frag_color;\n"
    "void main() {\n"
    "    frag_color = texture(tex, uv) * color;\n"
    "}\n";
#elif defined(SOKOL_GLES2)
const char* vs_src_imgui =
    "uniform vec2 disp_size;\n"
    "attribute vec2 position;\n"
    "attribute vec2 texcoord0;\n"
    "attribute vec4 color0;\n"
    "varying vec2 uv;\n"
    "varying vec4 color;\n"
    "void main() {\n"
    "    gl_Position = vec4(((position/disp_size)-0.5)*vec2(2.0,-2.0), 0.5, 1.0);\n"
    "    uv = texcoord0;\n"
    "    color = color0;\n"
    "}\n";
const char* fs_src_imgui =
    "precision mediump float;\n"
    "uniform sampler2D tex;\n"
    "varying vec2 uv;\n"
    "varying vec4 color;\n"
    "void main() {\n"
    "    gl_FragColor = texture2D(tex, uv) * color;\n"
    "}\n";
#elif defined(SOKOL_GLES3)
const char* vs_src_imgui =
    "#version 300 es\n"
    "uniform vec2 disp_size;\n"
    "in vec2 position;\n"
    "in vec2 texcoord0;\n"
    "in vec4 color0;\n"
    "out vec2 uv;\n"
    "out vec4 color;\n"
    "void main() {\n"
    "    gl_Position = vec4(((position/disp_size)-0.5)*vec2(2.0,-2.0), 0.5, 1.0);\n"
    "    uv = texcoord0;\n"
    "    color = color0;\n"
    "}\n";
const char* fs_src_imgui =
    "#version 300 es\n"
    "precision mediump float;"
    "uniform sampler2D tex;\n"
    "in vec2 uv;\n"
    "in vec4 color;\n"
    "out vec4 frag_color;\n"
    "void main() {\n"
    "    frag_color = texture(tex, uv) * color;\n"
    "}\n";
#elif defined(SOKOL_METAL)
const char* vs_src_imgui =
    "#include <metal_stdlib>\n"
    "using namespace metal;\n"
    "struct params_t {\n"
    "  float2 disp_size;\n"
    "};\n"
    "struct vs_in {\n"
    "  float2 pos [[attribute(0)]];\n"
    "  float2 uv [[attribute(1)]];\n"
    "  float4 color [[attribute(2)]];\n"
    "};\n"
    "struct vs_out {\n"
    "  float4 pos [[position]];\n"
    "  float2 uv;\n"
    "  float4 color;\n"
    "};\n"
    "vertex vs_out _main(vs_in in [[stage_in]], constant params_t& params [[buffer(0)]]) {\n"
    "  vs_out out;\n"
    "  out.pos = float4(((in.pos / params.disp_size)-0.5)*float2(2.0,-2.0), 0.5, 1.0);\n"
    "  out.uv = in.uv;\n"
    "  out.color = in.color;\n"
    "  return out;\n"
    "}\n";
const char* fs_src_imgui =
    "#include <metal_stdlib>\n"
    "using namespace metal;\n"
    "struct fs_in {\n"
    "  float2 uv;\n"
    "  float4 color;\n"
    "};\n"
    "fragment float4 _main(fs_in in [[stage_in]], texture2d<float> tex [[texture(0)]], sampler smp [[sampler(0)]]) {\n"
    "  return tex.sample(smp, in.uv) * in.color;\n"
    "}\n";
#elif defined(SOKOL_D3D11)
const char* vs_src_imgui =
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
const char* fs_src_imgui =
    "Texture2D<float4> tex: register(t0);\n"
    "sampler smp: register(s0);\n"
    "float4 main(float2 uv: TEXCOORD0, float4 color: COLOR0): SV_Target0 {\n"
    "  return tex.Sample(smp, uv) * color;\n"
    "}\n";
#endif
