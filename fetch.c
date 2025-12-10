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
#define BUFFER_SIZE 65536
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
    SYSTEM_CACHYOS,
    SYSTEM_OTHER
} system_type_t;

// System info structure
struct sysinfo_fast {
    char distro[SMALL_BUFFER];
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

    // Check for CachyOS
    if (access("/etc/cachyos-release", F_OK) == 0) {
        return SYSTEM_CACHYOS;
    }
    
    // Check os-release for "cachyos"
    FILE* f = fopen("/etc/os-release", "r");
    if (f) {
        char line[256];
        while (fgets(line, sizeof(line), f)) {
            if (strcasestr(line, "cachyos")) {
                fclose(f);
                return SYSTEM_CACHYOS;
            }
        }
        fclose(f);
    }
    
    return SYSTEM_OTHER;
}

// Ultra-fast file reading optimized for /proc
static int read_file_fast(const char* path, char* buffer, size_t size) {
    int fd = open(path, O_RDONLY);
    if (fd == -1) return 0;
    
    size_t total_bytes = 0;
    ssize_t bytes_read;
    
    while (total_bytes < size - 1) {
        bytes_read = read(fd, buffer + total_bytes, size - 1 - total_bytes);
        if (bytes_read <= 0) break;
        total_bytes += bytes_read;
    }
    
    close(fd);
    
    if (total_bytes > 0) {
        buffer[total_bytes] = '\0';
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

// Get Distro Name (OS) - parses os-release for PRETTY_NAME
static void get_distro(char* distro) {
    char buffer[SMALL_BUFFER];
    char* name = NULL;
    
    // helper to extract value from KEY="Value" or KEY=Value
    auto void extract_value(char* line, const char* key) {
        if (strncmp(line, key, strlen(key)) == 0) {
            char* val = line + strlen(key);
            if (*val == '"') val++;
            char* end = strchr(val, '"');
            if (end) *end = '\0';
            else {
                // handle unquoted value
                end = strchr(val, '\n');
                if (end) *end = '\0';
            }
            if (!name || strlen(val) > strlen(name)) name = val; // Prefer longer/prettier name
        } 
    }

    // Try Bedrock first
    if (read_file_fast("/bedrock/etc/os-release", buffer, sizeof(buffer))) {
         char* line = strtok(buffer, "\n");
         while (line) {
             if (strncmp(line, "PRETTY_NAME=", 12) == 0) {
                 char* val = line + 12;
                 if (*val == '"') val++;
                 char* end = strchr(val, '"');
                 if (end) *end = '\0';
                 strcpy(distro, val);
                 return;
             }
             line = strtok(NULL, "\n");
         }
    }

    // Try /etc/os-release
    if (read_file_fast("/etc/os-release", buffer, sizeof(buffer))) {
        char* line = strtok(buffer, "\n");
        while (line) {
             if (strncmp(line, "PRETTY_NAME=", 12) == 0) {
                 char* val = line + 12;
                 if (*val == '"') val++;
                 char* end = strchr(val, '"');
                 if (end) *end = '\0';
                 strcpy(distro, val);
                 return;
             }
             if (strncmp(line, "NAME=", 5) == 0 && !name) {
                 char* val = line + 5;
                 if (*val == '"') val++;
                 char* end = strchr(val, '"');
                 if (end) *end = '\0';
                 strcpy(distro, val); // Fallback
                 return;
             }
             line = strtok(NULL, "\n");
        }
    }
    
    if (name) strcpy(distro, name);
    else strcpy(distro, "Linux");
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
            // fastfetch style: 11.34 GiB / 31.25 GiB
            double used_gib = used / 1024.0 / 1024.0;
            double total_gib = total / 1024.0 / 1024.0;
            
            if (total_gib >= 1.0) {
                snprintf(memory, SMALL_BUFFER, "%.2f GiB / %.2f GiB", used_gib, total_gib);
            } else {
                snprintf(memory, SMALL_BUFFER, "%.2f MiB / %.2f MiB", used / 1024.0, total / 1024.0);
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

// Fast CPU info parser (Model + Cores + Freq)
static void get_cpu(char* cpu) {
    char model[SMALL_BUFFER] = "Unknown";
    int cores = 0;
    double mhz = 0.0;
    
    char buffer[BUFFER_SIZE];
    if (read_file_fast("/proc/cpuinfo", buffer, sizeof(buffer))) {
        char* line = buffer;
        char* end = buffer + strlen(buffer);
        
        while (line < end) {
            // Find end of current line
            const char* line_end = line;
            while (line_end < end && *line_end != '\n') line_end++;
            
            // Skip leading whitespace for key matching
            char* key_start = line;
            while (key_start < line_end && isspace(*key_start)) key_start++;
            
            // Model Name
            if (strncmp(key_start, "model name", 10) == 0) {
                 // only parse if not set yet
                 if (strcmp(model, "Unknown") == 0) {
                     char temp[SMALL_BUFFER];
                     // We pass 'line' here, parse_prop_line handles its own skipping but we should pass key_start?
                     // Actually parse_prop_line takes the line pointer and scans for key.
                     // But we already found the key manually?
                     // reusing parse_prop_line is fine as it re-checks key.
                     const char* ptr = line; // passing original line start to parse_prop_line
                     if (parse_prop_line(&ptr, "model name", temp, sizeof(temp))) {
                         strcpy(model, temp);
                     }
                 }
            } 
            // Count Processors (Logical Cores)
            else if (strncmp(key_start, "processor", 9) == 0) {
                cores++;
            }
            // Frequency (cpu MHz)
            else if (mhz == 0.0 && strncmp(key_start, "cpu MHz", 7) == 0) {
                 const char* ptr = line;
                 char temp[32];
                 if (parse_prop_line(&ptr, "cpu MHz", temp, sizeof(temp))) {
                     mhz = atof(temp);
                 }
            }
            
            line = (char*)line_end;
            if (line < end && *line == '\n') line++;
        }
    }
    
    // Fallback for frequency if not found in cpuinfo (often static base freq there)
    if (mhz == 0.0) {
         char freq_buf[64];
         if (read_file_fast("/sys/devices/system/cpu/cpu0/cpufreq/scaling_max_freq", freq_buf, sizeof(freq_buf))) {
             mhz = atof(freq_buf) / 1000.0;
         } else if (read_file_fast("/sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_max_freq", freq_buf, sizeof(freq_buf))) {
             mhz = atof(freq_buf) / 1000.0;
         }
    }

    // Format: Model (Cores) @ Freq GHz
    // Cleanup model name: remove "Processor", "CPU", duplicate spaces?
    // User requested direct copy of fastfetch logic essentially.
    
    if (cores > 0 && mhz > 0.0) {
        snprintf(cpu, SMALL_BUFFER, "%s (%d) @ %.2f GHz", model, cores, mhz / 1000.0);
    } else if (cores > 0) {
        snprintf(cpu, SMALL_BUFFER, "%s (%d)", model, cores);
    } else {
        strcpy(cpu, model);
    }
}

// GPU Detection using PCI IDs (Strictly File-Based)

typedef struct {
    char vendor[128];
    char name[128];
} gpu_info_t;

// Load PCI IDs file into a buffer
static int load_pci_ids(char** buffer, size_t* size) {
    const char* paths[] = {
        "/usr/share/hwdata/pci.ids",
        "/usr/share/misc/pci.ids",
        "/usr/share/pciids/pci.ids",
        "/var/lib/pciutils/pci.ids",
        NULL
    };
    
    for (int i = 0; paths[i]; i++) {
        int fd = open(paths[i], O_RDONLY);
        if (fd != -1) {
            struct stat st;
            if (fstat(fd, &st) == 0 && st.st_size > 0) {
                *size = st.st_size;
                *buffer = malloc(*size + 1);
                if (*buffer) {
                    if (read(fd, *buffer, *size) == *size) {
                        (*buffer)[*size] = '\0';
                        close(fd);
                        return 1;
                    }
                    free(*buffer);
                }
            }
            close(fd);
        }
    }
    return 0;
}

// Parse PCI IDs to find vendor and device name
// Ported/Adapted from fastfetch's gpu_pci.c logic
static void parse_pci_ids(const char* pci_ids, size_t pci_ids_len, 
                         unsigned int vendor_id, unsigned int device_id, 
                         gpu_info_t* info) {
    if (!pci_ids || !info) return;

    // Default hex strings if not found
    if (!info->vendor[0]) snprintf(info->vendor, sizeof(info->vendor), "%04x", vendor_id);
    if (!info->name[0]) snprintf(info->name, sizeof(info->name), "%04x", device_id);

    char vendor_search[32];
    int vlen = snprintf(vendor_search, sizeof(vendor_search), "\n%04x  ", vendor_id);
    
    // Search for vendor
    const char* v_start = (char*)memmem(pci_ids, pci_ids_len, vendor_search, vlen);
    if (!v_start) return;
    
    v_start += vlen; // Skip ID and spaces
    const char* v_end = strchr(v_start, '\n');
    if (!v_end) return;
    
    // Copy vendor name
    size_t name_len = v_end - v_start;
    if (name_len >= sizeof(info->vendor)) name_len = sizeof(info->vendor) - 1;
    strncpy(info->vendor, v_start, name_len);
    info->vendor[name_len] = '\0';
    
    // Search for device within this vendor block
    // The vendor block ends at the next line starting with a hex digit (or end of file)
    // Device lines start with check tab + hex id
    
    char device_search[32];
    int dlen = snprintf(device_search, sizeof(device_search), "\n\t%04x  ", device_id);
    
    const char* current = v_end; // Start searching from end of vendor line
    const char* d_start = NULL;
    
    // Limit search to next vendor or EOF
    while (*current) {
        if (*current == '\n' && isxdigit(current[1])) break; // Start of next vendor
        
        // Use memmem for safer search within block? No, just scan manually or use strstr carefully
        // Ideally we search until we hit next vendor.
        
        // Let's rely on finding the specific device string before we hit confirmed next vendor (non-tab start)
        const char* potential = (char*)memmem(current, pci_ids + pci_ids_len - current, device_search, dlen);
        if (!potential) return; // Not found in rest of file
        
        // Check if we passed a vendor boundary
        // Find closest newline before 'potential' to check if it's a vendor line?
        // Actually, memmem finds "\n\t%04x  ", so it guarantees it's a device line format. 
        // We just need to ensure we haven't crossed into another vendor's block.
        // But since we search specifically for device ID, collisions are possible across vendors.
        // So strict parsing is better.
        
        // Optimization: scan line by line
        const char* next_line = strchr(current, '\n');
        if (!next_line) break;
        current = next_line + 1;
        
        if (current[0] != '\t' && current[0] != '#') break; // New vendor or junk, stop
        
        // Check if this line matches our device
        if (current[0] == '\t' && strncmp(current + 1, device_search + 2, 4) == 0) { // +2 skip \n\t
             d_start = current + 1 + 6; // \tXXXX  (6 chars)
             break;
        }
    }
    
    if (d_start) {
        const char* d_end = strchr(d_start, '\n');
        if (!d_end) d_end = pci_ids + pci_ids_len;
        
        // Check for brackets [Name]
        const char* bracket_open = memchr(d_start, '[', d_end - d_start);
        const char* bracket_close = memchr(d_start, ']', d_end - d_start);
        
        if (bracket_open && bracket_close && bracket_close > bracket_open) {
            d_start = bracket_open + 1;
            name_len = bracket_close - bracket_open - 1;
        } else {
            name_len = d_end - d_start;
        }
        
        if (name_len >= sizeof(info->name)) name_len = sizeof(info->name) - 1;
        strncpy(info->name, d_start, name_len);
        info->name[name_len] = '\0';
    }
}

// Clean up vendor name (strip Corporation, Inc, etc.)
static void normalize_vendor(char* vendor, unsigned int vendor_id) {
    // Fastfetch-style overrides based on ID
    switch (vendor_id) {
        case 0x10de: strcpy(vendor, "NVIDIA"); return;
        case 0x1002: 
        case 0x1022: strcpy(vendor, "AMD"); return;
        case 0x8086: strcpy(vendor, "Intel"); return;
        case 0x106b: strcpy(vendor, "Apple"); return;
        case 0x15ad: strcpy(vendor, "VMware"); return;
        case 0x80ee: strcpy(vendor, "VirtualBox"); return;
        case 0x1414: strcpy(vendor, "Microsoft"); return;
    }

    // Fallback cleanup
    if (strstr(vendor, " Corporation")) *strstr(vendor, " Corporation") = '\0';
    else if (strstr(vendor, " Corp.")) *strstr(vendor, " Corp.") = '\0';
    else if (strstr(vendor, ", Inc.")) *strstr(vendor, ", Inc.") = '\0';
    else if (strstr(vendor, " Inc.")) *strstr(vendor, " Inc.") = '\0';
}

static void get_gpu(char* gpu) {
    strcpy(gpu, "Unknown");
    
    DIR* pci_dir = opendir("/sys/bus/pci/devices");
    if (!pci_dir) return;
    
    char* pci_ids_content = NULL;
    size_t pci_ids_len = 0;
    int has_pci_ids = load_pci_ids(&pci_ids_content, &pci_ids_len);
    
    struct dirent* entry;
    // We want to find the "best" GPU. 
    // Heuristic: Discrete > Integrated. 3D Controller > VGA Compatible.
    
    int best_score = -1;
    gpu_info_t best_gpu = {0};
    
    while ((entry = readdir(pci_dir)) != NULL) {
        if (entry->d_name[0] == '.') continue;
        
        char path[512], buffer[256];
        snprintf(path, sizeof(path), "/sys/bus/pci/devices/%s/class", entry->d_name);
        if (!read_file_fast(path, buffer, sizeof(buffer))) continue;
        
        unsigned int class_code = (unsigned int)strtoul(buffer, NULL, 16);
        // 0x0300 = VGA, 0x0302 = 3D Controller, 0x0380 = Display
        if ((class_code >> 16) != 0x03) continue;
        
        // Read vendor/device
        snprintf(path, sizeof(path), "/sys/bus/pci/devices/%s/vendor", entry->d_name);
        read_file_fast(path, buffer, sizeof(buffer));
        unsigned int vendor_id = (unsigned int)strtoul(buffer, NULL, 16);
        
        snprintf(path, sizeof(path), "/sys/bus/pci/devices/%s/device", entry->d_name);
        read_file_fast(path, buffer, sizeof(buffer));
        unsigned int device_id = (unsigned int)strtoul(buffer, NULL, 16);
        
        gpu_info_t current_gpu = {0};
        if (has_pci_ids) {
            parse_pci_ids(pci_ids_content, pci_ids_len, vendor_id, device_id, &current_gpu);
        } else {
            // Fallback: fastfetch generic naming
             const char* vendor_name = NULL;
             switch (vendor_id) {
                case 0x10de: vendor_name = "NVIDIA"; break;
                case 0x1002: case 0x1022: vendor_name = "AMD"; break;
                case 0x8086: vendor_name = "Intel"; break;
                default: break;
            }
            if (vendor_name) strcpy(current_gpu.vendor, vendor_name);
            else snprintf(current_gpu.vendor, sizeof(current_gpu.vendor), "%04x", vendor_id);
            
            snprintf(current_gpu.name, sizeof(current_gpu.name), "%04x", device_id);
        }
        
        // Calculate score
        int score = 0;
        // Discrete vs Integrated logic would go here (checking specific vendor/device ranges or memory usage)
        // For now, prioritize NVIDIA/AMD over Intel, assuming Intel is often iGPU
        if (vendor_id == 0x10de) score += 100; // NVIDIA
        if (vendor_id == 0x1002) score += 90;  // AMD
        if (vendor_id == 0x8086) score += 10;  // Intel
        
        if (score > best_score) {
            best_score = score;
            best_gpu = current_gpu;
            
            // Normalize vendor name immediately for scoring/candidate selection context if needed, 
            // but definitely before final output.
            normalize_vendor(best_gpu.vendor, vendor_id);
        }
    }
    
    closedir(pci_dir);
    if (pci_ids_content) free(pci_ids_content);
    
    if (best_score >= 0) {
        // GPU: NVIDIA GeForce RTX 3070 Ti [Discrete]
        // Add [Discrete] or [Integrated] based on vendor? 
        // Heuristic: NVIDIA/AMD usually discrete, Intel usually integrated (unless Arc)
        
        const char* type_str = "";
        // Simple heuristic - fastfetch checks memory/pci class etc. 
        // We will assume NVIDIA/AMD are discrete and Intel is integrated for now to match user's logic roughly.
        // Or better: check previous best_score logic?
        
        if (strcmp(best_gpu.vendor, "NVIDIA") == 0 || strcmp(best_gpu.vendor, "AMD") == 0) {
            type_str = " [Discrete]";
        } else if (strcmp(best_gpu.vendor, "Intel") == 0) {
            type_str = " [Integrated]";
        }
        
        snprintf(gpu, SMALL_BUFFER, "%s %s%s", best_gpu.vendor, best_gpu.name, type_str);
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

// Helper to parse Nix manifest.json and count installed packages
static int count_nix_packages_in_json(const char* buffer) {
    if (!buffer) return 0;
    
    // Modern manifest format uses "elements" object, legacy might be different
    // We look for keys in the "elements" object if likely new format, or "active": true
    
    // Robust-ish JSON parsing for this specific file structure without full JSON parser
    // New format (simplified view): {"elements": {"/nix/store/...": {...}, "/nix/store/...": {...}}, ...}
    
    int count = 0;
    const char* elements = strstr(buffer, "\"elements\"");
    if (elements) {
        elements = strchr(elements, '{');
        if (elements) {
            // We are inside the elements object. Count keys.
            // A primitive way: count valid Store Paths that are keys.
            // Or simpler: count ":" that are not inside nested objects? No, too risky.
            // Let's rely on the structure: "/nix/store/..." : { ... }
            
            const char* p = elements + 1;
            int brace_level = 1;
            int in_string = 0;
            
            while (*p) {
                if (*p == '"' && (p == buffer || *(p-1) != '\\')) {
                    in_string = !in_string;
                    
                    // If we just stared a string and we are at brace_level 1 using a comma-separation or start, it's a key
                    // But JSON keys are followed by :
                    
                } else if (!in_string) {
                    if (*p == '{') {
                        brace_level++;
                    } else if (*p == '}') {
                        brace_level--;
                        if (brace_level == 0) break; // End of elements
                    } else if (*p == ':' && brace_level == 1) {
                         // We found a key-value pair at level 1, which means a package entry
                         // Verify it's not a meta-field by strictly assuming keys are store paths?
                         // "fastfetch" filters out -doc, -info, etc. 
                         // For now, let's just count all entries as "packages"
                         count++;
                    }
                }
                p++;
            }
        }
    } else {
        // Fallback or older format
        // Count occurences of "active": true ?
        count = count_strings_in_buffer(buffer, "\"active\":true");
    }
    
    return count;
}

// Helper to count packages in a specific profile path
static int get_nix_profile_count(const char* path) {
    char buffer[BUFFER_SIZE * 16]; // Manifests can be large
    if (read_file_fast(path, buffer, sizeof(buffer))) {
        return count_nix_packages_in_json(buffer);
    }
    return 0;
}

// Fast Nix package counting via profile manifest scanning (File-based only, NO COMMANDS)
static int count_nix_packages(const char* strata_path) {
    (void)strata_path; // Unused
    int total = 0;
    char path[512];
    const char* home = getenv("HOME");
    
    // 1. User Profile (~/.nix-profile/manifest.json)
    if (home) {
        snprintf(path, sizeof(path), "%s/.nix-profile/manifest.json", home);
        total += get_nix_profile_count(path);
    }
    
    // 2. Default Profile
    total += get_nix_profile_count("/nix/var/nix/profiles/default/manifest.json");
    
    // 3. System Profile
    total += get_nix_profile_count("/run/current-system/sw/manifest.json");

    // 4. Per-user Profile
    const char* user = getenv("USER");
    if (user) {
        snprintf(path, sizeof(path), "/etc/profiles/per-user/%s/manifest.json", user);
        total += get_nix_profile_count(path);
    }

    // 5. XDG State Profile
    const char* xdg_state = getenv("XDG_STATE_HOME");
    if (xdg_state) {
        snprintf(path, sizeof(path), "%s/nix/profile/manifest.json", xdg_state);
        total += get_nix_profile_count(path);
    } else if (home) {
        snprintf(path, sizeof(path), "%s/.local/state/nix/profile/manifest.json", home);
        total += get_nix_profile_count(path);
    }

    return total;
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
    printf(RESET BOLD " │" NORD8 "██" RESET BOLD "│ " NORD12 "Distro: " NORD4 "%s\n", info->distro);
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
    printf(RESET BOLD " │" NORD7 "██" RESET BOLD "│ " NORD12 "Distro: " NORD4 "%s\n", info->distro);
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

// Print CachyOS ASCII art
static void print_cachyos_fetch(const struct sysinfo_fast* info) {
    // New design provided by user
    // Color scheme: Gray/White as requested with subtle CachyOS teal accents
    // + : NORD4 (White)
    // - : NORD3 (Gray)
    // / \ : NORD7 (Teal) or NORD8 (Cyan)
    
    printf(RESET BOLD " ┌──┐" NORD1 BOLD " ┌──────────────────────────────────┐ " NORD11 BOLD "┌────┐\n");
    printf(RESET BOLD " │" NORD1 "▒▒" RESET BOLD "│" NORD1 BOLD " │─────" NORD7 "/" NORD3 "--" NORD4 "++++++++++" NORD3 "----" NORD7 "/" NORD1 BOLD "───────────│ " NORD11 BOLD "│ 境 │\n");
    printf(RESET BOLD " │" NORD0 "██" RESET BOLD "│" NORD1 BOLD " │────" NORD7 "//" NORD4 "+++++++++++" NORD3 "----" NORD7 "/" NORD1 BOLD "─────" NORD7 "/\\\\" NORD1 BOLD "────│ " NORD11 BOLD "│    │\n");
    printf(RESET BOLD " │" NORD1 "██" RESET BOLD "│" NORD1 BOLD " │───" NORD7 "//" NORD4 "++++++++++++++++" NORD1 BOLD "──────" NORD7 "\\//" NORD1 BOLD "────│ " NORD11 BOLD "│ 界 │\n");
    printf(RESET BOLD " │" NORD11 "██" RESET BOLD "│" NORD1 BOLD " │──" NORD7 "//" NORD4 "++" NORD3 "---" NORD4 "+" NORD7 "//" NORD1 BOLD "──────────────────────│ " NORD11 BOLD "└────┘\n");
    printf(RESET BOLD " │" NORD12 "██" RESET BOLD "│" NORD1 BOLD " │─" NORD7 "//" NORD3 "---" NORD4 "+++" NORD7 "//" NORD1 BOLD "────────────" NORD7 "/+\\\\" NORD1 BOLD "───────│\n");
    printf(RESET BOLD " │" NORD13 "██" RESET BOLD "│" NORD1 BOLD " │─" NORD7 "\\\\" NORD4 "++++" NORD3 "--" NORD7 "/" NORD1 BOLD "─────────────" NORD7 "\\-//" NORD1 BOLD "───────│\n");
    printf(RESET BOLD " │" NORD14 "██" RESET BOLD "│" NORD1 BOLD " │──" NORD7 "\\\\" NORD3 "--" NORD4 "+++" NORD7 "\\" NORD1 BOLD "──────────────────" NORD7 "/++\\\\" NORD1 BOLD "─│\n");
    printf(RESET BOLD " │" NORD7 "██" RESET BOLD "│" NORD1 BOLD " │───" NORD7 "\\\\" NORD4 "+++" NORD3 "--" NORD7 "\\" NORD1 BOLD "─────────────────" NORD7 "\\--//" NORD1 BOLD "─│\n");
    printf(RESET BOLD " │" NORD8 "██" RESET BOLD "│" NORD1 BOLD " │────" NORD7 "\\\\" NORD3 "--" NORD4 "++++" NORD3 "-+" NORD4 "---" NORD4 "+" NORD3 "--" NORD4 "++++++" NORD7 "/" NORD1 BOLD "───────│\n");
    printf(RESET BOLD " │" NORD9 "██" RESET BOLD "│" NORD1 BOLD " │─────" NORD7 "\\" NORD3 "--" NORD4 "+++++++++++++++" NORD3 "--" NORD7 "/" NORD1 BOLD "────────│\n");
    printf(RESET BOLD " │" NORD10 "██" RESET BOLD "│" NORD1 BOLD " │──────" NORD7 "\\" NORD3 "-" NORD4 "++++++++++++" NORD3 "----" NORD7 "/" NORD1 BOLD "─────────│\n");
    printf(RESET BOLD " │" NORD15 "██" RESET BOLD "│" NORD1 BOLD " └──────────────────────────────────┘\n");
    printf(RESET BOLD " │" NORD7 "██" RESET BOLD "│ " NORD12 "Distro: " NORD4 "%s\n", info->distro);
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
    } else if (info->system_type == SYSTEM_CACHYOS) {
        print_cachyos_fetch(info);
    } else {
        print_bedrock_fetch(info);
    }
}

int main(int argc, char* argv[]) {
    struct sysinfo_fast info;
    
    // Check for command line arguments
    int force_system_type = -1; // -1 for auto-detect
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--gentoo") == 0) {
            force_system_type = SYSTEM_GENTOO;
        } else if (strcmp(argv[i], "--cachyos") == 0) {
            force_system_type = SYSTEM_CACHYOS;
        } else if (strcmp(argv[i], "--bedrock") == 0) {
            force_system_type = SYSTEM_BEDROCK;
        } else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--version") == 0) {
            printf("bfetch version 1.2.0-ricemeat\n");
            return 0;
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            printf("bfetch - Ultra-fast system information display\n");
            printf("Usage: %s [OPTIONS]\n\n", argv[0]);
            printf("Options:\n");
            printf("  -v, --version    Show version information\n");
            printf("  -h, --help       Show this help message\n");
            printf("      --gentoo     Force Gentoo mode\n");
            printf("      --cachyos    Force CachyOS mode\n");
            printf("      --bedrock    Force Bedrock mode\n");
            return 0;
        }
    }
    
    // Detect system type (or force mode)
    if (force_system_type != -1) {
        info.system_type = (system_type_t)force_system_type;
    } else {
        info.system_type = detect_system_type();
    }
    
    // Gather all system information dynamically
    get_distro(info.distro);
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
