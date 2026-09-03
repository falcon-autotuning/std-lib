# Falcon Standard Library Makefile
# Supports multi-package structure with independent builds and releases

.PHONY: all build test release clean help update-hashes

# Find all directories containing a falcon.yml (excluding root)
PKG_DIRS := $(shell find . -mindepth 2 -name "falcon.yml" -exec dirname {} \;)

# Compiler settings
CXX := clang++
CXXFLAGS := -std=c++20 -O3 -fPIC -Wall -Wextra -Delements=items
INCLUDES := -I$(shell pwd)/include -I/home/daniel/work/research/falcon/playground/falcon-dsl/vcpkg_installed/x64-linux-dynamic/include -I/home/daniel/work/research/falcon/playground/falcon-routine/vcpkg_installed/x64-linux-dynamic/include
LDFLAGS := -L/opt/falcon/lib -L/home/daniel/.falcon/opt/lib -L/home/daniel/work/research/falcon/playground/falcon-dsl/vcpkg_installed/x64-linux-dynamic/lib -lfalcon-core -lspdlog -lfmt -lhdf5_cpp -lhdf5

help: ## Show available targets
	@echo "Falcon Standard Library"
	@echo "========================"
	@echo "Available packages:"
	@for dir in $(PKG_DIRS); do echo "  - $$dir"; done
	@echo ""
	@echo "Targets:"
	@grep -E '^[a-zA-Z_-]+:.*?## ' $(MAKEFILE_LIST) | awk 'BEGIN {FS = ":.*?## "}; {printf "  %-20s %s\n", $$1, $$2}'

all: build ## Build all packages

build: ## Build all FFI wrappers
	@for dir in $(PKG_DIRS); do \
		echo "🔨 Building $$dir..."; \
		(cd $$dir && \
		 mkdir -p build && \
		 cpp_file=$$(ls *-wrapper.cpp 2>/dev/null) && \
		 if [ -n "$$cpp_file" ]; then \
		   so_file=build/$${cpp_file%.cpp}.so; \
		   $(CXX) $(CXXFLAGS) -shared -o $$so_file $$cpp_file $(INCLUDES) $(LDFLAGS) || exit 1; \
		   cp $$so_file . 2>/dev/null || true; \
		   echo "  ✓ Created $$so_file"; \
		 fi); \
	done

update-hashes: build ## Update SHA-256 hashes in all falcon.yml files
	@for dir in $(PKG_DIRS); do \
		echo "🔒 Updating hashes for $$dir..."; \
		python3 scripts/update_hashes.py $$dir; \
	done

test: build ## Run tests for all packages
	@for dir in $(PKG_DIRS); do \
		if [ -d "$$dir/tests" ]; then \
			echo "🧪 Testing $$dir..."; \
			(cd $$dir/tests && LD_LIBRARY_PATH=/opt/falcon/lib:/home/daniel/.falcon/opt/lib:$$LD_LIBRARY_PATH falcon-test ./run_tests.fal --log-level info || exit 1); \
		fi; \
	done

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
	@find . -type d -name "build" -exec rm -rf {} +
	@rm -rf dist
	@rm -f *.tar.gz
	@echo "✓ Clean complete"
