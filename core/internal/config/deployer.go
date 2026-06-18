package config

import (
	"context"
	"fmt"
	"os"
	"path/filepath"
	"strings"
	"time"

	"github.com/CoastLineSec/HyprGlassShell/core/internal/deps"
)

const hyprlandBackupDirName = ".hgs-backups"

type ConfigDeployer struct {
	logChan chan<- string
}

type DeploymentResult struct {
	ConfigType string
	Path       string
	BackupPath string
	Deployed   bool
	Error      error
}

func NewConfigDeployer(logChan chan<- string) *ConfigDeployer {
	return &ConfigDeployer{
		logChan: logChan,
	}
}

func (cd *ConfigDeployer) log(message string) {
	if cd.logChan != nil {
		cd.logChan <- message
	}
}

// DeployConfigurations deploys all necessary configurations based on the chosen window manager
func (cd *ConfigDeployer) DeployConfigurations(ctx context.Context, wm deps.WindowManager) ([]DeploymentResult, error) {
	return cd.DeployConfigurationsWithTerminal(ctx, wm, deps.TerminalGhostty)
}

// DeployConfigurationsWithTerminal deploys all necessary configurations based on chosen window manager and terminal
func (cd *ConfigDeployer) DeployConfigurationsWithTerminal(ctx context.Context, wm deps.WindowManager, terminal deps.Terminal) ([]DeploymentResult, error) {
	return cd.DeployConfigurationsSelective(ctx, wm, terminal, nil, nil)
}

// DeployConfigurationsWithSystemd deploys configurations with systemd option
func (cd *ConfigDeployer) DeployConfigurationsWithSystemd(ctx context.Context, wm deps.WindowManager, terminal deps.Terminal, useSystemd bool) ([]DeploymentResult, error) {
	return cd.deployConfigurationsInternal(ctx, wm, terminal, nil, nil, nil, useSystemd)
}

func (cd *ConfigDeployer) DeployConfigurationsSelective(ctx context.Context, wm deps.WindowManager, terminal deps.Terminal, installedDeps []deps.Dependency, replaceConfigs map[string]bool) ([]DeploymentResult, error) {
	return cd.DeployConfigurationsSelectiveWithReinstalls(ctx, wm, terminal, installedDeps, replaceConfigs, nil)
}

func (cd *ConfigDeployer) DeployConfigurationsSelectiveWithReinstalls(ctx context.Context, wm deps.WindowManager, terminal deps.Terminal, installedDeps []deps.Dependency, replaceConfigs map[string]bool, reinstallItems map[string]bool) ([]DeploymentResult, error) {
	return cd.deployConfigurationsInternal(ctx, wm, terminal, installedDeps, replaceConfigs, reinstallItems, true)
}

func (cd *ConfigDeployer) deployConfigurationsInternal(ctx context.Context, wm deps.WindowManager, terminal deps.Terminal, installedDeps []deps.Dependency, replaceConfigs map[string]bool, reinstallItems map[string]bool, useSystemd bool) ([]DeploymentResult, error) {
	var results []DeploymentResult

	// Primary config file paths used to detect fresh installs.
	configPrimaryPaths := map[string][]string{
		"Hyprland": {
			filepath.Join(os.Getenv("HOME"), ".config", "hypr", "hyprland.lua"),
			filepath.Join(os.Getenv("HOME"), ".config", "hypr", "hyprland.conf"),
		},
		"Ghostty": {
			filepath.Join(os.Getenv("HOME"), ".config", "ghostty", "config"),
		},
		"Kitty": {
			filepath.Join(os.Getenv("HOME"), ".config", "kitty", "kitty.conf"),
		},
		"Alacritty": {
			filepath.Join(os.Getenv("HOME"), ".config", "alacritty", "alacritty.toml"),
		},
	}

	shouldReplaceConfig := func(configType string) bool {
		if replaceConfigs == nil {
			return true
		}
		replace, exists := replaceConfigs[configType]
		if !exists || replace {
			return true
		}
		// Config is explicitly set to "don't replace" — but still deploy
		// if the config file doesn't exist yet (fresh install scenario).
		if primaryPaths, ok := configPrimaryPaths[configType]; ok {
			exists := false
			for _, primaryPath := range primaryPaths {
				if _, err := os.Stat(primaryPath); err == nil {
					exists = true
					break
				}
			}
			if !exists {
				return true
			}
		}
		return false
	}

	switch wm {
	case deps.WindowManagerHyprland:
		if shouldReplaceConfig("Hyprland") {
			result, err := cd.deployHyprlandConfig(terminal, useSystemd)
			results = append(results, result)
			if err != nil {
				return results, fmt.Errorf("failed to deploy Hyprland config: %w", err)
			}
		}
	default:
		return results, fmt.Errorf("unsupported compositor: HyprGlassShell currently supports Hyprland only")
	}

	switch terminal {
	case deps.TerminalGhostty:
		if shouldReplaceConfig("Ghostty") {
			ghosttyResults, err := cd.deployGhosttyConfig()
			results = append(results, ghosttyResults...)
			if err != nil {
				return results, fmt.Errorf("failed to deploy Ghostty config: %w", err)
			}
		}
	case deps.TerminalKitty:
		if shouldReplaceConfig("Kitty") {
			kittyResults, err := cd.deployKittyConfig()
			results = append(results, kittyResults...)
			if err != nil {
				return results, fmt.Errorf("failed to deploy Kitty config: %w", err)
			}
		}
	case deps.TerminalAlacritty:
		if shouldReplaceConfig("Alacritty") {
			alacrittyResults, err := cd.deployAlacrittyConfig()
			results = append(results, alacrittyResults...)
			if err != nil {
				return results, fmt.Errorf("failed to deploy Alacritty config: %w", err)
			}
		}
	}

	return results, nil
}

func (cd *ConfigDeployer) deployGhosttyConfig() ([]DeploymentResult, error) {
	var results []DeploymentResult

	mainResult := DeploymentResult{
		ConfigType: "Ghostty",
		Path:       filepath.Join(os.Getenv("HOME"), ".config", "ghostty", "config"),
	}

	configDir := filepath.Dir(mainResult.Path)
	if err := os.MkdirAll(configDir, 0o755); err != nil {
		mainResult.Error = fmt.Errorf("failed to create config directory: %w", err)
		return []DeploymentResult{mainResult}, mainResult.Error
	}

	if _, err := os.Stat(mainResult.Path); err == nil {
		cd.log("Found existing Ghostty configuration")

		existingData, err := os.ReadFile(mainResult.Path)
		if err != nil {
			mainResult.Error = fmt.Errorf("failed to read existing config: %w", err)
			return []DeploymentResult{mainResult}, mainResult.Error
		}

		timestamp := time.Now().Format("2006-01-02_15-04-05")
		mainResult.BackupPath = mainResult.Path + ".backup." + timestamp
		if err := os.WriteFile(mainResult.BackupPath, existingData, 0o644); err != nil {
			mainResult.Error = fmt.Errorf("failed to create backup: %w", err)
			return []DeploymentResult{mainResult}, mainResult.Error
		}
		cd.log(fmt.Sprintf("Backed up existing config to %s", mainResult.BackupPath))
	}

	if err := os.WriteFile(mainResult.Path, []byte(GhosttyConfig), 0o644); err != nil {
		mainResult.Error = fmt.Errorf("failed to write config: %w", err)
		return []DeploymentResult{mainResult}, mainResult.Error
	}

	mainResult.Deployed = true
	cd.log("Successfully deployed Ghostty configuration")
	results = append(results, mainResult)

	colorResult := DeploymentResult{
		ConfigType: "Ghostty Colors",
		Path:       filepath.Join(os.Getenv("HOME"), ".config", "ghostty", "themes", "hgscolors"),
	}

	themesDir := filepath.Dir(colorResult.Path)
	if err := os.MkdirAll(themesDir, 0o755); err != nil {
		mainResult.Error = fmt.Errorf("failed to create themes directory: %w", err)
		return []DeploymentResult{mainResult}, mainResult.Error
	}

	if err := os.WriteFile(colorResult.Path, []byte(GhosttyColorConfig), 0o644); err != nil {
		colorResult.Error = fmt.Errorf("failed to write color config: %w", err)
		return results, colorResult.Error
	}

	colorResult.Deployed = true
	cd.log("Successfully deployed Ghostty color configuration")
	results = append(results, colorResult)

	return results, nil
}

func (cd *ConfigDeployer) deployKittyConfig() ([]DeploymentResult, error) {
	var results []DeploymentResult

	mainResult := DeploymentResult{
		ConfigType: "Kitty",
		Path:       filepath.Join(os.Getenv("HOME"), ".config", "kitty", "kitty.conf"),
	}

	configDir := filepath.Dir(mainResult.Path)
	if err := os.MkdirAll(configDir, 0o755); err != nil {
		mainResult.Error = fmt.Errorf("failed to create config directory: %w", err)
		return []DeploymentResult{mainResult}, mainResult.Error
	}

	if _, err := os.Stat(mainResult.Path); err == nil {
		cd.log("Found existing Kitty configuration")

		existingData, err := os.ReadFile(mainResult.Path)
		if err != nil {
			mainResult.Error = fmt.Errorf("failed to read existing config: %w", err)
			return []DeploymentResult{mainResult}, mainResult.Error
		}

		timestamp := time.Now().Format("2006-01-02_15-04-05")
		mainResult.BackupPath = mainResult.Path + ".backup." + timestamp
		if err := os.WriteFile(mainResult.BackupPath, existingData, 0o644); err != nil {
			mainResult.Error = fmt.Errorf("failed to create backup: %w", err)
			return []DeploymentResult{mainResult}, mainResult.Error
		}
		cd.log(fmt.Sprintf("Backed up existing config to %s", mainResult.BackupPath))
	}

	if err := os.WriteFile(mainResult.Path, []byte(KittyConfig), 0o644); err != nil {
		mainResult.Error = fmt.Errorf("failed to write config: %w", err)
		return []DeploymentResult{mainResult}, mainResult.Error
	}

	mainResult.Deployed = true
	cd.log("Successfully deployed Kitty configuration")
	results = append(results, mainResult)

	themeResult := DeploymentResult{
		ConfigType: "Kitty Theme",
		Path:       filepath.Join(os.Getenv("HOME"), ".config", "kitty", "hgs-theme.conf"),
	}

	if err := os.WriteFile(themeResult.Path, []byte(KittyThemeConfig), 0o644); err != nil {
		themeResult.Error = fmt.Errorf("failed to write theme config: %w", err)
		return results, themeResult.Error
	}

	themeResult.Deployed = true
	cd.log("Successfully deployed Kitty theme configuration")
	results = append(results, themeResult)

	tabsResult := DeploymentResult{
		ConfigType: "Kitty Tabs",
		Path:       filepath.Join(os.Getenv("HOME"), ".config", "kitty", "hgs-tabs.conf"),
	}

	if err := os.WriteFile(tabsResult.Path, []byte(KittyTabsConfig), 0o644); err != nil {
		tabsResult.Error = fmt.Errorf("failed to write tabs config: %w", err)
		return results, tabsResult.Error
	}

	tabsResult.Deployed = true
	cd.log("Successfully deployed Kitty tabs configuration")
	results = append(results, tabsResult)

	return results, nil
}

func (cd *ConfigDeployer) deployAlacrittyConfig() ([]DeploymentResult, error) {
	var results []DeploymentResult

	mainResult := DeploymentResult{
		ConfigType: "Alacritty",
		Path:       filepath.Join(os.Getenv("HOME"), ".config", "alacritty", "alacritty.toml"),
	}

	configDir := filepath.Dir(mainResult.Path)
	if err := os.MkdirAll(configDir, 0o755); err != nil {
		mainResult.Error = fmt.Errorf("failed to create config directory: %w", err)
		return []DeploymentResult{mainResult}, mainResult.Error
	}

	if _, err := os.Stat(mainResult.Path); err == nil {
		cd.log("Found existing Alacritty configuration")

		existingData, err := os.ReadFile(mainResult.Path)
		if err != nil {
			mainResult.Error = fmt.Errorf("failed to read existing config: %w", err)
			return []DeploymentResult{mainResult}, mainResult.Error
		}

		timestamp := time.Now().Format("2006-01-02_15-04-05")
		mainResult.BackupPath = mainResult.Path + ".backup." + timestamp
		if err := os.WriteFile(mainResult.BackupPath, existingData, 0o644); err != nil {
			mainResult.Error = fmt.Errorf("failed to create backup: %w", err)
			return []DeploymentResult{mainResult}, mainResult.Error
		}
		cd.log(fmt.Sprintf("Backed up existing config to %s", mainResult.BackupPath))
	}

	if err := os.WriteFile(mainResult.Path, []byte(AlacrittyConfig), 0o644); err != nil {
		mainResult.Error = fmt.Errorf("failed to write config: %w", err)
		return []DeploymentResult{mainResult}, mainResult.Error
	}

	mainResult.Deployed = true
	cd.log("Successfully deployed Alacritty configuration")
	results = append(results, mainResult)

	themeResult := DeploymentResult{
		ConfigType: "Alacritty Theme",
		Path:       filepath.Join(os.Getenv("HOME"), ".config", "alacritty", "hgs-theme.toml"),
	}

	if err := os.WriteFile(themeResult.Path, []byte(AlacrittyThemeConfig), 0o644); err != nil {
		themeResult.Error = fmt.Errorf("failed to write theme config: %w", err)
		return results, themeResult.Error
	}

	themeResult.Deployed = true
	cd.log("Successfully deployed Alacritty theme configuration")
	results = append(results, themeResult)

	return results, nil
}

// deployHyprlandConfig handles Hyprland configuration deployment with backup and merging
func (cd *ConfigDeployer) deployHyprlandConfig(terminal deps.Terminal, useSystemd bool) (DeploymentResult, error) {
	result := DeploymentResult{
		ConfigType: "Hyprland",
		Path:       filepath.Join(os.Getenv("HOME"), ".config", "hypr", "hyprland.lua"),
	}

	configDir := filepath.Dir(result.Path)
	if err := os.MkdirAll(configDir, 0o755); err != nil {
		result.Error = fmt.Errorf("failed to create config directory: %w", err)
		return result, result.Error
	}

	hgsDir := filepath.Join(configDir, "hgs")
	if err := os.MkdirAll(hgsDir, 0o755); err != nil {
		result.Error = fmt.Errorf("failed to create hgs directory: %w", err)
		return result, result.Error
	}

	timestamp := time.Now().Format("2006-01-02_15-04-05")
	backupDir := filepath.Join(configDir, hyprlandBackupDirName, timestamp)
	var existingConfig string
	existingData, existingPath, err := readExistingHyprlandConfig(configDir)
	if err != nil {
		result.Error = err
		return result, result.Error
	}
	if existingData != "" {
		existingConfig = existingData
		cd.log(fmt.Sprintf("Found existing Hyprland configuration at %s", existingPath))

		result.BackupPath = filepath.Join(backupDir, filepath.Base(existingPath))
		if err := backupHyprlandConfigFile(existingPath, result.BackupPath, []byte(existingData), strings.EqualFold(filepath.Ext(existingPath), ".conf")); err != nil {
			result.Error = fmt.Errorf("failed to create backup: %w", err)
			return result, result.Error
		}
		cd.log(fmt.Sprintf("Backed up existing config to %s", result.BackupPath))
	}

	var terminalCommand string
	switch terminal {
	case deps.TerminalGhostty:
		terminalCommand = "ghostty"
	case deps.TerminalKitty:
		terminalCommand = "kitty"
	case deps.TerminalAlacritty:
		terminalCommand = "alacritty"
	default:
		terminalCommand = "ghostty"
	}

	newConfig := strings.ReplaceAll(HyprlandLuaConfig, "{{TERMINAL_COMMAND}}", terminalCommand)

	if !useSystemd {
		newConfig = transformHyprlandLuaForNonSystemd(newConfig, terminalCommand)
	}

	if existingConfig != "" {
		mergedConfig, err := cd.mergeHyprlandMonitorSections(newConfig, existingConfig, hgsDir)
		if err != nil {
			cd.log(fmt.Sprintf("Warning: Failed to merge monitor sections: %v", err))
		} else {
			newConfig = mergedConfig
			cd.log("Successfully merged existing monitor sections")
		}
	}

	if err := os.WriteFile(result.Path, []byte(newConfig), 0o644); err != nil {
		result.Error = fmt.Errorf("failed to write config: %w", err)
		return result, result.Error
	}

	movedLegacy, err := backupLegacyHyprlandConfFiles(configDir, hgsDir, backupDir)
	if err != nil {
		result.Error = fmt.Errorf("failed to back up legacy hyprlang configs: %w", err)
		return result, result.Error
	}
	if movedLegacy > 0 {
		if result.BackupPath == "" {
			result.BackupPath = backupDir
		}
		cd.log(fmt.Sprintf("Moved %d legacy hyprlang config(s) to %s", movedLegacy, backupDir))
	}

	if err := cd.deployHyprlandHgsConfigs(hgsDir, terminalCommand); err != nil {
		result.Error = fmt.Errorf("failed to deploy hgs configs: %w", err)
		return result, result.Error
	}

	CleanupStrayHyprlandConfFile(func(format string, v ...any) {
		cd.log(fmt.Sprintf(format, v...))
	})

	result.Deployed = true
	cd.log("Successfully deployed Hyprland configuration")
	return result, nil
}

func backupHyprlandConfigFile(src, dst string, data []byte, removeSource bool) error {
	if err := os.MkdirAll(filepath.Dir(dst), 0o755); err != nil {
		return err
	}
	if err := os.WriteFile(dst, data, 0o644); err != nil {
		return err
	}
	if removeSource {
		if err := os.Remove(src); err != nil && !os.IsNotExist(err) {
			return err
		}
	}
	return nil
}

func backupLegacyHyprlandConfFiles(configDir, hgsDir, backupDir string) (int, error) {
	legacyPaths := []string{filepath.Join(configDir, "hyprland.conf")}
	hgsConfPaths, err := filepath.Glob(filepath.Join(hgsDir, "*.conf"))
	if err != nil {
		return 0, err
	}
	legacyPaths = append(legacyPaths, hgsConfPaths...)
	backupPaths, err := adjacentHyprlandBackupFiles(configDir, hgsDir)
	if err != nil {
		return 0, err
	}
	legacyPaths = append(legacyPaths, backupPaths...)

	moved := 0
	for _, src := range legacyPaths {
		info, err := os.Lstat(src)
		if os.IsNotExist(err) {
			continue
		}
		if err != nil {
			return moved, err
		}
		if info.IsDir() {
			continue
		}

		rel, err := filepath.Rel(configDir, src)
		if err != nil {
			rel = filepath.Base(src)
		}
		dst := filepath.Join(backupDir, rel)
		if err := moveHyprlandConfigFile(src, dst); err != nil {
			return moved, err
		}
		moved++
	}

	return moved, nil
}

func moveHyprlandConfigFile(src, dst string) error {
	if err := os.MkdirAll(filepath.Dir(dst), 0o755); err != nil {
		return err
	}
	return os.Rename(src, dst)
}

func adjacentHyprlandBackupFiles(configDir, hgsDir string) ([]string, error) {
	var paths []string
	patterns := []string{
		filepath.Join(configDir, "hyprland.conf.backup.*"),
		filepath.Join(configDir, "hyprland.lua.backup.*"),
		filepath.Join(hgsDir, "*.conf.backup.*"),
		filepath.Join(hgsDir, "*.lua.backup.*"),
	}
	for _, pattern := range patterns {
		matches, err := filepath.Glob(pattern)
		if err != nil {
			return nil, err
		}
		paths = append(paths, matches...)
	}
	return paths, nil
}

func (cd *ConfigDeployer) deployHyprlandHgsConfigs(hgsDir string, terminalCommand string) error {
	_ = terminalCommand
	cd.migrateHyprlandHgsLua(hgsDir, "binds-user.lua", "user-keybinds.lua")
	cd.migrateHyprlandHgsLua(hgsDir, "outputs.lua", "monitors.lua")
	cd.migrateHyprlandHgsLua(hgsDir, "windowrules.lua", "rules.lua")

	configs := []struct {
		name      string
		content   string
		overwrite bool
	}{
		{name: "animations.lua", content: HGSAnimationsLuaConfig},
		{name: "colors.lua", content: HGSColorsLuaConfig},
		{name: "cursor.lua", content: HGSCursorLuaConfig},
		{name: "custom.lua", content: HGSCustomLuaConfig},
		{name: "environment.lua", content: HGSEnvironmentLuaConfig},
		{name: "general.lua", content: HGSGeneralLuaConfig},
		{name: "input.lua", content: HGSInputLuaConfig},
		{name: "keybinds.lua", content: HGSKeybindsLuaConfig, overwrite: true},
		{name: "layout.lua", content: HGSLayoutLuaConfig},
		{name: "misc.lua", content: HGSMiscLuaConfig},
		{name: "monitors.lua", content: HGSMonitorsLuaConfig},
		{name: "permissions.lua", content: HGSPermissionsLuaConfig},
		{name: "rules.lua", content: HGSRulesLuaConfig},
		{name: "user-keybinds.lua", content: HGSUserKeybindsLuaConfig},
	}

	for _, cfg := range configs {
		path := filepath.Join(hgsDir, cfg.name)
		existed := false
		if info, err := os.Stat(path); err == nil && info.Size() > 0 {
			existed = true
		}
		if existed && !cfg.overwrite {
			cd.log(fmt.Sprintf("Skipping %s (already exists)", cfg.name))
			continue
		}
		if err := os.WriteFile(path, []byte(cfg.content), 0o644); err != nil {
			return fmt.Errorf("failed to write %s: %w", cfg.name, err)
		}
		if existed {
			cd.log(fmt.Sprintf("Updated %s", cfg.name))
			continue
		}
		cd.log(fmt.Sprintf("Deployed %s", cfg.name))
	}

	return nil
}

func (cd *ConfigDeployer) migrateHyprlandHgsLua(hgsDir, oldName, newName string) {
	oldPath := filepath.Join(hgsDir, oldName)
	newPath := filepath.Join(hgsDir, newName)
	if info, err := os.Stat(newPath); err == nil && info.Size() > 0 {
		return
	}
	data, err := os.ReadFile(oldPath)
	if err != nil || strings.TrimSpace(string(data)) == "" {
		return
	}
	if err := os.WriteFile(newPath, data, 0o644); err != nil {
		cd.log(fmt.Sprintf("Warning: failed to migrate %s to %s: %v", oldName, newName, err))
		return
	}
	cd.log(fmt.Sprintf("Migrated %s to %s", oldName, newName))
}

func (cd *ConfigDeployer) mergeHyprlandMonitorSections(newConfig, existingConfig, hgsDir string) (string, error) {
	_ = newConfig
	lines := extractHyprlangMonitorLines(existingConfig)
	if len(lines) == 0 {
		return newConfig, nil
	}

	monitorsPath := filepath.Join(hgsDir, "monitors.lua")
	if info, err := os.Stat(monitorsPath); err == nil && info.Size() > 0 {
		cd.log("Skipping monitor migration: hgs/monitors.lua already exists")
		return newConfig, nil
	}

	var b strings.Builder
	b.WriteString("-- Migrated from existing hyprlang monitor lines\n\n")
	ok := 0
	for _, line := range lines {
		lua, err := hyprlangMonitorLineToLua(line)
		if err != nil {
			cd.log(fmt.Sprintf("Warning: could not migrate monitor line %q: %v", line, err))
			continue
		}
		b.WriteString(lua)
		b.WriteByte('\n')
		ok++
	}
	if ok == 0 {
		return newConfig, nil
	}
	b.WriteByte('\n')
	b.WriteString("-- Default fallback\n")
	b.WriteString("hl.monitor({ output = \"\", mode = \"preferred\", position = \"auto\", scale = \"auto\" })\n")
	if err := os.WriteFile(monitorsPath, []byte(b.String()), 0o644); err != nil {
		return newConfig, err
	}
	cd.log("Migrated monitor sections to hgs/monitors.lua")
	return newConfig, nil
}
