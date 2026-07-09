#ifndef TRANSFER_SWITCH_UI_GRAPHICAL_MENU_HPP
#define TRANSFER_SWITCH_UI_GRAPHICAL_MENU_HPP

/*
 * GraphicalMenu — menú principal con tarjetas cuadradas táctiles.
 *
 * Cada opción es una tarjeta de 200×200 px con:
 *   - Icono PNG 64×64 centrado
 *   - Punto de color (acento) en la esquina superior derecha
 *   - Etiqueta de texto centrada bajo el icono
 *   - Borde teal cuando está seleccionada
 *
 * Entrada:
 *   - Cruceta / palancas: navegar
 *   - A:                  seleccionar
 *   - Táctil:             toque dentro del bounding box de una tarjeta
 *
 * Devuelve el índice seleccionado a través de update() cuando el usuario
 * confirma con A o toca una tarjeta.
 */

#include <string>
#include <vector>

#include "transfer_switch/ui/fb_renderer.hpp"

namespace transfer_switch {

struct MenuItem {
    std::string label;       // texto bajo el icono
    std::string icon_path;   // ruta romfs (vacío = sin icono)
    Color       accent;      // color del punto de acento
    bool        enabled;     // falso = gris, no seleccionable
};

class GraphicalMenu {
public:
    explicit GraphicalMenu(FbRenderer& renderer);

    // Agrega un item al menú.
    void addItem(MenuItem item);

    // Muestra el estado actual (e.g. "Listo para exponer la SD mediante MTP.").
    void setStatusMessage(const char* msg);

    // Procesa input y dibuja. Devuelve el índice confirmado o -1 si nada.
    // Llamar en cada iteración del loop principal.
    int update();

private:
    struct CardLayout {
        int x, y, w, h;
    };

    void loadIcons();
    void render(int selected);
    void drawCard(const CardLayout& c, const MenuItem& item,
                  bool selected, int index);
    CardLayout cardLayout(int index) const;
    int hitTest(int touch_x, int touch_y) const;

    FbRenderer& rdr_;
    std::vector<MenuItem> items_;
    std::vector<Image>    icons_;
    bool  icons_loaded_ = false;
    int   selected_     = 0;
    std::string status_msg_;

    // Coordenadas fijas del grid de tarjetas
    static constexpr int kCardW   = 220;
    static constexpr int kCardH   = 220;
    static constexpr int kCardGap = 20;
    static constexpr int kGridY   = 100; // y de inicio del grid
};

} // namespace transfer_switch

#endif // TRANSFER_SWITCH_UI_GRAPHICAL_MENU_HPP
