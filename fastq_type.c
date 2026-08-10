/*************************************************************************
    > File Name: fastq_type.c
    > Author: xlzh
    > Mail: xiaolongzhang2015@163.com
    > Created Time: 2024年04月26日 星期五 10时30分32秒
 ************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

#include "file_read.h"
#include "file_type.h"

#define CACHE_SIZE 100000  /* number of cached read object for each file */
#define BLOOM_ERROR 0.000000001  /* probability of false positive for bloomfilter */
#define MAX_READ_LENGTH 52428800  /* 50MB */
#define COMPRESS_RATIO 10.0  /* default compression ratio */
#define FASTQ_TYPE_VERSION "2.0.2"


/*! @typedef phred_t
  @abstract the phred object for phred check
  @field  phred             the phred value of the sequence
  @field  qual_table        table of base quality distribution
 */
typedef struct {
    uint64_t phred;
    uint64_t qual_table[256];
} phred_t;


/*! @typedef estimate_t
  @abstract the memory estimate of the bloom filter
  @field  mem_size          the estimated memory size of the bloom filter (in GB)
  @field  max_reads         the estimated maximum number of reads of the bloom filter (in million)
 */
typedef struct {
    uint64_t mem_size;
    uint64_t max_reads;
} estimate_t;


static char *get_current_time(char *time_buf)
{
    time_t c_time;
    time(&c_time);
    const struct tm *tm_obj = gmtime(&c_time);

    const int year = tm_obj->tm_year + 1900;
    const int month = tm_obj->tm_mon + 1;
    const int day = tm_obj->tm_mday;
    const int hour = tm_obj->tm_hour + 8;
    const int minute = tm_obj->tm_min;
    const int second = tm_obj->tm_sec;

    sprintf(time_buf, "%d-%d-%d %d:%d:%d", year, month, day, hour, minute, second);
    return time_buf;
}


static long get_max_file_size(const FileObject *file_obj, int *max_file_idx)
{
    long file_size = -1;

    for (int idx=0; idx < file_obj->n; idx++) {
        FILE *fp = fopen(file_obj->file_name[idx], "r");
        if (fp == NULL) {
            char *err_fn = get_path_basename(file_obj->file_name[idx]);
            fprintf(stderr, "[SysError:get_file_size:019] failed to read the given file %s\n", err_fn);
            return -1;
        }
        fseek(fp, 0L, SEEK_END);
        const long f_size = ftell(fp);
        if (f_size > file_size) {
            file_size = f_size; *max_file_idx = idx;
        }
        fclose(fp);
    }
    return file_size;
}


static estimate_t bloom_memory_estimate(const FileObject *file_obj, const cache_t *fastq_cache, const int compress_ratio)
{
    estimate_t est;

    /* get the max_file_size */
    int max_file_idx;
    const long file_size = get_max_file_size(file_obj, &max_file_idx);
    const read_t *f_reads = fastq_cache[max_file_idx].reads;
    const uint64_t f_reads_num = fastq_cache[max_file_idx].n;

    /* get the minimum and maximum read length*/
    uint64_t n_total_bytes = 0;
    uint32_t min_read_len = 1<<30;
    uint32_t max_read_len = 0;
    uint32_t min_read_byte = 1<<30;
    for (uint64_t i=0; i < f_reads_num; i++) {
        const read_t *read = &f_reads[i];
        const uint32_t n_read_byte = read->name.l + read->seq.l + read->comment.l + read->qual.l + 4;
        
        n_total_bytes += n_read_byte;
        if (n_read_byte < min_read_byte) min_read_byte = n_read_byte;

        /* get the minimum and maximum length of the read */
        if (read->seq.l < min_read_len) min_read_len = read->seq.l;
        if (read->seq.l > max_read_len) max_read_len = read->seq.l;
    }

    /* evaluate the number of reads with best way */
    const uint64_t avg_read_byte = max_read_len > min_read_len ? min_read_byte : n_total_bytes / f_reads_num;
    const uint64_t max_item = file_size / avg_read_byte * compress_ratio;

    /* m = -1 * (n * ln(p)) / ln(2)^2 */
    uint64_t mem_size = (uint64_t)ceil(-1 * log(BLOOM_ERROR) * (double)max_item / 0.6185);  /* in bits */
    mem_size = mem_size / 8 / 1073741824;  /* in GB */
    est.mem_size = mem_size - mem_size % 5 + 5;
    est.max_reads = max_item / 1000000;  /* convert to million */

    return est;
}


static cache_t *fastq_cache_init(const int n_file)
{
    cache_t *fastq_cache = calloc(n_file, sizeof(cache_t));
    for (int i=0; i < n_file; i++) {
        cache_t *fc = &fastq_cache[i];
        fc->n_max = CACHE_SIZE;
        fc->reads = (read_t *)calloc(fc->n_max, sizeof(read_t));
    }
    return fastq_cache;
}


/* read four line from the input fastq file */
static int fastq_read_core(GzStream *gz, read_t *read)
{
    int ret = gz_read_util(gz, '\n', &read->name, MAX_READ_LENGTH);  /* get the read name; normal return is: ret==1 */
    switch (ret) {
        case -2: return -3;  /* failed to detect line breaks('\n') */
        case -1: return -1;  /* unexpected end of the file */
        case  0: return 0;  /* end of the file or empty file */
        default: break;  /* normal reading the file (ret==1) */
    }

    ret = gz_read_util(gz, '\n', &read->seq, MAX_READ_LENGTH);  /* get the read sequence */
    switch (ret) {
        case -2: return -3;  /* failed to detect line breaks('\n') */
        case -1: return -1;  /* unexpected end of the file */
        case  0: return -2;  /* incomplete fastq reads, since only one line is readed */
        default: break;  /* normal reading the file (ret==1) */
    }

    ret = gz_read_util(gz, '\n', &read->comment, MAX_READ_LENGTH); /* get the read comment, usually is '+' */
    switch (ret) {
        case -2: return -3;  /* failed to detect line breaks('\n') */
        case -1: return -1;  /* unexpected end of the file */
        case  0: return -2;  /* incomplete fastq reads, since only two lines are readed */
        default: break;  /* normal reading the file (ret==1) */
    }

    ret = gz_read_util(gz, '\n', &read->qual, MAX_READ_LENGTH); /* get the read quality */
    switch (ret) {
        case -2: return -3;  /* failed to detect line breaks('\n') */
        case -1: return -1;  /* unexpected end of the file */
        case  0: return -2;  /* incomplete fastq reads, since only three lines are readed */
        default: break;  /* normal reading the file  (ret==1)*/
    }

    if (read->name.s[0]=='@' && read->comment.s[0]=='+')
        return ret;  /* means read status is OK */
    
    return -2; /* incomplete fastq reads */
}


static int fastq_cache_read(const FileObject *file_obj, cache_t *fastq_cache)
{
    uint64_t line_num;

    for (int i=0; i < file_obj->n; i++) {
        int finish = 0;
        GzStream *f_gz = file_obj->gz_hd[i];
        cache_t *f_cache = &fastq_cache[i]; f_cache->n = 0;

        for (int idx=0; idx < f_cache->n_max && finish != 1; idx++) {
            const int ret = fastq_read_core(f_gz, &f_cache->reads[idx]);
            switch (ret) {
            case 0:
                finish = 1; break;  /* end of the file (EOF) */
            case -1:
                return -1;  /* unexpected end of fastq file */
            case -2:
                line_num = f_cache->n * 4 + 1;
                fprintf(stderr, "[FormatError:fastq_cache_read:201] incomplete fastq read '%s' is detected!\n", f_cache->reads[idx].name.s);
                fprintf(stderr, "[*] File%d: the line number of the error read is %lld!\n", i+1, line_num);
                return -2;
            case -3: /* failed to detect line breaks('\n') */
                line_num = f_cache->n * 4 + 1;
                fprintf(stderr, "[FormatError:fastq_cache_read:212] failed to detect line breaks('\\n') in the READ!\n");
                fprintf(stderr, "[*] File%d: the line number of the error read is %lld!\n", i+1, line_num);
                return -3;
            default:
                f_cache->n++; break;  /* normal reading of the fastq file (ret==1) */
            }
        }
    }
    /* check whether all caches have same number of reads */
    for (int i=1; i < file_obj->n; i++) {
        if(fastq_cache[i].n != fastq_cache[0].n) {
            fprintf(stderr, "[FormatError:fastq_cache_read:202] the number of reads cached is different!\n");
            fprintf(stderr, "[*] File1: %lld  <--> File%d: %lld\n", (int64_t)fastq_cache[0].n, i+1, (int64_t)fastq_cache[i].n);
            return -4;
        }
    }
    /* check whether all the files have been finished */
    if (fastq_cache[0].n < fastq_cache[0].n_max) return 0;
    return 1;  /* status is OK, could caching for next time */
}


static int windows_break_check(const cache_t *fastq_cache, const int n_file)
{
    read_t *f_reads;
    kstring_t *r_seq;  /* sequence of the read */

    for (int idx=0; idx < n_file; idx++) {
        f_reads = fastq_cache[idx].reads;

        for (int i=0; i < fastq_cache[idx].n; i++) {
            r_seq = &f_reads[i].seq;
            if (r_seq->s[r_seq->l-1] == '\r') goto error;
        }
    }
    return 1;

    error:
        fprintf(stderr,
                "[FormatError:windows_break_check:203] "
                "windows break ('\\r\\n') is detected in the fastq file!\n");
    return 0;
}


static int quality_phred_check(const cache_t *fastq_cache, const int n_file)
{
    uint64_t *f_qual_table;  /* quality table of the file */
    phred_t *phred_obj = calloc(n_file, sizeof(phred_t));

    /* quality of the read */
    for (int idx=0; idx < n_file; idx++) {
        f_qual_table = phred_obj[idx].qual_table;
        const read_t *f_reads = fastq_cache[idx].reads;

        /* record the quality and its number */
        for (int i=0; i < fastq_cache[idx].n; i++) {
            const kstring_t *r_qual = &f_reads[i].qual;
            for (int j=0; j < r_qual->l; j++) f_qual_table[r_qual->s[j]]++;
        }
    }

    /* get the phred of each file */
    for (int idx=0; idx < n_file; idx++) {
        uint64_t phred33 = 0;
        uint64_t phred64 = 0;
        f_qual_table = phred_obj[idx].qual_table;
        for (int i=33; i < 59; i++) phred33 += f_qual_table[i];  /* only existed in phred33 */
        for (int i=75; i < 104; i++) phred64 += f_qual_table[i];  /* only existed in phred64 */

        /* check phred and clean the temp value within  f_qual_table */
        if (phred64 > 0 && phred33 == 0) phred_obj[idx].phred = 64;
    }

    /* check whether the phred of all files are same */
    for (int idx=1; idx < n_file; idx++) {
        /* different phred existed */
        if (phred_obj[idx].phred != phred_obj[0].phred) {
            fprintf(stderr, "[FormatError:phred_check:204] the phred value of the given files is different!\n");
            for (int f_idx=0; f_idx < n_file; f_idx++) {
                fprintf(stderr, "  [*] File %d: Phred%lld\n", f_idx, phred_obj[f_idx].phred);
            }
            return 0;
        }
    }
    return 1;
}


int main(const int argc, char **argv)
{
    char buf[32];
    int status = 0;

    if (argc < 2) {
        fprintf(stderr, "Author: XiaolongZhang (zhangxiaolong@big.ac.cn)\n");
        fprintf(stderr, "usage: fastq_type (v%s) <input_list.txt>\n", FASTQ_TYPE_VERSION);
        exit(-1);
    }

    const FileObject *file_obj = read_file_list(argv[1]);
    if (file_type_check(file_obj, stderr) < 0) {
        exit(-2);
    }

    cache_t *fastq_cache = fastq_cache_init(file_obj->n);
    status = fastq_cache_read(file_obj, fastq_cache);
    if (status < 0) {
        fprintf(stderr, "[%s] Failed to read the fastq file!\n", get_current_time(buf));
        exit(-3);
    }

    /* check the windows breaker and phred distribution */
    windows_break_check(fastq_cache, file_obj->n);
    quality_phred_check(fastq_cache, file_obj->n);

    if (file_obj->n <= 2)  /* normal single-end or pair-end fastq file */
        fprintf(stdout, "SingleCell: Not Single Cell!\n");

    else {  /* check whether the order of the single-cell files is correct */
        if (single_cell_check(file_obj, fastq_cache) < 0) {
            fprintf(stdout, "SingleCell: Check Failed!\n");
            exit(-3);
        }
        fprintf(stdout, "SingleCell: Check Passed!\n");
    }

    /* estimate the memory needed by the bloom filter */
    const estimate_t est = bloom_memory_estimate(file_obj, fastq_cache, COMPRESS_RATIO);
    fprintf(stdout, "MaximumReads: %llu (Million)\n", est.max_reads);
    fprintf(stdout, "BloomMemory: %llu (GB)\n", est.mem_size);
}
