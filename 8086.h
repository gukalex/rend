#pragma once
#include "rend.h"
#include "imgui/imgui.h"
#include <stdio.h> // snprintf

#define BIN_FMT "%c%c%c%c%c%c%c%c"
#define BIN(byte)  (byte & 0x80 ? '1' : '0'), (byte & 0x40 ? '1' : '0'), \
                   (byte & 0x20 ? '1' : '0'), (byte & 0x10 ? '1' : '0'), \
                   (byte & 0x08 ? '1' : '0'), (byte & 0x04 ? '1' : '0'), \
                   (byte & 0x02 ? '1' : '0'), (byte & 0x01 ? '1' : '0') 

namespace cpu_sim {

#define MAX_INSTR_BYTES 6
enum ins_type {
    MOV_RR,
    MOV_IM_TO_RR_8,
    MOV_IM_TO_RR_16,
};
struct ins_desc { 
    ins_type type; // can be removed if we force strict order in ins_descs
    u16 opcode;
    u16 opcode_mask;
    u8 size;
    u8 reg0_offset;
    u8 reg1_offset;
    u8 is_whole_offset;
    u8 dir_offset;
    u8 data_offset;
    const char* description;
    const char* asm_name;
};
#define NOT_USED 0xFF
ins_desc ins_descs[] = {
    {MOV_RR, 0b11000000'10001000, 0b11000000'11111100, 2, 11, 8, 0, 1, NOT_USED, // MOD = 0b11
        "MOV register to register", "mov"},
    {MOV_IM_TO_RR_8,         0b10110000,          0b11111000, 2, 0,  NOT_USED, 3, NOT_USED, 8,
        "MOV immediate to register 8 bits", "mov"},
    {MOV_IM_TO_RR_16,        0b10111000,          0b11111000, 3, 0,  NOT_USED, 3, NOT_USED, 8,
        "MOV immediate to register 16 bits", "mov"}
};

#define REG_COUNT 8
struct register_file {
    union {
        u16 wide[REG_COUNT];
        u8 raw[REG_COUNT * 2];
        u8 half[REG_COUNT];
    };
};
#define REG_HALF_MEMORY_ORDER 2
const char* reg[3][REG_COUNT] = { {"al", "cl", "dl", "bl", "ah", "ch", "dh", "bh"},
                         { "ax", "cx", "dx", "bx", "sp", "bp", "si", "di" },
                         {"al", "ah", "cl", "ch", "dl", "dh", "bl", "bh"} };

register_file regs;
buffer ins_file, asm_file;
u8* curr_instr = NULL;
u8* prev_instr = NULL; // not needed?
struct {
    ins_desc* desc = NULL; // todo: just copy?
    bool is_whole = false;
    bool dir = false;
    u16 data = 0;
    u8 reg0 = 0, reg1 = 0;
} curr; // todo: put everything in for easier reset + don't keep static?
#define MAX_FILENAME 128
char bin_filename[MAX_FILENAME] = "exmp1";
char asm_filename[MAX_FILENAME] = "exmp1.asm";
#define MAX_STR 1024
#define HEX_STR_PAD 6
#define BIN_STR_PAD 10
char hex_str[MAX_STR];
char bin_str[MAX_STR];
char disasm[MAX_STR];
int disasm_curr = 0;

void init(rend& R) {
    //if (first_time)
    memset(&regs, 0, sizeof(regs));
    ins_file = read_file(bin_filename, MAX_INSTR_BYTES); // padding for the last instruction
    asm_file = read_file(asm_filename, 2048, true); // + alloc buffer for editing
    prev_instr = curr_instr = ins_file.data;
    curr = {};
    memset(hex_str, 0, MAX_STR);
    memset(bin_str, 0, MAX_STR);
    memset(disasm, 0, MAX_STR);
    char disasm_header[] = "; disasm\nbits 16\n";
    memcpy(disasm, disasm_header, sizeof(disasm_header));
    disasm_curr = sizeof(disasm_header) - 1; // - \0
    // build hex string 
    int hex_offset = 0;
    int bin_offset = 0;
    for (int i = 0; i < ins_file.size; i++) {
        if (hex_offset < MAX_STR)
            hex_offset += snprintf(hex_str + hex_offset, MAX_STR - hex_offset, "%c%c0x%X",
                (i % 8 == 0 ? '\n' : ' '), (i == 0 ? '>' : ' '), ins_file.data[i]);
        if (bin_offset < MAX_STR)
            bin_offset += snprintf(bin_str + bin_offset, MAX_STR - bin_offset, "%c%c" BIN_FMT,
                (i % 4 == 0 ? '\n' : ' '), (i == 0 ? '>' : ' '), BIN(ins_file.data[i]));
    }
}

void update(rend& R) {
    using namespace ImGui;
    Begin("8086");
    if (Button("Reset")) { init(R); } Separator();
    if (Button("Load")) init(R); SameLine(); InputText("##asm_file", asm_filename, MAX_FILENAME);
    InputTextMultiline("##src", (c8*)asm_file.data, asm_file.size, ImVec2(-FLT_MIN, ImGui::GetTextLineHeight() * 12), ImGuiInputTextFlags_AllowTabInput);
    Separator();
    if (Button("Load##2")) init(R); SameLine(); InputText("##bin_file", bin_filename, MAX_FILENAME);
    u32 flags = ImGuiInputTextFlags_ReadOnly;
    InputTextMultiline("##hex", hex_str, MAX_STR, ImVec2(-FLT_MIN, ImGui::GetTextLineHeight() * 4), flags);
    Separator();
    InputTextMultiline("##bin", bin_str, MAX_STR, ImVec2(-FLT_MIN, ImGui::GetTextLineHeight() * 5), flags);
    Separator();

    bool instructions_left = ((u64)(curr_instr - ins_file.data) < ins_file.size);
    if (ins_file.data && instructions_left && Button(">Step") /* todo: use inactive UI style if false */) {
        curr = {};
        u64 instr_wide = 0; // might not use all bytes, 6 is maximum
        memcpy(&instr_wide, curr_instr, MAX_INSTR_BYTES); // instr_wide = *(u64*)curr_instr; analigned read?
        u8 instr_size = 0;
        bool found = false;
        for (int i = 0; i < ARSIZE(ins_descs); i++) { // main decode loop
            u16 masked = instr_wide & ins_descs[i].opcode_mask;
            if (ins_descs[i].opcode == masked) {
                curr.desc = &ins_descs[i];
                instr_size = ins_descs[i].size;
                curr.reg0 = (instr_wide >> ins_descs[i].reg0_offset) & 0b111;
                curr.is_whole = instr_wide & (1ull << ins_descs[i].is_whole_offset);
                if (ins_descs[i].dir_offset != NOT_USED)
                    curr.dir = instr_wide & (1ull << ins_descs[i].dir_offset);
                if (ins_descs[i].reg1_offset != NOT_USED)
                    curr.reg1 = (instr_wide >> ins_descs[i].reg1_offset) & 0b111;
                if (ins_descs[i].data_offset != NOT_USED)
                    curr.data = (instr_wide >> ins_descs[i].data_offset) & (curr.is_whole ? 0xFFFF : 0xFF);
                found = true;
                break;
            }
        }
        if (found) {
            prev_instr = curr_instr;
            curr_instr += instr_size;

            // disassembly 
            const u64 TMP_SIZE = 64;
            char tmp_asm[TMP_SIZE] = "\0"; // todo: write in the dst directly
            int asm_size = 0;
            switch (curr.desc->type) { // todo: how to generalize it?
            case MOV_RR: {
                const char* dst = curr.dir ? reg[curr.is_whole][curr.reg0] : reg[curr.is_whole][curr.reg1];
                const char* src = curr.dir ? reg[curr.is_whole][curr.reg1] : reg[curr.is_whole][curr.reg0];
                asm_size = snprintf(tmp_asm, TMP_SIZE, "%s %s, %s\n\0", curr.desc->asm_name, dst, src);
            } break;
            case MOV_IM_TO_RR_8:
            case MOV_IM_TO_RR_16: {
                const char* dst = reg[curr.is_whole][curr.reg0];
                u16 value = curr.data;
                asm_size = snprintf(tmp_asm, TMP_SIZE, "%s %s, 0x%X\n\0", curr.desc->asm_name, dst, value);
            } break;
            default: 
                asm_size = snprintf(tmp_asm, TMP_SIZE, "; oops, unsuported instruction :)\n\0");
                break;
            }
            ASSERT(asm_size < TMP_SIZE);
            memcpy(disasm + disasm_curr, tmp_asm, asm_size);
            disasm_curr += asm_size;

            // cpu sim
            switch (curr.desc->type) { // todo: how to generalize it?
            case MOV_RR: {
                if (curr.is_whole)
                    regs.wide[curr.dir ? curr.reg0 : curr.reg1] = regs.wide[curr.dir ? curr.reg1 : curr.reg0];
                else {
                    int dst = curr.dir ? curr.reg0 : curr.reg1;
                    dst = (dst < 4 ? dst * 2 : 2 * dst - 7); // 0-0, 1-2, 2-4, 3-6, 4-1, 5-3, 6-5, 7-7 // WTF?
                    int src = curr.dir ? curr.reg1 : curr.reg0;
                    src = (src < 4 ? src * 2 : 2 * src - 7); // 0-0, 1-2, 2-4, 3-6, 4-1, 5-3, 6-5, 7-7
                    regs.half[dst] = regs.half[src];
                }
            } break;
            case MOV_IM_TO_RR_8:
            case MOV_IM_TO_RR_16: {
                if (curr.is_whole)
                    regs.wide[curr.reg0] = curr.data;
                else {
                    int dst = curr.reg0;
                    dst = (dst < 4 ? dst * 2 : 2 * dst - 7);
                    regs.half[dst] = curr.data & 0xFF;
                }
            } break;
            default: break; // ?
            }

            // debug view update, todo: simplify
            u64 bytes_decoded = (prev_instr - ins_file.data);
            static i64 hex_start_pointer = 1;
            hex_str[hex_start_pointer] = ' ';
            hex_start_pointer = bytes_decoded * HEX_STR_PAD + 1;
            hex_str[hex_start_pointer] = '<'; // clear the pointer
            hex_str[(bytes_decoded + instr_size) * HEX_STR_PAD + 1] = '>'; // place new pointer
            static i64 bin_start_pointer = 1;
            bin_str[bin_start_pointer] = ' ';
            bin_start_pointer = bytes_decoded * BIN_STR_PAD + 1;
            bin_str[bin_start_pointer] = '<'; // clear the pointer
            bin_str[(bytes_decoded + instr_size) * BIN_STR_PAD + 1] = '>'; // place new pointer
        }
        else { /* assert invalid? curr.desc is NULL */ }
    }

    // decoded instruction info
    Text("Decoded Instruction: %s", (curr.desc ? curr.desc->description : "Invalid/Unsupported Instruction"));
    const char* r0 = reg[curr.is_whole][curr.reg0]; // todo: if curr.desc
    const char* r1 = reg[curr.is_whole][curr.reg1]; //
    Text("W: %s, D: %s REG: %s, R/M: %s", curr.is_whole ? "Set" : "Not Set", curr.dir ? "Set" : "Not Set", r0, r1);
    Separator();

    // print register view
    const u32 MAX_REG_VIEW_SIZE = 256;
    char reg_view[MAX_REG_VIEW_SIZE] = "\0"; // todo: write in the dst directly
    int reg_offset = 0;
    for (int i = 0; i < REG_COUNT; i++) {
        if (i && i % 4 == 0) reg_view[reg_offset++] = '\n';
        reg_offset += snprintf(reg_view + reg_offset, MAX_REG_VIEW_SIZE - reg_offset, 
            " %s: 0x%04X", reg[1][i], regs.wide[i]);
    }
    reg_view[reg_offset++] = '\n'; reg_view[reg_offset++] = '\n';
    for (int i = 0; i < REG_COUNT; i++) { // todo: merge
        if (i && i % 4 == 0) reg_view[reg_offset++] = '\n';
        reg_offset += snprintf(reg_view + reg_offset, MAX_REG_VIEW_SIZE - reg_offset,
            " %s: 0x%02X", reg[REG_HALF_MEMORY_ORDER][i], regs.raw[i]);
    }
    reg_view[reg_offset++] = '\n'; reg_view[reg_offset++] = '\n';
    // print char view
    memcpy(reg_view + reg_offset, " Char View: ", sizeof(" Char View: ") - 1); reg_offset += sizeof(" Char View: ") - 1;
    memcpy(reg_view + reg_offset, regs.raw, REG_COUNT); reg_offset += REG_COUNT;
    reg_view[reg_offset++] = '\0';
    ASSERT(reg_offset < MAX_REG_VIEW_SIZE);
    InputTextMultiline("##reg_view", reg_view, reg_offset, ImVec2(-FLT_MIN, ImGui::GetTextLineHeight() * 8), ImGuiInputTextFlags_ReadOnly);
    Separator();

    InputTextMultiline("##disasm", disasm, MAX_STR, ImVec2(-FLT_MIN, ImGui::GetTextLineHeight() * 9), ImGuiInputTextFlags_ReadOnly);
    static char save_file[MAX_FILENAME] = "disasm.asm";
    if (Button("Save")) { write_file(save_file, {(u8*)disasm, (u64)disasm_curr}); }; 
    SameLine(); InputText("##save_file", save_file, MAX_FILENAME);

    End();
    R.clear({0,0,0,0});
}

}