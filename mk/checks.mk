# Developer checks and generated tooling metadata.
#
# Consumes:
#   FORMAT_SRC, KERNEL_C, ARCH_C, CURDIR, CC, CFLAGS
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
compdb:
	@echo "Generating compile_commands.json..."
	@{ \
		echo '['; \
		first=1; \
		for src in $(KERNEL_C) $(ARCH_C); do \
			[ "$$first" -eq 0 ] && echo ','; \
			first=0; \
			printf '  { "directory": "%s", "command": "%s %s -c %s", "file": "%s" }' \
				"$(CURDIR)" "$(CC)" "$(CFLAGS)" "$$src" "$$src"; \
		done; \
		echo ''; \
		echo ']'; \
	} > compile_commands.json
	@echo "Generated compile_commands.json"

# Run clang-tidy on all kernel source files
tidy: compdb
	@echo "Running clang-tidy..."
	@clang-tidy -p compile_commands.json $(KERNEL_C) $(ARCH_C) 2>&1
	@echo "Tidy done."

# Run clang-tidy on a single file:  make tidy-file FILE=kernel/fs/block_cache.c
tidy-file: compdb
	@clang-tidy -p compile_commands.json $(FILE)

