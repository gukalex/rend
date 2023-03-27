CFLAGS= -I. -Iimgui/backends -Iimgui -O3
CXXFLAGS= -I. -Iimgui/backends -Iimgui -O3 -std=c++17 
LDFLAGS= -lpthread -ldl -lglfw -lGLU  -lGL -lXrandr -lX11 -lrt
LDFLAGS_CM= -lpthread 

SRC=glad.c \
    glad.c \
    stb_image.c

SRCPP_COM=http.cpp \
    imgui_unity.cpp \
    rend.cpp \
    std.cpp

SRCPP_D=${SRCPP_COM} \
    main.cpp

SRCPP_S=${SRCPP_COM} \
    server.cpp
    
SRCPP_C=${SRCPP_COM} \
    client.cpp

SRCPP_CM=http.cpp \
    std.cpp \
    client_min.cpp

OBJ=${SRC:.c=.o}
OBJPP_D=${SRCPP_D:.cpp=.o}
OBJPP_S=${SRCPP_S:.cpp=.o}
OBJPP_C=${SRCPP_C:.cpp=.o}
OBJPP_CM=${SRCPP_CM:.cpp=.o}

BIN_DEMO=demo
BIN_SERVER=server
BIN_CLIENT=client
BIN_CLIENT_MIN=client_min

VPATH = imgui/:imgui/backends

all: imgui ${BIN_DEMO} ${BIN_SERVER} ${BIN_CLIENT}

${BIN_DEMO}: ${OBJ} ${OBJPP_D}
	g++ $^ ${LDFLAGS} -o $@

${BIN_SERVER}: ${OBJ} ${OBJPP_S}
	g++ $^ ${LDFLAGS} -o $@

${BIN_CLIENT}: ${OBJ} ${OBJPP_C}
	g++ $^ ${LDFLAGS} -o $@

${BIN_CLIENT_MIN}: ${OBJPP_CM}
	g++ $^ ${LDFLAGS} -o $@

imgui:
	git clone -b docking --depth 1 https://github.com/ocornut/imgui

clean:
	${RM} ${OBJ} ${OBJPP_D} ${OBJPP_C} ${OBJPP_S} ${OBJPP_CM} ${BIN_DEMO} ${BIN_SERVER} ${BIN_CLIENT} ${BIN_CLIENT_MIN}