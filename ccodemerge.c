#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define MAX_PATH_LENGTH 4096
#define PROGB_WIDTH 50
#define VERSION "1.2"

// Data structure to store a list of files with dynamic allocation
typedef struct
{
    char **items;     // Array of file paths
    size_t count;     // Current number of items
    size_t capacity;  // Total capacity of the array
} FileList;

// Enumeration of supported file categories
typedef enum
{
    CAT_MAKEFILE,  // GNU Make files
    CAT_MESON,     // Meson build system files
    CAT_CMAKE,     // CMake build system files
    CAT_AUTOTOOLS, // GNU Autotools files
    CAT_NINJA,     // Ninja build system files
    CAT_BAZEL,     // Bazel build system files
    CAT_QMAKE,     // QMake (Qt) build system files
    CAT_SCONS,     // SCons build system files
    CAT_HEADER,    // C/C++ header files
    CAT_SOURCE,    // C/C++ source files
    CAT_COUNT      // Total number of categories (must be last)
} FileCategory;

// Structure to map filenames to their categories
typedef struct
{
    const char *filename;   // Name or extension of the file
    FileCategory category;  // Corresponding category
} BuildFile;

// Array of known build system files and their categories
static const BuildFile BUILD_FILES[] = {
    {"Makefile", CAT_MAKEFILE},
    {"makefile", CAT_MAKEFILE},
    {"GNUmakefile", CAT_MAKEFILE},
    {"meson.build", CAT_MESON},
    {"meson_options.txt", CAT_MESON},
    {"CMakeLists.txt", CAT_CMAKE},
    {"CMakeCache.txt", CAT_CMAKE},
    {".cmake", CAT_CMAKE}, // Für .cmake Dateien
    {"configure.ac", CAT_AUTOTOOLS},
    {"configure.in", CAT_AUTOTOOLS},
    {"Makefile.am", CAT_AUTOTOOLS},
    {"Makefile.in", CAT_AUTOTOOLS},
    {"build.ninja", CAT_NINJA},
    {".ninja", CAT_NINJA}, // Für .ninja Dateien
    {"WORKSPACE", CAT_BAZEL},
    {"BUILD", CAT_BAZEL},
    {"BUILD.bazel", CAT_BAZEL},
    {".bzl", CAT_BAZEL}, // Für Bazel Skripte
    {".pro", CAT_QMAKE}, // Qt Project Dateien
    {".pri", CAT_QMAKE}, // Qt Include Dateien
    {"SConstruct", CAT_SCONS},
    {"SConscript", CAT_SCONS},
};

// Calculate the number of build file types
static const size_t BUILD_FILES_COUNT = sizeof(BUILD_FILES) / sizeof(BUILD_FILES[0]);

// List of directories to exclude from processing
static const char *EXCLUDED_DIRS[] = {
    ".cache",
    ".env",
    ".idea",
    ".venv",
    "build",
    "builddir",
    "cmake-build-debug",
    "cmake-build-release",    // CMake release build directory
    "dist",
    "env",
    "node_modules",
    "target",
    "venv",
    ".git",
    ".vscode",
    ".vs",                    // Visual Studio directory
    ".pytest_cache",          // Python test cache
    "__pycache__",            // Python bytecode cache
    "out",                    // Common output directory
    "bin",                    // Binary output directory
    "obj",                    // Object files directory
    "Debug",                  // Visual Studio debug build
    "Release",                // Visual Studio release build
    "x64",                    // Visual Studio 64-bit builds
    "x86",                    // Visual Studio 32-bit builds
    "deps",                   // Dependencies directory
    "vendor",                 // Third-party code
    "external",               // External dependencies
    "third_party",            // Third-party libraries
    ".github",                // GitHub specific files
    ".gitlab",                // GitLab specific files
    "coverage",               // Code coverage reports
    "docs/_build",            // Documentation builds
    "logs"                    // Log files directory
};

// Calculate the number of excluded directories
static const size_t EXCLUDED_DIRS_COUNT = sizeof(EXCLUDED_DIRS) / sizeof(EXCLUDED_DIRS[0]);

// Compare function for string sorting (used by qsort)
static int compare_strings(const void *a, const void *b)
{
    return strcmp(*(const char **)a, *(const char **)b);
}

// Prüft, ob ein Verzeichnis ausgeschlossen werden soll (lineare Suche statt bsearch)
static bool is_excluded_dir(const char *dirname)
{
    for (size_t i = 0; i < EXCLUDED_DIRS_COUNT; i++) {
        if (strcmp(dirname, EXCLUDED_DIRS[i]) == 0)
            return true;
    }
    return false;
}

// Check if a filename has a specific file extension
static bool has_extension(const char *filename, const char *ext)
{
    const char *dot = strrchr(filename, '.');
    return dot && strcmp(dot, ext) == 0;
}

// Check if a string ends with a specific suffix
static bool ends_with(const char *str, const char *suffix)
{
    size_t str_len = strlen(str);
    size_t suffix_len = strlen(suffix);

    if (suffix_len > str_len)
    {
        return false;
    }

    return strcmp(str + str_len - suffix_len, suffix) == 0;
}

// Determine the category of a file based on its name or extension
static FileCategory categorize_file(const char *filename)
{
    // First check for build system files
    for (size_t i = 0; i < BUILD_FILES_COUNT; i++)
    {
        // Check for exact filename matches
        if (strcmp(filename, BUILD_FILES[i].filename) == 0)
        {
            return BUILD_FILES[i].category;
        }
        // Check for extension matches (like .cmake, .ninja, .bzl)
        if (BUILD_FILES[i].filename[0] == '.' && ends_with(filename, BUILD_FILES[i].filename))
        {
            return BUILD_FILES[i].category;
        }
    }

    // Then check for header files
    if (has_extension(filename, ".h") ||
        has_extension(filename, ".hpp") ||
        has_extension(filename, ".hxx") ||
        has_extension(filename, ".hh"))
    {
        return CAT_HEADER;
    }

    // Finally check for source files
    if (has_extension(filename, ".c") ||
        has_extension(filename, ".cpp") ||
        has_extension(filename, ".cxx") ||
        has_extension(filename, ".cc"))
    {
        return CAT_SOURCE;
    }

    return CAT_COUNT;  // File type not recognized
}

// Initialize an empty FileList structure
static void init_filelist(FileList *list)
{
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
}

// Free all memory associated with a FileList
static void free_filelist(FileList *list)
{
    for (size_t i = 0; i < list->count; i++)
        free(list->items[i]);
    free(list->items);
    init_filelist(list);
}

// Add a new file path to the FileList, growing the array if needed
static int add_to_filelist(FileList *list, const char *path)
{
    if (list->count >= list->capacity)
    {
        size_t new_cap = list->capacity ? list->capacity * 2 : 16;
        char **tmp = realloc(list->items, new_cap * sizeof(char *));
        if (!tmp)
            return -1;
        list->items = tmp;
        list->capacity = new_cap;
    }

    list->items[list->count] = strdup(path);
    if (!list->items[list->count])
        return -1;
    list->count++;
    return 0;
}

// Resolve a symbolic link to its target path
static char *resolve_symlink(const char *path)
{
    char *resolved = malloc(MAX_PATH_LENGTH);
    if (!resolved)
        return NULL;

    ssize_t len = readlink(path, resolved, MAX_PATH_LENGTH - 1);
    if (len == -1)
    {
        free(resolved);
        return NULL;
    }
    resolved[len] = '\0';

    if (resolved[0] != '/')
    {
        char *dir = strdup(path);
        char *last_slash = strrchr(dir, '/');
        if (last_slash)
        {
            *last_slash = '\0';
            char temp[MAX_PATH_LENGTH];
            snprintf(temp, MAX_PATH_LENGTH, "%s/%s", dir, resolved);
            strcpy(resolved, temp);
        }
        free(dir);
    }

    return resolved;
}

// Process a single file or directory entry
static int process_entry(const char *path, const char *filename, FileList categories[CAT_COUNT])
{
    struct stat st;
    if (lstat(path, &st) == -1)
    {
        fprintf(stderr, "Error accessing %s: %s\n", path, strerror(errno));
        return -1;
    }

    char *actual_path = NULL;
    if (S_ISLNK(st.st_mode))
    {
        actual_path = resolve_symlink(path);
        if (!actual_path)
        {
            fprintf(stderr, "Error resolving symlink %s: %s\n", path, strerror(errno));
            return -1;
        }
        if (stat(actual_path, &st) == -1)
        {
            fprintf(stderr, "Error accessing symlink target %s: %s\n", actual_path, strerror(errno));
            free(actual_path);
            return -1;
        }
    }

    if (S_ISDIR(st.st_mode))
    {
        free(actual_path);
        return 0;
    }

    FileCategory cat = categorize_file(filename);
    if (cat == CAT_COUNT)
    {
        free(actual_path);
        return 0;
    }

    if (filename[0] == '.' &&
        cat != CAT_MAKEFILE &&
        cat != CAT_MESON &&
        cat != CAT_CMAKE &&
        cat != CAT_NINJA &&
        cat != CAT_BAZEL)
    {
        free(actual_path);
        return 0;
    }

    char *abs_path = realpath(actual_path ? actual_path : path, NULL);
    free(actual_path);

    if (!abs_path)
    {
        fprintf(stderr, "Error resolving path %s: %s\n", path, strerror(errno));
        return -1;
    }

    int result = add_to_filelist(&categories[cat], abs_path);
    free(abs_path);
    return result;
}

// Check if any part of the path contains an excluded directory
static bool contains_excluded_dir(const char *path) {
    char *path_copy = strdup(path);
    if (!path_copy) {
        return false;
    }

    bool excluded = false;
    char *token = strtok(path_copy, "/");
    
    while (token != NULL) {
        if (is_excluded_dir(token)) {
            excluded = true;
            break;
        }
        token = strtok(NULL, "/");
    }

    free(path_copy);
    return excluded;
}

// Recursively scan a directory for files to process
static int scan_directory(const char *dir_path, FileList categories[CAT_COUNT])
{
    DIR *dir = opendir(dir_path);
    if (!dir)
    {
        fprintf(stderr, "Error opening %s: %s\n", dir_path, strerror(errno));
        return -1;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)))
    {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        char sub_path[MAX_PATH_LENGTH];
        int written = snprintf(sub_path, MAX_PATH_LENGTH, "%s/%s", dir_path, entry->d_name);
        if (written >= MAX_PATH_LENGTH)
        {
            fprintf(stderr, "Path too long: %s/%s\n", dir_path, entry->d_name);
            continue;
        }

        // Check if the path contains any excluded directory
        if (contains_excluded_dir(sub_path))
            continue;

        if (process_entry(sub_path, entry->d_name, categories) == -1)
        {
            closedir(dir);
            return -1;
        }

        struct stat st;
        if (lstat(sub_path, &st) == -1)
        {
            fprintf(stderr, "Error accessing %s: %s\n", sub_path, strerror(errno));
            continue;
        }

        if (S_ISDIR(st.st_mode))
        {
            if (scan_directory(sub_path, categories) == -1)
            {
                closedir(dir);
                return -1;
            }
        }
    }

    closedir(dir);
    return 0;
}

// Display a progress bar showing the current processing status
static void print_progress(size_t current, size_t total)
{
    if (total == 0)
        return;

    int width = PROGB_WIDTH - 2;
    int pos = (int)((double)current / total * width);
    printf("\r[");
    for (int i = 0; i < width; i++)
        putchar(i < pos ? '=' : ' ');
    printf("] %3zu%%", (current * 100) / total);
    fflush(stdout);
}

// Write a single file's contents to the output file
static int write_file(FILE *dest, const char *path, bool *is_first)
{
    FILE *src = fopen(path, "r");
    if (!src)
    {
        fprintf(stderr, "Error opening %s: %s\n", path, strerror(errno));
        return -1;
    }

    struct stat st;
    if (stat(path, &st) == -1 || st.st_size == 0)
    {
        fclose(src);
        return 0;
    }

    if (*is_first)
    {
        fprintf(dest, "# Created by CCodemerge v%s\n# https://github.com/Lennart1978/ccodemerge\n\n", VERSION);
        *is_first = false;
    }

    fprintf(dest, "\nFile: %s\n\n", path);

    char buffer[8192];
    size_t bytes;
    while ((bytes = fread(buffer, 1, sizeof(buffer), src)) > 0)
    {
        if (fwrite(buffer, 1, bytes, dest) != bytes)
        {
            fprintf(stderr, "Write error for %s: %s\n", path, strerror(errno));
            fclose(src);
            return -1;
        }
    }

    if (ferror(src))
    {
        fprintf(stderr, "Read error for %s: %s\n", path, strerror(errno));
        fclose(src);
        return -1;
    }

    fprintf(dest, "\n-------------------------- End of %s --------------------------\n", path);
    fclose(src);
    return 0;
}

int main(void)
{
    FileList categories[CAT_COUNT];
    for (int i = 0; i < CAT_COUNT; i++)
        init_filelist(&categories[i]);

    if (scan_directory(".", categories) == -1)
    {
        for (int i = 0; i < CAT_COUNT; i++)
            free_filelist(&categories[i]);
        return EXIT_FAILURE;
    }

    FILE *output = fopen("merged.txt", "w");
    if (!output)
    {
        fprintf(stderr, "Error creating output: %s\n", strerror(errno));
        for (int i = 0; i < CAT_COUNT; i++)
            free_filelist(&categories[i]);
        return EXIT_FAILURE;
    }

    bool is_first = true;
    size_t total_files = 0;
    size_t processed = 0;

    for (int i = 0; i < CAT_COUNT; i++)
    {
        qsort(categories[i].items, categories[i].count, sizeof(char *), compare_strings);
        total_files += categories[i].count;
    }

    for (int cat = 0; cat < CAT_COUNT; cat++)
    {
        for (size_t i = 0; i < categories[cat].count; i++)
        {
            if (write_file(output, categories[cat].items[i], &is_first) == -1)
            {
                fclose(output);
                for (int j = 0; j < CAT_COUNT; j++)
                    free_filelist(&categories[j]);
                return EXIT_FAILURE;
            }
            processed++;
            print_progress(processed, total_files);
        }
    }

    fclose(output);
    for (int i = 0; i < CAT_COUNT; i++)
        free_filelist(&categories[i]);

    printf("\nSuccessfully merged %zu files into merged.txt\n", total_files);
    return EXIT_SUCCESS;
}
