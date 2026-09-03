/***************************************************************************/
/*   2008 by Nilay K Roy                                                   */
/*   nilayr@brandeis.edu                                                   */
/*   2024: Updated for large file (>2 GB) support and cross-platform       */
/*   compatibility (Linux x86-64 and macOS ARM64/x86-64).                  */
/*                                                                         */
/*   Key changes:                                                           */
/*   - offset/reclen/recno arguments changed from int* to int64_t*         */
/*     so Fortran INTEGER*8 values reach C without truncation.             */
/*   - fseek/ftell replaced with fseeko/ftello (64-bit on all platforms)  */
/*   - _FILE_OFFSET_BITS=64 define added for Linux 32-bit compat          */
/*   - Removed -mcmodel=large dependency (not available on Apple Silicon)  */
/***************************************************************************/

/* Enable 64-bit file offsets on Linux glibc */
#ifndef _FILE_OFFSET_BITS
#define _FILE_OFFSET_BITS 64
#endif
#ifndef _LARGEFILE_SOURCE
#define _LARGEFILE_SOURCE 1
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>   /* int64_t */
#include <sys/types.h> /* off_t */
#include <math.h>

#define MAXFILES       200
#define MAXFILENAME    1000

static FILE *file_stream[MAXFILES];
static char  file_name[MAXFILES][MAXFILENAME];
static int   file_mode[MAXFILES];
static int   initialised = 0;

/* -----------------------------------------------------------------------
 * copen_ : open a file
 *   fname   - Fortran character string (not null-terminated)
 *   f_flag  - file slot index (Fortran UNIT equivalent)
 *   f_mode  - 0=read, 1=write, 2=read/write
 *   f_name  - hidden Fortran character length
 * ----------------------------------------------------------------------- */
void copen_(char *fname, int *f_flag, int *f_mode, int f_name)
{
    int i, j, k, ftemp;
    char ctemp[MAXFILENAME];
    char *cfinal;

    for (k = 0; k < MAXFILENAME; k++) ctemp[k] = ' ';
    strncpy(ctemp, fname, f_name);
    ftemp = f_name;

    /* Trim trailing blanks */
    for (k = 0; k < ftemp; k++) {
        if (isblank((unsigned char)ctemp[k])) { ftemp = k; break; }
    }

    cfinal = (char *)malloc(ftemp + 2);
    if (!cfinal) { printf("malloc failed in copen_\n"); exit(1); }
    strncpy(cfinal, ctemp, ftemp);
    cfinal[ftemp] = '\0';

    i = *f_flag;
    j = *f_mode;

    if (!initialised) {
        for (k = 0; k < MAXFILES; k++) {
            file_stream[k] = NULL;
            file_name[k][0] = '\0';
            file_mode[k] = -1;
        }
        initialised = 1;
    }

    if (i < 0 || i >= MAXFILES) {
        printf("File flag %d out of range [0,%d).\n", i, MAXFILES);
        free(cfinal); exit(1);
    }
    if (file_stream[i] != NULL) {
        printf("Cannot allocate file buffer to %s. "
               "UNIT in use or maximum files open.\n", cfinal);
        free(cfinal); exit(1);
    }

    if (j == 0) {
        file_stream[i] = fopen(cfinal, "rb");
    } else if (j == 1) {
        file_stream[i] = fopen(cfinal, "wb");
    } else if (j == 2) {
        file_stream[i] = fopen(cfinal, "rb+");
    } else {
        printf("Unknown file mode %d for %s\n", j, cfinal);
        free(cfinal); exit(1);
    }

    if (file_stream[i] == NULL) {
        printf("Cannot open file %s (mode %d)\n", cfinal, j);
        free(cfinal); exit(1);
    }

    strncpy(file_name[i], cfinal, MAXFILENAME - 1);
    file_name[i][MAXFILENAME - 1] = '\0';
    file_mode[i] = j;
    free(cfinal);
}

/* -----------------------------------------------------------------------
 * cclose_ : close a file
 * ----------------------------------------------------------------------- */
void cclose_(int *f_flag)
{
    int i, k;
    i = *f_flag;
    if (i < 0 || i >= MAXFILES || file_stream[i] == NULL) {
        printf("Cannot close file (slot %d): not open.\n", i);
        exit(1);
    }
    fclose(file_stream[i]);
    file_stream[i] = NULL;
    for (k = 0; k < MAXFILENAME; k++) file_name[i][k] = ' ';
    file_mode[i] = -1;
}

/* -----------------------------------------------------------------------
 * cread_ : read bytes from file
 *
 *   readarray  - destination buffer
 *   offs       - byte offset from start of file (INTEGER*8 on Fortran side)
 *   reclen     - number of bytes to read      (INTEGER*8)
 *   recno      - 1-based record number         (INTEGER*8)
 *   f_flag     - file slot                     (INTEGER*4 / int)
 *   f_name     - hidden Fortran char length (ignored)
 *   r_array    - hidden Fortran char length (ignored)
 *
 *   Byte position = (recno-1)*reclen + offs
 * ----------------------------------------------------------------------- */
void cread_(char *readarray,
            int64_t *offs, int64_t *reclen, int64_t *recno,
            int *f_flag,
            int f_name, int r_array)
{
    int i;
    int64_t fw, ff;
    size_t result_read;

    fw  = *reclen;
    ff  = ((*recno) - 1) * fw + (*offs);
    i   = *f_flag;

    if (i < 0 || i >= MAXFILES || file_stream[i] == NULL || file_mode[i] == -1) {
        printf("cread_: file slot %d not open.\n", i);
        exit(1);
    }
    if (file_mode[i] == 1) {
        printf("cread_: file opened write-only.\n");
        exit(1);
    }

    fflush(file_stream[i]);

    if (fseeko(file_stream[i], (off_t)(long long)ff, SEEK_SET) != 0) {
        printf("cread_: fseeko failed at offset %lld in %s\n",
               (long long)ff, file_name[i]);
        exit(1);
    }

    result_read = fread(readarray, 1, (size_t)fw, file_stream[i]);
    if ((int64_t)result_read != fw) {
        printf("cread_: read %zu bytes, expected %lld in %s\n",
               result_read, (long long)fw, file_name[i]);
        exit(1);
    }
}

/* -----------------------------------------------------------------------
 * cwrite_ : write bytes to file  (same argument layout as cread_)
 * ----------------------------------------------------------------------- */
void cwrite_(char *writearray,
             int64_t *offs, int64_t *reclen, int64_t *recno,
             int *f_flag,
             int f_name, int w_array)
{
    int i;
    int64_t fw, ff;
    size_t result_write;

    fw  = *reclen;
    ff  = ((*recno) - 1) * fw + (*offs);
    i   = *f_flag;

    if (i < 0 || i >= MAXFILES || file_stream[i] == NULL || file_mode[i] == -1) {
        printf("cwrite_: file slot %d not open.\n", i);
        exit(1);
    }
    if (file_mode[i] == 0) {
        printf("cwrite_: file opened read-only.\n");
        exit(1);
    }

    if (fseeko(file_stream[i], (off_t)(long long)ff, SEEK_SET) != 0) {
        printf("cwrite_: fseeko failed at offset %lld in %s\n",
               (long long)ff, file_name[i]);
        exit(1);
    }

    result_write = fwrite(writearray, 1, (size_t)fw, file_stream[i]);
    if ((int64_t)result_write != fw) {
        printf("cwrite_: wrote %zu bytes, expected %lld in %s\n",
               result_write, (long long)fw, file_name[i]);
        exit(1);
    }
}
