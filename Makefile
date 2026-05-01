# Falcon Standard Library Makefile
# Supports multi-package structure with independent builds and releases

.PHONY: all build test release clean help update-hashes

# Find all directories containing a falcon.yml (excluding root)
PKG_DIRS := $(shell find . -mindepth 2 -name "falcon.yml" -exec dirname {} \;)

# Compiler settings
CXX := g++
CXXFLAGS := -std=c++20 -O3 -fPIC -Wall -Wextra -Delements=items
INCLUDES := -I$(shell pwd)/include -I/opt/falcon/include
LDFLAGS := -L/opt/falcon/lib -lfalcon_core_cpp -lfmt -lhdf5_cpp -lhdf5

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
		 cpp_file=$$(ls *-wrapper.cpp 2>/dev/null) && \
		 if [ -n "$$cpp_file" ]; then \
		   so_file=$${cpp_file%.cpp}.so; \
		   $(CXX) $(CXXFLAGS) -shared -o $$so_file $$cpp_file $(INCLUDES) $(LDFLAGS) || exit 1; \
		   echo "  ✓ Created $$so_file"; \
		 fi); \
	done

update-hashes: build ## Update SHA-256 hashes in all falcon.yml files
	@for dir in $(PKG_DIRS); do \
		echo "🔒 Updating hashes for $$dir..."; \
		(cd $$dir && \
		 for so in $$(ls *.so 2>/dev/null); do \
		   hash=$$(sha256sum $$so | cut -d' ' -f1); \
		   if command -v yq &> /dev/null; then \
		     yq eval ".ffi.\"$$so\" = \"sha256:$$hash\"" -i falcon.yml; \
		   else \
		     sed -i "s|$$so:.*|$$so: sha256:$$hash|" falcon.yml; \
		   fi; \
		 done); \
	done

test: build ## Run tests for all packages
	@for dir in $(PKG_DIRS); do \
		if [ -d "$$dir/tests" ]; then \
			echo "🧪 Testing $$dir..."; \
			(cd $$dir/tests && falcon-test ./run_tests.fal --log-level info || exit 1); \
		fi; \
	done

release: update-hashes ## Create releases for all packages (as per testing/Makefile)
	@for dir in $(PKG_DIRS); do \
		echo "🚀 Releasing $$dir..."; \
		(cd $$dir && \
		 VERSION=$$(grep "version:" falcon.yml | cut -d' ' -f2) && \
		 PKG_NAME=$$(grep "name:" falcon.yml | cut -d' ' -f2) && \
		 TARBALL="$$PKG_NAME-$$VERSION.tar.gz" && \
		 tar -czvf $$TARBALL falcon.yml *.fal *.so README.md 2>/dev/null || true; \
		 echo "  ✓ Created $$TARBALL"); \
	done

clean: ## Remove build artifacts
	@for dir in $(PKG_DIRS); do \
		(cd $$dir && rm -f *.so *.tar.gz); \
	done
	@echo "✓ Clean complete"
