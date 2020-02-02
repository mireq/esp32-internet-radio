#include "bits.h"


uint64_t ntoh64(uint64_t input) {
	uint8_t data[8];

	data[0] = input >> 56 & 0xff;
	data[1] = input >> 48 & 0xff;
	data[2] = input >> 40 & 0xff;
	data[3] = input >> 32 & 0xff;
	data[4] = input >> 24 & 0xff;
	data[5] = input >> 16 & 0xff;
	data[6] = input >> 8 & 0xff;
	data[7] = input >> 0 & 0xff;

	return *(uint64_t *)(&data[0]);
}


uint64_t hton64(uint64_t input) {
	return ntoh64(input);
}
