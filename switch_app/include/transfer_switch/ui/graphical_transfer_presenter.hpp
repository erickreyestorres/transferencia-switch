#ifndef TRANSFER_SWITCH_UI_GRAPHICAL_TRANSFER_PRESENTER_HPP
#define TRANSFER_SWITCH_UI_GRAPHICAL_TRANSFER_PRESENTER_HPP

/*
 * GraphicalTransferPresenter
 *
 * Reemplaza a ConsoleTransferPresenter. Implementa TransferObserver y dibuja
 * sobre el framebuffer con FbRenderer:
 *
 *   - Panel de progreso: animación SNES + nombre de archivo + barra de progreso
 *     + velocidad + porcentaje.
 *   - Panel de resultados: lista de los últimos resultados con icono de estado,
 *     nombre del archivo y mensaje de error completo (el problema central que
 *     motivó el proyecto — DBI no mostraba cuál archivo falló).
 *   - Contador de sesión: correctos / fallidos / cancelados siempre visible.
 *   - Nota de storages activos.
 *
 * El llamador debe:
 *   1. Crear y compartir un FbRenderer inicializado.
 *   2. Llamar render() en cada iteración del loop principal.
 *   3. Llamar writeSummary() al cerrar la sesión MTP.
 */

#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

#include "transfer_switch/ports/transfer_observer.hpp"
#include "transfer_switch/domain/transfer_result_category.h"
#include "transfer_switch/ui/fb_renderer.hpp"

namespace transfer_switch {

class GraphicalTransferPresenter final : public TransferObserver {
public:
    explicit GraphicalTransferPresenter(FbRenderer& renderer);

    // Configura la nota de storages activos (se muestra en la cabecera).
    void setStorageNote(const char* note);

    // Pone la pantalla en estado "esperando conexión".
    void showWaiting();

    // --- TransferObserver ---
    void transferStarted(const char* path, uint64_t total_bytes) override;
    void transferProgress(uint64_t transferred_bytes, uint64_t total_bytes) override;
    void transferFinished(
        const char* path,
        uint64_t transferred_bytes,
        uint64_t total_bytes,
        TransferOutcome outcome,
        const char* detail
    ) override;

    // Escribe el resumen de sesión en dst (para el status_message de main.cpp).
    void writeSummary(char* dst, size_t capacity) const;

    // Llama a begin/render/end sobre el renderer. Llamar en cada frame.
    void render();

    // Avanza la animación. Llamar periódicamente (e.g. cada 80 ms).
    void tickAnimation();

private:
    struct ResultLine {
        std::string name;
        std::string detail;
        TsTransferResultCategory category;
        TransferOutcome outcome;
    };

    void drawHeader();
    void drawProgressPanel();
    void drawResultsList();
    void drawFooter();
    void drawAnimationFrame(int x, int y);

    void loadIcons();
    void loadAnimFrames();

    FbRenderer& rdr_;
    mutable std::mutex state_mutex_;

    // Estado de transferencia activa
    std::string  active_path_;
    uint64_t     transferred_bytes_ = 0;
    uint64_t     total_bytes_       = 0;
    bool         transfer_active_   = false;

    // Contadores de sesión
    unsigned int succeeded_  = 0;
    unsigned int failed_     = 0;
    unsigned int cancelled_  = 0;

    // Historial — guardamos TODOS para que ningún fallo se pierda.
    // El panel muestra los últimos N visibles con scroll futuro.
    std::vector<ResultLine> results_;

    // Animación
    int  anim_frame_ = 0;
    static constexpr int kFrameCount = 10;
    Image anim_frames_[kFrameCount];
    bool  anim_loaded_ = false;

    // Iconos de estado
    Image icon_ok_;
    Image icon_fail_;
    Image icon_cancel_;
    bool  icons_loaded_ = false;

    std::string storage_note_;
};

} // namespace transfer_switch

#endif // TRANSFER_SWITCH_UI_GRAPHICAL_TRANSFER_PRESENTER_HPP
