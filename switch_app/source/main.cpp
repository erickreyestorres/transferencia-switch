#include <atomic>
#include <cerrno>
#include <cstdio>
#include <iterator>
#include <limits>
#include <sys/stat.h>
#include <thread>

#include <switch.h>

#include "MtpServer.h"
#include "MtpStorage.h"
#include "USBMtpInterface.h"
#include "mtp.h"
#include "usb.h"
#include "transfer_switch/adapters/album_mounts.hpp"
#include "transfer_switch/adapters/bis_mounts.hpp"
#include "transfer_switch/adapters/installed_catalog.hpp"
#include "transfer_switch/adapters/read_only_sd_database.hpp"
#include "transfer_switch/adapters/save_mounts.hpp"
#include "transfer_switch/adapters/sd_install_receiver.hpp"
#include "transfer_switch/ui/fb_renderer.hpp"
#include "transfer_switch/ui/graphical_menu.hpp"
#include "transfer_switch/ui/graphical_transfer_presenter.hpp"

namespace {

constexpr android::MtpStorageID kSdStorageId = 0x00010001;
constexpr android::MtpStorageID kNandUserStorageId = 0x00020001;
constexpr android::MtpStorageID kNandSystemStorageId = 0x00030001;
constexpr android::MtpStorageID kInstalledStorageId = 0x00040001;
constexpr android::MtpStorageID kSdInstallStorageId = 0x00050001;
constexpr android::MtpStorageID kSavesStorageId = 0x00070001;
constexpr android::MtpStorageID kAlbumSdStorageId = 0x00080001;
constexpr android::MtpStorageID kAlbumNandStorageId = 0x00080002;
constexpr android::MtpStorageID kInboxStorageId = 0x000A0001;
constexpr uint64_t kInboxReserveBytes = 512ull * 1024ull * 1024ull;
constexpr const char* kInboxPath = "sdmc:/switch/transferencia-switch/inbox/";

char status_message[256] = "Listo para exponer la SD mediante MTP.";

// Renderer compartido — se inicializa en main y se pasa a los presenters.
transfer_switch::FbRenderer g_renderer;

// Tick de animación: ~80 ms = 12 fps, suficiente para el sprite SNES.
static constexpr uint64_t kAnimTickNs = 80'000'000ULL;

void stop_server_on_input(android::MtpServer* server) {
    PadState pad;
    padInitializeDefault(&pad);
    while (appletMainLoop()) {
        padUpdate(&pad);
        const u64 down = padGetButtonsDown(&pad);
        if ((down & HidNpadButton_B) != 0 || (down & HidNpadButton_Plus) != 0) {
            server->stop();
            return;
        }
        svcSleepThread(10'000'000);
    }
    server->stop();
}

bool ensure_directory(const char* path) {
    return mkdir(path, 0777) == 0 || errno == EEXIST;
}

bool prepare_inbox() {
    return ensure_directory("sdmc:/switch") &&
        ensure_directory("sdmc:/switch/transferencia-switch") &&
        ensure_directory("sdmc:/switch/transferencia-switch/inbox") &&
        ensure_directory("sdmc:/switch/transferencia-switch/inbox/.transferencia-switch-staging");
}

bool run_mtp(bool receive_mode, bool combined_mode = false) {
    const bool write_enabled = receive_mode || combined_mode;
    if (write_enabled && !prepare_inbox()) {
        std::snprintf(status_message, sizeof(status_message), "No se pudo preparar el Inbox seguro.");
        return false;
    }

    transfer_switch::AlbumMounts album_mounts;
    transfer_switch::BisMounts bis_mounts;
    transfer_switch::SaveMounts save_mounts;
    transfer_switch::InstalledCatalog installed_catalog;
    bool installed_catalog_available = false;
    if (combined_mode) {
        album_mounts.mountAvailable();
        bis_mounts.mountAvailable();
        save_mounts.mountAvailable();
        installed_catalog_available = installed_catalog.generate();
    }

    struct usb_device_descriptor device_descriptor = {
        .bLength = USB_DT_DEVICE_SIZE,
        .bDescriptorType = USB_DT_DEVICE,
        .bcdUSB = 0x0110,
        .bDeviceClass = 0x00,
        .bDeviceSubClass = 0x00,
        .bDeviceProtocol = 0x00,
        .bMaxPacketSize0 = 0x40,
        .idVendor = 0x057e,
        .idProduct = 0x4000,
        .bcdDevice = 0x0540,
        .bNumConfigurations = 0x01,
    };

    UsbInterfaceDesc interface_description {};
    USBMtpInterface mtp_interface(0, &interface_description);
    const Result usb_result = usbInitialize(&device_descriptor, 1, &interface_description);
    if (R_FAILED(usb_result)) {
        std::snprintf(
            status_message,
            sizeof(status_message),
            "No se pudo iniciar MTP: error 0x%08X.",
            usb_result
        );
        return false;
    }

    const char* initial_path = receive_mode && !combined_mode ? kInboxPath : "sdmc:/";
    transfer_switch::ReadOnlySdDatabase database(initial_path, receive_mode && !combined_mode);
    android::MtpStorage sd_storage(
        kSdStorageId,
        "sdmc:/",
        "1: SD Card",
        0,
        true,
        std::numeric_limits<uint64_t>::max(),
        false
    );
    android::MtpStorage inbox_storage(
        kInboxStorageId,
        kInboxPath,
        "10: Safe Inbox",
        kInboxReserveBytes,
        true,
        0xFFFFFFFEull,
        true
    );
    android::MtpStorage album_sd_storage(
        kAlbumSdStorageId,
        transfer_switch::AlbumMounts::sdPath(),
        "8: Album (SD)",
        0,
        true,
        std::numeric_limits<uint64_t>::max(),
        false
    );
    android::MtpStorage saves_storage(
        kSavesStorageId,
        "sdmc:/",
        "7: Saves",
        0,
        true,
        std::numeric_limits<uint64_t>::max(),
        false
    );
    android::MtpStorage installed_storage(
        kInstalledStorageId,
        transfer_switch::InstalledCatalog::rootPath(),
        "4: Installed games",
        0,
        true,
        std::numeric_limits<uint64_t>::max(),
        false
    );
    android::MtpStorage sd_install_storage(
        kSdInstallStorageId,
        "sdmc:/",
        "5: SD Card install",
        kInboxReserveBytes,
        true,
        0xFFFFFFFEull,
        true
    );
    android::MtpStorage nand_user_storage(
        kNandUserStorageId,
        transfer_switch::BisMounts::userPath(),
        "2: Nand USER",
        0,
        false,
        std::numeric_limits<uint64_t>::max(),
        false
    );
    android::MtpStorage nand_system_storage(
        kNandSystemStorageId,
        transfer_switch::BisMounts::systemPath(),
        "3: Nand SYSTEM",
        0,
        false,
        std::numeric_limits<uint64_t>::max(),
        false
    );
    android::MtpStorage album_nand_storage(
        kAlbumNandStorageId,
        transfer_switch::AlbumMounts::nandPath(),
        "8: Album (NAND)",
        0,
        false,
        std::numeric_limits<uint64_t>::max(),
        false
    );
    if (combined_mode || !receive_mode) {
        database.addStoragePath("sdmc:/", "1: SD Card", kSdStorageId, true);
    }
    if (combined_mode) {
        database.addActionStorage(kSdInstallStorageId, "sdmc:/");
        database.addStoragePathWithPolicy(
            kInboxPath, "10: Safe Inbox", kInboxStorageId, true, true);
        if (bis_mounts.userMounted()) {
            database.addStoragePath(
                transfer_switch::BisMounts::userPath(),
                "2: Nand USER",
                kNandUserStorageId,
                true
            );
        }
        if (bis_mounts.systemMounted()) {
            database.addStoragePath(
                transfer_switch::BisMounts::systemPath(),
                "3: Nand SYSTEM",
                kNandSystemStorageId,
                true
            );
        }
        if (album_mounts.sdMounted()) {
            database.addStoragePath(
                transfer_switch::AlbumMounts::sdPath(),
                "8: Album (SD)",
                kAlbumSdStorageId,
                true
            );
        }
        if (album_mounts.nandMounted()) {
            database.addStoragePath(
                transfer_switch::AlbumMounts::nandPath(),
                "8: Album (NAND)",
                kAlbumNandStorageId,
                true
            );
        }
        if (!save_mounts.mounted().empty()) {
            database.addVirtualStorage(kSavesStorageId);
            for (const transfer_switch::MountedSave& save : save_mounts.mounted()) {
                database.addVirtualRootDirectory(
                    kSavesStorageId,
                    save.display_name,
                    save.path
                );
            }
        }
        if (installed_catalog_available) {
            database.addStoragePath(
                transfer_switch::InstalledCatalog::rootPath(),
                "4: Installed games",
                kInstalledStorageId,
                true
            );
        }
    } else if (receive_mode) {
        database.addStoragePath(kInboxPath, "10: Safe Inbox", kInboxStorageId, true);
    }
    transfer_switch::GraphicalTransferPresenter presenter(g_renderer);
    transfer_switch::SdInstallReceiverFactory sd_install_receiver(kSdInstallStorageId);
    android::MtpServer server(
        &mtp_interface,
        &database,
        false,
        0,
        0644,
        0755,
        write_enabled,
        write_enabled ? &presenter : nullptr,
        combined_mode ? &sd_install_receiver : nullptr
    );
    if (combined_mode || !receive_mode) {
        server.addStorage(&sd_storage);
    }
    if (combined_mode || receive_mode) {
        server.addStorage(&inbox_storage);
    }
    if (bis_mounts.userMounted()) {
        server.addStorage(&nand_user_storage);
    }
    if (bis_mounts.systemMounted()) {
        server.addStorage(&nand_system_storage);
    }
    if (album_mounts.sdMounted()) {
        server.addStorage(&album_sd_storage);
    }
    if (album_mounts.nandMounted()) {
        server.addStorage(&album_nand_storage);
    }
    if (!save_mounts.mounted().empty()) {
        server.addStorage(&saves_storage);
    }
    if (installed_catalog_available) {
        server.addStorage(&installed_storage);
    }
    if (combined_mode) {
        server.addStorage(&sd_install_storage);
    }

    if (write_enabled) {
        char storage_note[600];
        if (combined_mode) {
            std::snprintf(
                storage_note,
                sizeof(storage_note),
                "SD + Inbox + Install SD (NSP/XCI) | USER: %s (0x%08X) | SYSTEM: %s (0x%08X)\nInstalled: %s, apps %u, fail %u (0x%08X) | Album SD: %s (0x%08X) | Album NAND: %s (0x%08X)\nSaves: %u/%u, fail %u, limite %u (0x%08X)",
                bis_mounts.userMounted() ? "OK" : "no disponible",
                bis_mounts.userResult(),
                bis_mounts.systemMounted() ? "OK" : "no disponible",
                bis_mounts.systemResult(),
                installed_catalog_available ? "OK" : "no disponible",
                static_cast<unsigned int>(installed_catalog.applicationCount()),
                static_cast<unsigned int>(installed_catalog.metadataFailureCount()),
                installed_catalog.result(),
                album_mounts.sdMounted() ? "OK" : "no disponible",
                album_mounts.sdResult(),
                album_mounts.nandMounted() ? "OK" : "no disponible",
                album_mounts.nandResult(),
                static_cast<unsigned int>(save_mounts.mounted().size()),
                static_cast<unsigned int>(save_mounts.detectedCount()),
                static_cast<unsigned int>(save_mounts.failedCount()),
                static_cast<unsigned int>(save_mounts.truncatedCount()),
                save_mounts.enumerationResult()
            );
        } else {
            std::snprintf(storage_note, sizeof(storage_note), "Inbox seguro");
        }
        presenter.setStorageNote(storage_note);
        presenter.showWaiting();
    } else {
        // Modo solo lectura: mostrar pantalla de espera gráfica también.
        // El presenter se pasa como nullptr al servidor pero lo usamos
        // localmente para mostrar el estado mientras el servidor corre.
        using transfer_switch::Color;
        using transfer_switch::FbRenderer;
        g_renderer.begin();
        g_renderer.clear();
        g_renderer.fillRect(0, 0, FbRenderer::kScreenW, 60, Color::surface());
        g_renderer.fillRect(0, 58, FbRenderer::kScreenW, 2, Color::border());
        g_renderer.drawText(24, 18, "Transferencia Switch", Color::white(), 2);
        g_renderer.drawText(24, 90, "MTP activo - SD Card (solo lectura)", Color::teal(), 2);
        g_renderer.drawText(24, 130, "Windows muestra: Transferencia Switch > 1: SD Card", Color::grey(), 1);
        g_renderer.drawText(24, 160, "B o +: cerrar MTP y volver al menu", Color::dark_grey(), 1);
        g_renderer.end();
    }

    // Hilo de render: actualiza la pantalla cada ~80 ms mientras el servidor corre.
    // El servidor MTP bloquea en su propio loop; necesitamos render en paralelo.
    std::atomic<bool> render_running{true};
    std::thread render_thread([&]() {
        uint64_t last_tick = armGetSystemTick();
        while (render_running.load()) {
            const uint64_t now = armGetSystemTick();
            if (armTicksToNs(now - last_tick) >= kAnimTickNs) {
                presenter.tickAnimation();
                last_tick = now;
            }
            if (write_enabled) presenter.render();
            svcSleepThread(16'000'000); // ~60 fps máximo
        }
    });

    std::thread input_thread(stop_server_on_input, &server);
    server.run();
    render_running.store(false);
    render_thread.join();
    input_thread.join();
    usbExit();
    album_mounts.unmount();
    bis_mounts.unmount();
    save_mounts.unmount();
    if (write_enabled) {
        presenter.writeSummary(status_message, sizeof(status_message));
    } else {
        std::snprintf(status_message, sizeof(status_message), "Sesion MTP finalizada correctamente.");
    }
    return true;
}

}  // namespace

int main() {
    // Configurar input antes de inicializar el renderer.
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    hidInitializeTouchScreen();

    // Inicializar renderer gráfico (romfs + framebuffer).
    // Si falla (e.g. fuera de applet), usamos consola de respaldo.
    const bool renderer_ok = g_renderer.initialize();
    if (!renderer_ok) {
        // Respaldo mínimo con consola libnx.
        consoleInit(nullptr);
        std::printf("Error: no se pudo inicializar el renderer grafico.\n");
        std::printf("Presiona + para salir.\n");
        PadState pad; padInitializeDefault(&pad);
        while (appletMainLoop()) {
            padUpdate(&pad);
            if (padGetButtonsDown(&pad) & HidNpadButton_Plus) break;
            consoleUpdate(nullptr);
        }
        consoleExit(nullptr);
        return 1;
    }

    // Construir el menú con tarjetas.
    transfer_switch::GraphicalMenu menu(g_renderer);

    // Tarjeta 0: MTP principal (todas las vistas)
    menu.addItem({
        "MTP principal",
        "romfs:/ui/icons/01_mtp_connect.png",
        transfer_switch::Color::teal(),
        true
    });
    // Tarjeta 1: Explorar SD (solo lectura)
    menu.addItem({
        "Explorar SD",
        "romfs:/ui/icons/02_sd_card.png",
        transfer_switch::Color::yellow(),
        true
    });
    // Tarjeta 2: Inbox seguro
    menu.addItem({
        "Safe Inbox",
        "romfs:/ui/icons/03_inbox.png",
        transfer_switch::Color::teal(),
        true
    });
    // Tarjeta 3: Diagnóstico
    menu.addItem({
        "Diagnostico",
        "romfs:/ui/icons/13_diagnostics.png",
        transfer_switch::Color::teal(),
        true
    });
    // Tarjeta 4: Salir
    menu.addItem({
        "Salir",
        "romfs:/ui/icons/15_exit.png",
        transfer_switch::Color::teal(),
        true
    });

    menu.setStatusMessage(status_message);

    while (appletMainLoop()) {
        // Comprobar + para salir (el menú también lo gestiona internamente
        // pero lo verificamos aquí también por seguridad).
        PadState pad; padInitializeDefault(&pad); padUpdate(&pad);
        if (padGetButtonsDown(&pad) & HidNpadButton_Plus) break;

        const int choice = menu.update();

        if (choice == 0) {
            run_mtp(false, true);
            menu.setStatusMessage(status_message);
        } else if (choice == 1) {
            run_mtp(false);
            menu.setStatusMessage(status_message);
        } else if (choice == 2) {
            run_mtp(true);
            menu.setStatusMessage(status_message);
        } else if (choice == 3) {
            std::snprintf(
                status_message, sizeof(status_message),
                "%s | libnx y SD disponibles.",
                appletGetAppletType() == AppletType_Application
                    ? "Modo aplicacion" : "Modo applet"
            );
            menu.setStatusMessage(status_message);
        } else if (choice == 4) {
            break;
        }
    }

    g_renderer.shutdown();
    return 0;
}
