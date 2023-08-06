CONFIGURE (linux):
install glfw (sudo apt-get install libglfw3 libglfw3-dev)
run ./conf.sh (downloads imgui, stb_image, httplib)
BUILD (linux):
./build.sh

CONFIGURE (windows):
run ./conf_win.sh (run under windows' git bash command line, in case you're not on VS2022 - replace GLFW library from the config scrips) 
BUILD (windows):
open rend.sln and do stuff

Server ToDo:
* Use int in the state instead of float (probably increase arena size so we cover 2x the resolution)
* Fixed step simulation
* Guns 🔫 (each unit has 1 shot and it can recharge after sleeping at the spawn)
