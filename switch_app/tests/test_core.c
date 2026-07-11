#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "transfer_switch/adapters/dbi_catalog.h"
#include "transfer_switch/application/list_remote_files.h"
#include "transfer_switch/application/plan_save_backups.h"
#include "transfer_switch/application/probe_remote_file.h"
#include "transfer_switch/domain/save_backup_plan.h"
#include "transfer_switch/domain/safe_path.h"
#include "transfer_switch/domain/transfer_result_category.h"

static int tests_run = 0;
static int tests_failed = 0;

#define CHECK(condition) do { \
    ++tests_run; \
    if (!(condition)) { \
        ++tests_failed; \
        fprintf(stderr, "Fallo en %s:%d: %s\n", __FILE__, __LINE__, #condition); \
    } \
} while (0)

static void put_u32(unsigned char *target, unsigned int value) {
    target[0] = (unsigned char)(value & 0xffu);
    target[1] = (unsigned char)((value >> 8) & 0xffu);
    target[2] = (unsigned char)((value >> 16) & 0xffu);
    target[3] = (unsigned char)((value >> 24) & 0xffu);
}

static void put_header(unsigned char *target, unsigned int type, unsigned int command, unsigned int size) {
    memcpy(target, "DBI0", 4);
    put_u32(target + 4, type);
    put_u32(target + 8, command);
    put_u32(target + 12, size);
}

typedef struct {
    const unsigned char *reads;
    size_t reads_size;
    size_t read_offset;
    unsigned char writes[256];
    size_t write_size;
    size_t maximum_chunk;
    int open_count;
    int close_count;
} FakeTransport;

static TsStatus fake_open(void *context) {
    FakeTransport *fake = context;
    ++fake->open_count;
    return TS_OK;
}

static void fake_close(void *context) {
    FakeTransport *fake = context;
    ++fake->close_count;
}

static TsStatus fake_read(void *context, void *buffer, size_t size, size_t *transferred) {
    FakeTransport *fake = context;
    size_t available = fake->reads_size - fake->read_offset;
    size_t amount = size < available ? size : available;
    if (fake->maximum_chunk > 0 && amount > fake->maximum_chunk) {
        amount = fake->maximum_chunk;
    }
    memcpy(buffer, fake->reads + fake->read_offset, amount);
    fake->read_offset += amount;
    *transferred = amount;
    return amount > 0 || size == 0 ? TS_OK : TS_TRANSPORT_ERROR;
}

static TsStatus fake_write(void *context, const void *buffer, size_t size, size_t *transferred) {
    FakeTransport *fake = context;
    size_t amount = size;
    if (fake->maximum_chunk > 0 && amount > fake->maximum_chunk) {
        amount = fake->maximum_chunk;
    }
    if (fake->write_size + amount > sizeof(fake->writes)) {
        return TS_TRANSPORT_ERROR;
    }
    memcpy(fake->writes + fake->write_size, buffer, amount);
    fake->write_size += amount;
    *transferred = amount;
    return TS_OK;
}

static void test_catalog_domain(void) {
    char payload[] = "base.nsp\nupdate.nsp\n";
    char name[32];
    TsFileCatalog catalog;
    CHECK(ts_catalog_parse(&catalog, payload, strlen(payload)) == TS_OK);
    CHECK(catalog.file_count == 2);
    CHECK(catalog.length == strlen(payload));
    CHECK(ts_catalog_copy_name(&catalog, 1, name, sizeof(name)) == TS_OK);
    CHECK(strcmp(name, "update.nsp") == 0);
    CHECK(ts_catalog_copy_name(&catalog, 2, name, sizeof(name)) == TS_INVALID_ARGUMENT);

    payload[4] = '\0';
    CHECK(ts_catalog_parse(&catalog, payload, sizeof(payload) - 1) == TS_INVALID_CATALOG);
}

typedef struct {
    const char *payload;
    TsStatus status;
} FakeCatalog;

static TsStatus fake_fetch(void *context, char *buffer, size_t capacity, size_t *received) {
    FakeCatalog *fake = context;
    const size_t length = strlen(fake->payload);
    if (fake->status != TS_OK) {
        return fake->status;
    }
    if (length >= capacity) {
        return TS_BUFFER_TOO_SMALL;
    }
    memcpy(buffer, fake->payload, length);
    *received = length;
    return TS_OK;
}

static void test_list_use_case(void) {
    FakeCatalog fake = {"uno.nsp\ndos.nsp\n", TS_OK};
    TsRemoteCatalogPort port = {&fake, fake_fetch};
    TsFileCatalog catalog;
    char buffer[64];
    CHECK(ts_list_remote_files(&port, buffer, sizeof(buffer), &catalog) == TS_OK);
    CHECK(catalog.file_count == 2);
    CHECK(buffer[catalog.length] == '\0');
}

static void test_dbi_adapter(void) {
    static const unsigned char payload[] = "base.nsp\nupdate.nsp\n";
    unsigned char reads[16 + sizeof(payload) - 1 + 16 + 16 + 1 + 16];
    char catalog_buffer[64];
    TsFileCatalog catalog;
    TsFileProbe probe;
    FakeTransport fake = {0};
    TsByteTransport transport;
    TsDbiCatalogAdapter adapter;
    TsRemoteCatalogPort port;
    TsRemoteFilePort file_port;
    size_t offset = 0;

    put_header(reads + offset, 1, 3, sizeof(payload) - 1);
    offset += 16;
    memcpy(reads + offset, payload, sizeof(payload) - 1);
    offset += sizeof(payload) - 1;
    put_header(reads + offset, 2, 2, 24);
    offset += 16;
    put_header(reads + offset, 1, 2, 1);
    offset += 16;
    reads[offset++] = 0x50;
    put_header(reads + offset, 1, 0, 0);
    fake.reads = reads;
    fake.reads_size = sizeof(reads);
    fake.maximum_chunk = 3;

    transport.context = &fake;
    transport.open = fake_open;
    transport.close = fake_close;
    transport.read = fake_read;
    transport.write = fake_write;
    ts_dbi_catalog_adapter_init(&adapter, &transport, &port);
    ts_dbi_file_adapter_init(&adapter, &file_port);

    CHECK(ts_list_remote_files(&port, catalog_buffer, sizeof(catalog_buffer), &catalog) == TS_OK);
    CHECK(catalog.file_count == 2);
    CHECK(fake.open_count == 1);
    CHECK(fake.close_count == 0);
    CHECK(ts_probe_remote_file(&file_port, &catalog, 0, &probe) == TS_OK);
    CHECK(strcmp(probe.name, "base.nsp") == 0);
    CHECK(probe.first_byte == 0x50);
    CHECK(ts_dbi_catalog_adapter_close(&adapter) == TS_OK);
    CHECK(fake.close_count == 1);
    CHECK(fake.write_size == 104);
    CHECK(memcmp(fake.writes, "DBI0", 4) == 0);
    CHECK(fake.writes[8] == 3);
    CHECK(fake.writes[16 + 4] == 2);
    CHECK(fake.writes[32 + 8] == 2);
    CHECK(fake.writes[48] == 1);
    CHECK(memcmp(fake.writes + 64, "base.nsp", 8) == 0);
    CHECK(fake.writes[72 + 4] == 2);
    CHECK(fake.writes[88 + 8] == 0);
}

static void test_dbi_adapter_rejects_bad_magic(void) {
    unsigned char reads[16] = {0};
    char buffer[16];
    size_t received = 0;
    FakeTransport fake = {0};
    TsByteTransport transport = {&fake, fake_open, fake_close, fake_read, fake_write};
    TsDbiCatalogAdapter adapter;
    TsRemoteCatalogPort port;

    memcpy(reads, "NOPE", 4);
    fake.reads = reads;
    fake.reads_size = sizeof(reads);
    ts_dbi_catalog_adapter_init(&adapter, &transport, &port);

    CHECK(port.fetch(port.context, buffer, sizeof(buffer), &received) == TS_PROTOCOL_ERROR);
    CHECK(fake.close_count == 1);
}

static void test_safe_destination_paths(void) {
    const char* root = "sdmc:/switch/transferencia-switch/inbox/";
    CHECK(ts_is_safe_direct_child(root, root, "sdmc:/switch/transferencia-switch/inbox/demo.txt"));
    CHECK(ts_is_safe_direct_child(
        root,
        "sdmc:/switch/transferencia-switch/inbox/carpeta",
        "sdmc:/switch/transferencia-switch/inbox/carpeta/demo.txt"
    ));
    CHECK(!ts_is_safe_direct_child(root, root, "sdmc:/switch/transferencia-switch/inbox/.."));
    CHECK(!ts_is_safe_direct_child(root, root, "sdmc:/switch/transferencia-switch/inbox/a/b.txt"));
    CHECK(!ts_is_safe_direct_child(root, root, "sdmc:/atmosphere/config.ini"));
    CHECK(!ts_is_safe_direct_child(root, root, "sdmc:/switch/transferencia-switch/inbox/a\\b.txt"));
    CHECK(!ts_is_safe_direct_child(root, root, "sdmc:/switch/transferencia-switch/inbox/C:archivo"));
}

static void test_save_backup_plan(void) {
    TsSaveBackupPlan plan;
    char manifest[TS_SAVE_BACKUP_MANIFEST_MAX];

    CHECK(ts_save_backup_plan_create(
        &plan,
        0x0100ABCDEF123456ull,
        0x1122334455667788ull,
        0x99AABBCCDDEEFF00ull,
        "2026-07-10"
    ) == TS_OK);
    CHECK(strcmp(plan.date, "2026-07-10") == 0);
    CHECK(strcmp(plan.folder_name, "0100ABCDEF123456_user_55667788") == 0);
    CHECK(strcmp(
        plan.backup_path,
        "sdmc:/switch/transferencia-switch/backups/saves/2026-07-10/0100ABCDEF123456_user_55667788"
    ) == 0);
    CHECK(strcmp(
        plan.files_path,
        "sdmc:/switch/transferencia-switch/backups/saves/2026-07-10/0100ABCDEF123456_user_55667788/files"
    ) == 0);
    CHECK(strcmp(
        plan.manifest_path,
        "sdmc:/switch/transferencia-switch/backups/saves/2026-07-10/0100ABCDEF123456_user_55667788/manifest.json"
    ) == 0);

    CHECK(ts_save_backup_manifest_json(&plan, manifest, sizeof(manifest)) == TS_OK);
    CHECK(strstr(manifest, "\"schema\": \"transferencia-switch.save-backup.v1\"") != NULL);
    CHECK(strstr(manifest, "\"application_id\": \"0100ABCDEF123456\"") != NULL);
    CHECK(strstr(manifest, "\"files_dir\": \"files\"") != NULL);

    CHECK(ts_save_backup_plan_create(&plan, 0, 1, 2, "2026-07-10") == TS_INVALID_ARGUMENT);
    CHECK(ts_save_backup_plan_create(&plan, 1, 1, 2, "20260710") == TS_INVALID_ARGUMENT);
    CHECK(ts_save_backup_manifest_json(&plan, manifest, 16) == TS_BUFFER_TOO_SMALL);
}

static void test_plan_save_backups(void) {
    TsSaveBackupCandidate candidates[2] = {
        {0x0100000000000001ull, 0x1111222233334444ull, 0xAAAABBBBCCCCDDDDull},
        {0x0100000000000002ull, 0x5555666677778888ull, 0xEEEEFFFF00001111ull},
    };
    TsSaveBackupPlan plans[2];
    TsSaveBackupPlanningSummary summary;

    CHECK(ts_plan_save_backups(
        candidates,
        2,
        "2026-07-10",
        plans,
        2,
        &summary
    ) == TS_OK);
    CHECK(summary.requested_count == 2);
    CHECK(summary.planned_count == 2);
    CHECK(summary.failed_count == 0);
    CHECK(summary.first_error == TS_OK);
    CHECK(strcmp(plans[0].folder_name, "0100000000000001_user_33334444") == 0);
    CHECK(strcmp(plans[1].folder_name, "0100000000000002_user_77778888") == 0);

    CHECK(ts_plan_save_backups(
        candidates,
        2,
        "2026-07-10",
        plans,
        1,
        &summary
    ) == TS_BUFFER_TOO_SMALL);
    CHECK(summary.requested_count == 2);
    CHECK(summary.planned_count == 1);
    CHECK(summary.failed_count == 1);
    CHECK(summary.first_error == TS_BUFFER_TOO_SMALL);

    candidates[0].application_id = 0;
    CHECK(ts_plan_save_backups(
        candidates,
        2,
        "2026-07-10",
        plans,
        2,
        &summary
    ) == TS_INVALID_ARGUMENT);
    CHECK(summary.planned_count == 1);
    CHECK(summary.failed_count == 1);
    CHECK(summary.first_error == TS_INVALID_ARGUMENT);
}

static void test_transfer_result_categories(void) {
    CHECK(ts_transfer_result_category_from_detail(
        "XCI ya instalado; contenido existente reutilizado",
        1,
        0
    ) == TS_TRANSFER_CATEGORY_ALREADY_INSTALLED);
    CHECK(ts_transfer_result_category_from_detail(
        "particion secure fuera del XCI",
        0,
        0
    ) == TS_TRANSFER_CATEGORY_XCI_INVALID);
    CHECK(ts_transfer_result_category_from_detail(
        "abrir CNMT /Application_0100.cn (0x0000D401)",
        0,
        0
    ) == TS_TRANSFER_CATEGORY_CNMT_OPEN_FAILED);
    CHECK(ts_transfer_result_category_from_detail(
        "XCI NCA chica: demo.nca",
        0,
        0
    ) == TS_TRANSFER_CATEGORY_XCI_NCA_TOO_SMALL);
    CHECK(ts_transfer_result_category_from_detail(
        "usuario cerro el modo MTP",
        0,
        1
    ) == TS_TRANSFER_CATEGORY_CANCELLED);
    CHECK(ts_transfer_result_category_from_detail(
        "dispositivo desconectado o suspendido",
        0,
        0
    ) == TS_TRANSFER_CATEGORY_CONNECTION_OR_SLEEP);
    CHECK(strcmp(
        ts_transfer_result_category_title(TS_TRANSFER_CATEGORY_XCI_INVALID),
        "XCI invalido o incompleto"
    ) == 0);
    CHECK(strlen(ts_transfer_result_category_advice(TS_TRANSFER_CATEGORY_UNKNOWN_ERROR)) > 0);
}

int main(void) {
    test_catalog_domain();
    test_list_use_case();
    test_dbi_adapter();
    test_dbi_adapter_rejects_bad_magic();
    test_safe_destination_paths();
    test_save_backup_plan();
    test_plan_save_backups();
    test_transfer_result_categories();

    if (tests_failed != 0) {
        fprintf(stderr, "%d de %d comprobaciones fallaron.\n", tests_failed, tests_run);
        return EXIT_FAILURE;
    }
    printf("Núcleo C: %d comprobaciones aprobadas.\n", tests_run);
    return EXIT_SUCCESS;
}
