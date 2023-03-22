#pragma once
#include "rend.h"
#include "std.h"
#include <math.h> // sinf

namespace demo_coro {

f32 default_size = 0.25f;
v2 start_pos = { -0.5, 0.5 };
v2 pos = start_pos; f32 size = default_size;
f32 pre_charge_meter = 0; // 0-1
f32 pre_charge_sec = 2.f;
f32 charge_sec = 5.f;

bool use_coro = true;
i64 tstart;

void init(rend& R) { tstart = tnow(); }

void do_coro(rend& R) {
    static coroutine c1 = {}; // can also be a function argument
    static v2 target = pos;

    f32 elapsed = sec(tnow() - tstart);

    CR(c1) {
        CR_START();
        while (pre_charge_meter < 1.f) {
            pre_charge_meter += (R.key_pressed(' ') ? R.fd : -R.fd) / pre_charge_sec;
            pre_charge_meter = clamp(pre_charge_meter, 0, 1);
            size = default_size + default_size * 0.1f * sinf(elapsed * 2.f);
            CR_YIELD();
        }
        pre_charge_meter = 0;
        CR_WHILE_START(charge_sec) {
            size = default_size + default_size * 0.1f * sinf(elapsed * 20.f);
        } CR_END
        target = (target == start_pos ? v2{ -start_pos.x, start_pos.y } : start_pos);
        while (len(pos - target) > 0.001f) {
            v2 dir = norm(target - pos);
            pos += dir * R.fd;
            CR_YIELD();
        }
        CR_RESET();
    }
}

void do_switch(rend& R) {
    enum state {
        STATE_IDLE,
        STATE_CHARGE,
        STATE_MOVE
    };
    static state st = STATE_IDLE;
    static i64 charge_start = tstart;
    static v2 target = pos;

    f32 elapsed = sec(tnow() - tstart);

    switch (st) {
    case STATE_IDLE: {
        pre_charge_meter += (R.key_pressed(' ') ? R.fd : -R.fd) / pre_charge_sec;
        if (pre_charge_meter >= 1.f) {
            st = STATE_CHARGE;
            charge_start = tnow();
            pre_charge_meter = 0.f;
        }
        pre_charge_meter = clamp(pre_charge_meter, 0, 1);
        size = default_size + default_size * 0.1f * sinf(elapsed * 2.f);
    } break;
    case STATE_CHARGE:
        size = default_size + default_size * 0.1f * sinf(elapsed * 20.f);
        if (sec(tnow() - charge_start) > charge_sec) {
            st = STATE_MOVE;
            target = (target == start_pos ? v2{-start_pos.x, start_pos.y} : start_pos);
        }
        break;
    case STATE_MOVE: {
        v2 dir = norm(target - pos);
        pos += dir * R.fd;
        if (len(pos - target) < 0.001f) {
            st = STATE_IDLE;
        }
    } break;
    };

}

void update(rend& R) {
    ImGui::Checkbox("Use Coroutines", &use_coro);
    ImGui::TextWrapped("Size: %f, pre_charge_meter: %f", size, pre_charge_meter);

    (use_coro ? do_coro(R) : do_switch(R));

    R.clear(C::BLACK);
    R.quad_s(pos, size, C::RED);
    f32 length = pre_charge_meter * pre_charge_meter * 1 /* length of the bar */;
    R.quad({ -0.5,-1 }, { -0.5f + length, -0.75f }, C::BLUE);
}
}