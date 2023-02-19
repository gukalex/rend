#pragma once
#include "rend.h"
#include "imgui/imgui.h"
#include <stdlib.h>
namespace demo_compute {
const u32 WG_SIZE = 256; /*+ copy in the shader*/
u32 compute = 0, input_buffer = 0, output_buffer = 0;
u32* cpu_arr = 0;
u64 count = 0, padded = 0, group_size = 0, res = 0;
i64 diff = 0, sum_diff = 0;
bool use_compute = true, every_frame = false;
void init(rend& R) {
    if (!compute) {
        compute = R.shader(0, 0, R"(#version 450
        #define WG_SIZE 256
        layout (local_size_x = WG_SIZE) in;
        layout (binding = 0, std430) readonly buffer InputBuffer { uint InputData[]; };
        layout (binding = 1, std430) writeonly buffer OutputBuffer { uint OutputData[]; };
        shared uint SharedData[WG_SIZE];
        void main() {
            uint local_id = gl_LocalInvocationID.x;
            uint group_id = gl_WorkGroupID.x;
            SharedData[local_id] = InputData[group_id * gl_WorkGroupSize.x + local_id];
            for (uint i = WG_SIZE / 2; i > 0; i /= 2) { // Reduce in shared memory
                barrier(); // Wait for all threads to finish reducing
                if (local_id < i) {
                    SharedData[local_id] += SharedData[local_id + i];
                }
            }
            if (local_id == 0) OutputData[group_id] = SharedData[0];
        }
        )");
    }
}
void update(rend& R) {
    u64 min = 0, max = 4'000'000'000UL;
    ImGui::Checkbox("Every Frame", &every_frame);
    bool reupdate = ImGui::Checkbox("Use Compute", &use_compute);
    if (reupdate || ImGui::SliderScalar("Array Count", ImGuiDataType_U64, (void*)&count, &min, &max)) {
        if (use_compute) {
            R.free_resources({ {input_buffer, output_buffer} });
            padded = (count + (WG_SIZE - 1)) & ~(WG_SIZE - 1);
            group_size = padded / WG_SIZE;
            input_buffer = R.ssbo(padded * sizeof(u32), 0, true, 1 /*init with 1s*/);
            output_buffer = R.ssbo(group_size * sizeof(u32), 0, true, 0 /*init with 0s*/);
        } else {
            if (cpu_arr) free(cpu_arr);
            cpu_arr = (u32*)malloc(sizeof(u32) * count);
            for (u64 i = 0; i < count; i++) cpu_arr[i] = 1u;
        }
    }
    if (every_frame || ImGui::Button("Test")) {
        i64 start = tnow();
        res = 0;
        if (use_compute) {
            R.dispatch({ compute, (u32)group_size, 1, 1, {input_buffer, output_buffer} });
            R.ssbo_barrier(); // Wait for the compute shader to finish
            u32* out = (u32*)R.map(output_buffer, 0, group_size * sizeof(u32), MAP_READ);
            i64 sum_start = tnow();
            for (u32 i = 0; i < group_size; i++) res += out[i];
            sum_diff = tnow() - sum_start;
            R.unmap(output_buffer);
        } else {
            for (u64 i = 0; i < count; i++) res += cpu_arr[i];
        }
        diff = tnow() - start;
    }
    ImGui::Text("Last test time: %f seconds (%lld nano), res: %lu", sec(diff), diff, res);
    ImGui::Text("Sum diff: %f seconds (%lld nano)", sec(sum_diff), sum_diff);
    R.clear({});
}
}
