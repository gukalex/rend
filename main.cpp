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
#include "demo_coroutines.h"
#include "demo_new.h"
#include "demo_board.h"


struct demo { void (*init)(rend& R); void (*update)(rend& R); const char* name; };
demo demos[] = { {cpu_sim::init, cpu_sim::update, "0: 8086"},
                 {demo_quads::init, demo_quads::update, "1: quads"},
                 {demo_compute::init, demo_compute::update, "2: compute"},
                 {demo_qube::init, demo_qube::update, "3: qube"},
                 {demo_coro::init, demo_coro::update, "4: coroutines"},
                 {demo_new::init, demo_new::update, "5: fun"},
                 {demo_board::init, demo_board::update, "6: board"} };
int demos_count = sizeof(demos) / sizeof(demo);

rend R;

#ifdef _WIN32
int WinMain(void* hInstance, void* hPrevInstance, char* lpCmdLine, int nShowCmd) {
#else
int main(void) {
#endif
    const char* homedir = std::getenv("HOME");
    int homedir_size = strlen(homedir);
    char rend_ini_path[128] = {};
    char imgui_ini_path[128] = {};
    memcpy(rend_ini_path, homedir, homedir_size);
    memcpy(rend_ini_path + homedir_size, "/demos_rend.ini", strlen("/demos_rend.ini"));
    memcpy(imgui_ini_path, homedir, strlen(homedir));
    memcpy(imgui_ini_path + homedir_size, "/demos_imgui.ini", strlen("/demos_imgui.ini"));

    set_print_options({ "debug.txt" });
    defer{ set_print_options({0}); };
    R.ms = false;
    R.debug = 1;
    R.wh = { (u32)((1920 * 2) / 2.5f), (u32)(1080 / 2.5f) }; //16 * 2 / 9
    R.vsync = false;
    R.qb.max_quads = 100000;
    R.save_and_load_win_params = true;
    R.imgui_custom_path = imgui_ini_path; //"/workbig/demos_imgui.ini";
    R.rend_ini_path = rend_ini_path; // "/workbig/demos_rend.ini";
    #ifdef _WIN32
        R.imgui_font_size = 28; // todo: if dpi is high
    #else
        R.imgui_font_size = 16; // todo: if dpi is high
    #endif
    R.imgui_font_file_ttf = "Cousine-Regular.ttf";
    R.init();
    
    int curr_demo = 6;
    demos[curr_demo].init(R);

    i64 timer_start = tnow();
    i64 every_state = tnow();

    bool show_demo_window = false;
    while (!R.closed()) {
        auto* viewport = ImGui::GetWindowViewport();
        viewport->Flags |= ImGuiViewportFlags_TopMost;
        for (int i = 0; i < ARSIZE(demos); i++) {
            if (R.key_pressed('0' + i)) {
                demos[i].init(R);
                curr_demo = i;
            }
        }
        ImGui::Checkbox("ImGui Demo", &show_demo_window);
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
