
// sd_bmp_loader.cpp
// Bare-metal C++ app for AMD Xilinx Vitis (standalone A53).
// Reads a 24-bit BMP from SD (FatFs) and prints basic info.

#include "xil_printf.h"
#include "xil_cache.h"
#include "ff.h"              // xilffs (FatFs)
#include <cstdint>
#include <vector>
#include <string>

using namespace std;

// --------------------- SD helpers ---------------------
static FATFS fatfs;   // File system object
static FIL   file;    // File object

static bool sd_mount()
{
    // Mount drive 0 (root at "0:/")
    FRESULT rc = f_mount(&fatfs, "0:/", 1);
    if (rc != FR_OK)
    {
        xil_printf("f_mount failed: %d\r\n", rc);
        return false;
    }
    xil_printf("SD mounted: 0:/\r\n");
    return true;
}


static bool sd_read_all(const char* path, vector<uint8_t>& out)
{
    FRESULT rc = f_open(&file, path, FA_READ);
    if (rc != FR_OK) {
        xil_printf("f_open %s failed: %d\r\n", path, rc);
        return false;
    }

    const UINT fsize = f_size(&file);
    xil_printf("File size: %u bytes\r\n", fsize);   // <--- PUT IT HERE

    // (Optional) guard against bad_alloc on small heaps
    try {
        out.resize(fsize);
    } catch (const std::bad_alloc&) {
        xil_printf("bad_alloc: cannot allocate %u bytes\r\n", fsize);
        f_close(&file);
        return false;
    }

    UINT br = 0;
    rc = f_read(&file, out.data(), fsize, &br);
    f_close(&file);

    // xil_printf("Read %u bytes\r\n", br);            // Optional: confirm
    // if (rc != FR_OK || br != fsize) {
    //     xil_printf("f_read failed rc=%d br=%u size=%u\r\n", rc, br, fsize);
    //     return false;
    // }
    return true;
}


// --------------------- Image struct ---------------------
struct Image {
    int width = 0;
    int height = 0;
    int channels = 0;             // 3 for RGB
    vector<uint8_t> pixels;       // NHWC: rows*cols*channels (uint8)
};

// --------------------- BMP loader (24-bit, uncompressed) ---------------------
#pragma pack(push, 1)
struct BMPFileHeader {
    uint16_t bfType;      // 'BM' = 0x4D42
    uint32_t bfSize;
    uint16_t bfReserved1;
    uint16_t bfReserved2;
    uint32_t bfOffBits;
};
struct BMPInfoHeader {
    uint32_t biSize;          // 40 for BITMAPINFOHEADER
    int32_t  biWidth;
    int32_t  biHeight;        // >0 bottom-up; <0 top-down
    uint16_t biPlanes;        // 1
    uint16_t biBitCount;      // 24
    uint32_t biCompression;   // 0 (BI_RGB)
    uint32_t biSizeImage;     // may be 0 for BI_RGB
    int32_t  biXPelsPerMeter;
    int32_t  biYPelsPerMeter;
    uint32_t biClrUsed;
    uint32_t biClrImportant;
};
#pragma pack(pop)

static bool load_bmp_24(const vector<uint8_t>& buf, Image& img)
{
    if (buf.size() < sizeof(BMPFileHeader) + sizeof(BMPInfoHeader))
    {
        xil_printf("BMP: buffer too small\r\n");
        return false;
    }

    const BMPFileHeader* fh = reinterpret_cast<const BMPFileHeader*>(buf.data());
    if (fh->bfType != 0x4D42) // 'BM'
    {
        xil_printf("BMP: bad signature\r\n");
        return false;
    }

    const BMPInfoHeader* ih = reinterpret_cast<const BMPInfoHeader*>(buf.data() + sizeof(BMPFileHeader));
    if (ih->biSize != 40 || ih->biPlanes != 1 || ih->biBitCount != 24 || ih->biCompression != 0)
    {
        xil_printf("BMP: unsupported header (size=%lu planes=%u bpp=%u comp=%lu)\r\n",
                   (unsigned long)ih->biSize, ih->biPlanes, ih->biBitCount, (unsigned long)ih->biCompression);
        return false;
    }

    const int W = ih->biWidth;
    const int H = (ih->biHeight > 0) ? ih->biHeight : -ih->biHeight; // absolute height
    const bool bottom_up = (ih->biHeight > 0);

    const uint32_t pixel_offset = fh->bfOffBits;
    if (pixel_offset >= buf.size())
    {
        xil_printf("BMP: bad pixel offset\r\n");
        return false;
    }

    // Each row is padded to a 4-byte boundary in 24-bit BMP
    const uint32_t row_stride_in = ((W * 3 + 3) & ~3);

    if (pixel_offset + row_stride_in * H > buf.size())
    {
        xil_printf("BMP: truncated pixel data\r\n");
        return false;
    }

    img.width = W;
    img.height = H;
    img.channels = 3;
    img.pixels.resize(W * H * 3);

    const uint8_t* pixels_in = buf.data() + pixel_offset;

    // BMP stores pixels in BGR order. Convert to RGB (NHWC), and flip vertically if bottom-up.
    for (int y = 0; y < H; ++y)
    {
        const int src_y = bottom_up ? (H - 1 - y) : y;
        const uint8_t* row_in = pixels_in + src_y * row_stride_in;

        for (int x = 0; x < W; ++x)
        {
            const uint8_t b = row_in[x * 3 + 0];
            const uint8_t g = row_in[x * 3 + 1];
            const uint8_t r = row_in[x * 3 + 2];

            uint8_t* p = &img.pixels[(y * W + x) * 3];
            p[0] = r; p[1] = g; p[2] = b; // RGB
        }
    }

    return true;
}

// --------------------- main ---------------------
int main()
{
    xil_printf("SD BMP Loader (bare-metal)\r\n");

    if (!sd_mount())
    {
        xil_printf("Mount failed. Ensure SD is inserted and PS SD is enabled.\r\n");
        return -1;
    }

    vector<uint8_t> bmp_buf;
    const char* path = "0:/test.bmp";
    if (!sd_read_all(path, bmp_buf))
    {
        xil_printf("Failed to read %s\r\n", path);
        return -1;
    }

    Image img;
    if (!load_bmp_24(bmp_buf, img))
    {
        xil_printf("BMP parse failed\r\n");
        return -1;
    }

    xil_printf("Loaded BMP: %dx%d, %d channels\r\n", img.width, img.height, img.channels);

    // Example: print first pixel RGB
    if (!img.pixels.empty())
    {
        uint8_t r = img.pixels[0], g = img.pixels[1], b = img.pixels[2];
        xil_printf("Top-left pixel RGB = (%u,%u,%u)\r\n", r, g, b);
    }

    xil_printf("Done.\r\n");
    while (1) { /* idle */ }
    return 0;
}
