// todo:
// - fonts
// - shaders
// - textures/rendertarget
// - passes
// - coordinates
// - 3d
//      - scenes/camera/some math
// - meshes/models?
// - demoes
//      - random generation
//      - npc ai
// - terrain
// - compute shaders
// - aspect ratio stuff

#include "rend.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <stdlib.h>
#include <memory.h>
// oh my
#include <chrono>
#include <thread>

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

float RN() { return rand() / (float)RAND_MAX; }
float RNC(f32 b) { return b + RN() * (1.f - 2.f * b); }

i64 tnow() { // nanoseconds since epoch
    return std::chrono::high_resolution_clock::now().time_since_epoch().count();
}
void tsleep(i64 nano) {
    std::this_thread::sleep_for(std::chrono::nanoseconds(nano));
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height);

void rend::init() {
    curr_time = prev_time = tnow();
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    window = glfwCreateWindow(w, h, "rend", NULL, NULL);
    glfwMakeContextCurrent(window);
    gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
    if (vsync)
        glfwSwapInterval(1);
    glViewport(0, 0, w, h);
    glfwSetWindowUserPointer(window, (void*)this);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;       // Enable Keyboard Controls
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;           // Enable Docking
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;         // Enable Multi-Viewport / Platform Windows
    //io.ConfigViewportsNoAutoMerge = true;
    //io.ConfigViewportsNoTaskBarIcon = true;

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsLight();

    // When viewports are enabled we tweak WindowRounding/WindowBg so platform windows can look identical to regular ones.
    ImGuiStyle& style = ImGui::GetStyle();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        style.WindowRounding = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }

    // Setup Platform/Renderer backends
    const char* glsl_version = "#version 330 core";
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

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

}
void rend::cleanup() {
    // dont care rn
    glDeleteVertexArrays(1, &vb_quad);
    glDeleteProgram(prog_quad);
    u32 buffers[] = { sb_quad_pos, sb_quad_attr1, sb_quad_indices };
    glDeleteBuffers(3, buffers);

    // quad resources
    free(quad_pos);
    free(quad_attr1);
    free(quad_indices);

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwTerminate();
}
void rend::present() { // todo: separate quad drawing into different function
    if (curr_quad_count) {
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
    }
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        GLFWwindow* backup_current_context = glfwGetCurrentContext();
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
        glfwMakeContextCurrent(backup_current_context);
    }

    glfwSwapBuffers(window);
    prev_time = curr_time;
    curr_time = tnow();
    fd = sec(curr_time - prev_time);
}
bool rend::closed() {
    glfwPollEvents();

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    ImGui::GetIO().FontGlobalScale = 2.0f;

    return glfwWindowShouldClose(window);
}
void rend::clear(v4 c) {
    glClear(GL_COLOR_BUFFER_BIT);
    glClearColor(c.x, c.y, c.z, c.w);
}
void rend::quad(v2 lb, v2 rt, v4 c) { // 0-1
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
    memcpy((u8*)quad_indices + curr_quad_count * sizeof(u32) * 6, indices, sizeof(indices));
    curr_quad_count++; // todo: assert max quad
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    rend* R = (rend*)glfwGetWindowUserPointer(window);
    R->w = width;
    R->h = height;
    glViewport(0, 0, R->w, R->h);
}
