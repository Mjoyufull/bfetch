# bfetch

A lightweight, ultra-optimized system fetch tool written in C.

`bfetch` is designed for absolute maximum speed. It utilizes low-level techniques like inline assembly and memory mapping to achieve performance levels that exceed even purpose-built "fast" tools.

## Features

- **Blazing Performance**: Execution time is typically **~1.5ms to 2.2ms** (up to 55x faster than fastfetch).
- **CPUID Implementation**: Zero-allocation CPU detection using **inline assembly** (`cpuid`).
- **High-Speed GPU Detection**: Uses `mmap` with substring scanning on the PCI ID database for instant vendor/model identification.
- **Universal Package Counting**:
  - **Nix**: Deep manifest scanning across all profiles (Home Manager, Profiles, Channels, nix-env).
  - **Pacman**: Optimized directory entry counting.
  - **DPKG**: High-speed directory entry scanning.
  - **Flatpak & Snap**: Native filesystem-based counting.
- **Zero-Copy Architecture**: Minimal memory allocations and no subprocess spawning (`popen`/`exec`).
- **Aesthetic**: Custom professional ASCII art for **CachyOS**, **Gentoo**, and **Bedrock Linux**.


## Usage

```bash
# Auto-detect system and run
./bfetch

# Force specific ASCII art/mode
./bfetch --cachyos   # CachyOS mode (Teal/Gray theme)
./bfetch --gentoo    # Gentoo mode (Purple/White theme)
./bfetch --bedrock   # Bedrock Linux mode (Default)
```

## Comparisons

| Tool | Approx. Time | Approach |
|------|-------------|----------|
| **bfetch** | **~0.002s** | Direct C syscalls |
| fastfetch | ~0.01-0.10s | Optimized C |
| neofetch | ~0.20-1.00s | Shell script |

## Installation

### From Source

```bash
git clone https://github.com/Mjoyufull/bfetch.git
cd bfetch

# Standard build (GCC with optimizations)
make

# Run
./bfetch
```

### Nix

```bash
nix run github:Mjoyufull/bfetch
```

## License

MIT License
