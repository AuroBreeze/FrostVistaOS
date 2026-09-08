# Rust kernel static library.
#
# Included after the architecture Makefile so that ARCH, BUILD_DIR,
# KERNEL_ELF and CFLAGS have already been configured.

RUST_LIB :=

ifeq ($(CONFIG_RUST),Y)

ifeq ($(ARCH),riscv)
	RUST_TARGET := riscv64imac-unknown-none-elf
	RUST_CODEGEN := ["-C","relocation-model=static","-C","code-model=medium"]
else ifeq ($(ARCH),loongarch)
	RUST_TARGET := loongarch64-unknown-none
	RUST_CODEGEN := ["-C","relocation-model=static","-C","code-model=medium","-C","target-feature=-lsx,-lasx"]
else
	$(error Rust support is not configured for ARCH=$(ARCH))
endif

ifeq ($(BUILD),debug)
	RUST_PROFILE_DIR := debug
	RUST_PROFILE_ARGS :=
else
	RUST_PROFILE_DIR := release
	RUST_PROFILE_ARGS := --release
endif

RUST_MANIFEST := rust/Cargo.toml
RUST_TARGET_DIR := $(BUILD_DIR)/rust
RUST_LIB := $(RUST_TARGET_DIR)/$(RUST_TARGET)/$(RUST_PROFILE_DIR)/libfrostvista_rust.a

CFLAGS += -DCONFIG_RUST

# Ask Cargo to check its dependency graph on every kernel build.
# Cargo recompiles only when Rust sources, flags or dependencies changed.
.PHONY: rust-force
rust-force:

$(RUST_LIB): rust-force
	$(CARGO) --config 'build.rustflags=$(RUST_CODEGEN)' build \
		--manifest-path $(RUST_MANIFEST) \
		--target $(RUST_TARGET) \
		--target-dir $(RUST_TARGET_DIR) \
		$(RUST_PROFILE_ARGS) \
		--locked

# Add the Rust archive to the existing kernel target's prerequisites.
$(KERNEL_ELF): $(RUST_LIB)

.PHONY: rust

rust: $(RUST_LIB)

endif
