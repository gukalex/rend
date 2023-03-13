CFLAGS= -I. -Iimgui/backends -Iimgui -O3
CXXFLAGS= -I. -Iimgui/backends -Iimgui -O3 -std=c++17 
LDFLAGS= -lpthread -ldl -lglfw -lGLU  -lGL -lXrandr -lX11 -lrt -lpthread 

SRC=glad.c \
    glad.c \
    stb_image.c

SRCPP=	http.cpp \
    http.cpp \
    imgui.cpp \
    imgui_demo.cpp \
    imgui_draw.cpp \
    imgui_tables.cpp \
    imgui_widgets.cpp \
    imgui_impl_glfw.cpp \
    imgui_impl_opengl3.cpp \
    main.cpp \
    rend.cpp \
    std.cpp

OBJ=${SRC:.c=.o}
OBJPP=${SRCPP:.cpp=.o}

BIN=demo

VPATH = imgui/:imgui/backends


all: imgui ${BIN}

${BIN}: ${OBJ} ${OBJPP}
	g++ $^ ${LDFLAGS} -o $@

imgui:
	git clone -b docking --depth 1 https://github.com/ocornut/imgui

clean:
	${RM} ${OBJ} ${OBJPP} ${BIN}