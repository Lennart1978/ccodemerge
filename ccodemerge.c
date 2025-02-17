#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <errno.h>
#include <sys/stat.h>
#include <stdbool.h>

#define MAX_PATH_LENGTH 4096
#define INITIAL_CAPACITY 128
#define PROGB_WIDTH 50

// List of directories to exclude
static const char *EXCLUDED_DIRS[] = {
    "build",
    "builddir",
    "node_modules",
    "venv",
    "env",
    ".venv",
    ".env",
    ".cache",
    ".idea",
    "cmake-build-debug",
    "dist",
    "target"};

// Number of directories to exclude
static const size_t EXCLUDED_DIRS_COUNT = sizeof(EXCLUDED_DIRS) / sizeof(EXCLUDED_DIRS[0]);

// Flag to track if first file has been processed
bool is_first = true;

// Helper function to check if a directory should be excluded
static bool is_excluded_directory(const char *dirname)
{
    for (size_t i = 0; i < EXCLUDED_DIRS_COUNT; i++)
    {
        if (strcmp(dirname, EXCLUDED_DIRS[i]) == 0)
        {
            return true;
        }
    }
    return false;
}

// Function to copy file content with error handling and formatting
int copyFileContent(FILE *dest, const char *filepath)
{
    struct stat st;
    if (stat(filepath, &st) != 0)
    {
        fprintf(stderr, "Cannot stat file %s: %s\n", filepath, strerror(errno));
        return -1;
    }

    if (st.st_size == 0)
    {
        fprintf(stderr, "Warning: File %s is empty\n", filepath);
        return 0;
    }

    FILE *source = fopen(filepath, "r");
    if (source == NULL)
    {
        fprintf(stderr, "Error opening file: %s (Reason: %s)\n", filepath, strerror(errno));
        return -1;
    }
    is_first ? fprintf(dest, "Content of all files:\n\n") : fprintf(dest, "\n");
    is_first = false;

    fprintf(dest, "File : %s:\n\n", filepath);

    char buffer[4096];
    while (fgets(buffer, sizeof(buffer), source) != NULL)
    {
        if (fputs(buffer, dest) == EOF)
        {
            fprintf(stderr, "Error writing to destination file (Reason: %s)\n", strerror(errno));
            fclose(source);
            return -1;
        }
    }

    if (ferror(source))
    {
        fprintf(stderr, "Error reading file: %s (Reason: %s)\n", filepath, strerror(errno));
        fclose(source);
        return -1;
    }

    fprintf(dest, "\n-------------------------- End of document %s --------------------------\n\n", filepath);

    fclose(source);
    return 0;
}

// Comparison function for qsort
int compareStrings(const void *a, const void *b)
{
    return strcmp(*(const char **)a, *(const char **)b);
}

// Hilfsfunktion, um Dateiendungen zu prüfen
int has_extension(const char *filename, const char *ext)
{
    size_t name_len = strlen(filename);
    size_t ext_len = strlen(ext);

    if (name_len < ext_len)
        return 0;
    return strcmp(filename + name_len - ext_len, ext) == 0;
}

void cleanup(char **files, size_t count)
{
    if (files)
    {
        for (size_t i = 0; i < count; i++)
            free(files[i]);
        free(files);
    }
}

// Function that recursively scans a directory and collects relevant files
int process_directory(const char *base_path, char ***files, size_t *capacity, size_t *count, const char *filename, int is_special_file)
{
    char path[MAX_PATH_LENGTH];
    snprintf(path, sizeof(path), "%s/%s", base_path, filename);

    if (is_special_file || (filename[0] != '.' && (has_extension(filename, ".h") || has_extension(filename, ".c"))))
    {
        if (*count >= *capacity)
        {
            *capacity = *capacity == 0 ? 16 : *capacity * 2;
            char **temp = realloc(*files, *capacity * sizeof(char *));
            if (!temp)
            {
                fprintf(stderr, "Memory allocation failed\n");
                return -1;
            }
            *files = temp;
        }
        (*files)[*count] = strdup(path);
        (*count)++;
    }
    return 0;
}

int scan_directory(const char *dir_path, char ***files, size_t *capacity, size_t *count)
{
    DIR *dir = opendir(dir_path);
    if (dir == NULL)
    {
        fprintf(stderr, "Error opening directory %s (Reason: %s)\n", dir_path, strerror(errno));
        return -1;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL)
    {
        // Skip current and parent directory
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        // Exclude defined directories
        if (is_excluded_directory(entry->d_name))
            continue;

        char path[MAX_PATH_LENGTH];
        snprintf(path, sizeof(path), "%s/%s", dir_path, entry->d_name);

        struct stat statbuf;
        if (stat(path, &statbuf) == -1)
        {
            fprintf(stderr, "Error accessing %s (Reason: %s)\n", path, strerror(errno));
            continue;
        }

        if (S_ISDIR(statbuf.st_mode))
        {
            if (scan_directory(path, files, capacity, count) != 0)
            {
                closedir(dir);
                return -1;
            }
        }
        else
        {
            // Special files are Makefile and meson.build
            int is_special = (strcmp(entry->d_name, "Makefile") == 0 || strcmp(entry->d_name, "meson.build") == 0);
            if (process_directory(dir_path, files, capacity, count, entry->d_name, is_special) != 0)
            {
                closedir(dir);
                return -1;
            }
        }
    }

    closedir(dir);
    return 0;
}

// Shows a progress bar
void print_progress(size_t current, size_t total)
{
    const int bar_width = 50;
    float progress = (float)current / total;
    int pos = PROGB_WIDTH * progress;

    printf("\r[");
    for (int i = 0; i < PROGB_WIDTH; ++i)
    {
        printf(i < pos ? "=" : " ");
    }
    printf("] %3d%%", (int)(progress * 100));
    fflush(stdout);
}

// Main function: Controls the merge process
// 1. Scans the current directory
// 2. Sorts the files
// 3. Performs the merge operation in the following order:
//    Makefiles -> meson.build -> .h -> .c
int main()
{
    char **files = NULL;
    size_t capacity = 0;
    size_t count = 0;
    int status = 0;

    // Scan current directory and all subdirectories
    if (scan_directory(".", &files, &capacity, &count) != 0)
    {
        cleanup(files, count);
        return 1;
    }

    // Sort files
    qsort(files, count, sizeof(char *), compareStrings);

    // Open target file
    FILE *dest = fopen("merged.txt", "w");
    if (dest == NULL)
    {
        fprintf(stderr, "Error creating destination file (Reason: %s)\n", strerror(errno));
        cleanup(files, count);
        return 1;
    }

    size_t processed = 0;
    size_t total = count;

    // 1. Process Makefiles
    for (size_t i = 0; i < count; i++)
    {
        if (strstr(files[i], "Makefile") != NULL)
        {
            if (copyFileContent(dest, files[i]) != 0)
            {
                status = 1;
                break;
            }
            processed++;
            print_progress(processed, total);
        }
    }

    // 2. Process meson.build files
    for (size_t i = 0; i < count; i++)
    {
        if (strstr(files[i], "meson.build") != NULL)
        {
            if (copyFileContent(dest, files[i]) != 0)
            {
                status = 1;
                break;
            }
            processed++;
            print_progress(processed, total);
        }
    }

    // 3. Process .h files
    for (size_t i = 0; i < count; i++)
    {
        if (has_extension(files[i], ".h"))
        {
            if (copyFileContent(dest, files[i]) != 0)
            {
                status = 1;
                break;
            }
            processed++;
            print_progress(processed, total);
        }
    }

    // 4. Process .c files
    for (size_t i = 0; i < count; i++)
    {
        if (has_extension(files[i], ".c"))
        {
            if (copyFileContent(dest, files[i]) != 0)
            {
                status = 1;
                break;
            }
            processed++;
            print_progress(processed, total);
        }
    }

    fclose(dest);
    cleanup(files, count);

    // Ensure progress bar shows 100%
    print_progress(total, total);

    if (status == 0)
        printf("\nFiles successfully merged into 'merged.txt'\n");
    return status;
}
