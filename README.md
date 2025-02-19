# CCodemerge

CCodemerge is a command-line utility that merges multiple C/C++ source files into a single text file. It recursively scans directories for C/C++ source and header files and all well known build system files. It identifies and categorizes these files,then combines them in a structured manner into a single output file for easy review or analysis (by AI).

## Features

- Recursively scans directories for relevant files
- Automatically excludes common build and dependency directories
- Processes files in a specific order:
  1. Build system files (Makefiles, CMakeLists.txt, etc.)
  2. Header files (.h)
  3. Source files (.c)
- Sorts files alphabetically within each category
- Adds clear file separators and formatting
- Handles errors gracefully with detailed error messages
- Provides a progress bar during processing
- Can handle symbolic links correctly

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
- Files are organized by type (build system → header → sourcecode files)

## Excluded Directories

The following directories are automatically excluded from processing:

".cache",
".env",
".idea",
".venv",
"build",
"builddir",
"cmake-build-debug",
"cmake-build-release",
"dist",
"env",
"node_modules",
"target",
"venv",
".git",
".vscode",
".vs",
".pytest_cache",
"__pycache__",
"out",
"bin",
"obj",
"Debug",
"Release",
"x64",
"x86",
"deps",
"vendor",
"external",
"third_party",
".github",
".gitlab",
"coverage",
"docs/_build",
"logs"

## Building from Source

1. Clone the repository
2. Run `make` to build with optimizations
3. (Optional) Run `make install` to install system-wide

## License

MIT License

## Author

2025 Lennart Martens
