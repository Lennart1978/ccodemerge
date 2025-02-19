#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#define MAX_PATH_LENGTH 4096
#define PROGB_WIDTH 50
#define VERSION "1.1"

typedef struct {
    char **items;
    size_t count;
    size_t capacity;
} FileList;

typedef enum {
    CAT_MAKEFILE,
    CAT_MESON,
    CAT_HEADER,
    CAT_SOURCE,
    CAT_COUNT
} FileCategory;

// Alphabetically sorted for binary search
static const char *EXCLUDED_DIRS[] = {
    ".cache",
    ".env",
    ".idea",
    ".venv",
    "build",
    "builddir",
    "cmake-build-debug",
    "dist",
    "env",
    "node_modules",
    "target",
    "venv"};

static const size_t EXCLUDED_DIRS_COUNT = sizeof(EXCLUDED_DIRS) / sizeof(EXCLUDED_DIRS[0]);

static int compare_strings(const void *a, const void *b) {
    return strcmp(*(const char **)a, *(const char **)b);
}

static bool is_excluded_dir(const char *dirname) {
    return bsearch(&dirname, EXCLUDED_DIRS, EXCLUDED_DIRS_COUNT,
                   sizeof(const char *), 
                   (int (*)(const void *, const void *))compare_strings) != NULL;
}

static bool has_extension(const char *filename, const char *ext) {
    const char *dot = strrchr(filename, '.');
    return dot && strcmp(dot, ext) == 0;
}

static void init_filelist(FileList *list) {
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
}

static void free_filelist(FileList *list) {
    for (size_t i = 0; i < list->count; i++)
        free(list->items[i]);
    free(list->items);
    init_filelist(list);
}

static int add_to_filelist(FileList *list, const char *path) {
    if (list->count >= list->capacity) {
        size_t new_cap = list->capacity ? list->capacity * 2 : 16;
        char **tmp = realloc(list->items, new_cap * sizeof(char *));
        if (!tmp) return -1;
        list->items = tmp;
        list->capacity = new_cap;
    }
    
    list->items[list->count] = strdup(path);
    if (!list->items[list->count]) return -1;
    list->count++;
    return 0;
}

static FileCategory categorize_file(const char *filename) {
    if (strcmp(filename, "Makefile") == 0) return CAT_MAKEFILE;
    if (strcmp(filename, "meson.build") == 0) return CAT_MESON;
    if (has_extension(filename, ".h")) return CAT_HEADER;
    if (has_extension(filename, ".c")) return CAT_SOURCE;
    return CAT_COUNT;
}

static int process_entry(const char *path, const char *filename, FileList categories[CAT_COUNT]) {
    struct stat st;
    if (stat(path, &st) == -1) {
        fprintf(stderr, "Error accessing %s: %s\n", path, strerror(errno));
        return -1;
    }

    if (S_ISDIR(st.st_mode)) return 0;

    FileCategory cat = categorize_file(filename);
    if (cat == CAT_COUNT) return 0;

    // Skip hidden files except special cases
    if (filename[0] == '.' && cat != CAT_MAKEFILE && cat != CAT_MESON)
        return 0;

    char *abs_path = realpath(path, NULL);
    if (!abs_path) {
        fprintf(stderr, "Error resolving path %s: %s\n", path, strerror(errno));
        return -1;
    }

    int result = add_to_filelist(&categories[cat], abs_path);
    free(abs_path);
    return result;
}

static int scan_directory(const char *dir_path, FileList categories[CAT_COUNT]) {
    DIR *dir = opendir(dir_path);
    if (!dir) {
        fprintf(stderr, "Error opening %s: %s\n", dir_path, strerror(errno));
        return -1;
    }

    struct dirent *entry;
    while ((entry = readdir(dir))) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;
        
        if (is_excluded_dir(entry->d_name))
            continue;

        char sub_path[MAX_PATH_LENGTH];
        int written = snprintf(sub_path, MAX_PATH_LENGTH, "%s/%s", dir_path, entry->d_name);
        if (written >= MAX_PATH_LENGTH) {
            fprintf(stderr, "Path too long: %s/%s\n", dir_path, entry->d_name);
            continue;
        }

        if (process_entry(sub_path, entry->d_name, categories) == -1) {
            closedir(dir);
            return -1;
        }

        if (entry->d_type == DT_DIR) {
            if (scan_directory(sub_path, categories) == -1) {
                closedir(dir);
                return -1;
            }
        }
    }

    closedir(dir);
    return 0;
}

static void print_progress(size_t current, size_t total) {
    if (total == 0) return;
    
    int width = PROGB_WIDTH - 2;
    int pos = (int)((double)current / total * width);
    printf("\r[");
    for (int i = 0; i < width; i++)
        putchar(i < pos ? '=' : ' ');
    printf("] %3zu%%", (current * 100) / total);
    fflush(stdout);
}

static int write_file(FILE *dest, const char *path, bool *is_first) {
    FILE *src = fopen(path, "r");
    if (!src) {
        fprintf(stderr, "Error opening %s: %s\n", path, strerror(errno));
        return -1;
    }

    struct stat st;
    if (stat(path, &st) == -1 || st.st_size == 0) {
        fclose(src);
        return 0;
    }

    if (*is_first) {
        fprintf(dest, "# Created by CCodemerge v%s\n# https://github.com/Lennart1978/ccodemerge\n\n", VERSION);
        *is_first = false;
    }

    fprintf(dest, "\nFile: %s\n\n", path);

    char buffer[8192];
    size_t bytes;
    while ((bytes = fread(buffer, 1, sizeof(buffer), src)) > 0) {
        if (fwrite(buffer, 1, bytes, dest) != bytes) {
            fprintf(stderr, "Write error for %s: %s\n", path, strerror(errno));
            fclose(src);
            return -1;
        }
    }

    if (ferror(src)) {
        fprintf(stderr, "Read error for %s: %s\n", path, strerror(errno));
        fclose(src);
        return -1;
    }

    fprintf(dest, "\n-------------------------- End of %s --------------------------\n", path);
    fclose(src);
    return 0;
}

int main(void) {
    FileList categories[CAT_COUNT];
    for (int i = 0; i < CAT_COUNT; i++)
        init_filelist(&categories[i]);

    if (scan_directory(".", categories) == -1) {
        for (int i = 0; i < CAT_COUNT; i++)
            free_filelist(&categories[i]);
        return EXIT_FAILURE;
    }

    FILE *output = fopen("merged.txt", "w");
    if (!output) {
        fprintf(stderr, "Error creating output: %s\n", strerror(errno));
        for (int i = 0; i < CAT_COUNT; i++)
            free_filelist(&categories[i]);
        return EXIT_FAILURE;
    }

    bool is_first = true;
    size_t total_files = 0;
    size_t processed = 0;
    
    for (int i = 0; i < CAT_COUNT; i++) {
        qsort(categories[i].items, categories[i].count, sizeof(char *), compare_strings);
        total_files += categories[i].count;
    }

    for (int cat = 0; cat < CAT_COUNT; cat++) {
        for (size_t i = 0; i < categories[cat].count; i++) {
            if (write_file(output, categories[cat].items[i], &is_first) == -1) {
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
