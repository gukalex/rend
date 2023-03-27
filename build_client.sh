#build object files for libraries we are not gonna touch (slow):
if [[ ! -f imgui_unity.o || $1 = "ALL" ]] # todo: check all 
then
    echo "recompiling obj files"
    g++ -c glad.c stb_image.c -I. http.cpp -Iimgui/backends -Iimgui imgui_unity.cpp -O3 -std=c++17 -lpthread
fi
#build our demos (consider moving std.cpp/rend.cpp in the line above):
echo "compiling demos"
g++ client.cpp rend.cpp std.cpp -I. glad.o stb_image.o http.o -Iimgui/backends -Iimgui imgui_unity.o -ldl -lglfw -lGLU -lGL -lXrandr -lX11 -lrt -O3 -lpthread -std=c++17 -o client