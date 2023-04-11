#pragma once
#include "rend.h"
#include "imgui/imgui.h"
#include "glad/glad.h"
namespace demo_quads {
const u32 WG_SIZE = 256; // + copy in the shader
float sizem = 0.015f;
float x_ar = 1;
constexpr u32 POS = 0, ATTR = 1;
u32 max_quads = 10000000;
vertex_buffer vb = {};
bool upd = true;
struct quad { v2 center, hsize; v2 dir, _pad; };
int q_curr = 100000; float speed = 1.0;
bool draw = true; draw_data data = {};
u32 compute = 0; u32 q_buf;
void init(rend& R) {
    if (!compute) {
        compute = R.shader(0, 0, R"(#version 450
        #define POS(v) ((v) < 0 ? -(v) : (v))
        #define NEG(v) ((v) > 0 ? -(v) : (v))
        #define WG_SIZE 256
        layout (local_size_x = WG_SIZE) in;
        struct quad { vec2 center, hsize; vec4 dir; };
        layout (binding = 0, std430) buffer q { quad Q[]; };
        layout (binding = 1, std430) writeonly buffer vert_pos { vec4 pos[]; };
        //layout (binding = 2, std430) writeonly buffer vert_attr { vec4 attr[]; };
        uniform float fd;
        uniform float x_ar;
        uniform float sizem;
        uniform float speed;
        void main() {
            uint local_id = gl_LocalInvocationID.x;
            uint group_id = gl_WorkGroupID.x;
            uint i = group_id * gl_WorkGroupSize.x + local_id;
            vec2 size = Q[i].hsize * sizem;
            if (true) { // update (unoptimized)
                vec2 res = Q[i].center + Q[i].dir.xy * fd;
                if (res.x < size.x) { Q[i].dir.x = POS(Q[i].dir.x); }
                else if (res.x > x_ar - size.x) { Q[i].dir.x = NEG(Q[i].dir.x); }
                else if (res.y < size.y) { Q[i].dir.y = POS(Q[i].dir.y); }
                else if (res.y > 1 - size.y) { Q[i].dir.y = NEG(Q[i].dir.y); }
                Q[i].center = Q[i].center + Q[i].dir.xy * fd * speed;
            }
            { // draw // todo: 16 bit positions
                uint qi = i * 2; // 4 vec2
                vec2 lb = Q[i].center - size;
                vec2 rt = Q[i].center + size;
                pos[qi + 0] = vec4(rt, rt.x, lb.y); // top right,  bottom right
                pos[qi + 1] = vec4(lb, lb.x, rt.y); // bottom left,  top left 
                
                // asuming we never change the texture coordinates
                //attr[qi + 0] = vec4(1, 1, 0, 0); //attr[qi + 1] = vec4(1, 0, 0, 0);
                //attr[qi + 2] = vec4(0, 0, 0, 0); //attr[qi + 3] = vec4(0, 1, 0, 0);
            }
        }
        )");
        R.textures[R.curr_tex++] = data.tex[0] = R.texture("pepe.png");
        R.progs[R.curr_progs++] = data.prog = R.shader(R"(
        #version 450 core
        layout (location = 0) in vec2 aPos;
        layout (location = 1) in vec4 aAttr;
        out vec4 vAttr;
        uniform mat4 rend_m, rend_v, rend_p;
        void main() {
           gl_Position = rend_p * vec4(aPos, 0.0, 1.0); // model and view currently unused
           vAttr = aAttr;
        })", 
        R"(#version 450 core
        in vec4 vAttr;
        out vec4 FragColor;
        uniform sampler2D rend_t0;
        void main() {
            vec2 uv = vAttr.xy;
            FragColor.rgba = texture(rend_t0, uv).rgba;
            if (FragColor.a < 0.1) discard; // alpha test
        })");
        float x_ar = R.wh.x / (float)R.wh.y;
        i64 istart = tnow();
        q_buf = R.ssbo(sizeof(quad) * max_quads, 0, true, 0 /*init with 0s*/);
        quad* q = (quad*)map(q_buf, 0, sizeof(quad) * max_quads, MAP_WRITE); defer{ unmap(q_buf); };
        vb = { {}, max_quads * 4, 2, {
            {0, AT_FLOAT, 2},
            {0, AT_FLOAT, 4}
        } };
        init_vertex_buffer(&vb);
        init_quad_indicies(vb.ib.index_buf, max_quads);
        v4* attr = (v4*)map(vb.ab[ATTR].id, 0, sizeof(v4) * max_quads * 4, MAP_WRITE);
        u32 curr_quad_count = 0;
        for (u32 i = 0; i < max_quads; i++) { // units coord system
            float size = 0.2f * RN();
            v2 hsize = v2{ size,size } / 2.f;
            q[i].hsize = hsize;
            q[i].center = { RN() * x_ar, RNC(hsize) };
            q[i].dir = { RNC(-1.f,1.f) * 0.05f, RNC(-1.f, 1.f) * 0.07f};
            v4 base_attr[4] = {{1, 1, 0, 0},
                                {1, 0, 0, 0},
                                {0, 0, 0, 0},
                                {0, 1, 0, 0} };
            memcpy((u8*)attr + curr_quad_count * sizeof(v4) * 4, base_attr, sizeof(v4) * 4);
            curr_quad_count++;
        }
        unmap(vb.ab[ATTR].id);
        data.ib = vb.ib;
        print("init time :%f", sec(tnow()-istart));
    }
}
void update(rend& R) {
    x_ar = R.wh.x / (float)R.wh.y;
    data.p = ortho(0, x_ar, 0, 1);
    ImGui::SliderInt("quads", &q_curr, 0, max_quads - WG_SIZE);
    q_curr = ALIGN_UP(q_curr, WG_SIZE);
    ImGui::SliderFloat("speed", &speed, 0.f, 3.f);
    ImGui::SliderFloat("size", &sizem, 0.001f, 0.5f);
    ImGui::Checkbox("draw", &draw);
    ImGui::Checkbox("update", &upd);
    R.clear({ 0.3f, 0.2f, 0.5f, 0.f });
    i64 update_start = tnow();
    if (upd) {
        int group_size = q_curr / WG_SIZE;
        u32 pos_buf = vb.ab[POS].id;
        u32 attr_buf = 0; // vb.ab[ATTR].id; // not used currently
        R.dispatch({ compute, (u32)group_size, 1, 1, {q_buf, pos_buf, attr_buf}, {
            {"fd",    UNIFORM_FLOAT, &R.fd},
            {"x_ar",  UNIFORM_FLOAT, &x_ar},
            {"sizem", UNIFORM_FLOAT, &sizem},
            {"speed", UNIFORM_FLOAT, &speed}
        }});
        R.ssbo_barrier(); // wait for the compute shader to finish - todo: double buffer?
    }
    static avg_data update_avg = avg_init(0.5);
    ImGui::Text("update time: %f ms", avg(sec(tnow() - update_start), &update_avg) * 1000.f);
    i64 qf_start = tnow();
    if (draw) {
        data.ib.vertex_count = q_curr * 6;
        R.submit(&data, 1);
    }
}
}
