#pragma once
#include "rend.h"
#include "imgui/imgui.h"
#include <stdlib.h>
namespace demo_quads {
struct quad { v2 center, hsize; v4 c; v2 dir; } *q = nullptr;
int q_curr = 100'000; float speed = 1.0;
bool draw = true;
void init(rend& R) {
    if (!q) {
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
    ImGui::Checkbox("draw", &draw);
    R.clear({ 0.3f, 0.2f, 0.5f, 0.f });
    i64 update_start = tnow();
    for (int i = 0; i < q_curr; i++) {
        v2 res = q[i].center + q[i].dir * R.fd;
        bool hit = false;
        if (res.x < q[i].hsize.x || res.x > 1.f - q[i].hsize.x) { q[i].dir.x = -q[i].dir.x; hit = true; }
        if (res.y < q[i].hsize.y || res.y > 1.f - q[i].hsize.y) { q[i].dir.y = -q[i].dir.y; hit = true; }
        if (hit) q[i].c = { RN(), RN(), RN(), 0 };
        q[i].center = q[i].center + q[i].dir * R.fd * speed;
    }
    static avg_data update_avg = avg_init(0.5);
    ImGui::Text("update time: %f ms", avg(sec(tnow() - update_start), &update_avg) * 1000.f);
    if (draw) {
        for (int i = 0; i != q_curr; i++)
            R.quad(q[i].center - q[i].hsize, q[i].center + q[i].hsize, q[i].c);
    }
}
}
