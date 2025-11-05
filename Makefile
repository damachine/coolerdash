# -----------------------------------------------------------------------------
# author: damachine (christkue79@gmail.com)
# website: https://github.com/damachine
#          https://github.com/damachine/coolerdash
# -----------------------------------------------------------------------------
# MIT License
#
# Copyright (c) 2025 damachine
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.
# -----------------------------------------------------------------------------
# Info:
#   CoolerDash 'Makefile' - Installation and Build
#   Build system for CoolerDash (C99 compliant).
#   Project coding standards and packaging notes (see README for details)
#
# Details:
#   This 'Makefile' handles build, install, dependencies, and packaging.
#   Edit dependencies, paths, and user as needed for your system.
#   Do not run as root. Use dedicated user for security.
#   Ensure all required dependencies are installed.
#   It uses color output and Unicode icons for better readability. All paths and dependencies are configurable.
#   See 'README.md' for further details.
#
# Build:
#   'make'
#
# Dependency:
#   'cairo' 'coolercontrol' 'jansson' 'libcurl-gnutls' 'libinih' are required for core functionality
#   'ttf-roboto' is required for proper font rendering on the LCD
#   All dependencies are documented in 'README.md'.
# -----------------------------------------------------------------------------

.PHONY: clean install uninstall debug logs help detect-distro install-deps check-deps

VERSION := $(shell cat VERSION)

SUDO ?= sudo
REALOS ?= yes

CC = gcc
CFLAGS = -Wall -Wextra -O2 -std=c99 -march=x86-64-v3 -Iinclude $(shell pkg-config --cflags cairo jansson libcurl inih)
LIBS = $(shell pkg-config --libs cairo jansson libcurl inih) -lm
TARGET = coolerdash

# Directories
SRCDIR = src
OBJDIR = build
BINDIR = bin

# Source code files
MAIN_SOURCE = $(SRCDIR)/main.c
SRC_MODULES = $(SRCDIR)/config.c $(SRCDIR)/coolercontrol.c $(SRCDIR)/display.c $(SRCDIR)/monitor.c
HEADERS = $(SRCDIR)/config.h $(SRCDIR)/coolercontrol.h $(SRCDIR)/display.h $(SRCDIR)/monitor.h
OBJECTS = $(patsubst $(SRCDIR)/%.c,$(OBJDIR)/%.o,$(SRC_MODULES))

SERVICE = etc/systemd/coolerdash.service
MANPAGE = man/coolerdash.1
README = README.md

# Colors for terminal output
RED = \033[0;31m
GREEN = \033[0;32m
YELLOW = \033[0;33m
BLUE = \033[0;34m
PURPLE = \033[0;35m
CYAN = \033[0;36m
WHITE = \033[1;37m
RESET = \033[0m

# Icons (Unicode)
ICON_BUILD = 🔨
ICON_INSTALL = 📦
ICON_SERVICE = ⚙️
ICON_SUCCESS = ✅
ICON_WARNING = ⚠️
ICON_INFO = ℹ️
ICON_CLEAN = 🧹
ICON_UNINSTALL = 🗑️

# Standard Build Target - Standard C99 project structure
$(TARGET): $(OBJDIR) $(BINDIR) $(OBJECTS) $(MAIN_SOURCE)
	@printf "\n$(PURPLE)Manual Installation Check:$(RESET)\n"
	@printf "If you see errors about 'conflicting files' or manual installation, run 'make uninstall' and remove leftover files in /opt/coolerdash, /usr/bin/coolerdash, /etc/systemd/system/coolerdash.service.\n\n"
	@printf "$(ICON_BUILD) $(CYAN)Compiling $(TARGET) (Standard C99 structure)...$(RESET)\n"
	@printf "$(BLUE)Structure:$(RESET) src/ include/ build/ bin/\n"
	@printf "$(BLUE)CFLAGS:$(RESET) $(CFLAGS)\n"
	@printf "$(BLUE)LIBS:$(RESET) $(LIBS)\n"
	$(CC) $(CFLAGS) -o $(BINDIR)/$(TARGET) $(MAIN_SOURCE) $(OBJECTS) $(LIBS)
	@printf "$(ICON_SUCCESS) $(GREEN)Build successful: $(BINDIR)/$(TARGET)$(RESET)\n"

# Create build directory
$(OBJDIR):
	@mkdir -p $(OBJDIR)

# Create bin directory
$(BINDIR):
	@mkdir -p $(BINDIR)

# Compile object files from src/
$(OBJDIR)/%.o: $(SRCDIR)/%.c | $(OBJDIR)
	@printf "$(ICON_BUILD) $(YELLOW)Compiling module: $<$(RESET)\n"
	@$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

-include $(OBJECTS:.o=.d)

# Dependencies for header changes
$(OBJECTS): $(HEADERS)

# Banner
banner:
	@echo " "
	@echo " Developed and maintained by: "
	@echo "  ____    _    __  __    _    ____ _   _ ___ _   _ _____  "
	@echo " |  _ \  / \  |  \/  |  / \  / ___| | | |_ _| \ | | ____| "
	@echo " | | | |/ _ \ | |\/| | / _ \| |   | |_| || ||  \| |  _|   "
	@echo " | |_| / ___ \| |  | |/ ___ \ |___|  _  || || |\  | |___  "
	@echo " |____/_/   \_\_|  |_/_/   \_\____|_| |_|___|_| \_|_____| "
	@echo " Version: $(VERSION) "
	@echo " "

# Clean Target
clean:
	$(MAKE) banner
	@printf "$(ICON_CLEAN) $(YELLOW)Cleaning up...$(RESET)\n"
	rm -f $(BINDIR)/$(TARGET) $(OBJECTS) *.o
	rm -rf $(OBJDIR) $(BINDIR)
	@printf "$(ICON_SUCCESS) $(GREEN)Cleanup completed$(RESET)\n"

# Detect Linux Distribution (internal function)
detect-distro:
	@if [ -f /etc/arch-release ]; then \
		echo "arch"; \
	elif [ -f /etc/debian_version ]; then \
		echo "debian"; \
	elif [ -f /etc/fedora-release ]; then \
		echo "fedora"; \
	elif [ -f /etc/redhat-release ]; then \
		echo "rhel"; \
	elif [ -f /etc/opensuse-release ]; then \
		echo "opensuse"; \
	else \
		echo "unknown"; \
	fi

# Install Dependencies (internal function)
install-deps:
	@DISTRO=$$($(MAKE) detect-distro); \
	case $$DISTRO in \
		arch) \
            printf "$(ICON_INSTALL) $(GREEN)Installing dependencies for Arch Linux/Manjaro...$(RESET)\n"; \
            $(SUDO) pacman -S --needed cairo libcurl-gnutls libinih gcc make pkg-config ttf-roboto jansson || { \
                printf "$(ICON_WARNING) $(RED)Error installing dependencies!$(RESET)\n"; \
                printf "$(YELLOW)Please run manually:$(RESET) $(SUDO) pacman -S cairo libcurl-gnutls libinih gcc make pkg-config ttf-roboto jansson\n"; \
                exit 1; \
            }; \
            ;; \
        debian) \
            printf "$(ICON_INSTALL) $(GREEN)Installing dependencies for Ubuntu/Debian...$(RESET)\n"; \
            $(SUDO) apt update && $(SUDO) apt install -y libcairo2-dev libcurl4-openssl-dev libinih-dev gcc make pkg-config fonts-roboto libjansson-dev || { \
                printf "$(ICON_WARNING) $(RED)Error installing dependencies!$(RESET)\n"; \
                printf "$(YELLOW)Please run manually:$(RESET) $(SUDO) apt install libcairo2-dev libcurl4-openssl-dev libinih-dev gcc make pkg-config fonts-roboto libjansson-dev\n"; \
                exit 1; \
            }; \
            ;; \
        fedora) \
            printf "$(ICON_INSTALL) $(GREEN)Installing dependencies for Fedora...$(RESET)\n"; \
            $(SUDO) dnf install -y cairo-devel libcurl-devel inih-devel gcc make pkg-config google-roboto-fonts jansson-devel || { \
                printf "$(ICON_WARNING) $(RED)Error installing dependencies!$(RESET)\n"; \
                printf "$(YELLOW)Please run manually:$(RESET) $(SUDO) dnf install cairo-devel libcurl-devel inih-devel gcc make pkg-config google-roboto-fonts jansson-devel\n"; \
                exit 1; \
            }; \
            ;; \
        rhel) \
            printf "$(ICON_INSTALL) $(GREEN)Installing dependencies for RHEL/CentOS...$(RESET)\n"; \
            $(SUDO) yum install -y cairo-devel libcurl-devel inih-devel gcc make pkg-config google-roboto-fonts jansson-devel || { \
                printf "$(ICON_WARNING) $(RED)Error installing dependencies!$(RESET)\n"; \
                printf "$(YELLOW)Please run manually:$(RESET) $(SUDO) yum install cairo-devel libcurl-devel inih-devel gcc make pkg-config google-roboto-fonts jansson-devel\n"; \
                exit 1; \
            }; \
            ;; \
        opensuse) \
            printf "$(ICON_INSTALL) $(GREEN)Installing dependencies for openSUSE...$(RESET)\n"; \
            $(SUDO) zypper install -y cairo-devel libcurl-devel libinih-devel gcc make pkg-config google-roboto-fonts libjansson-devel || { \
                printf "$(ICON_WARNING) $(RED)Error installing dependencies!$(RESET)\n"; \
                printf "$(YELLOW)Please run manually:$(RESET) $(SUDO) zypper install cairo-devel libcurl-devel libinih-devel gcc make pkg-config google-roboto-fonts libjansson-devel\n"; \
                exit 1; \
            }; \
            ;; \
		*) \
			printf "$(ICON_WARNING) $(RED)Unknown distribution detected!$(RESET)\n"; \
			printf "\n"; \
			printf "$(YELLOW)Please install the following dependencies manually:$(RESET)\n"; \
			printf "\n"; \
			printf "$(WHITE)Arch Linux / Manjaro:$(RESET)\n"; \
			printf "  sudo pacman -S cairo libcurl-gnutls libinih gcc make pkg-config ttf-roboto jansson\n"; \
			printf "\n"; \
			printf "$(WHITE)Ubuntu / Debian:$(RESET)\n"; \
			printf "  sudo apt install libcairo2-dev libcurl4-openssl-dev libinih-dev gcc make pkg-config fonts-roboto libjansson-dev\n"; \
			printf "\n"; \
			printf "$(WHITE)Fedora:$(RESET)\n"; \
			printf "  sudo dnf install cairo-devel libcurl-devel inih-devel gcc make pkg-config google-roboto-fonts jansson-devel\n"; \
			printf "\n"; \
			printf "$(WHITE)RHEL / CentOS:$(RESET)\n"; \
			printf "  sudo yum install cairo-devel libcurl-devel inih-devel gcc make pkg-config google-roboto-fonts jansson-devel\n"; \
			printf "\n"; \
			printf "$(WHITE)openSUSE:$(RESET)\n"; \
			printf "  sudo zypper install cairo-devel libcurl-devel inih-devel gcc make pkg-config google-roboto-fonts libjansson-devel\n"; \
			printf "\n"; \
			exit 1; \
			;; \
	esac

# Check Dependencies for Installation (internal function called by install)
check-deps:
	@MISSING=""; \
	for dep in cairo libcurl inih jansson; do \
		if ! pkg-config --exists $$dep >/dev/null 2>&1; then \
			MISSING="$$MISSING $$dep"; \
		fi; \
	done; \
	if [ -n "$$MISSING" ]; then \
		printf "$(ICON_WARNING) $(YELLOW)Missing dependencies:$$MISSING$(RESET)\n"; \
		$(MAKE) install-deps; \
	else \
		printf "$(ICON_SUCCESS) $(GREEN)All dependencies found$(RESET)\n"; \
	fi

# Install Target - Installs to /opt/coolerdash/ (with automatic dependency check and service management)
install: check-deps $(TARGET)
	@printf "\n"
	@printf "$(ICON_INSTALL) $(WHITE)═══ COOLERDASH INSTALLATION ═══$(RESET)\n"
	@printf "\n"
	@if [ "$(REALOS)" = "yes" ]; then \
		if ! id -u coolerdash &>/dev/null; then \
			$(SUDO) useradd --system --no-create-home coolerdash; \
			printf "$(ICON_SUCCESS) $(GREEN)Runtime directory and user ready$(RESET)\n"; \
			printf "\n"; \
		fi; \
		printf "$(ICON_SERVICE) $(CYAN)Checking running service and processes...$(RESET)\n"; \
		if $(SUDO) systemctl is-active --quiet coolerdash.service; then \
			printf "  $(YELLOW)→$(RESET) Service running, stopping for update...\n"; \
			$(SUDO) systemctl stop coolerdash.service 2>/dev/null || true; \
			printf "  $(GREEN)→$(RESET) Service stopped\n"; \
		else \
			printf "  $(BLUE)→$(RESET) Service not running\n"; \
		fi; \
		COOLERDASH_COUNT=$$(pgrep -x coolerdash 2>/dev/null | wc -l); \
		if [ "$$COOLERDASH_COUNT" -gt 0 ]; then \
			printf "  $(YELLOW)→$(RESET) Found $$COOLERDASH_COUNT manual coolerdash process(es), terminating...\n"; \
			$(SUDO) killall -TERM coolerdash 2>/dev/null || true; \
			sleep 2; \
			REMAINING_COUNT=$$(pgrep -x coolerdash 2>/dev/null | wc -l); \
			if [ "$$REMAINING_COUNT" -gt 0 ]; then \
				printf "  $(RED)→$(RESET) Force killing $$REMAINING_COUNT remaining process(es)...\n"; \
				$(SUDO) killall -KILL coolerdash 2>/dev/null || true; \
			fi; \
			printf "  $(GREEN)→$(RESET) Manual processes terminated\n"; \
		else \
			printf "  $(BLUE)→$(RESET) No manual coolerdash processes found\n"; \
		fi; \
	else \
		printf "$(ICON_INFO) $(YELLOW)Service/user management skipped (CI environment).$(RESET)\n"; \
	fi
	@printf "\n"
	@printf "$(ICON_INFO) $(CYAN)Creating directories...$(RESET)\n"
	install -dm755 "$(DESTDIR)/opt/coolerdash"
	install -dm755 "$(DESTDIR)/opt/coolerdash/bin"
	install -dm755 "$(DESTDIR)/opt/coolerdash/images"
	@printf "$(ICON_SUCCESS) $(GREEN)Directories created$(RESET)\n"
	@printf "\n"
	@printf "$(ICON_INFO) $(CYAN)Installing files...$(RESET)\n"
	install -Dm755 $(BINDIR)/$(TARGET) "$(DESTDIR)/opt/coolerdash/bin/coolerdash"
	install -Dm644 $(README) "$(DESTDIR)/opt/coolerdash/README.md"
	install -Dm644 LICENSE "$(DESTDIR)/opt/coolerdash/LICENSE"
	install -Dm644 CHANGELOG.md "$(DESTDIR)/opt/coolerdash/CHANGELOG.md"
	install -Dm644 VERSION "$(DESTDIR)/opt/coolerdash/VERSION"
	install -Dm644 images/shutdown.png "$(DESTDIR)/opt/coolerdash/images/shutdown.png"
	@printf "$(YELLOW)Available version:$(RESET)\n"
	@printf "  $(GREEN)Program:$(RESET) /opt/coolerdash/bin/coolerdash [mode]\n"
	@printf "  $(GREEN)Documentation:$(RESET) $(DESTDIR)/opt/coolerdash/\n"
	@printf "  $(GREEN)  - README.md$(RESET)\n"
	@printf "  $(GREEN)  - LICENSE$(RESET)\n"
	@printf "  $(GREEN)  - CHANGELOG.md$(RESET)\n"
	@printf "  $(GREEN)Resources:$(RESET) $(DESTDIR)/opt/coolerdash/images/shutdown.png\n"
	@printf "$(ICON_INFO) $(CYAN)Files installed$(RESET)\n"
	@printf "\n"
	@printf "$(ICON_INFO) $(CYAN)Installing configuration...$(RESET)\n"
	install -Dm644 etc/coolerdash/config.ini "$(DESTDIR)/etc/coolerdash/config.ini"
	@printf "  $(GREEN)Config:$(RESET) $(DESTDIR)/etc/coolerdash/config.ini\n"
	@printf "\n"
	@if [ "$(REALOS)" = "yes" ]; then \
		printf "$(ICON_INFO) $(CYAN)Creating system symlink...$(RESET)\n"; \
		install -dm755 "$(DESTDIR)/usr/bin"; \
		$(SUDO) ln -sf /opt/coolerdash/bin/coolerdash "$(DESTDIR)/usr/bin/coolerdash"; \
		printf "  $(GREEN)Symlink:$(RESET) /usr/bin/coolerdash -> /opt/coolerdash/bin/coolerdash\n"; \
	else \
		printf "$(ICON_INFO) $(YELLOW)Symlink skipped (CI environment)$(RESET)\n"; \
	fi
	@printf "\n"
	@printf "$(ICON_SERVICE) $(CYAN)Installing service & documentation...$(RESET)\n"
	install -Dm644 $(SERVICE) "$(DESTDIR)/etc/systemd/system/coolerdash.service"
	install -Dm644 $(MANPAGE) "$(DESTDIR)/usr/share/man/man1/coolerdash.1"
	@printf "  $(GREEN)Service:$(RESET) $(DESTDIR)/etc/systemd/system/coolerdash.service\n"
	@printf "  $(GREEN)Manual:$(RESET) $(DESTDIR)/usr/share/man/man1/coolerdash.1\n"
	@printf "\n"
	@printf "$(ICON_SUCCESS) $(WHITE)INSTALLATION SUCCESSFUL$(RESET)\n"
	@printf "\n"
	@printf "$(YELLOW)Next steps:$(RESET)\n"
	@printf "  $(PURPLE)Reload systemd:$(RESET) systemctl daemon-reload\n"
	@printf "  $(PURPLE)Start service:$(RESET)  systemctl enable --now coolerdash.service\n"
	@printf "  $(PURPLE)Show manual:$(RESET)    man coolerdash\n"
	@printf "\n"

# Uninstall Target
uninstall:
	@printf "\n"
	@printf "$(ICON_UNINSTALL) $(WHITE)═══ COOLERDASH UNINSTALLATION ═══$(RESET)\n"
	@printf "\n"
	@printf "$(ICON_WARNING) $(YELLOW)Stopping and disabling service...$(RESET)\n"
	$(SUDO) systemctl stop coolerdash.service 2>/dev/null || true
	$(SUDO) systemctl disable coolerdash.service 2>/dev/null || true
	@printf "$(ICON_SUCCESS) $(GREEN)Service stopped$(RESET)\n"
	@printf "\n"
	@printf "$(ICON_INFO) $(CYAN)Removing all files...$(RESET)\n"
	$(SUDO) rm -f /etc/systemd/system/coolerdash.service 2>/dev/null || true
	$(SUDO) rm -f /usr/share/man/man1/coolerdash.1 2>/dev/null || true
	$(SUDO) rm -f /opt/coolerdash/README.md 2>/dev/null || true
	$(SUDO) rm -f /opt/coolerdash/LICENSE 2>/dev/null || true
	$(SUDO) rm -f /opt/coolerdash/CHANGELOG.md 2>/dev/null || true
	$(SUDO) rm -f /opt/coolerdash/VERSION 2>/dev/null || true
	$(SUDO) rm -f /opt/coolerdash/bin/$(TARGET) 2>/dev/null || true
	$(SUDO) rm -rf /opt/coolerdash/bin/ 2>/dev/null || true
	$(SUDO) rm -rf /opt/coolerdash/images/ 2>/dev/null || true
	$(SUDO) rm -rf /opt/coolerdash/ 2>/dev/null || true
	$(SUDO) rm -f /usr/bin/coolerdash 2>/dev/null || true
	# Remove config directory only if empty (preserves modified configs)
	$(SUDO) rmdir /etc/coolerdash 2>/dev/null || true
	# Remove any remaining files in /opt/coolerdash (catch-all, safe if dir already gone)
	$(SUDO) rm -f /opt/coolerdash/* 2>/dev/null || true
	@printf "  $(RED)✗$(RESET) Service: /etc/systemd/system/coolerdash.service\n"
	@printf "  $(RED)✗$(RESET) Manual: /usr/share/man/man1/coolerdash.1\n"
	@printf "  $(RED)✗$(RESET) Program: /opt/coolerdash/bin/$(TARGET)\n"
	@printf "  $(RED)✗$(RESET) Documentation: /opt/coolerdash/README.md, LICENSE, CHANGELOG.md\n"
	@printf "  $(RED)✗$(RESET) Images: /opt/coolerdash/images/\n"
	@printf "  $(RED)✗$(RESET) Installation: /opt/coolerdash/\n"
	@printf "  $(RED)✗$(RESET) Symlink: /usr/bin/coolerdash\n"
	@printf "\n"
	@printf "$(ICON_INFO) $(CYAN)Updating system...$(RESET)\n"
	@if id -u coolerdash &>/dev/null; then \
        $(SUDO) userdel -rf coolerdash || true; \
    fi
	$(SUDO) mandb -q 2>/dev/null || true
	$(SUDO) systemctl daemon-reload 2>/dev/null || true
	@printf "\n"
	@printf "$(ICON_SUCCESS) $(WHITE)═══ COMPLETE REMOVAL SUCCESSFUL ═══$(RESET)\n"
	@printf "\n"
	@printf "$(ICON_SUCCESS) $(GREEN)All coolerdash files have been completely removed$(RESET)\n"
	@printf "\n"

# Debug Build
debug: CFLAGS += -g -DDEBUG -fsanitize=address
debug: LIBS += -fsanitize=address
debug: $(TARGET)
	@printf "$(ICON_SUCCESS) $(GREEN)Debug build created with AddressSanitizer: $(BINDIR)/$(TARGET)$(RESET)\n"

logs:
	@printf "$(ICON_INFO) $(CYAN)Live logs (Ctrl+C to exit):$(RESET)\n"
	journalctl -u coolerdash.service -f

# Help
help:
	@printf "\n"
	@printf "$(WHITE)════════════════════════════════════════$(RESET)\n"
	@printf "$(WHITE)         COOLERDASH BUILD SYSTEM        $(RESET)\n"
	@printf "$(WHITE)════════════════════════════════════════$(RESET)\n"
	@printf "\n"
	@printf "$(YELLOW)🔨 Build Targets:$(RESET)\n"
	@printf "  $(GREEN)make$(RESET)          - Compiles the program\n"
	@printf "  $(GREEN)make clean$(RESET)    - Removes compiled files\n"
	@printf "  $(GREEN)make debug$(RESET)    - Debug build with AddressSanitizer\n"
	@printf "\n"
	@printf "$(YELLOW)📦 Installation:$(RESET)\n"
	@printf "  $(GREEN)make install$(RESET)  - Installs to /opt/coolerdash/bin/ (auto-installs dependencies)\n"
	@printf "  $(GREEN)make uninstall$(RESET)- Uninstalls the program\n"
	@printf "\n"
	@printf "$(YELLOW)⚙️  Service Management:$(RESET)\n"
	@printf "  $(GREEN)systemctl start coolerdash.service$(RESET)    - Starts the service\n"
	@printf "  $(GREEN)systemctl stop coolerdash.service$(RESET)     - Stops the service (sends shutdown.png to LCD automatically)\n"
	@printf "  $(GREEN)systemctl restart coolerdash.service$(RESET)  - Restarts the service\n"
	@printf "  $(GREEN)systemctl status coolerdash.service$(RESET)   - Shows service status\n"
	@printf "  $(GREEN)systemctl enable coolerdash.service$(RESET)   - Enables autostart\n"
	@printf "  $(GREEN)systemctl disable coolerdash.service$(RESET)  - Disables autostart\n"
	@printf "  $(GREEN)journalctl -u coolerdash.service -f$(RESET)   - Shows live logs\n"
	@printf "  $(BLUE)Shutdown:$(RESET) Service automatically displays shutdown.png when stopped (integrated in C code)\n"
	@printf "\n"
	@printf "$(YELLOW)📚 Documentation:$(RESET)\n"
	@printf "  $(GREEN)man coolerdash$(RESET) - Shows manual page\n"
	@printf "  $(GREEN)make help$(RESET)     - Shows this help\n"
	@printf "\n"
	@printf "$(YELLOW)🌍 README:$(RESET)\n"
	@printf "  $(GREEN)README.md$(RESET)         - 🇺🇸 English (main documentation)\n"
	@printf "\n"
	@printf "$(YELLOW)🔄 Version Usage:$(RESET)\n"
	@printf "  $(GREEN)Program:$(RESET) /opt/coolerdash/bin/coolerdash [mode]\n"
	@printf "\n"
