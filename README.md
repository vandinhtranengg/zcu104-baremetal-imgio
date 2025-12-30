
# zcu104-baremetal-imgio

A minimal **bare‑metal C++** starter for **AMD Xilinx ZCU104** that:
- mounts the **SD** card (FatFs / `xilffs`),
- loads a **24‑bit BMP** image,
- prints basic info (and sample pixel data),

Tested with **Vivado/Vitis 2022.1** on Zynq UltraScale+ MPSoC (ZCU104).

---

## Features

- ✅ **Bare‑metal A53** (standalone BSP)  
- ✅ **FatFs (xilffs)** SD file I/O  
- ✅ 24‑bit BMP loader (uncompressed, BI_RGB), with **row streaming** option  
- ✅ UART logs (115200 8N1)  

---

## Prerequisites

- **Vivado 2022.1** and **Vitis 2022.1** (same version)
- ZCU104 board powered and connected via **USB‑UART** + **JTAG**
- A microSD card (FAT/FAT32) with `test.bmp` (24‑bit, uncompressed) in the root(0:/)
- Board **boot mode** set appropriately for your workflow (JTAG run for development)

> **Docs you’ll use often**  
> - ZCU104 Quick Start & User Guide (boot DIP SW6, BIST, connectors)  
> - Standalone BSP & libraries (UG643) – FatFs (`xilffs`) parameters  
> - Bare‑metal stack & linker script guidance (UG1137)

---

## Quick start (Vitis Classic IDE)

1. **Create hardware (XSA)**
   - In Vivado: add Zynq MPSoC, apply **ZCU104 preset**  (enable **PS DDR**, **UART**, **SD**)
   - Generate wrapper, validate, export **XSA** (post‑implementation is safest).

2. **Create platform (standalone A53)**
   - Vitis → **New Platform** from your XSA → Domain = `psu_cortexa53_0`, **OS = standalone**.
   - In **BSP settings**, add library **`xilffs`** (FatFs).  
     - `fs_interface = 1 (SD)`, `read_only=false` (default).

3. **Create the app**
   - New C++ Application → Empty app → add `src/sd_bmp_loader.cpp`.

4. **Linker script (heap/stack in DDR)**
   - Project → **Generate Linker Script…**  
   - Map **`.heap`** and **`.stack`** to **`psu_ddr_0`**  
   - Suggested sizes:
     ```ld
     _STACK_SIZE = 0x00010000;   /* 64 KB */
     _HEAP_SIZE  = 0x00400000;   /* 4  MB (or larger for big images) */
     ```
   - Save, **Clean**, and **Build**.

5. **Run via JTAG**
   - Connect UART (115200 8N1), Run → expect:
     ```
     SD BMP Loader (bare-metal)
     SD mounted: 0:/
     File size: 120054 bytes
     Loaded BMP: 200x200, 3 channels
     Top-left pixel RGB = (255,0,0)
     Done.
     ```

---

\
