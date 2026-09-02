.PHONY: all banner clean install uninstall debug help detect-distro install-deps check-deps
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

# External dependencies (pkg-config, cached)
PKG_CFLAGS := $(shell pkg-config --cflags cairo jansson libcurl)
PKG_LIBS := $(shell pkg-config --libs cairo jansson libcurl)

# User-overridable flags
CFLAGS ?= -Wall -Wextra -O2
CPPFLAGS ?=
LDFLAGS ?=

# Required project flags (always applied)
override CFLAGS += -std=c99 -pthread
override CPPFLAGS += -Iinclude $(PKG_CFLAGS)
override CPPFLAGS += -DCOOLERDASH_VERSION='"$(VERSION)"'
LDLIBS = $(PKG_LIBS) -lm -pthread

ifeq ($(DEBUG),1)
override CPPFLAGS += -DDEBUG
override CFLAGS += -g -fsanitize=address
override LDFLAGS += -fsanitize=address
endif

TARGET = coolerdash

# Directories
SRCDIR = src
OBJDIR = build
BINDIR = bin
PROGRAM = $(BINDIR)/$(TARGET)

# Source code files
MAIN_SOURCE = $(SRCDIR)/main.c
SRC_MODULES = $(SRCDIR)/device/config.c $(SRCDIR)/device/profile.c $(SRCDIR)/device/hwreport.c $(SRCDIR)/srv/cc_main.c $(SRCDIR)/srv/cc_conf.c $(SRCDIR)/srv/cc_sensor.c $(SRCDIR)/mods/display.c $(SRCDIR)/mods/dual.c $(SRCDIR)/mods/circle.c $(SRCDIR)/mods/split.c
HEADERS = $(SRCDIR)/device/config.h $(SRCDIR)/device/profile.h $(SRCDIR)/device/hwreport.h $(SRCDIR)/srv/cc_main.h $(SRCDIR)/srv/cc_conf.h $(SRCDIR)/srv/cc_sensor.h $(SRCDIR)/mods/display.h $(SRCDIR)/mods/dual.h $(SRCDIR)/mods/circle.h $(SRCDIR)/mods/split.h
OBJECTS = $(patsubst $(SRCDIR)/%.c,$(OBJDIR)/%.o,$(SRC_MODULES))

MANIFEST = etc/coolercontrol/plugins/coolerdash/manifest.toml
README = README.md

# GNU standard install directories
prefix ?= /usr
exec_prefix ?= $(prefix)
bindir ?= $(exec_prefix)/bin
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
all: $(PROGRAM)

# Standard Build Target - Standard C99 project structure
$(PROGRAM): $(OBJECTS) $(MAIN_SOURCE) VERSION | $(BINDIR)
	@printf "$(CYAN)Compiling $(TARGET) (Standard C99 structure)...$(RESET)\n"
	@printf "$(BLUE)Structure:$(RESET) src/ include/ build/ bin/\n"
	@printf "$(BLUE)CPPFLAGS:$(RESET) $(CPPFLAGS)\n"
	@printf "$(BLUE)CFLAGS:$(RESET) $(CFLAGS)\n"
	@printf "$(BLUE)LDLIBS:$(RESET) $(LDLIBS)\n"
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $(MAIN_SOURCE) $(OBJECTS) $(LDLIBS)
	@printf "$(GREEN)Build successful: $@$(RESET)\n"

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
	rm -f $(PROGRAM) $(OBJECTS) *.o
	rm -rf $(OBJDIR) $(BINDIR)
	@printf "$(GREEN)Cleanup completed$(RESET)\n"

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
	elif grep -Eqi '^ID="?(ubuntu|debian)"?$$' /etc/os-release 2>/dev/null; then \
		echo "debian"; \
	elif grep -qi 'opensuse\|suse' /etc/os-release 2>/dev/null; then \
		echo "opensuse"; \
	elif [ -f /etc/redhat-release ]; then \
		echo "rhel"; \
	else \
		echo "unknown"; \
	fi

# Install build/runtime deps per distro
install-deps:
	@DISTRO=$$($(MAKE) --no-print-directory detect-distro); \
	case $$DISTRO in \
		arch) \
            printf "$(GREEN)Installing dependencies for Arch Linux/Manjaro...$(RESET)\n"; \
			$(SUDO) pacman -S --needed cairo curl gcc make pkg-config ttf-roboto jansson || { \
                printf "$(RED)Error installing dependencies!$(RESET)\n"; \
				printf "$(YELLOW)Please run manually:$(RESET) $(SUDO) pacman -S cairo curl gcc make pkg-config ttf-roboto jansson\n"; \
                exit 1; \
            }; \
            ;; \
        gentoo) \
            printf "$(GREEN)Installing dependencies for Gentoo...$(RESET)\n"; \
            $(SUDO) emerge --noreplace --oneshot sys-devel/gcc dev-build/make virtual/pkgconfig x11-libs/cairo net-misc/curl dev-libs/jansson media-fonts/roboto || { \
                printf "$(RED)Error installing dependencies!$(RESET)\n"; \
                printf "$(YELLOW)Please run manually:$(RESET) $(SUDO) emerge --noreplace --oneshot sys-devel/gcc dev-build/make virtual/pkgconfig x11-libs/cairo net-misc/curl dev-libs/jansson media-fonts/roboto\n"; \
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
			printf "  sudo pacman -S cairo curl gcc make pkg-config ttf-roboto jansson\n"; \
			printf "\n"; \
			printf "$(WHITE)Gentoo:$(RESET)\n"; \
			printf "  sudo emerge --noreplace --oneshot sys-devel/gcc dev-build/make virtual/pkgconfig x11-libs/cairo net-misc/curl dev-libs/jansson media-fonts/roboto\n"; \
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

# Install binary to /usr/libexec, plugin data to /var/lib/coolercontrol/plugins/coolerdash/
install:
	@printf "\n"
	@printf "$(WHITE)=== COOLERDASH INSTALLATION ===$(RESET)\n"
	@printf "\n"
	@if [ -z "$(DESTDIR)" ] && [ "$(REALOS)" = "yes" ] && [ "$$(id -u)" -ne 0 ]; then \
		printf "$(RED)Error: Installation requires root privileges$(RESET)\n"; \
		printf "$(YELLOW)Run: sudo make install$(RESET)\n"; \
		exit 1; \
	fi
	@if [ -z "$(DESTDIR)" ] && [ "$(REALOS)" = "yes" ]; then \
		$(MAKE) install-deps; \
	fi
	@$(MAKE) $(PROGRAM)
	@printf "$(CYAN)Installing plugin files...$(RESET)\n"
	@$(INSTALL) -d "$(DESTDIR)$(PLUGINDIR)"
	@$(INSTALL_PROGRAM) -D $(BINDIR)/$(TARGET) "$(DESTDIR)$(libexecdir)/coolerdash/coolerdash"
	@$(INSTALL) -d "$(DESTDIR)$(bindir)"
	@ln -sfn "../libexec/coolerdash/coolerdash" "$(DESTDIR)$(bindir)/coolerdash"
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
	@rm -f "$(DESTDIR)$(PLUGINDIR)/ui/update-status.js"
	@$(INSTALL_DATA) etc/coolercontrol/plugins/coolerdash/ui/index.html "$(DESTDIR)$(PLUGINDIR)/ui/index.html"
	@$(INSTALL_DATA) images/shutdown.png "$(DESTDIR)$(PLUGINDIR)/shutdown.png"
	@$(INSTALL_DATA) $(MANIFEST) "$(DESTDIR)$(PLUGINDIR)/manifest.toml"
	@sed -i 's/{{VERSION}}/$(VERSION)/g' "$(DESTDIR)$(PLUGINDIR)/manifest.toml"
	@sed -i 's/{{VERSION}}/$(VERSION)/g' "$(DESTDIR)$(PLUGINDIR)/ui/index.html"
	@printf "  $(GREEN)Binary:$(RESET)       $(DESTDIR)$(libexecdir)/coolerdash/coolerdash\n"
	@printf "  $(GREEN)CLI:$(RESET)          $(DESTDIR)$(bindir)/coolerdash\n"
	@printf "  $(GREEN)Config JSON:$(RESET)  $(DESTDIR)$(PLUGINDIR)/config.json (chmod 600)\n"
	@printf "  $(GREEN)Credentials:$(RESET) Runtime file preserved with chmod 600 when present\n"
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
	@if [ -z "$(DESTDIR)" ] && [ "$(REALOS)" = "yes" ]; then \
		if command -v systemctl >/dev/null 2>&1; then \
			if $(SUDO) systemctl is-active --quiet $(COOLERCONTROL_SERVICE).service; then \
				$(SUDO) systemctl restart $(COOLERCONTROL_SERVICE).service 2>/dev/null || true; \
			fi; \
		elif command -v rc-service >/dev/null 2>&1; then \
			for service in $(COOLERCONTROL_SERVICE) coolercontrol; do \
				if $(SUDO) rc-service $$service status >/dev/null 2>&1; then \
					$(SUDO) rc-service $$service restart >/dev/null 2>&1 || true; \
					break; \
				fi; \
			done; \
		fi; \
	fi
	@if command -v systemctl >/dev/null 2>&1; then \
		printf "  $(PURPLE)Restart CoolerControl:$(RESET) systemctl restart $(COOLERCONTROL_SERVICE).service\n"; \
	elif command -v rc-service >/dev/null 2>&1; then \
		printf "  $(PURPLE)Restart CoolerControl:$(RESET) rc-service coolercontrol restart\n"; \
	else \
		printf "  $(PURPLE)Restart CoolerControl:$(RESET) restart your CoolerControl daemon\n"; \
	fi
	@printf "  $(PURPLE)Plugin:$(RESET)         CoolerControl will manage coolerdash automatically\n"
	@printf "\n"

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
	@rm -f "$(DESTDIR)$(PLUGINDIR)/README.md"
	@rm -f "$(DESTDIR)$(PLUGINDIR)/CHANGELOG.md"
	@rm -f "$(DESTDIR)$(PLUGINDIR)/VERSION"
	@rm -f "$(DESTDIR)$(PLUGINDIR)/manifest.toml"
	@rm -f "$(DESTDIR)$(PLUGINDIR)/update-status.json"
	@rm -f "$(DESTDIR)$(PLUGINDIR)/shutdown.png"
	@rm -f "$(DESTDIR)$(PLUGINDIR)/ui/index.html"
	@rm -f "$(DESTDIR)$(PLUGINDIR)/ui/update-status.js"
	@rmdir "$(DESTDIR)$(PLUGINDIR)/ui" 2>/dev/null || true
	@if [ -f "$(DESTDIR)$(PLUGINDIR)/config.json" ]; then \
		backup="$(DESTDIR)$(PLUGINDIR)/config.json.manual-save"; \
		while [ -e "$$backup" ]; do backup="$$backup.old"; done; \
		mv "$(DESTDIR)$(PLUGINDIR)/config.json" "$$backup"; \
		printf "$(YELLOW)Configuration preserved as $$backup$(RESET)\n"; \
	fi
	@rm -f "$(DESTDIR)$(libexecdir)/coolerdash/coolerdash"
	@rmdir "$(DESTDIR)$(libexecdir)/coolerdash" 2>/dev/null || true
	@rm -f "$(DESTDIR)$(bindir)/coolerdash"
	@rm -f "$(DESTDIR)$(datarootdir)/licenses/coolerdash/LICENSE"
	@rmdir "$(DESTDIR)$(datarootdir)/licenses/coolerdash" 2>/dev/null || true
	@if [ -d "$(DESTDIR)$(PLUGINDIR)" ]; then \
		printf "$(YELLOW)Runtime data preserved in $(DESTDIR)$(PLUGINDIR)$(RESET)\n"; \
	fi
	@if [ -z "$(DESTDIR)" ] && [ "$(REALOS)" = "yes" ]; then \
		if command -v systemctl >/dev/null 2>&1; then \
			if $(SUDO) systemctl is-active --quiet $(COOLERCONTROL_SERVICE).service; then \
				$(SUDO) systemctl restart $(COOLERCONTROL_SERVICE).service >/dev/null 2>&1 || true; \
			fi; \
		elif command -v rc-service >/dev/null 2>&1; then \
			for service in $(COOLERCONTROL_SERVICE) coolercontrol; do \
				if $(SUDO) rc-service $$service status >/dev/null 2>&1; then \
					$(SUDO) rc-service $$service restart >/dev/null 2>&1 || true; \
					break; \
				fi; \
			done; \
		fi; \
	fi
	@printf "\n$(GREEN)Uninstallation completed successfully$(RESET)\n"
	@printf "\n"

# Debug Build
debug:
	@$(MAKE) clean
	@$(MAKE) DEBUG=1 $(PROGRAM)
	@printf "$(GREEN)Debug build created with AddressSanitizer: $(PROGRAM)$(RESET)\n"

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
	@printf "  $(GREEN)make debug$(RESET)        - Debug build with AddressSanitizer\n"
	@printf "\n"
	@printf "$(YELLOW)Installation:$(RESET)\n"
	@printf "  $(GREEN)make install-deps$(RESET)    - Installs required dependencies\n"
	@printf "  $(GREEN)sudo make install$(RESET)    - Installs dependencies, builds + installs\n"
	@printf "  $(GREEN)sudo make uninstall$(RESET) - Uninstalls and preserves user data\n"
	@printf "\n"
	@printf "$(YELLOW)Plugin Management:$(RESET)\n"
	@printf "  $(GREEN)systemctl enable --now $(COOLERCONTROL_SERVICE).service$(RESET)    - Start CoolerControl on systemd\n"
	@printf "  $(GREEN)rc-update add $(COOLERCONTROL_SERVICE) default$(RESET)       - Enable CoolerControl on OpenRC\n"
	@printf "  $(GREEN)rc-service $(COOLERCONTROL_SERVICE) start$(RESET)            - Start CoolerControl on OpenRC\n"
	@printf "  $(BLUE)Note:$(RESET) CoolerControl automatically manages coolerdash lifecycle\n"
	@printf "  $(BLUE)Shutdown:$(RESET) Plugin automatically displays shutdown.png when stopped\n"
	@printf "\n"
	@printf "$(YELLOW)Documentation:$(RESET)\n"
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
