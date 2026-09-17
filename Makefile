# Falcon Standard Library Makefile
# Supports multi-package structure with independent builds and releases

.PHONY: all build test release clean help update-hashes vcpkg-bootstrap

# Find all directories containing a falcon.yml (excluding root and vcpkg)
PKG_DIRS := $(shell find . -mindepth 2 -path "./vcpkg" -prune -o -name falcon.yml -exec dirname {} \;)

# Vcpkg settings
VCPKG_DIR ?= $(CURDIR)/vcpkg_installed/x64-linux-dynamic
PRESET ?= linux-gcc-release

# Compiler settings
CXX := clang++
CXXFLAGS := -std=c++20 -O3 -fPIC -Wall -Wextra -Delements=items
INCLUDES := -I$(VCPKG_DIR)/include
LDFLAGS := -L$(VCPKG_DIR)/lib -lfalcon-core -lfalcon-typing -lfalcon-routine -lfalcon-database -lfalcon-comms -lnats -lspdlog -lfmt -lhdf5_cpp -lhdf5 -lyaml-cpp


# Find all wrapper sources and target shared libraries
WRAPPER_SRCS := $(shell find . -mindepth 2 -path "./vcpkg*" -prune -o -name "*-wrapper.cpp" -print)
WRAPPER_SOS := $(foreach src,$(WRAPPER_SRCS),$(dir $(src))build/$(notdir $(src:%.cpp=%.so)))

# Find test directories
TEST_DIRS := $(shell find . -mindepth 2 -path "./vcpkg*" -prune -o -name "run_tests.fal" -exec dirname {} \;)
TEST_TARGETS := $(addprefix run-test-,$(TEST_DIRS))

# Build rule template for each wrapper .so
define WRAPPER_RULE
$(1): $(2)
	@mkdir -p $$(dir $$@)
	@echo "🔨 Building $(2)..."
	@$$(CXX) $$(CXXFLAGS) -shared -o $$@ $$< $$(INCLUDES) $$(LDFLAGS)
endef

$(foreach src,$(WRAPPER_SRCS),$(eval $(call WRAPPER_RULE,$(dir $(src))build/$(notdir $(src:%.cpp=%.so)),$(src))))

help: ## Show available targets
	@echo "Falcon Standard Library"
	@echo "========================"
	@echo "Available packages:"
	@for dir in $(PKG_DIRS); do echo "  - $$dir"; done
	@echo ""
	@echo "Targets:"
	@grep -E '^[a-zA-Z_-]+:.*?## ' $(MAKEFILE_LIST) | awk 'BEGIN {FS = ":.*?## "}; {printf "  %-20s %s\n", $$1, $$2}'

vcpkg-bootstrap:
	@echo "Bootstrapping vcpkg..."
	@cmake -P cmake/bootstrap/bootstrap-vcpkg.cmake

all: build ## Build all packages

build: vcpkg-bootstrap $(WRAPPER_SOS) ## Build all FFI wrappers in parallel

update-hashes: build ## Update SHA-256 hashes in all falcon.yml files
	@for dir in $(PKG_DIRS); do \
		echo "🔒 Updating hashes for $$dir..."; \
		python3 scripts/update_hashes.py $$dir; \
	done

$(TEST_TARGETS): run-test-%: build
	@echo "🧪 Testing $*..."
	@(cd $* && LD_LIBRARY_PATH=$(VCPKG_DIR)/lib:/opt/falcon/lib:$$LD_LIBRARY_PATH $(VCPKG_DIR)/bin/falcon-test ./run_tests.fal --log-level info || exit 1)

test: build $(TEST_TARGETS) ## Run tests for all packages

dist: build update-hashes ## Create a monolithic release tarball
	@VERSION=$$(grep "version:" falcon.yml | cut -d' ' -f2 | tr -d '"') && \
	 TARBALL="std-lib-$$VERSION.tar.gz" && \
	 mkdir -p dist && \
	 echo "📦 Creating monolithic release dist/$$TARBALL..." && \
	 tar -czf dist/$$TARBALL --exclude='.git*' --exclude="dist" --exclude='scripts' --exclude='Makefile' --exclude='dist' . && \
	 echo "  ✓ Created dist/$$TARBALL"

release: dist ## Create releases for all packages (monolithic and individual)
	@for dir in $(PKG_DIRS); do \
		echo "🚀 Releasing $$dir..."; \
		(cd $$dir && \
		 VERSION=$$(grep "version:" falcon.yml | cut -d' ' -f2 | tr -d '"') && \
		 PKG_NAME=$$(grep "name:" falcon.yml | cut -d' ' -f2 | tr -d '"') && \
		 TARBALL="$$PKG_NAME-$$VERSION.tar.gz" && \
		 mkdir -p build && \
		 tar -czvf build/$$TARBALL falcon.yml *.fal build/*.so README.md 2>/dev/null || true; \
		 echo "  ✓ Created build/$$TARBALL"); \
	done

clean: ## Remove build artifacts
	@echo "Cleaning up..."
	@find . -maxdepth 5 -type d -name "build" -not -path "*/vcpkg*" -exec rm -rf {} +
	@find . -maxdepth 5 -name "*-wrapper.so" -not -path "*/vcpkg*" -exec rm -f {} +
	@rm -rf dist
	@rm -f *.tar.gz
	@echo "✓ Clean complete"
