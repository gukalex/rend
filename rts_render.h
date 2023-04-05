#pragma once
#include "rend.h"
#include "demo_rts.h"
#include "imgui/imgui.h"

/*
example usage:
rend R = {}; R.init(); draw_data dd = init_rts_dd(R);
while (!R.closed()) {
    current_state state_to_render = ... // state from http request
    tazer_ef* tz_ef = ... // tazer effects from http request
    R.clear(C::NICE_DARK);
    draw_rts(R, state_to_render, tz_ef);
    R.submit(dd);
    R.present();
}
R.cleanup();
*/

namespace rts {

const char* fs_rst_uber = R"(#version 450 core
in vec4 vAttr;
out vec4 FragColor;
uniform sampler2D rend_t0;
uniform sampler2D rend_t1;
uniform sampler2D rend_t2;
uniform sampler2D rend_t3;
uniform sampler2D rend_t4;
uniform sampler2D rend_t5;
void main() {
    vec2 uv = vAttr.xy;
    vec4 alpha = vec4(1,1,1,vAttr.z);
    if (vAttr.w == 0) FragColor.rgba = texture(rend_t0, uv) * alpha;
    else if (vAttr.w == 1) FragColor.rgba = texture(rend_t1, uv) * alpha;
    else if (vAttr.w == 2) FragColor.rgba = texture(rend_t2, uv) * alpha;
    else if (vAttr.w == 3) FragColor.rgba = texture(rend_t3, uv) * alpha;
    else if (vAttr.w == 4) FragColor.rgba = texture(rend_t4, uv) * alpha;
    else if (vAttr.w == 5) {
        FragColor.rgba = vec4(vAttr.xyz, 0.25 );
    } else if (vAttr.w == 6) {
        int index = int(vAttr.z);
        vec4 colors[4] = { vec4(0, 0, 1, 1), vec4(1, 0.5, 0, 1), vec4(1,0,0,1), vec4(1,1,0,1)};
        FragColor.rgba = texture(rend_t5, uv) * colors[index];
    } else if (vAttr.w == 7) {
        FragColor.rgba = vec4(vAttr.xyz, 0.6);
    }
    else
        FragColor.rgba = vec4(1, 0, 0, 1); // debug
})";

draw_data init_rts_dd(rend &R) {
    draw_data dd;
    dd.p = ortho(0, ARENA_SIZE, 0, ARENA_SIZE);
    const char* textures[] = { "amogus.png", "din.jpg", "pool.png", "pepe.png", "coffee.png", "portal1.png" };
    for (int i = 0; i < ARSIZE(textures); i++) R.textures[R.curr_tex++] = dd.tex[i] = R.texture(textures[i]);
    R.progs[R.curr_progs++] = dd.prog = R.shader(R.vs_quad, fs_rst_uber);
    return dd;
}

void draw_rts(rend &R, current_state &cs, tazer_ef* tz_ef) {
    constexpr f32 SHADER_QUAD = 5.0f;
    constexpr f32 SHADER_COFF = 4.0f;
    constexpr f32 SHADER_PORT = 6.0f;
    constexpr f32 SHADER_TAZER = 7.0f;
    v4 spawn_color[MAX_TEAMS] = {
        {1, 0, 0, SHADER_QUAD}, {0, 1, 0, SHADER_QUAD},
        {1, 0, 1, SHADER_QUAD}, {0, 1, 1, SHADER_QUAD},
    };
    const char* team_names[MAX_TEAMS] = {"Amogus  ", "Stefan  ", "Torbjorn", "Pepe    "};
    FOR_SPAWN(i) {
        u8 team_id = cs.info[i].team_id;
        ImGui::Text("%s : %llu", team_names[team_id], cs.score[team_id]);
    }
    FOR_OBJ(i) {
        switch (cs.info[i].type) {
        case OBJ_UNIT: {
            f32 alpha_level = (cs.info[i].st == OBJ_STATE_UNIT_SLEEPING ? 0.2f : 1.0f);
            R.qb.quad_t(cs.info[i].pos - UNIT_SIZE / 2.f, cs.info[i].pos + UNIT_SIZE / 2.f, { alpha_level, (f32)cs.info[i].team_id });
        } break;
        case OBJ_COFF: {
            f32 size = clamp(cs.info[i].energy / MAX_COFF_ENERGY, 0.1f, 1.0f);
            f32 alpha_level = (cs.info[i].st == OBJ_STATE_COFF_TAKEN ? 0.2f : 1.0f);
            R.qb.quad_t(cs.info[i].pos - MAX_COFF_SIZE * size / 2.f, cs.info[i].pos + MAX_COFF_SIZE * size / 2.f, { alpha_level, SHADER_COFF });
        } break;
        case OBJ_SPAWN:
            R.qb.quad(cs.info[i].pos - SPAWN_SIZE / 2.f, cs.info[i].pos + SPAWN_SIZE / 2.f, spawn_color[cs.info[i].team_id]);
            break;
        case OBJ_PORTAL: {
            f32 color_index = (f32)(i - PORTAL_0);
            R.qb.quad_t(cs.info[i].pos - PORTAL_SIZE / 2.f, cs.info[i].pos + PORTAL_SIZE / 2.f, {color_index, SHADER_PORT});
        } break;
        default: break;
        }
    }
    static f32 tsize = 0.3f;
    static int nquads = 10;
    ImGui::SliderFloat("Tazer Size", &tsize, 0.1f, UNIT_SIZE);
    ImGui::SliderInt("Tazer Quads", &nquads, 1, 100);
    for (int i = 0; i < MAX_TAZER_EF; i++) {
        if (tz_ef[i].life > 0) {
            for (int j = 0; j < nquads; j++) {
                f32 offset = RN();
                v2 pos = cs.info[tz_ef[i].source_id].pos + (cs.info[tz_ef[i].target_id].pos - cs.info[tz_ef[i].source_id].pos) * offset;
                R.qb.quad(pos - tsize / 2.f, pos + tsize / 2.f, { RN(),RN(),1, SHADER_TAZER});
            }
        }
    }
}

}