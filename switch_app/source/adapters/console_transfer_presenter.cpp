#include "transfer_switch/adapters/console_transfer_presenter.hpp"

#include <algorithm>
#include <cstdio>

#include <switch.h>

namespace transfer_switch {

ConsoleTransferPresenter::ConsoleTransferPresenter()
    : transferred_bytes_(0),
      total_bytes_(0),
      last_percent_(101),
      succeeded_(0),
      failed_(0),
      cancelled_(0) {
}

void ConsoleTransferPresenter::setStorageNote(const char* note) {
    storage_note_ = note == nullptr ? "" : note;
}

std::string ConsoleTransferPresenter::fileName(const std::string& path) {
    const size_t separator = path.find_last_of('/');
    return separator == std::string::npos ? path : path.substr(separator + 1);
}

void ConsoleTransferPresenter::showWaiting() {
    active_path_.clear();
    transferred_bytes_ = 0;
    total_bytes_ = 0;
    last_percent_ = 101;
    render("Esperando archivos desde Windows");
}

void ConsoleTransferPresenter::transferStarted(const char* path, uint64_t total_bytes) {
    active_path_ = path == nullptr ? "" : path;
    transferred_bytes_ = 0;
    total_bytes_ = total_bytes;
    last_percent_ = 101;
    render("Recibiendo");
}

void ConsoleTransferPresenter::transferProgress(
    uint64_t transferred_bytes,
    uint64_t total_bytes
) {
    transferred_bytes_ = transferred_bytes;
    total_bytes_ = total_bytes;
    const unsigned int percent = total_bytes == 0
        ? 100
        : static_cast<unsigned int>((transferred_bytes * 100) / total_bytes);
    if (percent != last_percent_) {
        last_percent_ = percent;
        render("Recibiendo");
    }
}

void ConsoleTransferPresenter::transferFinished(
    const char* path,
    uint64_t transferred_bytes,
    uint64_t total_bytes,
    TransferOutcome outcome,
    const char* detail
) {
    transferred_bytes_ = transferred_bytes;
    total_bytes_ = total_bytes;
    if (outcome == TransferOutcome::succeeded) {
        ++succeeded_;
    } else if (outcome == TransferOutcome::cancelled) {
        ++cancelled_;
    } else {
        ++failed_;
    }
    recent_.push_back(ResultLine {
        fileName(path == nullptr ? std::string() : std::string(path)),
        outcome,
        detail == nullptr ? "" : detail,
    });
    if (recent_.size() > 6) {
        recent_.erase(recent_.begin());
    }
    active_path_.clear();
    render(outcome == TransferOutcome::succeeded ? "Archivo completado" : "Transferencia con error");
}

void ConsoleTransferPresenter::writeSummary(char* destination, size_t capacity) const {
    if (destination == nullptr || capacity == 0) {
        return;
    }
    std::snprintf(
        destination,
        capacity,
        "Sesion: %u correctos, %u fallidos, %u cancelados.",
        succeeded_,
        failed_,
        cancelled_
    );
}

void ConsoleTransferPresenter::render(const char* state) {
    consoleClear();
    std::printf("TRANSFERENCIA SWITCH - RECEPCION SEGURA\n");
    std::printf("=======================================\n\n");
    std::printf("Estado: %s\n", state);
    std::printf("Destino: switch/transferencia-switch/inbox\n\n");
    if (!storage_note_.empty()) {
        std::printf("Vistas: %s\n\n", storage_note_.c_str());
    }

    if (!active_path_.empty()) {
        const unsigned int percent = total_bytes_ == 0
            ? 100
            : static_cast<unsigned int>((transferred_bytes_ * 100) / total_bytes_);
        const std::string active_name = fileName(active_path_);
        std::printf("Archivo: %s\n", active_name.c_str());
        std::printf(
            "Progreso: %u%%  %.2f / %.2f MiB\n\n",
            std::min(percent, 100u),
            static_cast<double>(transferred_bytes_) / (1024.0 * 1024.0),
            static_cast<double>(total_bytes_) / (1024.0 * 1024.0)
        );
    }

    std::printf("Resumen: %u correctos | %u fallidos | %u cancelados\n", succeeded_, failed_, cancelled_);
    for (const ResultLine& result : recent_) {
        const char* marker = result.outcome == TransferOutcome::succeeded
            ? "OK"
            : (result.outcome == TransferOutcome::cancelled ? "CANCELADO" : "ERROR");
        std::printf("[%s] %s", marker, result.name.c_str());
        if (!result.detail.empty()) {
            std::printf(" - %s", result.detail.c_str());
        }
        std::printf("\n");
    }
    std::printf("\nB o +: cerrar MTP y volver al menu\n");
    consoleUpdate(nullptr);
}

}  // namespace transfer_switch
