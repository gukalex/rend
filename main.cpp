// todo:
// - vsync

#include <stdio.h>

#include "glad/glad.h"
#include <GLFW/glfw3.h>
#include <stdlib.h>
#include <memory.h>

float RN() { return rand() / (float)RAND_MAX; }

using u8 = unsigned char;
using u32 = unsigned int;
using i32 = int;

struct v4 {float x,y,z,w;};
struct v2 {float x,y;};
v2 operator+(v2 r, v2 l) { return { r.x + l.x, r.y + l.y }; };
v2 operator-(v2 r, v2 l) { return { r.x - l.x, r.y - l.y }; };
v2 operator*(v2 r, v2 l) { return { r.x * l.x, r.y * l.y }; };
v2 operator*(v2 r, float f) { return { r.x * f, r.y * f }; };
v2 operator+(v2 r, float f) { return { r.x + f, r.y + f }; };
v2 operator-(v2 r, float f) { return { r.x - f, r.y - f }; };

void framebuffer_size_callback(GLFWwindow* window, int width, int height);

struct rend {
    int w,h;
    GLFWwindow* window;
        
    const char* vs_quad = R"(
        #version 450 core
        layout (location = 0) in vec2 aPos;
        layout (location = 1) in vec4 aAttr;
        out vec4 vAttr;
        void main() {
           gl_Position = vec4(aPos.x, aPos.y, 0.0, 1.0);
           vAttr = aAttr; 
        }
    )";
    const char* fs_quad = R"(
        #version 450 core
        in vec4 vAttr;
        out vec4 FragColor;
        void main() {
           FragColor = vAttr;
        }
    )";
    u32 prog_quad;
    u32 vb_quad;
    u32 sb_quad_pos;
    u32 sb_quad_attr1;
    u32 sb_quad_indices;
    u32 max_quads = 1'000'000; // per init
    float *quad_pos; // vec2
    float *quad_attr1; // vec4
    u32* quad_indices; // 2 triangles = 6
    u32 curr_quad_count = 0;

    void init() {
        glfwInit();
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        window = glfwCreateWindow(w, h, "rend", NULL, NULL);
        glfwMakeContextCurrent(window);
        gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
        //glfwSwapInterval(1); vsync
        glViewport(0, 0, w, h);
        glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

        quad_pos = (float*)malloc(max_quads * sizeof(float) * 2 * 4);
        quad_attr1 = (float*)malloc(max_quads * sizeof(float) * 4 * 4);
        quad_indices = (u32*)malloc(max_quads * sizeof(u32) * 6);

        unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vertexShader, 1, &vs_quad, NULL);
        glCompileShader(vertexShader);
        int success;
        char infoLog[512];
        glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
        if (!success)
        {
            glGetShaderInfoLog(vertexShader, 512, NULL, infoLog);
            printf("vertex: %s\n", infoLog);
        }
        unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fragmentShader, 1, &fs_quad, NULL);
        glCompileShader(fragmentShader);
        glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
        if (!success)
        {
            glGetShaderInfoLog(fragmentShader, 512, NULL, infoLog);
            printf("fragment: %s\n", infoLog);
        }
        prog_quad = glCreateProgram();
        glAttachShader(prog_quad, vertexShader);
        glAttachShader(prog_quad, fragmentShader);
        glLinkProgram(prog_quad);
        glGetProgramiv(prog_quad, GL_LINK_STATUS, &success);
        if (!success) {
            glGetProgramInfoLog(prog_quad, 512, NULL, infoLog);
            printf("shader link: %s", infoLog);
        }
        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);
        
        glGenVertexArrays(1, &vb_quad);
        glGenBuffers(1, &sb_quad_pos);
        glGenBuffers(1, &sb_quad_indices);
        glGenBuffers(1, &sb_quad_attr1);
        // bind the Vertex Array Object first, then bind and set vertex buffer(s), and then configure vertex attributes(s).
        glBindVertexArray(vb_quad);

        glBindBuffer(GL_ARRAY_BUFFER, sb_quad_pos);
        glBufferData(GL_ARRAY_BUFFER, max_quads * sizeof(float) * 2 * 4, NULL, GL_DYNAMIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, sb_quad_indices);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, max_quads * sizeof(u32) * 6, NULL, GL_STATIC_DRAW);

        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);

        glBindBuffer(GL_ARRAY_BUFFER, sb_quad_attr1);
        glBufferData(GL_ARRAY_BUFFER, max_quads * sizeof(float) * 4 * 4, NULL, GL_DYNAMIC_DRAW);
        glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(1);
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        glBindVertexArray(0);

    };
    void cleanup() {
        // dont care rn

        // quad resources
        free(quad_pos);
        free(quad_attr1);
        free(quad_indices);

        glfwTerminate();
    }
    void present() { // todo: separate quad drawing into different function
        // upload quad vertex data
        glBindBuffer(GL_ARRAY_BUFFER, sb_quad_pos);
        glBufferSubData(GL_ARRAY_BUFFER, 0, curr_quad_count * 4 * sizeof(float) * 2, quad_pos);
        glBindBuffer(GL_ARRAY_BUFFER, sb_quad_attr1);
        glBufferSubData(GL_ARRAY_BUFFER, 0, curr_quad_count * 4 * sizeof(float) * 4, quad_attr1);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, sb_quad_indices);
        glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, curr_quad_count * sizeof(u32) * 6, quad_indices);
        // draw quads
        glUseProgram(prog_quad);
        glBindVertexArray(vb_quad);
        glDrawElements(GL_TRIANGLES, curr_quad_count * 6, GL_UNSIGNED_INT, 0);
        curr_quad_count = 0;

        glfwSwapBuffers(window);
    }
    bool closed() {
        glfwPollEvents();
        return glfwWindowShouldClose(window);
    }
    void clear(v4 c) {
        glClear(GL_COLOR_BUFFER_BIT);
        glClearColor(c.x, c.y, c.z, c.w);
    }
    void quad(v2 lb, v2 rt, v4 c) { // 0-1
        u32 curr_index = curr_quad_count * 4;
        v2 lb_ndc = (lb * 2.0f) - 1.0f;
        v2 rt_ndc = (rt * 2.0f) - 1.0f;
        float vertices[] = {
             rt_ndc.x,  rt_ndc.y,  // top right
             rt_ndc.x, lb_ndc.y,  // bottom right
             lb_ndc.x, lb_ndc.y,  // bottom left
             lb_ndc.x,  rt_ndc.y// top left 
        };
        float colors[] = {
             c.x, c.y, c.z, c.w, // !WASTEFUL SHAME!
             c.x, c.y, c.z, c.w,
             c.x, c.y, c.z, c.w,
             c.x, c.y, c.z, c.w,
        };
        unsigned int indices[] = {
            0 + curr_index, 1 + curr_index, 3 + curr_index,
            1 + curr_index, 2 + curr_index, 3 + curr_index
        };
        memcpy((u8*)quad_pos + curr_quad_count * sizeof(float) * 2 * 4, vertices, sizeof(vertices));
        memcpy((u8*)quad_attr1 + curr_quad_count * sizeof(float) * 4 * 4, colors, sizeof(colors));
        memcpy((u8*)quad_indices + curr_quad_count * sizeof(u32) * 6 , indices, sizeof(indices));
        curr_quad_count++; // todo: assert max quad
    }
};

rend R {1024, 1024};

void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    R.w = width;
    R.h = height;
    glViewport(0, 0, R.w, R.h);
}

int main(void) {
    R.init();
    constexpr u32 Q_COUNT = 500000;
    struct quad { v2 lb, size; v4 c; };
    quad* q = (quad*)malloc(sizeof(quad) * Q_COUNT);
    for (int i = 0; i != Q_COUNT; i++) {
        q[i].lb.x = RN();
        q[i].lb.y = RN();
        q[i].size = {0.001, 0.001};
        q[i].c = {RN(), RN(), RN(), 0};
    }
    float offset = 0.0;
    while (!R.closed()) {
        v4 cc = {1, 1, 0, 0};
        R.clear(cc);
        offset += 0.00000001; // todo: frame delta 
        for (int i = 0; i != Q_COUNT; i++) {
            q[i].lb.x += offset;
            R.quad(q[i].lb, q[i].lb + q[i].size, q[i].c);
        }
        R.present();
    }

    free(q);
    R.cleanup();
}
