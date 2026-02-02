#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <time.h>

// WCX header definitions (from wcxhead.h)
#define E_END_ARCHIVE     10
#define E_NO_MEMORY       11
#define E_BAD_DATA        12
#define E_BAD_ARCHIVE     13
#define E_UNKNOWN_FORMAT  14
#define E_EOPEN           15
#define E_ECREATE         16
#define E_ECLOSE          17
#define E_EREAD           18
#define E_EWRITE          19
#define E_SMALL_BUF       20
#define E_EABORTED        21
#define E_NO_FILES        22
#define E_TOO_MANY_FILES  23
#define E_NOT_SUPPORTED   24

#define PK_OM_LIST         0
#define PK_OM_EXTRACT      1

#define PK_SKIP            0
#define PK_TEST            1
#define PK_EXTRACT         2

#define PK_CAPS_BY_CONTENT 64

typedef struct {
    char ArcName[260];
    char FileName[260];
    int Flags;
    int PackSize;
    int UnpSize;
    int HostOS;
    int FileCRC;
    int FileTime;
    int UnpVer;
    int Method;
    int FileAttr;
    char* CmtBuf;
    int CmtBufSize;
    int CmtSize;
    int CmtState;
} tHeaderData;

typedef struct {
    char* ArcName;
    int OpenMode;
    int OpenResult;
    char* CmtBuf;
    int CmtBufSize;
    int CmtSize;
    int CmtState;
} tOpenArchiveData;

typedef int (*tChangeVolProc)(char *ArcName, int Mode);
typedef int (*tProcessDataProc)(char *FileName, int Size);

// Archive handle
typedef struct {
    FILE *file;
    int header_read;
    uint32_t unpacked_size;
} ArchiveHandle;

// Byte swap functions
static uint32_t swap32(uint32_t val) {
    return ((val >> 24) & 0xff) |
           ((val >> 8) & 0xff00) |
           ((val << 8) & 0xff0000) |
           ((val << 24) & 0xff000000);
}

static uint16_t swap16(uint16_t val) {
    return (val >> 8) | (val << 8);
}

// Yay0 decompression
static int decode_yay0(FILE *s, uint8_t **output_buf, uint32_t *output_size) {
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

// WCX Plugin Interface Functions

void* __attribute__((visibility("default"))) OpenArchive(tOpenArchiveData *ArchiveData) {
    if (!ArchiveData || !ArchiveData->ArcName) {
        if (ArchiveData) ArchiveData->OpenResult = E_BAD_ARCHIVE;
        return NULL;
    }

    FILE *f = fopen(ArchiveData->ArcName, "rb");
    if (!f) {
        ArchiveData->OpenResult = E_EOPEN;
        return NULL;
    }

    char sig[4];
    if (fread(sig, 1, 4, f) != 4 || memcmp(sig, "Yay0", 4) != 0) {
        fclose(f);
        ArchiveData->OpenResult = E_UNKNOWN_FORMAT;
        return NULL;
    }

    uint32_t size;
    if (fread(&size, 4, 1, f) != 1) {
        fclose(f);
        ArchiveData->OpenResult = E_EREAD;
        return NULL;
    }

    ArchiveHandle *handle = malloc(sizeof(ArchiveHandle));
    if (!handle) {
        fclose(f);
        ArchiveData->OpenResult = E_NO_MEMORY;
        return NULL;
    }

    handle->file = f;
    handle->header_read = 0;
    handle->unpacked_size = swap32(size);
    
    ArchiveData->OpenResult = 0;
    return handle;
}

int __attribute__((visibility("default"))) ReadHeader(void *hArcData, tHeaderData *HeaderData) {
    if (!hArcData || !HeaderData) {
        return E_BAD_ARCHIVE;
    }

    ArchiveHandle *handle = (ArchiveHandle *)hArcData;

    if (handle->header_read) {
        return E_END_ARCHIVE;
    }

    memset(HeaderData, 0, sizeof(tHeaderData));
    strcpy(HeaderData->FileName, "decompressed.bin");
    HeaderData->PackSize = 0;
    HeaderData->UnpSize = handle->unpacked_size;
    HeaderData->HostOS = 0;
    HeaderData->FileAttr = 0x20;
    HeaderData->FileTime = (int)time(NULL);
    HeaderData->Method = 0;
    HeaderData->FileCRC = 0;
    HeaderData->UnpVer = 10;
    HeaderData->Flags = 0;

    handle->header_read = 1;
    return 0;
}

int __attribute__((visibility("default"))) ProcessFile(void *hArcData, int Operation, char *DestPath, char *DestName) {
    if (!hArcData) {
        return E_BAD_ARCHIVE;
    }

    ArchiveHandle *handle = (ArchiveHandle *)hArcData;

    if (Operation == PK_SKIP || Operation == PK_TEST) {
        return 0;
    }

    if (Operation == PK_EXTRACT) {
        uint8_t *output = NULL;
        uint32_t output_size = 0;

        int result = decode_yay0(handle->file, &output, &output_size);
        if (result != 0) {
            return result;
        }

        char outpath[512];
        if (DestPath && DestPath[0] != '\0') {
            snprintf(outpath, sizeof(outpath), "%s/%s", DestPath, DestName);
        } else {
            snprintf(outpath, sizeof(outpath), "%s", DestName);
        }

        FILE *out = fopen(outpath, "wb");
        if (!out) {
            free(output);
            return E_ECREATE;
        }

        size_t written = fwrite(output, 1, output_size, out);
        fclose(out);
        free(output);

        if (written != output_size) {
            return E_EWRITE;
        }

        return 0;
    }

    return E_NOT_SUPPORTED;
}

int __attribute__((visibility("default"))) CloseArchive(void *hArcData) {
    if (!hArcData) {
        return 0;
    }

    ArchiveHandle *handle = (ArchiveHandle *)hArcData;
    if (handle->file) {
        fclose(handle->file);
    }
    free(handle);
    return 0;
}

void __attribute__((visibility("default"))) SetChangeVolProc(void *hArcData, tChangeVolProc pChangeVolProc1) {
    // Not needed for Yay0
}

void __attribute__((visibility("default"))) SetProcessDataProc(void *hArcData, tProcessDataProc pProcessDataProc) {
    // Not needed for Yay0
}

int __attribute__((visibility("default"))) GetPackerCaps(void) {
    return PK_CAPS_BY_CONTENT;  // We can detect Yay0 by content
}

// Optional: Detect if file is Yay0 by content
int __attribute__((visibility("default"))) CanYouHandleThisFile(char *FileName) {
    FILE *f = fopen(FileName, "rb");
    if (!f) {
        return 0;
    }

    char sig[4];
    int result = 0;
    if (fread(sig, 1, 4, f) == 4 && memcmp(sig, "Yay0", 4) == 0) {
        result = 1;
    }

    fclose(f);
    return result;
}