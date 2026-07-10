CC ?= cc
CFLAGS := -std=c11 -Wall -Wextra -Werror -pedantic -O2
INCLUDES := -I../include
SOURCES := \
	../source/domain/status.c \
	../source/domain/safe_path.c \
	../source/domain/save_backup_plan.c \
	../source/domain/catalog.c \
	../source/application/list_remote_files.c \
	../source/application/probe_remote_file.c \
	../source/adapters/dbi_catalog.c \
	test_core.c

.PHONY: all test clean
all: test_core

test_core: $(SOURCES)
	$(CC) $(CFLAGS) $(INCLUDES) $(SOURCES) -o $@

test: test_core
	./test_core

clean:
	rm -f test_core
