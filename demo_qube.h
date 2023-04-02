#pragma once
#include "rend.h"
#include "imgui/imgui.h"
#include <math.h>
#include <GLFW/glfw3.h>
namespace demo_qube {
i64 tstart;
draw_data data = {}, data_second = {};
f32 angle = 0, angle_b = PI/2, cam_speed = 100;
v4 cam_pos = {0,5,50}, cam_fwd = {0, 0, -1}, cam_up = {0,1,0};
bool use_orth = false, rotate = false, fps_look = false;
bool last_ctrl = false; // if ctrl was pressed
v2 prev_xy = {}; // last mouse pos
f32 mouse_sens = 0.001f; // radian per pixel?
f32 yaw = -PI/2, pitch = 0.f;
void init(rend& R) {
    tstart = tnow();
    if (!data.prog) {
        data_second.prog = data.prog = R.shader(R.vs_quad, R"(
        #version 450 core
        in vec4 vAttr;
        out vec4 FragColor;
        uniform sampler2D rend_t0;
        void main() {
            vec2 uv = vAttr.xy;
            FragColor.rgba = texture(rend_t0, uv).rgba;
            if (FragColor.a < 0.1) discard;
        })");
        data_second.tex[0] = data.tex[0] = R.texture("pepe.png");   
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
    data_second.p = data.p = use_orth ? ortho(-500.f, 500.f, -500.f, 500.f, 0.1f, 500.f) : perspective(PI/4, x_ar, 0.01f, 500.f);
    data_second.v = data.v = look_at(cam_pos, cam_up, at);
    data.m = rot_x_mat(angle);
    R.clear(C::BLACK);

    R.quad_t({ 0,0 }, { 10, 10 }, {});
    R.quad_t({ 20,0 }, { 30, 10 }, {});
    R.submit(data);

    data.m = translate({0,0,10,0});
    R.quad_t({ 30,0 }, { 40, 10 }, {});
    R.submit(data);

    data_second.m = rot_x_mat(angle_b);
    R.quad_t({ 0,0 }, { 100,100 }, {});
    R.submit(data_second);
}
}
