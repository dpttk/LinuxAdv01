# bldd (Backward LDD)

`bldd` is a tool that shows which executable files use specified shared libraries, effectively doing the reverse of what the `ldd` utility does.

## Features

- Find all executables that depend on specific shared libraries
- Scan directories recursively
- Support for multiple architectures (x86, x86_64, armv7, aarch64)
- Generate reports in TXT or PDF format
- Sort results by usage frequency (high to low)

## Requirements

- GCC compiler
- libmagic development files (`libmagic-dev` package)
- libharu PDF library (`libhpdf-dev` package)

## Installation

1. Install dependencies:

```bash
# On Debian/Ubuntu
sudo apt-get install build-essential libmagic-dev libhpdf-dev
```

2. Build the program:

```bash
make
```

3. Install (optional):

```bash
sudo make install
```

## Usage

```
Usage: bldd [OPTIONS]

bldd (backward ldd) - Find executables that use specific shared libraries

Options:
  -h, --help                 Show this help message and exit
  -l, --lib LIB              Shared library to search for (can be specified multiple times)
  -d, --dir DIR              Directory to scan for executables
  -f, --format FORMAT        Output report format (txt, pdf, both) (default: txt)
  -o, --output FILENAME      Output file name without extension (default: bldd_report)

Examples:
  bldd --lib libc.so.6 --dir /usr/bin --format txt
  bldd --lib libpthread.so --lib libm.so --dir /usr/local/bin
  bldd --lib libc.so.6 --dir /home --format pdf
```

## Example Output

```
Report on dynamic used libraries by ELF executables
------------------------------------------------------------
---------- x86 ----------
libc.so.0.1 (1 execs)
-> /home/user/bin/example1
libc.so.0.3 (1 execs)
-> /home/user/bin/example2

---------- x86_64 ----------
libc.so.6 (5 execs)
-> /usr/bin/example3
-> /usr/bin/example4
-> /usr/bin/example5
-> /usr/bin/example6
-> /usr/bin/example7
```

## Notes

- The scan can be time-consuming for large directories
- Requires appropriate permissions to read files in the scanned directory 