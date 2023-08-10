namespace demo_new {
i64 tstart;
draw_data dd = {}; struct amogus_data { v2 pos; v2 size;}; amogus_data amoguses[1000];
void init(rend& R) {
    if (!dd.prog) {
        dd.prog = R.shader(R.vs_quad, R"(
        #version 450 core
        in vec4 vAttr;
        out vec4 FragColor;
        uniform sampler2D rend_t0;
        void main() {
            vec2 uv = vAttr.xy;
            FragColor.rgba = texture(rend_t0, uv).rgba * vec4(vAttr.zw,1,1);
        })");
        dd.tex[0] = R.texture("amogus.png");
        for (int i = 0; i < 1000; i++) {
            amoguses[i].pos = {RN(), RN()};
            amoguses[i].size = {RN() * 0.05f, RN()* 0.05f};
        }
    }
    tstart = tnow();
    print("hello 💩");
}
void update(rend& R) {
    float t = sec(tnow() - tstart);
    dd.p = ortho(0, 1, 0, 1);
    R.clear(C::BLUE);
    for (int i = 0; i < 1000; i ++)
        R.qb.quad_t(amoguses[i].pos, amoguses[i].pos + amoguses[i].size, {sinf(t), cosf(t)});
    R.submit_quads(&dd);
}}