#pragma once
#include "rend.h"
#include "imgui/imgui.h"
#include <math.h>
#include <GLFW/glfw3.h>
namespace demo_qube {
i64 tstart;
draw_data dd[3] = {};
//quad_batcher quad_mesh = {};
indexed_buffer static_ib[3] = {};
f32 angle = 0, angle_b = PI/2, cam_speed = 100;
v4 cam_pos = {0,5,50}, cam_fwd = {0, 0, -1}, cam_up = {0,1,0};
bool use_orth = false, rotate = false, fps_look = false;
bool last_ctrl = false; // if ctrl was pressed
v2 prev_xy = {}; // last mouse pos
f32 mouse_sens = 0.001f; // radian per pixel?
f32 yaw = -PI/2, pitch = 0.f;
m4 mvp[3] = {};

struct mapped_mesh { v2* pos; u32* col; u32 curr_q; };
void push_quad(mapped_mesh* m, v2 lb, v2 rt, u32* c4) {
    float vertices[] = {
            rt.x, rt.y, // top right
            rt.x, lb.y,  // bottom right
            lb.x, lb.y,  // bottom left
            lb.x, rt.y  // top left 
    };
    memcpy((u8*)m->pos + m->curr_q * sizeof(v2) * 4, vertices, sizeof(vertices));
    memcpy((u8*)m->col + m->curr_q * sizeof(u32) * 4, c4, sizeof(u32) * 4);
    m->curr_q++;
}
indexed_buffer next_ib(indexed_buffer ib, u32* prev_q, u32 curr_q) {
    u32 offset = (*prev_q) * 6 * sizeof(int);
    indexed_buffer new_ib = { ib.id, offset, (curr_q - (*prev_q)) * 6, ib.index_buf };
    *prev_q = curr_q;
    return new_ib;
}

void init(rend& R) {
    tstart = tnow();
    if (!dd[0].prog) {
        u32 prog = R.shader(R"(#version 450 core
        layout (location = 0) in vec2 aPos;
        layout (location = 1) in vec4 aAttr;
        out vec4 vAttr;
        uniform mat4 mvp;
        void main() {
           gl_Position = mvp * vec4(aPos, 0.0, 1.0);
           vAttr = aAttr;
        })", R"(#version 450 core
        in vec4 vAttr;
        out vec4 FragColor;
        uniform sampler2D rend_t0;
        void main() {
            FragColor.rgba = vAttr;//texture(rend_t0, uv).rgba;
            //if (FragColor.a < 0.1) discard; // alpha test
        })");
        u32 tex = R.texture("pepe.png");

        u32 mesh_quads = 4;
        vertex_buffer vb = { {}, mesh_quads * 4, 2, {
            {0, AT_FLOAT, 2}, // todo: 3
            {0, AT_UBYTE, 4, true} // color (u32)
        } };
        init_vertex_buffer(&vb);
        init_quad_indicies(vb.ib.index_buf, mesh_quads);
        mapped_mesh m = {};
        m.pos = (v2*)map(vb.ab[0].id, 0, sizeof(v2) * mesh_quads * 4, MAP_WRITE); defer{ unmap(vb.ab[0].id); };
        m.col = (u32*)map(vb.ab[1].id, 0, sizeof(u32) * mesh_quads * 4, MAP_WRITE); defer{ unmap(vb.ab[1].id); };
        u32 prev_q = 0;
        u32 c[] = { 0xFF00FFFF,0xFF0000FF, 0xFF00FF00, 0xFFFF0000};
        push_quad(&m, { 0,0 }, { 10, 10 }, c);
        push_quad(&m, { 20,0 }, { 30, 10 }, c);
        static_ib[0] = next_ib(vb.ib, &prev_q, m.curr_q);
        push_quad(&m, { 30,0 }, { 40, 10 }, c);
        static_ib[1] = next_ib(vb.ib, &prev_q, m.curr_q);
        push_quad(&m, { 0,0 }, { 100,100 }, c);
        static_ib[2] = next_ib(vb.ib, &prev_q, m.curr_q);

        for (int i = 0; i < ARSIZE(dd); i++) { 
            dd[i].prog = prog; dd[i].tex[0] = tex; 
            dd[i].ib = static_ib[i];
            dd[i].uni[0] = { "mvp", UNIFORM_MAT4, &mvp[i] };
        }
    }
}
m4 rot_mat(f32 a) { return {
    cosf(a), -sinf(a), 0, 0,
    sinf(a), cosf(a), 0, 0,
    0,0,1,0,
    0,0,0,1
};}
m4 rot_x_mat(f32 a) { return {
    1,0,0,0,
    0, cosf(a), -sinf(a), 0,
    0, sinf(a), cosf(a), 0,
    0,0,0,1
};}
m4 translate(v4 pos /*v3*/) { return {
    1,0,0,pos.x,
    0,1,0,pos.y,
    0,0,1,pos.z,
    0,0,0,1
};}
void update(rend& R) { using namespace ImGui;
    iv2 mp = R.mouse();
    Text("mp: %d %d", mp.x, mp.y);
    SliderFloat("Mouse Sensetiviy", &mouse_sens, 0.0001f, 0.01f, "%.4f");
    SliderFloat("angle", &angle, -6.28f, 6.28f);
    SliderFloat("angle_b", &angle_b, -6.28f, 6.28f);
    SliderFloat3("cam pos", &cam_pos.x, -200.f, 200.f);
    Checkbox("Rotate", &rotate);
    Checkbox("Ortho", &use_orth);
    if (Checkbox("FPS Look", &fps_look)) {
        glfwSetInputMode(R.window, GLFW_CURSOR, fps_look ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
    }
    bool ctrl = R.key_pressed(KT::CTRL, KT::NONE); defer{ last_ctrl = ctrl; };
    if (ctrl != last_ctrl) {
        fps_look = !fps_look;
        glfwSetInputMode(R.window, GLFW_CURSOR, fps_look ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
    }

    f32 speed = R.key_pressed(KT::SHIFT, 0) ? cam_speed * 5.f : cam_speed;
    if (R.key_pressed('W', 0)) cam_pos += cam_fwd * speed * R.fd;
    if (R.key_pressed('S', 0)) cam_pos -= cam_fwd * speed * R.fd;
    if (R.key_pressed('A', 0)) cam_pos -= norm(cross(cam_fwd, cam_up)) * speed * R.fd;
    if (R.key_pressed('D', 0)) cam_pos += norm(cross(cam_fwd, cam_up)) * speed * R.fd;
    if (R.key_pressed('Q', 0)) cam_pos.y -= speed * R.fd;
    if (R.key_pressed('E', 0)) cam_pos.y += speed * R.fd;

    v2 cur_xy = to_v2(R.mouse()); defer{ prev_xy = cur_xy; };
    if (fps_look) {
        v2 offset = (cur_xy - prev_xy) * mouse_sens;
        yaw += offset.x;
        pitch += offset.y;
        pitch = clamp(pitch, -PI / 2 + 0.01f, PI / 2 - 0.01f); // todo: EPS?
        cam_fwd = norm(v4{ cosf(yaw) * cosf(pitch), sinf(pitch), sinf(yaw) * cosf(pitch) });
    }

    v4 at = {0,0,0};
    if (rotate) {
        f32 time = sec(tnow() - tstart);
        cam_pos = v4{ sinf(time) * 50.f, 5, cosf(time) * 50.f };
    } else {
        at = cam_pos + cam_fwd;
    }

    float x_ar = R.wh.x / (float)R.wh.y;
    m4 p = use_orth ? ortho(-500.f, 500.f, -500.f, 500.f, 0.1f, 500.f) : perspective(PI/4, x_ar, 0.01f, 500.f);
    m4 v = look_at(cam_pos, cam_up, at);
    m4 m[3] = {
        rot_x_mat(angle),
        translate({ 0,0,10,0 }),
        rot_x_mat(angle_b)
    };

    for (int i = 0; i < ARSIZE(dd); i++) mvp[i] = p * v * m[i];

    R.clear(C::BLACK);
    R.submit(dd, ARSIZE(dd));
}
}
