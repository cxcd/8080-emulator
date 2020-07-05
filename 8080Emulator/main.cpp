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
		conditionCodes() : z(1), s(1), p(1), cy(1), ac(1) {}
	};

	class registers {
	public:
		uint8_t a, b, c, d, e, h, l;
		uint16_t sp, pc;
		registers() : a(0), b(0), c(0), d(0), e(0), h(0), l(0) {}
	};

	class state {
	public:
		conditionCodes cc;
		registers r;
		uint8_t enable;
		std::vector<uint8_t> memory;
		uint16_t offset; // Catch-all holder for any 16 bit number needed in operations
		state() {
			memory.reserve(0x10000); // Reserve 16KB
		}
	};

	// Exit program when an unimplemented instruction is encountered
	void unimplementedInstruction(uint8_t opcode) {
		std::cout << "Error: Instruction "
			<< std::uppercase << std::hex << std::setw(2) << std::setfill('0')
			<< (int)opcode << " is unimplemented\n";
	}

	// Print CPU state
	void printState(state *s, uint8_t opcode) {
		std::cout << "PC: " << s->r.pc << " Opcode: "
			<< std::uppercase << std::hex << std::setw(2) << std::setfill('0')
			<< (int)opcode << "\n";
		std::cout << "Z:" << std::bitset<1>(s->cc.z)
			<< " S:" << std::bitset<1>(s->cc.s)
			<< " P:" << std::bitset<1>(s->cc.p)
			<< " CY:" << std::bitset<1>(s->cc.cy)
			<< " AC:" << std::bitset<1>(s->cc.ac) << "\n";
		std::cout << "A:" << std::bitset<8>(s->r.a)
			<< " B:" << std::bitset<8>(s->r.b)
			<< " C:" << std::bitset<8>(s->r.c)
			<< " D:" << std::bitset<8>(s->r.d)
			<< " E:" << std::bitset<8>(s->r.e)
			<< " H:" << std::bitset<8>(s->r.h)
			<< " L:" << std::bitset<8>(s->r.l) << "\n\n";
	}

	// Reading file into memory
	void readFile(state *s, const std::string &path) {
		// Get file
		std::ifstream file(path, std::ios::binary);
		file.unsetf(std::ios::skipws); // Skip whitespace
		/*
		// Get size
		std::streampos fileSize;
		file.seekg(0, std::ios::end);
		fileSize = file.tellg();
		file.seekg(0, std::ios::beg);
		// Reserve vector
		s->memory.reserve(fileSize);
		*/
		// Fill data
		s->memory.insert(s->memory.begin(), std::istream_iterator<uint8_t>(file), std::istream_iterator<uint8_t>());
	}

	// Operations
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

	void checkCarry(state *s, uint16_t result) {
		s->cc.cy = (result & 0xFF00) > 0;
	}
	void checkCarry(state *s, uint32_t result) {
		s->cc.cy = (result & 0xFFFF0000) > 0;
	}

	void checkFlags(state *s, uint16_t result, bool checkCY) {
		s->cc.z = (result & 0xFF) == 0; // Check if equal to zero
		s->cc.s = (result & 0x80) == 0x80; // Check if negative (msb is set)
		s->cc.p = parity(result, 0xFF); // Check parity
		if (checkCY) {
			checkCarry(s, result);
		}
		s->cc.ac = result >= 0x0F; // Check half carry
	}

	// Add value to 8 bit register
	void add(state *s, uint8_t &reg, uint8_t val, bool cy) {
		uint16_t result = (uint16_t)reg + (uint16_t)val;
		reg = result & 0xFF;
		checkFlags(s, result, cy);
	}
	// Add value to 16 bit register as two 8 bit registers
	void add(uint8_t &reg1, uint8_t &reg2, uint8_t val) {
		uint16_t result = (reg1 << 8 | reg2) + val;
		reg1 = result >> 8;
		reg2 = result & 0xFF;
	}
	// Add 16 bit register to 16 bit register as 8 bit registers
	void add(state *s, uint8_t &reg1, uint8_t &reg2, uint8_t &reg3, uint8_t &reg4) {
		uint32_t reg12 = (reg1 << 8) | reg2;
		uint32_t reg34 = (reg3 << 8) | reg4;
		uint32_t result = reg12 + reg34;
		reg1 = (result & 0xFF00) >> 8;
		reg2 = result & 0xFF;
		checkCarry(s, result);
	}
	// Add 16 bit register to 16 bit register as 8 bit registers and a 16 bit register
	void add(state *s, uint8_t &reg1, uint8_t &reg2, uint16_t &reg3) {
		uint32_t reg12 = (reg1 << 8) | reg2;
		uint32_t result = reg12 + reg3;
		reg1 = (result & 0xFF00) >> 8;
		reg2 = result & 0xFF;
		checkCarry(s, result);
	}
	// Add value and carry to 8 bit register
	void adc(state *s, uint8_t &reg, uint8_t val, bool cy) {
		uint16_t result = (uint16_t)reg + (uint16_t)val + s->cc.cy;
		reg = result & 0xFF;
		checkFlags(s, result, cy);
	}

	// Subtract value from 8 bit register
	void sub(state *s, uint8_t &reg, uint8_t val, bool cy) {
		uint16_t result = (uint16_t)reg - (uint16_t)val;
		reg = result & 0xFF;
		checkFlags(s, result, cy);
	}
	// Subtract value from 16 bit register
	void sub(uint8_t &reg1, uint8_t &reg2, uint8_t val) {
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
	void mov(uint8_t &reg1, uint8_t &reg2) {
		reg1 = reg2;
	}
	// Move 8 bit register to/from register at location HL
	void mov(state *s, uint8_t &reg, bool toHL) {
		s->offset = (s->r.h << 8) | s->r.l;
		if (toHL) {
			s->memory[s->offset] = reg;
		} else {
			reg = s->memory[s->offset];
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
		s->offset = s->r.pc + 2;
		s->memory[s->r.sp - 1] = (s->offset >> 8) & 0xff;
		s->memory[s->r.sp - 2] = (s->offset & 0xff);
		s->r.sp = s->r.sp - 2;
		s->r.pc = (reg[2] << 8) | reg[1];
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
			s->offset = (s->r.b << 8) | s->r.c;
			s->memory[s->offset] = s->r.a;
			break;
		case 0x03: // INX B
			add(s->r.b, s->r.c, (uint8_t)1);
			break;
		case 0x04: // INR B
			add(s, s->r.b, (uint8_t)1, false);
			break;
		case 0x05: // DCR B
			sub(s, s->r.b, (uint8_t)1, false);
			break;
		case 0x06: // MVI B, D8
			s->r.b = opcode[1];
			s->r.pc++;
			break;
		case 0x07: // RLC
			s->cc.cy = (s->r.a >> 7) & 1;
			s->offset = (uint16_t)s->cc.cy;
			s->r.a = (s->r.a << 1) | (uint8_t)s->offset;
			break;
		case 0x08: // -
			break;
		case 0x09: // DAD B
			add(s, s->r.h, s->r.l, s->r.b, s->r.c);
			break;
		case 0x0A: // LDAX B
			s->offset = (s->r.b << 8) | s->r.c;
			s->r.a = s->memory[s->offset];
			break;
		case 0x0B: // DCX B
			sub(s->r.b, s->r.c, (uint8_t)1);
			break;
		case 0x0C: // INR C
			add(s, s->r.c, (uint8_t)1, false);
			break;
		case 0x0D: // DCR C
			sub(s, s->r.c, (uint8_t)1, false);
			break;
		case 0x0E: // MVI C, D8
			s->r.c = opcode[1];
			s->r.pc++;
			break;
		case 0x0F: // RRC
			s->cc.cy = s->r.a & 1;
			s->offset = s->cc.cy;
			s->r.a = (s->r.a >> 1) | (uint8_t)(s->offset << 7);
			break;
		case 0x10: // -
			break;
		case 0x11: // LXI D, D16
			s->r.d = opcode[1];
			s->r.e = opcode[2];
			s->r.pc += 2;
			break;
		case 0x12: // STAX D
			s->offset = (s->r.d << 8) | s->r.e;
			s->memory[s->offset] = s->r.a;
			break;
		case 0x13: // INX D
			add(s->r.d, s->r.e, (uint8_t)1);
			break;
		case 0x14: // INR D
			add(s, s->r.d, (uint8_t)1, false);
			break;
		case 0x15: // DCR D
			sub(s, s->r.d, (uint8_t)1, false);
			break;
		case 0x16: // MVI D, D8
			s->r.d = opcode[1];
			s->r.pc++;
			break;
		case 0x17: // RAL
			s->offset = s->cc.cy;
			s->cc.cy = (s->r.a >> 7) & 1;
			s->r.a = (s->r.a << 1) | (uint8_t)s->offset;
			break;
		case 0x18: // -
			break;
		case 0x19: // DAD D
			add(s, s->r.h, s->r.l, s->r.d, s->r.e);
			break;
		case 0x1A: // LDAX D
			s->offset = (s->r.d << 8) | s->r.e;
			s->r.a = s->memory[s->offset];
			break;
		case 0x1B: // DCX D
			sub(s->r.d, s->r.e, (uint8_t)1);
			break;
		case 0x1C: // INR E
			add(s, s->r.e, (uint8_t)1, false);
			break;
		case 0x1D: // DCR E
			sub(s, s->r.e, (uint8_t)1, false);
			break;
		case 0x1E: // MVI E, D8
			s->r.e = opcode[1];
			s->r.pc++;
			break;
		case 0x1F: // RAR
			s->cc.cy = s->r.a & 1;
			s->offset = (uint16_t)s->r.a;
			s->r.a = (s->r.a >> 1) | (uint8_t)(s->offset << 7);
			break;
		case 0x20: // -
			break;
		case 0x21: // LXI H, D16
			s->r.l = opcode[1];
			s->r.h = opcode[2];
			s->r.pc += 2;
			break;
		case 0x22: // SHLD adr
			s->offset = (opcode[2] << 8) | opcode[1];
			s->memory[s->offset] = s->r.l;
			s->memory[s->offset++] = s->r.h;
			s->r.pc += 2;
			break;
		case 0x23: // INX H
			add(s->r.h, s->r.l, (uint8_t)1);
			break;
		case 0x24: // INR H
			add(s, s->r.h, (uint8_t)1, false);
			break;
		case 0x25: // DCR H
			sub(s, s->r.h, (uint8_t)1, false);
			break;
		case 0x26: // MVI H, D8
			s->r.h = opcode[1];
			s->r.pc++;
			break;
		case 0x27: // DAA
			unimplementedInstruction(*opcode); break;
		case 0x28: // -
			break;
		case 0x29: // DAD H
			add(s, s->r.h, s->r.l, s->r.h, s->r.l);
			break;
		case 0x2A: // LHLD adr
			s->offset = (opcode[2] << 8) | opcode[1];
			s->r.l = s->memory[s->offset];
			s->r.h = s->memory[s->offset++];
			s->r.pc += 2;
			break;
		case 0x2B: // DCX H
			sub(s->r.h, s->r.l, (uint8_t)1);
			break;
		case 0x2C: // INR L
			add(s, s->r.l, (uint8_t)1, false);
			break;
		case 0x2D: // DCR L
			sub(s, s->r.l, (uint8_t)1, false);
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
			s->offset = (opcode[2] << 8) | opcode[1];
			s->r.sp = s->offset;
			s->r.pc += 2;
			break;
		case 0x32: // STA adr
			unimplementedInstruction(*opcode); break;
		case 0x33: // INX SP
			s->r.sp++;
			break;
		case 0x34: // INR M
			s->offset = (s->r.h << 8) | s->r.l;
			add(s, s->memory[s->offset], (uint8_t)1, false);
			break;
		case 0x35: // DCR M
			s->offset = (s->r.h << 8) | s->r.l;
			sub(s, s->memory[s->offset], (uint8_t)1, false);
			break;
		case 0x36: // MVI M, D8
			s->offset = (s->r.h << 8) | s->r.l;
			s->memory[s->offset] = opcode[1];
			s->r.pc++;
			break;
		case 0x37: // STC
			s->cc.cy = 1;
			break;
		case 0x38: // -
			break;
		case 0x39: // DAD SP
			add(s, s->r.h, s->r.l, s->r.sp);
			break;
		case 0x3A: // LDA adr
			s->offset = (opcode[2] << 8) | opcode[1];
			s->r.a = s->memory[s->offset];
			break;
		case 0x3B: // DCX SP
			s->r.sp--;
			break;
		case 0x3C: // INR A
			add(s, s->r.a, (uint8_t)1, false);
			break;
		case 0x3D: // DCR A
			sub(s, s->r.a, (uint8_t)1, false);
			break;
		case 0x3E: // MVI A, D8
			s->r.a = opcode[1];
			s->r.pc++;
			break;
		case 0x3F: // CMC
			s->cc.cy = ~s->cc.cy;
			break;
		case 0x40: // MOV B, B
			mov(s->r.b, s->r.b);
			break;
		case 0x41: // MOV B, C
			mov(s->r.b, s->r.c);
			break;
		case 0x42: // MOV B, D
			mov(s->r.b, s->r.d);
			break;
		case 0x43: // MOV B, E
			mov(s->r.b, s->r.e);
			break;
		case 0x44: // MOV B, H
			mov(s->r.b, s->r.h);
			break;
		case 0x45: // MOV B, L
			mov(s->r.b, s->r.l);
			break;
		case 0x46: // MOV B, M
			mov(s, s->r.b, false);
			break;
		case 0x47: // MOV B, A
			mov(s->r.b, s->r.a);
			break;
		case 0x48: // MOV C, B
			mov(s->r.c, s->r.b);
			break;
		case 0x49: // MOV C, C
			mov(s->r.c, s->r.c);
			break;
		case 0x4A: // MOV C, D
			mov(s->r.c, s->r.d);
			break;
		case 0x4B: // MOV C, E
			mov(s->r.c, s->r.e);
			break;
		case 0x4C: // MOV C, H
			mov(s->r.c, s->r.h);
			break;
		case 0x4D: // MOV C, L
			mov(s->r.c, s->r.l);
			break;
		case 0x4E: // MOV C, M
			mov(s, s->r.c, false);
			break;
		case 0x4F: // MOV C, A
			mov(s->r.c, s->r.a);
			break;
		case 0x50: // MOV D, B
			mov(s->r.d, s->r.b);
			break;
		case 0x51: // MOV D, C
			mov(s->r.d, s->r.c);
			break;
		case 0x52: // MOV D, D
			mov(s->r.d, s->r.d);
			break;
		case 0x53: // MOV D, E
			mov(s->r.d, s->r.e);
			break;
		case 0x54: // MOV D, H
			mov(s->r.d, s->r.h);
			break;
		case 0x55: // MOV D, L
			mov(s->r.d, s->r.l);
			break;
		case 0x56: // MOV D, M
			mov(s, s->r.d, false);
			break;
		case 0x57: // MOV D, A
			mov(s->r.d, s->r.a);
			break;
		case 0x58: // MOV E, B
			mov(s->r.e, s->r.b);
			break;
		case 0x59: // MOV E, C
			mov(s->r.e, s->r.c);
			break;
		case 0x5A: // MOV E, D
			mov(s->r.e, s->r.d);
			break;
		case 0x5B: // MOV E, E
			mov(s->r.e, s->r.e);
			break;
		case 0x5C: // MOV E, H
			mov(s->r.e, s->r.h);
			break;
		case 0x5D: // MOV E, L
			mov(s->r.e, s->r.l);
			break;
		case 0x5E: // MOV E, M
			mov(s, s->r.e, false);
			break;
		case 0x5F: // MOV E, A
			mov(s->r.e, s->r.a);
			break;
		case 0x60: // MOV H, B
			mov(s->r.h, s->r.b);
			break;
		case 0x61: // MOV H, C
			mov(s->r.h, s->r.c);
			break;
		case 0x62: // MOV H, D
			mov(s->r.h, s->r.d);
			break;
		case 0x63: // MOV H, E
			mov(s->r.h, s->r.e);
			break;
		case 0x64: // MOV H, H
			mov(s->r.h, s->r.h);
			break;
		case 0x65: // MOV H, L
			mov(s->r.h, s->r.l);
			break;
		case 0x66: // MOV H, M
			mov(s, s->r.h, false);
			break;
		case 0x67: // MOV H, A
			mov(s->r.h, s->r.a);
			break;
		case 0x68: // MOV L, B
			mov(s->r.l, s->r.b);
			break;
		case 0x69: // MOV L, C
			mov(s->r.l, s->r.c);
			break;
		case 0x6A: // MOV L, D
			mov(s->r.l, s->r.d);
			break;
		case 0x6B: // MOV L, E
			mov(s->r.l, s->r.e);
			break;
		case 0x6C: // MOV L, H
			mov(s->r.l, s->r.h);
			break;
		case 0x6D: // MOV L, L
			mov(s->r.l, s->r.l);
			break;
		case 0x6E: // MOV L, M
			mov(s, s->r.l, false);
			break;
		case 0x6F: // MOV M, A
			mov(s->r.l, s->r.a);
			break;
		case 0x70: // MOV M, B
			mov(s, s->r.b, true);
			break;
		case 0x71: // MOV M, C
			mov(s, s->r.c, true);
			break;
		case 0x72: // MOV M, D
			mov(s, s->r.d, true);
			break;
		case 0x73: // MOV M, E
			mov(s, s->r.e, true);
			break;
		case 0x74: // MOV M, H
			mov(s, s->r.h, true);
			break;
		case 0x75: // MOV M, L
			mov(s, s->r.l, true);
			break;
		case 0x76: // HLT
			unimplementedInstruction(*opcode); break;
		case 0x77: // MOV M, A
			mov(s, s->r.a, true);
			break;
		case 0x78: // MOV A, B
			mov(s->r.a, s->r.b);
			break;
		case 0x79: // MOV A, C
			mov(s->r.a, s->r.c);
			break;
		case 0x7A: // MOV A, D
			mov(s->r.a, s->r.d);
			break;
		case 0x7B: // MOV A, E
			mov(s->r.a, s->r.e);
			break;
		case 0x7C: // MOV A, H
			mov(s->r.a, s->r.h);
			break;
		case 0x7D: // MOV A, L
			mov(s->r.a, s->r.l);
			break;
		case 0x7E: // MOV A, M
			mov(s, s->r.a, false);
			break;
		case 0x7F: // MOV A, A
			mov(s->r.a, s->r.a);
			break;
		case 0x80: // ADD B
			add(s, s->r.a, s->r.b, true);
			break;
		case 0x81: // ADD C
			add(s, s->r.a, s->r.c, true);
			break;
		case 0x82: // ADD D
			add(s, s->r.a, s->r.d, true);
			break;
		case 0x83: // ADD E
			add(s, s->r.a, s->r.e, true);
			break;
		case 0x84: // ADD H
			add(s, s->r.a, s->r.h, true);
			break;
		case 0x85: // ADD L
			add(s, s->r.a, s->r.l, true);
			break;
		case 0x86: // ADD M
			s->offset = (s->r.h << 8) | s->r.l;
			add(s, s->r.a, s->memory[s->offset], true);
			break;
		case 0x87: // ADD A
			add(s, s->r.a, s->r.a, true);
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
			s->offset = (s->r.h << 8) | s->r.l;
			adc(s, s->r.a, s->memory[s->offset], true);
			break;
		case 0x8F: // ADC A
			adc(s, s->r.a, s->r.a, true);
			break;
		case 0x90: // SUB B
			sub(s, s->r.a, s->r.b, true);
			break;
		case 0x91: // SUB C
			sub(s, s->r.a, s->r.c, true);
			break;
		case 0x92: // SUB D
			sub(s, s->r.a, s->r.d, true);
			break;
		case 0x93: // SUB E
			sub(s, s->r.a, s->r.e, true);
			break;
		case 0x94: // SUB H
			sub(s, s->r.a, s->r.h, true);
			break;
		case 0x95: // SUB L
			sub(s, s->r.a, s->r.l, true);
			break;
		case 0x96: // SUB M
			s->offset = (s->r.h << 8) | s->r.l;
			sub(s, s->r.a, s->memory[s->offset], true);
			break;
		case 0x97: // SUB A
			sub(s, s->r.a, s->r.a, true);
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
			s->offset = (s->r.h << 8) | s->r.l;
			sbb(s, s->r.a, s->memory[s->offset], true);
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
			s->offset = (s->r.h << 8) | s->r.l;
			ana(s, s->r.a, s->memory[s->offset]);
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
			s->offset = (s->r.h << 8) | s->r.l;
			xra(s, s->r.a, s->memory[s->offset]);
			break;
		case 0xAF: // XRA A
			xra(s, s->r.a, s->r.a);
			break;
		case 0xB0: // ORA B
			xra(s, s->r.a, s->r.b);
			break;
		case 0xB1: // ORA C
			xra(s, s->r.a, s->r.c);
			break;
		case 0xB2: // ORA D
			xra(s, s->r.a, s->r.d);
			break;
		case 0xB3: // ORA E
			xra(s, s->r.a, s->r.e);
			break;
		case 0xB4: // ORA H
			xra(s, s->r.a, s->r.h);
			break;
		case 0xB5: // ORA L
			xra(s, s->r.a, s->r.l);
			break;
		case 0xB6: // ORA M
			s->offset = (s->r.h << 8) | s->r.l;
			xra(s, s->r.a, s->memory[s->offset]);
			break;
		case 0xB7: // ORA A
			xra(s, s->r.a, s->r.a);
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
			s->offset = (s->r.h << 8) | s->r.l;
			cmp(s, s->memory[s->offset]);
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
				s->r.pc = (opcode[2] << 8) | opcode[1];
			} else {
				s->r.pc += 2;
			}
			break;
		case 0xC3: // JMP adr
			s->r.pc = (opcode[2] << 8) | opcode[1];
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
			add(s, s->r.a, opcode[1], true);
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
				s->r.pc = (opcode[2] << 8) | opcode[1];
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
			add(s, s->r.a, opcode[1] + s->cc.cy, true);
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
				s->r.pc = (opcode[2] << 8) | opcode[1];
			}
			break;
		case 0xD3: // OUT D8
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
			sub(s, s->r.a, opcode[1], true);
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
				s->r.pc = (opcode[2] << 8) | opcode[1];
			}
			break;
		case 0xDB: // IN D8
			unimplementedInstruction(*opcode); break;
		case 0xDC: // CC adr
			if (s->cc.cy) {
				call(s, opcode);
			}
			break;
		case 0xDD: // -
			break;
		case 0xDE: // SBI D8
			sub(s, s->r.a, opcode[1] - s->cc.cy, true);
			break;
		case 0xDF: // RST 3
			// TODO this is a special instruction, this subroutine might need to be implemented
			call(s, (uint8_t*)0x18);
			break;
		case 0xE0: // RP0
			unimplementedInstruction(*opcode);
			break;
		case 0xE1: // POP H
			unimplementedInstruction(*opcode); break;
		case 0xE2: // JPO adr
			unimplementedInstruction(*opcode); break;
		case 0xE3: // XTHL
			unimplementedInstruction(*opcode); break;
		case 0xE4: // CPO adr
			unimplementedInstruction(*opcode); break;
		case 0xE5: // PUSH H
			unimplementedInstruction(*opcode); break;
		case 0xE6: // ANI D8
			unimplementedInstruction(*opcode); break;
		case 0xE7: // RST 4
			// TODO this is a special instruction, this subroutine might need to be implemented
			call(s, (uint8_t*)0x20);
			break;
		case 0xE8: // RPE
			unimplementedInstruction(*opcode);
			break;
		case 0xE9: // PCHL
			unimplementedInstruction(*opcode); break;
		case 0xEA: // JPE adr
			unimplementedInstruction(*opcode); break;
		case 0xEB: // XCHG
			unimplementedInstruction(*opcode); break;
		case 0xEC: // CPE adr
			unimplementedInstruction(*opcode); break;
		case 0xED: // -
			break;
		case 0xEE: // XRI D8
			unimplementedInstruction(*opcode); break;
		case 0xEF: // RST 5
			// TODO this is a special instruction, this subroutine might need to be implemented
			call(s, (uint8_t*)0x28);
			break;
		case 0xF0: // RP
			unimplementedInstruction(*opcode);
			break;
		case 0xF1: // POP PSW
			unimplementedInstruction(*opcode); break;
		case 0xF2: // JP adr
			unimplementedInstruction(*opcode); break;
		case 0xF3: // DI
			unimplementedInstruction(*opcode); break;
		case 0xF4: // CP adr
			unimplementedInstruction(*opcode); break;
		case 0xF5: // PUSH PSW
			unimplementedInstruction(*opcode); break;
		case 0xF6: // ORI D8
			unimplementedInstruction(*opcode); break;
		case 0xF7: // RST 6
			// TODO this is a special instruction, this subroutine might need to be implemented
			call(s, (uint8_t*)0x30);
			break;
		case 0xF8: // RM
			unimplementedInstruction(*opcode); break;
		case 0xF9: // SPHL
			s->offset = (s->r.h << 8) | s->r.l;
			s->r.sp = s->offset;
			break;
		case 0xFA: // JM adr
			unimplementedInstruction(*opcode); break;
		case 0xFB: // EI
			unimplementedInstruction(*opcode); break;
		case 0xFC: // CM adr
			unimplementedInstruction(*opcode); break;
		case 0xFD: // -
			break;
		case 0xFE: // CPI D8
			unimplementedInstruction(*opcode); break;
		case 0xFF: // RST 7
			// TODO this is a special instruction, this subroutine might need to be implemented
			call(s, (uint8_t*)0x38);
			break;
		default:
			unimplementedInstruction(*opcode); break;
		}
		// Print state
		printState(s, *opcode);
		// Increment program counter
		s->r.pc++;
	}

	void testRegisters(state *s) {
		s->r.c = 0x01;
		s->r.e = 0xFF;
	}
}

int main(int argc, char *argv[]) {
	// New state
	Emu8080::state s;
	/*
	// Get file from arguments
	if (argc == 2) {
		s.readFile(argv[1]);
	} else {
		return 1;
	}
	*/
	Emu8080::testRegisters(&s);
	Emu8080::readFile(&s, "test.bin");
	// Print state
	std::cout << "Init\n";
	Emu8080::printState(&s, 0);
	// Emulate
	while (s.r.pc < s.memory.size()) {
		Emu8080::emulate8080(&s);
	}
	system("pause");
	return 0;
}