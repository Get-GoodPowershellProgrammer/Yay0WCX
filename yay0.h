#include <stdint.h>
#include <stdio.h>
#define SIGNATURE "Yay0"
uint32_t swap32(uint32_t val);
uint16_t swap16(uint16_t val);
int decode_yay0(FILE *s, uint8_t **output_buf, uint32_t *output_size);