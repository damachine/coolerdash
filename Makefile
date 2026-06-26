.PHONY: all clean distclean install install-strip installdirs uninstall check debug logs help detect-distro install-deps check-deps
.DELETE_ON_ERROR:
VERSION := $(shell cat VERSION)

# Auto-detect: skip sudo when running as root (e.g. sudo make install)
ifeq ($(shell id -u),0)
    SUDO ?=
else
    SUDO ?= sudo
endif
REALOS ?= yes

# Compiler
CC ?= gcc
MARCH ?= x86-64-v3

# External dependencies (pkg-config, cached)
PKG_CFLAGS := $(shell pkg-config --cflags cairo jansson libcurl)
PKG_LIBS := $(shell pkg-config --libs cairo jansson libcurl)

# User-overridable flags
CFLAGS ?= -Wall -Wextra -O2 -march=$(MARCH)
CPPFLAGS ?=
LDFLAGS ?=

# Required project flags (always applied)
override CFLAGS += -std=c99
override CPPFLAGS += -Iinclude $(PKG_CFLAGS)
LDLIBS = $(PKG_LIBS) -lm

TARGET = coolerdash

# Directories
SRCDIR = src
OBJDIR = build
BINDIR = bin

# Source code files
MAIN_SOURCE = $(SRCDIR)/main.c
SRC_MODULES = $(SRCDIR)/device/config.c $(SRCDIR)/srv/cc_main.c $(SRCDIR)/srv/cc_conf.c $(SRCDIR)/srv/cc_sensor.c $(SRCDIR)/mods/display.c $(SRCDIR)/mods/dual.c $(SRCDIR)/mods/circle.c
HEADERS = $(SRCDIR)/device/config.h $(SRCDIR)/srv/cc_main.h $(SRCDIR)/srv/cc_conf.h $(SRCDIR)/srv/cc_sensor.h $(SRCDIR)/mods/display.h $(SRCDIR)/mods/dual.h $(SRCDIR)/mods/circle.h
OBJECTS = $(patsubst $(SRCDIR)/%.c,$(OBJDIR)/%.o,$(SRC_MODULES))

MANIFEST = etc/coolercontrol/plugins/coolerdash/manifest.toml
README = README.md

# GNU standard install directories
prefix ?= /usr
exec_prefix ?= $(prefix)
libexecdir ?= $(exec_prefix)/libexec
sysconfdir ?= /etc
datarootdir ?= $(prefix)/share
datadir ?= $(datarootdir)

# Install commands
INSTALL ?= install
INSTALL_PROGRAM ?= $(INSTALL)
INSTALL_DATA ?= $(INSTALL) -m 644

# Plugin directory (canonical path per CoolerControl cc-plugins spec)
PLUGINDIR = /var/lib/coolercontrol/plugins/coolerdash
COOLERCONTROL_SERVICE ?= coolercontrold
COOLERDASH_PLUGIN_SERVICE ?= cc-plugin-coolerdash

# Colors for terminal output
RED = \033[0;31m
GREEN = \033[0;32m
YELLOW = \033[0;33m
BLUE = \033[0;34m
PURPLE = \033[0;35m
CYAN = \033[0;36m
WHITE = \033[1;37m
RESET = \033[0m

# Default target (GNU convention)
all: $(TARGET)

# Standard Build Target - Standard C99 project structure
$(TARGET): $(OBJDIR) $(BINDIR) $(OBJECTS) $(MAIN_SOURCE)
	@printf "$(CYAN)Compiling $(TARGET) (Standard C99 structure)...$(RESET)\n"
	@printf "$(BLUE)Structure:$(RESET) src/ include/ build/ bin/\n"
	@printf "$(BLUE)CPPFLAGS:$(RESET) $(CPPFLAGS)\n"
	@printf "$(BLUE)CFLAGS:$(RESET) $(CFLAGS)\n"
	@printf "$(BLUE)LDLIBS:$(RESET) $(LDLIBS)\n"
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $(BINDIR)/$(TARGET) $(MAIN_SOURCE) $(OBJECTS) $(LDLIBS)
	@printf "$(GREEN)Build successful: $(BINDIR)/$(TARGET)$(RESET)\n"

# Create build directory
$(OBJDIR):
	@mkdir -p $(OBJDIR)
	@mkdir -p $(OBJDIR)/device
	@mkdir -p $(OBJDIR)/srv
	@mkdir -p $(OBJDIR)/mods

# Create bin directory
$(BINDIR):
	@mkdir -p $(BINDIR)

# Compile object files from src/ and subdirectories
$(OBJDIR)/%.o: $(SRCDIR)/%.c | $(OBJDIR)
	@mkdir -p $(dir $@)
	@printf "$(YELLOW)Compiling module: $<$(RESET)\n"
	@$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

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
	@printf "$(YELLOW)Cleaning up...$(RESET)\n"
	rm -f $(BINDIR)/$(TARGET) $(OBJECTS) *.o
	rm -rf $(OBJDIR) $(BINDIR)
	@printf "$(GREEN)Cleanup completed$(RESET)\n"

# GNU standard: distclean removes everything clean does (no autoconf here)
distclean: clean

# Detect Linux distro via release files, os-release as fallback
detect-distro:
	@if [ -f /etc/arch-release ]; then \
		echo "arch"; \
	elif [ -f /etc/gentoo-release ]; then \
		echo "gentoo"; \
	elif [ -f /etc/debian_version ]; then \
		echo "debian"; \
	elif [ -f /etc/fedora-release ]; then \
		echo "fedora"; \
	elif grep -qi 'opensuse\|suse' /etc/os-release 2>/dev/null; then \
		echo "opensuse"; \
	elif [ -f /etc/redhat-release ]; then \
		echo "rhel"; \
	else \
		echo "unknown"; \
	fi

# Install build/runtime deps per distro
install-deps:
	@DISTRO=$$($(MAKE) detect-distro); \
	case $$DISTRO in \
		arch) \
            printf "$(GREEN)Installing dependencies for Arch Linux/Manjaro...$(RESET)\n"; \
            $(SUDO) pacman -S --needed cairo libcurl-gnutls gcc make pkg-config ttf-roboto jansson || { \
                printf "$(RED)Error installing dependencies!$(RESET)\n"; \
                printf "$(YELLOW)Please run manually:$(RESET) $(SUDO) pacman -S cairo libcurl-gnutls gcc make pkg-config ttf-roboto jansson\n"; \
                exit 1; \
            }; \
            ;; \
        gentoo) \
            printf "$(GREEN)Installing dependencies for Gentoo...$(RESET)\n"; \
            $(SUDO) emerge --noreplace sys-devel/gcc sys-devel/gmake virtual/pkgconfig x11-libs/cairo net-misc/curl dev-libs/jansson media-fonts/roboto || { \
                printf "$(RED)Error installing dependencies!$(RESET)\n"; \
                printf "$(YELLOW)Please run manually:$(RESET) $(SUDO) emerge --noreplace sys-devel/gcc sys-devel/gmake virtual/pkgconfig x11-libs/cairo net-misc/curl dev-libs/jansson media-fonts/roboto\n"; \
                exit 1; \
            }; \
            ;; \
        debian) \
            printf "$(GREEN)Installing dependencies for Ubuntu/Debian...$(RESET)\n"; \
            $(SUDO) apt update && $(SUDO) apt install -y libcairo2-dev libcurl4-openssl-dev gcc make pkg-config fonts-roboto libjansson-dev || { \
                printf "$(RED)Error installing dependencies!$(RESET)\n"; \
                printf "$(YELLOW)Please run manually:$(RESET) $(SUDO) apt install libcairo2-dev libcurl4-openssl-dev gcc make pkg-config fonts-roboto libjansson-dev\n"; \
                exit 1; \
            }; \
            ;; \
        fedora) \
            printf "$(GREEN)Installing dependencies for Fedora...$(RESET)\n"; \
            $(SUDO) dnf install -y cairo-devel libcurl-devel gcc make pkg-config google-roboto-fonts jansson-devel || { \
                printf "$(RED)Error installing dependencies!$(RESET)\n"; \
                printf "$(YELLOW)Please run manually:$(RESET) $(SUDO) dnf install cairo-devel libcurl-devel gcc make pkg-config google-roboto-fonts jansson-devel\n"; \
                exit 1; \
            }; \
            ;; \
        rhel) \
            printf "$(GREEN)Installing dependencies for RHEL/CentOS...$(RESET)\n"; \
            $(SUDO) yum install -y cairo-devel libcurl-devel gcc make pkg-config google-roboto-fonts jansson-devel || { \
                printf "$(RED)Error installing dependencies!$(RESET)\n"; \
                printf "$(YELLOW)Please run manually:$(RESET) $(SUDO) yum install cairo-devel libcurl-devel gcc make pkg-config google-roboto-fonts jansson-devel\n"; \
                exit 1; \
            }; \
            ;; \
        opensuse) \
            printf "$(GREEN)Installing dependencies for openSUSE...$(RESET)\n"; \
            $(SUDO) zypper install -y cairo-devel libcurl-devel gcc make pkg-config google-roboto-fonts libjansson-devel || { \
                printf "$(RED)Error installing dependencies!$(RESET)\n"; \
                printf "$(YELLOW)Please run manually:$(RESET) $(SUDO) zypper install cairo-devel libcurl-devel gcc make pkg-config google-roboto-fonts libjansson-devel\n"; \
                exit 1; \
            }; \
            ;; \
		*) \
			printf "$(RED)Unknown distribution detected!$(RESET)\n"; \
			printf "\n"; \
			printf "$(YELLOW)Please install the following dependencies manually:$(RESET)\n"; \
			printf "\n"; \
			printf "$(WHITE)Arch Linux / Manjaro:$(RESET)\n"; \
			printf "  sudo pacman -S cairo libcurl-gnutls gcc make pkg-config ttf-roboto jansson\n"; \
			printf "\n"; \
			printf "$(WHITE)Gentoo:$(RESET)\n"; \
			printf "  sudo emerge --noreplace sys-devel/gcc sys-devel/gmake virtual/pkgconfig x11-libs/cairo net-misc/curl dev-libs/jansson media-fonts/roboto\n"; \
			printf "\n"; \
			printf "$(WHITE)Ubuntu / Debian:$(RESET)\n"; \
			printf "  sudo apt install libcairo2-dev libcurl4-openssl-dev gcc make pkg-config fonts-roboto libjansson-dev\n"; \
			printf "\n"; \
			printf "$(WHITE)Fedora:$(RESET)\n"; \
			printf "  sudo dnf install cairo-devel libcurl-devel gcc make pkg-config google-roboto-fonts jansson-devel\n"; \
			printf "\n"; \
			printf "$(WHITE)RHEL / CentOS:$(RESET)\n"; \
			printf "  sudo yum install cairo-devel libcurl-devel gcc make pkg-config google-roboto-fonts jansson-devel\n"; \
			printf "\n"; \
			printf "$(WHITE)openSUSE:$(RESET)\n"; \
			printf "  sudo zypper install cairo-devel libcurl-devel gcc make pkg-config google-roboto-fonts libjansson-devel\n"; \
			printf "\n"; \
			exit 1; \
			;; \
	esac

# Check if required libs are available via pkg-config
check-deps:
	@MISSING=""; \
	for dep in cairo libcurl jansson; do \
		if ! pkg-config --exists $$dep >/dev/null 2>&1; then \
			MISSING="$$MISSING $$dep"; \
		fi; \
	done; \
	if [ -n "$$MISSING" ]; then \
		printf "$(YELLOW)Missing dependencies:$$MISSING$(RESET)\n"; \
		$(MAKE) install-deps; \
	else \
		printf "$(GREEN)All dependencies found$(RESET)\n"; \
	fi

# Run tests (GNU standard target)
check: $(OBJDIR)
	@printf "$(CYAN)Running tests...$(RESET)\n"
	$(CC) $(CPPFLAGS) $(CFLAGS) -I./src -o $(OBJDIR)/test_scaling tests/test_scaling.c -lm
	./$(OBJDIR)/test_scaling
	@printf "$(GREEN)All tests passed$(RESET)\n"

# Install binary to /usr/libexec, plugin data to /var/lib/coolercontrol/plugins/coolerdash/
install: check-deps $(TARGET)
	@printf "\n"
	@printf "$(WHITE)=== COOLERDASH INSTALLATION ===$(RESET)\n"
	@printf "\n"
	@if [ -z "$(DESTDIR)" ] && [ "$(REALOS)" = "yes" ] && [ "$$(id -u)" -ne 0 ]; then \
		printf "$(RED)Error: Installation requires root privileges$(RESET)\n"; \
		printf "$(YELLOW)Run: sudo make install$(RESET)\n"; \
		exit 1; \
	fi
	@printf "$(CYAN)Installing plugin files...$(RESET)\n"
	@$(INSTALL) -d "$(DESTDIR)$(PLUGINDIR)"
	@$(INSTALL_PROGRAM) -D $(BINDIR)/$(TARGET) "$(DESTDIR)$(libexecdir)/coolerdash/coolerdash"
	@$(INSTALL_DATA) $(README) "$(DESTDIR)$(PLUGINDIR)/README.md"
	@$(INSTALL_DATA) CHANGELOG.md "$(DESTDIR)$(PLUGINDIR)/CHANGELOG.md"
	@$(INSTALL_DATA) VERSION "$(DESTDIR)$(PLUGINDIR)/VERSION"
	@if [ -f "$(DESTDIR)$(PLUGINDIR)/config.json" ]; then \
		$(INSTALL) -m 600 etc/coolercontrol/plugins/coolerdash/config.json "$(DESTDIR)$(PLUGINDIR)/config.json.new"; \
		chmod 600 "$(DESTDIR)$(PLUGINDIR)/config.json"; \
		printf "  $(YELLOW)Config:$(RESET) Existing config.json preserved (permissions updated to 600). New defaults saved as config.json.new\n"; \
	else \
		$(INSTALL) -m 600 etc/coolercontrol/plugins/coolerdash/config.json "$(DESTDIR)$(PLUGINDIR)/config.json"; \
	fi
	@if [ -f "$(DESTDIR)$(PLUGINDIR)/credentials.json" ]; then \
		chmod 600 "$(DESTDIR)$(PLUGINDIR)/credentials.json"; \
		printf "  $(GREEN)Credentials:$(RESET) Existing credentials.json preserved (chmod 600)\n"; \
	fi
	@$(INSTALL) -d "$(DESTDIR)$(PLUGINDIR)/ui"
	@$(INSTALL_DATA) etc/coolercontrol/plugins/coolerdash/ui/index.html "$(DESTDIR)$(PLUGINDIR)/ui/index.html"
	@$(INSTALL_DATA) images/shutdown.png "$(DESTDIR)$(PLUGINDIR)/shutdown.png"
	@$(INSTALL_DATA) $(MANIFEST) "$(DESTDIR)$(PLUGINDIR)/manifest.toml"
	@sed -i 's/{{VERSION}}/$(VERSION)/g' "$(DESTDIR)$(PLUGINDIR)/manifest.toml"
	@sed -i 's/{{VERSION}}/$(VERSION)/g' "$(DESTDIR)$(PLUGINDIR)/ui/index.html"
	@printf "  $(GREEN)Binary:$(RESET)       $(DESTDIR)$(libexecdir)/coolerdash/coolerdash\n"
	@printf "  $(GREEN)Config JSON:$(RESET)  $(DESTDIR)$(PLUGINDIR)/config.json (chmod 600)\n"
	@printf "  $(GREEN)Credentials:$(RESET) $(DESTDIR)$(PLUGINDIR)/credentials.json (chmod 600)\n"
	@printf "  $(GREEN)Web UI:$(RESET)       $(DESTDIR)$(PLUGINDIR)/ui/index.html\n"
	@printf "  $(GREEN)Plugin Lib:$(RESET)   Served by CoolerControl at /plugins/lib/cc-plugin-lib.js\n"
	@printf "  $(GREEN)Plugin:$(RESET)       $(DESTDIR)$(PLUGINDIR)/manifest.toml\n"
	@printf "  $(GREEN)Image:$(RESET)        shutdown.png (coolerdash.png)\n"
	@printf "  $(GREEN)Documentation:$(RESET) README.md, LICENSE, CHANGELOG.md, VERSION\n"
	@printf "\n"
	@printf "$(CYAN)Note: Plugin binary is available at $(libexecdir)/coolerdash/coolerdash$(RESET)\\n"
	@printf "\n"
	@printf "$(CYAN)Installing license...$(RESET)\n"
	@$(INSTALL_DATA) -D LICENSE "$(DESTDIR)$(datarootdir)/licenses/coolerdash/LICENSE"
	@printf "  $(GREEN)License:$(RESET) $(DESTDIR)$(datarootdir)/licenses/coolerdash/LICENSE\n"
	@printf "\n"
	@printf "$(WHITE)INSTALLATION SUCCESSFUL$(RESET)\n"
	@printf "\n"
	@printf "$(YELLOW)Next steps:$(RESET)\n"
	@if [ "$(REALOS)" = "yes" ]; then \
		if command -v systemctl >/dev/null 2>&1; then \
			$(SUDO) systemctl daemon-reload 2>/dev/null || true; \
			$(SUDO) systemctl restart $(COOLERCONTROL_SERVICE).service 2>/dev/null || true; \
		elif command -v rc-service >/dev/null 2>&1; then \
			$(SUDO) rc-service $(COOLERCONTROL_SERVICE) restart 2>/dev/null || true; \
		fi; \
	fi
	@if command -v systemctl >/dev/null 2>&1; then \
		printf "  $(PURPLE)Restart CoolerControl:$(RESET) systemctl restart $(COOLERCONTROL_SERVICE).service\n"; \
	elif command -v rc-service >/dev/null 2>&1; then \
		printf "  $(PURPLE)Restart CoolerControl:$(RESET) rc-service $(COOLERCONTROL_SERVICE) restart\n"; \
	else \
		printf "  $(PURPLE)Restart CoolerControl:$(RESET) restart your CoolerControl daemon\n"; \
	fi
	@printf "  $(PURPLE)Plugin:$(RESET)         CoolerControl will manage coolerdash automatically\n"
	@printf "\n"

# Install with stripped binary (GNU standard target)
install-strip:
	$(MAKE) install INSTALL_PROGRAM='$(INSTALL_PROGRAM) -s'

# Create install directories without installing (GNU standard target)
installdirs:
	$(INSTALL) -d "$(DESTDIR)$(libexecdir)/coolerdash"
	$(INSTALL) -d "$(DESTDIR)$(PLUGINDIR)"
	$(INSTALL) -d "$(DESTDIR)$(PLUGINDIR)/ui"
	$(INSTALL) -d "$(DESTDIR)$(datarootdir)/licenses/coolerdash"

# Uninstall Target
uninstall:
	@printf "\n"
	@printf "$(WHITE)=== COOLERDASH UNINSTALLATION ===$(RESET)\n"
	@printf "\n"
	@if [ -z "$(DESTDIR)" ] && [ "$(REALOS)" = "yes" ] && [ "$$(id -u)" -ne 0 ]; then \
		printf "$(RED)Error: Uninstallation requires root privileges$(RESET)\n"; \
		printf "$(YELLOW)Run: sudo make uninstall$(RESET)\n"; \
		exit 1; \
	fi
	@if [ "$(REALOS)" = "yes" ]; then \
		printf "$(CYAN)Stopping and disabling services...$(RESET)\n"; \
		if command -v systemctl >/dev/null 2>&1; then \
			$(SUDO) systemctl stop $(COOLERDASH_PLUGIN_SERVICE).service >/dev/null 2>&1 || true; \
			$(SUDO) systemctl disable $(COOLERDASH_PLUGIN_SERVICE).service >/dev/null 2>&1 || true; \
		elif command -v rc-service >/dev/null 2>&1; then \
			$(SUDO) rc-service $(COOLERDASH_PLUGIN_SERVICE) stop >/dev/null 2>&1 || true; \
		fi; \
	fi
	@$(SUDO) rm -rf "$(DESTDIR)$(PLUGINDIR)"
	@$(SUDO) rm -rf "$(DESTDIR)$(libexecdir)/coolerdash"
	@$(SUDO) rm -rf "$(DESTDIR)$(datarootdir)/licenses/coolerdash"
	@if [ "$(REALOS)" = "yes" ]; then \
		$(SUDO) mandb -q >/dev/null 2>&1 || true; \
		if command -v systemctl >/dev/null 2>&1; then \
			$(SUDO) systemctl daemon-reload >/dev/null 2>&1 || true; \
			$(SUDO) systemctl restart $(COOLERCONTROL_SERVICE).service >/dev/null 2>&1 || true; \
		elif command -v rc-service >/dev/null 2>&1; then \
			$(SUDO) rc-service $(COOLERCONTROL_SERVICE) restart >/dev/null 2>&1 || true; \
		fi; \
	fi
	@printf "\n$(GREEN)Uninstallation completed successfully$(RESET)\n"
	@printf "\n"

# Debug Build
debug: CPPFLAGS += -DDEBUG
debug: CFLAGS += -g -fsanitize=address
debug: LDFLAGS += -fsanitize=address
debug: $(TARGET)
	@printf "$(GREEN)Debug build created with AddressSanitizer: $(BINDIR)/$(TARGET)$(RESET)\n"

logs:
	@printf "$(CYAN)Live logs (Ctrl+C to exit):$(RESET)\n"
	@if command -v journalctl >/dev/null 2>&1; then \
		journalctl -u $(COOLERDASH_PLUGIN_SERVICE).service -f; \
	else \
		printf "$(YELLOW)journalctl not found.$(RESET) On OpenRC, inspect your system logger for CoolerControl/coolerdash output.\n"; \
		rc-service $(COOLERCONTROL_SERVICE) status 2>/dev/null || true; \
	fi

# Help
help:
	@printf "\n"
	@printf "$(WHITE)========================================$(RESET)\n"
	@printf "$(WHITE)         COOLERDASH BUILD SYSTEM        $(RESET)\n"
	@printf "$(WHITE)========================================$(RESET)\n"
	@printf "\n"
	@printf "$(YELLOW)Build Targets:$(RESET)\n"
	@printf "  $(GREEN)make$(RESET)              - Compiles the program\n"
	@printf "  $(GREEN)make clean$(RESET)        - Removes compiled files\n"
	@printf "  $(GREEN)make distclean$(RESET)    - Same as clean (no autoconf)\n"
	@printf "  $(GREEN)make check$(RESET)        - Runs unit tests\n"
	@printf "  $(GREEN)make debug$(RESET)        - Debug build with AddressSanitizer\n"
	@printf "\n"
	@printf "$(YELLOW)Installation:$(RESET)\n"
	@printf "  $(GREEN)make install$(RESET)      - Installs binary + plugin data\n"
	@printf "  $(GREEN)make install-strip$(RESET)- Installs with stripped binary\n"
	@printf "  $(GREEN)make installdirs$(RESET) - Creates install directories only\n"
	@printf "  $(GREEN)make uninstall$(RESET)   - Uninstalls the program\n"
	@printf "\n"
	@printf "$(YELLOW)Plugin Management:$(RESET)\n"
	@printf "  $(GREEN)systemctl enable --now $(COOLERCONTROL_SERVICE).service$(RESET)    - Start CoolerControl on systemd\n"
	@printf "  $(GREEN)rc-update add $(COOLERCONTROL_SERVICE) default$(RESET)       - Enable CoolerControl on OpenRC\n"
	@printf "  $(GREEN)rc-service $(COOLERCONTROL_SERVICE) start$(RESET)            - Start CoolerControl on OpenRC\n"
	@printf "  $(GREEN)make logs$(RESET)                         - Shows plugin logs when journalctl is available\n"
	@printf "  $(BLUE)Note:$(RESET) CoolerControl automatically manages coolerdash lifecycle\n"
	@printf "  $(BLUE)Shutdown:$(RESET) Plugin automatically displays shutdown.png when stopped\n"
	@printf "\n"
	@printf "$(YELLOW)Documentation:$(RESET)\n"
	@printf "  $(GREEN)man coolerdash$(RESET) - Shows manual page\n"
	@printf "  $(GREEN)make help$(RESET)     - Shows this help\n"
	@printf "\n"
	@printf "$(YELLOW)README:$(RESET)\n"
	@printf "  $(GREEN)README.md$(RESET)         - English (main documentation)\n"
	@printf "\n"
	@printf "$(YELLOW)Version Usage:$(RESET)\n"
	@printf "  $(GREEN)Program:$(RESET) $(libexecdir)/coolerdash/coolerdash [mode]\n"
	@printf "  $(GREEN)Config:$(RESET)  $(PLUGINDIR)/config.json\n"
	@printf "  $(GREEN)Web UI:$(RESET)  CoolerControl Plugin Settings\n"
	@printf "\n"
