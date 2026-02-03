#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include "wcxhead.h"
#include "yay0.h"
#ifdef __WIN32
    #define EXPORT __declspec(dllexport)
#else
    #define EXPORT __attribute__((visibility("default")))
#endif

// Archive handle - arcname added
typedef struct {
    FILE *file;
    int header_read;
    uint32_t unpacked_size;
    char arcname[512];
} ArchiveHandle;


// WCX Plugin Interface Functions

void* EXPORT OpenArchive(tOpenArchiveData *ArchiveData) {
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
    if (fread(sig, 1, 4, f) != 4 || memcmp(sig, SIGNATURE, 4) != 0) {
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

    // Strip path, keep only filename
    const char *basename = strrchr(ArchiveData->ArcName, '/');
    if (basename) {
        basename++;
    } else {
        basename = ArchiveData->ArcName;
    }
    strncpy(handle->arcname, basename, sizeof(handle->arcname) - 1);
    handle->arcname[sizeof(handle->arcname) - 1] = '\0';

    // Replace extension with .arc
    char *dot = strrchr(handle->arcname, '.');
    if (dot) {
        *dot = '\0';
    }
    strncat(handle->arcname, ".arc", sizeof(handle->arcname) - strlen(handle->arcname) - 1);
    
    ArchiveData->OpenResult = 0;
    return handle;
}

int EXPORT ReadHeader(void *hArcData, tHeaderData *HeaderData) {
    if (!hArcData || !HeaderData) {
        return E_BAD_ARCHIVE;
    }

    ArchiveHandle *handle = (ArchiveHandle *)hArcData;

    if (handle->header_read) {
        return E_END_ARCHIVE;
    }

    memset(HeaderData, 0, sizeof(tHeaderData));
    // Use the arcname we stored in OpenArchive
    strncpy(HeaderData->FileName, handle->arcname, sizeof(HeaderData->FileName) - 1);
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

int EXPORT CloseArchive(void *hArcData) {
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

void EXPORT SetChangeVolProc(void *hArcData, tChangeVolProc pChangeVolProc1) {
}

void EXPORT SetProcessDataProc(void *hArcData, tProcessDataProc pProcessDataProc) {
}

int EXPORT GetPackerCaps(void) {
    return PK_CAPS_BY_CONTENT;
}

int EXPORT CanYouHandleThisFile(char *FileName) {
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