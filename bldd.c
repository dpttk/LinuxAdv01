#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdbool.h>
#include <limits.h>
#include <libgen.h>
#include <errno.h>

// Define PDF_SUPPORT to 0 if you don't have libhpdf installed
#ifndef PDF_SUPPORT
#define PDF_SUPPORT 1
#endif

#if PDF_SUPPORT
#include <hpdf.h>
#endif

#define MAX_LIBS 100
#define MAX_PATH 4096
#define MAX_EXECS 10000
#define MAX_LINE 1024
#define MAX_ARCH 4
#define MAX_HEADER_SIZE 100
#define SEPARATOR "----------"

// Structure to hold library information
typedef struct {
    char name[256];
    char *execs[MAX_EXECS];
    int exec_count;
} Library;

// Structure to hold architecture information
typedef struct {
    char name[32];
    Library libraries[MAX_LIBS];
    int lib_count;
} Architecture;

// Program options
typedef struct {
    char **libs;
    int lib_count;
    char dir[MAX_PATH];
    char output[MAX_PATH];
    bool txt_format;
    bool pdf_format;
} Options;

// Global variables
Architecture archs[MAX_ARCH];
int arch_count = 0;
int total_execs = 0;

// Function prototypes
void parse_arguments(int argc, char *argv[], Options *options);
void print_help();
void scan_directory(const char *dir_path, Options *options);
bool is_executable(const char *file_path);
const char *get_architecture(const char *file_path);
void get_dependencies(const char *file_path, char **libs, int lib_count, const char *arch);
int find_or_add_architecture(const char *arch);
int find_or_add_library(int arch_index, const char *lib_name);
void add_executable(int arch_index, int lib_index, const char *exec_path);
void generate_txt_report(Options *options);
void generate_pdf_report(Options *options);
void cleanup();
void error_handler(HPDF_STATUS error_no, HPDF_STATUS detail_no, void *user_data);

int main(int argc, char *argv[]) {
    Options options;
    
    // Initialize
    memset(&options, 0, sizeof(Options));
    memset(archs, 0, sizeof(archs));
    
    // Parse command line arguments
    parse_arguments(argc, argv, &options);
    
    // Scan the directory
    scan_directory(options.dir, &options);
    
    // Generate reports
    if (options.txt_format) {
        generate_txt_report(&options);
    }
    
    if (options.pdf_format) {
        generate_pdf_report(&options);
    }
    
    // Display summary
    printf("Summary: Found %d executables across %d architectures\n", 
           total_execs, arch_count);
    
    // Cleanup
    cleanup();
    
    // Free allocated memory for libraries
    for (int i = 0; i < options.lib_count; i++) {
        free(options.libs[i]);
    }
    free(options.libs);
    
    return 0;
}

void parse_arguments(int argc, char *argv[], Options *options) {
    int i;
    bool dir_set = false;
    
    // Allocate memory for library names
    options->libs = (char **)malloc(MAX_LIBS * sizeof(char *));
    if (!options->libs) {
        fprintf(stderr, "Memory allocation failed\n");
        exit(1);
    }
    
    // Default values
    options->txt_format = true;
    options->pdf_format = false;
    strcpy(options->output, "bldd_report");
    
    // Check for at least one argument
    if (argc < 2) {
        print_help();
        exit(0);
    }
    
    // Parse arguments
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_help();
            exit(0);
        } else if (strcmp(argv[i], "--lib") == 0 || strcmp(argv[i], "-l") == 0) {
            if (i + 1 < argc) {
                options->libs[options->lib_count] = strdup(argv[++i]);
                options->lib_count++;
            } else {
                fprintf(stderr, "Error: --lib requires a library name\n");
                exit(1);
            }
        } else if (strcmp(argv[i], "--dir") == 0 || strcmp(argv[i], "-d") == 0) {
            if (i + 1 < argc) {
                strcpy(options->dir, argv[++i]);
                dir_set = true;
            } else {
                fprintf(stderr, "Error: --dir requires a directory path\n");
                exit(1);
            }
        } else if (strcmp(argv[i], "--format") == 0 || strcmp(argv[i], "-f") == 0) {
            if (i + 1 < argc) {
                i++;
                if (strcmp(argv[i], "txt") == 0) {
                    options->txt_format = true;
                    options->pdf_format = false;
                } else if (strcmp(argv[i], "pdf") == 0) {
                    options->txt_format = false;
                    options->pdf_format = true;
                } else if (strcmp(argv[i], "both") == 0) {
                    options->txt_format = true;
                    options->pdf_format = true;
                } else {
                    fprintf(stderr, "Error: Unknown format: %s\n", argv[i]);
                    exit(1);
                }
            } else {
                fprintf(stderr, "Error: --format requires a format type\n");
                exit(1);
            }
        } else if (strcmp(argv[i], "--output") == 0 || strcmp(argv[i], "-o") == 0) {
            if (i + 1 < argc) {
                strcpy(options->output, argv[++i]);
            } else {
                fprintf(stderr, "Error: --output requires a filename\n");
                exit(1);
            }
        } else {
            fprintf(stderr, "Error: Unknown option: %s\n", argv[i]);
            print_help();
            exit(1);
        }
    }
    
    // Check required options
    if (options->lib_count == 0) {
        fprintf(stderr, "Error: At least one library must be specified with --lib\n");
        exit(1);
    }
    
    if (!dir_set) {
        fprintf(stderr, "Error: Scan directory must be specified with --dir\n");
        exit(1);
    }
    
    // Verify the directory exists
    DIR *dir = opendir(options->dir);
    if (dir == NULL) {
        fprintf(stderr, "Error: Cannot open directory %s: %s\n", 
                options->dir, strerror(errno));
        exit(1);
    }
    closedir(dir);
}

void print_help() {
    printf("Usage: bldd [OPTIONS]\n");
    printf("\nbldd (backward ldd) - Find executables that use specific shared libraries\n\n");
    printf("Options:\n");
    printf("  -h, --help                 Show this help message and exit\n");
    printf("  -l, --lib LIB              Shared library to search for (can be specified multiple times)\n");
    printf("  -d, --dir DIR              Directory to scan for executables\n");
    printf("  -f, --format FORMAT        Output report format (txt, pdf, both) (default: txt)\n");
    printf("  -o, --output FILENAME      Output file name without extension (default: bldd_report)\n");
    printf("\nExamples:\n");
    printf("  bldd --lib libc.so.6 --dir /usr/bin --format txt\n");
    printf("  bldd --lib libpthread.so --lib libm.so --dir /usr/local/bin\n");
    printf("  bldd --lib libc.so.6 --dir /home --format pdf\n");
}

void scan_directory(const char *dir_path, Options *options) {
    DIR *dir;
    struct dirent *entry;
    struct stat statbuf;
    char path[MAX_PATH];
    int file_count = 0;
    int matched_count = 0;
    
    printf("Scanning directory: %s\n", dir_path);
    printf("Looking for executables using: ");
    for (int i = 0; i < options->lib_count; i++) {
        printf("%s ", options->libs[i]);
    }
    printf("\n");
    
    if ((dir = opendir(dir_path)) == NULL) {
        fprintf(stderr, "Cannot open directory: %s\n", dir_path);
        return;
    }
    
    while ((entry = readdir(dir)) != NULL) {
        // Skip . and ..
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        
        // Construct full path
        snprintf(path, MAX_PATH, "%s/%s", dir_path, entry->d_name);
        
        // Get file status
        if (lstat(path, &statbuf) == -1) {
            continue;
        }
        
        // If directory, recursively scan
        if (S_ISDIR(statbuf.st_mode)) {
            scan_directory(path, options);
        } 
        // If regular file, check if executable
        else if (S_ISREG(statbuf.st_mode)) {
            if (is_executable(path)) {
                file_count++;
                if (file_count % 100 == 0) {
                    printf("Scanned %d executables so far, found %d matches\n", 
                           file_count, matched_count);
                }
                
                const char *arch = get_architecture(path);
                if (strcmp(arch, "unknown") != 0) {
                    get_dependencies(path, options->libs, options->lib_count, arch);
                }
            }
        }
    }
    
    closedir(dir);
}

bool is_executable(const char *file_path) {
    char cmd[MAX_PATH * 2];
    
    // Check if file is accessible and executable
    if (access(file_path, X_OK) != 0) {
        return false;
    }
    
    snprintf(cmd, sizeof(cmd), "readelf -h \"%s\" >/dev/null 2>&1", file_path);
    int result = system(cmd);
    
    return (result == 0);
}

const char *get_architecture(const char *file_path) {
    FILE *fp;
    char cmd[MAX_PATH * 2];
    char line[MAX_LINE];
    
    snprintf(cmd, sizeof(cmd), "readelf -h \"%s\" 2>/dev/null", file_path);
    fp = popen(cmd, "r");
    if (fp == NULL) {
        return "unknown";
    }
    
    while (fgets(line, sizeof(line), fp) != NULL) {
        if (strstr(line, "Machine:") != NULL) {
            if (strstr(line, "Advanced Micro Devices X86-64") != NULL ||
                strstr(line, "AMD x86-64") != NULL) {
                pclose(fp);
                return "x86_64";
            } else if (strstr(line, "Intel 80386") != NULL) {
                pclose(fp);
                return "x86";
            } else if (strstr(line, "ARM aarch64") != NULL ||
                       strstr(line, "AArch64") != NULL) {
                pclose(fp);
                return "aarch64";
            } else if (strstr(line, "ARM") != NULL) {
                pclose(fp);
                return "armv7";
            }
        }
    }
    
    pclose(fp);
    return "unknown";
}

void get_dependencies(const char *file_path, char **libs, int lib_count, const char *arch) {
    FILE *fp;
    char cmd[MAX_PATH * 2];
    char line[MAX_LINE];
    
    snprintf(cmd, sizeof(cmd), "readelf -d \"%s\" 2>/dev/null", file_path);
    fp = popen(cmd, "r");
    if (fp == NULL) {
        fprintf(stderr, "Failed to run readelf on %s\n", file_path);
        return;
    }
    
    bool has_needed_section = false;
    
    while (fgets(line, sizeof(line), fp) != NULL) {
        if (strstr(line, "(NEEDED)") != NULL) {
            has_needed_section = true;
            char lib_name[256] = {0};
            
            char *start = strstr(line, "[");
            char *end = strstr(line, "]");
            
            if (start && end && (end > start)) {
                start++;
                strncpy(lib_name, start, end - start);
                lib_name[end - start] = '\0';
                
                for (int i = 0; i < lib_count; i++) {
                    char lib_pattern[256];
                    const char *lib_search = libs[i];
                    
                    if (strstr(lib_search, ".so") == NULL) {
                        if (strncmp(lib_search, "lib", 3) != 0) {
                            snprintf(lib_pattern, sizeof(lib_pattern), "lib%s.so", lib_search);
                        } else {
                            snprintf(lib_pattern, sizeof(lib_pattern), "%s.so", lib_search);
                        }
                    } else {
                        strcpy(lib_pattern, lib_search);
                    }
                    
                    if (strstr(lib_name, lib_pattern) != NULL) {
                        int arch_index = find_or_add_architecture(arch);
                        int lib_index = find_or_add_library(arch_index, lib_pattern);
                        add_executable(arch_index, lib_index, file_path);
                        break;
                    }
                }
            }
        }
    }
    
    pclose(fp);
    
    if (!has_needed_section) {
        return;
    }
}

int find_or_add_architecture(const char *arch) {
    // Look for existing architecture
    for (int i = 0; i < arch_count; i++) {
        if (strcmp(archs[i].name, arch) == 0) {
            return i;
        }
    }
    
    // Add new architecture
    if (arch_count < MAX_ARCH) {
        strncpy(archs[arch_count].name, arch, sizeof(archs[arch_count].name) - 1);
        archs[arch_count].lib_count = 0;
        return arch_count++;
    }
    
    fprintf(stderr, "Error: Too many architectures\n");
    return -1;
}

int find_or_add_library(int arch_index, const char *lib_name) {
    Architecture *arch = &archs[arch_index];
    
    // Look for existing library
    for (int i = 0; i < arch->lib_count; i++) {
        if (strcmp(arch->libraries[i].name, lib_name) == 0) {
            return i;
        }
    }
    
    // Add new library
    if (arch->lib_count < MAX_LIBS) {
        strncpy(arch->libraries[arch->lib_count].name, lib_name, sizeof(arch->libraries[0].name) - 1);
        arch->libraries[arch->lib_count].exec_count = 0;
        return arch->lib_count++;
    }
    
    fprintf(stderr, "Error: Too many libraries for architecture %s\n", arch->name);
    return -1;
}

void add_executable(int arch_index, int lib_index, const char *exec_path) {
    Library *lib = &archs[arch_index].libraries[lib_index];
    
    // Check if executable is already in the list
    for (int i = 0; i < lib->exec_count; i++) {
        if (strcmp(lib->execs[i], exec_path) == 0) {
            return;  // Already added
        }
    }
    
    // Add new executable
    if (lib->exec_count < MAX_EXECS) {
        lib->execs[lib->exec_count] = strdup(exec_path);
        lib->exec_count++;
        total_execs++;
    } else {
        fprintf(stderr, "Error: Too many executables for library %s\n", lib->name);
    }
}

// Sort libraries by exec count in descending order
int compare_libraries(const void *a, const void *b) {
    Library *lib_a = (Library *)a;
    Library *lib_b = (Library *)b;
    
    return lib_b->exec_count - lib_a->exec_count;
}

void generate_txt_report(Options *options) {
    FILE *fp;
    char output_file[MAX_PATH];
    
    if (strlen(options->output) + 5 > MAX_PATH) {  // 5 = ".txt\0"
        fprintf(stderr, "Error: Output filename too long\n");
        return;
    }
    
    snprintf(output_file, sizeof(output_file), "%s.txt", options->output);
    fp = fopen(output_file, "w");
    if (fp == NULL) {
        fprintf(stderr, "Error: Cannot create output file %s\n", output_file);
        return;
    }
    
    fprintf(fp, "Report on dynamic used libraries by ELF executables\n");
    fprintf(fp, "%s\n", "------------------------------------------------------------");
    
    for (int a = 0; a < arch_count; a++) {
        Architecture *arch = &archs[a];
        
        // Make sure the architecture name doesn't exceed buffer size
        char safe_arch_name[32]; // Same size as in Architecture struct
        strncpy(safe_arch_name, arch->name, sizeof(safe_arch_name) - 1);
        safe_arch_name[sizeof(safe_arch_name) - 1] = '\0'; // Ensure null termination
        
        fprintf(fp, "%s %s %s\n", SEPARATOR, safe_arch_name, SEPARATOR);
        
        // Sort libraries by exec count
        qsort(arch->libraries, arch->lib_count, sizeof(Library), compare_libraries);
        
        for (int l = 0; l < arch->lib_count; l++) {
            Library *lib = &arch->libraries[l];
            
            fprintf(fp, "%s (%d execs)\n", lib->name, lib->exec_count);
            for (int e = 0; e < lib->exec_count; e++) {
                fprintf(fp, "-> %s\n", lib->execs[e]);
            }
            
            fprintf(fp, "\n");
        }
    }
    
    fclose(fp);
    printf("Text report saved to %s\n", output_file);
}

void error_handler(HPDF_STATUS error_no, HPDF_STATUS detail_no, void *user_data) {
#if PDF_SUPPORT
    /* Suppress unused parameter warning */
    (void)user_data;
    
    fprintf(stderr, "PDF Error: error_no=%04X, detail_no=%d\n", (unsigned int)error_no, (int)detail_no);
#endif
}

void generate_pdf_report(Options *options) {
#if PDF_SUPPORT
    HPDF_Doc pdf;
    HPDF_Page page;
    HPDF_Font font;
    HPDF_Font bold_font;
    char output_file[MAX_PATH];
    float y_position;
    float margin = 50;
    
    if (strlen(options->output) + 5 > MAX_PATH) {  // 5 = ".pdf\0"
        fprintf(stderr, "Error: Output filename too long\n");
        return;
    }
    
    snprintf(output_file, sizeof(output_file), "%s.pdf", options->output);
    
    pdf = HPDF_New(error_handler, NULL);
    if (!pdf) {
        fprintf(stderr, "Error: Cannot create PDF document\n");
        return;
    }
    
    // Add a new page
    page = HPDF_AddPage(pdf);
    HPDF_Page_SetSize(page, HPDF_PAGE_SIZE_A4, HPDF_PAGE_PORTRAIT);
    
    // Get page dimensions
    float page_height = HPDF_Page_GetHeight(page);
    float page_width = HPDF_Page_GetWidth(page);
    
    // Set fonts
    font = HPDF_GetFont(pdf, "Helvetica", NULL);
    bold_font = HPDF_GetFont(pdf, "Helvetica-Bold", NULL);
    
    // Set initial y position
    y_position = page_height - margin;
    
    // Add title
    HPDF_Page_SetFontAndSize(page, bold_font, 16);
    HPDF_Page_BeginText(page);
    HPDF_Page_TextOut(page, margin, y_position, "Report on dynamic used libraries by ELF executables");
    HPDF_Page_EndText(page);
    
    y_position -= 30;
    
    for (int a = 0; a < arch_count; a++) {
        Architecture *arch = &archs[a];
        
        // Check if we need a new page
        if (y_position < margin + 50) {
            page = HPDF_AddPage(pdf);
            HPDF_Page_SetSize(page, HPDF_PAGE_SIZE_A4, HPDF_PAGE_PORTRAIT);
            y_position = page_height - margin;
        }
        
        // Add architecture header - safely construct the header string
        HPDF_Page_SetFontAndSize(page, bold_font, 14);
        HPDF_Page_BeginText(page);
        char arch_header[MAX_HEADER_SIZE];
        
        // Make sure the architecture name doesn't exceed buffer size
        char safe_arch_name[32]; // Same size as in Architecture struct
        strncpy(safe_arch_name, arch->name, sizeof(safe_arch_name) - 1);
        safe_arch_name[sizeof(safe_arch_name) - 1] = '\0'; // Ensure null termination
        
        // Build the header safely
        snprintf(arch_header, MAX_HEADER_SIZE, "%s %s %s", SEPARATOR, safe_arch_name, SEPARATOR);
        HPDF_Page_TextOut(page, margin, y_position, arch_header);
        HPDF_Page_EndText(page);
        
        y_position -= 20;
        
        // Sort libraries by exec count
        qsort(arch->libraries, arch->lib_count, sizeof(Library), compare_libraries);
        
        for (int l = 0; l < arch->lib_count; l++) {
            Library *lib = &arch->libraries[l];
            
            // Check if we need a new page
            if (y_position < margin + 50) {
                page = HPDF_AddPage(pdf);
                HPDF_Page_SetSize(page, HPDF_PAGE_SIZE_A4, HPDF_PAGE_PORTRAIT);
                y_position = page_height - margin;
            }
            
            // Add library header
            HPDF_Page_SetFontAndSize(page, bold_font, 12);
            HPDF_Page_BeginText(page);
            char lib_header[MAX_HEADER_SIZE];
            snprintf(lib_header, MAX_HEADER_SIZE, "%s (%d execs)", lib->name, lib->exec_count);
            HPDF_Page_TextOut(page, margin, y_position, lib_header);
            HPDF_Page_EndText(page);
            
            y_position -= 15;
            
            // Add executables
            HPDF_Page_SetFontAndSize(page, font, 10);
            for (int e = 0; e < lib->exec_count; e++) {
                if (y_position < margin) {
                    page = HPDF_AddPage(pdf);
                    HPDF_Page_SetSize(page, HPDF_PAGE_SIZE_A4, HPDF_PAGE_PORTRAIT);
                    y_position = page_height - margin;
                }
                
                HPDF_Page_BeginText(page);
                HPDF_Page_TextOut(page, margin + 10, y_position, "-> ");

                char *path = lib->execs[e];
                if (HPDF_Page_TextWidth(page, path) > page_width - margin * 2 - 20) {
                    char *base = basename(strdup(path));
                    char truncated[MAX_PATH];
                    snprintf(truncated, sizeof(truncated), ".../%s", base);
                    HPDF_Page_TextOut(page, margin + 30, y_position, truncated);
                    free(base);
                } else {
                    HPDF_Page_TextOut(page, margin + 30, y_position, path);
                }
                HPDF_Page_EndText(page);
                
                y_position -= 12;
            }
            
            y_position -= 10;
        }
    }
    
    // Save the PDF
    if (HPDF_SaveToFile(pdf, output_file) != HPDF_OK) {
        fprintf(stderr, "Error: Cannot save PDF to %s\n", output_file);
    } else {
        printf("PDF report saved to %s\n", output_file);
    }
    
    // Clean up
    HPDF_Free(pdf);
#else
    fprintf(stderr, "PDF support is not available. Recompile with PDF_SUPPORT=1 and libhpdf installed.\n");
#endif
}

void cleanup() {
    // Free allocated memory for executables
    for (int a = 0; a < arch_count; a++) {
        for (int l = 0; l < archs[a].lib_count; l++) {
            for (int e = 0; e < archs[a].libraries[l].exec_count; e++) {
                free(archs[a].libraries[l].execs[e]);
            }
        }
    }
} 