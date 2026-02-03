#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int encode_yay0(char *PackedFile, char *SubPath, char *SrcPath, char *AddList, int Flags);
void search(unsigned int a1, int a2, int *a3, unsigned int *a4);
int mischarsearch(unsigned char *a1, int a2, unsigned char *a3, int a4);
void initskip(unsigned char *a1, int a2);
void writeshort(short a1);
void writeint4(int a1);

unsigned short skip[256];
int cp;
int ndp;
FILE *fp;
unsigned char *def;
unsigned short *pol;
int pp;
int insize;
int ncp;
int npp;
unsigned char *bz;
int dp;
unsigned int *cmd;

int encode_yay0(char *PackedFile, char *SubPath, char *SrcPath, char *AddList, int Flags)
{
    unsigned int v0;
    unsigned int v1;
    int v2;
    int v3;
    int v4;
    char v5;
    unsigned int v6;
    unsigned int v7;
    int v8;
    unsigned int a4;
    int a3;

    // Build the full source path
    char fullpath[1024];
    if (SubPath && SubPath[0] != '\0') {
        snprintf(fullpath, sizeof(fullpath), "%s/%s/%s", SrcPath, SubPath, AddList);
    } else {
        snprintf(fullpath, sizeof(fullpath), "%s/%s", SrcPath, AddList);
    }

    // Read the entire source file into bz
    FILE *src = fopen(fullpath, "rb");
    if (src == NULL) {
        return -1;  // Cannot open source file
    }

    fseek(src, 0, SEEK_END);
    insize = ftell(src);
    fseek(src, 0, SEEK_SET);

    if (insize <= 0) {
        fclose(src);
        return -2;  // Empty or invalid file
    }

    bz = (unsigned char *)malloc(insize);
    if (bz == NULL) {
        fclose(src);
        return -3;  // Memory allocation failed
    }

    if (fread(bz, 1, insize, src) != (size_t)insize) {
        fclose(src);
        free(bz);
        return -4;  // Failed to read file
    }
    fclose(src);

    // Reset all state
    dp = 0;
    pp = 0;
    cp = 0;
    npp = 4096;
    ndp = 4096;
    ncp = 4096;

    cmd = (unsigned int *)calloc(ncp, sizeof(unsigned int));
    if (cmd == NULL) {
        free(bz);
        return 1;
    }

    pol = (unsigned short *)malloc(2 * npp);
    if (pol == NULL) {
        free(bz);
        free(cmd);
        return 2;
    }

    def = (unsigned char *)malloc(ndp);
    if (def == NULL) {
        free(bz);
        free(cmd);
        free(pol);
        return 3;
    }

    v0 = 0;
    v6 = 1024;
    v1 = 2147483648;

    fp = fopen(PackedFile, "wb");
    if (fp == NULL) {
        free(bz);
        free(cmd);
        free(pol);
        free(def);
        return 4;
    }

    // Main compression loop
    while (insize > (int)v0)
    {
        if (v6 < v0)
            v6 += 1024;
        search(v0, insize, &a3, &a4);
        if (a4 <= 2)
        {
            cmd[cp] |= v1;
            def[dp++] = bz[v0++];
            if (ndp == dp)
            {
                ndp = dp + 4096;
                def = (unsigned char *)realloc(def, ndp);
                if (def == NULL) {
                    fclose(fp);
                    free(bz);
                    free(cmd);
                    free(pol);
                    return 5;
                }
            }
        }
        else
        {
            search(v0 + 1, insize, &v8, &v7);
            if (v7 > a4 + 1)
            {
                cmd[cp] |= v1;
                def[dp++] = bz[v0++];
                if (ndp == dp)
                {
                    ndp = dp + 4096;
                    def = (unsigned char *)realloc(def, ndp);
                    if (def == NULL) {
                        fclose(fp);
                        free(bz);
                        free(cmd);
                        free(pol);
                        return 6;
                    }
                }
                v1 >>= 1;
                if (!v1)
                {
                    v1 = 2147483648;
                    v2 = cp++;
                    if (cp == ncp)
                    {
                        ncp = v2 + 1025;
                        cmd = (unsigned int *)realloc(cmd, 4 * ncp);
                        if (cmd == NULL) {
                            fclose(fp);
                            free(bz);
                            free(pol);
                            free(def);
                            return 7;
                        }
                    }
                    cmd[cp] = 0;
                }
                a4 = v7;
                a3 = v8;
            }
            v3 = v0 - a3 - 1;
            a3 = v0 - a3 - 1;
            v5 = a4;
            if (a4 > 0x11)
            {
                pol[pp++] = v3;
                def[dp++] = v5 - 18;
                if (ndp == dp)
                {
                    ndp = dp + 4096;
                    def = (unsigned char *)realloc(def, ndp);
                    if (def == NULL) {
                        fclose(fp);
                        free(bz);
                        free(cmd);
                        free(pol);
                        return 8;
                    }
                }
            }
            else
            {
                pol[pp++] = v3 | (((short)a4 - 2) << 12);
            }
            if (npp == pp)
            {
                npp = pp + 4096;
                pol = (unsigned short *)realloc(pol, 2 * npp);
                if (pol == NULL) {
                    fclose(fp);
                    free(bz);
                    free(cmd);
                    free(def);
                    return 9;
                }
            }
            v0 += a4;
        }
        v1 >>= 1;
        if (!v1)
        {
            v1 = 2147483648;
            v4 = cp++;
            if (cp == ncp)
            {
                ncp = v4 + 1025;
                cmd = (unsigned int *)realloc(cmd, 4 * ncp);
                if (cmd == NULL) {
                    fclose(fp);
                    free(bz);
                    free(pol);
                    free(def);
                    return 10;
                }
            }
            cmd[cp] = 0;
        }
    }
    if (v1 != (unsigned int)0x80000000)
        ++cp;

    // Write header
    fprintf(fp, "Yay0");
    writeint4(insize);
    writeint4(4 * cp + 16);
    writeint4(2 * pp + 4 * cp + 16);

    // Write command table
    for (int i = 0; i < cp; i++) {
        writeint4(cmd[i]);
    }

    // Write link table
    for (int i = 0; i < pp; i++) {
        writeshort(pol[i]);
    }

    // Write byte data
    fwrite(def, 1, dp, fp);

    fclose(fp);

    free(bz);
    free(cmd);
    free(pol);
    free(def);

    return 0;
}

void search(unsigned int a1, int a2, int *a3, unsigned int *a4)
{
    unsigned int v4;
    unsigned int v5;
    unsigned int *v6;
    unsigned int v7;
    int v8;
    unsigned int v9;

    v4 = 3;
    v5 = 0;
    if ( a1 > 0x1000 )
        v5 = a1 - 4096;
    v9 = 273;
    if ( a2 - a1 <= 0x111 )
        v9 = a2 - a1;
    if ( v9 > 2 )
    {
        while ( a1 > v5 )
        {
            v7 = mischarsearch(&bz[a1], v4, &bz[v5], v4 + a1 - v5);
            if ( v7 >= a1 - v5 )
                break;
            for ( ; v9 > v4; ++v4 )
            {
                if ( bz[v4 + v5 + v7] != bz[v4 + a1] )
                    break;
            }
            if ( v9 == v4 )
            {
                *a3 = v7 + v5;
                goto LABEL_17;
            }
            v8 = v5 + v7;
            ++v4;
            v5 += v7 + 1;
        }
        *a3 = v8;
        if ( v4 > 3 )
        {
            --v4;
LABEL_17:
            *a4 = v4;
            return;
        }
        v6 = a4;
    }
    else
    {
        *a4 = 0;
        v6 = (unsigned int *)a3;
    }
    *v6 = 0;
}

int mischarsearch(unsigned char *pattern, int patternlen, unsigned char *data, int datalen)
{
    int result;
    int i;
    int v6;
    int j;

    result = datalen;
    if ( patternlen <= datalen )
    {
        initskip(pattern, patternlen);
        for ( i = patternlen - 1; ; i += v6 )
        {
            if ( pattern[patternlen - 1] == data[i] )
            {
                --i;
                j = patternlen - 2;
                if ( patternlen - 2 < 0 )
                    return i + 1;
                while ( pattern[j] == data[i] )
                {
                    --i;
                    if ( --j < 0 )
                        return i + 1;
                }
                v6 = patternlen - j;
                if ( skip[data[i]] > patternlen - j )
                    v6 = skip[data[i]];
            }
            else
            {
                v6 = skip[data[i]];
            }

            // Bounds check to prevent infinite loop / out of bounds read
            if ( i + v6 >= datalen )
                return datalen;
        }
    }
    return result;
}

void initskip(unsigned char *pattern, int len)
{
    for (int i = 0; i < 256; i++)
        skip[i] = len;

    for (int i = 0; i < len; i++)
        skip[pattern[i]] = len - i - 1;
}

void writeshort(short val)
{
    fputc((val & 0xff00) >> 8, fp);
    fputc((val & 0x00ff) >> 0, fp);
}

void writeint4(int val)
{
    fputc((val & 0x00ff000000) >> 24, fp);
    fputc((val & 0x0000ff0000) >> 16, fp);
    fputc((val & 0x000000ff00) >>  8, fp);
    fputc((val & 0x00000000ff) >>  0, fp);
}