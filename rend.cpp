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

#include "stb_image.h"

m4 ortho(f32 l, f32 r, f32 b, f32 t, f32 n, f32 f) {
    return { 
        {2 / (r - l), 0, 0, -(r + l) / (r - l)},
        {0, 2 / (t - b), 0, -(t + b) / (t - b)},
        {0, 0, -2 /(f - n), -(f + n) / (f - n)},
        {0, 0, 0, 1} };
}

m4 identity() {
    return {
        {1, 0, 0, 0},
        {0, 1, 0, 0},
        {0, 0, 1, 0},
        {0, 0, 0, 1} };
}

float RN() { return rand() / (float)RAND_MAX; }
float RNC(f32 b) { return b + RN() * (1.f - 2.f * b); }

i64 tnow() { // nanoseconds since epoch
    return std::chrono::high_resolution_clock::now().time_since_epoch().count();
}
void tsleep(i64 nano) {
    std::this_thread::sleep_for(std::chrono::nanoseconds(nano));
}

void GLAPIENTRY gl_errors(GLenum source,GLenum type,GLuint id,GLenum severity,GLsizei length,
    const GLchar* message, const void* userParam) {
    printf("GL %s t(0x%x), s(0x%x): %s\n", (type == GL_DEBUG_TYPE_ERROR ? " ERROR" : ""),
        type, severity, message);
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height);

u32 rend::shader(const char* vs, const char* fs) {
    unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vs, NULL);
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
    glShaderSource(fragmentShader, 1, &fs, NULL);
    glCompileShader(fragmentShader);
    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(fragmentShader, 512, NULL, infoLog);
        printf("fragment: %s\n", infoLog);
    }
    u32 prog = glCreateProgram();
    glAttachShader(prog, vertexShader);
    glAttachShader(prog, fragmentShader);
    glLinkProgram(prog);
    glGetProgramiv(prog, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(prog, 512, NULL, infoLog);
        printf("shader link: %s", infoLog);
    }
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    return prog;
}

void rend::init() {
    stbi_set_flip_vertically_on_load(true);
    curr_time = prev_time = tnow();
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    if (ms)
        glfwWindowHint(GLFW_SAMPLES, 4);
    window = glfwCreateWindow(w, h, window_name, NULL, NULL);
    glfwMakeContextCurrent(window);
    gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
    if (ms)
        glEnable(GL_MULTISAMPLE);
    if (vsync)
        glfwSwapInterval(1);
    glViewport(0, 0, w, h);
    glfwSetWindowUserPointer(window, (void*)this);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    if (debug) {
        glEnable(GL_DEBUG_OUTPUT);
        glDebugMessageCallback(gl_errors, 0);
    }
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;     // Enable Docking
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;   // Enable Multi-Viewport / Platform Windows
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330 core");

    progs = (u32*)malloc(max_progs * sizeof(u32));
    textures = (u32*)malloc(max_textures * sizeof(u32));
    quad_pos = (float*)malloc(max_quads * sizeof(float) * 2 * 4);
    quad_attr1 = (float*)malloc(max_quads * sizeof(float) * 4 * 4);

    progs[curr_progs++] = shader(vs_quad, fs_quad);
    default_quad_data = { progs[0],0 };

    glGenVertexArrays(1, &vb_quad);
    glGenBuffers(1, &sb_quad_pos);
    glGenBuffers(1, &sb_quad_indices);
    glGenBuffers(1, &sb_quad_attr1);
    glBindVertexArray(vb_quad);

    glBindBuffer(GL_ARRAY_BUFFER, sb_quad_pos);
    glBufferData(GL_ARRAY_BUFFER, max_quads * sizeof(float) * 2 * 4, NULL, GL_DYNAMIC_DRAW);

    u32 *quad_indices = (u32*)malloc(max_quads * sizeof(u32) * 6);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, sb_quad_indices);
    for (u32 i = 0; i < max_quads; i++) {
        u32 i_s = i * 6;
        u32 q = i * 4;
        quad_indices[i_s] =     q + 0;
        quad_indices[i_s + 1] = q + 1;
        quad_indices[i_s + 2] = q + 3;
        quad_indices[i_s + 3] = q + 1;
        quad_indices[i_s + 4] = q + 2;
        quad_indices[i_s + 5] = q + 3;
    }
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, max_quads * sizeof(u32) * 6, quad_indices, GL_STATIC_DRAW);
    free(quad_indices);

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
    glDeleteVertexArrays(1, &vb_quad);
    u32 buffers[] = { sb_quad_pos, sb_quad_attr1, sb_quad_indices };
    glDeleteBuffers(3, buffers);
    for (int i = 0; i < curr_progs; i++)
        glDeleteProgram(progs[i]);

    glDeleteTextures(curr_tex, textures);

    // quad resources
    free(quad_pos);
    free(quad_attr1);

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwTerminate();
}

u32 rend::texture(const char* filename) {
    int w, h, ch;
    unsigned char* data = stbi_load(filename, &w, &h, &ch, 0);
    return texture(data, w, h, ch);
    stbi_image_free(data);
}

u32 rend::texture(u8* data, int w, int h, int channel_count) {
    u32 tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    u32 formats[] = {GL_RED, GL_RG, GL_RGB, GL_RGBA};
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, formats[channel_count - 1], GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);
    return tex;
}

void rend::submit(draw_data data) {
    if (curr_quad_count) {
        // upload quad vertex data
        glBindBuffer(GL_ARRAY_BUFFER, sb_quad_pos);
        glBufferSubData(GL_ARRAY_BUFFER, 0, curr_quad_count * 4 * sizeof(float) * 2, quad_pos);
        glBindBuffer(GL_ARRAY_BUFFER, sb_quad_attr1);
        glBufferSubData(GL_ARRAY_BUFFER, 0, curr_quad_count * 4 * sizeof(float) * 4, quad_attr1);
        // indicies are preinited
        glUseProgram(data.prog);
        if (data.tex) {
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, data.tex);
            u32 loc = glGetUniformLocation(data.prog, "rend_t0"); // todo: when creating shader
            glUniform1i(loc, 0);
        }
        glUniformMatrix4fv(glGetUniformLocation(data.prog, "rend_m"), 1, true, &data.m._0.x);
        glUniformMatrix4fv(glGetUniformLocation(data.prog, "rend_v"), 1, true, &data.v._0.x);
        glUniformMatrix4fv(glGetUniformLocation(data.prog, "rend_p"), 1, true, &data.p._0.x);

        // draw quads
        glBindVertexArray(vb_quad);
        glDrawElements(GL_TRIANGLES, curr_quad_count * 6, GL_UNSIGNED_INT, 0);
        curr_quad_count = 0;
    }
}

void rend::present() { // todo: separate quad drawing into different function
    submit(default_quad_data);
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

    if (glfwGetKey(window, GLFW_KEY_ESCAPE))
        return true;

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    ImGui::GetIO().FontGlobalScale = 3.0f;

    return glfwWindowShouldClose(window);
}
void rend::clear(v4 c) {
    glClear(GL_COLOR_BUFFER_BIT);
    glClearColor(c.x, c.y, c.z, c.w);
}

void rend::quad_a(v2 lb, v2 rt, v4 attr[4]) {
    float vertices[] = {
            rt.x, rt.y, // top right
            rt.x, lb.y,  // bottom right
            lb.x, lb.y,  // bottom left
            lb.x, rt.y  // top left 
    };
    float colors[] = {
            attr[0].x, attr[0].y, attr[0].z, attr[0].w,
            attr[1].x, attr[1].y, attr[1].z, attr[1].w,
            attr[2].x, attr[2].y, attr[2].z, attr[2].w,
            attr[3].x, attr[3].y, attr[3].z, attr[3].w,
    };
    memcpy((u8*)quad_pos + curr_quad_count * sizeof(float) * 2 * 4, vertices, sizeof(vertices));
    memcpy((u8*)quad_attr1 + curr_quad_count * sizeof(float) * 4 * 4, colors, sizeof(colors));
    // quad_indices are preinitialized since the indicies do not change
    curr_quad_count++; // todo: assert max quad
}
void rend::quad(v2 lb, v2 rt, v4 c) { // 0-1
    v4 attr[4] = { c, c, c, c };
    quad_a(lb, rt, attr);
}

void rend::quad_t(v2 lb, v2 rt, v2 attr) {
    v4 base_attr[4] = { {1, 1, attr.x, attr.y},
                        {1, 0, attr.x, attr.y}, 
                        {0, 0, attr.x, attr.y},
                        {0, 1, attr.x, attr.y} };
    quad_a(lb, rt, base_attr);
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    rend* R = (rend*)glfwGetWindowUserPointer(window);
    R->w = width;
    R->h = height;
    glViewport(0, 0, R->w, R->h);
}
