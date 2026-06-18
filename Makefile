# Root Makefile for HyprGlassShell (HGS)
# Orchestrates building, installation, and systemd management

# Build configuration
BINARY_NAME=hgs
CORE_DIR=core
BUILD_DIR=$(CORE_DIR)/bin
PREFIX ?= /usr/local
INSTALL_DIR=$(PREFIX)/bin
DATA_DIR=$(PREFIX)/share
ICON_DIR=$(DATA_DIR)/icons/hicolor/scalable/apps

USER_HOME := $(if $(SUDO_USER),$(shell getent passwd $(SUDO_USER) | cut -d: -f6),$(HOME))
SYSTEMD_USER_DIR=$(USER_HOME)/.config/systemd/user

SHELL_DIR=quickshell
SHELL_INSTALL_DIR=$(DATA_DIR)/quickshell/hgs
ASSETS_DIR=assets
APPLICATIONS_DIR=$(DATA_DIR)/applications

.PHONY: all build clean lint-qml install install-bin install-shell install-completions install-systemd install-icon install-desktop uninstall uninstall-bin uninstall-shell uninstall-completions uninstall-systemd uninstall-icon uninstall-desktop help

all: build

build:
	@echo "Building $(BINARY_NAME)..."
	@$(MAKE) -C $(CORE_DIR) build
	@echo "Build complete"

clean:
	@echo "Cleaning build artifacts..."
	@$(MAKE) -C $(CORE_DIR) clean
	@echo "Clean complete"

lint-qml:
	@./quickshell/scripts/qmllint-entrypoints.sh

# Installation targets
install-bin:
	@echo "Installing $(BINARY_NAME) to $(INSTALL_DIR)..."
	@install -D -m 755 $(BUILD_DIR)/$(BINARY_NAME) $(INSTALL_DIR)/$(BINARY_NAME)
	@install -D -m 755 $(ASSETS_DIR)/hgs-terminal $(INSTALL_DIR)/hgs-terminal
	@echo "Binary installed"

install-shell:
	@echo "Installing shell files to $(SHELL_INSTALL_DIR)..."
	@mkdir -p $(SHELL_INSTALL_DIR)
	@cp -r $(SHELL_DIR)/* $(SHELL_INSTALL_DIR)/
	@rm -rf $(SHELL_INSTALL_DIR)/.git* $(SHELL_INSTALL_DIR)/.github
	@echo "Shell files installed"

install-completions:
	@echo "Installing shell completions..."
	@mkdir -p $(DATA_DIR)/bash-completion/completions
	@mkdir -p $(DATA_DIR)/zsh/site-functions
	@mkdir -p $(DATA_DIR)/fish/vendor_completions.d
	@$(BUILD_DIR)/$(BINARY_NAME) completion bash > $(DATA_DIR)/bash-completion/completions/hgs 2>/dev/null || true
	@$(BUILD_DIR)/$(BINARY_NAME) completion zsh > $(DATA_DIR)/zsh/site-functions/_hgs 2>/dev/null || true
	@$(BUILD_DIR)/$(BINARY_NAME) completion fish > $(DATA_DIR)/fish/vendor_completions.d/hgs.fish 2>/dev/null || true
	@echo "Shell completions installed"

install-systemd:
	@echo "Installing systemd user service..."
	@mkdir -p $(SYSTEMD_USER_DIR)
	@if [ -n "$(SUDO_USER)" ]; then chown -R $(SUDO_USER):"$(id -gn $SUDO_USER)" $(SYSTEMD_USER_DIR); fi
	@sed 's|/usr/bin/hgs|$(INSTALL_DIR)/hgs|g' $(ASSETS_DIR)/systemd/hgs.service > $(SYSTEMD_USER_DIR)/hgs.service
	@chmod 644 $(SYSTEMD_USER_DIR)/hgs.service
	@if [ -n "$(SUDO_USER)" ]; then chown $(SUDO_USER):"$(id -gn $SUDO_USER)" $(SYSTEMD_USER_DIR)/hgs.service; fi
	@echo "Systemd service installed to $(SYSTEMD_USER_DIR)/hgs.service"

install-icon:
	@echo "Installing icon..."
	@install -D -m 644 $(ASSETS_DIR)/hgslogo.svg $(ICON_DIR)/hgslogo.svg
	@gtk-update-icon-cache -q $(DATA_DIR)/icons/hicolor 2>/dev/null || true
	@echo "Icon installed"

install-desktop:
	@echo "Installing desktop entry..."
	@install -D -m 644 $(ASSETS_DIR)/hgs-open.desktop $(APPLICATIONS_DIR)/hgs-open.desktop
	@update-desktop-database -q $(APPLICATIONS_DIR) 2>/dev/null || true
	@echo "Desktop entry installed"

install: install-bin install-shell install-completions install-systemd install-icon install-desktop
	@echo ""
	@echo "Installation complete!"
	@echo ""
	@echo "=== Cheers, the HGS Team! ==="

# Uninstallation targets
uninstall-bin:
	@echo "Removing $(BINARY_NAME) from $(INSTALL_DIR)..."
	@rm -f $(INSTALL_DIR)/$(BINARY_NAME)
	@rm -f $(INSTALL_DIR)/hgs-terminal
	@echo "Binary removed"

uninstall-shell:
	@echo "Removing shell files from $(SHELL_INSTALL_DIR)..."
	@rm -rf $(SHELL_INSTALL_DIR)
	@echo "Shell files removed"

uninstall-completions:
	@echo "Removing shell completions..."
	@rm -f $(DATA_DIR)/bash-completion/completions/hgs
	@rm -f $(DATA_DIR)/zsh/site-functions/_hgs
	@rm -f $(DATA_DIR)/fish/vendor_completions.d/hgs.fish
	@echo "Shell completions removed"

uninstall-systemd:
	@echo "Removing systemd user service..."
	@rm -f $(SYSTEMD_USER_DIR)/hgs.service
	@echo "Systemd service removed"
	@echo "Note: Stop/disable service manually if running: systemctl --user stop hgs"

uninstall-icon:
	@echo "Removing icon..."
	@rm -f $(ICON_DIR)/hgslogo.svg
	@gtk-update-icon-cache -q $(DATA_DIR)/icons/hicolor 2>/dev/null || true
	@echo "Icon removed"

uninstall-desktop:
	@echo "Removing desktop entry..."
	@rm -f $(APPLICATIONS_DIR)/hgs-open.desktop
	@update-desktop-database -q $(APPLICATIONS_DIR) 2>/dev/null || true
	@echo "Desktop entry removed"

uninstall: uninstall-systemd uninstall-desktop uninstall-icon uninstall-completions uninstall-shell uninstall-bin
	@echo ""
	@echo "Uninstallation complete!"

# Target assist
help:
	@echo "Available targets:"
	@echo ""
	@echo "Build:"
	@echo "  all (default)        - Build the HGS binary"
	@echo "  build                - Same as 'all'"
	@echo "  clean                - Clean build artifacts"
	@echo "  lint-qml             - Run qmllint on shell entrypoints using the Quickshell tooling VFS"
	@echo ""
	@echo "Install:"
	@echo "  install              - Build and install everything (requires sudo)"
	@echo "  install-bin          - Install only the binary"
	@echo "  install-shell        - Install only shell files"
	@echo "  install-completions  - Install only shell completions"
	@echo "  install-systemd      - Install only systemd service"
	@echo "  install-icon         - Install only icon"
	@echo "  install-desktop      - Install only desktop entry"
	@echo ""
	@echo "Uninstall:"
	@echo "  uninstall            - Remove everything (requires sudo)"
	@echo "  uninstall-bin        - Remove only the binary"
	@echo "  uninstall-shell      - Remove only shell files"
	@echo "  uninstall-completions - Remove only shell completions"
	@echo "  uninstall-systemd    - Remove only systemd service"
	@echo "  uninstall-icon       - Remove only icon"
	@echo "  uninstall-desktop    - Remove only desktop entry"
	@echo ""
	@echo "Usage:"
	@echo "  sudo make install              - Build and install HGS"
	@echo "  sudo make uninstall            - Remove HGS"
	@echo "  systemctl --user enable --now hgs  - Enable and start service"
