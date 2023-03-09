#build object files for libraries we are not gonna touch (slow):
if [[ ! -f imgui.o || $1 = "ALL" ]] # todo: check all 
then
    echo "recompiling obj files"
    g++ -c glad.c stb_image.c -I. -Iimgui/backends -Iimgui imgui/imgui.cpp imgui/imgui_demo.cpp imgui/imgui_draw.cpp imgui/imgui_tables.cpp imgui/imgui_widgets.cpp imgui/backends/imgui_impl_glfw.cpp imgui/backends/imgui_impl_opengl3.cpp -O3
fi
#build our demos (consider moving std.cpp/rend.cpp in the line above):
echo "compiling demos"
g++ main.cpp rend.cpp std.cpp -I. glad.o stb_image.o -Iimgui/backends -Iimgui imgui.o imgui_demo.o imgui_draw.o imgui_tables.o imgui_widgets.o imgui_impl_glfw.o imgui_impl_opengl3.o -ldl -lglfw -lGLU -lGL -lXrandr -lX11 -lrt -O3