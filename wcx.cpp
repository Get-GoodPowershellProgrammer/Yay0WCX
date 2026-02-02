#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

// Byte swap functions for big-endian to little-endian conversion
uint32_t swap32(uint32_t val) {
    return ((val >> 24) & 0xff) |
           ((val >> 8) & 0xff00) |
           ((val << 8) & 0xff0000) |
           ((val << 24) & 0xff000000);
}

uint16_t swap16(uint16_t val) {
    return (val >> 8) | (val << 8);
}

// Decompression function
void decode(FILE *s, FILE *d) {
    uint32_t i, j, k;
    uint32_t cnt;
    uint32_t r22, r26, r25, r30;
    uint32_t p, q;

    // Read header
    fseek(s, 4, SEEK_SET);
    fread(&i, sizeof(uint32_t), 1, s);  // decompressed size
    fread(&j, sizeof(uint32_t), 1, s);  // link table offset
    fread(&k, sizeof(uint32_t), 1, s);  // byte chunks offset
    
    // Swap from big-endian to little-endian
    i = swap32(i);
    j = swap32(j);
    k = swap32(k);
    
    printf("Decoded size: %u (0x%X)\n", i, i);
    printf("Link table offset: %u (0x%X)\n", j, j);
    printf("Byte chunks offset: %u (0x%X)\n", k, k);
    
    // Sanity check
    if (i == 0 || i > 100000000) {  // 100MB limit
        fprintf(stderr, "Invalid decompressed size: %u\n", i);
        return;
    }

    // Allocate output buffer
    uint8_t *output = (uint8_t *)malloc(i);
    if (!output) {
        fprintf(stderr, "Memory allocation failed.\n");
        return;
    }

    q = 0;    // current position in output
    cnt = 0;  // mask bit counter
    p = 16;   // current position in mask table

    do {
        if (cnt == 0) {
            fseek(s, p, SEEK_SET);
            fread(&r22, sizeof(uint32_t), 1, s);
            r22 = swap32(r22);  // SWAP MASK BITS
            p += 4;
            cnt = 32;
        }

        if (r22 & 0x80000000) {
            // Non-linked chunk: read literal byte
            fseek(s, k, SEEK_SET);
            fread(&output[q], sizeof(uint8_t), 1, s);
            k++;
            q++;
        } else {
            // Linked chunk: copy from earlier in output
            fseek(s, j, SEEK_SET);
            fread(&r26, sizeof(uint16_t), 1, s);
            r26 = swap16(r26);  // SWAP LINK DATA
            j += 2;

            uint32_t dist = (r26 & 0xfff) + 1;  // FIX: distance is stored as (dist-1)
            r30 = r26 >> 12;                      // count

            if (r30 == 0) {
                uint8_t modifier;
                fseek(s, k, SEEK_SET);
                fread(&modifier, sizeof(uint8_t), 1, s);
                k++;
                r30 = modifier + 18;
            } else {
                r30 += 2;
            }

            r25 = q - dist;  // source offset (lookback)

            // Bounds check to prevent segfault
            if (dist > q) {
                fprintf(stderr, "Invalid backreference: dist=%u > q=%u\n", dist, q);
                free(output);
                return;
            }
            if (q + r30 > i) {
                fprintf(stderr, "Copy would exceed buffer: %u + %u > %u\n", q, r30, i);
                free(output);
                return;
            }

            // Copy bytes from earlier position in output buffer
            for (uint32_t n = 0; n < r30; n++) {
                output[q++] = output[r25++];
            }
        }

        r22 <<= 1;
        cnt--;
    } while (q < i);

    // Write entire output buffer to file
    fwrite(output, 1, i, d);
    printf("Successfully decompressed %u bytes\n", i);
    free(output);
}

// Function to check the Yay0 signature and decode
void check_Yay0(char *arg) {
    printf("File: %s\n", arg);

    unsigned char yay0_signature[4] = {0x59, 0x61, 0x79, 0x30};
    FILE *file = fopen(arg, "rb");
    if (!file) {
        perror("Error opening file");
        return;
    }

    unsigned char buffer[4];
    size_t bytes_read = fread(buffer, 1, sizeof(buffer), file);
    if (bytes_read < 4) {
        printf("Not enough bytes\n");
    } else {
        printf("First 4 bytes as characters: ");
        for (size_t i = 0; i < 4; ++i) {
            if (buffer[i] >= 32 && buffer[i] <= 126)
                printf("%c", buffer[i]);
            else
                printf("\\x%02X", buffer[i]);
        }
        printf("\n");

        if (memcmp(buffer, yay0_signature, sizeof(yay0_signature)) == 0) {
            printf("This is a Yay0 file.\n");

            // Open an output file for writing the decompressed data
            FILE *output_file = fopen("output_file.bin", "wb");
            if (!output_file) {
                perror("Error creating output file");
                fclose(file);
                return;
            }

            // Reset file position to the start of the Yay0 file and call Decode function
            fseek(file, 0, SEEK_SET);
            decode(file, output_file);

            // Close output file after decompression
            fclose(output_file);
        } else {
            printf("This is not a Yay0 file.\n");
        }
    }

    fclose(file);
}

// Main function
int main(int argc, char **argv) {
    if (argc < 2) {
        printf("Usage: %s <filename>\n", argv[0]);
        return 1;
    }

    printf("Hello World\n");
    printf("Scan File\n");

    // Check if the file is a Yay0 file and decode it
    check_Yay0(argv[1]);

    return 0;
}