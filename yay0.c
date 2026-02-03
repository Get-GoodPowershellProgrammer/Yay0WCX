#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "wcxhead.h"
#include "yay0.h"
// Byte swap functions
uint32_t swap32(uint32_t val) {
    return ((val >> 24) & 0xff) |
           ((val >> 8) & 0xff00) |
           ((val << 8) & 0xff0000) |
           ((val << 24) & 0xff000000);
}

uint16_t swap16(uint16_t val) {
    return (val >> 8) | (val << 8);
}

// Yay0 decompression
int decode_yay0(FILE *s, uint8_t **output_buf, uint32_t *output_size) {
    uint32_t i, j, k, cnt, r22, r26, r25, r30, p, q;

    fseek(s, 0, SEEK_SET);
    char sig[4];
    if (fread(sig, 1, 4, s) != 4 || memcmp(sig, "Yay0", 4) != 0) {
        return E_UNKNOWN_FORMAT;
    }

    if (fread(&i, 4, 1, s) != 1 || fread(&j, 4, 1, s) != 1 || fread(&k, 4, 1, s) != 1) {
        return E_EREAD;
    }
    
    i = swap32(i);
    j = swap32(j);
    k = swap32(k);
    
    if (i == 0 || i > 100000000) {
        return E_BAD_DATA;
    }

    uint8_t *output = malloc(i);
    if (!output) {
        return E_NO_MEMORY;
    }

    q = 0;
    cnt = 0;
    p = 16;

    while (q < i) {
        if (cnt == 0) {
            fseek(s, p, SEEK_SET);
            if (fread(&r22, 4, 1, s) != 1) {
                free(output);
                return E_EREAD;
            }
            r22 = swap32(r22);
            p += 4;
            cnt = 32;
        }

        if (r22 & 0x80000000) {
            fseek(s, k, SEEK_SET);
            if (fread(&output[q], 1, 1, s) != 1) {
                free(output);
                return E_EREAD;
            }
            k++;
            q++;
        } else {
            fseek(s, j, SEEK_SET);
            if (fread(&r26, 2, 1, s) != 1) {
                free(output);
                return E_EREAD;
            }
            r26 = swap16(r26);
            j += 2;

            uint32_t dist = (r26 & 0xfff) + 1;
            r30 = r26 >> 12;

            if (r30 == 0) {
                uint8_t modifier;
                fseek(s, k, SEEK_SET);
                if (fread(&modifier, 1, 1, s) != 1) {
                    free(output);
                    return E_EREAD;
                }
                k++;
                r30 = modifier + 18;
            } else {
                r30 += 2;
            }

            if (dist > q || q + r30 > i) {
                free(output);
                return E_BAD_DATA;
            }

            r25 = q - dist;
            for (uint32_t n = 0; n < r30; n++) {
                output[q++] = output[r25++];
            }
        }

        r22 <<= 1;
        cnt--;
    }

    *output_buf = output;
    *output_size = i;
    return 0;
}
