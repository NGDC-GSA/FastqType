/*************************************************************************
    > File Name: file_type.h
    > Author: xlzh
    > Mail: xiaolongzhang2015@163.com
    > Created Time: 2022年11月03日 星期四 09时36分35秒
 ************************************************************************/

#ifndef __FILE_TYPE_H
#define __FILE_TYPE_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <zlib.h>
#include <bzlib.h>
#include "file_read.h"


typedef struct __type_t {
    uint32_t index;
    uint32_t len;
    uint8_t code[8];
    char format[8];
} type_t;


typedef enum {
    TAG_X = -1,  // Unknown tag
    TAG_R1,      // _R1
    TAG_R2,      // _R2
    TAG_R3,      // _R3
    TAG_I1,      // _I1
    TAG_I2       // _I2
} tag_t;


#define TAG_MASK(tag) (1u << (tag))

/* single cell type 1
 * _R1, _R2, _I1, _I2  —  expected avg len order: R2 > R1 > I1 >= I2
 */
#define MASK_TYPE1 \
    ( TAG_MASK(TAG_R1) | \
      TAG_MASK(TAG_R2) | \
      TAG_MASK(TAG_I1) | \
      TAG_MASK(TAG_I2) )

/* single cell type 2
 * _R1, _R2, _R3, _I1  —  expected avg len order: R1 >= R3 > R2 > I1
 */
#define MASK_TYPE2 \
    ( TAG_MASK(TAG_R1) | \
      TAG_MASK(TAG_R2) | \
      TAG_MASK(TAG_R3) | \
      TAG_MASK(TAG_I1) )


typedef struct {
    int file_idx;
    tag_t tag;
    double avg_len;
} info_t;


/*!
  @function: validate file format and detect renamed file extensions
  @param  file_obj  the file object containing file paths
  @param  err_fp    FILE stream for reporting format errors (typically stderr)
  @return           status: 0 -> OK, -1 -> format error detected
  @discussion:
    For each file, reads the first 512 bytes and matches them against
    known magic-number signatures (gz, bz2, zip, rar, tar). Compressed
    files (.gz / .bz2) are further decompressed to verify that the
    underlying content starts with '@' — the FASTQ record marker.

    If the decompressed content does NOT start with '@', the file's
    original extension has likely been renamed (e.g. A.tar.gz -> A.gz).
    In that case the function infers the true format from the magic
    bytes and reports a descriptive error to err_fp.
 */
int file_type_check(const FileObject *file_obj, FILE *err_fp);


/*!
  @function: check single-cell file order by average read length
  @param  file_obj     the file object containing file names
  @param  fastq_cache  cached read data (first CACHE_SIZE reads per file)
  @return              status: 0 -> OK, -1 -> format error
  @discussion:
    Single-cell data files contain read-type tags in their filenames:
      Type 1: _R1, _R2, _I1, _I2  —  expected avg len order: R2 > R1 > I1 >= I2
      Type 2: _R1, _R2, _R3, _I1  —  expected avg len order: R1 >= R3 > R2 > I1

    The function determines the type by detecting the presence of _R3,
    then validates whether the files (sorted by average read length descending)
    match the expected tag order.
 */
int single_cell_check(const FileObject *file_obj, const cache_t *fastq_cache);


#endif //__FILE_TYPE_H
