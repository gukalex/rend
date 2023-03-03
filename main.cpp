#include "rend.h"
#include "imgui/imgui.h"

#include "demo_quads.h"
#include "demo_compute.h"
#include "demo_qube.h"
#include "8086.h"


struct demo { void (*init)(rend& R); void (*update)(rend& R); const char* name; };
demo demos[] = { {cpu_sim::init, cpu_sim::update, "8086"},
                 {demo_quads::init, demo_quads::update, "quads"},
                 {demo_compute::init, demo_compute::update, "compute"},
                 {demo_qube::init, demo_qube::update, "qube"} };
int demos_count = sizeof(demos) / sizeof(demo);

rend R;

#ifdef _WIN32
int WinMain(void* hInstance, void* hPrevInstance, char* lpCmdLine, int nShowCmd) {
#else
int main(void) {
#endif
    R.w = 1024; R.h = 1024;
    R.vsync = true;
    #ifdef _WIN32
        // todo: if dpi is high
        R.imgui_font_size = 28;
        R.imgui_font_file_ttf = "C:\\data\\rend\\imgui\\misc\\fonts\\Cousine-Regular.ttf";
    #endif
    R.init();
    
    int curr_demo = 0;
    demos[curr_demo].init(R);

    i64 timer_start = tnow();
    i64 every_state = tnow();

    bool show_demo_window = false;
    while (!R.closed()) {
        static avg_data fps_avg = avg_init(0.5), frametime_avg = avg_init(0.5);
        ImGui::TextWrapped("FPS: %f (avg %f), avg timeframe: %f, time: %f", 1/R.fd, avg(1 / R.fd, &fps_avg), avg(R.fd * 1000.f, &frametime_avg), sec(tnow()-timer_start));
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
    exit(0);
}
