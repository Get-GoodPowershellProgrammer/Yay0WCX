#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

// WCX Plugin header (wcxhead.h equivalent)
#define E_END_ARCHIVE    10  // No more files in archive
#define E_NO_MEMORY      11  // Not enough memory
#define E_BAD_DATA       12  // Data is bad
#define E_BAD_ARCHIVE    13  // CRC error in archive data
#define E_UNKNOWN_FORMAT 14  // Archive format unknown
#define E_EOPEN          15  // Cannot open existing file
#define E_ECREATE        16  // Cannot create file
#define E_ECLOSE         17  // Error closing file
#define E_EREAD          18  // Error reading from file
#define E_EWRITE         19  // Error writing to file

typedef struct {
    char ArcName[260];
    char FileName[260];
    int Flags;
    int UnpSize;
    int UnpSizeHigh;
    int FileTime;
    int FileAttr;
} tHeaderData;

typedef struct {
    char FileName[260];
    int FindHandle;
    FILE *ArcFile;
} tOpenArchiveData;

// Handle management using intptr_t
typedef intptr_t HANDLE;

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

// Yay0 decompression function
int decode_yay0(FILE *s, uint8_t **output_buf, uint32_t *output_size) {
    uint32_t i, j, k;
    uint32_t cnt;
    uint32_t r22, r26, r25, r30;
    uint32_t p, q;

    // Read header
    char sig[4];
    fseek(s, 0, SEEK_SET);
    fread(sig, 1, 4, s);
    
    if (memcmp(sig, "Yay0", 4) != 0) {
        return E_UNKNOWN_FORMAT;
    }

    fread(&i, sizeof(uint32_t), 1, s);  // decompressed size
    fread(&j, sizeof(uint32_t), 1, s);  // link table offset
    fread(&k, sizeof(uint32_t), 1, s);  // byte chunks offset
    
    i = swap32(i);
    j = swap32(j);
    k = swap32(k);
    
    if (i == 0 || i > 100000000) {
        return E_BAD_DATA;
    }

    uint8_t *output = (uint8_t *)malloc(i);
    if (!output) {
        return E_NO_MEMORY;
    }

    q = 0;
    cnt = 0;
    p = 16;

    do {
        if (cnt == 0) {
            fseek(s, p, SEEK_SET);
            fread(&r22, sizeof(uint32_t), 1, s);
            r22 = swap32(r22);
            p += 4;
            cnt = 32;
        }

        if (r22 & 0x80000000) {
            fseek(s, k, SEEK_SET);
            fread(&output[q], sizeof(uint8_t), 1, s);
            k++;
            q++;
        } else {
            fseek(s, j, SEEK_SET);
            fread(&r26, sizeof(uint16_t), 1, s);
            r26 = swap16(r26);
            j += 2;

            uint32_t dist = (r26 & 0xfff) + 1;
            r30 = r26 >> 12;

            if (r30 == 0) {
                uint8_t modifier;
                fseek(s, k, SEEK_SET);
                fread(&modifier, sizeof(uint8_t), 1, s);
                k++;
                r30 = modifier + 18;
            } else {
                r30 += 2;
            }

            r25 = q - dist;

            if (dist > q || q + r30 > i) {
                free(output);
                return E_BAD_DATA;
            }

            for (uint32_t n = 0; n < r30; n++) {
                output[q++] = output[r25++];
            }
        }

        r22 <<= 1;
        cnt--;
    } while (q < i);

    *output_buf = output;
    *output_size = i;
    return 0;
}

// WCX Plugin Functions

HANDLE __attribute__((visibility("default"))) OpenArchive(tOpenArchiveData *ArchiveData) {
    FILE *f = fopen(ArchiveData->FileName, "rb");
    if (!f) {
        return (HANDLE)E_EOPEN;
    }
    
    char sig[4];
    fread(sig, 1, 4, f);
    
    if (memcmp(sig, "Yay0", 4) != 0) {
        fclose(f);
        return (HANDLE)E_UNKNOWN_FORMAT;
    }
    
    ArchiveData->ArcFile = f;
    return (HANDLE)f;
}

int __attribute__((visibility("default"))) ReadHeader(HANDLE hArcData, tHeaderData *HeaderData) {
    FILE *f = (FILE *)hArcData;
    
    if (!f) {
        return E_BAD_ARCHIVE;
    }
    
    // Read decompressed size
    fseek(f, 4, SEEK_SET);
    uint32_t size;
    fread(&size, sizeof(uint32_t), 1, f);
    size = swap32(size);
    
    // Fill header data
    strcpy(HeaderData->FileName, "decompressed.bin");
    HeaderData->UnpSize = size;
    HeaderData->UnpSizeHigh = 0;
    HeaderData->FileAttr = 0x20; // Archive attribute
    
    // Return E_END_ARCHIVE after first file since Yay0 contains only one file
    static int already_read = 0;
    if (already_read) {
        already_read = 0;
        return E_END_ARCHIVE;
    }
    already_read = 1;
    
    return 0;
}

int __attribute__((visibility("default"))) ProcessFile(HANDLE hArcData, int Operation, char *DestPath, char *DestName) {
    FILE *f = (FILE *)hArcData;
    
    if (Operation == 0) {  // Skip
        return 0;
    }
    
    if (Operation == 1) {  // Extract
        uint8_t *output = NULL;
        uint32_t output_size = 0;
        
        int result = decode_yay0(f, &output, &output_size);
        if (result != 0) {
            return result;
        }
        
        // Write to destination
        char outpath[512];
        snprintf(outpath, sizeof(outpath), "%s/%s", DestPath, DestName);
        
        FILE *out = fopen(outpath, "wb");
        if (!out) {
            free(output);
            return E_ECREATE;
        }
        
        fwrite(output, 1, output_size, out);
        fclose(out);
        free(output);
        
        return 0;
    }
    
    return E_BAD_DATA;
}

int __attribute__((visibility("default"))) CloseArchive(HANDLE hArcData) {
    FILE *f = (FILE *)hArcData;
    if (f) {
        fclose(f);
    }
    return 0;
}

void __attribute__((visibility("default"))) SetChangeVolProc(HANDLE hArcData, void *pChangeVolProc1) {
    // Not needed for Yay0
}

void __attribute__((visibility("default"))) SetProcessDataProc(HANDLE hArcData, void *pProcessDataProc) {
    // Not needed for Yay0
}