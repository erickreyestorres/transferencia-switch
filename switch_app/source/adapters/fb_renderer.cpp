/*
 * fb_renderer.cpp
 * Renderizador de framebuffer directo para libnx 1280x720 RGBA8.
 *
 * Texto: usa la consola de libnx como superficie auxiliar —
 *   se inicializa una PrintConsole en un buffer temporal, se
 *   copia a nuestro framebuffer píxel a píxel con el color deseado.
 *   Esto evita embeber una fuente bitmap completa y reutiliza la
 *   fuente oficial de libnx que ya está en la imagen.
 *
 * Imágenes PNG: stb_image carga desde FILE* de romfs.
 */

// stb_image — implementación en esta unidad de compilación únicamente.
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
// Usamos FILE* estándar que libnx redirige a romfs sin problema.
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#endif
#include "stb_image.h"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#include "transfer_switch/ui/fb_renderer.hpp"

#include <algorithm>
#include <cstring>
#include <cstdio>
#include <cmath>

#include <switch.h>

namespace transfer_switch {

// ---------------------------------------------------------------------------
// Framebuffer global de libnx
// ---------------------------------------------------------------------------
static Framebuffer s_fb;
static bool        s_fb_ready = false;

// ---------------------------------------------------------------------------
// Constructor / destructor
// ---------------------------------------------------------------------------
FbRenderer::FbRenderer() = default;

FbRenderer::~FbRenderer() {
    shutdown();
}

// ---------------------------------------------------------------------------
// initialize / shutdown
// ---------------------------------------------------------------------------
bool FbRenderer::initialize() {
    if (initialized_) return true;

    // Montar romfs — ignora error si ya está montado por otra parte del app.
    Result rc = romfsInit();
    romfs_mounted_ = R_SUCCEEDED(rc);

    // Inicializar framebuffer libnx sobre la capa de applet por defecto.
    // viInitialize ya fue llamado por libnx runtime; usamos la capa existente.
    if (!s_fb_ready) {
        NWindow* win = nwindowGetDefault();
        rc = framebufferCreate(&s_fb, win, kScreenW, kScreenH, PIXEL_FORMAT_RGBA_8888, 2);
        if (R_FAILED(rc)) return false;
        framebufferMakeLinear(&s_fb);
        s_fb_ready = true;
    }

    initialized_ = true;
    return true;
}

void FbRenderer::shutdown() {
    if (!initialized_) return;
    if (s_fb_ready) {
        framebufferClose(&s_fb);
        s_fb_ready = false;
    }
    if (romfs_mounted_) {
        romfsExit();
        romfs_mounted_ = false;
    }
    initialized_ = false;
}

// ---------------------------------------------------------------------------
// Ciclo de frame
// ---------------------------------------------------------------------------
void FbRenderer::begin() {
    if (!s_fb_ready) return;
    fb_ = static_cast<uint8_t*>(
        framebufferBegin(&s_fb, reinterpret_cast<u32*>(&fb_stride_)));
}

void FbRenderer::end() {
    if (!s_fb_ready) return;
    framebufferEnd(&s_fb);
    fb_ = nullptr;
}

// ---------------------------------------------------------------------------
// Escritura de píxel — inline en header, definición aquí para el linker.
// fb stride es en bytes, formato RGBA8888.
// ---------------------------------------------------------------------------
void FbRenderer::putPixel(int x, int y, Color c) {
    if (!fb_ || x < 0 || y < 0 || x >= kScreenW || y >= kScreenH) return;
    uint8_t* p = fb_ + y * fb_stride_ + x * 4;
    p[0] = c.r; p[1] = c.g; p[2] = c.b; p[3] = c.a;
}

void FbRenderer::blendPixel(int x, int y, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    if (!fb_ || x < 0 || y < 0 || x >= kScreenW || y >= kScreenH) return;
    if (a == 255) { putPixel(x, y, {r, g, b, 255}); return; }
    if (a == 0)   return;
    uint8_t* p = fb_ + y * fb_stride_ + x * 4;
    const uint32_t inv = 255 - a;
    p[0] = static_cast<uint8_t>((r * a + p[0] * inv) / 255);
    p[1] = static_cast<uint8_t>((g * a + p[1] * inv) / 255);
    p[2] = static_cast<uint8_t>((b * a + p[2] * inv) / 255);
    p[3] = 255;
}

// ---------------------------------------------------------------------------
// Primitivas
// ---------------------------------------------------------------------------
void FbRenderer::clear(Color c) {
    if (!fb_) return;
    for (int y = 0; y < kScreenH; ++y) {
        uint8_t* row = fb_ + y * fb_stride_;
        for (int x = 0; x < kScreenW; ++x) {
            row[x * 4 + 0] = c.r;
            row[x * 4 + 1] = c.g;
            row[x * 4 + 2] = c.b;
            row[x * 4 + 3] = c.a;
        }
    }
}

void FbRenderer::fillRect(int x, int y, int w, int h, Color c) {
    if (!fb_) return;
    const int x1 = std::max(0, x);
    const int y1 = std::max(0, y);
    const int x2 = std::min(kScreenW, x + w);
    const int y2 = std::min(kScreenH, y + h);
    for (int py = y1; py < y2; ++py)
        for (int px = x1; px < x2; ++px)
            blendPixel(px, py, c.r, c.g, c.b, c.a);
}

void FbRenderer::drawRect(int x, int y, int w, int h, Color c, int t) {
    fillRect(x,       y,       w,  t,  c); // top
    fillRect(x,       y+h-t,   w,  t,  c); // bottom
    fillRect(x,       y,       t,  h,  c); // left
    fillRect(x+w-t,   y,       t,  h,  c); // right
}

void FbRenderer::fillRoundedRect(int x, int y, int w, int h, int r, Color c) {
    // Cuerpo central + brazos
    fillRect(x + r, y,     w - 2*r, h,     c);
    fillRect(x,     y + r, r,       h - 2*r, c);
    fillRect(x+w-r, y + r, r,       h - 2*r, c);
    // Esquinas: cuadrante de círculo rasterizado
    for (int dy = 0; dy < r; ++dy) {
        for (int dx = 0; dx < r; ++dx) {
            if (dx*dx + dy*dy <= r*r) {
                blendPixel(x + r - 1 - dx, y + r - 1 - dy, c.r, c.g, c.b, c.a); // TL
                blendPixel(x+w-r+dx,       y + r - 1 - dy, c.r, c.g, c.b, c.a); // TR
                blendPixel(x + r - 1 - dx, y+h-r+dy,       c.r, c.g, c.b, c.a); // BL
                blendPixel(x+w-r+dx,       y+h-r+dy,       c.r, c.g, c.b, c.a); // BR
            }
        }
    }
}

void FbRenderer::drawRoundedRect(int x, int y, int w, int h, int r, Color c, int t) {
    for (int i = 0; i < t; ++i)
        drawRect(x+i, y+i, w-2*i, h-2*i, c, 1);
    // Tapar esquinas cuadradas del drawRect con el color de fondo (transparente en realidad
    // solo rellenamos la esquina exterior con la misma curva)
    (void)r; // la curvatura se aplica visualmente por fillRoundedRect del fondo
}

void FbRenderer::drawProgressBar(int x, int y, int w, int h, float fraction) {
    const int r = h / 2;
    // Fondo
    fillRoundedRect(x, y, w, h, r, Color::border());
    // Relleno teal
    if (fraction > 0.0f) {
        const int fill_w = std::max(h, static_cast<int>(w * std::min(fraction, 1.0f)));
        fillRoundedRect(x, y, fill_w, h, r, Color::teal());
    }
    // Bolita lima en el extremo del relleno
    if (fraction > 0.0f && fraction < 1.0f) {
        const int cx = x + static_cast<int>(w * std::min(fraction, 1.0f));
        const int cy = y + h / 2;
        const int br = h / 2 + 2;
        for (int dy = -br; dy <= br; ++dy)
            for (int dx = -br; dx <= br; ++dx)
                if (dx*dx + dy*dy <= br*br)
                    blendPixel(cx+dx, cy+dy, 0xB8, 0xE8, 0x3F, 255);
    }
}

// ---------------------------------------------------------------------------
// Texto — fuente bitmap 8x8 embebida (ASCII 32-126).
// Generada con el subset necesario del font CP437 de dominio público.
// ---------------------------------------------------------------------------
// Solo incluimos los 95 caracteres imprimibles (0x20..0x7E).
static const uint8_t kFont[95][8] = {
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, // 0x20 espacio
    {0x18,0x3C,0x3C,0x18,0x18,0x00,0x18,0x00}, // !
    {0x36,0x36,0x00,0x00,0x00,0x00,0x00,0x00}, // "
    {0x36,0x36,0x7F,0x36,0x7F,0x36,0x36,0x00}, // #
    {0x0C,0x3E,0x03,0x1E,0x30,0x1F,0x0C,0x00}, // $
    {0x00,0x63,0x33,0x18,0x0C,0x66,0x63,0x00}, // %
    {0x1C,0x36,0x1C,0x6E,0x3B,0x33,0x6E,0x00}, // &
    {0x06,0x06,0x03,0x00,0x00,0x00,0x00,0x00}, // '
    {0x18,0x0C,0x06,0x06,0x06,0x0C,0x18,0x00}, // (
    {0x06,0x0C,0x18,0x18,0x18,0x0C,0x06,0x00}, // )
    {0x00,0x66,0x3C,0xFF,0x3C,0x66,0x00,0x00}, // *
    {0x00,0x0C,0x0C,0x3F,0x0C,0x0C,0x00,0x00}, // +
    {0x00,0x00,0x00,0x00,0x00,0x0C,0x0C,0x06}, // ,
    {0x00,0x00,0x00,0x3F,0x00,0x00,0x00,0x00}, // -
    {0x00,0x00,0x00,0x00,0x00,0x0C,0x0C,0x00}, // .
    {0x60,0x30,0x18,0x0C,0x06,0x03,0x01,0x00}, // /
    {0x3E,0x63,0x73,0x7B,0x6F,0x67,0x3E,0x00}, // 0
    {0x0C,0x0E,0x0C,0x0C,0x0C,0x0C,0x3F,0x00}, // 1
    {0x1E,0x33,0x30,0x1C,0x06,0x33,0x3F,0x00}, // 2
    {0x1E,0x33,0x30,0x1C,0x30,0x33,0x1E,0x00}, // 3
    {0x38,0x3C,0x36,0x33,0x7F,0x30,0x78,0x00}, // 4
    {0x3F,0x03,0x1F,0x30,0x30,0x33,0x1E,0x00}, // 5
    {0x1C,0x06,0x03,0x1F,0x33,0x33,0x1E,0x00}, // 6
    {0x3F,0x33,0x30,0x18,0x0C,0x0C,0x0C,0x00}, // 7
    {0x1E,0x33,0x33,0x1E,0x33,0x33,0x1E,0x00}, // 8
    {0x1E,0x33,0x33,0x3E,0x30,0x18,0x0E,0x00}, // 9
    {0x00,0x0C,0x0C,0x00,0x00,0x0C,0x0C,0x00}, // :
    {0x00,0x0C,0x0C,0x00,0x00,0x0C,0x0C,0x06}, // ;
    {0x18,0x0C,0x06,0x03,0x06,0x0C,0x18,0x00}, // <
    {0x00,0x00,0x3F,0x00,0x00,0x3F,0x00,0x00}, // =
    {0x06,0x0C,0x18,0x30,0x18,0x0C,0x06,0x00}, // >
    {0x1E,0x33,0x30,0x18,0x0C,0x00,0x0C,0x00}, // ?
    {0x3E,0x63,0x7B,0x7B,0x7B,0x03,0x1E,0x00}, // @
    {0x0C,0x1E,0x33,0x33,0x3F,0x33,0x33,0x00}, // A
    {0x3F,0x66,0x66,0x3E,0x66,0x66,0x3F,0x00}, // B
    {0x3C,0x66,0x03,0x03,0x03,0x66,0x3C,0x00}, // C
    {0x1F,0x36,0x66,0x66,0x66,0x36,0x1F,0x00}, // D
    {0x7F,0x46,0x16,0x1E,0x16,0x46,0x7F,0x00}, // E
    {0x7F,0x46,0x16,0x1E,0x16,0x06,0x0F,0x00}, // F
    {0x3C,0x66,0x03,0x03,0x73,0x66,0x7C,0x00}, // G
    {0x33,0x33,0x33,0x3F,0x33,0x33,0x33,0x00}, // H
    {0x1E,0x0C,0x0C,0x0C,0x0C,0x0C,0x1E,0x00}, // I
    {0x78,0x30,0x30,0x30,0x33,0x33,0x1E,0x00}, // J
    {0x67,0x66,0x36,0x1E,0x36,0x66,0x67,0x00}, // K
    {0x0F,0x06,0x06,0x06,0x46,0x66,0x7F,0x00}, // L
    {0x63,0x77,0x7F,0x7F,0x6B,0x63,0x63,0x00}, // M
    {0x63,0x67,0x6F,0x7B,0x73,0x63,0x63,0x00}, // N
    {0x1C,0x36,0x63,0x63,0x63,0x36,0x1C,0x00}, // O
    {0x3F,0x66,0x66,0x3E,0x06,0x06,0x0F,0x00}, // P
    {0x1E,0x33,0x33,0x33,0x3B,0x1E,0x38,0x00}, // Q
    {0x3F,0x66,0x66,0x3E,0x36,0x66,0x67,0x00}, // R
    {0x1E,0x33,0x07,0x0E,0x38,0x33,0x1E,0x00}, // S
    {0x3F,0x2D,0x0C,0x0C,0x0C,0x0C,0x1E,0x00}, // T
    {0x33,0x33,0x33,0x33,0x33,0x33,0x3F,0x00}, // U
    {0x33,0x33,0x33,0x33,0x33,0x1E,0x0C,0x00}, // V
    {0x63,0x63,0x63,0x6B,0x7F,0x77,0x63,0x00}, // W
    {0x63,0x63,0x36,0x1C,0x1C,0x36,0x63,0x00}, // X
    {0x33,0x33,0x33,0x1E,0x0C,0x0C,0x1E,0x00}, // Y
    {0x7F,0x63,0x31,0x18,0x4C,0x66,0x7F,0x00}, // Z
    {0x1E,0x06,0x06,0x06,0x06,0x06,0x1E,0x00}, // [
    {0x03,0x06,0x0C,0x18,0x30,0x60,0x40,0x00}, // backslash
    {0x1E,0x18,0x18,0x18,0x18,0x18,0x1E,0x00}, // ]
    {0x08,0x1C,0x36,0x63,0x00,0x00,0x00,0x00}, // ^
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xFF}, // _
    {0x0C,0x0C,0x18,0x00,0x00,0x00,0x00,0x00}, // `
    {0x00,0x00,0x1E,0x30,0x3E,0x33,0x6E,0x00}, // a
    {0x07,0x06,0x06,0x3E,0x66,0x66,0x3B,0x00}, // b
    {0x00,0x00,0x1E,0x33,0x03,0x33,0x1E,0x00}, // c
    {0x38,0x30,0x30,0x3e,0x33,0x33,0x6E,0x00}, // d
    {0x00,0x00,0x1E,0x33,0x3f,0x03,0x1E,0x00}, // e
    {0x1C,0x36,0x06,0x0f,0x06,0x06,0x0F,0x00}, // f
    {0x00,0x00,0x6E,0x33,0x33,0x3E,0x30,0x1F}, // g
    {0x07,0x06,0x36,0x6E,0x66,0x66,0x67,0x00}, // h
    {0x0C,0x00,0x0E,0x0C,0x0C,0x0C,0x1E,0x00}, // i
    {0x30,0x00,0x30,0x30,0x30,0x33,0x33,0x1E}, // j
    {0x07,0x06,0x66,0x36,0x1E,0x36,0x67,0x00}, // k
    {0x0E,0x0C,0x0C,0x0C,0x0C,0x0C,0x1E,0x00}, // l
    {0x00,0x00,0x33,0x7F,0x7F,0x6B,0x63,0x00}, // m
    {0x00,0x00,0x1F,0x33,0x33,0x33,0x33,0x00}, // n
    {0x00,0x00,0x1E,0x33,0x33,0x33,0x1E,0x00}, // o
    {0x00,0x00,0x3B,0x66,0x66,0x3E,0x06,0x0F}, // p
    {0x00,0x00,0x6E,0x33,0x33,0x3E,0x30,0x78}, // q
    {0x00,0x00,0x3B,0x6E,0x66,0x06,0x0F,0x00}, // r
    {0x00,0x00,0x3E,0x03,0x1E,0x30,0x1F,0x00}, // s
    {0x08,0x0C,0x3E,0x0C,0x0C,0x2C,0x18,0x00}, // t
    {0x00,0x00,0x33,0x33,0x33,0x33,0x6E,0x00}, // u
    {0x00,0x00,0x33,0x33,0x33,0x1E,0x0C,0x00}, // v
    {0x00,0x00,0x63,0x6B,0x7F,0x7F,0x36,0x00}, // w
    {0x00,0x00,0x63,0x36,0x1C,0x36,0x63,0x00}, // x
    {0x00,0x00,0x33,0x33,0x33,0x3E,0x30,0x1F}, // y
    {0x00,0x00,0x3F,0x19,0x0C,0x26,0x3F,0x00}, // z
    {0x38,0x0C,0x0C,0x07,0x0C,0x0C,0x38,0x00}, // {
    {0x18,0x18,0x18,0x00,0x18,0x18,0x18,0x00}, // |
    {0x07,0x0C,0x0C,0x38,0x0C,0x0C,0x07,0x00}, // }
    {0x6E,0x3B,0x00,0x00,0x00,0x00,0x00,0x00}, // ~
};

void FbRenderer::drawText(int x, int y, const char* text, Color c, int scale) {
    if (!fb_ || !text) return;
    int cx = x;
    for (const char* p = text; *p; ++p) {
        unsigned char ch = static_cast<unsigned char>(*p);
        if (ch == '\n') { cx = x; y += 8 * scale; continue; }
        if (ch < 0x20 || ch > 0x7E) { cx += 8 * scale; continue; }
        const uint8_t* glyph = kFont[ch - 0x20];
        for (int row = 0; row < 8; ++row) {
            uint8_t bits = glyph[row];
            for (int col = 0; col < 8; ++col) {
                // La tabla bitmap usada aquí codifica el bit menos significativo
                // como la columna izquierda. Leerla como MSB primero hacía que
                // cada glifo se viera espejado en hardware real.
                if (bits & (1u << col)) {
                    for (int sy = 0; sy < scale; ++sy)
                        for (int sx = 0; sx < scale; ++sx)
                            putPixel(cx + col*scale + sx, y + row*scale + sy, c);
                }
            }
        }
        cx += 8 * scale;
    }
}

void FbRenderer::drawTextCentered(int x, int y, int w, const char* text, Color c, int scale) {
    if (!text) return;
    int len = 0;
    for (const char* p = text; *p && *p != '\n'; ++p) ++len;
    const int text_w = len * 8 * scale;
    drawText(x + (w - text_w) / 2, y, text, c, scale);
}

// ---------------------------------------------------------------------------
// Imágenes PNG
// ---------------------------------------------------------------------------
Image FbRenderer::loadPng(const char* romfsPath) {
    Image img;
    FILE* f = fopen(romfsPath, "rb");
    if (!f) return img;

    // Leer el archivo completo en memoria.
    fseek(f, 0, SEEK_END);
    const long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (size <= 0) { fclose(f); return img; }

    std::vector<uint8_t> buf(static_cast<size_t>(size));
    fread(buf.data(), 1, buf.size(), f);
    fclose(f);

    int w = 0, h = 0, comp = 0;
    uint8_t* data = stbi_load_from_memory(
        buf.data(), static_cast<int>(buf.size()), &w, &h, &comp, 4);
    if (!data) return img;

    img.width  = w;
    img.height = h;
    img.pixels.assign(data, data + w * h * 4);
    stbi_image_free(data);
    return img;
}

void FbRenderer::drawImage(int x, int y, const Image& img, int dst_w, int dst_h) {
    if (!fb_ || !img.valid()) return;
    const int dw = (dst_w  > 0) ? dst_w  : img.width;
    const int dh = (dst_h > 0) ? dst_h : img.height;

    for (int py = 0; py < dh; ++py) {
        const int sy = (py * img.height) / dh;
        for (int px = 0; px < dw; ++px) {
            const int sx = (px * img.width) / dw;
            const uint8_t* src = img.pixels.data() + (sy * img.width + sx) * 4;
            blendPixel(x + px, y + py, src[0], src[1], src[2], src[3]);
        }
    }
}

} // namespace transfer_switch
