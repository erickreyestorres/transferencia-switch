#include "transfer_switch/domain/status.h"

const char *ts_status_message(TsStatus status) {
    switch (status) {
        case TS_OK:
            return "operación completada";
        case TS_INVALID_ARGUMENT:
            return "argumento inválido";
        case TS_INVALID_CATALOG:
            return "lista de archivos inválida";
        case TS_BUFFER_TOO_SMALL:
            return "la lista supera el límite disponible";
        case TS_TRANSPORT_ERROR:
            return "error de comunicación USB";
        case TS_PROTOCOL_ERROR:
            return "respuesta incompatible del programa del PC";
        default:
            return "error desconocido";
    }
}
