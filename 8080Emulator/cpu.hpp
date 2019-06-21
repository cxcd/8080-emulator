#ifndef CPU_H
#define CPU_H

class conditionCodes {
public:
	uint8_t z, s, p, cy, ac, pad;
	conditionCodes();
};

class registers {
public:
	uint8_t a, b, c, d, e, h, l;
	uint16_t sp, pc;
	registers();
};

class state {
public:
	conditionCodes cc;
	registers r;
	uint8_t enable;
	std::vector<uint8_t> memory;
	uint16_t offset;
	state();
};

#endif