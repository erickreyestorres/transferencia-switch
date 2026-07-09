#include "transfer_switch/ui/graphical_transfer_presenter.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <mutex>

namespace transfer_switch {

// â”€â”€â”€ Constantes de layout â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
// Pantalla 1280Ã—720
// Cabecera:    y=0..59
// Panel prog:  y=60..299
// Panel res:   y=300..659
// Pie:         y=660..719

static constexpr int kHeaderH   = 84;
static constexpr int kProgH     = 270;
static constexpr int kFooterH   = 76;
static constexpr int kMargin    = 24;
static constexpr int kAnimSize  = 190;
static constexpr int kIconSmall = 56;   // iconos de estado en la lista
static constexpr int kLineH     = 82;   // altura de cada fila de resultado

// â”€â”€â”€ Constructor â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
GraphicalTransferPresenter::GraphicalTransferPresenter(FbRenderer& renderer)
    : rdr_(renderer) {}

// â”€â”€â”€ ConfiguraciÃ³n â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
void GraphicalTransferPresenter::setStorageNote(const char* note) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    storage_note_ = note ? note : "";
}

void GraphicalTransferPresenter::showWaiting() {
    std::lock_guard<std::mutex> lock(state_mutex_);
    active_path_.clear();
    transferred_bytes_ = 0;
    total_bytes_       = 0;
    transfer_active_   = false;
}

// â”€â”€â”€ TransferObserver â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
void GraphicalTransferPresenter::transferStarted(
    const char* path, uint64_t total_bytes)
{
    std::lock_guard<std::mutex> lock(state_mutex_);
    active_path_       = path ? path : "";
    transferred_bytes_ = 0;
    total_bytes_       = total_bytes;
    transfer_active_   = true;
}

void GraphicalTransferPresenter::transferProgress(
    uint64_t transferred_bytes, uint64_t total_bytes)
{
    std::lock_guard<std::mutex> lock(state_mutex_);
    transferred_bytes_ = transferred_bytes;
    total_bytes_       = total_bytes;
}

void GraphicalTransferPresenter::transferFinished(
    const char* path,
    uint64_t transferred_bytes,
    uint64_t total_bytes,
    TransferOutcome outcome,
    const char* detail)
{
    std::lock_guard<std::mutex> lock(state_mutex_);
    transferred_bytes_ = transferred_bytes;
    total_bytes_       = total_bytes;
    transfer_active_   = false;
    active_path_.clear();

    switch (outcome) {
        case TransferOutcome::succeeded: ++succeeded_; break;
        case TransferOutcome::cancelled: ++cancelled_; break;
        default:                         ++failed_;    break;
    }

    // Extraer solo el nombre del archivo, no la ruta completa.
    std::string full = path ? path : "";
    const size_t sep = full.find_last_of('/');
    const std::string name = (sep == std::string::npos)
        ? full : full.substr(sep + 1);

    results_.push_back({name, detail ? detail : "", outcome});
}

// â”€â”€â”€ Resumen de sesiÃ³n â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
void GraphicalTransferPresenter::writeSummary(char* dst, size_t capacity) const {
    if (!dst || capacity == 0) return;
    std::lock_guard<std::mutex> lock(state_mutex_);
    const ResultLine* first_failure = nullptr;
    for (const ResultLine& result : results_) {
        if (result.outcome == TransferOutcome::failed) {
            first_failure = &result;
            break;
        }
    }
    if (first_failure) {
        std::snprintf(dst, capacity,
            "Sesion: %u OK, %u fallidos. Fallo: %.80s",
            succeeded_, failed_, first_failure->name.c_str());
    } else {
        std::snprintf(dst, capacity,
            "Sesion: %u correctos, %u fallidos, %u cancelados.",
            succeeded_, failed_, cancelled_);
    }
}

// â”€â”€â”€ AnimaciÃ³n â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
void GraphicalTransferPresenter::tickAnimation() {
    std::lock_guard<std::mutex> lock(state_mutex_);
    if (transfer_active_)
        anim_frame_ = (anim_frame_ + 1) % kFrameCount;
}

// â”€â”€â”€ Carga lazy de recursos â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
void GraphicalTransferPresenter::loadIcons() {
    if (icons_loaded_) return;
    icon_ok_     = FbRenderer::loadPng("romfs:/ui/icons/20_success.png");
    icon_fail_   = FbRenderer::loadPng("romfs:/ui/icons/21_failed.png");
    icon_cancel_ = FbRenderer::loadPng("romfs:/ui/icons/22_cancelled.png");
    icons_loaded_ = true;
}

void GraphicalTransferPresenter::loadAnimFrames() {
    if (anim_loaded_) return;
    char path[64];
    for (int i = 0; i < kFrameCount; ++i) {
        std::snprintf(path, sizeof(path), "romfs:/ui/animation/frame_%02d.png", i);
        anim_frames_[i] = FbRenderer::loadPng(path);
    }
    anim_loaded_ = true;
}

// â”€â”€â”€ Dibujo de cada secciÃ³n â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

void GraphicalTransferPresenter::drawHeader() {
    // Fondo de cabecera
    rdr_.fillRect(0, 0, FbRenderer::kScreenW, kHeaderH, Color::surface());
    // LÃ­nea separadora
    rdr_.fillRect(0, kHeaderH - 2, FbRenderer::kScreenW, 2, Color::border());
    // TÃ­tulo
    rdr_.drawText(kMargin, kHeaderH/2 - 14, "Transferencia Switch", Color::white(), 3);
    // VersiÃ³n alineada a la derecha
    rdr_.drawText(FbRenderer::kScreenW - kMargin - 6*8*2, kHeaderH/2 - 8,
        "v0.5.5", Color::grey(), 2);
}

void GraphicalTransferPresenter::drawAnimationFrame(int x, int y) {
    if (!anim_loaded_) return;
    const Image& frame = anim_frames_[anim_frame_];
    if (frame.valid()) {
        rdr_.drawImage(x, y, frame, kAnimSize, kAnimSize);
    } else {
        // Fallback: cuadrado teal pulsante
        rdr_.fillRoundedRect(x, y, kAnimSize, kAnimSize, 12, Color::teal());
    }
}

void GraphicalTransferPresenter::drawProgressPanel() {
    const int py   = kHeaderH + kMargin;
    const int ph   = kProgH - 2 * kMargin;
    const int left = kMargin;

    // Fondo del panel
    rdr_.fillRoundedRect(left, py,
        FbRenderer::kScreenW - 2*kMargin, ph, 14, Color::surface());
    rdr_.drawRoundedRect(left, py,
        FbRenderer::kScreenW - 2*kMargin, ph, 14, Color::border(), 2);

    const int inner_x = left + kMargin;
    const int inner_y = py  + kMargin;

    if (!transfer_active_) {
        // Estado de espera
        const Image& icon_wait = anim_frames_[0]; // frame 0 = icono estÃ¡tico
        if (anim_loaded_ && icon_wait.valid())
            rdr_.drawImage(inner_x, inner_y, icon_wait, kAnimSize, kAnimSize);
        rdr_.drawText(inner_x + kAnimSize + kMargin, inner_y + kAnimSize/2 - 16,
            "Esperando Windows...", Color::grey(), 3);
        return;
    }

    // AnimaciÃ³n a la izquierda
    drawAnimationFrame(inner_x, inner_y);

    // Info a la derecha de la animaciÃ³n
    const int tx = inner_x + kAnimSize + kMargin;
    int ty = inner_y;

    // Nombre del archivo (truncar si es muy largo)
    std::string name = active_path_;
    const size_t sep = name.find_last_of('/');
    if (sep != std::string::npos) name = name.substr(sep + 1);
    if (name.size() > 36) { name = name.substr(0, 33) + "..."; }

    rdr_.drawText(tx, ty, name.c_str(), Color::white(), 3);
    ty += 42;

    // Porcentaje y bytes
    const unsigned int pct = (total_bytes_ > 0)
        ? static_cast<unsigned int>((transferred_bytes_ * 100) / total_bytes_)
        : 0;
    const double transferred_mib = static_cast<double>(transferred_bytes_) / (1024.0*1024.0);
    const double total_mib       = static_cast<double>(total_bytes_)       / (1024.0*1024.0);

    char buf[64];
    std::snprintf(buf, sizeof(buf), "%u%%   %.1f / %.1f MiB",
        pct, transferred_mib, total_mib);
    rdr_.drawText(tx, ty, buf, Color::teal(), 2);
    ty += 36;

    // Barra de progreso
    const float fraction = (total_bytes_ > 0)
        ? static_cast<float>(transferred_bytes_) / static_cast<float>(total_bytes_)
        : 0.0f;
    const int bar_w = FbRenderer::kScreenW - tx - kMargin;
    rdr_.drawProgressBar(tx, ty, bar_w, 26, fraction);
}

void GraphicalTransferPresenter::drawResultsList() {
    const int list_y = kHeaderH + kProgH + kMargin;
    const int list_h = FbRenderer::kScreenH - kHeaderH - kProgH - kFooterH - kMargin;
    const int max_visible = list_h / kLineH;

    char hdr[64];
    if (failed_ > 0) {
        std::snprintf(hdr, sizeof(hdr),
            "Fallos: %u   OK: %u   Cancelados: %u",
            failed_, succeeded_, cancelled_);
        rdr_.drawText(kMargin, list_y, hdr, Color::red(), 2);
    } else {
        std::snprintf(hdr, sizeof(hdr),
            "Resumen: %u OK   %u fallidos   %u cancelados",
            succeeded_, failed_, cancelled_);
        rdr_.drawText(kMargin, list_y, hdr, Color::grey(), 2);
    }

    if (results_.empty()) return;

    std::vector<size_t> visible;
    visible.reserve(static_cast<size_t>(max_visible));
    if (failed_ > 0) {
        for (int i = static_cast<int>(results_.size()) - 1; i >= 0; --i) {
            if (results_[static_cast<size_t>(i)].outcome == TransferOutcome::failed) {
                visible.push_back(static_cast<size_t>(i));
                if (static_cast<int>(visible.size()) >= max_visible) break;
            }
        }
        std::reverse(visible.begin(), visible.end());
    } else {
        const int first = static_cast<int>(results_.size()) - max_visible;
        const int start = std::max(0, first);
        for (int i = start; i < static_cast<int>(results_.size()); ++i) {
            visible.push_back(static_cast<size_t>(i));
        }
    }

    for (int row = 0; row < static_cast<int>(visible.size()); ++row) {
        const size_t result_index = visible[static_cast<size_t>(row)];
        const ResultLine& r = results_[result_index];
        const int row_y = list_y + 34 + row * kLineH;

        const Color row_bg = (row % 2 == 0) ? Color::surface() : Color::background();
        rdr_.fillRect(kMargin, row_y, FbRenderer::kScreenW - 2*kMargin, kLineH - 2, row_bg);

        const Image* icon = nullptr;
        Color label_color = Color::grey();
        const char* marker = "?";
        if (r.outcome == TransferOutcome::succeeded) {
            icon = icons_loaded_ && icon_ok_.valid() ? &icon_ok_ : nullptr;
            label_color = Color::lime();
            marker = "OK";
        } else if (r.outcome == TransferOutcome::cancelled) {
            icon = icons_loaded_ && icon_cancel_.valid() ? &icon_cancel_ : nullptr;
            label_color = Color::yellow();
            marker = "~~";
        } else {
            icon = icons_loaded_ && icon_fail_.valid() ? &icon_fail_ : nullptr;
            label_color = Color::red();
            marker = "!!";
        }

        const int icon_x = kMargin + 4;
        const int text_x = icon_x + kIconSmall + 8;

        if (icon) {
            rdr_.drawImage(icon_x, row_y + (kLineH - kIconSmall)/2,
                *icon, kIconSmall, kIconSmall);
        } else {
            rdr_.drawText(icon_x, row_y + kLineH/2 - 8, marker, label_color, 2);
        }

        std::string name = r.name;
        if (name.size() > 64) name = name.substr(0, 61) + "...";
        rdr_.drawText(text_x, row_y + 6, name.c_str(), Color::white(), 2);

        if (!r.detail.empty()) {
            std::string detail = r.detail;
            if (detail.size() > 64) detail = detail.substr(0, 61) + "...";
            rdr_.drawText(text_x, row_y + 40, detail.c_str(), label_color, 2);
        }
    }
}
void GraphicalTransferPresenter::drawFooter() {
    const int fy = FbRenderer::kScreenH - kFooterH;
    rdr_.fillRect(0, fy, FbRenderer::kScreenW, 2, Color::border());
    rdr_.fillRect(0, fy + 2, FbRenderer::kScreenW, kFooterH - 2, Color::surface());

    if (!storage_note_.empty()) {
        std::string note = storage_note_;
        if (note.size() > 42) note = note.substr(0, 39) + "...";
        rdr_.drawText(kMargin, fy + kFooterH/2 - 8, note.c_str(), Color::dark_grey(), 2);
    }
    rdr_.drawText(FbRenderer::kScreenW - kMargin - 16*8*2,
        fy + kFooterH/2 - 8, "B/+ CERRAR MTP", Color::grey(), 2);
}

// â”€â”€â”€ Render principal â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
void GraphicalTransferPresenter::render() {
    loadIcons();
    loadAnimFrames();
    std::lock_guard<std::mutex> lock(state_mutex_);

    rdr_.begin();
    rdr_.clear();
    drawHeader();
    drawProgressPanel();
    drawResultsList();
    drawFooter();
    rdr_.end();
}

} // namespace transfer_switch

