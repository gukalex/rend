#pragma once
#include "rend.h"
#include "imgui/imgui.h"
#include <stdlib.h>
namespace demo_quads {
struct quad { v2 center, hsize; v4 c; v2 dir; } *q = nullptr;
int q_curr = 10000; float speed = 1.0;
bool draw = true; draw_data data = {}, data_c = {};
void init(rend& R) {
    if (!q) {
        R.textures[R.curr_tex++] = data.tex[0] = R.texture("pepe.png");
        R.progs[R.curr_progs++] = data.prog = R.shader(R.vs_quad, R.fs_quad_tex);
        R.progs[R.curr_progs++] = data_c.prog = R.shader(R.vs_quad, R.fs_quad);
        float x_ar = R.wh.x / (float)R.wh.y;
        q = (quad*)malloc(sizeof(quad) * R.qb.max_quads); // todo: cleanup
        for (int i = 0; i != R.qb.max_quads; i++) { // units coord system
            float size = 0.2f * RN();
            q[i].hsize = v2{ size,size} / 2.f;
            q[i].center = { RN() * x_ar, RNC(q[i].hsize.y) };
            q[i].c = { RN(), RN(), RN(), RN() };
            q[i].dir = { (RN() * 2 - 1) * 0.05f, (RN() * 2 - 1) * 0.07f};
        }
    }
}
void update(rend& R) {
    float x_ar = R.wh.x / (float)R.wh.y;
    data_c.p = data.p = ortho(0, x_ar, 0, 1);
    ImGui::SliderInt("quads", &q_curr, 0, R.qb.max_quads);
    ImGui::SliderFloat("speed", &speed, 0.f, 3.f);
    static float sizem = 0.1f;
    ImGui::SliderFloat("size", &sizem, 0.01f, 10.f);
    ImGui::Checkbox("draw", &draw);
    static bool update = true;
    ImGui::Checkbox("update", &update);
    static bool use_color = false;
    ImGui::Checkbox("use color", &use_color);
    R.clear({ 0.3f, 0.2f, 0.5f, 0.f });
    i64 update_start = tnow();
    if (update) {
        for (int i = 0; i < q_curr; i++) {
#define POS(v) ((v) < 0 ? -(v) : (v))
#define NEG(v) ((v) > 0 ? -(v) : (v))
            v2 res = q[i].center + q[i].dir * R.fd;
            bool hit = false;
            if (res.x < q[i].hsize.x * sizem) { q[i].dir.x = POS(q[i].dir.x); hit = true; }
            else if (res.x > x_ar - q[i].hsize.x * sizem) { q[i].dir.x = NEG(q[i].dir.x); hit = true; }
            else if (res.y < q[i].hsize.y * sizem) { q[i].dir.y = POS(q[i].dir.y); hit = true; }
            else if (res.y > 1 - q[i].hsize.y * sizem) { q[i].dir.y = NEG(q[i].dir.y); hit = true; }
            if (hit) q[i].c = { RN(), RN(), RN(), RN() };
            q[i].center = q[i].center + q[i].dir * R.fd * speed;
        }
    }
    static avg_data update_avg = avg_init(0.5);
    ImGui::Text("update time: %f ms", avg(sec(tnow() - update_start), &update_avg) * 1000.f);
    static bool freeze = false;
    i64 qf_start = tnow();
    if (draw && !freeze) {
        if (!use_color) {
            for (int i = 0; i != q_curr; i++) { // wtf textured is slightly faster?
                R.qb.quad_t(q[i].center - q[i].hsize * sizem, q[i].center + q[i].hsize * sizem, { 0,0 });
            }
        } else {
            for (int i = 0; i != q_curr; i++) {
                R.qb.quad(q[i].center - q[i].hsize * sizem, q[i].center + q[i].hsize * sizem, q[i].c);
            }
        }
    }
    static avg_data qf_avg = avg_init(0.5);
    ImGui::Text("quad fill time: %f ms", avg(sec(tnow() - qf_start), &qf_avg) * 1000.f);
    ImGui::Checkbox("freeze", &freeze);
    if (!freeze)
        R.submit_quads(&(use_color? data_c : data));
    else {
        R.submit(&(use_color? data_c : data), 1);
    }
}
}
