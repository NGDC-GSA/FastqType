FastqType
=========================
A lightweight tool for detecting the library type of FASTQ sequencing data and estimating memory requirements for downstream processing.


__PROGRAM: FastqType__<br>
__VERSION: 2.0.6__<br>
__PLATFORM: Linux / macOS / Windows__<br>
__ARCHITECTURE: x86_64 / ARM64__<br>
__COMPILER: gcc / clang (C99)__<br>
__AUTHOR: Xiaolong Zhang__<br>
__EMAIL: zhangxiaolong@big.ac.cn__<br>
__DATE:   2024-04-26__<br>
__UPDATE: 2026-09-01__<br>
__DEPENDENCE__<br>
* __cmake (>= 3.16) and a C compiler (gcc or clang)__<br>
* __zlib / libbz2 are bundled in the `external/` directory and built from source, no system library is needed__<br>



# 1. Description

* FastqType is a lightweight pre-processing tool for FASTQ sequencing data. It performs two primary functions:
  - **Library type detection:** Identifies whether the input FASTQ files belong to standard paired-end sequencing or single-cell sequencing, and verifies the correctness of single-cell file ordering.
  - **Memory estimation:** Estimates the memory required for a Bloom filter, which is useful for downstream duplicate-removal tools (e.g., FastqCheck).

* It is **cross-platform**, running on Linux, macOS and Windows (x86_64 / ARM64).

* It supports both **plain FASTQ** (`.fastq` / `.fq`) and **compressed FASTQ** (`.fastq.gz` / `.fq.gz` / `.fastq.bz2`) as input.

* For single-cell data, FastqType detects read-type tags (`_R1`, `_R2`, `_R3`, `_I1`, `_I2`) embedded in filenames and validates that files are ordered correctly by average read length — a common requirement for single-cell analysis pipelines.



# 2. Building


## 2.1 Dependencies

All dependencies (zlib-ng and bzip2) are bundled in the `external/` directory and
built from source together with FastqType, so no system library is required.
Only a C compiler and CMake are needed:

* **CMake** (>= 3.16)
* **A C compiler** (gcc or clang, C99)

## 2.2 Compilation

```bash
# Configure + build (Release mode by default)
cmake -S . -B build
cmake --build build -j

# Build with debug symbols
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j

# Clean build artifacts
rm -rf build
```

The compiled binary `fastqtype` will be generated at `build/fastqtype`.

## 2.3 Prebuilt Binaries

Prebuilt binaries for Linux (amd64/arm64, fully static musl), macOS (amd64/arm64)
and Windows (amd64/arm64) are published with each release — see the
[Releases](https://github.com/NGDC-GSA/FastqType/releases) page. Building from
source is only needed if you want to customize the build.

## 2.4 Build Options

| Option              | Default   | Description                                        |
|---------------------|-----------|----------------------------------------------------|
| `CMAKE_BUILD_TYPE`  | `Release` | `Release`: compile with `-O3`; `Debug`: compile with `-g -O0` |



# 3. Usage

```
fastqtype <input_list.txt>
```

**Arguments:**

| Argument         | Description                                                         |
|------------------|---------------------------------------------------------------------|
| `input_list.txt` | A text file containing the full paths of FASTQ files, one per line. |

**Example `input_list.txt`:**
```
/home/user/data/sample_R1.fastq.gz
/home/user/data/sample_R2.fastq.gz
/home/user/data/sample_I1.fastq.gz
/home/user/data/sample_I2.fastq.gz
```



# 4. What It Does

## 4.1 File Format Validation

Before processing, FastqType validates each input file by reading its magic-number signature (first few bytes) to confirm the actual file format. It detects the following types:

| Format | Magic Bytes (hex)                  |
|--------|------------------------------------|
| gzip   | `1f 8b`                            |
| bzip2  | `42 5a 68`                         |
| zip    | `50 4b`                            |
| rar    | `52 61 72 21`                      |
| tar    | `75 73 74 61 72` (at offset 257)   |

For `.gz` and `.bz2` files, it further decompresses and checks that the content starts with `@` (the FASTQ record marker). If the extension has been renamed (e.g., `data.tar.gz` → `data.gz`), a descriptive error is reported.

## 4.2 Library Type Detection

FastqType determines the library type based on the number of input files:

- **1 or 2 files:** Standard single-end or paired-end FASTQ data. Outputs:
  ```
  SingleCell: Not Single Cell!
  ```

- **4 files:** Treated as single-cell data. The program detects the library type from filename tags and validates file ordering:

  | Library Type | Required Tags     | Expected Order (by avg read length) |
  |--------------|-------------------|-------------------------------------|
  | Type 1       | R1, R2, I1, I2    | R2 > R1 > I1 ≥ I2                   |
  | Type 2       | R1, R2, R3, I1    | R1 ≥ R3 > R2 > I1                   |

  On success:
  ```
  SingleCell: Check Passed!
  ```

  On failure, the expected and observed orders are printed for diagnosis.

## 4.3 Memory Estimation

FastqType estimates the memory (in GB) needed for a Bloom filter based on the largest input file size and average read length. This helps users plan resource allocation for downstream deduplication tools.

```
MaximumReads: <n> (Million)
BloomMemory: <n> (GB)
```



# 5. Input and Output


## 5.1 FASTQ Input Format

FastqType accepts both plain FASTQ (`.fastq`, `.fq`) and compressed FASTQ (`.fastq.gz`, `.fq.gz`, `.fastq.bz2`, `.fq.bz2`) as input. Each FASTQ file should follow the standard four-line-per-read format:

```
@read_identifier
ACGTACGTACGT...
+
IIIIIIIIIIII...
```

## 5.2 Filename Convention for Single-Cell Data

Single-cell FASTQ files must include one of the following tags in their filenames, immediately followed by a dot (`.`) or file extension:

| Tag | Meaning      |
|-----|--------------|
| _R1 | Read 1       |
| _R2 | Read 2       |
| _R3 | Read 3       |
| _I1 | Index 1      |
| _I2 | Index 2      |

**Valid examples:**
- `sample_R1.fastq.gz`
- `sample_R2.fq.gz`
- `sample_I1.fastq`

## 5.3 Output

A typical successful run prints something like:

```
SingleCell: Check Passed!
PhredValue: 33
MaximumReads: 123 (Million)
BloomMemory: 16 (GB)
```

Or for non-single-cell data:

```
SingleCell: Not Single Cell!
PhredValue: 33
MaximumReads: 456 (Million)
BloomMemory: 8 (GB)
```



# 6. Error Handling

FastqType reports errors to stderr in the format `[<Type>:<function>:<code>] <message>`, e.g.:

```
[SysError:gz_stream_open:008] failed to open gzip file of (sample_R1.fastq.gz)!
[FormatError:fastq_cache_read:201] incomplete fastq read '@read1' is detected!
```

| Error Tag     | Description                                                |
|---------------|------------------------------------------------------------|
| `SysError`    | System-level errors (cannot open file, out of memory, ...) |
| `FileError`   | File format errors (unsupported format, renamed extension, truncated file) |
| `FormatError` | FASTQ format errors (incomplete reads, phred mismatch, single-cell ordering) |

## 6.1 SysError (system-level errors, 001 - 019)

| Code | Origin          | Message                                                                 |
|------|-----------------|-------------------------------------------------------------------------|
| `001` | `file_name_copy` | failed to malloc memory when copy file name `XXX`!                    |
| `002` | `read_file_list` | failed to open the file list of `XXX`                                 |
| `003` | `gz_stream_open` | operate mode(`XXX`) error, it should be "w" or "r".                   |
| `004` | `gz_stream_open` | can not open bz2 file of (`XXX`)!                                     |
| `005` | `gz_stream_open` | the file of (`XXX`) does not seem to be a .bz2 file!                  |
| `006` | `gz_stream_open` | the file of (`XXX`) is neither a real bzip2(.bz2) file nor a stander fastq file! |
| `007` | `gz_stream_open` | failed to open file of (`XXX`)!                                       |
| `008` | `gz_stream_open` | failed to open gzip file of (`XXX`)!                                  |
| `009` | `gz_stream_open` | the file of (`XXX`) is neither a real gzip(.gz) file nor a stander fastq file! |
| `010` | `gz_stream_open` | failed to create file of (`XXX`) in .gz format!                       |
| `011` | `gz_stream_open` | failed to create file of (`XXX`)!                                     |
| `012` | `gz_stream_open` | failed to create file of (`XXX`) in .bz2 format!                      |
| `013` | `gz_stream_open` | failed to create a normal file of (`XXX`)!                            |
| `014` | `gz_read_util`   | read length can not be longer than `XXX`!                             |
| `015` | `gz_read_util`   | failed to reallocated memory!                                         |
| `016` | `gz_stream_open` | failed to open a normal file of (`XXX`)!                              |
| `019` | `get_max_file_size` | failed to read the given file `XXX`                                |

## 6.2 FileError (file format errors, 101 - 107)

| Code | Origin          | Message                                                                 |
|------|-----------------|-------------------------------------------------------------------------|
| `101` | `file_type_check` | unexpected end of fastq file `XXX` (truncated file) is detected!      |
| `102` | `file_type_check` | unsupported file format (.XXX), only file type of (.gz) and (.bz2) is available (Err: `XXX`)! |
| `103` | `file_type_check` | unsupported gzip file format of (.XXX.gz), please do not rename the original suffix of the filename (Err: `XXX`)! |
| `104` | `file_type_check` | the file format of `XXX` may be (.`XXX`.gz), please do not rename the original suffix of the filename! |
| `105` | `file_type_check` | unsupported bzip2 file format of (.XXX.bz2), please do not rename the original suffix of the filename (Err: `XXX`)! |
| `106` | `file_type_check` | the file format of `XXX` may be (.`XXX`.bz2), please do not rename the original suffix of the filename! |
| `107` | `file_type_check` | the file format of `XXX` may be (.`XXX`), please do not rename the original suffix of the filename! |

## 6.3 FormatError (fastq format errors, 201 - 217)

| Code | Origin                  | Message                                                                 |
|------|-------------------------|-------------------------------------------------------------------------|
| `201` | `fastq_cache_read`      | incomplete fastq read 'XXX' is detected!                               |
| `202` | `fastq_cache_read`      | the number of reads cached is different!                               |
| `203` | `windows_break_check`   | windows break ('\r\n') is detected in the fastq file!                  |
| `204` | `phred_check`           | the phred value of the given files is different!                       |
| `212` | `fastq_cache_read`      | failed to detect line breaks('\n') in the READ!                        |
| `213` | `single_cell_check`     | expected 4 single-cell files, but got `XXX`!                           |
| `214` | `single_cell_check`     | unrecognized read tag is detected from `XXX`!                          |
| `215` | `single_cell_check`     | duplicated tag (`XXX`) is detected!                                    |
| `216` | `single_cell_check`     | unsupported single-cell tag combination!                               |
| `217` | `single_cell_check`     | incorrect single-cell file order detected!                             |

**Notes:**

* Codes `205` - `211` are reserved by the downstream FastqCheck tool and are not emitted by FastqType.
* For single-cell data, when file ordering is incorrect (code `217`), both the expected and observed orders (with average read lengths) are printed to help users reorder their input files.



# 7. Citation

If you find FastqType useful in your research, please cite the following article:

*To be added upon publication.*
