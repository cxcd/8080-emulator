#include <iostream>
#include <iomanip>
#include <bitset>
#include <cstdint>
#include <vector>
#include <fstream>
#include <iterator>

namespace Emu8080 {
	// CPU
	class conditionCodes {
	public:
		uint8_t z, s, p, cy, ac;
		conditionCodes() : z(1), s(1), p(1), cy(0), ac(1) {}
	};

	class registers {
	public:
		uint8_t a, b, c, d, e, h, l;
		uint16_t sp, pc;
		registers() : a(0), b(0), c(0), d(0), e(0), h(0), l(0), sp(0), pc(0) {}
	};

	class state {
	public:
		conditionCodes cc;
		registers r;
		uint8_t enabled = 0;
		std::vector<uint8_t> memory;
		uint16_t temp16 = 0; // Catch-all holder for any 16 bit number needed in operations
		uint8_t temp8 = 0;
		state() {
			memory = std::vector<uint8_t>(0x10000, 0); // Reserve 16KB
		}
	};

	// Exit program when an unimplemented instruction is encountered
	void unimplementedInstruction(uint8_t opcode) {
		std::cout << "Error: Instruction "
			<< std::uppercase << std::hex << std::setw(2) << std::setfill('0')
			<< (int)opcode << " is unimplemented\n";
	}

	// Print CPU state
	void printState(state *s, uint8_t opcode, uint16_t data) {
		std::cout << "PC: " <<  s->r.pc << " Opcode: "
			<< std::uppercase << std::hex << std::setw(2) << std::setfill('0') << (int)opcode
			<< " Data: " << data 
			<< "\n"
			<< "SP:" << std::uppercase << std::hex << std::setw(2) << std::setfill('0') << (s->r.sp) << "\n"
			<< "Z:" << std::bitset<1>(s->cc.z)
			<< " S:" << std::bitset<1>(s->cc.s)
			<< " P:" << std::bitset<1>(s->cc.p)
			<< " CY:" << std::bitset<1>(s->cc.cy)
			<< " AC:" << std::bitset<1>(s->cc.ac) 
			<< "\n"
			<< "A:" << std::bitset<8>(s->r.a)
			<< " B:" << std::bitset<8>(s->r.b)
			<< " C:" << std::bitset<8>(s->r.c)
			<< "\nD:" << std::bitset<8>(s->r.d)
			<< " E:" << std::bitset<8>(s->r.e)
			<< " H:" << std::bitset<8>(s->r.h)
			<< " L:" << std::bitset<8>(s->r.l) 
			<< "\n\n";
	}

	// Reading file into memory
	void readFile(state *s, const std::string &path) {
		// Get file
		std::ifstream file(path, std::ios::binary);
		file.unsetf(std::ios::skipws); // Skip whitespace
		// Fill data
		s->memory.insert(s->memory.begin(), std::istream_iterator<uint8_t>(file), std::istream_iterator<uint8_t>());
	}

	// Operations

	// Check parity
	uint8_t parity(uint16_t x, uint16_t size) {
		int i;
		int p = 0;
		x = x & ((1 << size) - 1);
		for (i = 0; i < size; i++) {
			if (x & 0x01) {
				p++;
			}
			x = x >> 1;
		}
		return (p & 0x01) == 0;
	}

	// Check carry 16 bit
	void checkCarry16(state *s, uint16_t result) {
		s->cc.cy = (result & 0xFF00) > 0;
	}
	// Check carry 32 bit
	void checkCarry32(state *s, uint32_t result) {
		s->cc.cy = (result & 0xFFFF0000) > 0;
	}

	// Check flags
	void checkFlags(state *s, uint16_t result, bool checkCY) {
		s->cc.z = (result & 0xFF) == 0; // Check if equal to zero
		s->cc.s = (result & 0x80) == 0x80; // Check if negative (msb is set)
		s->cc.p = parity(result, 0xFF); // Check parity
		if (checkCY) {
			checkCarry16(s, result);
		}
		s->cc.ac = result >= 0x0F; // Check half carry
	}

	// Add value to 8 bit register
	void add8(state *s, uint8_t &reg, uint8_t val, bool cy) {
		uint16_t result = (uint16_t)reg + (uint16_t)val;
		reg = result & 0xFF;
		checkFlags(s, result, cy);
	}
	// Add value to 16 bit register as two 8 bit registers
	void add16(uint8_t &reg1, uint8_t &reg2, uint8_t val) {
		uint16_t result = (reg1 << 8 | reg2) + val;
		reg1 = result >> 8;
		reg2 = result & 0xFF;
	}
	// Add 16 bit register to 16 bit register as 8 bit registers
	void add32_8(state *s, uint8_t &reg1, uint8_t &reg2, uint8_t &reg3, uint8_t &reg4) {
		uint32_t reg12 = (reg1 << 8) | reg2;
		uint32_t reg34 = (reg3 << 8) | reg4;
		uint32_t result = reg12 + reg34;
		reg1 = (result & 0xFF00) >> 8;
		reg2 = result & 0xFF;
		checkCarry32(s, result);
	}
	// Add 16 bit register to 16 bit register as 8 bit registers and a 16 bit register
	void add32_16(state *s, uint8_t &reg1, uint8_t &reg2, uint16_t &reg3) {
		uint32_t reg12 = (reg1 << 8) | reg2;
		uint32_t result = reg12 + reg3;
		reg1 = (result & 0xFF00) >> 8;
		reg2 = result & 0xFF;
		checkCarry32(s, result);
	}
	// Add value and carry to 8 bit register
	void adc(state *s, uint8_t &reg, uint8_t val, bool cy) {
		uint16_t result = (uint16_t)reg + (uint16_t)val + s->cc.cy;
		reg = result & 0xFF;
		checkFlags(s, result, cy);
	}

	// Subtract value from 8 bit register
	void sub8(state *s, uint8_t &reg, uint8_t val, bool cy) {
		uint16_t result = (uint16_t)reg - (uint16_t)val;
		reg = result & 0xFF;
		checkFlags(s, result, cy);
	}
	// Subtract value from 16 bit register
	void sub16(uint8_t &reg1, uint8_t &reg2, uint8_t val) {
		uint16_t result = (reg1 << 8 | reg2) - val;
		reg1 = result >> 8;
		reg2 = result & 0xFF;
	}
	// Subtract value and carry from 8 bit register
	void sbb(state *s, uint8_t &reg, uint8_t val, bool cy) {
		uint16_t result = (uint16_t)reg - (uint16_t)val - s->cc.cy;
		reg = result & 0xFF;
		checkFlags(s, result, cy);
	}

	// AND value from 8 bit register
	void ana(state *s, uint8_t &reg, uint8_t val) {
		uint16_t result = (uint16_t)reg & (uint16_t)val;
		reg = result & 0xFF;
		checkFlags(s, result, true);
	}
	// XOR value from 8 bit register
	void xra(state *s, uint8_t &reg, uint8_t val) {
		uint16_t result = (uint16_t)reg ^ (uint16_t)val;
		reg = result & 0xFF;
		checkFlags(s, result, true);
	}
	// OR value from 8 bit register
	void ora(state *s, uint8_t &reg, uint8_t val) {
		uint16_t result = (uint16_t)reg | (uint16_t)val;
		reg = result & 0xFF;
		checkFlags(s, result, true);
	}

	// Move 8 bit register to 8 bit register
	void mov8(uint8_t &reg1, uint8_t &reg2) {
		reg1 = reg2;
	}
	// Move 8 bit register to/from register at location HL
	void movHL(state *s, uint8_t &reg, bool toHL) {
		s->temp16 = (s->r.h << 8) | s->r.l;
		if (toHL) {
			s->memory[s->temp16] = reg;
		} else {
			reg = s->memory[s->temp16];
		}
	}

	// Compare register with accumulator
	void cmp(state *s, uint8_t &reg) {
		uint16_t result = (uint16_t)s->r.a - (uint16_t)reg;
		checkFlags(s, result, true);
	}

	// Push to stack
	void push(state *s, uint8_t &reg1, uint8_t &reg2) {
		s->memory[s->r.sp - 1] = reg1;
		s->memory[s->r.sp - 2] = reg2;
		s->r.sp -= 2;
	}
	// Pop from stack
	void pop(state *s, uint8_t &reg1, uint8_t &reg2) {
		reg2 = s->memory[s->r.sp];
		reg1 = s->memory[s->r.sp + 1];
		s->r.sp += 2;
	}

	// Return
	void ret(state *s) {
		s->r.pc = s->memory[s->r.sp] | (s->memory[s->r.sp + 1] << 8);
		s->r.sp += 2;
	}

	// Call adr
	void call(state *s, uint8_t *reg) {
		s->temp16 = s->r.pc + 2;
		s->memory[s->r.sp - 1] = (s->temp16 >> 8) & 0xff;
		s->memory[s->r.sp - 2] = (s->temp16 & 0xff);
		s->r.sp = s->r.sp - 2;
		s->r.pc = ((reg[2] << 8) | reg[1]) - 1; // -1 to account for PC + 1 at the end of switch
	}

	// Jump adr
	void jump(state* s, uint8_t *opcode) {
		// -1 to account for PC + 1 at the end of switch
		s->r.pc = ((opcode[2] << 8) | opcode[1]) - 1;
	}

	// Parse code and execute instruction
	void emulate8080(state *s) {
		// Get the current instruction from the program counter
		uint8_t *opcode = &s->memory[s->r.pc];
		// Check the instruction and execute it
		switch (*opcode) {
		case 0x00: // NOP
			break;
		case 0x01: // LXI B, D16
			s->r.c = opcode[1];
			s->r.b = opcode[2];
			s->r.pc += 2;
			break;
		case 0x02: // STAX B
			s->temp16 = (s->r.b << 8) | s->r.c;
			s->memory[s->temp16] = s->r.a;
			break;
		case 0x03: // INX B
			add16(s->r.b, s->r.c, (uint8_t)1);
			break;
		case 0x04: // INR B
			add8(s, s->r.b, (uint8_t)1, false);
			break;
		case 0x05: // DCR B
			sub8(s, s->r.b, (uint8_t)1, false);
			break;
		case 0x06: // MVI B, D8
			s->r.b = opcode[1];
			s->r.pc++;
			break;
		case 0x07: // RLC
			s->cc.cy = (s->r.a >> 7) & 1;
			s->temp16 = (uint16_t)s->cc.cy;
			s->r.a = (s->r.a << 1) | (uint8_t)s->temp16;
			break;
		case 0x08: // -
			break;
		case 0x09: // DAD B
			add32_8(s, s->r.h, s->r.l, s->r.b, s->r.c);
			break;
		case 0x0A: // LDAX B
			s->temp16 = (s->r.b << 8) | s->r.c;
			s->r.a = s->memory[s->temp16];
			break;
		case 0x0B: // DCX B
			sub16(s->r.b, s->r.c, (uint8_t)1);
			break;
		case 0x0C: // INR C
			add8(s, s->r.c, (uint8_t)1, false);
			break;
		case 0x0D: // DCR C
			sub8(s, s->r.c, (uint8_t)1, false);
			break;
		case 0x0E: // MVI C, D8
			s->r.c = opcode[1];
			s->r.pc++;
			break;
		case 0x0F: // RRC
			s->cc.cy = s->r.a & 1;
			s->temp16 = s->cc.cy;
			s->r.a = (s->r.a >> 1) | (uint8_t)(s->temp16 << 7);
			break;
		case 0x10: // -
			break;
		case 0x11: // LXI D, D16
			s->r.d = opcode[1];
			s->r.e = opcode[2];
			s->r.pc += 2;
			break;
		case 0x12: // STAX D
			s->temp16 = (s->r.d << 8) | s->r.e;
			s->memory[s->temp16] = s->r.a;
			break;
		case 0x13: // INX D
			add16(s->r.d, s->r.e, (uint8_t)1);
			break;
		case 0x14: // INR D
			add8(s, s->r.d, (uint8_t)1, false);
			break;
		case 0x15: // DCR D
			sub8(s, s->r.d, (uint8_t)1, false);
			break;
		case 0x16: // MVI D, D8
			s->r.d = opcode[1];
			s->r.pc++;
			break;
		case 0x17: // RAL
			s->temp16 = s->cc.cy;
			s->cc.cy = (s->r.a >> 7) & 1;
			s->r.a = (s->r.a << 1) | (uint8_t)s->temp16;
			break;
		case 0x18: // -
			break;
		case 0x19: // DAD D
			add32_8(s, s->r.h, s->r.l, s->r.d, s->r.e);
			break;
		case 0x1A: // LDAX D
			s->temp16 = (s->r.d << 8) | s->r.e;
			s->r.a = s->memory[s->temp16];
			break;
		case 0x1B: // DCX D
			sub16(s->r.d, s->r.e, (uint8_t)1);
			break;
		case 0x1C: // INR E
			add8(s, s->r.e, (uint8_t)1, false);
			break;
		case 0x1D: // DCR E
			sub8(s, s->r.e, (uint8_t)1, false);
			break;
		case 0x1E: // MVI E, D8
			s->r.e = opcode[1];
			s->r.pc++;
			break;
		case 0x1F: // RAR
			s->cc.cy = s->r.a & 1;
			s->temp16 = (uint16_t)s->r.a;
			s->r.a = (s->r.a >> 1) | (uint8_t)(s->temp16 << 7);
			break;
		case 0x20: // -
			break;
		case 0x21: // LXI H, D16
			s->r.l = opcode[1];
			s->r.h = opcode[2];
			s->r.pc += 2;
			break;
		case 0x22: // SHLD adr
			s->temp16 = (opcode[2] << 8) | opcode[1];
			s->memory[s->temp16] = s->r.l;
			s->memory[s->temp16++] = s->r.h;
			s->r.pc += 2;
			break;
		case 0x23: // INX H
			add16(s->r.h, s->r.l, (uint8_t)1);
			break;
		case 0x24: // INR H
			add8(s, s->r.h, (uint8_t)1, false);
			break;
		case 0x25: // DCR H
			sub8(s, s->r.h, (uint8_t)1, false);
			break;
		case 0x26: // MVI H, D8
			s->r.h = opcode[1];
			s->r.pc++;
			break;
		case 0x27: // DAA - special
			unimplementedInstruction(*opcode); break;
		case 0x28: // -
			break;
		case 0x29: // DAD H
			add32_8(s, s->r.h, s->r.l, s->r.h, s->r.l);
			break;
		case 0x2A: // LHLD adr
			s->temp16 = (opcode[2] << 8) | opcode[1];
			s->r.l = s->memory[s->temp16];
			s->r.h = s->memory[s->temp16++];
			s->r.pc += 2;
			break;
		case 0x2B: // DCX H
			sub16(s->r.h, s->r.l, (uint8_t)1);
			break;
		case 0x2C: // INR L
			add8(s, s->r.l, (uint8_t)1, false);
			break;
		case 0x2D: // DCR L
			sub8(s, s->r.l, (uint8_t)1, false);
			break;
		case 0x2E: // MVI L, D8
			s->r.l = opcode[1];
			s->r.pc++;
			break;
		case 0x2F: // CMA
			s->r.a = ~s->r.a;
			break;
		case 0x30: // -
			break;
		case 0x31: // LXI SP, D16
			s->r.sp = (opcode[2] << 8) | opcode[1];
			s->r.pc += 2;
			break;
		case 0x32: // STA adr
			s->memory[(opcode[2] << 8) | opcode[1]] = s->r.a;
			break;
		case 0x33: // INX SP
			s->r.sp++;
			break;
		case 0x34: // INR M
			s->temp16 = (s->r.h << 8) | s->r.l;
			add8(s, s->memory[s->temp16], (uint8_t)1, false);
			break;
		case 0x35: // DCR M
			s->temp16 = (s->r.h << 8) | s->r.l;
			sub8(s, s->memory[s->temp16], (uint8_t)1, false);
			break;
		case 0x36: // MVI M, D8
			s->temp16 = (s->r.h << 8) | s->r.l;
			s->memory[s->temp16] = opcode[1];
			s->r.pc++;
			break;
		case 0x37: // STC
			s->cc.cy = 1;
			break;
		case 0x38: // -
			break;
		case 0x39: // DAD SP
			add32_16(s, s->r.h, s->r.l, s->r.sp);
			break;
		case 0x3A: // LDA adr
			s->temp16 = (opcode[2] << 8) | opcode[1];
			s->r.a = s->memory[s->temp16];
			break;
		case 0x3B: // DCX SP
			s->r.sp--;
			break;
		case 0x3C: // INR A
			add8(s, s->r.a, (uint8_t)1, false);
			break;
		case 0x3D: // DCR A
			sub8(s, s->r.a, (uint8_t)1, false);
			break;
		case 0x3E: // MVI A, D8
			s->r.a = opcode[1];
			s->r.pc++;
			break;
		case 0x3F: // CMC
			s->cc.cy = ~s->cc.cy;
			break;
		case 0x40: // MOV B, B
			mov8(s->r.b, s->r.b);
			break;
		case 0x41: // MOV B, C
			mov8(s->r.b, s->r.c);
			break;
		case 0x42: // MOV B, D
			mov8(s->r.b, s->r.d);
			break;
		case 0x43: // MOV B, E
			mov8(s->r.b, s->r.e);
			break;
		case 0x44: // MOV B, H
			mov8(s->r.b, s->r.h);
			break;
		case 0x45: // MOV B, L
			mov8(s->r.b, s->r.l);
			break;
		case 0x46: // MOV B, M
			movHL(s, s->r.b, false);
			break;
		case 0x47: // MOV B, A
			mov8(s->r.b, s->r.a);
			break;
		case 0x48: // MOV C, B
			mov8(s->r.c, s->r.b);
			break;
		case 0x49: // MOV C, C
			mov8(s->r.c, s->r.c);
			break;
		case 0x4A: // MOV C, D
			mov8(s->r.c, s->r.d);
			break;
		case 0x4B: // MOV C, E
			mov8(s->r.c, s->r.e);
			break;
		case 0x4C: // MOV C, H
			mov8(s->r.c, s->r.h);
			break;
		case 0x4D: // MOV C, L
			mov8(s->r.c, s->r.l);
			break;
		case 0x4E: // MOV C, M
			movHL(s, s->r.c, false);
			break;
		case 0x4F: // MOV C, A
			mov8(s->r.c, s->r.a);
			break;
		case 0x50: // MOV D, B
			mov8(s->r.d, s->r.b);
			break;
		case 0x51: // MOV D, C
			mov8(s->r.d, s->r.c);
			break;
		case 0x52: // MOV D, D
			mov8(s->r.d, s->r.d);
			break;
		case 0x53: // MOV D, E
			mov8(s->r.d, s->r.e);
			break;
		case 0x54: // MOV D, H
			mov8(s->r.d, s->r.h);
			break;
		case 0x55: // MOV D, L
			mov8(s->r.d, s->r.l);
			break;
		case 0x56: // MOV D, M
			movHL(s, s->r.d, false);
			break;
		case 0x57: // MOV D, A
			mov8(s->r.d, s->r.a);
			break;
		case 0x58: // MOV E, B
			mov8(s->r.e, s->r.b);
			break;
		case 0x59: // MOV E, C
			mov8(s->r.e, s->r.c);
			break;
		case 0x5A: // MOV E, D
			mov8(s->r.e, s->r.d);
			break;
		case 0x5B: // MOV E, E
			mov8(s->r.e, s->r.e);
			break;
		case 0x5C: // MOV E, H
			mov8(s->r.e, s->r.h);
			break;
		case 0x5D: // MOV E, L
			mov8(s->r.e, s->r.l);
			break;
		case 0x5E: // MOV E, M
			movHL(s, s->r.e, false);
			break;
		case 0x5F: // MOV E, A
			mov8(s->r.e, s->r.a);
			break;
		case 0x60: // MOV H, B
			mov8(s->r.h, s->r.b);
			break;
		case 0x61: // MOV H, C
			mov8(s->r.h, s->r.c);
			break;
		case 0x62: // MOV H, D
			mov8(s->r.h, s->r.d);
			break;
		case 0x63: // MOV H, E
			mov8(s->r.h, s->r.e);
			break;
		case 0x64: // MOV H, H
			mov8(s->r.h, s->r.h);
			break;
		case 0x65: // MOV H, L
			mov8(s->r.h, s->r.l);
			break;
		case 0x66: // MOV H, M
			movHL(s, s->r.h, false);
			break;
		case 0x67: // MOV H, A
			mov8(s->r.h, s->r.a);
			break;
		case 0x68: // MOV L, B
			mov8(s->r.l, s->r.b);
			break;
		case 0x69: // MOV L, C
			mov8(s->r.l, s->r.c);
			break;
		case 0x6A: // MOV L, D
			mov8(s->r.l, s->r.d);
			break;
		case 0x6B: // MOV L, E
			mov8(s->r.l, s->r.e);
			break;
		case 0x6C: // MOV L, H
			mov8(s->r.l, s->r.h);
			break;
		case 0x6D: // MOV L, L
			mov8(s->r.l, s->r.l);
			break;
		case 0x6E: // MOV L, M
			movHL(s, s->r.l, false);
			break;
		case 0x6F: // MOV M, A
			mov8(s->r.l, s->r.a);
			break;
		case 0x70: // MOV M, B
			movHL(s, s->r.b, true);
			break;
		case 0x71: // MOV M, C
			movHL(s, s->r.c, true);
			break;
		case 0x72: // MOV M, D
			movHL(s, s->r.d, true);
			break;
		case 0x73: // MOV M, E
			movHL(s, s->r.e, true);
			break;
		case 0x74: // MOV M, H
			movHL(s, s->r.h, true);
			break;
		case 0x75: // MOV M, L
			movHL(s, s->r.l, true);
			break;
		case 0x76: // HLT - special
			unimplementedInstruction(*opcode); break;
		case 0x77: // MOV M, A
			movHL(s, s->r.a, true);
			break;
		case 0x78: // MOV A, B
			mov8(s->r.a, s->r.b);
			break;
		case 0x79: // MOV A, C
			mov8(s->r.a, s->r.c);
			break;
		case 0x7A: // MOV A, D
			mov8(s->r.a, s->r.d);
			break;
		case 0x7B: // MOV A, E
			mov8(s->r.a, s->r.e);
			break;
		case 0x7C: // MOV A, H
			mov8(s->r.a, s->r.h);
			break;
		case 0x7D: // MOV A, L
			mov8(s->r.a, s->r.l);
			break;
		case 0x7E: // MOV A, M
			movHL(s, s->r.a, false);
			break;
		case 0x7F: // MOV A, A
			mov8(s->r.a, s->r.a);
			break;
		case 0x80: // ADD B
			add8(s, s->r.a, s->r.b, true);
			break;
		case 0x81: // ADD C
			add8(s, s->r.a, s->r.c, true);
			break;
		case 0x82: // ADD D
			add8(s, s->r.a, s->r.d, true);
			break;
		case 0x83: // ADD E
			add8(s, s->r.a, s->r.e, true);
			break;
		case 0x84: // ADD H
			add8(s, s->r.a, s->r.h, true);
			break;
		case 0x85: // ADD L
			add8(s, s->r.a, s->r.l, true);
			break;
		case 0x86: // ADD M
			s->temp16 = (s->r.h << 8) | s->r.l;
			add8(s, s->r.a, s->memory[s->temp16], true);
			break;
		case 0x87: // ADD A
			add8(s, s->r.a, s->r.a, true);
			break;
		case 0x88: // ADC B
			adc(s, s->r.a, s->r.b, true);
			break;
		case 0x89: // ADC C
			adc(s, s->r.a, s->r.c, true);
			break;
		case 0x8A: // ADC D
			adc(s, s->r.a, s->r.d, true);
			break;
		case 0x8B: // ADC E
			adc(s, s->r.a, s->r.e, true);
			break;
		case 0x8C: // ADC H
			adc(s, s->r.a, s->r.h, true);
			break;
		case 0x8D: // ADC L
			adc(s, s->r.a, s->r.l, true);
			break;
		case 0x8E: // ADC M
			s->temp16 = (s->r.h << 8) | s->r.l;
			adc(s, s->r.a, s->memory[s->temp16], true);
			break;
		case 0x8F: // ADC A
			adc(s, s->r.a, s->r.a, true);
			break;
		case 0x90: // SUB B
			sub8(s, s->r.a, s->r.b, true);
			break;
		case 0x91: // SUB C
			sub8(s, s->r.a, s->r.c, true);
			break;
		case 0x92: // SUB D
			sub8(s, s->r.a, s->r.d, true);
			break;
		case 0x93: // SUB E
			sub8(s, s->r.a, s->r.e, true);
			break;
		case 0x94: // SUB H
			sub8(s, s->r.a, s->r.h, true);
			break;
		case 0x95: // SUB L
			sub8(s, s->r.a, s->r.l, true);
			break;
		case 0x96: // SUB M
			s->temp16 = (s->r.h << 8) | s->r.l;
			sub8(s, s->r.a, s->memory[s->temp16], true);
			break;
		case 0x97: // SUB A
			sub8(s, s->r.a, s->r.a, true);
			break;
		case 0x98: // SBB B
			sbb(s, s->r.a, s->r.b, true);
			break;
		case 0x99: // SBB C
			sbb(s, s->r.a, s->r.c, true);
			break;
		case 0x9A: // SBB D
			sbb(s, s->r.a, s->r.d, true);
			break;
		case 0x9B: // SBB E
			sbb(s, s->r.a, s->r.e, true);
			break;
		case 0x9C: // SBB H
			sbb(s, s->r.a, s->r.h, true);
			break;
		case 0x9D: // SBB L
			sbb(s, s->r.a, s->r.l, true);
			break;
		case 0x9E: // SBB M
			s->temp16 = (s->r.h << 8) | s->r.l;
			sbb(s, s->r.a, s->memory[s->temp16], true);
			break;
		case 0x9F: // SBB A
			sbb(s, s->r.a, s->r.a, true);
			break;
		case 0xA0: // ANA B
			ana(s, s->r.a, s->r.b);
			break;
		case 0xA1: // ANA C
			ana(s, s->r.a, s->r.c);
			break;
		case 0xA2: // ANA D
			ana(s, s->r.a, s->r.d);
			break;
		case 0xA3: // ANA E
			ana(s, s->r.a, s->r.e);
			break;
		case 0xA4: // ANA H
			ana(s, s->r.a, s->r.h);
			break;
		case 0xA5: // ANA L
			ana(s, s->r.a, s->r.l);
			break;
		case 0xA6: // ANA M
			s->temp16 = (s->r.h << 8) | s->r.l;
			ana(s, s->r.a, s->memory[s->temp16]);
			break;
		case 0xA7: // ANA A
			ana(s, s->r.a, s->r.a);
			break;
		case 0xA8: // XRA B
			xra(s, s->r.a, s->r.b);
			break;
		case 0xA9: // XRA C
			xra(s, s->r.a, s->r.c);
			break;
		case 0xAA: // XRA D
			xra(s, s->r.a, s->r.d);
			break;
		case 0xAB: // XRA E
			xra(s, s->r.a, s->r.e);
			break;
		case 0xAC: // XRA H
			xra(s, s->r.a, s->r.h);
			break;
		case 0xAD: // XRA L
			xra(s, s->r.a, s->r.l);
			break;
		case 0xAE: // XRA M
			s->temp16 = (s->r.h << 8) | s->r.l;
			xra(s, s->r.a, s->memory[s->temp16]);
			break;
		case 0xAF: // XRA A
			xra(s, s->r.a, s->r.a);
			break;
		case 0xB0: // ORA B
			ora(s, s->r.a, s->r.b);
			break;
		case 0xB1: // ORA C
			ora(s, s->r.a, s->r.c);
			break;
		case 0xB2: // ORA D
			ora(s, s->r.a, s->r.d);
			break;
		case 0xB3: // ORA E
			ora(s, s->r.a, s->r.e);
			break;
		case 0xB4: // ORA H
			ora(s, s->r.a, s->r.h);
			break;
		case 0xB5: // ORA L
			ora(s, s->r.a, s->r.l);
			break;
		case 0xB6: // ORA M
			s->temp16 = (s->r.h << 8) | s->r.l;
			ora(s, s->r.a, s->memory[s->temp16]);
			break;
		case 0xB7: // ORA A
			ora(s, s->r.a, s->r.a);
			break;
		case 0xB8: // CMP B
			cmp(s, s->r.b);
			break;
		case 0xB9: // CMP C
			cmp(s, s->r.c);
			break;
		case 0xBA: // CMP D
			cmp(s, s->r.d);
			break;
		case 0xBB: // CMP E
			cmp(s, s->r.e);
			break;
		case 0xBC: // CMP H
			cmp(s, s->r.h);
			break;
		case 0xBD: // CMP L
			cmp(s, s->r.l);
			break;
		case 0xBE: // CMP M
			s->temp16 = (s->r.h << 8) | s->r.l;
			cmp(s, s->memory[s->temp16]);
			break;
		case 0xBF: // CMP A
			cmp(s, s->r.a);
			break;
		case 0xC0: // RNZ
			if (!s->cc.z) {
				ret(s);
			}
			break;
		case 0xC1: // POP B
			pop(s, s->r.b, s->r.c);
			break;
		case 0xC2: // JNZ adr
			if (s->cc.z) {
				jump(s, opcode);
			} else {
				s->r.pc += 2;
			}
			break;
		case 0xC3: // JMP adr
			jump(s, opcode);
			break;
		case 0xC4: // CNZ adr
			if (!s->cc.z) {
				call(s, opcode);
			}
			break;
		case 0xC5: // PUSH B
			push(s, s->r.b, s->r.c);
			break;
		case 0xC6: // ADI D8
			add8(s, s->r.a, opcode[1], true);
			break;
		case 0xC7: // RST 0
			// TODO this is a special instruction, this subroutine might need to be implemented
			call(s, (uint8_t*)0x00);
			break;
		case 0xC8: // RZ
			if (s->cc.z) {
				ret(s);
			}
			break;
		case 0xC9: // RET
			ret(s);
			break;
		case 0xCA: // JZ adr
			if (s->cc.z) {
				jump(s, opcode);
			} else {
				s->r.pc += 2;	
			}
			break;
		case 0xCB: // -
			break;
		case 0xCC: // CZ adr
			if (s->cc.z) {
				call(s, opcode);
			}
			break;
		case 0xCD: // CALL adr
			call(s, opcode);
			break;
		case 0xCE: // ACI D8
			add8(s, s->r.a, opcode[1] + s->cc.cy, true);
			break;
		case 0xCF: // RST 1
			// TODO this is a special instruction, this subroutine might need to be implemented
			call(s, (uint8_t*)0x08);
			break;
		case 0xD0: // RNC
			if (!s->cc.cy) {
				ret(s);
			}
			break;
		case 0xD1: // POP D
			pop(s, s->r.d, s->r.e); 
			break;
		case 0xD2: // JNC adr
			if (!s->cc.cy) {
				jump(s, opcode);
			} else {
				s->r.pc += 2;
			}
			break;
		case 0xD3: // OUT D8 - special
			unimplementedInstruction(*opcode); break;
		case 0xD4: // CNC adr
			if (!s->cc.cy) {
				call(s, opcode);
			}
			break;
		case 0xD5: // PUSH D
			push(s, s->r.d, s->r.e);
			break;
		case 0xD6: // SUI D8
			sub8(s, s->r.a, opcode[1], true);
			break;
		case 0xD7: // RST 2
			// TODO this is a special instruction, this subroutine might need to be implemented
			call(s, (uint8_t*)0x10);
			break;
		case 0xD8: // RC
			if (s->cc.cy) {
				ret(s);
			}
			break;
		case 0xD9: // -
			break;
		case 0xDA: // JC adr
			if (s->cc.cy) {
				jump(s, opcode);
			}
			break;
		case 0xDB: // IN D8 - special
			unimplementedInstruction(*opcode); break;
		case 0xDC: // CC adr
			if (s->cc.cy) {
				call(s, opcode);
			}
			break;
		case 0xDD: // -
			break;
		case 0xDE: // SBI D8
			sub8(s, s->r.a, opcode[1] - s->cc.cy, true);
			break;
		case 0xDF: // RST 3
			// TODO this is a special instruction, this subroutine might need to be implemented
			call(s, (uint8_t*)0x18);
			break;
		case 0xE0: // RPO
			if (!s->cc.p) {
				ret(s);
			}
			break;
		case 0xE1: // POP H
			pop(s, s->r.h, s->r.l);
			break;
		case 0xE2: // JPO adr
			if (!s->cc.p) {
				jump(s, opcode);
			} else {
				s->r.pc += 2;
			}
			break;
		case 0xE3: // XTHL
			// Swap L and SP
			s->temp8 = s->memory[s->r.sp]; // Save SP
			s->memory[s->r.sp] = s->r.l; // Move L to SP
			s->r.l = s->temp8; // Move prev SP to L
			// Swap H and SP + 1
			s->temp8 = s->memory[s->r.sp + 1]; // Save SP + 1
			s->memory[s->r.sp + 1] = s->r.h; // Move H to SP + 1
			s->r.h = s->temp8; // Move prev SP + 1 to H
			break;
		case 0xE4: // CPO adr
			if (!s->cc.p) {
				call(s, opcode);
			}
			break;
		case 0xE5: // PUSH H
			push(s, s->r.h, s->r.l);
			break;
		case 0xE6: // ANI D8	
			ana(s, s->r.a, opcode[1]);
			s->r.pc++;
			break;
		case 0xE7: // RST 4
			// TODO this is a special instruction, this subroutine might need to be implemented
			call(s, (uint8_t*)0x20);
			break;
		case 0xE8: // RPE
			if (s->cc.p) {
				ret(s);
			}
			break;
		case 0xE9: // PCHL
			// High order is H
			s->r.pc = (s->r.pc & 0x00ff) | (s->r.h << 8);
			// Low order is L
			s->r.pc = (s->r.pc & 0xff00) | s->r.l;		
			break;
		case 0xEA: // JPE adr
			if (s->cc.p) {
				jump(s, opcode);
			} else {
				s->r.pc += 2;
			}
			break;
		case 0xEB: // XCHG
			// Swap L and SP
			s->temp8 = s->r.d; // Save D
			s->r.d = s->r.h; // Move H to D
			s->r.h = s->temp8; // Move prev D to H
			// Swap H and SP + 1
			s->temp8 = s->r.e; // Save E
			s->r.e = s->r.l; // Move L to E
			s->r.l = s->temp8; // Move prev E to L
			break;
		case 0xEC: // CPE adr
			if (s->cc.p) {
				call(s, opcode);
			}
			break;
		case 0xED: // -
			break;
		case 0xEE: // XRI D8
			xra(s, s->r.a, opcode[1]);
			break;
		case 0xEF: // RST 5
			// TODO this is a special instruction, this subroutine might need to be implemented
			call(s, (uint8_t*)0x28);
			break;
		case 0xF0: // RP
			if (!s->cc.s) {
				ret(s);
			}
			break;
		case 0xF1: // POP PSW
			s->r.a = s->memory[s->r.sp + 1];
			s->temp8 = s->memory[s->r.sp]; // PSW
			s->cc.z = (0x01 == (s->temp8 & 0x01));
			s->cc.s = (0x02 == (s->temp8 & 0x02));
			s->cc.p = (0x04 == (s->temp8 & 0x04));
			s->cc.cy = (0x05 == (s->temp8 & 0x08));
			s->cc.ac = (0x10 == (s->temp8 & 0x10));
			s->r.sp += 2;
			break;
		case 0xF2: // JP adr
			if (!s->cc.s) {
				jump(s, opcode);
			} else {
				s->r.pc += 2;
			}
			break;
		case 0xF3: // DI - special
			unimplementedInstruction(*opcode); break;
		case 0xF4: // CP adr
			if (!s->cc.s) {
				call(s, opcode);
			}
			break;
		case 0xF5: // PUSH PSW
			s->temp8 = (s->cc.z | s->cc.s << 1 | s->cc.p << 2 | s->cc.cy << 3 | s->cc.ac << 4); // PSW
			push(s, s->r.a, s->temp8);
			break;
		case 0xF6: // ORI D8
			ora(s, s->r.a, opcode[1]);
			s->r.pc++;
			break;
		case 0xF7: // RST 6
			// TODO this is a special instruction, this subroutine might need to be implemented
			call(s, (uint8_t*)0x30);
			break;
		case 0xF8: // RM
			if (s->cc.s) {
				ret(s);
			}
			break;
		case 0xF9: // SPHL
			s->temp16 = (s->r.h << 8) | s->r.l;
			s->r.sp = s->temp16;
			break;
		case 0xFA: // JM adr
			if (s->cc.s) {
				jump(s, opcode);
			} else {
				s->r.pc += 2;
			}
			break;
		case 0xFB: // EI - special
			unimplementedInstruction(*opcode); break;
		case 0xFC: // CM adr
			if (s->cc.s) {
				call(s, opcode);
			}
			break;
		case 0xFD: // -
			break;
		case 0xFE: // CPI D8
			s->temp16 = s->r.a - opcode[1];
			checkFlags(s, s->temp16, true);
			break;
		case 0xFF: // RST 7
			// TODO this is a special instruction, this subroutine might need to be implemented
			call(s, (uint8_t*)0x38);
			break;
		default:
			unimplementedInstruction(*opcode); break;
		}
		// Increment program counter
		s->r.pc++;
		// Print state - testing only
		printState(s, *opcode, (opcode[2] << 8) | opcode[1]);
	}
	
	// Tests

	void testRegisters(state *s) {
		s->r.c = 0x01;
		s->r.e = 0xFF;
	}

	void cpudiagFix(state *s) {
		//Fix the first instruction to be JMP 0x100    
		s->memory[0] = 0xc3;
		s->memory[1] = 0;
		s->memory[2] = 0x01;

		//Fix the stack pointer from 0x6ad to 0x7ad    
		// this 0x06 byte 112 in the code, which is    
		// byte 112 + 0x100 = 368 in memory    
		s->memory[368] = 0x7;

		//Skip DAA test    
		s->memory[0x59c] = 0xc3; //JMP    
		s->memory[0x59d] = 0xc2;
		s->memory[0x59e] = 0x05;
	}
}

int main(int argc, char *argv[]) {
	// New state
	Emu8080::state s;
	// Test emulation
	// Emu8080::testRegisters(&s);
	Emu8080::readFile(&s, "invaders.bin");
	// Emu8080::cpudiagFix(&s);
	// Print state
	std::cout << "Init\n";
	// Emulate
	while (s.r.pc < s.memory.size()) {
		Emu8080::emulate8080(&s);
		// Wait for input
		system("pause");
	}
	std::cout << "\nEnd of emulation.\n";
	system("pause");
	return 0;
}