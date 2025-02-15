# CCodemerge

CCodemerge is a command-line utility that merges multiple C source files into a single text file. It recursively scans directories for C source files (.c), header files (.h), Makefiles, and meson.build files, then combines them in a structured manner.

## Features

- Recursively scans directories for relevant files
- Automatically excludes common build and dependency directories
- Processes files in a specific order:
  1. Makefiles
  2. meson.build files
  3. Header files (.h)
  4. Source files (.c)
- Sorts files alphabetically within each category
- Adds clear file separators and formatting
- Handles errors gracefully with detailed error messages

## Building

### Prerequisites

- GCC compiler
- Make build system

### Compilation

```bash
# Standard optimized build
make

# Debug build
make debug
```

### Installation (Optional)

```bash
sudo make install
```

## Usage

Simply run the program in the root directory of your C project:

```bash
./ccodemerge
```

The program will create a `merged.txt` file containing all the merged source code.

## Output Format

The merged output file follows this structure:

- A header indicating the start of the merged content
- Each file is preceded by its path
- Clear separators between files
- Files are organized by type (Makefiles → meson.build → .h → .c)

## Excluded Directories

The following directories are automatically excluded from processing:

- build/builddir
- node_modules
- venv/env/.venv/.env
- .cache
- .idea
- cmake-build-debug
- dist
- target

## Building from Source

1. Clone the repository
2. Run `make` to build with optimizations
3. (Optional) Run `make install` to install system-wide

## License

MIT License

## Author

2025 Lennart Martens
