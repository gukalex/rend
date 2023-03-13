/*
CONFIGURE:
(./conf.sh) 
configure (download imgui and stb_image):


BUILD:
(./build.sh)
build object files for libraries we are not gonna touch (slow):
g++ -c glad.c stb_image.c -I. -Iimgui/backends -Iimgui imgui/imgui.cpp imgui/imgui_demo.cpp imgui/imgui_draw.cpp imgui/imgui_tables.cpp imgui/imgui_widgets.cpp imgui/backends/imgui_impl_glfw.cpp imgui/backends/imgui_impl_opengl3.cpp -O3
build our demos (consider moving std.cpp/rend.cpp in the line above):
g++ main.cpp rend.cpp std.cpp -I. glad.o stb_image.o -Iimgui/backends -Iimgui imgui.o imgui_demo.o imgui_draw.o imgui_tables.o imgui_widgets.o imgui_impl_glfw.o imgui_impl_opengl3.o -ldl -lglfw -lGLU -lGL -lXrandr -lX11 -lrt -O3
*/

#include "rend.h"
#include "imgui/imgui.h"

#include "demo_quads.h"
#include "demo_compute.h"
#include "demo_qube.h"
#include "8086.h"
#include "demo_client.h"
#include "demo_server.h"


struct demo { void (*init)(rend& R); void (*update)(rend& R); const char* name; };
demo demos[] = { {cpu_sim::init, cpu_sim::update, "8086"},
                 {demo_quads::init, demo_quads::update, "quads"},
                 {demo_compute::init, demo_compute::update, "compute"},
                 {demo_qube::init, demo_qube::update, "qube"},
                 {client::init, client::update, "client"},
                 {demo_server::init, demo_server::update, "server"} };
int demos_count = sizeof(demos) / sizeof(demo);

rend R;

#ifdef _WIN32
int WinMain(void* hInstance, void* hPrevInstance, char* lpCmdLine, int nShowCmd) {
#else
int main(void) {
#endif
    set_print_options({ "debug.txt" });
    defer{ set_print_options({0}); };

    R.wh = { 1024, 1024 };
    R.vsync = true;
    R.save_and_load_win_params = true;
    #ifdef _WIN32
        // todo: if dpi is high
        R.imgui_font_size = 28;
        R.imgui_font_file_ttf = "C:\\data\\rend\\imgui\\misc\\fonts\\Cousine-Regular.ttf";
    #else
        // todo: if dpi is high
        R.imgui_font_size = 16;
        R.imgui_font_file_ttf = "imgui/misc/fonts/Cousine-Regular.ttf";
    #endif
    R.init();
    
    int curr_demo = 2;
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
