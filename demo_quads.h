#pragma once
#include "rend.h"
#include "imgui/imgui.h"
#include "glad/glad.h"
namespace demo_quads {
const u32 WG_SIZE = 256; // + copy in the shader
const u32 AUX_DIM = 100; // + copy in the shader
float sizem = 0.055f;
float x_ar = 1;
constexpr u32 POS = 0, ATTR = 1;
u32 max_quads = 1000000;
vertex_buffer vb = {}, vb_aux = {};
bool upd = true;
struct quad { v2 center, hsize; v2 dir, _pad; };
struct aux { iv2 test, counter; };
int q_curr = 100000; float speed = 1.0;
bool draw = true; draw_data data = {};
u32 compute = 0; u32 q_buf; u32 aux_buf; u32 aux_compute = 0;
// WIP, IDK what is going on by now, something random and unnecessary 💩
char cs_src[1024 * 1024] =  R"(#version 450
#define POS(v) ((v) < 0 ? -(v) : (v))
#define NEG(v) ((v) > 0 ? -(v) : (v))
#define WG_SIZE 256
// todo: no issue being updated dynamically
#define AUX_DIM 100
layout (local_size_x = WG_SIZE) in;
struct aux { uvec4 counter; };
struct quad { vec2 center, hsize; vec4 dir; };
layout (binding = 0, std430) buffer q { quad Q[]; };
layout (binding = 1, std430) writeonly buffer vert_pos { vec4 pos[]; };
layout (binding = 2, std430) /*writeonly*/ buffer vert_attr { vec4 attr[]; };
layout (binding = 3, std430) buffer aux_buf { aux A[]; }; // 100x100
uniform float fd;
uniform float x_ar;
uniform float sizem;
uniform float speed;
uniform uint curframe;
uniform vec4 mp; // x,y, z - pressed, w - size
uniform vec4 test; // dump, move_mode, dump_enable
void main() {
    uint local_id = gl_LocalInvocationID.x;
    uint group_id = gl_WorkGroupID.x;
    uint i = group_id * gl_WorkGroupSize.x + local_id;


    // get aux index positioning on a aux grid
    uint ax = uint(Q[i].center.x * AUX_DIM); // wrong
    uint ay = uint(Q[i].center.y * AUX_DIM); // wrong
    uint ai = ay * AUX_DIM + ax; // wrong
    //A[ai].counter.x += 1; // todo: do stuff (probably need second compute pass)

    //memoryBarrierBuffer();??
    /*if (A[ai].counter.y != curframe) { // some shit going on here
        A[ai].counter.z = A[ai].counter.x;
        A[ai].counter.x = 0;
        A[ai].counter.y = curframe;
        A[ai].counter.w = 42069;
        //...
    }*/
    //memoryBarrierBuffer();
//	atomicAdd(A[ai].counter.x, 1);
 //   memoryBarrierBuffer(); // ? do i need one here so I can access up-to-date stuff later? or it's atomic and fine?

    Q[i].hsize = clamp(abs(Q[i].hsize) * test.x, 0.03, 1.);

    vec2 size = Q[i].hsize * sizem;
    { // update (unoptimized)
        vec2 res = Q[i].center + Q[i].dir.xy * fd;
        if (res.x < size.x) { Q[i].dir.x = POS(Q[i].dir.x); }
        else if (res.x > x_ar - size.x) { Q[i].dir.x = NEG(Q[i].dir.x); }
        else if (res.y < size.y) { Q[i].dir.y = POS(Q[i].dir.y); }
        else if (res.y > 1 - size.y) { Q[i].dir.y = NEG(Q[i].dir.y); }
        Q[i].center = Q[i].center + Q[i].dir.xy * fd * speed;
        if (test.z > 0) // velocity damping enabled
            Q[i].dir = Q[i].dir * test.x; 
        if (mp.z > 0.) { // if mouse is pressed
            vec2 dif = Q[i].center - mp.xy;
            vec2 dir = normalize(dif);
            float v = 0.000001 / dot(dif, dif);
            if (test.y > 0 && length(Q[i].dir.xy) < 1.) { // velocity mode todo: unconditional 
                vec2 addition = dir * v * mp.w;
                if (dot(addition, addition) < 1.0)
                    Q[i].dir.xy += addition.yx*vec2(1,-1);
            }
             else { // velocity mode disabled
                vec2 addition = dir * v * mp.w; 
                Q[i].center += addition;
                if (dot(dif, dif) < 0.02) {
                    uint qi = i * 4; // 4 vec4
                    float a = mix(0.9, 0.0, dot(dif,dif) * 100);
                    // asuming we never change the texture coordinates
                    attr[qi + 0] = vec4(1, 1, a, 0); attr[qi + 1] = vec4(1, 0, a, 0);
                    attr[qi + 2] = vec4(0, 0, a, 0); attr[qi + 3] = vec4(0, 1, a, 0);
					Q[i].hsize = a* vec2(0.3);
                }
            }
        }
    }
    { // draw // todo: 16 bit positions
        uint qi = i * 2; // 4 vec2
        vec2 lb = Q[i].center - size;
        vec2 rt = Q[i].center + size;
        pos[qi + 0] = vec4(rt, rt.x, lb.y); // top right,  bottom right
        pos[qi + 1] = vec4(lb, lb.x, rt.y); // bottom left,  top left 
    }
    {
        uint qi = i * 4; // 4 vec4
        float a = clamp(attr[qi + 0].z + 0.001, 0.1, 1.0);
        // asuming we never change the texture coordinates
        attr[qi + 0] = vec4(1, 1, a, 0); attr[qi + 1] = vec4(1, 0, a, 0);
        attr[qi + 2] = vec4(0, 0, a, 0); attr[qi + 3] = vec4(0, 1, a, 0);
    }
}
)";
char cs_aux_src[1024 * 1024] =  R"(#version 450
// todo: different WG_SIZE
#define AUX_WG_SIZE 1
#define AUX_DIM 100
layout (local_size_x = AUX_WG_SIZE) in; // todo: x and y
struct aux { uvec4 counter; };
layout (binding = 1, std430) writeonly buffer vert_pos { vec4 pos[]; };
layout (binding = 2, std430) /*writeonly*/ buffer vert_attr { vec4 attr[]; };
layout (binding = 3, std430) buffer aux_buf { aux A[]; }; // 100x100
void main() {
    uint local_id = gl_LocalInvocationID.x;
    uint group_id = gl_WorkGroupID.x;
    uint i = group_id * gl_WorkGroupSize.x + local_id;
    uint x = i % 100; // sloooowww, use local_size x y
    uint y = i / 100; // slooowww
    { // draw
        vec2 size = vec2(0.01, 0.01);
        vec2 center = vec2(x / 100.f, y / 100.f);
        uint qi = i * 2; // 4 vec2
        vec2 lb = center; ////// wrong
        vec2 rt = center + size;//////// wrong
        pos[qi + 0] = vec4(rt, rt.x, lb.y); // top right,  bottom right
        pos[qi + 1] = vec4(lb, lb.x, rt.y); // bottom left,  top left 
    }
    {
        uint qi = i * 4; // 4 vec4
        float a = A[i].counter.z * 0.1f;
        attr[qi + 0] = vec4(1, 1, a, 0); attr[qi + 1] = vec4(1, 0, a, 0);
        attr[qi + 2] = vec4(0, 0, a, 0); attr[qi + 3] = vec4(0, 1, a, 0);
    }
}
)";
char vs_src[1024 * 64] = R"(
#version 450 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec4 aAttr;
out vec4 vAttr;
uniform mat4 rend_m, rend_v, rend_p;
void main() {
    gl_Position = rend_p * vec4(aPos, 0.0, 1.0); // model and view currently unused
    vAttr = aAttr;
})"; 
char fs_src[1024 * 64] = R"(#version 450 core
in vec4 vAttr;
out vec4 FragColor;
uniform sampler2D rend_t0;
uniform sampler2D rend_t1;
void main() {
    vec2 uv = vAttr.xy;
    vec4 color = texture(rend_t0, uv).rgba;
    vec4 color1 = texture(rend_t1, uv).rgba;
    //FragColor = mix(color.gbra, color, vAttr.z);
    FragColor = mix(color, color1, vAttr.z);
    //FragColor.rgba = texture(rend_t0, uv).rgba * vec4(1,1,1,vAttr.z);
    //if (FragColor.a < 0.1) discard; // alpha test
})";
void init(rend& R) {
    if (!compute) {
        compute = R.shader(0, 0, cs_src);
        aux_compute = R.shader(0,0, cs_aux_src);
        R.textures[R.curr_tex++] = data.tex[0] = R.texture("amogus.png");//R.texture("pepe.png");
        R.textures[R.curr_tex++] = data.tex[1] = R.texture("pepe.png");//R.texture("pepe.png");
        R.progs[R.curr_progs++] = data.prog = R.shader(vs_src, fs_src);
        float x_ar = R.wh.x / (float)R.wh.y;
        i64 istart = tnow();
        aux_buf = R.ssbo(AUX_DIM*AUX_DIM *sizeof(aux), 0, true, 0 /* init with 0s */);
        q_buf = R.ssbo(sizeof(quad) * max_quads, 0, true, 0 /*init with 0s*/);
        quad* q = (quad*)map(q_buf, 0, sizeof(quad) * max_quads, MAP_WRITE); defer{ unmap(q_buf); };
        vb = { {}, max_quads * 4, 2, {
            {0, AT_FLOAT, 2},
            {0, AT_FLOAT, 4}
        } };
        init_vertex_buffer(&vb);
        init_quad_indicies(vb.ib.index_buf, max_quads);

        vb_aux = { {}, AUX_DIM * AUX_DIM * 4 /*quad*/, 2, {
            {0, AT_FLOAT, 2},
            {0, AT_FLOAT, 4}
        } };
        init_vertex_buffer(&vb_aux);
        init_quad_indicies(vb_aux.ib.index_buf, AUX_DIM * AUX_DIM);

        v4* attr = (v4*)map(vb.ab[ATTR].id, 0, sizeof(v4) * max_quads * 4, MAP_WRITE);
        u32 curr_quad_count = 0;
        for (u32 i = 0; i < max_quads; i++) { // units coord system
            float size = 0.2f * RN();
            v2 hsize = v2{ size,size } / 2.f;
            q[i].hsize = hsize;
            q[i].center = { RN() * x_ar, RNC(hsize) };
            q[i].dir = { RNC(-1.f,1.f) * 0.05f, RNC(-1.f, 1.f) * 0.07f};
            v4 base_attr[4] = {{1, 1, 0.3f, 0},
                                {1, 0, 0.3f, 0},
                                {0, 0, 0.3f, 0},
                                {0, 1, 0.3f, 0} };
            memcpy((u8*)attr + curr_quad_count * sizeof(v4) * 4, base_attr, sizeof(v4) * 4);
            curr_quad_count++;
        }
        unmap(vb.ab[ATTR].id);
        data.ib = vb.ib;
        print("init time :%f", sec(tnow()-istart));
    }
}
void update(rend& R) {
    static bool mon = false;
    static float brush_size = 1.0;
    static float damp = 0.999f;
    static bool damp_enabled = false;
    static bool velocity_mode = false;
    if (R.key_pressed('F', KT::IGNORE_IMGUI))
        mon = !mon;
    
    x_ar = R.wh.x / (float)R.wh.y;
    data.p = ortho(0, x_ar, 0, 1);

    // todo: clear resources
    if (ImGui::TreeNode("Shader VSFS")) {
		ImGuiInputTextFlags flags = ImGuiInputTextFlags_AllowTabInput;
		ImGui::InputTextMultiline("##source1", fs_src, IM_ARRAYSIZE(fs_src), ImVec2(-FLT_MIN, ImGui::GetTextLineHeight() * 16), flags);
        ImGui::InputTextMultiline("##source2", vs_src, IM_ARRAYSIZE(vs_src), ImVec2(-FLT_MIN, ImGui::GetTextLineHeight() * 16), flags);
		if (ImGui::Button("Compiler go brr")) {
			data.prog = R.shader(vs_src, fs_src);
		}
		ImGui::TreePop();
	}

     if (ImGui::TreeNode("Shader Compute")) {
		ImGuiInputTextFlags flags = ImGuiInputTextFlags_AllowTabInput;
		ImGui::InputTextMultiline("##source3", cs_src, IM_ARRAYSIZE(cs_src), ImVec2(-FLT_MIN, ImGui::GetTextLineHeight() * 32), flags);
		if (ImGui::Button("Compiler go brr")) {
			compute = R.shader(0, 0, cs_src);
		}
		ImGui::TreePop();
	}

    ImGui::SliderInt("quads", &q_curr, 0, max_quads - WG_SIZE);
    q_curr = ALIGN_UP(q_curr, WG_SIZE);
    ImGui::SliderFloat("speed", &speed, 0.f, 3.f);
    ImGui::SliderFloat("size", &sizem, 0.001f, 0.5f);
    ImGui::Checkbox("draw", &draw);
    ImGui::Checkbox("update", &upd);
    ImGui::Checkbox("mouse always on [F]", &mon);
    ImGui::Checkbox("speed damp enabled", &damp_enabled);
    ImGui::Checkbox("velocity mode enabled", &velocity_mode);
    ImGui::SliderFloat("dampening", &damp, 0.995f, 1, "%.6f");
    ImGui::SliderFloat("brush size", &brush_size, -5, 5);
    R.clear({ 0.3f, 0.2f, 0.5f, 0.f });
    i64 update_start = tnow();
    if (upd) {
        int group_size = q_curr / WG_SIZE;
        u32 pos_buf = vb.ab[POS].id;
        u32 attr_buf = vb.ab[ATTR].id; // errors? not used currently
        v2 mouse_norm = R.mouse_norm();
        bool mouse_on = mon || R.key_pressed(KT::MBL) || R.key_pressed(KT::MBR);
        v4 mp = {mouse_norm.x * x_ar, mouse_norm.y, mouse_on ? 1.f : 0.f, R.key_pressed(KT::MBR) ? -brush_size : brush_size };
        v4 test = {damp, (f32)velocity_mode, (f32)damp_enabled};
        static u32 curframe = 0;
        curframe++;
        R.dispatch({ compute, (u32)group_size, 1, 1, {q_buf, pos_buf, attr_buf, aux_buf}, {
            {"fd",    UNIFORM_FLOAT, &R.fd},
            {"x_ar",  UNIFORM_FLOAT, &x_ar},
            {"sizem", UNIFORM_FLOAT, &sizem},
            {"speed", UNIFORM_FLOAT, &speed},
            {"mp", UNIFORM_VEC4, &mp},
            {"test", UNIFORM_VEC4, &test},
            {"curframe", UNIFORM_UINT, &curframe}
        }});
        R.ssbo_barrier(); // wait for the compute shader to finish - todo: double buffer?
        u32 aux_pos_buf = vb_aux.ab[POS].id;
        u32 aux_attr_buf = vb_aux.ab[ATTR].id; // errors? not used currently
        //R.dispatch({ aux_compute, AUX_DIM * AUX_DIM, 1, 1, { aux_pos_buf, aux_attr_buf, aux_buf} });
        //R.ssbo_barrier();
    }
    static avg_data update_avg = avg_init(0.5);
    ImGui::Text("update time: %f ms", avg(sec(tnow() - update_start), &update_avg) * 1000.f);
    i64 qf_start = tnow();
    if (draw) {
        data.ib = vb.ib;
        data.ib.vertex_count = q_curr * 6;
        R.submit(&data, 1);

        // temp
        //data.ib = vb_aux.ib;
        //data.ib.vertex_count = AUX_DIM * AUX_DIM * 6;
        //R.submit(&data, 1);
    }
}
}
