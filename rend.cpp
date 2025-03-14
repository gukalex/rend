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
    window = glfwCreateWindow(wh.x, wh.y, window_name, (fs ? glfwGetPrimaryMonitor() : NULL), NULL);
    if (save_and_load_win_params) {
        buffer rend_file = read_file(rend_ini_path);
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
    glViewport(0, 0, wh.x, wh.y);
    glfwSetWindowUserPointer(window, (void*)this);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    if (debug) {
        glEnable(GL_DEBUG_OUTPUT);
        if (debug > 1)
            glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
        glDebugMessageCallback(gl_errors, 0);
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    if (imgui_custom_path)
        io.IniFilename = imgui_custom_path;
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

static u32 gl_type(attrib_type at) {
    switch (at) {
    case AT_FLOAT: return GL_FLOAT;
    case AT_UBYTE: return GL_UNSIGNED_BYTE;
    case AT_HALF: return GL_HALF_FLOAT;
    case AT_UINT: return GL_UNSIGNED_INT;
    }
    ASSERT(false);
    return AT_FLOAT;
}
static u32 gl_size(attrib_type at) {
    switch (at) {
    case AT_FLOAT: return sizeof(float);
    case AT_UBYTE: return 1;
    case AT_HALF: return 2;
    case AT_UINT: return sizeof(int);
    }
    ASSERT(false);
    return 4;
}

void init_vertex_buffer(vertex_buffer *vb) {
    glGenVertexArrays(1, &vb->ib.id);
    glGenBuffers(1, &vb->ib.index_buf);
    for (int i = 0; i < vb->ab_count; i++)
        glGenBuffers(1, &vb->ab[i].id);
    glBindVertexArray(vb->ib.id);

    for (int i = 0; i < vb->ab_count; i++) {
        u8 size_attr = gl_size(vb->ab[i].type) * vb->ab[i].num_comp;
        u64 size = vb->elem_num * size_attr;
        glBindBuffer(GL_ARRAY_BUFFER, vb->ab[i].id);
        glBufferData(GL_ARRAY_BUFFER, size, NULL, GL_STATIC_DRAW); //?
        glVertexAttribPointer(i, vb->ab[i].num_comp, gl_type(vb->ab[i].type), /*normalize?*/ vb->ab[i].normalize, /*stride?*/size_attr, (void*)0);
        glEnableVertexAttribArray(i);
    }

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vb->ib.index_buf);
    glBindVertexArray(0);
}

void init_quad_indicies(u32 index_buf, u32 quads) {
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_buf);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(u32) * quads * 6, NULL, GL_STATIC_DRAW);
    u32* quad_indices = (u32*)map(index_buf, 0, sizeof(u32) * quads * 6, MAP_WRITE);
    for (u32 i = 0; i < quads; i++) {
        u32 i_s = i * 6;
        u32 q = i * 4;
        quad_indices[i_s] = q + 0;
        quad_indices[i_s + 1] = q + 1;
        quad_indices[i_s + 2] = q + 3;
        quad_indices[i_s + 3] = q + 1;
        quad_indices[i_s + 4] = q + 2;
        quad_indices[i_s + 5] = q + 3;

        // top right     0
        // bottom right  1
        // bottom left   2
        // top left      3
    }
    unmap(index_buf);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);//?
}

void cleanup_vertex_buffer(vertex_buffer* vb) {
    glDeleteVertexArrays(1, &vb->ib.id);
    u32 buffers[DATA_MAX_ELEM] = {};
    for (int i = 0; i < vb->ab_count; i++) buffers[i] = vb->ab[i].id;
    glDeleteBuffers(DATA_MAX_ELEM, buffers);
}

void quad_batcher::init() {
    quad_pos = (float*)alloc(max_quads * sizeof(float) * 2 * 4);
    quad_attr1 = (float*)alloc(max_quads * sizeof(float) * 4 * 4);
    
    vb = { {}, max_quads * 4, 2, {
        {0, AT_FLOAT, 2},
        {0, AT_FLOAT, 4}
    } };

    init_vertex_buffer(&vb);
    init_quad_indicies(vb.ib.index_buf, max_quads);
}

void quad_batcher::cleanup() {
    cleanup_vertex_buffer(&vb);
    dealloc(quad_pos);
    dealloc(quad_attr1);
}

void quad_batcher::upload(indexed_buffer* ib) {
    if (curr_quad_count) {
        // upload quad vertex data
        // glBindVertexArray(vb_quad); ?? todo: please check with multiple batchers
        glBindBuffer(GL_ARRAY_BUFFER, vb.ab[POS].id);
        //glBufferSubData(GL_ARRAY_BUFFER, 0, curr_quad_count * 4 * sizeof(float) * 2, quad_pos);
        glBufferData(GL_ARRAY_BUFFER, curr_quad_count * 4 * sizeof(float) * 2, quad_pos, GL_DYNAMIC_DRAW); //static?
        glBindBuffer(GL_ARRAY_BUFFER, vb.ab[ATTR].id);
        //glBufferSubData(GL_ARRAY_BUFFER, 0, curr_quad_count * 4 * sizeof(float) * 4, quad_attr1);
        glBufferData(GL_ARRAY_BUFFER, curr_quad_count * 4 * sizeof(float) * 4, quad_attr1, GL_DYNAMIC_DRAW); //static?
        // indicies are preinited
        if (ib) {
            *ib = vb.ib;
            ib->vertex_count = curr_quad_count * 6;
        }
        curr_quad_count = 0;
        saved_count = 0;
    }
    // todo: unbind?
}

indexed_buffer quad_batcher::next_ib() {
    indexed_buffer ib = { vb.ib.id, saved_count * 6 * (u32)sizeof(int), vertex_count() - (saved_count * 6), vb.ib.index_buf };
    saved_count = curr_quad_count;
    return ib;
}

void quad_batcher::quad_manual(v2 lb, v2 rt, f32* attr, u32* cur_quad) {
    float vertices[] = {
            rt.x, rt.y, // top right
            rt.x, lb.y,  // bottom right
            lb.x, lb.y,  // bottom left
            lb.x, rt.y  // top left 
    };
    memcpy((u8*)quad_pos + (*cur_quad) * sizeof(float) * 2 * 4, vertices, sizeof(vertices));
    memcpy((u8*)quad_attr1 + (*cur_quad) * sizeof(float) * 4 * 4, attr, sizeof(float) * 16);
    // quad_indices are preinitialized since the indicies do not change
    (*cur_quad)++;
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

static void uniforms(u32 prog, uniform *uni, int size) {
    for (int i = 0; i < size; i++) {
        if (uni[i].name) {
            int loc = glGetUniformLocation(prog, uni[i].name);
            if (loc == -1) continue;
            switch (uni[i].type) {
            case UNIFORM_UINT: glUniform1ui(loc, *(u32*)uni[i].data); break;
            case UNIFORM_FLOAT: glUniform1f(loc, *(f32*)uni[i].data); break;
            case UNIFORM_VEC4: glUniform4fv(loc, 1, (f32*)uni[i].data); break;
            case UNIFORM_MAT4: glUniformMatrix4fv(loc, 1, true, (f32*)uni[i].data); break;
            }
        }
    }
}

void rend::submit(draw_data* dd, int dd_count) {
    for (int i = 0; i < dd_count; i++) {
        // hack!!!!., todo: remove, it may or may not affect the perf
        static blending prev_blend_state = (blending)-1; defer{ prev_blend_state = dd[i].state.blend; };
        static depth_state prev_depth_state = (depth_state)-1; defer{ prev_depth_state = dd[i].state.depth; };
        if (prev_blend_state != dd[i].state.blend) {
            switch(dd[i].state.blend) { // todo: if state changed
                case BLEND_NONE:
                    glDisable(GL_BLEND);
                break;
                case BLEND_ALPHABLEND:
                    glEnable(GL_BLEND);
                    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                break;
            }
        }
        if (prev_depth_state != dd[i].state.depth)
        switch(dd[i].state.depth) { // todo: if state changed
            case DEPTH_NONE:
                glDisable(GL_DEPTH_TEST);
            break;
            case DEPTH_LESS:
                glEnable(GL_DEPTH_TEST);
                glDepthFunc(GL_LESS); // LEQUAL??
            break;
        }
        glUseProgram(dd[i].prog);
        for (int t = 0; t < DATA_MAX_ELEM; t++) {
            if (dd[i].tex[t]) {
                glActiveTexture(GL_TEXTURE0 + t);
                glBindTexture(GL_TEXTURE_2D, dd[i].tex[t]);
                char tex_name[] = "rend_t*"; tex_name[6] = '0' + t;
                u32 loc = glGetUniformLocation(dd[i].prog, tex_name); // todo: when creating shader
                glUniform1i(loc, t);
            }
        }
        uniform mat_uni[] = { 
            {"rend_m", UNIFORM_MAT4, &dd[i].m}, 
            {"rend_v", UNIFORM_MAT4, &dd[i].v},
            {"rend_p", UNIFORM_MAT4, &dd[i].p}
        };
        uniforms(dd[i].prog, mat_uni, ARSIZE(mat_uni));
        uniforms(dd[i].prog, dd[i].uni, DATA_MAX_ELEM);
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
        write_file(rend_ini_path, {(u8*)&data, sizeof(data)});
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

void* map(u32 buffer, u64 offset, u64 size, u32 flags) {
    ASSERT(GL_MAP_READ_BIT == MAP_READ && GL_MAP_WRITE_BIT == MAP_WRITE);
    if ((flags & MAP_WRITE) && ~(flags & MAP_READ))
        flags |= GL_MAP_INVALIDATE_BUFFER_BIT;
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, buffer); // todo: type parameter
    void* data = glMapBufferRange(GL_SHADER_STORAGE_BUFFER, offset, size, flags);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0); //?
    return data;
}

void unmap(u32 buffer) {
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, buffer);
    glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
    ASSERT(glGetError() == GL_NO_ERROR);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0); //?
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
    uniforms(data.prog, data.uni, DATA_MAX_ELEM);
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

void rend::clear(v4 c, bool clear_depth) {
    glClearColor(c.x, c.y, c.z, c.w);
    glClear(GL_COLOR_BUFFER_BIT | (clear_depth ? GL_DEPTH_BUFFER_BIT : 0));
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    rend* R = (rend*)glfwGetWindowUserPointer(window);
    R->wh.x = width;
    R->wh.y = height;
    glViewport(0, 0, R->wh.x, R->wh.y);
}
