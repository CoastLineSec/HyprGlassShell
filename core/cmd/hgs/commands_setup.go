package main

import (
	"context"
	"fmt"
	"os"
	"os/exec"
	"path/filepath"
	"strings"

	"github.com/CoastLineSec/HyprGlassShell/core/internal/config"
	"github.com/CoastLineSec/HyprGlassShell/core/internal/deps"
	"github.com/CoastLineSec/HyprGlassShell/core/internal/log"
	"github.com/CoastLineSec/HyprGlassShell/core/internal/privesc"
	"github.com/CoastLineSec/HyprGlassShell/core/internal/utils"
	"github.com/spf13/cobra"
)

var setupCmd = &cobra.Command{
	Use:               "setup",
	Short:             "Deploy HGS configurations",
	Long:              "Deploy compositor and terminal configurations with interactive prompts",
	PersistentPreRunE: preRunPrivileged,
	Run: func(cmd *cobra.Command, args []string) {
		if err := runSetup(); err != nil {
			log.Fatalf("Error during setup: %v", err)
		}
	},
}

var setupBindsCmd = &cobra.Command{
	Use:     "keybinds",
	Aliases: []string{"binds"},
	Short:   "Deploy default keybinds config",
	Run: func(cmd *cobra.Command, args []string) {
		if err := runSetupHgsConfig("keybinds"); err != nil {
			log.Fatalf("Error: %v", err)
		}
	},
}

var setupAnimationsCmd = &cobra.Command{
	Use:   "animations",
	Short: "Deploy default animations config",
	Run: func(cmd *cobra.Command, args []string) {
		if err := runSetupHgsConfig("animations"); err != nil {
			log.Fatalf("Error: %v", err)
		}
	},
}

var setupLayoutCmd = &cobra.Command{
	Use:   "layout",
	Short: "Deploy default layout config",
	Run: func(cmd *cobra.Command, args []string) {
		if err := runSetupHgsConfig("layout"); err != nil {
			log.Fatalf("Error: %v", err)
		}
	},
}

var setupColorsCmd = &cobra.Command{
	Use:   "colors",
	Short: "Deploy default colors config",
	Run: func(cmd *cobra.Command, args []string) {
		if err := runSetupHgsConfig("colors"); err != nil {
			log.Fatalf("Error: %v", err)
		}
	},
}

var setupCustomCmd = &cobra.Command{
	Use:   "custom",
	Short: "Deploy default custom config",
	Run: func(cmd *cobra.Command, args []string) {
		if err := runSetupHgsConfig("custom"); err != nil {
			log.Fatalf("Error: %v", err)
		}
	},
}

var setupEnvironmentCmd = &cobra.Command{
	Use:   "environment",
	Short: "Deploy default environment config",
	Run: func(cmd *cobra.Command, args []string) {
		if err := runSetupHgsConfig("environment"); err != nil {
			log.Fatalf("Error: %v", err)
		}
	},
}

var setupGeneralCmd = &cobra.Command{
	Use:   "general",
	Short: "Deploy default general config",
	Run: func(cmd *cobra.Command, args []string) {
		if err := runSetupHgsConfig("general"); err != nil {
			log.Fatalf("Error: %v", err)
		}
	},
}

var setupInputCmd = &cobra.Command{
	Use:   "input",
	Short: "Deploy default input config",
	Run: func(cmd *cobra.Command, args []string) {
		if err := runSetupHgsConfig("input"); err != nil {
			log.Fatalf("Error: %v", err)
		}
	},
}

var setupMiscCmd = &cobra.Command{
	Use:   "misc",
	Short: "Deploy default misc config",
	Run: func(cmd *cobra.Command, args []string) {
		if err := runSetupHgsConfig("misc"); err != nil {
			log.Fatalf("Error: %v", err)
		}
	},
}

var setupOutputsCmd = &cobra.Command{
	Use:     "monitors",
	Aliases: []string{"outputs"},
	Short:   "Deploy default monitors config",
	Run: func(cmd *cobra.Command, args []string) {
		if err := runSetupHgsConfig("monitors"); err != nil {
			log.Fatalf("Error: %v", err)
		}
	},
}

var setupCursorCmd = &cobra.Command{
	Use:   "cursor",
	Short: "Deploy default cursor config",
	Run: func(cmd *cobra.Command, args []string) {
		if err := runSetupHgsConfig("cursor"); err != nil {
			log.Fatalf("Error: %v", err)
		}
	},
}

var setupPermissionsCmd = &cobra.Command{
	Use:   "permissions",
	Short: "Deploy default permissions config",
	Run: func(cmd *cobra.Command, args []string) {
		if err := runSetupHgsConfig("permissions"); err != nil {
			log.Fatalf("Error: %v", err)
		}
	},
}

var setupWindowrulesCmd = &cobra.Command{
	Use:     "rules",
	Aliases: []string{"windowrules"},
	Short:   "Deploy default window rules config",
	Run: func(cmd *cobra.Command, args []string) {
		if err := runSetupHgsConfig("rules"); err != nil {
			log.Fatalf("Error: %v", err)
		}
	},
}

var setupUserKeybindsCmd = &cobra.Command{
	Use:     "user-keybinds",
	Aliases: []string{"binds-user"},
	Short:   "Deploy default user keybind overrides config",
	Run: func(cmd *cobra.Command, args []string) {
		if err := runSetupHgsConfig("user-keybinds"); err != nil {
			log.Fatalf("Error: %v", err)
		}
	},
}

type hgsConfigSpec struct {
	hyprFile    string
	hyprContent func(terminal string) string
}

var hgsConfigSpecs = map[string]hgsConfigSpec{
	"animations": {
		hyprFile:    "animations.lua",
		hyprContent: func(_ string) string { return config.HGSAnimationsLuaConfig },
	},
	"colors": {
		hyprFile:    "colors.lua",
		hyprContent: func(_ string) string { return config.HGSColorsLuaConfig },
	},
	"cursor": {
		hyprFile:    "cursor.lua",
		hyprContent: func(_ string) string { return config.HGSCursorLuaConfig },
	},
	"custom": {
		hyprFile:    "custom.lua",
		hyprContent: func(_ string) string { return config.HGSCustomLuaConfig },
	},
	"environment": {
		hyprFile:    "environment.lua",
		hyprContent: func(_ string) string { return config.HGSEnvironmentLuaConfig },
	},
	"general": {
		hyprFile:    "general.lua",
		hyprContent: func(_ string) string { return config.HGSGeneralLuaConfig },
	},
	"input": {
		hyprFile:    "input.lua",
		hyprContent: func(_ string) string { return config.HGSInputLuaConfig },
	},
	"keybinds": {
		hyprFile:    "keybinds.lua",
		hyprContent: func(_ string) string { return config.HGSKeybindsLuaConfig },
	},
	"layout": {
		hyprFile:    "layout.lua",
		hyprContent: func(_ string) string { return config.HGSLayoutLuaConfig },
	},
	"misc": {
		hyprFile:    "misc.lua",
		hyprContent: func(_ string) string { return config.HGSMiscLuaConfig },
	},
	"monitors": {
		hyprFile:    "monitors.lua",
		hyprContent: func(_ string) string { return config.HGSMonitorsLuaConfig },
	},
	"permissions": {
		hyprFile:    "permissions.lua",
		hyprContent: func(_ string) string { return config.HGSPermissionsLuaConfig },
	},
	"rules": {
		hyprFile:    "rules.lua",
		hyprContent: func(_ string) string { return config.HGSRulesLuaConfig },
	},
	"user-keybinds": {
		hyprFile:    "user-keybinds.lua",
		hyprContent: func(_ string) string { return config.HGSUserKeybindsLuaConfig },
	},
	"binds":       {hyprFile: "keybinds.lua", hyprContent: func(_ string) string { return config.HGSKeybindsLuaConfig }},
	"binds-user":  {hyprFile: "user-keybinds.lua", hyprContent: func(_ string) string { return config.HGSUserKeybindsLuaConfig }},
	"outputs":     {hyprFile: "monitors.lua", hyprContent: func(_ string) string { return config.HGSMonitorsLuaConfig }},
	"windowrules": {hyprFile: "rules.lua", hyprContent: func(_ string) string { return config.HGSRulesLuaConfig }},
}

func detectCompositorForSetup() (string, error) {
	if !utils.CommandExists("Hyprland") && !utils.CommandExists("hyprctl") {
		return "", fmt.Errorf("Hyprland is required")
	}
	return "hyprland", nil
}

func runSetupHgsConfig(name string) error {
	spec, ok := hgsConfigSpecs[name]
	if !ok {
		return fmt.Errorf("unknown config: %s", name)
	}

	compositor, err := detectCompositorForSetup()
	if err != nil {
		return err
	}

	if compositor != "hyprland" {
		return fmt.Errorf("unsupported compositor: %s", compositor)
	}

	filename := spec.hyprFile
	contentFn := spec.hyprContent
	if filename == "" {
		return fmt.Errorf("%s is not supported for Hyprland", name)
	}

	hgsDir := filepath.Join(utils.XDGConfigHome(), "hypr", "hgs")

	if err := os.MkdirAll(hgsDir, 0o755); err != nil {
		return fmt.Errorf("failed to create hgs directory: %w", err)
	}

	path := filepath.Join(hgsDir, filename)
	if info, err := os.Stat(path); err == nil && info.Size() > 0 {
		return fmt.Errorf("%s already exists and is not empty: %s", name, path)
	}

	content := contentFn("")
	if err := os.WriteFile(path, []byte(content), 0o644); err != nil {
		return fmt.Errorf("failed to write %s: %w", filename, err)
	}

	fmt.Printf("Deployed %s to %s\n", name, path)
	return nil
}

func runSetup() error {
	fmt.Println("=== HGS Configuration Setup ===")

	ensureInputGroup()

	wm, wmSelected := promptCompositor()
	terminal, terminalSelected := promptTerminal()
	useSystemd := true
	if wmSelected {
		useSystemd = promptSystemd()
	}

	if !wmSelected && !terminalSelected {
		fmt.Println("No configurations selected. Exiting.")
		return nil
	}

	if wmSelected || terminalSelected {
		willBackup := checkExistingConfigs(wm, wmSelected, terminal, terminalSelected)
		if willBackup {
			fmt.Println("\n⚠ Existing configurations will be backed up with timestamps.")
		}

		fmt.Print("\nProceed with deployment? (y/N): ")
		var response string
		fmt.Scanln(&response)
		response = strings.ToLower(strings.TrimSpace(response))

		if response != "y" && response != "yes" {
			fmt.Println("Setup cancelled.")
			return nil
		}
	}

	fmt.Println("\nDeploying configurations...")
	logChan := make(chan string, 100)
	deployer := config.NewConfigDeployer(logChan)

	go func() {
		for msg := range logChan {
			fmt.Println("  " + msg)
		}
	}()

	ctx := context.Background()
	var results []config.DeploymentResult
	var err error

	if wmSelected && terminalSelected {
		results, err = deployer.DeployConfigurationsWithSystemd(ctx, wm, terminal, useSystemd)
	} else if wmSelected {
		results, err = deployer.DeployConfigurationsWithSystemd(ctx, wm, deps.TerminalGhostty, useSystemd)
		if len(results) > 1 {
			results = results[:1]
		}
	} else if terminalSelected {
		results, err = deployer.DeployConfigurationsWithSystemd(ctx, deps.WindowManagerHyprland, terminal, useSystemd)
		if len(results) > 0 && results[0].ConfigType == "Hyprland" {
			results = results[1:]
		}
	}

	close(logChan)

	if err != nil {
		return fmt.Errorf("deployment failed: %w", err)
	}

	fmt.Println("\n=== Deployment Complete ===")
	for _, result := range results {
		if result.Deployed {
			fmt.Printf("✓ %s: %s\n", result.ConfigType, result.Path)
			if result.BackupPath != "" {
				fmt.Printf("  Backup: %s\n", result.BackupPath)
			}
		}
	}

	return nil
}

// Add user to the input group for the evdev manager for inut state tracking.
// Caps Lock OSD and the Caps Lock bar indicator.
func ensureInputGroup() {
	if !utils.HasGroup("input") {
		return
	}
	currentUser := os.Getenv("USER")
	if currentUser == "" {
		currentUser = os.Getenv("LOGNAME")
	}
	if currentUser == "" {
		return
	}
	out, err := execGroups(currentUser)
	if err == nil && strings.Contains(out, "input") {
		fmt.Printf("✓ %s is already in the input group (Caps Lock OSD enabled)\n", currentUser)
		return
	}
	fmt.Println("Adding user to input group for Caps Lock OSD support...")
	if err := privesc.Run(context.Background(), "", "usermod", "-aG", "input", currentUser); err != nil {
		fmt.Printf("⚠ Could not add %s to input group (Caps Lock OSD will be unavailable): %v\n", currentUser, err)
	} else {
		fmt.Printf("✓ Added %s to input group (logout/login required to take effect)\n", currentUser)
	}
}

func execGroups(user string) (string, error) {
	out, err := exec.Command("groups", user).Output()
	return string(out), err
}

func promptCompositor() (deps.WindowManager, bool) {
	fmt.Println("Select compositor:")
	fmt.Println("1) Hyprland")
	fmt.Println("2) None")

	var response string
	fmt.Print("\nChoice (1-2): ")
	fmt.Scanln(&response)
	response = strings.TrimSpace(response)

	switch response {
	case "1":
		return deps.WindowManagerHyprland, true
	default:
		return deps.WindowManagerHyprland, false
	}
}

func promptTerminal() (deps.Terminal, bool) {
	fmt.Println("\nSelect terminal:")
	fmt.Println("1) Ghostty")
	fmt.Println("2) Kitty")
	fmt.Println("3) Alacritty")
	fmt.Println("4) None")

	var response string
	fmt.Print("\nChoice (1-4): ")
	fmt.Scanln(&response)
	response = strings.TrimSpace(response)

	switch response {
	case "1":
		return deps.TerminalGhostty, true
	case "2":
		return deps.TerminalKitty, true
	case "3":
		return deps.TerminalAlacritty, true
	default:
		return deps.TerminalGhostty, false
	}
}

func promptSystemd() bool {
	fmt.Println("\nUse systemd for session management?")
	fmt.Println("1) Yes (recommended for most distros)")
	fmt.Println("2) No (standalone, no systemd integration)")

	var response string
	fmt.Print("\nChoice (1-2): ")
	fmt.Scanln(&response)
	response = strings.TrimSpace(response)

	return response != "2"
}

func checkExistingConfigs(wm deps.WindowManager, wmSelected bool, terminal deps.Terminal, terminalSelected bool) bool {
	homeDir := os.Getenv("HOME")
	willBackup := false

	if wmSelected {
		var configPaths []string
		switch wm {
		case deps.WindowManagerHyprland:
			configPaths = []string{
				filepath.Join(homeDir, ".config", "hypr", "hyprland.lua"),
				filepath.Join(homeDir, ".config", "hypr", "hyprland.conf"),
			}
		}

		for _, configPath := range configPaths {
			if _, err := os.Stat(configPath); err == nil {
				willBackup = true
				break
			}
		}
	}

	if terminalSelected {
		var configPath string
		switch terminal {
		case deps.TerminalGhostty:
			configPath = filepath.Join(homeDir, ".config", "ghostty", "config")
		case deps.TerminalKitty:
			configPath = filepath.Join(homeDir, ".config", "kitty", "kitty.conf")
		case deps.TerminalAlacritty:
			configPath = filepath.Join(homeDir, ".config", "alacritty", "alacritty.toml")
		}

		if _, err := os.Stat(configPath); err == nil {
			willBackup = true
		}
	}

	return willBackup
}
