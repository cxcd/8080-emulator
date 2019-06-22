#include <iostream>
#include <iomanip>
#include <bitset>
#include <cstdint>
#include <vector>
#include <fstream>
#include <iterator>
#include "cpu.hpp"

// Exit program when an unimplemented instruction is encountered
int unimplementedInstruction(uint8_t opcode) {
	std::cout << "Error: Instruction 0x" 
			  << std::uppercase << std::hex << std::setw(2) << std::setfill('0') 
			  << (int)opcode << " is unimplemented\n";
	return 1;
}

void printState(state *s) {
	std::cout	<< "Z:" << std::bitset<1>(s->cc.z)
				<< " S:" << std::bitset<1>(s->cc.s)
				<< " P:" << std::bitset<1>(s->cc.p)
				<< " CY:" << std::bitset<1>(s->cc.cy)
				<< " AC:" << std::bitset<1>(s->cc.ac) << "\n";
	std::cout	<< "A:" << std::bitset<8>(s->r.a)
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
// Add value to 16 bit register
void add(state *s, uint8_t &reg1, uint8_t &reg2, uint8_t val) {
	uint16_t result = (reg1 << 8 | reg2) + val;
	reg1 = result >> 8;
	reg2 = result & 0xFF;
}
// Add 16 bit register to 16 bit register
void add(state *s, uint8_t &reg1, uint8_t &reg2, uint8_t &reg3, uint8_t &reg4) {
	uint32_t reg12 = (reg1 << 8) | reg2;
	uint32_t reg34 = (reg3 << 8) | reg4;
	uint32_t result = reg12 + reg34;
	reg1 = (result & 0xFF00) >> 8;
	reg2 = result & 0xFF;
	checkCarry(s, result);
}
// Subtract value from 8 bit register
void subtract(state *s, uint8_t &reg, uint8_t val, bool cy) {
	uint16_t result = (uint16_t)reg - (uint16_t)val;
	reg = result & 0xFF;
	checkFlags(s, result, cy);
}
// Subtract value from 16 bit register
void subtract(state *s, uint8_t &reg1, uint8_t &reg2, uint8_t val) {
	uint16_t result = (reg1 << 8 | reg2) - val;
	reg1 = result >> 8;
	reg2 = result & 0xFF;
}

// Parse code and execute instruction
void emulate8080(state *s) {
	// Get the current instruction from the program counter
	uint8_t *opcode = &s->memory[s->r.pc];
	// Check the instruction and execute it
	switch (*opcode) {
	case 0x00: // NOP
		break;
	case 0x01: // LXI B, d16
		s->r.c = opcode[1];
		s->r.b = opcode[2];
		s->r.pc += 2;
		break;
	case 0x02: // STAX B
		s->offset = ((uint16_t)s->r.b << 8) | s->r.c;
		s->memory[s->offset] = s->r.a;
		break;
	case 0x03: // INX B
		add(s, s->r.b, s->r.c, (uint8_t)1);
		break;
	case 0x04: // INR B
		add(s, s->r.b, (uint8_t)1, false);
		break;
	case 0x05: // DCR B
		subtract(s, s->r.b, (uint8_t)1, false);
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
	case 0x08: // NOP
		break;
	case 0x09: // DAD B
		add(s, s->r.h, s->r.l, s->r.b, s->r.c);
		break;
	case 0x0A: // LDAX B
		s->offset = (s->r.b << 8) | s->r.c;
		s->r.a = s->memory[s->offset];
		break;
	case 0x0B: // DCX B
		subtract(s, s->r.b, s->r.c, (uint8_t)1);
		break;
	case 0x0C: // INR C
		add(s, s->r.c, (uint8_t)1, false);
		break;
	case 0x0D: // DCR C
		subtract(s, s->r.c, (uint8_t)1, false);
		break;
	case 0x0E: // MVI C, D8
		s->r.c = opcode[1];
		s->r.pc++;
		break;
	case 0x0F: // RRC
		s->cc.cy = s->r.a & 1;
		s->offset = (uint16_t)s->cc.cy;
		s->r.a = (s->r.a >> 1) | (uint8_t)(s->offset << 7);
		break;
	case 0x10: // NOP
		break;
	case 0x11: // LDX D, D16
		s->r.d = opcode[1];
		s->r.e = opcode[2];
		s->r.pc += 2;
		break;
	case 0x12: // STAX D
		s->offset = ((uint16_t)s->r.d << 8) | s->r.e;
		s->memory[s->offset] = s->r.a; 
		break;
	case 0x13: // INX D
		add(s, s->r.d, s->r.e, (uint8_t)1);
		break;
	case 0x14: // INR D
		add(s, s->r.d, (uint8_t)1, false); 
		break;
	case 0x15: // DCR D
		subtract(s, s->r.d, (uint8_t)1, false); 
		break;
	case 0x16: // MVI D, D8
		s->r.d = opcode[1];
		s->r.pc++; 
		break;
	case 0x17: // RAL
		s->offset = (uint16_t)s->cc.cy;
		s->cc.cy = (s->r.a >> 7) & 1;
		s->r.a = (s->r.a << 1) | (uint8_t)s->offset; 
		break;
	case 0x18: // NOP
		break;
	case 0x19: // DAD D
		add(s, s->r.h, s->r.l, s->r.d, s->r.e); 
		break;
	case 0x1A: // LDAX D
		s-> offset = (s->r.d << 8) | s->r.e;
		s->r.a = s->memory[s->offset];
		break;
	case 0x1B: // DCX D
		subtract(s, s->r.d, s->r.e, (uint8_t)1);
		break;
	case 0x1C: // INR E
		add(s, s->r.e, (uint8_t)1, false); 
		break;
	case 0x1D: // DCR E
		subtract(s, s->r.e, (uint8_t)1, false); 
		break;
	case 0x1E: // MVI E, D8
		s->r.e = opcode[1];
		s->r.pc++; 
		break;
	case 0x1F: // RAR
		unimplementedInstruction(*opcode); break;
	case 0x20: // NOP
		break;
	case 0x21: // LXI H, D16
		unimplementedInstruction(*opcode); break;
	case 0x22: // SHLD adr
		unimplementedInstruction(*opcode); break;
	case 0x23: // INX H
		unimplementedInstruction(*opcode); break;
	case 0x24: // INR H
		unimplementedInstruction(*opcode); break;
	case 0x25: // DCR H
		unimplementedInstruction(*opcode); break;
	case 0x26: // MVI H, D8
		unimplementedInstruction(*opcode); break;
	case 0x27: // DAA
		unimplementedInstruction(*opcode); break;
	case 0x28: // NOP
		break;
	case 0x29: // DAD H
		unimplementedInstruction(*opcode); break;
	case 0x2A: // LHLD adr
		unimplementedInstruction(*opcode); break;
	case 0x2B: // DCX H
		unimplementedInstruction(*opcode); break;
	case 0x2C: // INR L
		unimplementedInstruction(*opcode); break;
	case 0x2D: // DCR L
		unimplementedInstruction(*opcode); break;
	case 0x2E: // MVI L, D8
		unimplementedInstruction(*opcode); break;
	case 0x2F: // CMA
		unimplementedInstruction(*opcode); break;
	case 0x30: // NOP
		break;


	// Jumping ahead
	case 0x80: // ADD B
		add(s, s->r.a, s->r.b, true); break;
	case 0x81: // ADD C
		add(s, s->r.a, s->r.c, true); break;
	case 0x82: // ADD D
		add(s, s->r.a, s->r.d, true); break;
	case 0x83: // ADD E
		add(s, s->r.a, s->r.e, true); break;
	case 0x84: // ADD H
		add(s, s->r.a, s->r.h, true); break;
	case 0x85: // ADD L
		add(s, s->r.a, s->r.l, true); break;
	case 0x86: // ADD M
		s->offset = ((uint16_t)s->r.h << 8) | s->r.l;
		add(s, s->r.a, s->memory[s->offset], true);
		break;
	case 0x87: // ADD A
		add(s, s->r.a, s->r.a, true); break;
	case 0x88: // ADC B
		break;
	// Jumping to end
	case 0xFF: unimplementedInstruction(*opcode); break;
	default: unimplementedInstruction(*opcode); break;
	}
	// Print state
	std::cout << "PC:" << s->r.pc << "\n";
	printState(s);
	// Increment program counter
	s->r.pc++;
}

void testRegisters(state *s) {
	s->r.c = 0x01;
	s->r.e = 0xFF;
}

int main(int argc, char *argv[]) {
	// New state
	state s;
	/*
	// Get file from arguments
	if (argc == 2) {
		s.readFile(argv[1]);
	} else {
		return 1;
	}
	*/
	testRegisters(&s);
	readFile(&s, "test.bin");
	// Print state
	std::cout << "Init\n";
	printState(&s);
	// Emulate
	while (s.r.pc < s.memory.size()) {
		emulate8080(&s);
	}
	system("pause");
	return 0;
}