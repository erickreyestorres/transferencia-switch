#ifndef TRANSFER_SWITCH_UI_FB_RENDERER_HPP
#define TRANSFER_SWITCH_UI_FB_RENDERER_HPP

/*
 * FbRenderer — renderizador de framebuffer directo para libnx (1280x720 RGBA8).
 *
 * Responsabilidades:
 *   - Gestionar el ciclo begin/end del framebuffer de libnx.
 *   - Dibujar primitivas (rectángulos sólidos, bordes, imágenes RGBA).
 *   - Renderizar texto usando la fuente de consola incorporada de libnx (8x8).
 *   - Cargar imágenes PNG desde romfs usando stb_image.
 *
 * Restricciones:
 *   - No depende de ningún otro módulo del proyecto.
 *   - Toda la escritura es sobre el buffer activo devuelto por framebufferBegin.
 *   - Las coordenadas son en píxeles, origen en la esquina superior izquierda.
 */

#include <cstdint>
#include <string>
#include <vector>

namespace transfer_switch {

// Colores de la paleta del proyecto (RGBA8, little-endian = ABGR en memoria).
// libnx framebuffer usa formato RGBA8888 con bytes en orden R,G,B,A.
struct Color {
    uint8_t r, g, b, a;

    constexpr Color(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255)
        : r(r), g(g), b(b), a(a) {}

    // Paleta del proyecto
    static constexpr Color background()   { return {0x17, 0x19, 0x23, 255}; } // #171923
    static constexpr Color surface()      { return {0x1E, 0x20, 0x2E, 255}; } // tarjeta base
    static constexpr Color surface_hi()   { return {0x2A, 0x2D, 0x40, 255}; } // tarjeta seleccionada
    static constexpr Color border()       { return {0x29, 0x26, 0x3A, 255}; } // #29263A
    static constexpr Color border_hi()    { return {0x35, 0xD7, 0xCE, 255}; } // teal seleccion
    static constexpr Color teal()         { return {0x35, 0xD7, 0xCE, 255}; } // #35D7CE
    static constexpr Color lime()         { return {0xB8, 0xE8, 0x3F, 255}; } // #B8E83F
    static constexpr Color yellow()       { return {0xFF, 0xD6, 0x5A, 255}; } // #FFD65A
    static constexpr Color purple()       { return {0x73, 0x53, 0xB6, 255}; } // #7353B6
    static constexpr Color pink()         { return {0xD2, 0x5C, 0xD9, 255}; } // #D25CD9
    static constexpr Color red()          { return {0xF1, 0x5A, 0x5A, 255}; } // #F15A5A
    static constexpr Color white()        { return {0xFF, 0xFF, 0xFF, 255}; }
    static constexpr Color grey()         { return {0xCF, 0xCF, 0xD7, 255}; } // #CFCFD7
    static constexpr Color dark_grey()    { return {0x60, 0x62, 0x70, 255}; }
    static constexpr Color transparent()  { return {0, 0, 0, 0}; }
};

// Imagen decodificada en RAM (RGBA8).
struct Image {
    std::vector<uint8_t> pixels; // tamaño = width * height * 4
    int width  = 0;
    int height = 0;

    bool valid() const { return width > 0 && height > 0 && !pixels.empty(); }
};

class FbRenderer {
public:
    static constexpr int kScreenW = 1280;
    static constexpr int kScreenH = 720;

    FbRenderer();
    ~FbRenderer();

    // No copiable ni movible — gestiona recursos de libnx.
    FbRenderer(const FbRenderer&) = delete;
    FbRenderer& operator=(const FbRenderer&) = delete;

    // Inicializa romfs y el framebuffer. Llamar una vez al inicio.
    // Devuelve false si algo falla (romfs no montado, etc.).
    bool initialize();
    void shutdown();

    // --- Ciclo de frame ---
    void begin();   // adquiere el buffer del framebuffer
    void end();     // libera y presenta

    // --- Primitivas ---
    void clear(Color c = Color::background());
    void fillRect(int x, int y, int w, int h, Color c);
    void drawRect(int x, int y, int w, int h, Color c, int thickness = 2);
    // Esquinas redondeadas simuladas recortando las 4 esquinas cuadradas.
    void fillRoundedRect(int x, int y, int w, int h, int radius, Color c);
    void drawRoundedRect(int x, int y, int w, int h, int radius, Color c, int thickness = 2);
    // Barra de progreso estilo proyecto (fondo oscuro, relleno teal, bolita lima).
    void drawProgressBar(int x, int y, int w, int h, float fraction);

    // --- Texto con fuente de consola libnx (8x8 px por carácter) ---
    // scale=1 → 8px, scale=2 → 16px, etc.
    void drawText(int x, int y, const char* text, Color c, int scale = 1);
    void drawText(int x, int y, const std::string& text, Color c, int scale = 1) {
        drawText(x, y, text.c_str(), c, scale);
    }
    // Centrado horizontal dentro de [x, x+w].
    void drawTextCentered(int x, int y, int w, const char* text, Color c, int scale = 1);

    // --- Imágenes ---
    // Carga un PNG desde romfs (e.g. "romfs:/ui/icons/02_sd_card.png").
    // Devuelve imagen inválida si falla.
    static Image loadPng(const char* romfsPath);

    // Dibuja una imagen decodificada con composición alpha sobre el framebuffer.
    // dst_w/dst_h == 0 → usar tamaño original.
    void drawImage(int x, int y, const Image& img, int dst_w = 0, int dst_h = 0);

    // Dibuja un pixel directamente (sin bounds check en release para velocidad).
    void putPixel(int x, int y, Color c);

private:
    uint8_t*     fb_     = nullptr;  // puntero al buffer activo
    uint32_t     fb_stride_ = 0;     // stride en bytes
    bool         initialized_ = false;
    bool         romfs_mounted_ = false;

    // Blend un pixel RGBA sobre el framebuffer (alpha compositing).
    void blendPixel(int x, int y, uint8_t r, uint8_t g, uint8_t b, uint8_t a);
};

} // namespace transfer_switch

#endif // TRANSFER_SWITCH_UI_FB_RENDERER_HPP
