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
#include <sys/ioctl.h>
#include <stdint.h>
#if defined(__i386__) || defined(__x86_64__)
#include <cpuid.h>
#endif

// Buffer sizes optimized for performance
#define BUFFER_SIZE 65536
#define SMALL_BUFFER 256
#define LINE_BUFFER 1024

// Nord colors as ANSI escape codes for maximum speed
#define RESET "\033[0m"
#define BOLD "\033[1m"
#define NORD0 "\033[30m"    // #2E3440
#define NORD1 "\033[90m"    // #3B4252
#define NORD2 "\033[37m"    // #434C5E
#define NORD3 "\033[97m"    // #4C566A
#define NORD4 "\033[97m"    // #D8DEE9
#define NORD5 "\033[37m"    // #E5E9F0
#define NORD6 "\033[97m"    // #ECEFF4
#define NORD7 "\033[36m"    // #8FBCBB
#define NORD8 "\033[96m"    // #88C0D0
#define NORD9 "\033[34m"    // #81A1C1
#define NORD10 "\033[94m"   // #5E81AC
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

// --------------------------------------------------------------------------------
// Performance Helper Functions
// --------------------------------------------------------------------------------

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

static int count_dir(const char* path) {
    DIR* d = opendir(path);
    if (!d) return 0;
    int count = 0;
    struct dirent* de;
    while ((de = readdir(d))) {
        if (de->d_name[0] != '.') count++;
    }
    closedir(d);
    return count;
}

// --------------------------------------------------------------------------------
// CPU Detection: CPUID Assembly (Fastest)
// --------------------------------------------------------------------------------
static void get_cpu(char* cpu) {
#if defined(__i386__) || defined(__x86_64__)
    unsigned int eax, ebx, ecx, edx;
    char brand[49] = {0};

    __get_cpuid(0x80000000, &eax, &ebx, &ecx, &edx);
    if (eax >= 0x80000004) {
        unsigned int* b = (unsigned int*)brand;
        for (unsigned int i = 0; i < 3; i++) {
            __get_cpuid(0x80000002 + i, &b[i*4+0], &b[i*4+1], &b[i*4+2], &b[i*4+3]);
        }
        brand[48] = '\0';
        char* s = brand;
        while (*s == ' ') s++;
        
        char clean_brand[SMALL_BUFFER];
        char* d = clean_brand;
        int space = 0;
        const char* keywords[] = {
            "Six-Core", "Eight-Core", "Quad-Core", "Twelve-Core", "Sixteen-Core", 
            "24-Core", "32-Core", "64-Core", "6-Core", "8-Core", "12-Core", "16-Core", 
            "-Core", "Core", "Processor", "with Radeon Graphics", "with Graphics", NULL
        };
        
        while (*s && (d - clean_brand) < SMALL_BUFFER - 64) {
             if (*s == '@') break; 
             
             int matched = 0;
             for (int i = 0; keywords[i]; i++) {
                 size_t klen = strlen(keywords[i]);
                 if (strncasecmp(s, keywords[i], klen) == 0) {
                     s += klen;
                     matched = 1;
                     break;
                 }
             }
             if (matched) continue;

             if (*s == ' ') {
                 if (!space) { *d++ = ' '; space = 1; }
                 s++;
             } else {
                 *d++ = *s++;
                 space = 0;
             }
        }
        while (d > clean_brand) {
            char last = *(d-1);
            if (last == ' ' || last == '-' || last == '/') { d--; continue; }
            if (isdigit(last)) {
                if (d > clean_brand + 1 && *(d-2) == ' ') { d--; continue; }
            }
            break;
        }
        *d = '\0';

        int threads = get_nprocs();
        double ghz = 0.0;
        char freq_buf[32];
        if (read_file_fast("/sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_max_freq", freq_buf, sizeof(freq_buf))) {
            ghz = strtod(freq_buf, NULL) / 1000000.0;
        }

        if (ghz > 0.1) {
            snprintf(cpu, SMALL_BUFFER, "%s (%d) @ %.2f GHz", clean_brand, threads, ghz);
        } else {
            snprintf(cpu, SMALL_BUFFER, "%s (%d)", clean_brand, threads);
        }
    } else {
        strcpy(cpu, "Unknown Processor");
    }
#elif defined(__arm__) || defined(__aarch64__)
    char buf[BUFFER_SIZE];
    if (read_file_fast("/proc/cpuinfo", buf, sizeof(buf))) {
        char* model = strstr(buf, "model name");
        if (!model) model = strstr(buf, "Hardware");
        if (!model) model = strstr(buf, "Processor");
        
        if (model) {
            char* start = strchr(model, ':');
            if (start) {
                start++;
                while (*start == ' ') start++;
                char* end = strchr(start, '\n');
                if (end) {
                    size_t len = end - start;
                    if (len > SMALL_BUFFER - 1) len = SMALL_BUFFER - 1;
                    memcpy(cpu, start, len);
                    cpu[len] = '\0';
                    
                    int threads = get_nprocs();
                    char freq_buf[32];
                    double ghz = 0.0;
                    if (read_file_fast("/sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_max_freq", freq_buf, sizeof(freq_buf))) {
                        ghz = strtod(freq_buf, NULL) / 1000000.0;
                    }
                    
                    if (ghz > 0.1) {
                        char final_cpu[SMALL_BUFFER];
                        snprintf(final_cpu, SMALL_BUFFER, "%s (%d) @ %.2f GHz", cpu, threads, ghz);
                        strcpy(cpu, final_cpu);
                    } else {
                        char final_cpu[SMALL_BUFFER];
                        snprintf(final_cpu, SMALL_BUFFER, "%s (%d)", cpu, threads);
                        strcpy(cpu, final_cpu);
                    }
                    return;
                }
            }
        }
    }
    strcpy(cpu, "ARM Processor");
#else
    strcpy(cpu, "Unknown Processor");
#endif
}

// --------------------------------------------------------------------------------
// GPU Detection: Fast DRM-based lookup
// --------------------------------------------------------------------------------
static void get_gpu(char* gpu) {
    char buf[16], path[64];
    unsigned int vendor = 0, device = 0, sub_vendor = 0, sub_device = 0;
    
    // Scan card0-card9 to find first valid GPU
    for (int i = 0; i < 10 && vendor == 0; i++) {
        snprintf(path, sizeof(path), "/sys/class/drm/card%d/device/vendor", i);
        if (read_file_fast(path, buf, sizeof(buf))) {
            vendor = strtoul(buf, NULL, 16);
            snprintf(path, sizeof(path), "/sys/class/drm/card%d/device/device", i);
            if (read_file_fast(path, buf, sizeof(buf))) {
                device = strtoul(buf, NULL, 16);
            }
            // attempt to get subsystem IDs for more specific naming
            snprintf(path, sizeof(path), "/sys/class/drm/card%d/device/subsystem_vendor", i);
            if (read_file_fast(path, buf, sizeof(buf))) sub_vendor = strtoul(buf, NULL, 16);
            snprintf(path, sizeof(path), "/sys/class/drm/card%d/device/subsystem_device", i);
            if (read_file_fast(path, buf, sizeof(buf))) sub_device = strtoul(buf, NULL, 16);
        } else {
            // Fallback for integrated GPUs (ARM) that don't have a PCI vendor file
            snprintf(path, sizeof(path), "/sys/class/drm/card%d/device/uevent", i);
            char uevent[BUFFER_SIZE];
            if (read_file_fast(path, uevent, sizeof(uevent))) {
                char* p = strstr(uevent, "DRIVER=");
                if (p) {
                    p += 7;
                    char* end = strchr(p, '\n');
                    if (end) {
                        size_t len = end - p;
                        if (len > SMALL_BUFFER - 1) len = SMALL_BUFFER - 1;
                        memcpy(gpu, p, len);
                        gpu[len] = '\0';
                        // Capitalize driver name as a fallback for GPU name idk
                        gpu[0] = toupper(gpu[0]);
                        return;
                    }
                }
            }
        }
    }
    
    if (vendor == 0) { strcpy(gpu, "Unknown GPU"); return; }
    
    // For AMD, get specific marketing name from amdgpu.ids
    if (vendor == 0x1002) {
        // gettin amdgpu.ids (Specific model based on revision)
        char rev_buf[16];
        unsigned int revision = 0;
        for (int i = 0; i < 8; i++) {
            snprintf(path, sizeof(path), "/sys/class/drm/card%d/device/revision", i);
            if (read_file_fast(path, rev_buf, sizeof(rev_buf))) {
                revision = strtoul(rev_buf, NULL, 16);
                break;
            }
        }

        int afd = open("/usr/share/libdrm/amdgpu.ids", O_RDONLY);
        if (afd != -1) {
            struct stat ast; fstat(afd, &ast);
            char* amap = mmap(NULL, ast.st_size, PROT_READ, MAP_PRIVATE, afd, 0);
            close(afd);
            if (amap != MAP_FAILED) {
                char s_key[16];
                snprintf(s_key, sizeof(s_key), "%04X,\t%02X,", device, revision);
                const char* s_line = memmem(amap, ast.st_size, s_key, strlen(s_key));
                if (!s_line) {
                    snprintf(s_key, sizeof(s_key), "%04X, %02X,", device, revision);
                    s_line = memmem(amap, ast.st_size, s_key, strlen(s_key));
                }
                if (s_line) {
                    const char* s_name = s_line + strlen(s_key);
                    while (*s_name == ' ' || *s_name == '\t') s_name++;
                    const char* s_end = strchr(s_name, '\n');
                    if (s_end) {
                        size_t slen = s_end - s_name;
                        if (slen > SMALL_BUFFER - 1) slen = SMALL_BUFFER - 1;
                        memcpy(gpu, s_name, slen); gpu[slen] = '\0';
                        munmap(amap, ast.st_size); return;
                    }
                }
                munmap(amap, ast.st_size);
            }
        }
    }

    // Open pci.ids
    int fd = open("/usr/share/hwdata/pci.ids", O_RDONLY);
    if (fd == -1) fd = open("/usr/share/misc/pci.ids", O_RDONLY);
    if (fd == -1) {
        if (vendor == 0x10de) snprintf(gpu, SMALL_BUFFER, "NVIDIA GPU 0x%04x", device);
        else if (vendor == 0x1002) snprintf(gpu, SMALL_BUFFER, "AMD GPU 0x%04x", device);
        else snprintf(gpu, SMALL_BUFFER, "GPU 0x%04x:0x%04x", vendor, device);
        return;
    }
    
    struct stat st;
    fstat(fd, &st);
    char* map = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);
    if (map == MAP_FAILED) { strcpy(gpu, "Unknown GPU"); return; }
    
    char v_search[8];
    snprintf(v_search, sizeof(v_search), "\n%04x", vendor);
    const char* v_line = memmem(map, st.st_size, v_search, 5);
    if (v_line) {
        v_line++;
        const char* v_end = strchr(v_line + 4, '\n');
        if (v_end) {
            char d_search[12];
            snprintf(d_search, sizeof(d_search), "\n\t%04x", device);
            const char* d_line = memmem(v_end, st.st_size - (v_end - map), d_search, 6);
            if (d_line) {
                const char* d_name = d_line + 6;
                while (*d_name == ' ' || *d_name == '\t') d_name++;
                const char* d_end = strchr(d_name, '\n');

                // If we have subsystem info, try to find the specific manufacturer name
                if (sub_vendor && sub_device) {
                    char s_search[32];
                    snprintf(s_search, sizeof(s_search), "\n\t\t%04x %04x", sub_vendor, sub_device);
                    const char* s_line = memmem(d_name, st.st_size - (d_name - map), s_search, strlen(s_search));
                    if (s_line && s_line < (d_line + 10000)) { 
                        const char* s_name = s_line + strlen(s_search);
                        while (*s_name == ' ' || *s_name == '\t') s_name++;
                        const char* s_end = strchr(s_name, '\n');
                        if (s_end) { d_name = s_name; d_end = s_end; }
                    }
                }

                const char* br = strchr(d_name, '[');
                if (br && br < d_end) {
                    const char* br_end = strchr(br, ']');
                    if (br_end && br_end < d_end) { d_name = br + 1; d_end = br_end; }
                }
                if (d_end) {
                    size_t len = d_end - d_name;
                    if (len > 120) len = 120;
                    const char* prefix = "";
                    if (d_name[0] != 'N' && d_name[0] != 'A') { // Simple heuristic to avoid double prefix
                        prefix = (vendor == 0x10de) ? "NVIDIA " : (vendor == 0x1002) ? "AMD " : "";
                    }
                    snprintf(gpu, SMALL_BUFFER, "%s%.*s", prefix, (int)len, d_name);
                    munmap(map, st.st_size);
                    return;
                }
            }
        }
    }
    munmap(map, st.st_size);
    if (vendor == 0x10de) strcpy(gpu, "NVIDIA GPU");
    else if (vendor == 0x1002) strcpy(gpu, "AMD GPU");
    else strcpy(gpu, "Unknown GPU");
}


// --------------------------------------------------------------------------------
// Terminal Detection: readlink (Fastest)
// --------------------------------------------------------------------------------
static void get_terminal(char* terminal) {
    if (getenv("TERM_PROGRAM")) {
        strncpy(terminal, getenv("TERM_PROGRAM"), SMALL_BUFFER-1);
        return;
    }
    char path[64], buf[256];
    pid_t ppid = getppid();
    snprintf(path, sizeof(path), "/proc/%d/exe", ppid);
    ssize_t len = readlink(path, buf, sizeof(buf)-1);
    if (len != -1) {
        buf[len] = '\0';
        char* name = strrchr(buf, '/');
        name = name ? name + 1 : buf;
        if (strcmp(name, "bash") != 0 && strcmp(name, "zsh") != 0 && strcmp(name, "fish") != 0 && strcmp(name, "sh") != 0) {
            strcpy(terminal, name);
            return;
        }
        char stat_path[64], content[512];
        snprintf(stat_path, sizeof(stat_path), "/proc/%d/stat", ppid);
        if (read_file_fast(stat_path, content, sizeof(content))) {
            char* p = strrchr(content, ')');
            if (p) {
                pid_t pppid;
                sscanf(p + 2, "%*c %d", &pppid);
                snprintf(path, sizeof(path), "/proc/%d/exe", pppid);
                len = readlink(path, buf, sizeof(buf)-1);
                if (len != -1) {
                    buf[len] = '\0';
                    name = strrchr(buf, '/');
                    strcpy(terminal, name ? name + 1 : buf);
                    return;
                }
            }
        }
    }
    strcpy(terminal, getenv("TERM") ? getenv("TERM") : "Unknown");
}

// --------------------------------------------------------------------------------
// Package Counting Optimized
// --------------------------------------------------------------------------------

static int count_nix_manifest(const char* path) {
    int fd = open(path, O_RDONLY);
    if (fd == -1) return 0;
    struct stat st;
    fstat(fd, &st);
    if (st.st_size == 0) { close(fd); return 0; }
    char* map = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    int count = 0;
    if (map != MAP_FAILED) {
        const char* needle = strstr(path, ".json") ? "\"active\":true" : "name = \"";
        size_t nlen = strlen(needle);
        const char* p = map;
        while ((p = memmem(p, st.st_size - (p - map), needle, nlen))) {
            count++;
            p += nlen;
        }
        munmap(map, st.st_size);
    }
    close(fd);
    return count;
}

static int count_dpkg() {
    DIR* d = opendir("/var/lib/dpkg/info");
    if (!d) return 0;
    int count = 0; struct dirent* de;
    while ((de = readdir(d))) {
        if (de->d_name[0] == '.') continue;
        size_t len = strlen(de->d_name);
        if (len > 5 && strcmp(de->d_name + len - 5, ".list") == 0) count++;
    }
    closedir(d);
    return count;
}

static void get_packages(char* packages, system_type_t system_type) {
    int total_pacman = count_dir("/var/lib/pacman/local");
    int total_dpkg = count_dpkg();
    int total_nix = 0;
    int total_flatpak = count_dir("/var/lib/flatpak/app");
    int total_snap = count_dir("/var/lib/snapd/snaps");
    const char* home = getenv("HOME");
    if (home) {
        char p[512];
        snprintf(p, sizeof(p), "%s/.local/share/flatpak/app", home); total_flatpak += count_dir(p);
        snprintf(p, sizeof(p), "%s/.nix-profile/manifest.json", home); total_nix += count_nix_manifest(p);
        if (total_nix == 0) { snprintf(p, sizeof(p), "%s/.nix-profile/manifest.nix", home); total_nix += count_nix_manifest(p); }
        snprintf(p, sizeof(p), "%s/.local/state/nix/profiles/home-manager/manifest.json", home); total_nix += count_nix_manifest(p);
    }
    total_nix += count_nix_manifest("/nix/var/nix/profiles/default/manifest.json");
    total_nix += count_nix_manifest("/run/current-system/sw/manifest.json");

    if (system_type == SYSTEM_GENTOO) {
        DIR* cat_dir = opendir("/var/db/pkg");
        if (cat_dir) {
            int e_count = 0; struct dirent* ce;
            while ((ce = readdir(cat_dir))) {
                if (ce->d_name[0] == '.' || ce->d_type != DT_DIR) continue;
                char cp[1024]; snprintf(cp, sizeof(cp), "/var/db/pkg/%s", ce->d_name);
                e_count += count_dir(cp);
            }
            closedir(cat_dir);
            snprintf(packages, BUFFER_SIZE, "%d (emerge)", e_count);
            return;
        }
    }

    char res[SMALL_BUFFER] = ""; int off = 0;
    if (total_pacman > 0) off += snprintf(res + off, SMALL_BUFFER - off, "%d (pacman), ", total_pacman);
    if (total_dpkg > 0) off += snprintf(res + off, SMALL_BUFFER - off, "%d (dpkg), ", total_dpkg);
    if (total_flatpak > 0) off += snprintf(res + off, SMALL_BUFFER - off, "%d (flatpak), ", total_flatpak);
    if (total_snap > 0) off += snprintf(res + off, SMALL_BUFFER - off, "%d (snap), ", total_snap);
    if (total_nix > 0) off += snprintf(res + off, SMALL_BUFFER - off, "%d (nix), ", total_nix);
    if (off > 2) { res[off - 2] = '\0'; strcpy(packages, res); } else strcpy(packages, "Unknown");
}

// COMBINED: Reads /etc/os-release ONCE, sets both distro and system_type
static system_type_t get_distro_and_type(char* distro) {
    char buf[1024];
    system_type_t type = SYSTEM_OTHER;
    
    if (read_file_fast("/etc/os-release", buf, sizeof(buf))) {
        char* p = strstr(buf, "PRETTY_NAME=\"");
        if (p) {
            p += 13;
            char* end = strchr(p, '"');
            if (end) { *end = 0; strcpy(distro, p); }
        }
        if (strcasestr(buf, "cachyos")) type = SYSTEM_CACHYOS;
        else if (strcasestr(buf, "gentoo")) type = SYSTEM_GENTOO;
        else if (strcasestr(buf, "bedrock")) type = SYSTEM_BEDROCK;
    }
    
    if (type == SYSTEM_OTHER && access("/bedrock", F_OK) == 0) type = SYSTEM_BEDROCK;
    if (distro[0] == '\0') strcpy(distro, "Linux");
    return type;
}

static void get_kernel(char* kernel) {
    struct utsname u;
    if (uname(&u) == 0) strcpy(kernel, u.release);
    else strcpy(kernel, "Unknown");
}

static void get_uptime(char* uptime) {
    char buf[64];
    if (read_file_fast("/proc/uptime", buf, sizeof(buf))) {
        unsigned long secs = strtoul(buf, NULL, 10);
        long m = secs / 60, h = m / 60, d = h / 24;
        if (d > 0) snprintf(uptime, SMALL_BUFFER, "%ldd %ldh %ldm", d, h % 24, m % 60);
        else snprintf(uptime, SMALL_BUFFER, "%ldh %ldm", h, m % 60);
    } else strcpy(uptime, "Unknown");
}

static void get_memory(char* memory) {
    char buf[2048];
    if (read_file_fast("/proc/meminfo", buf, sizeof(buf))) {
        unsigned long long total = 0, available = 0;
        char* p = strstr(buf, "MemTotal:");
        if (p) { p += 9; while (*p == ' ') p++; total = strtoull(p, NULL, 10); }
        p = strstr(buf, "MemAvailable:");
        if (p) { p += 13; while (*p == ' ') p++; available = strtoull(p, NULL, 10); }
        unsigned long long used = total - available;
        snprintf(memory, SMALL_BUFFER, "%.2f GiB / %.2f GiB", (double)used / 1048576.0, (double)total / 1048576.0);
    } else strcpy(memory, "Unknown");
}

static void get_wm(char* wm) {
    char* v = getenv("XDG_CURRENT_DESKTOP");
    if (!v) v = getenv("DESKTOP_SESSION");
    if (v) strcpy(wm, v); else strcpy(wm, "Unknown");
}


// Buffered output builder
#define OUTPUT_BUFFER 16384
static char g_out[OUTPUT_BUFFER];
static int g_off = 0;

#define OUT(...) g_off += snprintf(g_out + g_off, OUTPUT_BUFFER - g_off, __VA_ARGS__)

static void print_gentoo_fetch(const struct sysinfo_fast* info) {
    OUT(RESET BOLD " ┌──┐" NORD1 " ┌──────────────────────────────────┐ " NORD15 BOLD "┌─────┐\n");
    OUT(RESET BOLD " │" NORD1 "▒▒" RESET BOLD "│" NORD1 " │─────────" RESET BOLD "\\\\\\\\\\\\\\\\\\\\" NORD1 "───────────────│ " NORD15 BOLD "│  G  │\n");
    OUT(RESET BOLD " │" NORD0 "██" RESET BOLD "│" NORD1 " │───────" RESET BOLD "//+++++++++++\\" NORD1 BOLD "─────────────│ " NORD15 BOLD "│  e  │\n");
    OUT(RESET BOLD " │" NORD1 "██" RESET BOLD "│" NORD1 " │──────" RESET BOLD "//+++++" NORD1 BOLD "\\\\\\" RESET BOLD "+++++\\" NORD1 BOLD "────────────│ " NORD15 BOLD "│  n  │\n");
    OUT(RESET BOLD " │" NORD11 "██" RESET BOLD "│" NORD1 " │─────" RESET BOLD "//+++++" NORD1 BOLD "// " RESET BOLD "/" RESET BOLD "+++++++\\" NORD1 BOLD "──────────│ " NORD15 BOLD "│  t  │\n");
    OUT(RESET BOLD " │" NORD12 "██" RESET BOLD "│" NORD1 " │──────" RESET BOLD "+++++++" NORD1 BOLD "\\\\" RESET BOLD "++++++++++\\" NORD1 BOLD "────────│ " NORD15 BOLD "│  o  │\n");
    OUT(RESET BOLD " │" NORD13 "██" RESET BOLD "│" NORD1 " │────────" RESET BOLD "++++++++++++++++++" NORD1 BOLD "\\\\" NORD1 "──────│ " NORD15 BOLD "│  o  │\n");
    OUT(RESET BOLD " │" NORD14 "██" RESET BOLD "│" NORD1 " │─────────" RESET BOLD "//++++++++++++++" NORD1 BOLD "//" NORD1 "───────│ " NORD15 BOLD "└─────┘\n");
    OUT(RESET BOLD " │" NORD7 "██" RESET BOLD "│" NORD1 " │───────" RESET BOLD "//++++++++++++++" NORD1 BOLD "//" NORD1 "─────────│ \n");
    OUT(RESET BOLD " │" NORD8 "██" RESET BOLD "│" NORD1 " │──── " RESET BOLD "//++++++++++++++" NORD1 BOLD "//" NORD1 "───────────│ \n");
    OUT(RESET BOLD " │" NORD9 "██" RESET BOLD "│" NORD1 " │─────" RESET BOLD "//++++++++++" NORD1 BOLD "//" NORD1 "───────────────│\n");
    OUT(RESET BOLD " │" NORD10 "██" RESET BOLD "│" NORD1 " │─────" RESET BOLD "//+++++++" NORD1 BOLD "//" NORD1 "──────────────────│\n");
    OUT(RESET BOLD " │" NORD15 "██" RESET BOLD "│" NORD1 " │──────" RESET BOLD "////////" NORD1 BOLD "────────────────────│\n");
    OUT(RESET BOLD " │" NORD7 "██" RESET BOLD "│" NORD1 " └──────────────────────────────────┘\n");
    OUT(RESET BOLD " │" NORD8 "██" RESET BOLD "│ " NORD12 "Distro: " NORD4 "%s\n", info->distro);
    OUT(RESET BOLD " │" NORD9 "██" RESET BOLD "│ " NORD12 "Kernel: " NORD4 "%s\n", info->kernel);
    OUT(RESET BOLD " │" NORD10 "██" RESET BOLD "│ " NORD15 "Uptime: " NORD4 "%s\n", info->uptime);
    OUT(RESET BOLD " │" NORD15 "██" RESET BOLD "│ " NORD15 "WM: " NORD4 "%s\n", info->wm);
    OUT(RESET BOLD " │" NORD11 "██" RESET BOLD "│ " NORD15 "Packages: " NORD4 "%s\n", info->packages);
    OUT(RESET BOLD " │" NORD12 "██" RESET BOLD "│ " NORD13 "Terminal: " NORD4 "%s\n", info->terminal);
    OUT(RESET BOLD " │" NORD13 "██" RESET BOLD "│ " NORD13 "Memory: " NORD4 "%s\n", info->memory);
    OUT(RESET BOLD " │" NORD14 "██" RESET BOLD "│ " NORD13 "Shell: " NORD4 "%s\n", info->shell);
    OUT(RESET BOLD " │" NORD7 "██" RESET BOLD "│ " NORD9 "CPU: " NORD4 "%s\n", info->cpu);
    OUT(RESET BOLD " │" NORD1 "▒▒" RESET BOLD "│ " NORD9 "GPU: " NORD4 "%s\n", info->gpu);
    OUT(RESET BOLD " └──┘" RESET "\n");
}

static void print_bedrock_fetch(const struct sysinfo_fast* info) {
    OUT(RESET BOLD " ┌──┐" NORD1 BOLD " ┌──────────────────────────────────┐ " NORD11 BOLD "┌────┐\n");
    OUT(RESET BOLD " │" NORD1 "▒▒" RESET BOLD "│" NORD1 BOLD " │─" RESET BOLD "\\\\\\\\\\\\\\\\\\\\\\\\\\" NORD1 BOLD "────────────────────│ " NORD11 BOLD "│ 境 │\n");
    OUT(RESET BOLD " │" NORD0 "██" RESET BOLD "│" NORD1 BOLD " │──" RESET BOLD "\\\\\\      \\\\\\" NORD1 BOLD "────────────────────│ " NORD11 BOLD "│    │\n");
    OUT(RESET BOLD " │" NORD1 "██" RESET BOLD "│" NORD1 BOLD " │───" RESET BOLD "\\\\\\      \\\\\\" NORD1 BOLD "───────────────────│ " NORD11 BOLD "│ 界 │\n");
    OUT(RESET BOLD " │" NORD11 "██" RESET BOLD "│" NORD1 BOLD " │────" RESET BOLD "\\\\\\      \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\" NORD1 BOLD "────│ " NORD11 BOLD "└────┘\n");
    OUT(RESET BOLD " │" NORD12 "██" RESET BOLD "│" NORD1 BOLD " │─────" RESET BOLD "\\\\\\                    \\\\\\" NORD1 BOLD "───│\n");
    OUT(RESET BOLD " │" NORD13 "██" RESET BOLD "│" NORD1 BOLD " │──────" RESET BOLD "\\\\\\                    \\\\\\" NORD1 BOLD "──│\n");
    OUT(RESET BOLD " │" NORD14 "██" RESET BOLD "│" NORD1 BOLD " │───────" RESET BOLD "\\\\\\        ──────      \\\\\\" NORD1 BOLD "─│\n");
    OUT(RESET BOLD " │" NORD7 "██" RESET BOLD "│" NORD1 BOLD " │────────" RESET BOLD "\\\\\\                   ///" NORD1 BOLD "─│\n");
    OUT(RESET BOLD " │" NORD8 "██" RESET BOLD "│" NORD1 BOLD " │─────────" RESET BOLD "\\\\\\                 ///" NORD1 BOLD "──│\n");
    OUT(RESET BOLD " │" NORD9 "██" RESET BOLD "│" NORD1 BOLD " │──────────" RESET BOLD "\\\\\\               ///" NORD1 BOLD "───│\n");
    OUT(RESET BOLD " │" NORD10 "██" RESET BOLD "│" NORD1 BOLD " │───────────" RESET BOLD "\\\\\\////////////////" NORD1 BOLD "────│\n");
    OUT(RESET BOLD " │" NORD15 "██" RESET BOLD "│" NORD1 BOLD " └──────────────────────────────────┘\n");
    OUT(RESET BOLD " │" NORD7 "██" RESET BOLD "│ " NORD12 "Distro: " NORD4 "%s\n", info->distro);
    OUT(RESET BOLD " │" NORD8 "██" RESET BOLD "│ " NORD12 "Kernel: " NORD4 "%s\n", info->kernel);
    OUT(RESET BOLD " │" NORD9 "██" RESET BOLD "│ " NORD15 "Uptime: " NORD4 "%s\n", info->uptime);
    OUT(RESET BOLD " │" NORD10 "██" RESET BOLD "│ " NORD15 "WM: " NORD4 "%s\n", info->wm);
    OUT(RESET BOLD " │" NORD15 "██" RESET BOLD "│ " NORD15 "Packages: " NORD4 "%s\n", info->packages);
    OUT(RESET BOLD " │" NORD11 "██" RESET BOLD "│ " NORD13 "Terminal: " NORD4 "%s\n", info->terminal);
    OUT(RESET BOLD " │" NORD12 "██" RESET BOLD "│ " NORD13 "Memory: " NORD4 "%s\n", info->memory);
    OUT(RESET BOLD " │" NORD13 "██" RESET BOLD "│ " NORD13 "Shell: " NORD4 "%s\n", info->shell);
    OUT(RESET BOLD " │" NORD14 "██" RESET BOLD "│ " NORD9 "CPU: " NORD4 "%s\n", info->cpu);
    OUT(RESET BOLD " │" NORD1 "▒▒" RESET BOLD "│ " NORD9 "GPU: " NORD4 "%s\n", info->gpu);
    OUT(RESET BOLD " └──┘" RESET "\n");
}

static void print_cachyos_fetch(const struct sysinfo_fast* info) {
    OUT(RESET BOLD " ┌──┐" NORD1 BOLD " ┌──────────────────────────────────┐ " NORD11 BOLD "┌────┐\n");
    OUT(RESET BOLD " │" NORD1 "▒▒" RESET BOLD "│" NORD1 BOLD " │─────" NORD7 "/" NORD3 "--" NORD4 "++++++++++" NORD3 "----" NORD7 "/" NORD1 BOLD "───────────│ " NORD11 BOLD "│ 境 │\n");
    OUT(RESET BOLD " │" NORD0 "██" RESET BOLD "│" NORD1 BOLD " │────" NORD7 "//" NORD4 "+++++++++++" NORD3 "----" NORD7 "/" NORD1 BOLD "─────" NORD7 "/\\\\" NORD1 BOLD "────│ " NORD11 BOLD "│    │\n");
    OUT(RESET BOLD " │" NORD1 "██" RESET BOLD "│" NORD1 BOLD " │───" NORD7 "//" NORD4 "++++++++++++++++" NORD1 BOLD "──────" NORD7 "\\//" NORD1 BOLD "────│ " NORD11 BOLD "│ 界 │\n");
    OUT(RESET BOLD " │" NORD11 "██" RESET BOLD "│" NORD1 BOLD " │──" NORD7 "//" NORD4 "++" NORD3 "---" NORD4 "+" NORD7 "//" NORD1 BOLD "──────────────────────│ " NORD11 BOLD "└────┘\n");
    OUT(RESET BOLD " │" NORD12 "██" RESET BOLD "│" NORD1 BOLD " │─" NORD7 "//" NORD3 "---" NORD4 "+++" NORD7 "//" NORD1 BOLD "────────────" NORD7 "/+\\\\" NORD1 BOLD "───────│\n");
    OUT(RESET BOLD " │" NORD13 "██" RESET BOLD "│" NORD1 BOLD " │─" NORD7 "\\\\" NORD4 "++++" NORD3 "--" NORD7 "/" NORD1 BOLD "─────────────" NORD7 "\\-//" NORD1 BOLD "───────│\n");
    OUT(RESET BOLD " │" NORD14 "██" RESET BOLD "│" NORD1 BOLD " │──" NORD7 "\\\\" NORD3 "--" NORD4 "+++" NORD7 "\\" NORD1 BOLD "──────────────────" NORD7 "/++\\\\" NORD1 BOLD "─│\n");
    OUT(RESET BOLD " │" NORD7 "██" RESET BOLD "│" NORD1 BOLD " │───" NORD7 "\\\\" NORD4 "+++" NORD3 "--" NORD7 "\\" NORD1 BOLD "─────────────────" NORD7 "\\--//" NORD1 BOLD "─│\n");
    OUT(RESET BOLD " │" NORD8 "██" RESET BOLD "│" NORD1 BOLD " │────" NORD7 "\\\\" NORD3 "--" NORD4 "++++" NORD3 "-+" NORD4 "---" NORD4 "+" NORD3 "--" NORD4 "++++++" NORD7 "/" NORD1 BOLD "───────│\n");
    OUT(RESET BOLD " │" NORD9 "██" RESET BOLD "│" NORD1 BOLD " │─────" NORD7 "\\" NORD3 "--" NORD4 "+++++++++++++++" NORD3 "--" NORD7 "/" NORD1 BOLD "────────│\n");
    OUT(RESET BOLD " │" NORD10 "██" RESET BOLD "│" NORD1 BOLD " │──────" NORD7 "\\" NORD3 "-" NORD4 "++++++++++++" NORD3 "----" NORD7 "/" NORD1 BOLD "─────────│\n");
    OUT(RESET BOLD " │" NORD15 "██" RESET BOLD "│" NORD1 BOLD " └──────────────────────────────────┘\n");
    OUT(RESET BOLD " │" NORD7 "██" RESET BOLD "│ " NORD12 "Distro: " NORD4 "%s\n", info->distro);
    OUT(RESET BOLD " │" NORD8 "██" RESET BOLD "│ " NORD12 "Kernel: " NORD4 "%s\n", info->kernel);
    OUT(RESET BOLD " │" NORD9 "██" RESET BOLD "│ " NORD15 "Uptime: " NORD4 "%s\n", info->uptime);
    OUT(RESET BOLD " │" NORD10 "██" RESET BOLD "│ " NORD15 "WM: " NORD4 "%s\n", info->wm);
    OUT(RESET BOLD " │" NORD15 "██" RESET BOLD "│ " NORD15 "Packages: " NORD4 "%s\n", info->packages);
    OUT(RESET BOLD " │" NORD11 "██" RESET BOLD "│ " NORD13 "Terminal: " NORD4 "%s\n", info->terminal);
    OUT(RESET BOLD " │" NORD12 "██" RESET BOLD "│ " NORD13 "Memory: " NORD4 "%s\n", info->memory);
    OUT(RESET BOLD " │" NORD13 "██" RESET BOLD "│ " NORD13 "Shell: " NORD4 "%s\n", info->shell);
    OUT(RESET BOLD " │" NORD14 "██" RESET BOLD "│ " NORD9 "CPU: " NORD4 "%s\n", info->cpu);
    OUT(RESET BOLD " │" NORD1 "▒▒" RESET BOLD "│ " NORD9 "GPU: " NORD4 "%s\n", info->gpu);
    OUT(RESET BOLD " └──┘" RESET "\n");
}

static void print_fetch(const struct sysinfo_fast* info) {
    if (info->system_type == SYSTEM_GENTOO) print_gentoo_fetch(info);
    else if (info->system_type == SYSTEM_CACHYOS) print_cachyos_fetch(info);
    else print_bedrock_fetch(info);
}

int main(int argc, char* argv[]) {
    struct sysinfo_fast info = {0};
    int force_type = -1;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--gentoo") == 0) force_type = SYSTEM_GENTOO;
        else if (strcmp(argv[i], "--cachyos") == 0) force_type = SYSTEM_CACHYOS;
        else if (strcmp(argv[i], "--bedrock") == 0) force_type = SYSTEM_BEDROCK;
        else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--version") == 0) {
            printf("bfetch version 2.4.0-fastasf\n"); return 0;
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "--Help") == 0) {
            printf("Usage: %s [OPTIONS]\n", argv[0]);
            printf("Options:\n");
            printf("  -v, --version    Show version\n");
            printf("  -h, --Help       Show this help\n");
            printf("  --gentoo         Force Gentoo mode\n");
            printf("  --cachyos        Force CachyOS mode\n");
            printf("  --bedrock        Force Bedrock mode\n");
            return 0;
        }
    }
    
    // Combined distro + system type (single file read)
    info.system_type = (force_type != -1) ? (system_type_t)force_type : get_distro_and_type(info.distro);
    if (force_type != -1 && info.distro[0] == '\0') {
        get_distro_and_type(info.distro);
    }
    
    get_kernel(info.kernel);
    get_uptime(info.uptime);
    get_memory(info.memory);
    get_wm(info.wm);
    get_terminal(info.terminal);
    get_cpu(info.cpu);
    get_gpu(info.gpu);
    get_packages(info.packages, info.system_type);
    
    char* sh = getenv("SHELL");
    if (sh) { char* b = strrchr(sh, '/'); strcpy(info.shell, b ? b + 1 : sh); }
    else strcpy(info.shell, "Unknown");

    print_fetch(&info);
    
    // Single write syscall
    write(STDOUT_FILENO, g_out, g_off);
    
    return 0;
}

