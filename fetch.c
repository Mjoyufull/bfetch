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
    unsigned int eax, ebx, ecx, edx;
    char brand[49] = {0};

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
             if (*s == ' ') {
                 if (!space) { *d++ = ' '; space = 1; }
             } else {
                 *d++ = *s;
                 space = 0;
             }
             s++;
        }
        if (d > cpu && *(d-1) == ' ') d--;
        *d = '\0';
    } else {
        strcpy(cpu, "Unknown Processor");
    }
}

// --------------------------------------------------------------------------------
// GPU Detection: Robust mmap-based lookup
// --------------------------------------------------------------------------------
static void get_gpu(char* gpu) {
    strcpy(gpu, "Unknown GPU");
    DIR* dir = opendir("/sys/bus/pci/devices");
    if (!dir) return;

    const char* paths[] = {
        "/usr/share/hwdata/pci.ids",
        "/usr/share/misc/pci.ids",
        "/usr/share/pciids/pci.ids",
        "/var/lib/pciutils/pci.ids",
        NULL
    };

    int pci_fd = -1;
    struct stat st;
    for (int i = 0; paths[i]; i++) {
        pci_fd = open(paths[i], O_RDONLY);
        if (pci_fd != -1) {
            fstat(pci_fd, &st);
            if (st.st_size > 0) break;
            close(pci_fd); pci_fd = -1;
        }
    }

    struct dirent* de;
    unsigned int best_v = 0, best_d = 0;
    int best_score = -1;

    while ((de = readdir(dir))) {
        if (de->d_name[0] == '.') continue;
        char path[512], buf[16];
        snprintf(path, sizeof(path), "/sys/bus/pci/devices/%s/class", de->d_name);
        if (!read_file_fast(path, buf, sizeof(buf))) continue;
        unsigned int class_code = strtoul(buf, NULL, 16);
        if ((class_code >> 16) != 0x03) continue;

        snprintf(path, sizeof(path), "/sys/bus/pci/devices/%s/vendor", de->d_name);
        read_file_fast(path, buf, sizeof(buf));
        unsigned int v = strtoul(buf, NULL, 16);
        snprintf(path, sizeof(path), "/sys/bus/pci/devices/%s/device", de->d_name);
        read_file_fast(path, buf, sizeof(buf));
        unsigned int d = strtoul(buf, NULL, 16);

        int score = 0;
        if (v == 0x10de) score = 100;
        else if (v == 0x1002 || v == 0x1022) score = 90;
        else if (v == 0x8086) score = 10;
        if (score > best_score) {
            best_score = score; best_v = v; best_d = d;
        }
    }
    closedir(dir);

    if (best_score != -1 && pci_fd != -1) {
        char* map = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, pci_fd, 0);
        if (map != MAP_FAILED) {
            char v_search[8], d_search[12];
            snprintf(v_search, sizeof(v_search), "\n%04x", best_v);
            const char* v_line = memmem(map, st.st_size, v_search, 5);
            if (v_line) {
                v_line++; // Skip \n
                const char* v_name_start = v_line + 4;
                while (*v_name_start == ' ' || *v_name_start == '\t') v_name_start++;
                const char* v_end = strchr(v_name_start, '\n');
                if (v_end) {
                    char v_name[64] = {0};
                    size_t v_len = v_end - v_name_start;
                    if (v_len > 63) v_len = 63;
                    memcpy(v_name, v_name_start, v_len);
                    if (strstr(v_name, " Corporation")) *strstr(v_name, " Corporation") = '\0';
                    
                    snprintf(d_search, sizeof(d_search), "\n\t%04x", best_d);
                    const char* d_line = memmem(v_end, st.st_size - (v_end - map), d_search, 6);
                    if (d_line) {
                        const char* d_name_start = d_line + 6;
                        while (*d_name_start == ' ' || *d_name_start == '\t') d_name_start++;
                        const char* d_end = strchr(d_name_start, '\n');
                        const char* br_open = strchr(d_name_start, '[');
                        if (br_open && br_open < d_end) {
                            const char* br_close = strchr(br_open, ']');
                            if (br_close && br_close < d_end) { d_name_start = br_open + 1; d_end = br_close; }
                        }
                        if (d_end) {
                            char d_name[128] = {0};
                            size_t d_len = d_end - d_name_start;
                            if (d_len > 127) d_len = 127;
                            memcpy(d_name, d_name_start, d_len);
                            snprintf(gpu, SMALL_BUFFER, "%s %s", (best_v == 0x10de) ? "NVIDIA" : (best_v == 0x1002 || best_v == 0x1022) ? "AMD" : v_name, d_name);
                        }
                    }
                }
            }
            munmap(map, st.st_size);
        }
    }
    if (pci_fd != -1) close(pci_fd);
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

// --------------------------------------------------------------------------------
// System Info Gathering
// --------------------------------------------------------------------------------

static void get_distro(char* distro) {
    char buf[1024];
    if (read_file_fast("/etc/os-release", buf, sizeof(buf))) {
        char* p = strstr(buf, "PRETTY_NAME=\"");
        if (p) {
            p += 13; char* end = strchr(p, '"');
            if (end) { *end = 0; strcpy(distro, p); return; }
        }
    }
    strcpy(distro, "Linux");
}

static void get_kernel(char* kernel) {
    struct utsname u;
    if (uname(&u) == 0) strcpy(kernel, u.release);
    else strcpy(kernel, "Unknown");
}

static void get_uptime(char* uptime) {
    struct sysinfo s;
    if (sysinfo(&s) == 0) {
        long m = s.uptime / 60, h = m / 60, d = h / 24;
        if (d > 0) snprintf(uptime, SMALL_BUFFER, "%ldd %ldh %ldm", d, h % 24, m % 60);
        else snprintf(uptime, SMALL_BUFFER, "%ldh %ldm", h, m % 60);
    } else strcpy(uptime, "Unknown");
}

static void get_memory(char* memory) {
    struct sysinfo s;
    if (sysinfo(&s) == 0) {
        unsigned long t = s.totalram * s.mem_unit, f = s.freeram * s.mem_unit;
        unsigned long u = t - f - (s.bufferram * s.mem_unit) - (s.sharedram * s.mem_unit);
        snprintf(memory, SMALL_BUFFER, "%.2f GiB / %.2f GiB", (double)u / 1073741824, (double)t / 1073741824);
    } else strcpy(memory, "Unknown");
}

static void get_wm(char* wm) {
    char* v = getenv("XDG_CURRENT_DESKTOP");
    if (!v) v = getenv("DESKTOP_SESSION");
    if (v) strcpy(wm, v); else strcpy(wm, "Unknown");
}

static system_type_t detect_system_type() {
    char buf[1024];
    if (read_file_fast("/etc/os-release", buf, sizeof(buf))) {
        if (strcasestr(buf, "cachyos")) return SYSTEM_CACHYOS;
        if (strcasestr(buf, "gentoo")) return SYSTEM_GENTOO;
    }
    if (access("/etc/cachyos-release", F_OK) == 0) return SYSTEM_CACHYOS;
    if (access("/etc/gentoo-release", F_OK) == 0) return SYSTEM_GENTOO;
    if (access("/bedrock", F_OK) == 0) return SYSTEM_BEDROCK;
    return SYSTEM_OTHER;
}

// --------------------------------------------------------------------------------
// ASCII ART (PRESERVED)
// --------------------------------------------------------------------------------

static void print_gentoo_fetch(const struct sysinfo_fast* info) {
    printf(RESET BOLD " ┌──┐" NORD1 " ┌──────────────────────────────────┐ " NORD15 BOLD "┌─────┐\n");
    printf(RESET BOLD " │" NORD1 "▒▒" RESET BOLD "│" NORD1 " │─────────" RESET BOLD "\\\\\\\\\\\\\\\\\\\\" NORD1 "────────────────│ " NORD15 BOLD "│  G  │\n");
    printf(RESET BOLD " │" NORD0 "██" RESET BOLD "│" NORD1 " │───────" RESET BOLD "//+++++++++++\\" NORD1 BOLD "─────────────│ " NORD15 BOLD "│  a  │\n");
    printf(RESET BOLD " │" NORD1 "██" RESET BOLD "│" NORD1 " │──────" RESET BOLD "//+++++" NORD1 BOLD "\\\\\\" RESET BOLD "+++++\\" NORD1 BOLD "────────────│ " NORD15 BOLD "│  n  │\n");
    printf(RESET BOLD " │" NORD11 "██" RESET BOLD "│" NORD1 " │─────" RESET BOLD "//+++++" NORD1 BOLD "// " RESET BOLD "/" RESET BOLD "+++++++\\" NORD1 BOLD "──────────│ " NORD15 BOLD "│  y  │\n");
    printf(RESET BOLD " │" NORD12 "██" RESET BOLD "│" NORD1 " │──────" RESET BOLD "+++++++" NORD1 BOLD "\\\\" RESET BOLD "++++++++++\\" NORD1 BOLD "────────│ " NORD15 BOLD "│  m  │\n");
    printf(RESET BOLD " │" NORD13 "██" RESET BOLD "│" NORD1 " │────────" RESET BOLD "++++++++++++++++++" NORD1 BOLD "\\\\" NORD1 "──────│ " NORD15 BOLD "│  e  │\n");
    printf(RESET BOLD " │" NORD14 "██" RESET BOLD "│" NORD1 " │─────────" RESET BOLD "//++++++++++++++" NORD1 BOLD "//" NORD1 "───────│ " NORD15 BOLD "│  d  │\n");
    printf(RESET BOLD " │" NORD7 "██" RESET BOLD "│" NORD1 " │───────" RESET BOLD "//++++++++++++++" NORD1 BOLD "//" NORD1 "─────────│ " NORD15 BOLD "│  e  │\n");
    printf(RESET BOLD " │" NORD8 "██" RESET BOLD "│" NORD1 " │──── " RESET BOLD "//++++++++++++++" NORD1 BOLD "//" NORD1 "───────────│ " NORD15 BOLD "└─────┘\n");
    printf(RESET BOLD " │" NORD9 "██" RESET BOLD "│" NORD1 " │─────" RESET BOLD "//++++++++++" NORD1 BOLD "//" NORD1 "───────────────│\n");
    printf(RESET BOLD " │" NORD10 "██" RESET BOLD "│" NORD1 " │─────" RESET BOLD "//+++++++" NORD1 BOLD "//" NORD1 "──────────────────│\n");
    printf(RESET BOLD " │" NORD15 "██" RESET BOLD "│" NORD1 " │──────" RESET BOLD "////////" NORD1 BOLD "────────────────────│\n");
    printf(RESET BOLD " │" NORD7 "██" RESET BOLD "│" NORD1 " └──────────────────────────────────┘\n");
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

static void print_cachyos_fetch(const struct sysinfo_fast* info) {
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

static void print_fetch(const struct sysinfo_fast* info) {
    if (info->system_type == SYSTEM_GENTOO) print_gentoo_fetch(info);
    else if (info->system_type == SYSTEM_CACHYOS) print_cachyos_fetch(info);
    else print_bedrock_fetch(info);
}

int main(int argc, char* argv[]) {
    struct sysinfo_fast info;
    int force_type = -1;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--gentoo") == 0) force_type = SYSTEM_GENTOO;
        else if (strcmp(argv[i], "--cachyos") == 0) force_type = SYSTEM_CACHYOS;
        else if (strcmp(argv[i], "--bedrock") == 0) force_type = SYSTEM_BEDROCK;
        else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--version") == 0) {
            printf("bfetch version 2.3.6.9-boltnott\n"); return 0;
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
    info.system_type = (force_type != -1) ? (system_type_t)force_type : detect_system_type();
    
    get_distro(info.distro);
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
    return 0;
}
