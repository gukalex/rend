#pragma once
#include "rend.h"
#include "imgui/imgui.h"
namespace demo_qube {
draw_data data = { 0, 0 };

void init(rend& R) {
    
}
void update(rend& R) {
    float x_ar = R.w / (float)R.h;
    data.p = ortho(0, x_ar, 0, 1);
    R.clear({ 0.3f, 0.2f, 0.5f, 0.f });

    // do stuff

    R.submit(data);
}
}
