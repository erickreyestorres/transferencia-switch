#include "transfer_switch/domain/transfer_result_category.h"

#include <ctype.h>
#include <stddef.h>
#include <string.h>

static int contains_ci(const char *text, const char *needle) {
    if (text == NULL || needle == NULL || needle[0] == '\0') {
        return 0;
    }

    const size_t needle_len = strlen(needle);
    for (const char *cursor = text; *cursor != '\0'; ++cursor) {
        size_t index = 0;
        while (index < needle_len && cursor[index] != '\0') {
            const unsigned char left = (unsigned char)cursor[index];
            const unsigned char right = (unsigned char)needle[index];
            if (tolower(left) != tolower(right)) {
                break;
            }
            ++index;
        }
        if (index == needle_len) {
            return 1;
        }
    }
    return 0;
}

TsTransferResultCategory ts_transfer_result_category_from_detail(
    const char *detail,
    int succeeded,
    int cancelled
) {
    if (succeeded) {
        if (contains_ci(detail, "ya instalado") ||
            contains_ci(detail, "contenido existente")) {
            return TS_TRANSFER_CATEGORY_ALREADY_INSTALLED;
        }
        return TS_TRANSFER_CATEGORY_OK;
    }

    if (cancelled) {
        return TS_TRANSFER_CATEGORY_CANCELLED;
    }

    if (contains_ci(detail, "secure fuera del XCI") ||
        contains_ci(detail, "cabecera secure fuera del XCI") ||
        contains_ci(detail, "particion secure no encontrada")) {
        return TS_TRANSFER_CATEGORY_XCI_INVALID;
    }
    if (contains_ci(detail, "abrir CNMT")) {
        return TS_TRANSFER_CATEGORY_CNMT_OPEN_FAILED;
    }
    if (contains_ci(detail, "NCA chica") ||
        contains_ci(detail, "NCA pequena") ||
        contains_ci(detail, "NCA pequeña")) {
        return TS_TRANSFER_CATEGORY_XCI_NCA_TOO_SMALL;
    }
    if (contains_ci(detail, "desconect") ||
        contains_ci(detail, "suspend") ||
        contains_ci(detail, "timeout")) {
        return TS_TRANSFER_CATEGORY_CONNECTION_OR_SLEEP;
    }

    return TS_TRANSFER_CATEGORY_UNKNOWN_ERROR;
}

const char *ts_transfer_result_category_title(TsTransferResultCategory category) {
    switch (category) {
        case TS_TRANSFER_CATEGORY_OK:
            return "Instalado correctamente";
        case TS_TRANSFER_CATEGORY_ALREADY_INSTALLED:
            return "Juego ya instalado";
        case TS_TRANSFER_CATEGORY_XCI_INVALID:
            return "XCI invalido o incompleto";
        case TS_TRANSFER_CATEGORY_CNMT_OPEN_FAILED:
            return "No se pudo abrir CNMT";
        case TS_TRANSFER_CATEGORY_XCI_NCA_TOO_SMALL:
            return "NCA de XCI demasiado pequena";
        case TS_TRANSFER_CATEGORY_CANCELLED:
            return "Transferencia cancelada";
        case TS_TRANSFER_CATEGORY_CONNECTION_OR_SLEEP:
            return "Conexion perdida o suspension";
        case TS_TRANSFER_CATEGORY_UNKNOWN_ERROR:
        default:
            return "Fallo no clasificado";
    }
}

const char *ts_transfer_result_category_advice(TsTransferResultCategory category) {
    switch (category) {
        case TS_TRANSFER_CATEGORY_ALREADY_INSTALLED:
            return "El contenido ya existe y fue reutilizado.";
        case TS_TRANSFER_CATEGORY_XCI_INVALID:
            return "Verifica que el XCI no este corrupto, truncado o mal recortado.";
        case TS_TRANSFER_CATEGORY_CNMT_OPEN_FAILED:
            return "El metadato no abre; revisa archivo o compatibilidad.";
        case TS_TRANSFER_CATEGORY_XCI_NCA_TOO_SMALL:
            return "La NCA parece incompleta; revisa el origen del XCI.";
        case TS_TRANSFER_CATEGORY_CANCELLED:
            return "Operacion cancelada o cerrada antes de terminar.";
        case TS_TRANSFER_CATEGORY_CONNECTION_OR_SLEEP:
            return "Revisa cable/puerto y evita suspension durante copias largas.";
        case TS_TRANSFER_CATEGORY_UNKNOWN_ERROR:
            return "Conserva install.log para crear una regla especifica.";
        case TS_TRANSFER_CATEGORY_OK:
        default:
            return "";
    }
}
