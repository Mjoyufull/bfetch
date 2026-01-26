# bfetch

A lightweight, ultra-optimized system fetch tool written in C.

`bfetch` is designed for absolute maximum speed. It utilizes low-level techniques like inline assembly and memory mapping to achieve performance levels that exceed even purpose-built "fast" tools.

## Features

- **Blazing Performance**: Execution time is typically **~2ms** (up to 55x faster than fastfetch).
- **Accurate Memory**: Directly parses `/proc/meminfo` to calculate available memory correctly, excluding cache.
- **Instant GPU Detection**: Scans `/sys/class/drm` for cards instead of traversing the entire PCI bus, using `mmap` for instant model lookup.
- **Buffered Output**: Builds the entire output in memory and flushes with a single `write()` syscall.
- **Universal Package Counting**:
  - **Nix**: Deep manifest scanning via `mmap` substring search.
  - **Pacman**: Optimized directory entry counting.
  - **Flatpak & Snap**: Native filesystem-based counting.
- **Zero-Copy Architecture**: Minimal memory allocations and no subprocess spawning.
- **Aesthetic**: Custom professional ASCII art for **CachyOS**, **Gentoo**, and **Bedrock Linux**.

## Usage

```bash
# Build (defaults to fast mode)
make

# Run
./bfetch

# Show Help
./bfetch --Help
```

## Comparisons

| Tool       | Approx. Time | Approach                          |
| ---------- | ------------ | --------------------------------- |
| **bfetch** | **~0.002s**  | **ASM + DRM Lookup + Direct I/O** |
| fastfetch  | ~0.10s       | Optimized C / libpci              |
| neofetch   | ~0.80s       | Shell script                      |

## Technical Implementation

- **CPU**: Uses `cpuid` inline assembly to fetch the processor brand string.
- **GPU**: Direct `/sys/class/drm/card*` lookup to identifying vendor/device IDs, then memory-maps `pci.ids` for model name resolution.
- **Memory**: Parses `/proc/meminfo` to calculate `Used = Total - Available` without `sysinfo()` syscall overhead.
- **I/O**: Combined `/etc/os-release` read for both distro name and system type detection.
- **Packages**: optimized recursive directory counting and memory-mapped manifest scanning.

## Installation

### From Source

```bash
git clone https://github.com/Mjoyufull/bfetch.git
cd bfetch
make
./bfetch
```

### Nix

```bash
nix run github:Mjoyufull/bfetch
```

## License

MIT License
