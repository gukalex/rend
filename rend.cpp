// todo:
// - fonts
// - rendertarget
// - passes
// - coordinates
// - 3d
//      - scenes/camera/some math
// - meshes/models?
// - demos
//      - random generation
//      - npc ai
// - terrain

#include "std.h"
#include "rend.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include "stb_image.h"

#define REND_INI_FILENAME "rend.ini"
#define REND_INI_VER 2
struct rend_ini {
    int ver = REND_INI_VER; int x = 0; int y = 0;
    int width = 100; int height = 100; 
};

void GLAPIENTRY gl_errors(GLenum source,GLenum type,GLuint id,GLenum severity,GLsizei length,
    const GLchar* message, const void* userParam) {
    if (GL_DEBUG_SEVERITY_HIGH == severity || GL_DEBUG_SEVERITY_MEDIUM == severity) {
        print("GL %s t(0x%x), s(0x%x): %s", (type == GL_DEBUG_TYPE_ERROR ? " ERROR" : ""),
            type, severity, message);
    }
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height);

u32 rend::shader(const char* vs, const char* fs, const char* cs, bool verbose) {
    auto compile = [](u32 type, const char* src, bool verbose) {
        u32 shader = glCreateShader(type);
        glShaderSource(shader, 1, &src, NULL);
        glCompileShader(shader);
        if (verbose) {
            int success;
            char infoLog[512];
            glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
            if (!success)
            {
                glGetShaderInfoLog(shader, 512, NULL, infoLog);
                print("%d: %s", type, infoLog);
                print("%s", src);
            }
        }
        return shader;
    };
    u32 vertex = vs ? compile(GL_VERTEX_SHADER, vs, verbose) : 0;
    u32 fragment = fs ? compile(GL_FRAGMENT_SHADER, fs, verbose) : 0;
    u32 compute = cs ? compile(GL_COMPUTE_SHADER, cs, verbose) : 0;
    u32 prog = glCreateProgram();
    if (vs) glAttachShader(prog, vertex);
    if (fs) glAttachShader(prog, fragment);
    if (cs) glAttachShader(prog, compute);
    glLinkProgram(prog);
    int success;
    char infoLog[512];
    if (verbose) {
        glGetProgramiv(prog, GL_LINK_STATUS, &success);
        if (!success) {
            glGetProgramInfoLog(prog, 512, NULL, infoLog);
            print("shader link: %s", infoLog);
        }
    }
    if (vs) glDeleteShader(vertex);
    if (fs) glDeleteShader(fragment);
    if (cs) glDeleteShader(compute);
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
    window = glfwCreateWindow(wh.x, wh.y, window_name, NULL, NULL);
    if (save_and_load_win_params) {
        buffer rend_file = read_file(REND_INI_FILENAME);
        if (rend_file.data) {
            rend_ini* data = (rend_ini*)rend_file.data;
            if (data->ver == REND_INI_VER && data->width > 0 && data->height > 0) {
                // todo: if data->x > 0 && data->y > 0 or can grab the title
                glfwSetWindowPos(window, data->x, data->y);
                wh.x = data->width;
                wh.y = data->height;
                glfwSetWindowSize(window, wh.x, wh.y);
            } // else log
        }
    }
    glfwMakeContextCurrent(window);
    int glad_ok = gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
    ASSERT(glad_ok);
    if (ms)
        glEnable(GL_MULTISAMPLE);
    if (vsync)
        glfwSwapInterval(1);
    else
        glfwSwapInterval(0);
    if (depth_test)
        glEnable(GL_DEPTH_TEST);
    glViewport(0, 0, wh.x, wh.y);
    glfwSetWindowUserPointer(window, (void*)this);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    if (debug) {
        glEnable(GL_DEBUG_OUTPUT);
        if (debug > 1)
            glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
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
    if (imgui_font_file_ttf)
        io.Fonts->AddFontFromFileTTF(imgui_font_file_ttf, (float)imgui_font_size);
    ImGui::StyleColorsDark();
    ImGui::GetStyle().FrameRounding = 12.0;
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330 core");
    progs = (u32*)alloc(max_progs * sizeof(u32));
    textures = (u32*)alloc(max_textures * sizeof(u32));
    progs[curr_progs++] = shader(vs_quad, fs_quad);
    dd = { {}, progs[0], 0 };
    qb.init();
}

void quad_batcher::init() {
    quad_pos = (float*)alloc(max_quads * sizeof(float) * 2 * 4);
    quad_attr1 = (float*)alloc(max_quads * sizeof(float) * 4 * 4);

    glGenVertexArrays(1, &vb_quad);
    glGenBuffers(1, &sb_quad_pos);
    glGenBuffers(1, &sb_quad_indices);
    glGenBuffers(1, &sb_quad_attr1);
    glBindVertexArray(vb_quad);

    glBindBuffer(GL_ARRAY_BUFFER, sb_quad_pos);
    glBufferData(GL_ARRAY_BUFFER, max_quads * sizeof(float) * 2 * 4, NULL, GL_DYNAMIC_DRAW);

    u32* quad_indices = (u32*)alloc(max_quads * sizeof(u32) * 6);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, sb_quad_indices);
    for (u32 i = 0; i < max_quads; i++) {
        u32 i_s = i * 6;
        u32 q = i * 4;
        quad_indices[i_s] = q + 0;
        quad_indices[i_s + 1] = q + 1;
        quad_indices[i_s + 2] = q + 3;
        quad_indices[i_s + 3] = q + 1;
        quad_indices[i_s + 4] = q + 2;
        quad_indices[i_s + 5] = q + 3;
    }
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, max_quads * sizeof(u32) * 6, quad_indices, GL_STATIC_DRAW);
    dealloc(quad_indices);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glBindBuffer(GL_ARRAY_BUFFER, sb_quad_attr1);
    glBufferData(GL_ARRAY_BUFFER, max_quads * sizeof(float) * 4 * 4, NULL, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glBindVertexArray(0);
}

void quad_batcher::cleanup() {
    glDeleteVertexArrays(1, &vb_quad);
    u32 buffers[] = { sb_quad_pos, sb_quad_attr1, sb_quad_indices };
    glDeleteBuffers(3, buffers);

    dealloc(quad_pos);
    dealloc(quad_attr1);
}

void quad_batcher::upload(indexed_buffer* ib) {
    if (curr_quad_count) {
        // upload quad vertex data
        // glBindVertexArray(vb_quad); ?? todo: please check with multiple batchers
        glBindBuffer(GL_ARRAY_BUFFER, sb_quad_pos);
        glBufferSubData(GL_ARRAY_BUFFER, 0, curr_quad_count * 4 * sizeof(float) * 2, quad_pos);
        glBindBuffer(GL_ARRAY_BUFFER, sb_quad_attr1);
        glBufferSubData(GL_ARRAY_BUFFER, 0, curr_quad_count * 4 * sizeof(float) * 4, quad_attr1);
        // indicies are preinited
        if (ib) {
            ib->id = vb_quad;
            ib->vertex_count = curr_quad_count * 6;
            ib->index_offset = 0;
        }
        curr_quad_count = 0;
        saved_count = 0;
    }
    // todo: unbind?
}

indexed_buffer quad_batcher::next_ib() {
    indexed_buffer ib = { vb_quad, saved_count * 6 * sizeof(int), vertex_count() - (saved_count * 6)};
    saved_count = curr_quad_count;
    return ib;
}

void quad_batcher::quad_a(v2 lb, v2 rt, v4 attr[4]) {
    float vertices[] = {
            rt.x, rt.y, // top right
            rt.x, lb.y,  // bottom right
            lb.x, lb.y,  // bottom left
            lb.x, rt.y  // top left 
    };
    memcpy((u8*)quad_pos + curr_quad_count * sizeof(float) * 2 * 4, vertices, sizeof(vertices));
    memcpy((u8*)quad_attr1 + curr_quad_count * sizeof(float) * 4 * 4, attr, sizeof(float) * 16);
    // quad_indices are preinitialized since the indicies do not change
    curr_quad_count++;
    ASSERT(curr_quad_count <= max_quads);
}
void quad_batcher::quad(v2 lb, v2 rt, v4 c) { // 0-1
    v4 attr[4] = { c, c, c, c };
    quad_a(lb, rt, attr);
}

void quad_batcher::quad_t(v2 lb, v2 rt, v2 attr) {
    v4 base_attr[4] = { {1, 1, attr.x, attr.y},
                        {1, 0, attr.x, attr.y},
                        {0, 0, attr.x, attr.y},
                        {0, 1, attr.x, attr.y} };
    quad_a(lb, rt, base_attr);
}

void quad_batcher::quad_s(v2 center, f32 size, v4 c) {
    v2 lb = center - size / 2.f;
    v2 rt = center + size / 2.f;
    quad(lb, rt, c);
}

void rend::submit_quads(draw_data* dd) { // do you need pointer ? if you want to use indexed_buffer afterwards
    qb.upload(&dd->ib);
    submit(dd, 1);
}

void rend::submit(draw_data* dd, int dd_count) {
    for (int i = 0; i < dd_count; i++) {
        glUseProgram(dd[i].prog);
        for (int t = 0; t < DATA_MAX_ELEM; t++) {
            if (dd[i].tex) {
                glActiveTexture(GL_TEXTURE0 + t);
                glBindTexture(GL_TEXTURE_2D, dd[i].tex[t]);
                char tex_name[] = "rend_t*"; tex_name[6] = '0' + i;
                u32 loc = glGetUniformLocation(dd[i].prog, tex_name); // todo: when creating shader
                glUniform1i(loc, i);
            }
        }
        glUniformMatrix4fv(glGetUniformLocation(dd[i].prog, "rend_m"), 1, true, &dd[i].m._0.x);
        glUniformMatrix4fv(glGetUniformLocation(dd[i].prog, "rend_v"), 1, true, &dd[i].v._0.x);
        glUniformMatrix4fv(glGetUniformLocation(dd[i].prog, "rend_p"), 1, true, &dd[i].p._0.x);

        glBindVertexArray(dd[i].ib.id);
        glDrawElements(GL_TRIANGLES, dd[i].ib.vertex_count, GL_UNSIGNED_INT, (void*)(u64)dd[i].ib.index_offset);
    }
    glBindVertexArray(0);
}

void rend::cleanup() {
    for (int i = 0; i < curr_progs; i++)
        glDeleteProgram(progs[i]);

    qb.cleanup();

    glDeleteTextures(curr_tex, textures);

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    if (save_and_load_win_params) {
        rend_ini data = {};
        glfwGetWindowPos(window, &data.x, &data.y);
        glfwGetWindowSize(window, &data.width, &data.height);
        write_file(REND_INI_FILENAME, {(u8*)&data, sizeof(data)});
    }

    glfwTerminate();
}

u32 rend::texture(const char* filename) {
    int w, h, ch;
    unsigned char* data = stbi_load(filename, &w, &h, &ch, 0);
    return texture(data, w, h, ch);
    stbi_image_free(data);
}

u32 rend::ssbo(u64 size, void* init_data, bool init_with_value, u32 default_u32_value) {
    // Generate the input buffer
    u32 buffer = 0;
    glGenBuffers(1, &buffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, buffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, size, init_data, GL_STATIC_DRAW);
    if (glGetError() != GL_NO_ERROR) return 0; // no need ?
    if (init_with_value) {
        glClearBufferData(GL_SHADER_STORAGE_BUFFER, GL_R32UI, GL_RED, GL_UNSIGNED_INT, (void*)&default_u32_value);
    }
    return buffer;
}

void rend::free_resources(resources res) {
    glDeleteBuffers(DATA_MAX_ELEM, res.buffer);
    for (int i = 0; i < DATA_MAX_ELEM; i++)
        glDeleteProgram(res.prog[i]);
    glDeleteTextures(DATA_MAX_ELEM, res.texture);
}

void* rend::map(u32 buffer, u64 offset, u64 size, map_type flags) {
    ASSERT(GL_MAP_READ_BIT == MAP_READ && GL_MAP_WRITE_BIT == MAP_WRITE);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, buffer); // todo: type parameter
    void* data = glMapBufferRange(GL_SHADER_STORAGE_BUFFER, offset, size, flags);
    return data;
}

void rend::unmap(u32 buffer) {
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, buffer);
    glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
}

u32 rend::texture(u8* data, int w, int h, int channel_count) {
    u32 tex = 0;
    if (data) {
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        u32 formats[] = {GL_RED, GL_RG, GL_RGB, GL_RGBA};
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, formats[channel_count - 1], GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);
    }
    return tex;
}

void rend::dispatch(dispatch_data data) {
    glUseProgram(data.prog);
    for (int i = 0; i < DATA_MAX_ELEM; i++) {
        if (data.ssbo[i]) glBindBufferBase(GL_SHADER_STORAGE_BUFFER, i, data.ssbo[i]); // todo: unbind
    }
    glDispatchCompute(data.x, data.y, data.z);
}

void rend::ssbo_barrier() {
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

void rend::present() {
    if (qb.curr_quad_count)// default quad_batcher assumed to handle default quads
        submit_quads(&dd);
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

    //ImGui::GetIO().FontGlobalScale = 1.0f;

    return glfwWindowShouldClose(window);
}

void rend::win_size(iv2 pwh) {
    wh = pwh;
    glfwSetWindowSize(window, wh.x, wh.y);
}

int rend::key_pressed(u32 kt, u32 flags) {
    if (flags & KT::IGNORE_IMGUI && (ImGui::IsWindowFocused() || ImGui::IsWindowHovered() /*?*/)) return 0;
    switch (kt) {
    case KT::MBL: return glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT);
    case KT::MBR: return glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT);
    case KT::CTRL: return glfwGetKey(window, GLFW_KEY_LEFT_CONTROL);
    case KT::SHIFT: return glfwGetKey(window, GLFW_KEY_LEFT_SHIFT);
    default: {
        return glfwGetKey(window, kt);
    } break;
    }
    return 0;
}

iv2 rend::mouse() {
    double x, y;
    glfwGetCursorPos(window, &x, &y);
    return {(i32)(f32)x, wh.y - (i32)(f32)y};
}
v2 rend::mouse_norm() {
    iv2 pos = mouse();
    v2 norm = {pos.x/(f32)wh.x, pos.y/(f32)wh.y};
    return clamp(norm, { 0,0 }, {1, 1});
}

void rend::clear(v4 c) {
    glClear(GL_COLOR_BUFFER_BIT | (depth_test ? GL_DEPTH_BUFFER_BIT : 0));
    glClearColor(c.x, c.y, c.z, c.w); // why it's under glClear?
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    rend* R = (rend*)glfwGetWindowUserPointer(window);
    R->wh.x = width;
    R->wh.y = height;
    glViewport(0, 0, R->wh.x, R->wh.y);
}
