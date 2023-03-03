#pragma once
#include "rend.h"
#include "imgui/imgui.h"

#define BIN_FMT "%c%c%c%c%c%c%c%c"
#define BIN(byte)  \
  (byte & 0x80 ? '1' : '0'), \
  (byte & 0x40 ? '1' : '0'), \
  (byte & 0x20 ? '1' : '0'), \
  (byte & 0x10 ? '1' : '0'), \
  (byte & 0x08 ? '1' : '0'), \
  (byte & 0x04 ? '1' : '0'), \
  (byte & 0x02 ? '1' : '0'), \
  (byte & 0x01 ? '1' : '0') 

namespace cpu_sim {

struct ins_desc { 
    u8 opcode;
    u8 opcode_mask;
    u8 size;
    u8 reg0_offset;
    u8 reg1_offset;
    u8 is_whole_offset;
    const char* description;
};

#define NOT_USED 0xFF
ins_desc ins_descs[] = {
    {0b10001000, 0b11111100, 2, 11, 8,       0, "MOV register to register"},
    {0b10110000, 0b11111000, 2, 0,  NOT_USED, 3, "MOV immediate to register 8 bits"},
    {0b10111000, 0b11111000, 3, 0,  NOT_USED, 3, "MOV immediate to register 16 bits"}
};

enum reg_t: u8 {
         AL,      CL,      DL,      BL,      AH,      CH,      DH,      BH,
    AX = AL, CX = CL, DX = DL, BX = BL, SP = AH, BP = CH, SI = DH, DI = BH
};

const char* reg_half[] = { "AL", "CL", "DL", "BL", "AH", "CH", "DH", "BH" };
const char* reg_whole[] = { "AX", "CX", "DX", "BX", "SP", "BP", "SI", "DI" };

buffer ins_file, asm_file;
u8* curr_instr = NULL;
u8* prev_instr = NULL;
struct {
    bool is_whole = false;
    u8 reg0 = 0, reg1 = 0;
    const char* instr_name = "FUN";
} curr;

void init(rend& R) {
    ins_file = read_file("exmp1", 6); // 6 is the max instruction lenght
    asm_file = read_file("exmp1.asm", 1);
    prev_instr = curr_instr = ins_file.data;
    curr = {};
}

void update(rend& R) {
    using namespace ImGui;
    Begin("8086");
    TextWrapped((c8*)asm_file.data);
    Separator();

    Text("instruction stream size: %d\n"
         "[i+0]:" BIN_FMT " [i+1]:" BIN_FMT, ins_file.size, BIN(prev_instr[0]), BIN(prev_instr[1]));

    if (Button("Reset")) { init(R); }

    bool instructions_left = ((u64)(curr_instr - ins_file.data) < ins_file.size);
    if (instructions_left && Button("Decode Next")) {
        curr = {};
        u16 instr_wide = *(u16*)curr_instr;
        u8 instr = *curr_instr;
        u8 instr_size = 0;
        bool found = false;
        for (int i = 0; i < ARSIZE(ins_descs); i++) {
            u8 masked = instr & ins_descs[i].opcode_mask;
            if (ins_descs[i].opcode == masked) {
                curr.instr_name = ins_descs[i].description;
                instr_size = ins_descs[i].size;
                // decode registers
                curr.is_whole = instr_wide & (1 << ins_descs[i].is_whole_offset);
                //u32 temp = ((u32)instr_wide >> 1);
                curr.reg0 = (instr_wide >> ins_descs[i].reg0_offset) & 0b111;
                //ImGui::Text("debug: %X, reg0: %X", (u64)instr_wide, (u64)temp);
                if (ins_descs[i].reg1_offset != NOT_USED)
                    curr.reg1 = (instr_wide >> ins_descs[i].reg1_offset) & 0b111;
                found = true;
                break;
            }
        }
        ASSERT(found); // invalid/unsupported instruction
        prev_instr = curr_instr;
        curr_instr += instr_size;
    }

    Text("Instruction: %s", curr.instr_name);
    const char* r0 = curr.is_whole ? reg_whole[curr.reg0] : reg_half[curr.reg0];
    const char* r1 = curr.is_whole ? reg_whole[curr.reg1] : reg_half[curr.reg1];
    Text("W bit: %s, Reg0: %s, Reg1: %s", curr.is_whole ? "Set" : "Not Set", r0, r1);

    End();
    R.clear({0,0,0,0});
}

}