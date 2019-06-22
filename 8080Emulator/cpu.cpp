#include <vector>
#include "cpu.hpp"

// Condition Codes init
conditionCodes::conditionCodes() : z(1), s(1), p(1), cy(1), ac(1) {}

// Registers
registers::registers() : a(0), b(0), c(0), d(0), e(0), h(0), l(0) {}

// State
state::state() {
	memory.reserve(0x10000); // Reserve 16KB
}
