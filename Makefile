.PHONY: clean install uninstall debug logs help detect-distro install-deps check-deps
VERSION := $(shell cat VERSION)

SUDO ?= sudo
REALOS ?= yes

CC = gcc
CFLAGS = -Wall -Wextra -O2 -std=c99 -march=x86-64-v3 -Iinclude $(shell pkg-config --cflags cairo jansson libcurl)
LIBS = $(shell pkg-config --libs cairo jansson libcurl) -lm
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

SERVICE = etc/systemd/coolerdash.service
MANIFEST = etc/coolercontrol/plugins/coolerdash/manifest.toml
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
	@printf "If you see errors about 'conflicting files' or manual installation, run 'make uninstall' and remove leftover files in /opt/coolerdash, /etc/coolerdash, /etc/systemd/system/coolerdash.service.\n\n"
	@printf "$(ICON_BUILD) $(CYAN)Compiling $(TARGET) (Standard C99 structure)...$(RESET)\n"
	@printf "$(BLUE)Structure:$(RESET) src/ include/ build/ bin/\n"
	@printf "$(BLUE)CFLAGS:$(RESET) $(CFLAGS)\n"
	@printf "$(BLUE)LIBS:$(RESET) $(LIBS)\n"
	$(CC) $(CFLAGS) -o $(BINDIR)/$(TARGET) $(MAIN_SOURCE) $(OBJECTS) $(LIBS)
	@printf "$(ICON_SUCCESS) $(GREEN)Build successful: $(BINDIR)/$(TARGET)$(RESET)\n"

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
            $(SUDO) pacman -S --needed cairo libcurl-gnutls gcc make pkg-config ttf-roboto jansson || { \
                printf "$(ICON_WARNING) $(RED)Error installing dependencies!$(RESET)\n"; \
                printf "$(YELLOW)Please run manually:$(RESET) $(SUDO) pacman -S cairo libcurl-gnutls gcc make pkg-config ttf-roboto jansson\n"; \
                exit 1; \
            }; \
            ;; \
        debian) \
            printf "$(ICON_INSTALL) $(GREEN)Installing dependencies for Ubuntu/Debian...$(RESET)\n"; \
            $(SUDO) apt update && $(SUDO) apt install -y libcairo2-dev libcurl4-openssl-dev gcc make pkg-config fonts-roboto libjansson-dev || { \
                printf "$(ICON_WARNING) $(RED)Error installing dependencies!$(RESET)\n"; \
                printf "$(YELLOW)Please run manually:$(RESET) $(SUDO) apt install libcairo2-dev libcurl4-openssl-dev gcc make pkg-config fonts-roboto libjansson-dev\n"; \
                exit 1; \
            }; \
            ;; \
        fedora) \
            printf "$(ICON_INSTALL) $(GREEN)Installing dependencies for Fedora...$(RESET)\n"; \
            $(SUDO) dnf install -y cairo-devel libcurl-devel gcc make pkg-config google-roboto-fonts jansson-devel || { \
                printf "$(ICON_WARNING) $(RED)Error installing dependencies!$(RESET)\n"; \
                printf "$(YELLOW)Please run manually:$(RESET) $(SUDO) dnf install cairo-devel libcurl-devel gcc make pkg-config google-roboto-fonts jansson-devel\n"; \
                exit 1; \
            }; \
            ;; \
        rhel) \
            printf "$(ICON_INSTALL) $(GREEN)Installing dependencies for RHEL/CentOS...$(RESET)\n"; \
            $(SUDO) yum install -y cairo-devel libcurl-devel gcc make pkg-config google-roboto-fonts jansson-devel || { \
                printf "$(ICON_WARNING) $(RED)Error installing dependencies!$(RESET)\n"; \
                printf "$(YELLOW)Please run manually:$(RESET) $(SUDO) yum install cairo-devel libcurl-devel gcc make pkg-config google-roboto-fonts jansson-devel\n"; \
                exit 1; \
            }; \
            ;; \
        opensuse) \
            printf "$(ICON_INSTALL) $(GREEN)Installing dependencies for openSUSE...$(RESET)\n"; \
            $(SUDO) zypper install -y cairo-devel libcurl-devel gcc make pkg-config google-roboto-fonts libjansson-devel || { \
                printf "$(ICON_WARNING) $(RED)Error installing dependencies!$(RESET)\n"; \
                printf "$(YELLOW)Please run manually:$(RESET) $(SUDO) zypper install cairo-devel libcurl-devel gcc make pkg-config google-roboto-fonts libjansson-devel\n"; \
                exit 1; \
            }; \
            ;; \
		*) \
			printf "$(ICON_WARNING) $(RED)Unknown distribution detected!$(RESET)\n"; \
			printf "\n"; \
			printf "$(YELLOW)Please install the following dependencies manually:$(RESET)\n"; \
			printf "\n"; \
			printf "$(WHITE)Arch Linux / Manjaro:$(RESET)\n"; \
			printf "  sudo pacman -S cairo libcurl-gnutls gcc make pkg-config ttf-roboto jansson\n"; \
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

# Check Dependencies for Installation (internal function called by install)
check-deps:
	@MISSING=""; \
	for dep in cairo libcurl jansson; do \
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

# Install Target - Installs to /etc/coolercontrol/plugins/coolerdash/ (Plugin-mode only, with migration cleanup)
install: check-deps $(TARGET)
	@printf "\n"
	@printf "$(ICON_INSTALL) $(WHITE)═══ COOLERDASH INSTALLATION ═══$(RESET)\n"
	@printf "\n"
	@if [ "$(REALOS)" = "yes" ]; then \
		printf "$(ICON_SERVICE) $(CYAN)Migration: Checking for legacy files and services...$(RESET)\n"; \
		LEGACY_FOUND=0; \
		if $(SUDO) systemctl is-active --quiet coolerdash.service 2>/dev/null; then \
			$(SUDO) systemctl stop coolerdash.service 2>/dev/null || true; \
			LEGACY_FOUND=1; \
		fi; \
		if $(SUDO) systemctl is-enabled --quiet coolerdash.service 2>/dev/null; then \
			$(SUDO) systemctl disable coolerdash.service 2>/dev/null || true; \
			LEGACY_FOUND=1; \
		fi; \
		if [ -f /etc/systemd/system/coolerdash.service ]; then \
			$(SUDO) rm -f /etc/systemd/system/coolerdash.service 2>/dev/null || true; \
			LEGACY_FOUND=1; \
		fi; \
		if [ -d /opt/coolerdash ]; then \
			$(SUDO) rm -rf /opt/coolerdash 2>/dev/null || true; \
			LEGACY_FOUND=1; \
		fi; \
		if [ -d /etc/coolerdash ]; then \
			$(SUDO) rm -rf /etc/coolerdash 2>/dev/null || true; \
			LEGACY_FOUND=1; \
		fi; \
		if [ -f /etc/coolercontrol/plugins/coolerdash/config.ini ]; then \
			$(SUDO) rm -f /etc/coolercontrol/plugins/coolerdash/config.ini 2>/dev/null || true; \
			LEGACY_FOUND=1; \
		fi; \
		if [ -f /etc/coolercontrol/plugins/coolerdash/ui.html ]; then \
			$(SUDO) rm -f /etc/coolercontrol/plugins/coolerdash/ui.html 2>/dev/null || true; \
			LEGACY_FOUND=1; \
		fi; \
		if [ -f /usr/share/applications/coolerdash-settings.desktop ]; then \
			$(SUDO) rm -f /usr/share/applications/coolerdash-settings.desktop 2>/dev/null || true; \
			LEGACY_FOUND=1; \
		fi; \
		if [ -L /bin/coolerdash ] || [ -f /bin/coolerdash ]; then \
			$(SUDO) rm -f /bin/coolerdash 2>/dev/null || true; \
			LEGACY_FOUND=1; \
		fi; \
		if [ -L /usr/bin/coolerdash ] || [ -f /usr/bin/coolerdash ]; then \
			$(SUDO) rm -f /usr/bin/coolerdash 2>/dev/null || true; \
			LEGACY_FOUND=1; \
		fi; \
		if id -u coolerdash &>/dev/null 2>&1; then \
			$(SUDO) userdel -rf coolerdash 2>/dev/null || true; \
			LEGACY_FOUND=1; \
		fi; \
		if [ "$$LEGACY_FOUND" -eq 1 ]; then \
			printf "  $(GREEN)✓$(RESET) Legacy cleanup complete\n" >/dev/null 2>&1; \
		else \
			printf "  $(BLUE)→$(RESET) No legacy files found (clean install)\n" >/dev/null 2>&1; \
		fi; \
		printf "\n" >/dev/null 2>&1; \
		COOLERDASH_COUNT=$$(pgrep -x coolerdash 2>/dev/null | wc -l); \
		if [ "$$COOLERDASH_COUNT" -gt 0 ]; then \
			printf "$(ICON_SERVICE) $(CYAN)Terminating running coolerdash process(es)...$(RESET)\n" >/dev/null 2>&1; \
			$(SUDO) killall -TERM coolerdash 2>/dev/null || true; \
			sleep 2; \
			REMAINING_COUNT=$$(pgrep -x coolerdash 2>/dev/null | wc -l); \
			if [ "$$REMAINING_COUNT" -gt 0 ]; then \
				printf "  $(YELLOW)→$(RESET) Force killing $$REMAINING_COUNT remaining process(es)...\n" >/dev/null 2>&1; \
				$(SUDO) killall -KILL coolerdash 2>/dev/null || true; \
			fi; \
			printf "  $(GREEN)→$(RESET) Processes terminated\n" >/dev/null 2>&1; \
			printf "\n" >/dev/null 2>&1; \
		fi; \
	else \
		printf "$(ICON_INFO) $(YELLOW)Migration skipped (CI environment).$(RESET)\n" >/dev/null 2>&1; \
	fi
	@printf "\n"
	@printf "$(ICON_INFO) $(CYAN)Installing plugin files to /etc/coolercontrol/plugins/coolerdash/...$(RESET)\n"
	@install -dm775 "$(DESTDIR)/etc/coolercontrol/plugins/coolerdash"
	@install -Dm755 $(BINDIR)/$(TARGET) "$(DESTDIR)/etc/coolercontrol/plugins/coolerdash/coolerdash"
	@install -Dm644 $(README) "$(DESTDIR)/etc/coolercontrol/plugins/coolerdash/README.md"
	@install -Dm644 LICENSE "$(DESTDIR)/etc/coolercontrol/plugins/coolerdash/LICENSE"
	@install -Dm644 CHANGELOG.md "$(DESTDIR)/etc/coolercontrol/plugins/coolerdash/CHANGELOG.md"
	@install -Dm644 VERSION "$(DESTDIR)/etc/coolercontrol/plugins/coolerdash/VERSION"
	@install -Dm644 etc/coolercontrol/plugins/coolerdash/config.json "$(DESTDIR)/etc/coolercontrol/plugins/coolerdash/config.json"
	@install -Dm644 etc/coolercontrol/plugins/coolerdash/index.html "$(DESTDIR)/etc/coolercontrol/plugins/coolerdash/index.html"
	@install -Dm644 images/shutdown.png "$(DESTDIR)/etc/coolercontrol/plugins/coolerdash/shutdown.png"
	@install -Dm644 $(MANIFEST) "$(DESTDIR)/etc/coolercontrol/plugins/coolerdash/manifest.toml"
	@# Substitute VERSION placeholder in manifest.toml during install
	@sed -i 's/{{VERSION}}/$(VERSION)/g' "$(DESTDIR)/etc/coolercontrol/plugins/coolerdash/manifest.toml"
	@printf "  $(GREEN)Binary:$(RESET)       $(DESTDIR)/etc/coolercontrol/plugins/coolerdash/coolerdash\n"
	@printf "  $(GREEN)Config JSON:$(RESET)  $(DESTDIR)/etc/coolercontrol/plugins/coolerdash/config.json\n"
	@printf "  $(GREEN)Web UI:$(RESET)       $(DESTDIR)/etc/coolercontrol/plugins/coolerdash/index.html\n"
	@printf "  $(GREEN)Plugin:$(RESET)       $(DESTDIR)/etc/coolercontrol/plugins/coolerdash/manifest.toml\n"
	@printf "  $(GREEN)Image:$(RESET)        shutdown.png (coolerdash.png)\n"
	@printf "  $(GREEN)Documentation:$(RESET) README.md, LICENSE, CHANGELOG.md, VERSION\n"
	@printf "\n"
	@printf "$(ICON_INFO) $(CYAN)Note: Plugin binary is available at /etc/coolercontrol/plugins/coolerdash/coolerdash$(RESET)\\n"
	@printf "\n"
	@printf "$(ICON_SERVICE) $(CYAN)Installing documentation...$(RESET)\n"
	@install -Dm644 $(MANPAGE) "$(DESTDIR)/usr/share/man/man1/coolerdash.1"
	@printf "  $(GREEN)Manual:$(RESET)  $(DESTDIR)/usr/share/man/man1/coolerdash.1\n"
	@printf "$(ICON_SERVICE) $(CYAN)Installing desktop shortcut...$(RESET)\n"
	@install -Dm644 etc/applications/coolerdash.desktop "$(DESTDIR)/usr/share/applications/coolerdash.desktop"
	@printf "  $(GREEN)Shortcut:$(RESET) $(DESTDIR)/usr/share/applications/coolerdash.desktop\n"
	@printf "$(ICON_SERVICE) $(CYAN)Installing icon...$(RESET)\n"
	@install -Dm644 etc/icons/coolerdash.svg "$(DESTDIR)/usr/share/icons/hicolor/scalable/apps/coolerdash.svg"
	@printf "  $(GREEN)Icon:$(RESET)     $(DESTDIR)/usr/share/icons/hicolor/scalable/apps/coolerdash.svg\n"
	@printf "\n"
	@printf "$(ICON_SUCCESS) $(WHITE)INSTALLATION SUCCESSFUL$(RESET)\n"
	@printf "\n"
	@printf "$(YELLOW)Next steps:$(RESET)\n"
	@$(SUDO) systemctl daemon-reload 2>/dev/null || true
	@$(SUDO) systemctl restart coolercontrold.service 2>/dev/null || true
	@printf "  $(PURPLE)Reload systemd:$(RESET) systemctl daemon-reload\n"
	@printf "  $(PURPLE)Restart CoolerControl:$(RESET) systemctl restart coolercontrold.service\n"
	@printf "  $(PURPLE)Plugin:$(RESET)         CoolerControl will manage coolerdash automatically\n"
	@printf "  $(PURPLE)Show manual:$(RESET)    man coolerdash\n"
	@printf "\n"

# Uninstall Target
uninstall:
	@if [ "$(REALOS)" = "yes" ]; then \
		LEGACY_FOUND=0; \
		if $(SUDO) systemctl is-active --quiet coolerdash.service 2>/dev/null; then \
			$(SUDO) systemctl stop coolerdash.service >/dev/null 2>&1 || true; \
			LEGACY_FOUND=1; \
		fi; \
		if $(SUDO) systemctl is-enabled --quiet coolerdash.service 2>/dev/null; then \
			$(SUDO) systemctl disable coolerdash.service >/dev/null 2>&1 || true; \
			LEGACY_FOUND=1; \
		fi; \
		if [ -f /etc/systemd/system/coolerdash.service ]; then \
			$(SUDO) rm -f /etc/systemd/system/coolerdash.service >/dev/null 2>&1 || true; \
			LEGACY_FOUND=1; \
		fi; \
		if [ -d /opt/coolerdash ]; then \
			$(SUDO) rm -rf /opt/coolerdash >/dev/null 2>&1 || true; \
			LEGACY_FOUND=1; \
		fi; \
		if [ -d /etc/coolerdash ]; then \
			$(SUDO) rm -rf /etc/coolerdash >/dev/null 2>&1 || true; \
			LEGACY_FOUND=1; \
		fi; \
		if [ -f /etc/coolercontrol/plugins/coolerdash/config.ini ]; then \
			$(SUDO) rm -f /etc/coolercontrol/plugins/coolerdash/config.ini >/dev/null 2>&1 || true; \
			LEGACY_FOUND=1; \
		fi; \
		if [ -f /etc/coolercontrol/plugins/coolerdash/ui.html ]; then \
			$(SUDO) rm -f /etc/coolercontrol/plugins/coolerdash/ui.html >/dev/null 2>&1 || true; \
			LEGACY_FOUND=1; \
		fi; \
		if [ -f /usr/share/applications/coolerdash-settings.desktop ]; then \
			$(SUDO) rm -f /usr/share/applications/coolerdash-settings.desktop 2>/dev/null || true; \
			LEGACY_FOUND=1; \
		fi; \
		if [ -L /bin/coolerdash ] || [ -f /bin/coolerdash ]; then \
			$(SUDO) rm -f /bin/coolerdash >/dev/null 2>&1 || true; \
			LEGACY_FOUND=1; \
		fi; \
		if [ -L /usr/bin/coolerdash ] || [ -f /usr/bin/coolerdash ]; then \
			$(SUDO) rm -f /usr/bin/coolerdash >/dev/null 2>&1 || true; \
			LEGACY_FOUND=1; \
		fi; \
		if id -u coolerdash &>/dev/null 2>&1; then \
			$(SUDO) userdel -rf coolerdash >/dev/null 2>&1 || true; \
			LEGACY_FOUND=1; \
		fi; \
	fi
	@$(SUDO) rm -rf /etc/coolercontrol/plugins/coolerdash >/dev/null 2>&1 || true
	@$(SUDO) rm -f /usr/share/man/man1/coolerdash.1 >/dev/null 2>&1 || true
	@$(SUDO) rm -f /usr/share/applications/coolerdash.desktop >/dev/null 2>&1 || true
	@$(SUDO) rm -f /usr/share/icons/hicolor/scalable/apps/coolerdash.svg >/dev/null 2>&1 || true
	@if id -u coolerdash &>/dev/null; then \
        $(SUDO) userdel -rf coolerdash >/dev/null 2>&1 || true; \
    fi
	@$(SUDO) mandb -q >/dev/null 2>&1 || true
	@$(SUDO) systemctl daemon-reload >/dev/null 2>&1 || true
	@$(SUDO) systemctl restart coolercontrold.service >/dev/null 2>&1 || true

# Debug Build
debug: CFLAGS += -g -DDEBUG -fsanitize=address
debug: LIBS += -fsanitize=address
debug: $(TARGET)
	@printf "$(ICON_SUCCESS) $(GREEN)Debug build created with AddressSanitizer: $(BINDIR)/$(TARGET)$(RESET)\n"

logs:
	@printf "$(ICON_INFO) $(CYAN)Live logs (Ctrl+C to exit):$(RESET)\n"
	journalctl -u cc-plugin-coolerdash.service -f

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
	@printf "  $(GREEN)make install$(RESET)  - Installs to /etc/coolercontrol/plugins/coolerdash/ as Plugin\n"
	@printf "  $(GREEN)make uninstall$(RESET)- Uninstalls the program\n"
	@printf "\n"
	@printf "$(YELLOW)⚙️  Plugin Management:$(RESET)\n"
	@printf "  $(GREEN)systemctl enable --now coolercontrold.service$(RESET)    - Active CoolerControl service\n"
	@printf "  $(GREEN)systemctl status coolercontrold.service$(RESET)   		- Shows CoolerControl status\n"
	@printf "  $(GREEN)journalctl -u coolercontrold.service -f$(RESET)   		- Shows live logs\n"
	@printf "  $(BLUE)Note:$(RESET) CoolerControl automatically manages coolerdash lifecycle\n"
	@printf "  $(BLUE)Shutdown:$(RESET) Plugin automatically displays shutdown.png when stopped\n"
	@printf "\n"
	@printf "$(YELLOW)📚 Documentation:$(RESET)\n"
	@printf "  $(GREEN)man coolerdash$(RESET) - Shows manual page\n"
	@printf "  $(GREEN)make help$(RESET)     - Shows this help\n"
	@printf "\n"
	@printf "$(YELLOW)🌍 README:$(RESET)\n"
	@printf "  $(GREEN)README.md$(RESET)         - 🇺🇸 English (main documentation)\n"
	@printf "\n"
	@printf "$(YELLOW)🔄 Version Usage:$(RESET)\n"
	@printf "  $(GREEN)Program:$(RESET) /etc/coolercontrol/plugins/coolerdash/coolerdash [mode]\n"
	@printf "  $(GREEN)Config:$(RESET)  /etc/coolercontrol/plugins/coolerdash/config.json\n"
	@printf "  $(GREEN)Web UI:$(RESET)  CoolerControl Plugin Settings\n"
	@printf "\n"
