# MongoDB TUI Client Makefile
# Supports Windows/MSYS environment

# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -Isrc
DEBUGFLAGS = -g -O0 -DDEBUG
RELEASEFLAGS = -O2 -DNDEBUG

# Detect pkg-config
PKG_CONFIG = pkg-config

# Check for libmongoc-1.0
MONGOC_CFLAGS := $(shell $(PKG_CONFIG) --cflags libmongoc-1.0 2>/dev/null)
MONGOC_LIBS := $(shell $(PKG_CONFIG) --libs libmongoc-1.0 2>/dev/null)

# Check for ncurses (try different variants for Windows/MSYS)
NCURSES_CFLAGS := $(shell $(PKG_CONFIG) --cflags ncursesw 2>/dev/null || $(PKG_CONFIG) --cflags ncurses 2>/dev/null)
NCURSES_LIBS := $(shell $(PKG_CONFIG) --libs ncursesw 2>/dev/null || $(PKG_CONFIG) --libs ncurses 2>/dev/null || echo "-lncurses")

# Combine flags
INCLUDES = $(MONGOC_CFLAGS) $(NCURSES_CFLAGS)
LIBS = $(MONGOC_LIBS) $(NCURSES_LIBS)

# Directories
SRCDIR = src
OBJDIR = obj

# Source files
SOURCES = $(wildcard $(SRCDIR)/*.c)
OBJECTS = $(patsubst $(SRCDIR)/%.c,$(OBJDIR)/%.o,$(SOURCES))

# Target executable
TARGET = mongodb-tui

# Default target
all: check-deps release

# Check dependencies
check-deps:
	@echo "Checking dependencies..."
	@$(PKG_CONFIG) --exists libmongoc-1.0 || (echo "ERROR: libmongoc-1.0 not found. Install mongo-c-driver." && exit 1)
	@echo "  - libmongoc-1.0: OK"
	@$(PKG_CONFIG) --exists ncursesw || $(PKG_CONFIG) --exists ncurses || (echo "WARNING: ncurses not found via pkg-config, using fallback -lncurses")
	@echo "  - ncurses: OK"
	@echo "Dependencies satisfied."

# Debug build
debug: CFLAGS += $(DEBUGFLAGS)
debug: $(TARGET)
	@echo "Debug build complete: $(TARGET)"

# Release build
release: CFLAGS += $(RELEASEFLAGS)
release: $(TARGET)
	@echo "Release build complete: $(TARGET)"

# Link
$(TARGET): $(OBJECTS)
	@echo "Linking $@..."
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

# Compile source files
$(OBJDIR)/%.o: $(SRCDIR)/%.c | $(OBJDIR)
	@echo "Compiling $<..."
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# Create obj directory
$(OBJDIR):
	mkdir -p $(OBJDIR)

# Clean build artifacts
clean:
	@echo "Cleaning build artifacts..."
	rm -rf $(OBJDIR) $(TARGET) $(TARGET).exe

# Clean and rebuild
rebuild: clean all

# Install (optional, for Unix-like systems)
install: release
	@echo "Installing $(TARGET) to /usr/local/bin..."
	install -m 755 $(TARGET) /usr/local/bin/

# Uninstall
uninstall:
	@echo "Removing $(TARGET) from /usr/local/bin..."
	rm -f /usr/local/bin/$(TARGET)

# Show configuration
config:
	@echo "Build Configuration:"
	@echo "  CC:             $(CC)"
	@echo "  CFLAGS:         $(CFLAGS)"
	@echo "  MONGOC_CFLAGS:  $(MONGOC_CFLAGS)"
	@echo "  MONGOC_LIBS:    $(MONGOC_LIBS)"
	@echo "  NCURSES_CFLAGS: $(NCURSES_CFLAGS)"
	@echo "  NCURSES_LIBS:   $(NCURSES_LIBS)"
	@echo "  Sources:        $(SOURCES)"
	@echo "  Objects:        $(OBJECTS)"

# Help
help:
	@echo "MongoDB TUI Client - Makefile targets:"
	@echo "  make / make all     - Build release version (with dependency check)"
	@echo "  make debug          - Build debug version with symbols"
	@echo "  make release        - Build optimized release version"
	@echo "  make clean          - Remove build artifacts"
	@echo "  make rebuild        - Clean and rebuild"
	@echo "  make check-deps     - Check if dependencies are installed"
	@echo "  make config         - Show build configuration"
	@echo "  make install        - Install to /usr/local/bin (Unix-like only)"
	@echo "  make uninstall      - Remove from /usr/local/bin"
	@echo "  make help           - Show this help message"

.PHONY: all debug release clean rebuild install uninstall check-deps config help
