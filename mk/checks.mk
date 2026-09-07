# Developer checks and generated tooling metadata.
#
# Consumes:
#   FORMAT_SRC, KERNEL_C, ARCH_C, CURDIR, CC, CFLAGS, ARCH, GEN_DIR
#
# Produces targets:
#   lint, format, compdb, tidy, tidy-file

# Check if all source files comply with .clang-format (for CI)
lint:
	@echo "Checking code style..."
# 	@clang-format --dry-run -Werror $(FORMAT_SRC) && echo "All files formatted correctly."
#
# # Reformat all source files in-place
format:
	@echo "Formatting source files..."
	@clang-format -i $(FORMAT_SRC)
	@echo "Formatting done."

# Generate compile_commands.json for clang-tidy / clangd
ifeq ($(OS),Windows_NT)
  PYTHON ?= python
else
  PYTHON ?= python3
endif

compdb:
	@$(PYTHON) scripts/generate_compile_commands.py \
		--output=compile_commands.json \
		--directory="$(CURDIR)" \
		--arch="$(ARCH)" \
		--generated-include="$(GEN_DIR)" \
		--compiler="$(CC)" \
		--flags="$(CFLAGS)" \
		--sources $(KERNEL_C) $(ARCH_C)

# Run clang-tidy on all kernel source files
tidy: compdb
	@echo "Running clang-tidy..."
	@clang-tidy -p compile_commands.json $(KERNEL_C) $(ARCH_C) 2>&1
	@echo "Tidy done."

# Run clang-tidy on a single file:  make tidy-file FILE=kernel/fs/block_cache.c
tidy-file: compdb
	@clang-tidy -p compile_commands.json $(FILE)
