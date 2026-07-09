#include <stdio.h>
#include <switch.h>

#include "transfer_switch/adapters/dbi_catalog.h"
#include "transfer_switch/adapters/libnx_usb_transport.h"
#include "transfer_switch/application/list_remote_files.h"
#include "transfer_switch/application/probe_remote_file.h"

#define CATALOG_CAPACITY (64u * 1024u)

static const char *items[] = {
    "Diagnóstico del entorno",
    "Conectar al PC y obtener lista",
    "Probar lectura del primer archivo",
    "Salir",
};

static char catalog_buffer[CATALOG_CAPACITY];
static char message[256] = "Entorno libnx iniciado correctamente.";
static TsFileCatalog current_catalog;
static bool catalog_loaded = false;
static TsLibnxUsbTransport usb_adapter;
static TsByteTransport transport;
static TsDbiCatalogAdapter catalog_adapter;
static TsRemoteCatalogPort catalog_port;
static TsRemoteFilePort file_port;

static void draw_menu(int selected) {
    consoleClear();
    printf("TRANSFERENCIA SWITCH - PROTOTIPO SEGURO\n");
    printf("======================================\n\n");
    printf("Esta versión no instala, elimina ni escribe en la SD.\n\n");
    for (unsigned int index = 0; index < sizeof(items) / sizeof(items[0]); ++index) {
        printf("%s %s\n", index == (unsigned int)selected ? ">" : " ", items[index]);
    }
    printf("\n%s\n", message);
    printf("\nArriba/Abajo: navegar  A: seleccionar  +: salir\n");
}

static void list_files_from_pc(void) {
    TsFileCatalog catalog;
    TsStatus status;

    snprintf(message, sizeof(message), "Esperando el programa del PC por USB...");
    draw_menu(1);
    consoleUpdate(NULL);

    status = ts_list_remote_files(
        &catalog_port,
        catalog_buffer,
        sizeof(catalog_buffer),
        &catalog
    );

    if (status == TS_OK) {
        current_catalog = catalog;
        catalog_loaded = true;
        snprintf(
            message,
            sizeof(message),
            "Conexión correcta: %zu archivo(s) disponibles en el PC.",
            catalog.file_count
        );
    } else {
        snprintf(message, sizeof(message), "No se pudo obtener la lista: %s.", ts_status_message(status));
    }
}

static void probe_first_file(void) {
    TsFileProbe probe;
    TsStatus status;

    if (!catalog_loaded || current_catalog.file_count == 0) {
        snprintf(message, sizeof(message), "Primero obtén una lista de archivos desde el PC.");
        return;
    }

    snprintf(message, sizeof(message), "Solicitando el primer byte del archivo...");
    draw_menu(2);
    consoleUpdate(NULL);
    status = ts_probe_remote_file(&file_port, &current_catalog, 0, &probe);
    if (status == TS_OK) {
        snprintf(
            message,
            sizeof(message),
            "Rango recibido: %.180s comienza con 0x%02X.",
            probe.name,
            (unsigned int)probe.first_byte
        );
    } else {
        snprintf(message, sizeof(message), "Falló la lectura de prueba: %s.", ts_status_message(status));
    }
}

int legacy_backend_probe_main(void) {
    int selected = 0;
    const int item_count = (int)(sizeof(items) / sizeof(items[0]));

    consoleInit(NULL);
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);

    PadState pad;
    padInitializeDefault(&pad);
    ts_libnx_usb_transport_init(&usb_adapter, &transport);
    ts_dbi_catalog_adapter_init(&catalog_adapter, &transport, &catalog_port);
    ts_dbi_file_adapter_init(&catalog_adapter, &file_port);

    while (appletMainLoop()) {
        padUpdate(&pad);
        const u64 down = padGetButtonsDown(&pad);
        if ((down & HidNpadButton_Plus) != 0) {
            break;
        }
        if ((down & HidNpadButton_Up) != 0) {
            selected = (selected + item_count - 1) % item_count;
        }
        if ((down & HidNpadButton_Down) != 0) {
            selected = (selected + 1) % item_count;
        }
        if ((down & HidNpadButton_A) != 0) {
            if (selected == 0) {
                snprintf(
                    message,
                    sizeof(message),
                    "%s detectado; libnx funciona correctamente.",
                    appletGetAppletType() == AppletType_Application ? "Modo aplicación" : "Modo applet"
                );
            } else if (selected == 1) {
                list_files_from_pc();
            } else if (selected == 2) {
                probe_first_file();
            } else {
                break;
            }
        }
        draw_menu(selected);
        consoleUpdate(NULL);
    }

    ts_dbi_catalog_adapter_close(&catalog_adapter);
    consoleExit(NULL);
    return 0;
}
