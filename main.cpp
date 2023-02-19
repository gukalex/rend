#include "demo_quads.h"
#include "demo_compute.h"

#include "rend.h"
#include "imgui/imgui.h"

struct demo { void (*init)(rend& R); void (*update)(rend& R); const char* name; };
demo demos[] = { {demo_quads::init, demo_quads::update, "quads"},
                 {demo_compute::init, demo_compute::update, "compute"} };
int demos_count = sizeof(demos) / sizeof(demo);

rend R;

int main(void) {
    R.w = 1024; R.h = 1024;
    R.vsync = false;
    R.init();
    
    int curr_demo = 0;
    demos[curr_demo].init(R);

    i64 timer_start = tnow();
    i64 every_state = tnow();

    bool show_demo_window = false;
    while (!R.closed()) {
        static avg_data fps_avg = avg_init(0.5), frametime_avg = avg_init(0.5);
        ImGui::Text("FPS: %f (avg %f), avg timeframe: %f, time: %f", 1/R.fd, avg(1 / R.fd, &fps_avg), avg(R.fd * 1000.f, &frametime_avg), sec(tnow()-timer_start));
        for (int i = 0; i < demos_count; i++) {
            ImGui::PushID(i);
            if (ImGui::RadioButton(demos[i].name, &curr_demo, i)) {
                demos[i].init(R);
            }
            ImGui::PopID();
        }

        if (show_demo_window)
            ImGui::ShowDemoWindow(&show_demo_window);

        demos[curr_demo].update(R);
        R.present();
    }
    R.cleanup();
}
