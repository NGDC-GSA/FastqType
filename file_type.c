/*************************************************************************
    > File Name: file_type.c
    > Author: xlzh
    > Mail: xiaolongzhang2015@163.com
    > Created Time: 2022年11月03日 星期四 09时37分12秒
 ************************************************************************/

#include "file_type.h"
#include "file_read.h"


static const int N_TYPE = 5;

static const type_t TYPE_ARRAY[5] = {
        {0, 2, {0x1f, 0x8b}, "gz"},
        {0, 3, {0x42, 0x5a, 0x68}, "bz2"},
        {0, 2, {0x50, 0x4b}, "zip"},
        {0, 4, {0x52, 0x61, 0x72, 0x21}, "rar"},
        {257, 5, {0x75, 0x73, 0x74, 0x61, 0x72}, "tar"}
};


static char *get_file_basename(char *file_path)
{
    char *start = strrchr(file_path, '/');
    if (start != NULL) return start+1;

    return file_path;
}


static const type_t *get_file_format(const unsigned char *cache, int n_byte)
{
    for (int i=0; i < N_TYPE; i++) {
        int flag = 1;
        const type_t *type_obj = &TYPE_ARRAY[i];

        if (n_byte < type_obj->index+type_obj->len)  /* it can not be 'tar' file */
            continue;

        for (int j=0; j < type_obj->len; j++) {
            if (cache[type_obj->index+j] != type_obj->code[j]) {
                flag = 0; break;
            }
        }
        if (flag == 1) return type_obj;
    }
    return NULL;
}


int file_type_check(const FileObject *file_obj, FILE *err_fp)
{
    int status = 0;

    for (int f_idx=0; f_idx < file_obj->n; f_idx++) {
        /* read 512 bytes from the file and check whether the suffix is accurate */
        unsigned char cache[512];
        char *err_fn = get_file_basename(file_obj->file_name[f_idx]);
        const char *file_name = file_obj->file_name[f_idx];

        FILE *fp = fopen(file_name, "rb");
        if (fp == NULL) {
            fprintf(stderr,
                    "[SysError:file_type_check:007] "
                    "failed to open file of %s\n",
                    err_fn);
            exit(-1);
        }
        int n_byte = (int)fread(cache, 1, 512, fp);
        const type_t *type_obj = get_file_format(cache, n_byte);

        if (type_obj == NULL) {  /* not gz, bz2, zip, rar or tar file */
            status = -1;
            fprintf(err_fp,
                    "[FileError:file_type_check:102] "
                    "unsupported file format (.XXX), only file type of (.gz) and (.bz2) is available (Err: %s)!\n",
                    err_fn);
            continue;
        }

        if (strcmp(type_obj->format, "gz") == 0) {  /* is .gz file */
            gzFile gz_fp = gzopen(file_name, "r");
            if (gz_fp <= 0) {
                fprintf(stderr,
                        "[SysError:file_type_check:005] "
                        "failed to open gzip file of %s!\n",
                        err_fn);
                continue;
            }

            n_byte = gzread(gz_fp, cache, 512);
            if (n_byte < 0) {
                fprintf(err_fp,
                        "[FileError:file_type_check:101] "
                        "unexpected end of fastq file %s (truncated file) is detected!\n",
                        err_fn);
                continue;
            }
            if (cache[0] != '@') {  /* the suffix of the filename has been changed */
                status = -1;
                type_obj = get_file_format(cache, n_byte);
                if (type_obj == NULL)  /* unsupported file format (eg. A.exe.gz -> A.gz) */
                    fprintf(err_fp,
                            "[FileError:file_type_check:103] "
                            "unsupported gzip file format of (.XXX.gz), please do not rename the original suffix of the filename (Err: %s)!\n",
                            err_fn);

                else  /* known file format (eg. A.tar.gz->A.gz; A.rar.gz->A.gz; A.zip.gz->A.gz)*/
                    fprintf(err_fp,
                            "[FileError:file_type_check:104] "
                            "the file format of %s may be (.%s.gz), please do not rename the original suffix of the filename!\n",
                            err_fn, type_obj->format);
            }
            gzclose(gz_fp); fclose(fp);
        }
        else if (strcmp(type_obj->format, "bz2") == 0) {  /* is .bz2 file */
            int bz_error=0;
            fseek(fp, 0, SEEK_SET);
            BZFILE *bz2_fp = BZ2_bzReadOpen(&bz_error, fp, 0, 0, NULL, 0);
            if (bz_error!=BZ_OK && bz_error!=BZ_STREAM_END) {
                fprintf(stderr,
                        "[SysError:file_type_check:004] "
                        "can not open bz2 file of %s!\n",
                        err_fn);
                continue;
            }
            n_byte = BZ2_bzRead(&bz_error, bz2_fp, cache, 512);
            if (bz_error!=BZ_OK && bz_error!=BZ_STREAM_END) {
                fprintf(err_fp,
                        "[FileError:file_type_check:101] "
                        "unexpected end of fastq file %s (truncated file) is detected!\n",
                        err_fn);
                continue;
            }
            if (cache[0] != '@') {  /* the suffix of the filename has been changed */
                status = -1;
                type_obj = get_file_format(cache, n_byte);
                if (type_obj == NULL)  /* unsupported file format (eg. A.exe.gz -> A.gz) */
                    fprintf(err_fp,
                            "[FileError:file_type_check:105] "
                            "unsupported bzip2 file format of (.XXX.bz2), please do not rename the original suffix of the filename (Err: %s)!\n",
                            err_fn);

                else  /* known file format (eg. A.tar.gz->A.gz; A.rar.gz->A.gz; A.zip.gz->A.gz)*/
                    fprintf(err_fp,
                            "[FileError:file_type_check:106] "
                            "the file format of %s may be (.%s.bz2), please do not rename the original suffix of the filename!\n",
                            err_fn, type_obj->format);
            }
            BZ2_bzReadClose(&bz_error, bz2_fp);
        }
        else {  /* may be zip, rar, tar */
            status = -1;
            fprintf(err_fp,
                    "[FileError:file_type_check:107] "
                    "the file format of %s may be (.%s), please do not rename the original suffix of the filename!\n",
                    err_fn, type_obj->format);
        }
    }
    return status;
}


static const char *tag_name(const tag_t tag)
{
    switch (tag)
    {
        case TAG_R1: return "R1";
        case TAG_R2: return "R2";
        case TAG_R3: return "R3";
        case TAG_I1: return "I1";
        case TAG_I2: return "I2";
        default:     return "Unknown";
    }
}


static tag_t detect_sc_tag(const char *filename)
{
    const struct {
        const char *text;
        tag_t tag;
        int l_tag;
    } table[] = {
        {.text = "_R1", .tag = TAG_R1, .l_tag = 3},
        {.text = "_R2", .tag = TAG_R2, .l_tag = 3},
        {.text = "_R3", .tag = TAG_R3, .l_tag = 3},
        {.text = "_I1", .tag = TAG_I1, .l_tag = 3},
        {.text = "_I2", .tag = TAG_I2, .l_tag = 3},
    };

    for (size_t i = 0; i < sizeof(table)/sizeof(table[0]); ++i) {
        const char *p = strstr(filename, table[i].text);
        if (!p)
            continue;

        const char next = p[table[i].l_tag];
        if (next == '.' || next == '_')  /* _R1.fq.gz or _R1_001.fq.gz */
            return table[i].tag;
    }

    return TAG_X;
}


static double calc_avg_length(const cache_t *cache)
{
    uint64_t total = 0;
    const read_t *reads = cache->reads;

    for (uint64_t i = 0; i < cache->n; ++i)
        total += reads[i].seq.l;

    return (double)total / (double)cache->n;
}


static int cmp_avg_len(const void *a,const void *b)
{
    const info_t *x = a;
    const info_t *y = b;

    if (x->avg_len < y->avg_len) return 1;
    if (x->avg_len > y->avg_len) return -1;
    return 0;
}


static int single_cell_type1(const info_t info[4])
{
    return
        info[0].tag == TAG_R2 &&
        info[1].tag == TAG_R1 &&
        (
            (info[2].tag == TAG_I1 && info[3].tag == TAG_I2) ||
            (info[2].tag == TAG_I2 && info[3].tag == TAG_I1)
        );
}


static int single_cell_type2(const info_t info[4])
{
    return
        (
            (info[0].tag == TAG_R1 && info[1].tag == TAG_R3) ||
            (info[0].tag == TAG_R3 && info[1].tag == TAG_R1)
        ) &&
        info[2].tag == TAG_R2 &&
        info[3].tag == TAG_I1;
}


int single_cell_check(const FileObject *file_obj, const cache_t *fastq_cache)
{
    /* paired-end single-cell data must have exactly 4 files */
    if (file_obj->n != 4) {
        fprintf(stderr,
                "[FormatError:single_cell_check:213] "
                "expected 4 single-cell files, but got %d!\n",
                file_obj->n);
        return -1;
    }

    uint32_t tag_mask = 0;
    info_t info[4] = {0};

    for (int i=0; i < file_obj->n; i++) {
        const char *file_name = get_path_basename(file_obj->file_name[i]);

        info[i].file_idx = i;
        info[i].tag = detect_sc_tag(file_name);

        if (info[i].tag == TAG_X) {
            fprintf(stderr,
                    "[FormatError:single_cell_check:214] "
                    "unrecognized read tag is detected from %s!\n",
                    file_name);
            return -2;
        }

        if (tag_mask & TAG_MASK(info[i].tag)) {
            fprintf(stderr,
                    "[FormatError:single_cell_check:215] "
                    "duplicated tag (%s) is detected!\n",
                    tag_name(info[i].tag));
            return -3;
        }

        tag_mask |= TAG_MASK(info[i].tag);
        info[i].avg_len = calc_avg_length(&fastq_cache[i]);
    }

    const char *type_name = NULL;
    const char *expect_order = NULL;
    int (*check_func)(const info_t *) = NULL;

    /* determine library type */
    switch (tag_mask) {
        case MASK_TYPE1:
            type_name = "Single Cell Type1 (R1/R2/I1/I2)";
            expect_order = "R2 > R1 > I1 >= I2";
            check_func = single_cell_type1;
            break;

        case MASK_TYPE2:
            type_name = "Single Cell Type2 (R1/R2/R3/I1)";
            expect_order = "R1 >= R3 > R2 > I1";
            check_func = single_cell_type2;
            break;

        default:
            fprintf(stderr,
                    "[FormatError:single_cell_check:216] "
                    "unsupported single-cell tag combination!\n");
            return -4;
    }

    qsort(info, 4, sizeof(info_t), cmp_avg_len);

    if (!check_func(info)) {
        fprintf(stderr,
                "[FormatError:single_cell_check:217] "
                "incorrect single-cell file order detected!\n");

        fprintf(stderr, "[*] Library Type : %s\n", type_name);
        fprintf(stderr, "[*] Expected     : %s\n", expect_order);
        fprintf(stderr,
                "[*] Observed     : %s(%.1f) > %s(%.1f) > %s(%.1f) > %s(%.1f)\n",
                tag_name(info[0].tag), info[0].avg_len,
                tag_name(info[1].tag), info[1].avg_len,
                tag_name(info[2].tag), info[2].avg_len,
                tag_name(info[3].tag), info[3].avg_len);

        return -5;
    }

    return 0;
}
