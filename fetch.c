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
#include <sys/wait.h>
#include <sys/mman.h>
#include <time.h>

// Buffer sizes optimized for performance
#define BUFFER_SIZE 8192
#define SMALL_BUFFER 256
#define PCI_IDS_PATH "/usr/share/hwdata/pci.ids"

// Nord colors
#define RESET "\033[0m"
#define BOLD "\033[1m"
#define NORD0 "\033[30m"
#define NORD1 "\033[90m"
#define NORD2 "\033[37m"
#define NORD3 "\033[97m"
#define NORD4 "\033[97m"
#define NORD5 "\033[37m"
#define NORD6 "\033[97m"
#define NORD7 "\033[36m"
#define NORD8 "\033[96m"
#define NORD9 "\033[34m"
#define NORD10 "\033[94m"
#define NORD11 "\033[91m"
#define NORD12 "\033[93m"
#define NORD13 "\033[33m"
#define NORD14 "\033[32m"
#define NORD15 "\033[95m"

typedef enum {
    SYSTEM_BEDROCK,
    SYSTEM_GENTOO,
    SYSTEM_CACHYOS,
    SYSTEM_OTHER
} system_type_t;

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

static int read_file_fast(const char* path, char* buffer, size_t size) {
    int fd = open(path, O_RDONLY);
    if (fd == -1) return 0;
    ssize_t bytes_read = read(fd, buffer, size - 1);
    close(fd);
    if (bytes_read > 0) {
        buffer[bytes_read] = '\0';
        return 1;
    }
    return 0;
}

// CPU ID
static void get_cpu(char* cpu) {
    unsigned int eax, ebx, ecx, edx;
    char brand[49];
    brand[0] = '\0';
    __asm__ volatile("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(0x80000000));
    if (eax >= 0x80000004) {
        unsigned int* b = (unsigned int*)brand;
        for (unsigned int i = 0; i < 3; i++) {
            __asm__ volatile("cpuid" : "=a"(b[i*4+0]), "=b"(b[i*4+1]), "=c"(b[i*4+2]), "=d"(b[i*4+3]) : "a"(0x80000002 + i));
        }
        brand[48] = '\0';
        char* s = brand;
        while (*s == ' ') s++;
        char* d = cpu;
        int space = 0;
        while (*s && (d - cpu) < SMALL_BUFFER - 1) {
             if (*s == ' ') { if (!space) { *d++ = ' '; space = 1; } } 
             else { *d++ = *s; space = 0; }
             s++;
        }
        if (d > cpu && *(d-1) == ' ') d--;
        *d = '\0';
    } else strcpy(cpu, "Unknown"); 
}

// --------------------------------------------------------------------------------
// GPU Detection: MMAP + Binary Search (Optimized)
// --------------------------------------------------------------------------------

static unsigned int parse_hex4(const char* s) {
    unsigned int val = 0;
    for (int i = 0; i < 4; i++) {
        char c = s[i];
        val <<= 4;
        if (c >= '0' && c <= '9') val |= (c - '0');
        else if (c >= 'a' && c <= 'f') val |= (c - 'a' + 10);
        else if (c >= 'A' && c <= 'F') val |= (c - 'A' + 10);
        else return 0;
    }
    return val;
}

// Find the start of the vendor block for `vendor_id`.
// Returns pointer to line "\nXXXX  ..." inside map or NULL.
static const char* find_vendor_line(const char* map, size_t size, unsigned int target_id) {
    // Binary Search on file text
    const char* start = map;
    const char* end = map + size;
    const char* low = start;
    const char* high = end;
    
    // Safety check for empty or small file
    if (size < 100) return NULL; // Fallback

    while (low < high) {
        const char* mid = low + (high - low) / 2;
        
        // Align mid to previous newline to start parsing a line
        const char* line_start = mid;
        while (line_start > start && *line_start != '\n') line_start--;
        
        // Now line_start points to \n (or start of file).
        // Check what kind of line we have.
        // We need to find the "Current Vendor" required to compare with target_id.
        // If we are on a device line (\n\tXXXX), or sub-device (\n\t\t), we scan BACKWARDS to find the Vendor.
        
        const char* vendor_line = line_start;
        unsigned int current_id = 0;
        int found_v = 0;
        
        // Scan backwards safely
        while (vendor_line >= start) {
            // Check if this line is a Vendor line
            // Format: "\nXXXX  " (if > start) or "XXXX  " (if == start)
            const char* check = (vendor_line == start) ? vendor_line : vendor_line + 1;
            
            if (isxdigit(check[0]) && isxdigit(check[1]) && isxdigit(check[2]) && isxdigit(check[3]) &&
                check[4] == ' ' && check[5] == ' ') {
                current_id = parse_hex4(check);
                found_v = 1;
                break;
            }
            // Skip comments or blank lines or tabs
            // Move back to previous line
            if (vendor_line == start) break;
            vendor_line--;
            while (vendor_line > start && *vendor_line != '\n') vendor_line--;
        }
        
        if (!found_v) {
            // If we didn't find a vendor line scanning back, we might be in the header comments.
            // Move low up past this block.
            low = mid + 1;
            continue;
        }
        
        if (current_id == target_id) {
            return (vendor_line == start) ? vendor_line : vendor_line; 
            // Note: if vendor_line > start, it points to '\n'. 
        } else if (current_id < target_id) {
            // Target is further down.
            // We must advance 'low'.
            // To be safe, set low to end of current line at 'mid' or 'line_start'?
            // If current_id < target, we need to go deeper.
            // But 'mid' might be far into the 'current_id' block.
            // We should Set low = mid + 1.
            low = mid + 1;
        } else {
            // current_id > target_id.
            // Target is before 'vendor_line'.
            high = vendor_line;
        }
    }
    return NULL;
}

static void get_gpu(char* gpu) {
    strcpy(gpu, "Unknown");
    int fd_pci = open(PCI_IDS_PATH, O_RDONLY);
    char* pci_map = NULL;
    size_t pci_size = 0;
    
    if (fd_pci != -1) {
        struct stat st;
        if (fstat(fd_pci, &st) == 0 && st.st_size > 0) {
            pci_size = st.st_size;
            pci_map = mmap(NULL, pci_size, PROT_READ, MAP_PRIVATE | MAP_POPULATE, fd_pci, 0);
            if (pci_map == MAP_FAILED) pci_map = NULL;
        }
        close(fd_pci);
    }
    
    // Scan sysfs for GPU without readdir?
    // /sys/class/drm/card0/device/vendor ?
    // Try card0 and card1 (for iGPU + dGPU systems)
    // Heuristic: Check card0. If it exists, check likely secondary.
    int best_score = -1;
    char best_vendor[64] = "";
    char best_device[128] = "";
    
    for (int i = 0; i < 4; i++) {
        char path[128], buf[64];
        snprintf(path, sizeof(path), "/sys/class/drm/card%d/device/vendor", i);
        if (!read_file_fast(path, buf, sizeof(buf))) continue;
        unsigned int vendor_id = (unsigned int)strtoul(buf, NULL, 16);
        
        snprintf(path, sizeof(path), "/sys/class/drm/card%d/device/device", i);
        if (!read_file_fast(path, buf, sizeof(buf))) continue;
        unsigned int device_id = (unsigned int)strtoul(buf, NULL, 16);
        
        int score = 0;
        if (vendor_id == 0x10de) score = 100;      // NVIDIA
        else if (vendor_id == 0x1002) score = 90;  // AMD
        else if (vendor_id == 0x8086) score = 10;  // Intel
        
        if (score > best_score) {
            best_score = score;
            
            if (pci_map) {
                // Find Vendor
                const char* v_line = find_vendor_line(pci_map, pci_size, vendor_id);
                if (v_line) {
                    const char* name_start = (v_line == pci_map) ? v_line + 6 : v_line + 7;
                    const char* nl = (const char*)memchr(name_start, '\n', 256);
                    int len = nl ? (nl - name_start) : 32;
                    if (len > 63) len = 63;
                    strncpy(best_vendor, name_start, len);
                    best_vendor[len] = '\0';
                    
                    char d_query[8];
                    snprintf(d_query, sizeof(d_query), "\n\t%04x  ", device_id);
                    const char* curr = nl ? nl : name_start;
                    int found_d = 0;
                    
                    while (curr < pci_map + pci_size) {
                        if (curr[0] == '\n' && isxdigit(curr[1]) && isxdigit(curr[4])) {
                             if (isxdigit(curr[1]) && isxdigit(curr[2]) && isxdigit(curr[3]) && 
                                 curr[5] == ' ' && curr[6] == ' ') break; 
                        }
                        
                        if (curr[0] == '\n' && curr[1] == '\t' && 
                            parse_hex4(curr + 2) == device_id && curr[6] == ' ' && curr[7] == ' ') {
                            const char* d_start = curr + 8;
                            const char* d_end = (const char*)memchr(d_start, '\n', 256);
                            if (d_end) {
                                int dlen = d_end - d_start;
                                if (dlen > SMALL_BUFFER - 1) dlen = SMALL_BUFFER - 1;
                                const char* ob = memchr(d_start, '[', dlen);
                                const char* cb = memchr(d_start, ']', dlen);
                                if (ob && cb && cb > ob) {
                                    d_start = ob + 1;
                                    dlen = cb - ob - 1;
                                }
                                strncpy(best_device, d_start, dlen);
                                best_device[dlen] = '\0';
                                found_d = 1;
                            }
                            break;
                        }
                        curr = (const char*)memchr(curr + 1, '\n', 1024);
                        if (!curr) break;
                    }
                    if (!found_d) snprintf(best_device, sizeof(best_device), "Device %04x", device_id);
                } else {
                    snprintf(best_vendor, sizeof(best_vendor), "Vendor %04x", vendor_id);
                    snprintf(best_device, sizeof(best_device), "Device %04x", device_id);
                }
            } else {
                 if (vendor_id == 0x10de) strcpy(best_vendor, "NVIDIA");
                 else if (vendor_id == 0x1002) strcpy(best_vendor, "AMD");
                 else if (vendor_id == 0x8086) strcpy(best_vendor, "Intel");
                 else snprintf(best_vendor, sizeof(best_vendor), "Vendor %04x", vendor_id);
                 snprintf(best_device, sizeof(best_device), "Device %04x", device_id);
            }
        }
        if (score >= 90) break; // Optimization: Stop if we found a discrete GPU
    }
    
    if (pci_map) munmap(pci_map, pci_size);
    
    if (best_score >= 0) {
        char* corp = strstr(best_vendor, " Corporation"); if (corp) *corp = 0;
        corp = strstr(best_vendor, ", Inc."); if (corp) *corp = 0;
        snprintf(gpu, SMALL_BUFFER, "%s %s", best_vendor, best_device);
    }
}

// Terminal
static void get_terminal(char* terminal) {
    if (getenv("TERM_PROGRAM")) {
        strncpy(terminal, getenv("TERM_PROGRAM"), SMALL_BUFFER-1);
        return;
    }
    pid_t pid = getppid();
    char path[256];
    char buf[256];
    ssize_t len;
    snprintf(path, sizeof(path), "/proc/%d/exe", pid);
    if ((len = readlink(path, buf, sizeof(buf)-1)) != -1) {
        buf[len] = '\0';
        char* name = strrchr(buf, '/');
        if (name) name++; else name = buf;
        if (strcmp(name, "bfetch") != 0 && strcmp(name, "bash") != 0 && strcmp(name, "zsh") != 0) {
            strncpy(terminal, name, SMALL_BUFFER-1);
            return;
        }
    }
    if (getenv("TERM")) strncpy(terminal, getenv("TERM"), SMALL_BUFFER-1);
    else strcpy(terminal, "Unknown");
}

static int count_dir(const char* path) {
    // Optimization: Check st_nlink for directories (e.g. pacman)
    // Works on Ext4, XFS. Btrfs often returns 1.
    struct stat st;
    if (stat(path, &st) == 0) {
        if (st.st_nlink > 2) return st.st_nlink - 2;
    }

    DIR* dir = opendir(path);
    if (!dir) return 0;
    int c = 0;
    while (readdir(dir)) c++;
    closedir(dir);
    return c > 2 ? c - 2 : 0;
}

static void get_packages(char* packages, system_type_t system_type) {
    int count = count_dir("/var/lib/pacman/local");
    char mgr[32] = "pacman";
    int nix = count_dir("/nix/var/nix/profiles/default/bin"); // Fast proxy
    
    char result[SMALL_BUFFER] = "";
    if (count > 0) snprintf(result, sizeof(result), "%d (%s)", count, mgr);
    if (nix > 0) {
        char tmp[64];
        snprintf(tmp, sizeof(tmp), "%s%d (nix)", count > 0 ? "," : "", nix);
        strcat(result, tmp);
    }
    if (strlen(result) == 0) strcpy(packages, "Unknown");
    else strcpy(packages, result);
}

static void get_wm(char* wm) {
    char* val = getenv("XDG_CURRENT_DESKTOP");
    if (val) { strncpy(wm, val, SMALL_BUFFER-1); return; }
    strcpy(wm, "Unknown");
}

static system_type_t detect_system_type() {
    if (access("/bedrock", F_OK) == 0) return SYSTEM_BEDROCK;
    return SYSTEM_CACHYOS; // Assume
}

static void get_kernel(char* kernel) {
    struct utsname u;
    if (uname(&u) == 0) strcpy(kernel, u.release);
    else strcpy(kernel, "Unknown");
}

static void get_uptime(char* uptime) {
    struct sysinfo s;
    if (sysinfo(&s) == 0) {
        long h = s.uptime / 3600;
        long m = (s.uptime % 3600) / 60;
        snprintf(uptime, SMALL_BUFFER, "%ldh %ldm", h, m);
    } else strcpy(uptime, "Unknown");
}

static void get_memory(char* memory) {
    struct sysinfo s;
    if (sysinfo(&s) == 0) {
        unsigned long t = s.totalram * s.mem_unit;
        unsigned long u = t - (s.freeram + s.bufferram + s.sharedram) * s.mem_unit;
        snprintf(memory, SMALL_BUFFER, "%.2f GiB / %.2f GiB", (double)u/1073741824.0, (double)t/1073741824.0);
    } else strcpy(memory, "Unknown");
}

// Pre-define os-release read buffer to stack to avoid allocation
static void get_distro(char* distro) {
    char buf[512];
    int fd = open("/etc/os-release", O_RDONLY);
    if (fd != -1) {
        ssize_t n = read(fd, buf, sizeof(buf)-1);
        close(fd);
        if (n > 0) {
            buf[n] = 0;
            char* p = strstr(buf, "PRETTY_NAME=\"");
            if (p) {
                p += 13;
                char* end = strchr(p, '"');
                if (end) {
                    *end = 0;
                    strncpy(distro, p, SMALL_BUFFER-1);
                    return;
                }
            }
        }
    }
    strcpy(distro, "Linux");
}

static void print_cachyos_fetch(const struct sysinfo_fast* info) {
    // ... (Your print logic)
    // Minimizing calls.
    // For brevity in task, I keep your art code logic assumed standard.
    // Pasting the art block from previous state to ensure correctness.
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

int main(int argc, char* argv[]) {
    struct sysinfo_fast info;
    info.system_type = detect_system_type();
    get_cpu(info.cpu);
    get_distro(info.distro);
    get_kernel(info.kernel);
    get_uptime(info.uptime);
    get_memory(info.memory);
    get_wm(info.wm);
    get_terminal(info.terminal);
    get_gpu(info.gpu);
    get_packages(info.packages, info.system_type);
    
    char* sh = getenv("SHELL");
    if(sh) {
        char* b = strrchr(sh, '/');
        strcpy(info.shell, b ? b+1 : sh);
    } else strcpy(info.shell, "Unknown");

    print_cachyos_fetch(&info);
    return 0;
}
