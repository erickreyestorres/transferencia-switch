#include "transfer_switch/ui/graphical_menu.hpp"

#include <algorithm>
#include <cstring>
#include <switch.h>

namespace transfer_switch {

GraphicalMenu::GraphicalMenu(FbRenderer& renderer)
    : rdr_(renderer) {}

void GraphicalMenu::addItem(MenuItem item) {
    items_.push_back(std::move(item));
    icons_.push_back(Image{});     // placeholder; se carga lazy
}

void GraphicalMenu::setStatusMessage(const char* msg) {
    status_msg_ = msg ? msg : "";
}

// ─── Layout ──────────────────────────────────────────────────────────────────
GraphicalMenu::CardLayout GraphicalMenu::cardLayout(int index) const {
    const int n        = static_cast<int>(items_.size());
    // Centrar el grid horizontalmente
    const int total_w  = n * kCardW + (n - 1) * kCardGap;
    const int start_x  = (FbRenderer::kScreenW - total_w) / 2;
    return {
        start_x + index * (kCardW + kCardGap),
        kGridY,
        kCardW,
        kCardH
    };
}

int GraphicalMenu::hitTest(int tx, int ty) const {
    for (int i = 0; i < static_cast<int>(items_.size()); ++i) {
        if (!items_[static_cast<size_t>(i)].enabled) continue;
        const CardLayout c = cardLayout(i);
        if (tx >= c.x && tx < c.x + c.w &&
            ty >= c.y && ty < c.y + c.h)
            return i;
    }
    return -1;
}

// ─── Carga lazy ──────────────────────────────────────────────────────────────
void GraphicalMenu::loadIcons() {
    if (icons_loaded_) return;
    for (size_t i = 0; i < items_.size(); ++i) {
        if (!items_[i].icon_path.empty())
            icons_[i] = FbRenderer::loadPng(items_[i].icon_path.c_str());
    }
    icons_loaded_ = true;
}

// ─── Dibujado ─────────────────────────────────────────────────────────────────
void GraphicalMenu::drawCard(const CardLayout& c, const MenuItem& item,
                              bool selected, int index)
{
    const Color bg      = selected ? Color::surface_hi() : Color::surface();
    const Color border  = selected ? Color::border_hi()  : Color::border();
    const Color text_c  = item.enabled ? Color::white() : Color::dark_grey();

    // Cuerpo
    rdr_.fillRoundedRect(c.x, c.y, c.w, c.h, 16, bg);
    rdr_.drawRoundedRect(c.x, c.y, c.w, c.h, 16, border, selected ? 3 : 2);

    // Punto de acento — esquina superior derecha
    const int dot_r = 10;
    const int dot_x = c.x + c.w - dot_r - 12;
    const int dot_y = c.y + dot_r + 12;
    if (item.enabled) {
        for (int dy = -dot_r; dy <= dot_r; ++dy)
            for (int dx = -dot_r; dx <= dot_r; ++dx)
                if (dx*dx + dy*dy <= dot_r*dot_r)
                    rdr_.putPixel(dot_x + dx, dot_y + dy, item.accent);
    }

    // Icono centrado. En hardware real 64px se veía demasiado pequeño.
    constexpr int kIconSize = 112;
    const int icon_x = c.x + (c.w - kIconSize) / 2;
    const int icon_y = c.y + 28;

    const Image& icon = icons_[static_cast<size_t>(index)];
    if (icon.valid()) {
        rdr_.drawImage(icon_x, icon_y, icon, kIconSize, kIconSize);
    } else {
        // Fallback: cuadrado del color del acento
        rdr_.fillRoundedRect(icon_x, icon_y, kIconSize, kIconSize, 8,
            item.enabled ? item.accent : Color::dark_grey());
    }

    // Etiqueta centrada bajo el icono
    rdr_.drawTextCentered(c.x, icon_y + kIconSize + 14, c.w,
        item.label.c_str(), text_c, 2);

    // Si seleccionada: pequeña flecha/indicador abajo
    if (selected) {
        const int tri_cx = c.x + c.w / 2;
        const int tri_y  = c.y + c.h - 10;
        for (int i = 0; i < 6; ++i)
            rdr_.fillRect(tri_cx - i, tri_y + i, i*2 + 1, 1, Color::border_hi());
    }
}

void GraphicalMenu::render(int selected) {
    rdr_.begin();
    rdr_.clear();

    // Título
    rdr_.fillRect(0, 0, FbRenderer::kScreenW, 80, Color::surface());
    rdr_.fillRect(0, 78, FbRenderer::kScreenW, 2, Color::border());
    rdr_.drawTextCentered(0, 18, FbRenderer::kScreenW,
        "TRANSFERENCIA SWITCH", Color::white(), 3);

    // Tarjetas
    for (int i = 0; i < static_cast<int>(items_.size()); ++i)
        drawCard(cardLayout(i), items_[static_cast<size_t>(i)],
                 i == selected, i);

    // Mensaje de estado
    const int status_y = kGridY + kCardH + 34;
    rdr_.drawTextCentered(0, status_y, FbRenderer::kScreenW,
        status_msg_.c_str(), Color::grey(), 2);

    // Instrucciones
    const int help_y = FbRenderer::kScreenH - 40;
    rdr_.fillRect(0, help_y - 2, FbRenderer::kScreenW, 2, Color::border());
    rdr_.fillRect(0, help_y, FbRenderer::kScreenW, 40, Color::surface());
    rdr_.drawTextCentered(0, help_y + 8, FbRenderer::kScreenW,
        "D-PAD/STICK: MOVER   A: OK   +: SALIR", Color::grey(), 2);

    rdr_.end();
}

// ─── Update (input + dibujo) ──────────────────────────────────────────────────
int GraphicalMenu::update() {
    loadIcons();

    const int n = static_cast<int>(items_.size());

    // ── Input de pad ──
    static PadState pad;
    static bool pad_initialized = false;
    if (!pad_initialized) {
        padInitializeDefault(&pad);
        pad_initialized = true;
    }
    padUpdate(&pad);
    const u64 down = padGetButtonsDown(&pad);

    if (down & HidNpadButton_AnyUp) {
        // Buscar anterior habilitado
        for (int step = 1; step < n; ++step) {
            const int candidate = ((selected_ - step) % n + n) % n;
            if (items_[static_cast<size_t>(candidate)].enabled) {
                selected_ = candidate;
                break;
            }
        }
    }
    if (down & HidNpadButton_AnyDown) {
        for (int step = 1; step < n; ++step) {
            const int candidate = (selected_ + step) % n;
            if (items_[static_cast<size_t>(candidate)].enabled) {
                selected_ = candidate;
                break;
            }
        }
    }
    if (down & HidNpadButton_AnyLeft) {
        for (int step = 1; step < n; ++step) {
            const int candidate = ((selected_ - step) % n + n) % n;
            if (items_[static_cast<size_t>(candidate)].enabled) {
                selected_ = candidate;
                break;
            }
        }
    }
    if (down & HidNpadButton_AnyRight) {
        for (int step = 1; step < n; ++step) {
            const int candidate = (selected_ + step) % n;
            if (items_[static_cast<size_t>(candidate)].enabled) {
                selected_ = candidate;
                break;
            }
        }
    }

    int confirmed = -1;
    if ((down & HidNpadButton_A) &&
        items_[static_cast<size_t>(selected_)].enabled)
        confirmed = selected_;

    // ── Input táctil ──
    HidTouchScreenState touch {};
    hidGetTouchScreenStates(&touch, 1);
    if (touch.count > 0) {
        const int tx = static_cast<int>(touch.touches[0].x);
        const int ty = static_cast<int>(touch.touches[0].y);
        const int hit = hitTest(tx, ty);
        if (hit >= 0) {
            selected_ = hit;
            // Doble toque = confirmar; primer toque = solo seleccionar.
            // Implementación simple: un toque selecciona Y confirma.
            confirmed = hit;
        }
    }

    render(selected_);
    return confirmed;
}

} // namespace transfer_switch
