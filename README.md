FastqType
=========================
A lightweight tool for detecting the library type of FASTQ sequencing data and estimating memory requirements for downstream processing.


__PROGRAM: FastqType__<br>
__VERSION: 2.0.0__<br>
__PLATFORM: Linux / macOS__<br>
__ARCHITECTURE: x86_64__<br>
__COMPILER: gcc (C99)__<br>
__AUTHOR: Xiaolong Zhang__<br>
__EMAIL: zhangxiaolong@big.ac.cn__<br>
__DATE:   2024-04-26__<br>
__UPDATE: 2026-07-20__<br>
__DEPENDENCE__<br>
* __GNU make and gcc__<br>
* __zlib__<br>
* __libbz2__<br>



# 1. Description

* FastqType is a lightweight pre-processing tool for FASTQ sequencing data. It performs two primary functions:
  - **Library type detection:** Identifies whether the input FASTQ files belong to standard paired-end sequencing or single-cell sequencing, and verifies the correctness of single-cell file ordering.
  - **Memory estimation:** Estimates the memory required for a Bloom filter, which is useful for downstream duplicate-removal tools (e.g., FastqCheck).

* It supports both **plain FASTQ** (`.fastq` / `.fq`) and **compressed FASTQ** (`.fastq.gz` / `.fq.gz` / `.fastq.bz2`) as input.

* For single-cell data, FastqType detects read-type tags (`_R1`, `_R2`, `_R3`, `_I1`, `_I2`) embedded in filenames and validates that files are ordered correctly by average read length — a common requirement for single-cell analysis pipelines.



# 2. Building


## 2.1 Dependencies

Before building FastqType, ensure the following libraries are installed:

* **zlib** — required for gzip-compressed file I/O
* **libbz2** — required for bzip2-compressed file I/O

## 2.2 Compilation

```bash
# Build with optimization (release mode, default)
make

# Build with debug symbols
# Edit the makefile and set DEBUG = 1, then run:
make

# Clean build artifacts
make clean
```

The compiled binary `fastq_type` will be generated in the current directory.

## 2.3 Build Options

| Switch  | Default | Description                                                        |
|---------|---------|--------------------------------------------------------------------|
| `DEBUG` | `0`     | `1`: compile with `-g -O0` for debugging; `0`: compile with `-O3` |



# 3. Usage

```
fastq_type <input_list.txt>
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
  SingleCellCheck: Not Single Cell!
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
Memory: <n> GB
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
Memory: 16 GB
```

Or for non-single-cell data:

```
SingleCellCheck: Not Single Cell!
Memory: 8 GB
```



# 6. Error Handling

FastqType reports errors with descriptive tags for easy diagnosis:

| Error Tag      | Description                                               |
|----------------|-----------------------------------------------------------|
| `SysError`     | System-level errors (cannot open file, truncated file)    |
| `FileError`    | File format errors (unsupported format, renamed extension)|
| `FormatError`  | FASTQ format errors (incomplete reads, tag detection)     |

For single-cell data, when file ordering is incorrect, both the expected and observed orders are printed to help users reorder their input files.



# 7. Citation

If you find FastqType useful in your research, please cite the following article:

*To be added upon publication.*
