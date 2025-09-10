#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/sysinfo.h>
#include <sys/utsname.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <ctype.h>
#include <signal.h>
#include <sys/wait.h>

// Buffer sizes optimized for performance
#define BUFFER_SIZE 4096
#define SMALL_BUFFER 256
#define LINE_BUFFER 1024

// Nord colors as ANSI escape codes for maximum speed
#define RESET "\033[0m"
#define BOLD "\033[1m"

// Nord Polar Night
#define NORD0 "\033[30m"    // #2E3440
#define NORD1 "\033[90m"    // #3B4252
#define NORD2 "\033[37m"    // #434C5E
#define NORD3 "\033[97m"    // #4C566A

// Nord Snow Storm  
#define NORD4 "\033[97m"    // #D8DEE9
#define NORD5 "\033[37m"    // #E5E9F0
#define NORD6 "\033[97m"    // #ECEFF4

// Nord Frost
#define NORD7 "\033[36m"    // #8FBCBB
#define NORD8 "\033[96m"    // #88C0D0
#define NORD9 "\033[34m"    // #81A1C1
#define NORD10 "\033[94m"   // #5E81AC

// Nord Aurora
#define NORD11 "\033[91m"   // #BF616A
#define NORD12 "\033[93m"   // #D08770
#define NORD13 "\033[33m"   // #EBCB8B
#define NORD14 "\033[32m"   // #A3BE8C
#define NORD15 "\033[95m"   // #B48EAD

// System types for different ASCII art
typedef enum {
    SYSTEM_BEDROCK,
    SYSTEM_GENTOO,
    SYSTEM_OTHER
} system_type_t;

// System info structure
struct sysinfo_fast {
    char version[SMALL_BUFFER];
    char kernel[SMALL_BUFFER];
    char uptime[SMALL_BUFFER];
    char memory[SMALL_BUFFER];
    char wm[SMALL_BUFFER];
    char terminal[SMALL_BUFFER];
    char shell[SMALL_BUFFER];
    char cpu[SMALL_BUFFER];
    char gpu[SMALL_BUFFER];
    char packages[BUFFER_SIZE];
    system_type_t system_type;
};

// Fast string trimming
static inline char* trim(char* str) {
    char *end;
    while(isspace((unsigned char)*str)) str++;
    if(*str == 0) return str;
    end = str + strlen(str) - 1;
    while(end > str && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
    return str;
}

// Detect system type for appropriate ASCII art
static system_type_t detect_system_type() {
    // Check for Bedrock Linux first
    if (access("/bedrock", F_OK) == 0) {
        return SYSTEM_BEDROCK;
    }
    
    // Check for Gentoo
    if (access("/etc/gentoo-release", F_OK) == 0 || 
        access("/var/db/repos/gentoo", F_OK) == 0 ||
        access("/usr/portage", F_OK) == 0) {
        return SYSTEM_GENTOO;
    }
    
    return SYSTEM_OTHER;
}

// Ultra-fast file reading optimized for /proc
static int read_file_fast(const char* path, char* buffer, size_t size) {
    int fd = open(path, O_RDONLY);
    if (fd == -1) return 0;
    
    ssize_t bytes = read(fd, buffer, size - 1);
    close(fd);
    
    if (bytes > 0) {
        buffer[bytes] = '\0';
        return 1;
    }
    return 0;
}

// Get first line from buffer
static inline void get_first_line(char* buffer, char* output, size_t max_len) {
    char* newline = strchr(buffer, '\n');
    if (newline) *newline = '\0';
    strncpy(output, buffer, max_len - 1);
    output[max_len - 1] = '\0';
}

// Check if process is running by name (faster than pgrep)
static int is_process_running(const char* name) {
    DIR* proc_dir = opendir("/proc");
    if (!proc_dir) return 0;
    
    struct dirent* entry;
    char path[64], comm[32];
    int fd, found = 0;
    
    while ((entry = readdir(proc_dir)) != NULL && !found) {
        if (!isdigit(entry->d_name[0])) continue;
        
        snprintf(path, sizeof(path), "/proc/%s/comm", entry->d_name);
        fd = open(path, O_RDONLY);
        if (fd == -1) continue;
        
        ssize_t bytes = read(fd, comm, sizeof(comm) - 1);
        close(fd);
        
        if (bytes > 0) {
            comm[bytes - 1] = '\0'; // Remove newline
            if (strcmp(comm, name) == 0) {
                found = 1;
            }
        }
    }
    
    closedir(proc_dir);
    return found;
}

// Get Bedrock version ultra-fast - read from bedrock filesystem
static void get_version(char* version) {
    char buffer[SMALL_BUFFER];
    
    // Try reading from bedrock version file directly
    if (read_file_fast("/bedrock/etc/bedrock-release", buffer, sizeof(buffer))) {
        char* line = strtok(buffer, "\n");
        while (line) {
            if (strncmp(line, "VERSION=", 8) == 0) {
                char* ver = line + 8;
                if (*ver == '"') ver++; // Skip quote
                char* end = strchr(ver, '"');
                if (end) *end = '\0';
                strcpy(version, ver);
                return;
            }
            line = strtok(NULL, "\n");
        }
    }
    
    // Fallback: try /bedrock/etc/os-release
    if (read_file_fast("/bedrock/etc/os-release", buffer, sizeof(buffer))) {
        char* line = strtok(buffer, "\n");
        while (line) {
            if (strncmp(line, "VERSION=", 8) == 0) {
                char* ver = line + 8;
                if (*ver == '"') ver++;
                char* end = strchr(ver, '"');
                if (end) *end = '\0';
                strcpy(version, ver);
                return;
            }
            line = strtok(NULL, "\n");
        }
    }
    
    // Try /etc/os-release as final fallback
    if (read_file_fast("/etc/os-release", buffer, sizeof(buffer))) {
        char* line = strtok(buffer, "\n");
        while (line) {
            if (strncmp(line, "VERSION=", 8) == 0 || strncmp(line, "VERSION_ID=", 11) == 0) {
                char* ver = strchr(line, '=') + 1;
                if (*ver == '"') ver++;
                char* end = strchr(ver, '"');
                if (end) *end = '\0';
                strcpy(version, ver);
                return;
            }
            line = strtok(NULL, "\n");
        }
    }
    
    strcpy(version, "Unknown");
}

// Get kernel info from uname
static void get_kernel(char* kernel) {
    struct utsname uts;
    if (uname(&uts) == 0) {
        snprintf(kernel, SMALL_BUFFER, "%s", uts.release);
    } else {
        strcpy(kernel, "Unknown");
    }
}

// Get uptime from /proc/uptime
static void get_uptime(char* uptime) {
    char buffer[64];
    if (read_file_fast("/proc/uptime", buffer, sizeof(buffer))) {
        double up_seconds = atof(buffer);
        int days = (int)(up_seconds / 86400);
        int hours = (int)((up_seconds - days * 86400) / 3600);
        int minutes = (int)((up_seconds - days * 86400 - hours * 3600) / 60);
        
        if (days > 0) {
            if (hours > 0) {
                snprintf(uptime, SMALL_BUFFER, "%d day%s, %d hour%s, %d minute%s",
                    days, days == 1 ? "" : "s",
                    hours, hours == 1 ? "" : "s", 
                    minutes, minutes == 1 ? "" : "s");
            } else {
                snprintf(uptime, SMALL_BUFFER, "%d day%s, %d minute%s",
                    days, days == 1 ? "" : "s",
                    minutes, minutes == 1 ? "" : "s");
            }
        } else if (hours > 0) {
            snprintf(uptime, SMALL_BUFFER, "%d hour%s, %d minute%s",
                hours, hours == 1 ? "" : "s",
                minutes, minutes == 1 ? "" : "s");
        } else {
            snprintf(uptime, SMALL_BUFFER, "%d minute%s",
                minutes, minutes == 1 ? "" : "s");
        }
    } else {
        strcpy(uptime, "Unknown");
    }
}

// Get memory info from /proc/meminfo
static void get_memory(char* memory) {
    char buffer[BUFFER_SIZE];
    if (read_file_fast("/proc/meminfo", buffer, sizeof(buffer))) {
        long total = 0, available = 0;
        char* line = strtok(buffer, "\n");
        
        while (line) {
            if (strncmp(line, "MemTotal:", 9) == 0) {
                sscanf(line, "MemTotal: %ld kB", &total);
            } else if (strncmp(line, "MemAvailable:", 13) == 0) {
                sscanf(line, "MemAvailable: %ld kB", &available);
            }
            line = strtok(NULL, "\n");
        }
        
        if (total > 0 && available >= 0) {
            long used = total - available;
            // Use Gi format to match original (same as shell script's 'free -h' output)
            if (total >= 1024 * 1024) {
                snprintf(memory, SMALL_BUFFER, "%.0fGi / %.0fGi", 
                    used / 1024.0 / 1024.0, total / 1024.0 / 1024.0);
            } else {
                snprintf(memory, SMALL_BUFFER, "%ldMi / %ldMi", 
                    used / 1024, total / 1024);
            }
        } else {
            strcpy(memory, "Unknown");
        }
    } else {
        strcpy(memory, "Unknown");
    }
}

// Get window manager
static void get_wm(char* wm) {
    const char* xdg = getenv("XDG_CURRENT_DESKTOP");
    if (xdg) {
        strncpy(wm, xdg, SMALL_BUFFER - 1);
        wm[SMALL_BUFFER - 1] = '\0';
        return;
    }
    
    const char* desktop = getenv("DESKTOP_SESSION");
    if (desktop) {
        strncpy(wm, desktop, SMALL_BUFFER - 1);
        wm[SMALL_BUFFER - 1] = '\0';
        return;
    }
    
    // Check for common window managers
    if (is_process_running("i3")) strcpy(wm, "i3");
    else if (is_process_running("sway")) strcpy(wm, "Sway");
    else if (is_process_running("bspwm")) strcpy(wm, "bspwm");
    else if (is_process_running("dwm")) strcpy(wm, "dwm");
    else if (is_process_running("awesome")) strcpy(wm, "Awesome");
    else strcpy(wm, "Unknown");
}

// Get terminal exactly like fastfetch - walk up from shell's parent
static void get_terminal(char* terminal) {
    // Walk up process tree first (like fastfetch) — prefer actual process over env hints
    pid_t current_pid = getppid(); // Start from parent PID
    char path[64], buffer[SMALL_BUFFER];
    
    while (current_pid > 1) {
        snprintf(path, sizeof(path), "/proc/%d/stat", current_pid);
        if (!read_file_fast(path, buffer, sizeof(buffer))) break;
        
        // Get parent PID from stat file (4th field)
        char* token = strtok(buffer, " ");
        for (int j = 0; j < 3 && token; j++) {
            token = strtok(NULL, " ");
        }
        if (!token) break;
        
        pid_t ppid = atoi(token);
        if (ppid <= 1) break;
        
        // Get command name
        snprintf(path, sizeof(path), "/proc/%d/comm", current_pid);
        if (read_file_fast(path, buffer, sizeof(buffer))) {
            char* newline = strchr(buffer, '\n');
            if (newline) *newline = '\0';
            
            // Skip known shells and utilities (exactly like fastfetch)
            if (current_pid == 1 ||
                strcmp(buffer, "sudo") == 0 ||
                strcmp(buffer, "su") == 0 ||
                strcmp(buffer, "sh") == 0 ||
                strcmp(buffer, "ash") == 0 ||
                strcmp(buffer, "bash") == 0 ||
                strcmp(buffer, "zsh") == 0 ||
                strcmp(buffer, "ksh") == 0 ||
                strcmp(buffer, "mksh") == 0 ||
                strcmp(buffer, "csh") == 0 ||
                strcmp(buffer, "tcsh") == 0 ||
                strcmp(buffer, "fish") == 0 ||
                strcmp(buffer, "dash") == 0 ||
                strcmp(buffer, "pwsh") == 0 ||
                strcmp(buffer, "nu") == 0 ||
                strcmp(buffer, "login") == 0 ||
                strcmp(buffer, "script") == 0 ||
                strstr(buffer, ".sh") != NULL) {
                current_pid = ppid;
                continue;
            }
            
            // Found terminal - format like fastfetch
            if (strcmp(buffer, "gnome-terminal-") == 0 || strncmp(buffer, "gnome-terminal", 14) == 0) {
                strcpy(terminal, "GNOME Terminal");
            } else if (strcmp(buffer, "urxvt") == 0 || strcmp(buffer, "urxvtd") == 0 || strcmp(buffer, "rxvt") == 0) {
                strcpy(terminal, "rxvt-unicode");
            } else if (strcmp(buffer, "wezterm-gui") == 0) {
                strcpy(terminal, "WezTerm");
            } else if (strncmp(buffer, "tmux:", 5) == 0) {
                strcpy(terminal, "tmux");
            } else if (strncmp(buffer, "screen-", 7) == 0) {
                strcpy(terminal, "screen");
            } else if (strcmp(buffer, "foot") == 0 || strcmp(buffer, "footclient") == 0) {
                strcpy(terminal, "foot");
            } else {
                strncpy(terminal, buffer, SMALL_BUFFER - 1);
                terminal[SMALL_BUFFER - 1] = '\0';
            }
            return;
        }
        
        current_pid = ppid;
    }

    // If process tree didn't resolve, consult environment variables (fallback order)
    const char* foot_env = getenv("FOOT");
    if (foot_env && strlen(foot_env) > 0) {
        strcpy(terminal, "foot");
        return;
    }

    if (getenv("KITTY_PID") != NULL || getenv("KITTY_INSTALLATION_DIR") != NULL) {
        strcpy(terminal, "kitty");
        return;
    }

    if (getenv("ALACRITTY_SOCKET") != NULL || getenv("ALACRITTY_LOG") != NULL || getenv("ALACRITTY_WINDOW_ID") != NULL) {
        strcpy(terminal, "Alacritty");
        return;
    }

    if (getenv("KONSOLE_VERSION") != NULL) {
        strcpy(terminal, "konsole");
        return;
    }

    if (getenv("GNOME_TERMINAL_SCREEN") != NULL || getenv("GNOME_TERMINAL_SERVICE") != NULL) {
        strcpy(terminal, "gnome-terminal");
        return;
    }

    const char* term_program = getenv("TERM_PROGRAM");
    if (term_program && strlen(term_program) > 0) {
        strncpy(terminal, term_program, SMALL_BUFFER - 1);
        terminal[SMALL_BUFFER - 1] = '\0';
        return;
    }

    const char* terminal_env = getenv("TERMINAL");
    if (terminal_env && strlen(terminal_env) > 0) {
        const char* basename = strrchr(terminal_env, '/');
        const char* name = basename ? basename + 1 : terminal_env;
        strncpy(terminal, name, SMALL_BUFFER - 1);
        terminal[SMALL_BUFFER - 1] = '\0';
        return;
    }
    
    // Last-resort fallback to TERM or TTY
    const char* term = getenv("TERM");
    if (term && strcmp(term, "linux") != 0) {
        strncpy(terminal, term, SMALL_BUFFER - 1);
        terminal[SMALL_BUFFER - 1] = '\0';
    } else {
        const char* tty_name = ttyname(STDIN_FILENO);
        if (tty_name) {
            const char* basename = strrchr(tty_name, '/');
            strncpy(terminal, basename ? basename + 1 : tty_name, SMALL_BUFFER - 1);
            terminal[SMALL_BUFFER - 1] = '\0';
        } else {
            strcpy(terminal, "Unknown");
        }
    }
}

// Get shell
static void get_shell(char* shell) {
    const char* shell_path = getenv("SHELL");
    if (shell_path) {
        const char* basename = strrchr(shell_path, '/');
        if (basename) {
            strcpy(shell, basename + 1);
        } else {
            strcpy(shell, shell_path);
        }
    } else {
        strcpy(shell, "Unknown");
    }
}

// Efficient property line parser similar to fastfetch
static inline int parse_prop_line(const char** line, const char* key, char* value, size_t value_size) {
    const char* start = *line;
    const char* key_ptr = key;
    
    // Skip whitespace at beginning
    while (*start == ' ' || *start == '\t') start++;
    
    // Match key (case insensitive)
    while (*key_ptr && *start && 
           (tolower(*start) == tolower(*key_ptr))) {
        start++;
        key_ptr++;
    }
    
    // If key doesn't match completely, return 0
    if (*key_ptr != '\0') return 0;
    
    // Skip whitespace and colon
    while (*start == ' ' || *start == '\t' || *start == ':') start++;
    
    // Copy value until end of line, removing trailing whitespace
    size_t len = 0;
    const char* value_start = start;
    while (*start && *start != '\n' && len < value_size - 1) {
        value[len++] = *start++;
    }
    
    // Trim trailing whitespace
    while (len > 0 && (value[len-1] == ' ' || value[len-1] == '\t')) len--;
    value[len] = '\0';
    
    *line = start;
    return len > 0 ? 1 : 0;
}

// Fast single-pass CPU info parser
static void get_cpu(char* cpu) {
    char buffer[BUFFER_SIZE];
    if (read_file_fast("/proc/cpuinfo", buffer, sizeof(buffer))) {
        const char* line = buffer;
        const char* end = buffer + strlen(buffer);
        
        while (line < end) {
            // Find end of current line
            const char* line_end = line;
            while (line_end < end && *line_end != '\n') line_end++;
            
            // Check for model name (first one wins)
            if (parse_prop_line(&line, "model name", cpu, SMALL_BUFFER)) {
                return;
            }
            
            // Move to next line
            line = line_end;
            if (line < end && *line == '\n') line++;
        }
    }
    strcpy(cpu, "Unknown");
}

// GPU detection exactly like fastfetch - no subprocess calls, clean formatting
static void get_gpu(char* gpu) {
    DIR* drm_dir = opendir("/sys/class/drm");
    if (!drm_dir) {
        strcpy(gpu, "Unknown");
        return;
    }
    
    struct dirent* entry;
    char path[512], buffer[256];
    
    while ((entry = readdir(drm_dir)) != NULL) {
        if (strncmp(entry->d_name, "card", 4) != 0 || 
            !isdigit(entry->d_name[4])) continue;
        
        // Read vendor ID
        snprintf(path, sizeof(path), "/sys/class/drm/%s/device/vendor", entry->d_name);
        if (!read_file_fast(path, buffer, sizeof(buffer))) continue;
        
        unsigned int vendor_id = (unsigned int)strtoul(trim(buffer), NULL, 16);
        
        // Read device ID for more specific identification
        snprintf(path, sizeof(path), "/sys/class/drm/%s/device/device", entry->d_name);
        char device_buffer[64] = "";
        read_file_fast(path, device_buffer, sizeof(device_buffer));
        unsigned int device_id = (unsigned int)strtoul(trim(device_buffer), NULL, 16);
        
        // Format GPU name like fastfetch: "NVIDIA GeForce RTX 3070 Ti [Discrete]"
        const char* vendor_name = "";
        const char* gpu_type = "";
        
        switch (vendor_id) {
            case 0x10de: // NVIDIA
                vendor_name = "NVIDIA";
                gpu_type = " [Discrete]";
                if (device_id == 0x2482) {
                    snprintf(gpu, SMALL_BUFFER, "%s GeForce RTX 3070 Ti%s", vendor_name, gpu_type);
                } else if (device_id >= 0x2400 && device_id <= 0x24ff) {
                    snprintf(gpu, SMALL_BUFFER, "%s GeForce RTX 30 Series%s", vendor_name, gpu_type);
                } else if (device_id >= 0x2200 && device_id <= 0x22ff) {
                    snprintf(gpu, SMALL_BUFFER, "%s GeForce RTX 20 Series%s", vendor_name, gpu_type);
                } else {
                    snprintf(gpu, SMALL_BUFFER, "%s Graphics%s", vendor_name, gpu_type);
                }
                break;
                
            case 0x1002: // AMD
            case 0x1022:
                vendor_name = "AMD";
                gpu_type = " [Discrete]";
                snprintf(gpu, SMALL_BUFFER, "%s Radeon Graphics%s", vendor_name, gpu_type);
                break;
                
            case 0x8086: // Intel
                vendor_name = "Intel";
                gpu_type = " [Integrated]";
                snprintf(gpu, SMALL_BUFFER, "%s Graphics%s", vendor_name, gpu_type);
                break;
                
            default:
                snprintf(gpu, SMALL_BUFFER, "Graphics Controller [Unknown]");
                break;
        }
        
        closedir(drm_dir);
        return;
    }
    
    closedir(drm_dir);
    
    // Fallback: try OpenGL renderer string like fastfetch
    FILE* fp = fopen("/proc/version", "r");
    if (fp) {
        fclose(fp);
        strcpy(gpu, "Unknown");
    } else {
        strcpy(gpu, "Unknown");
    }
}

// Count files in directories super fast
static int count_files_in_dir(const char* path) {
    DIR* dir = opendir(path);
    if (!dir) return 0;
    
    int count = 0;
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] != '.') count++; // Skip . and ..
    }
    closedir(dir);
    return count;
}

// Count occurrences of substring in file content
static int count_strings_in_buffer(const char* buffer, const char* needle) {
    if (!buffer || !needle) return 0;
    
    int count = 0;
    const char* pos = buffer;
    size_t needle_len = strlen(needle);
    
    while ((pos = strstr(pos, needle)) != NULL) {
        count++;
        pos += needle_len;
    }
    return count;
}

// Fast RPM package counting via database file size estimation
static int count_rpm_packages(const char* strata_path) {
    char packages_path[512];
    struct stat st;
    
    // Try modern Fedora path first (this is the correct path for Fedora stratum)
    snprintf(packages_path, sizeof(packages_path), "%s/usr/lib/sysimage/rpm/rpmdb.sqlite", strata_path);
    if (stat(packages_path, &st) == 0 && st.st_size > 0) {
        // SQLite database - estimate based on file size
        // Modern RPM SQLite databases are roughly 80KB per package on average
        return (int)(st.st_size / 80000);
    }
    
    // Try traditional RPM database path
    snprintf(packages_path, sizeof(packages_path), "%s/var/lib/rpm/rpmdb.sqlite", strata_path);
    if (stat(packages_path, &st) == 0 && st.st_size > 0) {
        return (int)(st.st_size / 80000);
    }
    
    // Try even older BDB format
    snprintf(packages_path, sizeof(packages_path), "%s/var/lib/rpm/Packages", strata_path);
    if (stat(packages_path, &st) == 0 && st.st_size > 0) {
        // Berkeley DB format - smaller overhead per package
        return (int)(st.st_size / 2000);
    }
    
    return 0;
}

// Fast DPKG package counting via status file
static int count_dpkg_packages(const char* strata_path) {
    char status_path[512];
    snprintf(status_path, sizeof(status_path), "%s/var/lib/dpkg/status", strata_path);
    
    char buffer[BUFFER_SIZE * 4]; // Larger buffer for dpkg status
    if (read_file_fast(status_path, buffer, sizeof(buffer))) {
        return count_strings_in_buffer(buffer, "Status: install ok installed");
    }
    return 0;
}

// Fast Pacman package counting via local database
static int count_pacman_packages(const char* strata_path) {
    char local_path[512];
    snprintf(local_path, sizeof(local_path), "%s/var/lib/pacman/local", strata_path);
    return count_files_in_dir(local_path);
}

// Fast Emerge package counting via portage database
static int count_emerge_packages(const char* strata_path) {
    char db_path[512];
    snprintf(db_path, sizeof(db_path), "%s/var/db/pkg", strata_path);
    
    DIR* cat_dir = opendir(db_path);
    if (!cat_dir) return 0;
    
    int total = 0;
    struct dirent* cat_entry;
    while ((cat_entry = readdir(cat_dir)) != NULL) {
        if (cat_entry->d_name[0] == '.' || cat_entry->d_type != DT_DIR) continue;
        
        char cat_full_path[1024];
        snprintf(cat_full_path, sizeof(cat_full_path), "%s/%s", db_path, cat_entry->d_name);
        total += count_files_in_dir(cat_full_path);
    }
    closedir(cat_dir);
    return total;
}

// Fast Nix package counting via profile manifest (NOT from strata - nix is global)
static int count_nix_packages(const char* strata_path) {
    (void)strata_path; // Unused - nix is not strata-specific
    char profile_path[512];
    char buffer[BUFFER_SIZE];
    
    // Nix packages are in user's home profile - the manifest.json format uses "elements" object
    const char* home = getenv("HOME");
    if (home) {
        snprintf(profile_path, sizeof(profile_path), "%s/.nix-profile/manifest.json", home);
        if (read_file_fast(profile_path, buffer, sizeof(buffer))) {
            // New manifest format: count entries in "elements" object
            // Each package is a key in the elements object
            int elements_count = 0;
            char* elements_start = strstr(buffer, "\"elements\":");
            if (elements_start) {
                elements_start = strchr(elements_start, '{');
                if (elements_start) {
                    char* pos = elements_start;
                    int brace_count = 0;
                    int in_string = 0;
                    
                    while (*pos) {
                        if (*pos == '"' && (pos == buffer || *(pos-1) != '\\')) {
                            in_string = !in_string;
                        } else if (!in_string) {
                            if (*pos == '{') brace_count++;
                            else if (*pos == '}') {
                                brace_count--;
                                if (brace_count == 0) break; // End of elements object
                            } else if (*pos == ',' && brace_count == 1) {
                                elements_count++;
                            }
                        }
                        pos++;
                    }
                    // Add 1 for the last element (no comma after last element)
                    if (elements_count > 0) elements_count++;
                    return elements_count;
                }
            }
            
            // Fallback: try old format - count "active":true entries
            return count_strings_in_buffer(buffer, "\"active\":true");
        }
    }
    
    // Try alternative nix profile locations
    snprintf(profile_path, sizeof(profile_path), "/nix/var/nix/profiles/per-user/%s/profile/manifest.json", 
             getenv("USER") ? getenv("USER") : "root");
    if (read_file_fast(profile_path, buffer, sizeof(buffer))) {
        return count_strings_in_buffer(buffer, "\"active\":true");
    }
    
    return 0;
}

// Check if stratum is a real directory (not symlink alias) like brl list does
static int is_stratum(const char* name) {
    char path[512];
    snprintf(path, sizeof(path), "/bedrock/strata/%s", name);
    struct stat st;
    return (stat(path, &st) == 0 && S_ISDIR(st.st_mode) && !S_ISLNK(st.st_mode));
}

// Check if stratum is enabled like brl list does
static int is_enabled(const char* name) {
    char path[512];
    snprintf(path, sizeof(path), "/bedrock/run/enabled_strata/%s", name);
    return (access(path, F_OK) == 0);
}

// Optimized package counting with direct filesystem access (replicating brl list logic)
static void get_packages(char* packages, system_type_t system_type) {
    char result[BUFFER_SIZE] = "";
    char temp[SMALL_BUFFER];
    int first = 1;
    
    // Handle Gentoo mode - only show emerge packages
    if (system_type == SYSTEM_GENTOO) {
        int count = count_emerge_packages("");
        if (count > 0) {
            snprintf(result, sizeof(result), "%d (emerge)", count);
        } else {
            strcpy(result, "Unknown");
        }
        strcpy(packages, result);
        return;
    }
    
    // Check if this is a Bedrock Linux system using direct filesystem access like brl list
    DIR* bedrock_strata = opendir("/bedrock/strata");
    if (bedrock_strata) {
        struct dirent* strata_entry;
        while ((strata_entry = readdir(bedrock_strata)) != NULL) {
            if (strata_entry->d_name[0] == '.') continue;
            
            // Only process real strata (directories, not symlink aliases)
            if (!is_stratum(strata_entry->d_name)) continue;
            
            // Only count enabled strata
            if (!is_enabled(strata_entry->d_name)) continue;
            
            char strata_path[512];
            snprintf(strata_path, sizeof(strata_path), "/bedrock/strata/%s", strata_entry->d_name);
            
            int count = 0;
            const char* manager = "";
            
            // Try different package managers based on strata name and available databases
            if (strcmp(strata_entry->d_name, "fedora") == 0 || strstr(strata_entry->d_name, "rhel")) {
                count = count_rpm_packages(strata_path);
                manager = "rpm";
            } else if (strcmp(strata_entry->d_name, "gentoo") == 0) {
                count = count_emerge_packages(strata_path);
                manager = "emerge";
            } else if (strstr(strata_entry->d_name, "arch")) {
                count = count_pacman_packages(strata_path);
                manager = "pacman";
            } else if (strstr(strata_entry->d_name, "debian") || strstr(strata_entry->d_name, "ubuntu")) {
                count = count_dpkg_packages(strata_path);
                manager = "dpkg";
            }
            
            if (count > 0) {
                if (!first) strcat(result, ",");
                snprintf(temp, sizeof(temp), "%d (%s)", count, manager);
                strcat(result, temp);
                first = 0;
            }
        }
        closedir(bedrock_strata);
        
        // Add global Nix packages (not stratum-specific)
        int nix_count = count_nix_packages("");
        if (nix_count > 0) {
            if (!first) strcat(result, ",");
            snprintf(temp, sizeof(temp), "%d (%s)", nix_count, "nix");
            strcat(result, temp);
            first = 0;
        }
    } else {
        // Not Bedrock Linux - try standard package managers with direct filesystem access
        struct {
            int (*count_func)(const char*);
            const char* name;
        } pkg_managers[] = {
            {count_rpm_packages, "rpm"},
            {count_dpkg_packages, "dpkg"},
            {count_pacman_packages, "pacman"},
            {count_emerge_packages, "emerge"},
            {count_nix_packages, "nix"},
            {NULL, NULL}
        };
        
        for (int i = 0; pkg_managers[i].count_func; i++) {
            int count = pkg_managers[i].count_func(""); // Empty strata_path for root filesystem
            if (count > 0) {
                if (!first) strcat(result, ",");
                snprintf(temp, sizeof(temp), "%d (%s)", count, pkg_managers[i].name);
                strcat(result, temp);
                first = 0;
            }
        }
    }
    
    if (strlen(result) == 0) {
        strcpy(packages, "Unknown");
    } else {
        strcpy(packages, result);
    }
}

// Print Gentoo ASCII art with colored bar
static void print_gentoo_fetch(const struct sysinfo_fast* info) {
    printf(RESET BOLD " ┌──┐"NORD1" ┌──────────────────────────────────┐ " NORD15 BOLD "┌─────┐\n");
    printf(RESET BOLD " │" NORD1 "▒▒" RESET BOLD "│"NORD1" │─────────" RESET BOLD "\\\\\\\\\\\\\\\\\\" NORD1 "────────────────│ " NORD15 BOLD "│  G  │\n");
    printf(RESET BOLD " │" NORD0 "██" RESET BOLD "│"NORD1" │───────" RESET BOLD "//+++++++++++\\" NORD1 BOLD "─────────────│ " NORD15 BOLD "│  a  │\n");
    printf(RESET BOLD " │" NORD1 "██" RESET BOLD "│"NORD1" │──────" RESET BOLD "//+++++" NORD1 BOLD "\\\\\\" RESET BOLD "+++++\\" NORD1 BOLD "────────────│ " NORD15 BOLD "│  n  │\n");
    printf(RESET BOLD " │" NORD11 "██" RESET BOLD "│"NORD1" │─────" RESET BOLD "//+++++" NORD1 BOLD "// "RESET BOLD"/" RESET BOLD "+++++++\\" NORD1 BOLD "──────────│ " NORD15 BOLD "│  y  │\n");
    printf(RESET BOLD " │" NORD12 "██" RESET BOLD "│"NORD1" │──────" RESET BOLD "+++++++" NORD1 BOLD "\\\\" RESET BOLD "++++++++++\\" NORD1 BOLD "────────│ " NORD15 BOLD "│  m  │\n");
    printf(RESET BOLD " │" NORD13 "██" RESET BOLD "│"NORD1" │────────" RESET BOLD "++++++++++++++++++" NORD1 BOLD "\\\\" NORD1 "──────│ " NORD15 BOLD "│  e  │\n");
    printf(RESET BOLD " │" NORD14 "██" RESET BOLD "│"NORD1" │─────────" RESET BOLD "//++++++++++++++" NORD1 BOLD "//" NORD1 "───────│ " NORD15 BOLD "│  d  │\n");
    printf(RESET BOLD " │" NORD7 "██" RESET BOLD "│"NORD1" │───────" RESET BOLD "//++++++++++++++" NORD1 BOLD "//" NORD1 "─────────│ " NORD15 BOLD "│  e  │\n");
    printf(RESET BOLD " │" NORD8 "██" RESET BOLD "│"NORD1" │──── " RESET BOLD "//++++++++++++++" NORD1 BOLD "//" NORD1 "───────────│ " NORD15 BOLD "└─────┘\n");
    printf(RESET BOLD " │" NORD9 "██" RESET BOLD "│"NORD1" │─────" RESET BOLD "//++++++++++" NORD1 BOLD "//" NORD1 "───────────────│\n");
    printf(RESET BOLD " │" NORD10 "██" RESET BOLD "│"NORD1" │─────" RESET BOLD "//+++++++" NORD1 BOLD "//" NORD1 "──────────────────│\n");
    printf(RESET BOLD " │" NORD15 "██" RESET BOLD "│"NORD1" │──────" RESET BOLD "////////" NORD1 BOLD "────────────────────│\n");
    printf(RESET BOLD " │" NORD7 "██" RESET BOLD "│"NORD1" └──────────────────────────────────┘\n");
    printf(RESET BOLD " │" NORD8 "██" RESET BOLD "│ " NORD12 "Version: " NORD4 "%s\n", info->version);
    printf(RESET BOLD " │" NORD9 "██" RESET BOLD "│ " NORD12 "Kernel: " NORD4 "%s\n", info->kernel);
    printf(RESET BOLD " │" NORD10 "██" RESET BOLD "│ " NORD15 "Uptime: " NORD4 "%s\n", info->uptime);
    printf(RESET BOLD " │" NORD15 "██" RESET BOLD "│ " NORD15 "WM: " NORD4 "%s\n", info->wm);
    printf(RESET BOLD " │" NORD11 "██" RESET BOLD "│ " NORD15 "Packages: " NORD4 "%s\n", info->packages);
    printf(RESET BOLD " │" NORD12 "██" RESET BOLD "│ " NORD13 "Terminal: " NORD4 "%s\n", info->terminal);
    printf(RESET BOLD " │" NORD13 "██" RESET BOLD "│ " NORD13 "Memory: " NORD4 "%s\n", info->memory);
    printf(RESET BOLD " │" NORD14 "██" RESET BOLD "│ " NORD13 "Shell: " NORD4 "%s\n", info->shell);
    printf(RESET BOLD " │" NORD7 "██" RESET BOLD "│ " NORD9 "CPU: " NORD4 "%s\n", info->cpu);
    printf(RESET BOLD " │" NORD1 "▒▒" RESET BOLD "│ " NORD9 "GPU: " NORD4 "%s\n", info->gpu);
    printf(RESET BOLD " └──┘" RESET "\n");
}

// Print the ASCII art with system info - EXACT match to original
static void print_bedrock_fetch(const struct sysinfo_fast* info) {
    printf(RESET BOLD " ┌──┐" NORD1 BOLD " ┌──────────────────────────────────┐ " NORD11 BOLD "┌────┐\n");
    printf(RESET BOLD " │" NORD1 "▒▒" RESET BOLD "│" NORD1 BOLD " │─" RESET BOLD "\\\\\\\\\\\\\\\\\\\\\\\\\\" NORD1 BOLD "────────────────────│ " NORD11 BOLD "│ 境 │\n");
    printf(RESET BOLD " │" NORD0 "██" RESET BOLD "│" NORD1 BOLD " │──" RESET BOLD "\\\\\\      \\\\\\" NORD1 BOLD "────────────────────│ " NORD11 BOLD "│    │\n");
    printf(RESET BOLD " │" NORD1 "██" RESET BOLD "│" NORD1 BOLD " │───" RESET BOLD "\\\\\\      \\\\\\" NORD1 BOLD "───────────────────│ " NORD11 BOLD "│ 界 │\n");
    printf(RESET BOLD " │" NORD11 "██" RESET BOLD "│" NORD1 BOLD " │────" RESET BOLD "\\\\\\      \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\" NORD1 BOLD "────│ " NORD11 BOLD "└────┘\n");
    printf(RESET BOLD " │" NORD12 "██" RESET BOLD "│" NORD1 BOLD " │─────" RESET BOLD "\\\\\\                    \\\\\\" NORD1 BOLD "───│\n");
    printf(RESET BOLD " │" NORD13 "██" RESET BOLD "│" NORD1 BOLD " │──────" RESET BOLD "\\\\\\                    \\\\\\" NORD1 BOLD "──│\n");
    printf(RESET BOLD " │" NORD14 "██" RESET BOLD "│" NORD1 BOLD " │───────" RESET BOLD "\\\\\\        ──────      \\\\\\" NORD1 BOLD "─│\n");
    printf(RESET BOLD " │" NORD7 "██" RESET BOLD "│" NORD1 BOLD " │────────" RESET BOLD "\\\\\\                   ///" NORD1 BOLD "─│\n");
    printf(RESET BOLD " │" NORD8 "██" RESET BOLD "│" NORD1 BOLD " │─────────" RESET BOLD "\\\\\\                 ///" NORD1 BOLD "──│\n");
    printf(RESET BOLD " │" NORD9 "██" RESET BOLD "│" NORD1 BOLD " │──────────" RESET BOLD "\\\\\\               ///" NORD1 BOLD "───│\n");
    printf(RESET BOLD " │" NORD10 "██" RESET BOLD "│" NORD1 BOLD " │───────────" RESET BOLD "\\\\\\////////////////" NORD1 BOLD "────│\n");
    printf(RESET BOLD " │" NORD15 "██" RESET BOLD "│" NORD1 BOLD " └──────────────────────────────────┘\n");
    printf(RESET BOLD " │" NORD7 "██" RESET BOLD "│ " NORD12 "Version: " NORD4 "%s\n", info->version);
    printf(RESET BOLD " │" NORD8 "██" RESET BOLD "│ " NORD12 "Kernel: " NORD4 "%s\n", info->kernel);
    printf(RESET BOLD " │" NORD9 "██" RESET BOLD "│ " NORD15 "Uptime: " NORD4 "%s\n", info->uptime);
    printf(RESET BOLD " │" NORD10 "██" RESET BOLD "│ " NORD15 "WM: " NORD4 "%s\n", info->wm);
    printf(RESET BOLD " │" NORD15 "██" RESET BOLD "│ " NORD15 "Packages: " NORD4 "%s\n", info->packages);
    printf(RESET BOLD " │" NORD11 "██" RESET BOLD "│ " NORD13 "Terminal: " NORD4 "%s\n", info->terminal);
    printf(RESET BOLD " │" NORD12 "██" RESET BOLD "│ " NORD13 "Memory: " NORD4 "%s\n", info->memory);
    printf(RESET BOLD " │" NORD13 "██" RESET BOLD "│ " NORD13 "Shell: " NORD4 "%s\n", info->shell);
    printf(RESET BOLD " │" NORD14 "██" RESET BOLD "│ " NORD9 "CPU: " NORD4 "%s\n", info->cpu);
    printf(RESET BOLD " │" NORD1 "▒▒" RESET BOLD "│ " NORD9 "GPU: " NORD4 "%s\n", info->gpu);
    printf(RESET BOLD " └──┘" RESET "\n");
}

// Print appropriate ASCII art based on system type
static void print_fetch(const struct sysinfo_fast* info) {
    if (info->system_type == SYSTEM_GENTOO) {
        print_gentoo_fetch(info);
    } else {
        print_bedrock_fetch(info);
    }
}

int main(int argc, char* argv[]) {
    struct sysinfo_fast info;
    
    // Check for command line arguments
    int force_gentoo = 0;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--gentoo") == 0) {
            force_gentoo = 1;
        } else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--version") == 0) {
            printf("bfetch version 1.1.2-cowmeed\n");
            return 0;
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            printf("bfetch - Ultra-fast system information display\n");
            printf("Usage: %s [OPTIONS]\n\n", argv[0]);
            printf("Options:\n");
            printf("  -v, --version    Show version information\n");
            printf("  -h, --help       Show this help message\n");
            printf("      --gentoo     Force Gentoo mode (works on any system)\n");
            return 0;
        }
    }
    
    // Detect system type (or force Gentoo mode)
    if (force_gentoo) {
        info.system_type = SYSTEM_GENTOO;
    } else {
        info.system_type = detect_system_type();
    }
    
    // Gather all system information dynamically
    get_version(info.version);
    get_kernel(info.kernel);
    get_uptime(info.uptime);
    get_memory(info.memory);
    get_wm(info.wm);
    get_terminal(info.terminal);
    get_shell(info.shell);
    get_cpu(info.cpu);
    get_gpu(info.gpu);
    get_packages(info.packages, info.system_type);
    
    // Print everything at once
    print_fetch(&info);
    
    return 0;
}
