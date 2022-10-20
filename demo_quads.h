// todo: hang/slow fps OOB glitch
#pragma once
#include "rend.h"
#include "imgui/imgui.h"
#include <stdlib.h>
namespace demo_quads {
struct quad { v2 center, hsize; v4 c; v2 dir; } *q = nullptr;
int q_curr = 100'000; float speed = 1.0;
bool draw = true; draw_data data = {0, 0};
void init(rend& R) {
    if (!q) {
        R.textures[R.curr_tex++] = data.tex = R.texture("pepe.png");
        R.progs[R.curr_progs++] = data.prog = R.shader(R.vs_quad, R.fs_quad_tex);

        q = (quad*)malloc(sizeof(quad) * R.max_quads); // todo: cleanup
        for (int i = 0; i != R.max_quads; i++) {
            q[i].hsize = v2{ 0.03f * RN(), 0.03f * RN()} / 2.f;
            q[i].center = { RNC(q[i].hsize.x), RNC(q[i].hsize.y) };
            q[i].c = { RN(), RN(), RN(), 0 };
            q[i].dir = { (RN() * 2 - 1) * 0.05f, (RN() * 2 - 1) * 0.07f};
        }
    }
}
void update(rend& R) {
    ImGui::SliderInt("quads", &q_curr, 0, R.max_quads);
    ImGui::SliderFloat("speed", &speed, 0.f, 3.f);
    static float sizem = 1.0f;
    ImGui::SliderFloat("size", &sizem, 0.01f, 10.f);
    ImGui::Checkbox("draw", &draw);
    R.clear({ 0.3f, 0.2f, 0.5f, 0.f });
    i64 update_start = tnow();
    for (int i = 0; i < q_curr; i++) {
        v2 res = q[i].center + q[i].dir * R.fd;
        bool hit = false;
        if (res.x < q[i].hsize.x * sizem || res.x > 1.f - q[i].hsize.x * sizem) { q[i].dir.x = -q[i].dir.x; hit = true; }
        if (res.y < q[i].hsize.y * sizem || res.y > 1.f - q[i].hsize.y * sizem) { q[i].dir.y = -q[i].dir.y; hit = true; }
        if (hit) q[i].c = { RN(), RN(), RN(), 0 };
        q[i].center = q[i].center + q[i].dir * R.fd * speed;
    }
    static avg_data update_avg = avg_init(0.5);
    ImGui::Text("update time: %f ms", avg(sec(tnow() - update_start), &update_avg) * 1000.f);
    if (draw) {
        for (int i = 0; i != q_curr; i++) {
            //R.quad_t(q[i].center - q[i].hsize * sizem, q[i].center + q[i].hsize * sizem, {0,0});
            v4 atr[4] = { {1,1,0,0}, {1,0,0,0}, {0,0,0,0}, {0,1,0,0} };
            if (q[i].dir.x < 0) for (int i = 0; i < 4; i++) atr[i].x = 1 - atr[i].x;
            if (q[i].dir.y < 0) for (int i = 0; i < 4; i++) atr[i].y = 1 - atr[i].y;
            R.quad_a(q[i].center - q[i].hsize * sizem, q[i].center + q[i].hsize * sizem, atr);
        }
    }
    R.submit(data);
}
}
