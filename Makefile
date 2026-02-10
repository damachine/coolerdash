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

# Standard Build Target - Standard C99 project structure
$(TARGET): $(OBJDIR) $(BINDIR) $(OBJECTS) $(MAIN_SOURCE)
	@printf "\n$(PURPLE)Manual Installation Check:$(RESET)\n"
	@printf "If you see errors about 'conflicting files' or manual installation, run 'make uninstall' and remove leftover files in /opt/coolerdash, /etc/coolerdash, /etc/systemd/system/coolerdash.service.\n\n"
	@printf "$(CYAN)Compiling $(TARGET) (Standard C99 structure)...$(RESET)\n"
	@printf "$(BLUE)Structure:$(RESET) src/ include/ build/ bin/\n"
	@printf "$(BLUE)CFLAGS:$(RESET) $(CFLAGS)\n"
	@printf "$(BLUE)LIBS:$(RESET) $(LIBS)\n"
	$(CC) $(CFLAGS) -o $(BINDIR)/$(TARGET) $(MAIN_SOURCE) $(OBJECTS) $(LIBS)
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
	@printf "$(YELLOW)Cleaning up...$(RESET)\n"
	rm -f $(BINDIR)/$(TARGET) $(OBJECTS) *.o
	rm -rf $(OBJDIR) $(BINDIR)
	@printf "$(GREEN)Cleanup completed$(RESET)\n"

# Detect Linux distro via release files, os-release as fallback
detect-distro:
	@if [ -f /etc/arch-release ]; then \
		echo "arch"; \
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

# Install binary to /usr/libexec, plugin data to /etc/coolercontrol/plugins/coolerdash/
install: check-deps $(TARGET)
	@printf "\n"
	@printf "$(WHITE)=== COOLERDASH INSTALLATION ===$(RESET)\n"
	@printf "\n"
	@if [ "$(REALOS)" = "yes" ]; then \
		printf "$(CYAN)Migration: Checking for legacy files and services...$(RESET)\n"; \
		LEGACY_FOUND=0; \
		if $(SUDO) systemctl is-active --quiet coolerdash.service 2>/dev/null; then \
			$(SUDO) systemctl stop coolerdash.service; \
			LEGACY_FOUND=1; \
		fi; \
		if $(SUDO) systemctl is-enabled --quiet coolerdash.service 2>/dev/null; then \
			$(SUDO) systemctl disable coolerdash.service; \
			LEGACY_FOUND=1; \
		fi; \
		if [ -f /etc/systemd/system/coolerdash.service ]; then \
			$(SUDO) rm -f /etc/systemd/system/coolerdash.service; \
			LEGACY_FOUND=1; \
		fi; \
		if [ -d /opt/coolerdash ]; then \
			$(SUDO) rm -rf /opt/coolerdash; \
			LEGACY_FOUND=1; \
		fi; \
		if [ -d /etc/coolerdash ]; then \
			$(SUDO) rm -rf /etc/coolerdash; \
			LEGACY_FOUND=1; \
		fi; \
		if [ -f /etc/coolercontrol/plugins/coolerdash/config.ini ]; then \
			$(SUDO) rm -f /etc/coolercontrol/plugins/coolerdash/config.ini; \
			LEGACY_FOUND=1; \
		fi; \
		if [ -f /etc/coolercontrol/plugins/coolerdash/ui.html ]; then \
			$(SUDO) rm -f /etc/coolercontrol/plugins/coolerdash/ui.html; \
			LEGACY_FOUND=1; \
		fi; \
		if [ -f /etc/coolercontrol/plugins/coolerdash/LICENSE ]; then \
			$(SUDO) rm -f /etc/coolercontrol/plugins/coolerdash/LICENSE; \
			LEGACY_FOUND=1; \
		fi; \
		if [ -f /etc/coolercontrol/plugins/coolerdash/coolerdash ]; then \
			$(SUDO) rm -f /etc/coolercontrol/plugins/coolerdash/coolerdash; \
			LEGACY_FOUND=1; \
		fi; \
		if [ -f /usr/share/applications/coolerdash-settings.desktop ]; then \
			$(SUDO) rm -f /usr/share/applications/coolerdash-settings.desktop; \
			LEGACY_FOUND=1; \
		fi; \
		if [ -L /bin/coolerdash ] || [ -f /bin/coolerdash ]; then \
			$(SUDO) rm -f /bin/coolerdash; \
			LEGACY_FOUND=1; \
		fi; \
		if [ -L /usr/bin/coolerdash ] || [ -f /usr/bin/coolerdash ]; then \
			$(SUDO) rm -f /usr/bin/coolerdash; \
			LEGACY_FOUND=1; \
		fi; \
		if id -u coolerdash >/dev/null 2>&1; then \
			$(SUDO) userdel -rf coolerdash; \
			LEGACY_FOUND=1; \
		fi; \
		if [ "$$LEGACY_FOUND" -eq 1 ]; then \
			printf "  $(GREEN)OK$(RESET) Legacy cleanup complete\n"; \
		else \
			printf "  $(BLUE)->$(RESET) No legacy files found (clean install)\n"; \
		fi; \
		printf "\n"; \
		COOLERDASH_COUNT=$$(pgrep -x coolerdash 2>/dev/null | wc -l); \
		if [ "$$COOLERDASH_COUNT" -gt 0 ]; then \
			printf "$(CYAN)Terminating running coolerdash process(es)...$(RESET)\n"; \
			$(SUDO) killall -TERM coolerdash 2>/dev/null || true; \
			sleep 2; \
			REMAINING_COUNT=$$(pgrep -x coolerdash 2>/dev/null | wc -l); \
			if [ "$$REMAINING_COUNT" -gt 0 ]; then \
				printf "  $(YELLOW)->$(RESET) Force killing $$REMAINING_COUNT remaining process(es)...\n"; \
				$(SUDO) killall -KILL coolerdash 2>/dev/null || true; \
			fi; \
			printf "  $(GREEN)->$(RESET) Processes terminated\n"; \
			printf "\n"; \
		fi; \
	else \
		printf "$(YELLOW)Migration skipped (CI environment).$(RESET)\n"; \
	fi
	@printf "\n"
	@printf "$(CYAN)Installing plugin files...$(RESET)\n"
	@install -dm755 "$(DESTDIR)/etc/coolercontrol/plugins/coolerdash"
	@install -Dm755 $(BINDIR)/$(TARGET) "$(DESTDIR)/usr/libexec/coolerdash/coolerdash"
	@install -m644 $(README) "$(DESTDIR)/etc/coolercontrol/plugins/coolerdash/README.md"
	@install -m644 CHANGELOG.md "$(DESTDIR)/etc/coolercontrol/plugins/coolerdash/CHANGELOG.md"
	@install -m644 VERSION "$(DESTDIR)/etc/coolercontrol/plugins/coolerdash/VERSION"
	@install -m666 etc/coolercontrol/plugins/coolerdash/config.json "$(DESTDIR)/etc/coolercontrol/plugins/coolerdash/config.json"
	@install -dm755 "$(DESTDIR)/etc/coolercontrol/plugins/coolerdash/ui"
	@install -m644 etc/coolercontrol/plugins/coolerdash/ui/index.html "$(DESTDIR)/etc/coolercontrol/plugins/coolerdash/ui/index.html"
	@install -m644 etc/coolercontrol/plugins/coolerdash/ui/cc-plugin-lib.js "$(DESTDIR)/etc/coolercontrol/plugins/coolerdash/ui/cc-plugin-lib.js"
	@install -m644 images/shutdown.png "$(DESTDIR)/etc/coolercontrol/plugins/coolerdash/shutdown.png"
	@install -m644 $(MANIFEST) "$(DESTDIR)/etc/coolercontrol/plugins/coolerdash/manifest.toml"
	@sed -i 's/{{VERSION}}/$(VERSION)/g' "$(DESTDIR)/etc/coolercontrol/plugins/coolerdash/manifest.toml"
	@sed -i 's/{{VERSION}}/$(VERSION)/g' "$(DESTDIR)/etc/coolercontrol/plugins/coolerdash/ui/index.html"
	@install -Dm644 etc/systemd/cc-plugin-coolerdash.service.d/startup-delay.conf "$(DESTDIR)/etc/systemd/system/cc-plugin-coolerdash.service.d/startup-delay.conf"
	@install -Dm644 etc/systemd/coolerdash-helperd.service "$(DESTDIR)/usr/lib/systemd/system/coolerdash-helperd.service"
	@printf "  $(GREEN)Drop-in:$(RESET)       $(DESTDIR)/etc/systemd/system/cc-plugin-coolerdash.service.d/startup-delay.conf\n"
	@printf "  $(GREEN)Service:$(RESET)       $(DESTDIR)/usr/lib/systemd/system/coolerdash-helperd.service\n"
	@printf "  $(GREEN)Binary:$(RESET)       $(DESTDIR)/usr/libexec/coolerdash/coolerdash\n"
	@printf "  $(GREEN)Config JSON:$(RESET)  $(DESTDIR)/etc/coolercontrol/plugins/coolerdash/config.json\n"
	@printf "  $(GREEN)Web UI:$(RESET)       $(DESTDIR)/etc/coolercontrol/plugins/coolerdash/ui/index.html\n"
	@printf "  $(GREEN)Plugin Lib:$(RESET)   $(DESTDIR)/etc/coolercontrol/plugins/coolerdash/ui/cc-plugin-lib.js\n"
	@printf "  $(GREEN)Plugin:$(RESET)       $(DESTDIR)/etc/coolercontrol/plugins/coolerdash/manifest.toml\n"
	@printf "  $(GREEN)Image:$(RESET)        shutdown.png (coolerdash.png)\n"
	@printf "  $(GREEN)Documentation:$(RESET) README.md, LICENSE, CHANGELOG.md, VERSION\n"
	@printf "\n"
	@printf "$(CYAN)Note: Plugin binary is available at /usr/libexec/coolerdash/coolerdash$(RESET)\\n"
	@printf "\n"
	@printf "$(CYAN)Installing documentation...$(RESET)\n"
	@install -Dm644 $(MANPAGE) "$(DESTDIR)/usr/share/man/man1/coolerdash.1"
	@printf "  $(GREEN)Manual:$(RESET)  $(DESTDIR)/usr/share/man/man1/coolerdash.1\n"
	@printf "$(CYAN)Installing license...$(RESET)\n"
	@install -Dm644 LICENSE "$(DESTDIR)/usr/share/licenses/coolerdash/LICENSE"
	@printf "  $(GREEN)License:$(RESET) $(DESTDIR)/usr/share/licenses/coolerdash/LICENSE\n"
	@printf "$(CYAN)Installing desktop shortcut...$(RESET)\n"
	@install -Dm644 etc/applications/coolerdash.desktop "$(DESTDIR)/usr/share/applications/coolerdash.desktop"
	@printf "  $(GREEN)Shortcut:$(RESET) $(DESTDIR)/usr/share/applications/coolerdash.desktop\n"
	@printf "$(CYAN)Installing icon...$(RESET)\n"
	@install -Dm644 etc/icons/coolerdash.svg "$(DESTDIR)/usr/share/icons/hicolor/scalable/apps/coolerdash.svg"
	@printf "  $(GREEN)Icon:$(RESET)     $(DESTDIR)/usr/share/icons/hicolor/scalable/apps/coolerdash.svg\n"
	@printf "$(CYAN)Installing udev rules for USB power management...$(RESET)\n"
	@install -Dm644 etc/udev/rules.d/99-coolerdash.rules "$(DESTDIR)/usr/lib/udev/rules.d/99-coolerdash.rules"
	@printf "  $(GREEN)udev rule:$(RESET) $(DESTDIR)/usr/lib/udev/rules.d/99-coolerdash.rules\n"
	@if [ "$(REALOS)" = "yes" ]; then \
		$(SUDO) udevadm control --reload-rules 2>/dev/null || true; \
		$(SUDO) udevadm trigger --subsystem-match=usb 2>/dev/null || true; \
		printf "  $(GREEN)OK$(RESET) USB power management configured\n"; \
	fi
	@printf "\n"
	@printf "$(WHITE)INSTALLATION SUCCESSFUL$(RESET)\n"
	@printf "\n"
	@printf "$(YELLOW)Next steps:$(RESET)\n"
	@if [ "$(REALOS)" = "yes" ]; then \
		$(SUDO) systemctl daemon-reload 2>/dev/null || true; \
		$(SUDO) systemctl enable --now coolerdash-helperd.service 2>/dev/null || true; \
		$(SUDO) systemctl restart coolercontrold.service 2>/dev/null || true; \
	fi
	@printf "  $(PURPLE)Reload systemd:$(RESET) systemctl daemon-reload\n"
	@printf "  $(PURPLE)Restart CoolerControl:$(RESET) systemctl restart coolercontrold.service\n"
	@printf "  $(PURPLE)Plugin:$(RESET)         CoolerControl will manage coolerdash automatically\n"
	@printf "  $(PURPLE)Show manual:$(RESET)    man coolerdash\n"
	@printf "\n"

# Uninstall Target
uninstall:
	@printf "\n"
	@printf "$(WHITE)=== COOLERDASH UNINSTALLATION ===$(RESET)\n"
	@printf "\n"
	@if [ "$(REALOS)" = "yes" ]; then \
		printf "$(CYAN)Stopping and disabling services...$(RESET)\n"; \
		$(SUDO) systemctl stop cc-plugin-coolerdash.service >/dev/null 2>&1 || true; \
		$(SUDO) systemctl disable cc-plugin-coolerdash.service >/dev/null 2>&1 || true; \
	fi
	@if [ "$(REALOS)" = "yes" ]; then \
		LEGACY_FOUND=0; \
		if $(SUDO) systemctl is-active --quiet coolerdash.service 2>/dev/null; then \
			$(SUDO) systemctl stop coolerdash.service; \
			LEGACY_FOUND=1; \
		fi; \
		if $(SUDO) systemctl is-enabled --quiet coolerdash.service 2>/dev/null; then \
			$(SUDO) systemctl disable coolerdash.service; \
			LEGACY_FOUND=1; \
		fi; \
		if [ -f /etc/systemd/system/coolerdash.service ]; then \
			$(SUDO) rm -f /etc/systemd/system/coolerdash.service; \
			LEGACY_FOUND=1; \
		fi; \
		if [ -d /opt/coolerdash ]; then \
			$(SUDO) rm -rf /opt/coolerdash; \
			LEGACY_FOUND=1; \
		fi; \
		if [ -d /etc/coolerdash ]; then \
			$(SUDO) rm -rf /etc/coolerdash; \
			LEGACY_FOUND=1; \
		fi; \
		if [ -f /etc/coolercontrol/plugins/coolerdash/config.ini ]; then \
			$(SUDO) rm -f /etc/coolercontrol/plugins/coolerdash/config.ini; \
			LEGACY_FOUND=1; \
		fi; \
		if [ -f /etc/coolercontrol/plugins/coolerdash/ui.html ]; then \
			$(SUDO) rm -f /etc/coolercontrol/plugins/coolerdash/ui.html; \
			LEGACY_FOUND=1; \
		fi; \
		if [ -f /etc/coolercontrol/plugins/coolerdash/LICENSE ]; then \
			$(SUDO) rm -f /etc/coolercontrol/plugins/coolerdash/LICENSE; \
			LEGACY_FOUND=1; \
		fi; \
		if [ -f /etc/coolercontrol/plugins/coolerdash/coolerdash ]; then \
			$(SUDO) rm -f /etc/coolercontrol/plugins/coolerdash/coolerdash; \
			LEGACY_FOUND=1; \
		fi; \
		if [ -f /usr/share/applications/coolerdash-settings.desktop ]; then \
			$(SUDO) rm -f /usr/share/applications/coolerdash-settings.desktop; \
			LEGACY_FOUND=1; \
		fi; \
		if [ -L /bin/coolerdash ] || [ -f /bin/coolerdash ]; then \
			$(SUDO) rm -f /bin/coolerdash; \
			LEGACY_FOUND=1; \
		fi; \
		if [ -L /usr/bin/coolerdash ] || [ -f /usr/bin/coolerdash ]; then \
			$(SUDO) rm -f /usr/bin/coolerdash; \
			LEGACY_FOUND=1; \
		fi; \
		if id -u coolerdash >/dev/null 2>&1; then \
			$(SUDO) userdel -rf coolerdash; \
			LEGACY_FOUND=1; \
		fi; \
	fi
	@if [ "$(REALOS)" = "yes" ]; then \
		$(SUDO) rm -f /etc/systemd/system/coolerdash-helperd.service; \
	fi
	@$(SUDO) rm -f "$(DESTDIR)/usr/lib/systemd/system/coolerdash-helperd.service"
	@$(SUDO) rm -rf "$(DESTDIR)/etc/systemd/system/cc-plugin-coolerdash.service.d"
	@$(SUDO) rm -rf "$(DESTDIR)/etc/coolercontrol/plugins/coolerdash"
	@$(SUDO) rm -rf "$(DESTDIR)/usr/libexec/coolerdash"
	@$(SUDO) rm -rf "$(DESTDIR)/usr/share/licenses/coolerdash"
	@$(SUDO) rm -f "$(DESTDIR)/usr/share/man/man1/coolerdash.1"
	@$(SUDO) rm -f "$(DESTDIR)/usr/share/applications/coolerdash.desktop"
	@printf "$(CYAN)Removing udev rule...$(RESET)\n"
	@$(SUDO) rm -f "$(DESTDIR)/usr/lib/udev/rules.d/99-coolerdash.rules"
	@if [ "$(REALOS)" = "yes" ]; then \
		$(SUDO) udevadm control --reload-rules 2>/dev/null || true; \
		$(SUDO) udevadm trigger --subsystem-match=usb 2>/dev/null || true; \
		printf "  $(GREEN)OK$(RESET) udev rules reloaded\n"; \
	fi
	@$(SUDO) rm -f "$(DESTDIR)/usr/share/icons/hicolor/scalable/apps/coolerdash.svg"
	@if [ "$(REALOS)" = "yes" ]; then \
		if id -u coolerdash >/dev/null 2>&1; then \
			$(SUDO) userdel -rf coolerdash; \
		fi; \
		$(SUDO) mandb -q >/dev/null 2>&1 || true; \
		$(SUDO) systemctl daemon-reload >/dev/null 2>&1 || true; \
		$(SUDO) systemctl restart coolercontrold.service >/dev/null 2>&1 || true; \
	fi
	@printf "\n$(GREEN)Uninstallation completed successfully$(RESET)\n"
	@printf "\n"

# Debug Build
debug: CFLAGS += -g -DDEBUG -fsanitize=address
debug: LIBS += -fsanitize=address
debug: $(TARGET)
	@printf "$(GREEN)Debug build created with AddressSanitizer: $(BINDIR)/$(TARGET)$(RESET)\n"

logs:
	@printf "$(CYAN)Live logs (Ctrl+C to exit):$(RESET)\n"
	journalctl -u cc-plugin-coolerdash.service -f

# Help
help:
	@printf "\n"
	@printf "$(WHITE)========================================$(RESET)\n"
	@printf "$(WHITE)         COOLERDASH BUILD SYSTEM        $(RESET)\n"
	@printf "$(WHITE)========================================$(RESET)\n"
	@printf "\n"
	@printf "$(YELLOW)Build Targets:$(RESET)\n"
	@printf "  $(GREEN)make$(RESET)          - Compiles the program\n"
	@printf "  $(GREEN)make clean$(RESET)    - Removes compiled files\n"
	@printf "  $(GREEN)make debug$(RESET)    - Debug build with AddressSanitizer\n"
	@printf "\n"
	@printf "$(YELLOW)Installation:$(RESET)\n"
	@printf "  $(GREEN)make install$(RESET)  - Installs binary + plugin data + systemd units\n"
	@printf "  $(GREEN)make uninstall$(RESET)- Uninstalls the program\n"
	@printf "\n"
	@printf "$(YELLOW)Plugin Management:$(RESET)\n"
	@printf "  $(GREEN)systemctl enable --now coolercontrold.service$(RESET)    - Active CoolerControl service\n"
	@printf "  $(GREEN)systemctl status coolercontrold.service$(RESET)   		- Shows CoolerControl status\n"
	@printf "  $(GREEN)journalctl -u coolercontrold.service -f$(RESET)   		- Shows live logs\n"
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
	@printf "  $(GREEN)Program:$(RESET) /usr/libexec/coolerdash/coolerdash [mode]\n"
	@printf "  $(GREEN)Config:$(RESET)  /etc/coolercontrol/plugins/coolerdash/config.json\n"
	@printf "  $(GREEN)Web UI:$(RESET)  CoolerControl Plugin Settings\n"
	@printf "\n"
