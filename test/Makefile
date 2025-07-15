DIR_BUILD   := build
DIR_INCLUDE := -I../

SRC_FILES := $(wildcard *.c)
TARGETS   := $(SRC_FILES:.c=)
BINS      := $(addprefix $(DIR_BUILD)/, $(TARGETS))

CC       := gcc
FLAGS_C  += -Wall -Wextra
FLAGS_C  += -g
FLAGS_C  += -D_GNU_SOURCE
FLAGS_LD += -lpthread

ifeq ($(OS), Windows_NT)
	SHELL    := cmd.exe
	MKDIR    := md
	RMDIR    := rd /s /q
	FLAGS_LD += -lkernel32 -lws2_32
else
	MKDIR := mkdir -p
	RMDIR := rm -rf
endif

.PHONY: all clean $(TARGETS)

all: $(BINS)

$(DIR_BUILD):
	$(MKDIR) $(DIR_BUILD)

$(DIR_BUILD)/%: %.c | $(DIR_BUILD)
	$(CC) $(FLAGS_C) $(DIR_INCLUDE) $< -o $@ $(FLAGS_LD)
	@echo "[COMPILE] $< -> $@"

$(TARGETS): %: $(DIR_BUILD)/%

clean:
	$(RMDIR) $(DIR_BUILD)
	@echo "[CLEAN] Removed build directory"

help:
	@echo "Available targets:"
	@echo "  all     - Compile all .c files"
	@echo "  clean   - Clean all generated files"
	@echo "  help    - Show this help message"
	@echo ""
	@echo "Found .c files: $(SRC_FILES)"
	@echo "Targets: $(TARGETS)"
