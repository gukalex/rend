CONFIGURE:

install glfw (sudo apt-get install libglfw3 libglfw3-dev)

run ./conf.sh (downloads imgui, stb_image, httplib)

BUILD (or use make):

./build.sh

Server ToDo:
* Use int in the state instead of float (probably increase arena size so we cover 2x the resolution)
* Fixed step simulation
* Guns 🔫 (each unit has 1 shot and it can recharge after sleeping at the spawn)
