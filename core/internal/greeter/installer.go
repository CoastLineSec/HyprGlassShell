package greeter

import (
	"bufio"
	"context"
	_ "embed"
	"encoding/json"
	"errors"
	"fmt"
	"os"
	"os/exec"
	"os/user"
	"path/filepath"
	"strings"
	"time"

	"github.com/CoastLineSec/HyprGlassShell/core/internal/config"
	"github.com/CoastLineSec/HyprGlassShell/core/internal/distros"
	"github.com/CoastLineSec/HyprGlassShell/core/internal/matugen"
	sharedpam "github.com/CoastLineSec/HyprGlassShell/core/internal/pam"
	"github.com/CoastLineSec/HyprGlassShell/core/internal/privesc"
	"github.com/CoastLineSec/HyprGlassShell/core/internal/utils"
)

var appArmorProfileData []byte

const appArmorProfileDest = "/etc/apparmor.d/usr.bin.hgs-greeter"

const GreeterCacheDir = "/var/cache/hgs-greeter"

func DetectHGSPath() (string, error) {
	return config.LocateHGSConfig()
}

// IsNixOS returns true when running on NixOS, which manages PAM configs through
// its module system. The HGS PAM managed block won't be written on NixOS.
func IsNixOS() bool {
	_, err := os.Stat("/etc/NIXOS")
	return err == nil
}

func DetectGreeterGroup() string {
	data, err := os.ReadFile("/etc/group")
	if err != nil {
		fmt.Fprintln(os.Stderr, "⚠ Warning: could not read /etc/group, defaulting to greeter")
		return "greeter"
	}

	if group, found := utils.FindGroupData(string(data), "greeter", "greetd", "_greeter"); found {
		return group
	}

	fmt.Fprintln(os.Stderr, "⚠ Warning: no greeter group found in /etc/group, defaulting to greeter")
	return "greeter"
}

func hasPasswdUser(passwdData, user string) bool {
	prefix := user + ":"
	for line := range strings.SplitSeq(passwdData, "\n") {
		if strings.HasPrefix(line, prefix) {
			return true
		}
	}
	return false
}

func findPasswdUser(passwdData string, candidates ...string) (string, bool) {
	for _, candidate := range candidates {
		if hasPasswdUser(passwdData, candidate) {
			return candidate, true
		}
	}
	return "", false
}

func stripTomlComment(line string) string {
	trimmed := strings.TrimSpace(line)
	if idx := strings.Index(trimmed, "#"); idx >= 0 {
		return strings.TrimSpace(trimmed[:idx])
	}
	return trimmed
}

func parseTomlSection(line string) (string, bool) {
	trimmed := stripTomlComment(line)
	if len(trimmed) < 3 || !strings.HasPrefix(trimmed, "[") || !strings.HasSuffix(trimmed, "]") {
		return "", false
	}
	return strings.TrimSpace(trimmed[1 : len(trimmed)-1]), true
}

func extractDefaultSessionUser(configContent string) string {
	inDefaultSession := false
	for line := range strings.SplitSeq(configContent, "\n") {
		if section, ok := parseTomlSection(line); ok {
			inDefaultSession = section == "default_session"
			continue
		}

		if !inDefaultSession {
			continue
		}

		trimmed := stripTomlComment(line)
		if !strings.HasPrefix(trimmed, "user =") && !strings.HasPrefix(trimmed, "user=") {
			continue
		}

		parts := strings.SplitN(trimmed, "=", 2)
		if len(parts) != 2 {
			continue
		}
		user := strings.Trim(strings.TrimSpace(parts[1]), `"`)
		if user != "" {
			return user
		}
	}

	return ""
}

func upsertDefaultSession(configContent, greeterUser, command string) string {
	lines := strings.Split(configContent, "\n")
	var out []string

	inDefaultSession := false
	foundDefaultSession := false
	defaultSessionUserSet := false
	defaultSessionCommandSet := false

	appendDefaultSessionFields := func() {
		if !defaultSessionUserSet {
			out = append(out, fmt.Sprintf(`user = "%s"`, greeterUser))
		}
		if !defaultSessionCommandSet {
			out = append(out, command)
		}
	}

	for _, line := range lines {
		if section, ok := parseTomlSection(line); ok {
			if inDefaultSession {
				appendDefaultSessionFields()
			}

			inDefaultSession = section == "default_session"
			if inDefaultSession {
				foundDefaultSession = true
				defaultSessionUserSet = false
				defaultSessionCommandSet = false
			}

			out = append(out, line)
			continue
		}

		if inDefaultSession {
			trimmed := stripTomlComment(line)
			if strings.HasPrefix(trimmed, "user =") || strings.HasPrefix(trimmed, "user=") {
				out = append(out, fmt.Sprintf(`user = "%s"`, greeterUser))
				defaultSessionUserSet = true
				continue
			}

			if strings.HasPrefix(trimmed, "command =") || strings.HasPrefix(trimmed, "command=") {
				if !defaultSessionCommandSet {
					out = append(out, command)
					defaultSessionCommandSet = true
				}
				continue
			}
		}

		out = append(out, line)
	}

	if inDefaultSession {
		appendDefaultSessionFields()
	}

	if !foundDefaultSession {
		if len(out) > 0 && strings.TrimSpace(out[len(out)-1]) != "" {
			out = append(out, "")
		}
		out = append(out, "[default_session]")
		out = append(out, fmt.Sprintf(`user = "%s"`, greeterUser))
		out = append(out, command)
	}

	return strings.Join(out, "\n")
}

func removeTomlSection(configContent, sectionName string) string {
	lines := strings.Split(configContent, "\n")
	var out []string
	inSection := false

	for _, line := range lines {
		if section, ok := parseTomlSection(line); ok {
			inSection = section == sectionName
			if inSection {
				continue
			}
			out = append(out, line)
			continue
		}

		if inSection {
			continue
		}

		out = append(out, line)
	}

	result := strings.TrimRight(strings.Join(out, "\n"), "\n")
	if result != "" {
		result += "\n"
	}
	return result
}

func stripDesktopExecCodes(execLine string) string {
	fields := strings.Fields(execLine)
	cleaned := make([]string, 0, len(fields))
	for _, field := range fields {
		if strings.HasPrefix(field, "%") {
			continue
		}
		cleaned = append(cleaned, field)
	}
	return strings.Join(cleaned, " ")
}

func formatInitialSessionCommand(sessionExec string) string {
	execLine := strings.TrimSpace(stripDesktopExecCodes(sessionExec))
	if execLine == "" {
		return `command = ""`
	}
	escaped := strings.ReplaceAll(execLine, `'`, `'\''`)
	inner := fmt.Sprintf("env XDG_SESSION_TYPE=wayland sh -c 'exec %s'", escaped)
	tomlEscaped := strings.ReplaceAll(inner, `\`, `\\`)
	tomlEscaped = strings.ReplaceAll(tomlEscaped, `"`, `\"`)
	return fmt.Sprintf(`command = "%s"`, tomlEscaped)
}

func upsertInitialSession(configContent, loginUser, sessionExec string, enabled bool) string {
	if !enabled {
		return removeTomlSection(configContent, "initial_session")
	}

	commandLine := formatInitialSessionCommand(sessionExec)
	lines := strings.Split(configContent, "\n")
	var out []string

	inInitialSession := false
	foundInitialSession := false
	initialSessionUserSet := false
	initialSessionCommandSet := false

	appendInitialSessionFields := func() {
		if !initialSessionUserSet {
			out = append(out, fmt.Sprintf(`user = "%s"`, loginUser))
		}
		if !initialSessionCommandSet {
			out = append(out, commandLine)
		}
	}

	for _, line := range lines {
		if section, ok := parseTomlSection(line); ok {
			if inInitialSession {
				appendInitialSessionFields()
			}

			inInitialSession = section == "initial_session"
			if inInitialSession {
				foundInitialSession = true
				initialSessionUserSet = false
				initialSessionCommandSet = false
			}

			out = append(out, line)
			continue
		}

		if inInitialSession {
			trimmed := stripTomlComment(line)
			if strings.HasPrefix(trimmed, "user =") || strings.HasPrefix(trimmed, "user=") {
				out = append(out, fmt.Sprintf(`user = "%s"`, loginUser))
				initialSessionUserSet = true
				continue
			}

			if strings.HasPrefix(trimmed, "command =") || strings.HasPrefix(trimmed, "command=") {
				if !initialSessionCommandSet {
					out = append(out, commandLine)
					initialSessionCommandSet = true
				}
				continue
			}
		}

		out = append(out, line)
	}

	if inInitialSession {
		appendInitialSessionFields()
	}

	if !foundInitialSession {
		if len(out) > 0 && strings.TrimSpace(out[len(out)-1]) != "" {
			out = append(out, "")
		}
		out = append(out, "[initial_session]")
		out = append(out, fmt.Sprintf(`user = "%s"`, loginUser))
		out = append(out, commandLine)
	}

	return strings.Join(out, "\n")
}

type greeterAutoLoginConfig struct {
	GreeterAutoLogin           bool `json:"greeterAutoLogin"`
	GreeterRememberLastUser    bool `json:"greeterRememberLastUser"`
	GreeterRememberLastSession bool `json:"greeterRememberLastSession"`
}

type greeterAutoLoginMemory struct {
	LastSuccessfulUser string `json:"lastSuccessfulUser"`
	LastSessionID      string `json:"lastSessionId"`
	LastSessionExec    string `json:"lastSessionExec"`
	AutoLoginEnabled   bool   `json:"autoLoginEnabled"`
}

func readGreeterAutoLoginConfig(settingsPath string) (greeterAutoLoginConfig, error) {
	cfg := greeterAutoLoginConfig{
		GreeterRememberLastUser:    true,
		GreeterRememberLastSession: true,
	}
	data, err := os.ReadFile(settingsPath)
	if err != nil {
		if os.IsNotExist(err) {
			return cfg, nil
		}
		return cfg, err
	}
	if err := json.Unmarshal(data, &cfg); err != nil {
		return cfg, fmt.Errorf("failed to parse settings at %s: %w", settingsPath, err)
	}
	return cfg, nil
}

func readGreeterAutoLoginMemory(memoryPath string) (greeterAutoLoginMemory, error) {
	var mem greeterAutoLoginMemory
	data, err := os.ReadFile(memoryPath)
	if err != nil {
		if os.IsNotExist(err) {
			return mem, nil
		}
		return mem, err
	}
	if err := json.Unmarshal(data, &mem); err != nil {
		return mem, fmt.Errorf("failed to parse greeter memory at %s: %w", memoryPath, err)
	}
	return mem, nil
}

func execFromDesktopFile(path string) (string, error) {
	data, err := os.ReadFile(path)
	if err != nil {
		return "", err
	}
	for line := range strings.SplitSeq(string(data), "\n") {
		trimmed := strings.TrimSpace(line)
		if strings.HasPrefix(trimmed, "Exec=") {
			return strings.TrimSpace(trimmed[len("Exec="):]), nil
		}
	}
	return "", fmt.Errorf("no Exec= line found in %s", path)
}

func resolveGreeterAutoLoginState(cacheDir, homeDir string) (enabled bool, loginUser string, sessionExec string, err error) {
	settingsPath := filepath.Join(cacheDir, "settings.json")
	if _, statErr := os.Stat(settingsPath); statErr != nil {
		settingsPath = filepath.Join(homeDir, ".config", "HyprGlassShell", "settings.json")
	}

	cfg, err := readGreeterAutoLoginConfig(settingsPath)
	if err != nil {
		return false, "", "", err
	}

	memoryPath := filepath.Join(cacheDir, ".local/state/memory.json")
	mem, err := readGreeterAutoLoginMemory(memoryPath)
	if err != nil {
		return false, "", "", err
	}

	enabled = cfg.GreeterAutoLogin
	if !enabled {
		return false, "", "", nil
	}

	if !cfg.GreeterRememberLastUser || !cfg.GreeterRememberLastSession {
		return true, "", "", nil
	}

	loginUser = mem.LastSuccessfulUser
	if loginUser == "" {
		current, userErr := user.Current()
		if userErr != nil {
			return true, "", "", userErr
		}
		loginUser = current.Username
	}

	sessionExec = mem.LastSessionExec
	if sessionExec == "" && mem.LastSessionID != "" {
		sessionExec, err = execFromDesktopFile(mem.LastSessionID)
		if err != nil {
			sessionExec = ""
		}
	}

	return true, loginUser, sessionExec, nil
}

func writeGreetdConfig(configPath, content string, logFunc func(string), sudoPassword, successMsg string) error {
	if err := backupFileIfExists(sudoPassword, configPath, ".backup"); err != nil {
		return fmt.Errorf("failed to backup greetd config: %w", err)
	}

	tmpFile, err := os.CreateTemp("", "greetd-config-*.toml")
	if err != nil {
		return fmt.Errorf("failed to create temp greetd config: %w", err)
	}
	defer os.Remove(tmpFile.Name())

	if _, err := tmpFile.WriteString(content); err != nil {
		_ = tmpFile.Close()
		return fmt.Errorf("failed to write temp greetd config: %w", err)
	}
	if err := tmpFile.Close(); err != nil {
		return fmt.Errorf("failed to close temp greetd config: %w", err)
	}

	if err := privesc.Run(context.Background(), sudoPassword, "mkdir", "-p", "/etc/greetd"); err != nil {
		return fmt.Errorf("failed to create /etc/greetd: %w", err)
	}

	if err := privesc.Run(context.Background(), sudoPassword, "install", "-o", "root", "-g", "root", "-m", "0644", tmpFile.Name(), configPath); err != nil {
		return fmt.Errorf("failed to install greetd config: %w", err)
	}

	if logFunc != nil && successMsg != "" {
		logFunc(successMsg)
	}
	return nil
}

func clearGreeterAutoLoginMemory(memoryPath, sudoPassword string) error {
	data, err := readGreeterMemoryFile(memoryPath, sudoPassword)
	if err != nil {
		if os.IsNotExist(err) {
			return nil
		}
		return err
	}
	if len(strings.TrimSpace(string(data))) == 0 {
		return nil
	}
	var raw map[string]json.RawMessage
	if err := json.Unmarshal(data, &raw); err != nil {
		return fmt.Errorf("failed to parse greeter memory at %s: %w", memoryPath, err)
	}
	if _, ok := raw["autoLoginEnabled"]; !ok {
		return nil
	}
	delete(raw, "autoLoginEnabled")
	encoded, err := json.MarshalIndent(raw, "", "  ")
	if err != nil {
		return err
	}
	if len(encoded) == 0 || string(encoded) == "null" {
		encoded = []byte("{}")
	}
	encoded = append(encoded, '\n')

	if err := os.WriteFile(memoryPath, encoded, 0o644); err == nil {
		return nil
	} else if !os.IsPermission(err) {
		return err
	}

	tmpFile, err := os.CreateTemp("", "greeter-memory-*.json")
	if err != nil {
		return fmt.Errorf("failed to create temp greeter memory file: %w", err)
	}
	defer os.Remove(tmpFile.Name())

	if _, err := tmpFile.Write(encoded); err != nil {
		_ = tmpFile.Close()
		return fmt.Errorf("failed to write temp greeter memory file: %w", err)
	}
	if err := tmpFile.Close(); err != nil {
		return fmt.Errorf("failed to close temp greeter memory file: %w", err)
	}

	greeterUser := DetectGreeterUser()
	greeterGroup := DetectGreeterGroup()
	owner := greeterUser + ":" + greeterGroup
	if err := privesc.Run(context.Background(), sudoPassword, "install", "-o", greeterUser, "-g", greeterGroup, "-m", "0664", tmpFile.Name(), memoryPath); err != nil {
		if fallbackErr := privesc.Run(context.Background(), sudoPassword, "install", "-o", "root", "-g", greeterGroup, "-m", "0664", tmpFile.Name(), memoryPath); fallbackErr != nil {
			return fmt.Errorf("failed to install greeter memory file (preferred %s: %w; fallback root:%s: %v)", owner, err, greeterGroup, fallbackErr)
		}
	}
	return nil
}

func readGreeterMemoryFile(memoryPath, sudoPassword string) ([]byte, error) {
	data, err := os.ReadFile(memoryPath)
	if err == nil || !os.IsPermission(err) {
		return data, err
	}

	tmpFile, err := os.CreateTemp("", "greeter-memory-read-*")
	if err != nil {
		return nil, fmt.Errorf("failed to create temp file for greeter memory read: %w", err)
	}
	tmpPath := tmpFile.Name()
	_ = tmpFile.Close()
	defer os.Remove(tmpPath)

	if err := privesc.Run(context.Background(), sudoPassword, "cp", "-f", memoryPath, tmpPath); err != nil {
		return nil, fmt.Errorf("failed to read greeter memory at %s: %w", memoryPath, err)
	}
	return os.ReadFile(tmpPath)
}

func SyncGreetdAutoLogin(cacheDir, homeDir string, logFunc func(string), sudoPassword string) error {
	enabled, loginUser, sessionExec, err := resolveGreeterAutoLoginState(cacheDir, homeDir)
	if err != nil {
		return err
	}

	configPath := "/etc/greetd/config.toml"
	configContent := ""
	if data, readErr := os.ReadFile(configPath); readErr == nil {
		configContent = string(data)
	} else if !os.IsNotExist(readErr) {
		return fmt.Errorf("failed to read greetd config: %w", readErr)
	}

	if !enabled {
		memoryPath := filepath.Join(cacheDir, ".local/state/memory.json")
		if err := clearGreeterAutoLoginMemory(memoryPath, sudoPassword); err != nil && logFunc != nil {
			logFunc(fmt.Sprintf("⚠ Warning: Failed to clear greeter auto-login memory flag: %v", err))
		}
		newConfig := upsertInitialSession(configContent, "", "", false)
		if newConfig == configContent {
			if logFunc != nil {
				logFunc("✓ Greeter auto-login disabled")
			}
			return nil
		}
		return writeGreetdConfig(configPath, newConfig, logFunc, sudoPassword, "✓ Disabled greeter auto-login")
	}

	if loginUser == "" || sessionExec == "" {
		if logFunc != nil {
			logFunc("⚠ Greeter auto-login is enabled but user or session is not configured yet. Log in manually once, then run sync.")
		}
		newConfig := upsertInitialSession(configContent, "", "", false)
		if newConfig != configContent {
			return writeGreetdConfig(configPath, newConfig, nil, sudoPassword, "")
		}
		return nil
	}

	newConfig := upsertInitialSession(configContent, loginUser, sessionExec, true)
	if newConfig == configContent {
		if logFunc != nil {
			logFunc(fmt.Sprintf("✓ Greeter auto-login already configured for %s", loginUser))
		}
		memoryPath := filepath.Join(cacheDir, ".local/state/memory.json")
		_ = clearGreeterAutoLoginMemory(memoryPath, sudoPassword)
		return nil
	}

	if err := writeGreetdConfig(configPath, newConfig, logFunc, sudoPassword, fmt.Sprintf("✓ Configured greeter auto-login for %s", loginUser)); err != nil {
		return err
	}
	memoryPath := filepath.Join(cacheDir, ".local/state/memory.json")
	if err := clearGreeterAutoLoginMemory(memoryPath, sudoPassword); err != nil && logFunc != nil {
		logFunc(fmt.Sprintf("⚠ Warning: Failed to clear greeter auto-login memory flag: %v", err))
	}
	return nil
}

func SyncGreeterAutoLoginOnly(logFunc func(string), sudoPassword string) error {
	homeDir, err := os.UserHomeDir()
	if err != nil {
		return fmt.Errorf("failed to get user home directory: %w", err)
	}
	return SyncGreetdAutoLogin(GreeterCacheDir, homeDir, logFunc, sudoPassword)
}

func DetectGreeterUser() string {
	passwdData, err := os.ReadFile("/etc/passwd")
	if err == nil {
		passwdContent := string(passwdData)

		if configData, cfgErr := os.ReadFile("/etc/greetd/config.toml"); cfgErr == nil {
			if configured := extractDefaultSessionUser(string(configData)); configured != "" && hasPasswdUser(passwdContent, configured) {
				return configured
			}
		}

		if user, found := findPasswdUser(passwdContent, "greeter", "greetd", "_greeter"); found {
			return user
		}
	} else {
		fmt.Fprintln(os.Stderr, "⚠ Warning: could not read /etc/passwd, defaulting greeter user to 'greeter'")
	}

	if configData, cfgErr := os.ReadFile("/etc/greetd/config.toml"); cfgErr == nil {
		if configured := extractDefaultSessionUser(string(configData)); configured != "" {
			return configured
		}
	}

	fmt.Fprintln(os.Stderr, "⚠ Warning: no greeter user found, defaulting to 'greeter'")
	return "greeter"
}

func resolveGreeterWrapperPath() string {
	if override := strings.TrimSpace(os.Getenv("HGS_GREETER_WRAPPER_CMD")); override != "" {
		return override
	}

	// Packaged installs only use the official wrapper; never fall back to /usr/local/bin.
	if IsGreeterPackaged() {
		packagedWrapper := "/usr/bin/hgs-greeter"
		if info, err := os.Stat(packagedWrapper); err == nil && !info.IsDir() && (info.Mode()&0o111) != 0 {
			return packagedWrapper
		}
		fmt.Fprintln(os.Stderr, "⚠ Warning: packaged hgs-greeter detected, but /usr/bin/hgs-greeter is missing or not executable")
		return packagedWrapper
	}

	for _, candidate := range []string{"/usr/bin/hgs-greeter", "/usr/local/bin/hgs-greeter"} {
		if info, err := os.Stat(candidate); err == nil && !info.IsDir() && (info.Mode()&0o111) != 0 {
			return candidate
		}
	}

	if path, err := exec.LookPath("hgs-greeter"); err == nil {
		resolved := path
		if realPath, realErr := filepath.EvalSymlinks(path); realErr == nil {
			resolved = realPath
		}
		if strings.HasPrefix(resolved, "/home/") || strings.HasPrefix(resolved, "/tmp/") {
			fmt.Fprintf(os.Stderr, "⚠ Warning: ignoring non-system hgs-greeter on PATH: %s\n", path)
		} else {
			return path
		}
	}

	return "/usr/bin/hgs-greeter"
}

func DetectCompositors() []string {
	var compositors []string

	if utils.CommandExists("Hyprland") {
		compositors = append(compositors, "Hyprland")
	}

	return compositors
}

func PromptCompositorChoice(compositors []string) (string, error) {
	fmt.Println("\nMultiple compositors detected:")
	for i, comp := range compositors {
		fmt.Printf("%d) %s\n", i+1, comp)
	}

	reader := bufio.NewReader(os.Stdin)
	fmt.Print("Choose compositor for greeter (1-2): ")
	response, err := reader.ReadString('\n')
	if err != nil {
		return "", fmt.Errorf("error reading input: %w", err)
	}

	response = strings.TrimSpace(response)
	switch response {
	case "1":
		return compositors[0], nil
	case "2":
		if len(compositors) > 1 {
			return compositors[1], nil
		}
		return "", fmt.Errorf("invalid choice")
	default:
		return "", fmt.Errorf("invalid choice")
	}
}

// EnsureGreetdInstalled checks if greetd is installed - greetd is a daemon in /usr/sbin on Debian/Ubuntu
func EnsureGreetdInstalled(logFunc func(string), sudoPassword string) error {
	greetdFound := utils.CommandExists("greetd")
	if !greetdFound {
		for _, p := range []string{"/usr/sbin/greetd", "/sbin/greetd"} {
			if _, err := os.Stat(p); err == nil {
				greetdFound = true
				break
			}
		}
	}
	if greetdFound {
		logFunc("✓ greetd is already installed")
		return nil
	}

	logFunc("greetd is not installed. Installing...")

	osInfo, err := distros.GetOSInfo()
	if err != nil {
		return fmt.Errorf("failed to detect OS: %w", err)
	}

	config, exists := distros.Registry[osInfo.Distribution.ID]
	if !exists {
		return fmt.Errorf("unsupported distribution for automatic greetd installation: %s", osInfo.Distribution.ID)
	}

	ctx := context.Background()
	var installCmd *exec.Cmd

	switch config.Family {
	case distros.FamilyArch:
		installCmd = privesc.ExecCommand(ctx, sudoPassword, "pacman -S --needed --noconfirm greetd")
	case distros.FamilyFedora:
		installCmd = privesc.ExecCommand(ctx, sudoPassword, "dnf install -y greetd")
	case distros.FamilySUSE:
		installCmd = privesc.ExecCommand(ctx, sudoPassword, "zypper install -y greetd")
	case distros.FamilyUbuntu, distros.FamilyDebian:
		installCmd = privesc.ExecCommand(ctx, sudoPassword, "apt-get install -y greetd")
	case distros.FamilyGentoo:
		installCmd = privesc.ExecCommand(ctx, sudoPassword, "emerge --ask n sys-apps/greetd")
	case distros.FamilyNix:
		return fmt.Errorf("on NixOS, please add greetd to your configuration.nix")
	default:
		return fmt.Errorf("unsupported distribution family for automatic greetd installation: %s", config.Family)
	}

	installCmd.Stdout = os.Stdout
	installCmd.Stderr = os.Stderr

	if err := installCmd.Run(); err != nil {
		return fmt.Errorf("failed to install greetd: %w", err)
	}

	logFunc("✓ greetd installed successfully")
	return nil
}

// IsGreeterPackaged returns true if hgs-greeter was installed from a system package.
func IsGreeterPackaged() bool {
	if !utils.CommandExists("hgs-greeter") {
		return false
	}
	packagedPath := "/usr/share/quickshell/hgs-greeter"
	info, err := os.Stat(packagedPath)
	return err == nil && info.IsDir()
}

// HasLegacyLocalGreeterWrapper returns true when a manually installed wrapper exists.
func HasLegacyLocalGreeterWrapper() bool {
	info, err := os.Stat("/usr/local/bin/hgs-greeter")
	return err == nil && !info.IsDir()
}

// TryInstallGreeterPackage attempts to install hgs-greeter from the distro's official repo.
func TryInstallGreeterPackage(logFunc func(string), sudoPassword string) bool {
	osInfo, err := distros.GetOSInfo()
	if err != nil {
		return false
	}
	config, exists := distros.Registry[osInfo.Distribution.ID]
	if !exists {
		return false
	}

	if IsGreeterPackaged() {
		logFunc("✓ hgs-greeter package already installed")
		return true
	}

	ctx := context.Background()
	var installCmd *exec.Cmd
	var failHint string

	switch config.Family {
	case distros.FamilyDebian:
		obsSlug := getDebianOBSSlug(osInfo)
		keyURL := fmt.Sprintf("https://download.opensuse.org/repositories/home:AvengeMedia:coastlinesec/%s/Release.key", obsSlug)
		repoLine := fmt.Sprintf("deb [signed-by=/etc/apt/keyrings/coastlinesec.gpg] https://download.opensuse.org/repositories/home:/AvengeMedia:/coastlinesec/%s/ /", obsSlug)
		failHint = fmt.Sprintf("⚠ hgs-greeter install failed. Add OBS repo manually:\nsudo apt-get install -y gnupg\nsudo mkdir -p /etc/apt/keyrings\ncurl -fsSL %s | sudo gpg --dearmor -o /etc/apt/keyrings/coastlinesec.gpg\necho '%s' | sudo tee /etc/apt/sources.list.d/coastlinesec.list\nsudo apt update && sudo apt-get install -y hgs-greeter", keyURL, repoLine)
		logFunc(fmt.Sprintf("Adding CoastLineSec OBS repository (%s)...", obsSlug))
		if _, err := exec.LookPath("gpg"); err != nil {
			logFunc("Installing gnupg for OBS repository key import...")
			installGPGCmd := privesc.ExecCommand(ctx, sudoPassword, "apt-get install -y gnupg")
			installGPGCmd.Stdout = os.Stdout
			installGPGCmd.Stderr = os.Stderr
			if err := installGPGCmd.Run(); err != nil {
				logFunc(fmt.Sprintf("⚠ Failed to install gnupg: %v", err))
			}
		}
		mkdirCmd := privesc.ExecCommand(ctx, sudoPassword, "mkdir -p /etc/apt/keyrings")
		mkdirCmd.Stdout = os.Stdout
		mkdirCmd.Stderr = os.Stderr
		mkdirCmd.Run()
		addKeyCmd := privesc.ExecCommand(ctx, sudoPassword,
			fmt.Sprintf(`bash -c "curl -fsSL %s | gpg --dearmor -o /etc/apt/keyrings/coastlinesec.gpg"`, keyURL))
		addKeyCmd.Stdout = os.Stdout
		addKeyCmd.Stderr = os.Stderr
		addKeyCmd.Run()
		addRepoCmd := privesc.ExecCommand(ctx, sudoPassword,
			fmt.Sprintf(`bash -c "echo '%s' > /etc/apt/sources.list.d/coastlinesec.list"`, repoLine))
		addRepoCmd.Stdout = os.Stdout
		addRepoCmd.Stderr = os.Stderr
		addRepoCmd.Run()
		privesc.ExecCommand(ctx, sudoPassword, "apt-get update").Run()
		installCmd = privesc.ExecCommand(ctx, sudoPassword, "apt-get install -y hgs-greeter")
	case distros.FamilySUSE:
		repoURL := getOpenSUSEOBSRepoURL(osInfo)
		failHint = fmt.Sprintf("⚠ hgs-greeter install failed. Add OBS repo manually:\nsudo zypper addrepo %s\nsudo zypper refresh && sudo zypper install hgs-greeter", repoURL)
		logFunc("Adding CoastLineSec OBS repository...")
		addRepoCmd := privesc.ExecCommand(ctx, sudoPassword, fmt.Sprintf("zypper addrepo %s", repoURL))
		addRepoCmd.Stdout = os.Stdout
		addRepoCmd.Stderr = os.Stderr
		addRepoCmd.Run()
		privesc.ExecCommand(ctx, sudoPassword, "zypper refresh").Run()
		installCmd = privesc.ExecCommand(ctx, sudoPassword, "zypper install -y hgs-greeter")
	case distros.FamilyUbuntu:
		failHint = "⚠ hgs-greeter install failed. Add PPA manually: sudo add-apt-repository ppa:avengemedia/coastlinesec && sudo apt-get update && sudo apt-get install -y hgs-greeter"
		logFunc("Enabling PPA ppa:avengemedia/coastlinesec...")
		ppacmd := privesc.ExecCommand(ctx, sudoPassword, "add-apt-repository -y ppa:avengemedia/coastlinesec")
		ppacmd.Stdout = os.Stdout
		ppacmd.Stderr = os.Stderr
		ppacmd.Run()
		privesc.ExecCommand(ctx, sudoPassword, "apt-get update").Run()
		installCmd = privesc.ExecCommand(ctx, sudoPassword, "apt-get install -y hgs-greeter")
	case distros.FamilyFedora:
		failHint = "⚠ hgs-greeter install failed. Enable COPR manually: sudo dnf copr enable avengemedia/coastlinesec && sudo dnf install hgs-greeter"
		logFunc("Enabling COPR avengemedia/coastlinesec...")
		coprcmd := privesc.ExecCommand(ctx, sudoPassword, "dnf copr enable -y avengemedia/coastlinesec")
		coprcmd.Stdout = os.Stdout
		coprcmd.Stderr = os.Stderr
		coprcmd.Run()
		installCmd = privesc.ExecCommand(ctx, sudoPassword, "dnf install -y hgs-greeter")
	case distros.FamilyArch:
		aurHelper := ""
		for _, helper := range []string{"paru", "yay"} {
			if _, err := exec.LookPath(helper); err == nil {
				aurHelper = helper
				break
			}
		}
		if aurHelper == "" {
			logFunc("⚠ No AUR helper found (paru/yay). Install greetd-hgs-greeter-git from AUR: https://aur.archlinux.org/packages/greetd-hgs-greeter-git")
			return false
		}
		failHint = fmt.Sprintf("⚠ hgs-greeter install failed. Install from AUR: %s -S greetd-hgs-greeter-git", aurHelper)
		installCmd = exec.CommandContext(ctx, aurHelper, "-S", "--noconfirm", "greetd-hgs-greeter-git")
	default:
		return false
	}

	logFunc("Installing hgs-greeter from official repository...")
	installCmd.Stdout = os.Stdout
	installCmd.Stderr = os.Stderr

	if err := installCmd.Run(); err != nil {
		logFunc(failHint)
		return false
	}

	logFunc("✓ hgs-greeter package installed")
	return true
}

// CopyGreeterFiles installs the hgs-greeter wrapper and sets up cache directory
func CopyGreeterFiles(hgsPath, compositor string, logFunc func(string), sudoPassword string) error {
	if IsGreeterPackaged() {
		logFunc("✓ hgs-greeter package already installed")
	} else {
		if hgsPath == "" {
			return fmt.Errorf("hgs path is required for manual hgs-greeter wrapper installs")
		}

		assetsDir := filepath.Join(hgsPath, "Modules", "Greetd", "assets")
		wrapperSrc := filepath.Join(assetsDir, "hgs-greeter")

		if _, err := os.Stat(wrapperSrc); os.IsNotExist(err) {
			return fmt.Errorf("hgs-greeter wrapper not found at %s", wrapperSrc)
		}

		wrapperDst := "/usr/local/bin/hgs-greeter"
		action := "Installed"
		if info, err := os.Stat(wrapperDst); err == nil && !info.IsDir() {
			action = "Updated"
		}
		if err := privesc.Run(context.Background(), sudoPassword, "cp", wrapperSrc, wrapperDst); err != nil {
			return fmt.Errorf("failed to copy hgs-greeter wrapper: %w", err)
		}
		logFunc(fmt.Sprintf("✓ %s hgs-greeter wrapper at %s", action, wrapperDst))

		if err := privesc.Run(context.Background(), sudoPassword, "chmod", "+x", wrapperDst); err != nil {
			return fmt.Errorf("failed to make wrapper executable: %w", err)
		}

		osInfo, err := distros.GetOSInfo()
		if err == nil {
			if config, exists := distros.Registry[osInfo.Distribution.ID]; exists && (config.Family == distros.FamilyFedora || config.Family == distros.FamilySUSE) {
				if err := privesc.Run(context.Background(), sudoPassword, "semanage", "fcontext", "-a", "-t", "bin_t", wrapperDst); err != nil {
					logFunc(fmt.Sprintf("⚠ Warning: Failed to set SELinux fcontext: %v", err))
				} else {
					logFunc("✓ Set SELinux fcontext for hgs-greeter")
				}

				if err := privesc.Run(context.Background(), sudoPassword, "restorecon", "-v", wrapperDst); err != nil {
					logFunc(fmt.Sprintf("⚠ Warning: Failed to restore SELinux context: %v", err))
				} else {
					logFunc("✓ Restored SELinux context for hgs-greeter")
				}
			}
		}
	}

	if err := EnsureGreeterCacheDir(logFunc, sudoPassword); err != nil {
		return err
	}

	return nil
}

// EnsureGreeterCacheDir creates /var/cache/hgs-greeter with correct ownership if it does not exist.
// It is safe to call multiple times (idempotent) and will repair ownership/mode
// when the directory already exists with stale permissions.
func EnsureGreeterCacheDir(logFunc func(string), sudoPassword string) error {
	cacheDir := GreeterCacheDir
	created := false
	if info, err := os.Stat(cacheDir); err != nil {
		if !os.IsNotExist(err) {
			return fmt.Errorf("failed to stat cache directory: %w", err)
		}
		if err := privesc.Run(context.Background(), sudoPassword, "mkdir", "-p", cacheDir); err != nil {
			return fmt.Errorf("failed to create cache directory: %w", err)
		}
		created = true
	} else if !info.IsDir() {
		return fmt.Errorf("cache path exists but is not a directory: %s", cacheDir)
	}

	group := DetectGreeterGroup()
	daemonUser := DetectGreeterUser()
	preferredOwner := fmt.Sprintf("%s:%s", daemonUser, group)
	owner := preferredOwner
	if err := privesc.Run(context.Background(), sudoPassword, "chown", owner, cacheDir); err != nil {
		// Some setups may not have a matching daemon user at this moment; fall back
		// to root:<group> while still allowing group-writable greeter runtime access.
		fallbackOwner := fmt.Sprintf("root:%s", group)
		if fallbackErr := privesc.Run(context.Background(), sudoPassword, "chown", fallbackOwner, cacheDir); fallbackErr != nil {
			return fmt.Errorf("failed to set cache directory owner (preferred %s: %v; fallback %s: %w)", preferredOwner, err, fallbackOwner, fallbackErr)
		}
		owner = fallbackOwner
	}

	if err := privesc.Run(context.Background(), sudoPassword, "chmod", "2770", cacheDir); err != nil {
		return fmt.Errorf("failed to set cache directory permissions: %w", err)
	}

	runtimeDirs := []string{
		filepath.Join(cacheDir, "users"),
		filepath.Join(cacheDir, ".local"),
		filepath.Join(cacheDir, ".local", "state"),
		filepath.Join(cacheDir, ".local", "share"),
		filepath.Join(cacheDir, ".cache"),
	}
	for _, dir := range runtimeDirs {
		if err := privesc.Run(context.Background(), sudoPassword, "mkdir", "-p", dir); err != nil {
			return fmt.Errorf("failed to create cache runtime directory %s: %w", dir, err)
		}
		if err := privesc.Run(context.Background(), sudoPassword, "chown", owner, dir); err != nil {
			return fmt.Errorf("failed to set owner for cache runtime directory %s: %w", dir, err)
		}
		if err := privesc.Run(context.Background(), sudoPassword, "chmod", "2770", dir); err != nil {
			return fmt.Errorf("failed to set permissions for cache runtime directory %s: %w", dir, err)
		}
	}

	legacyMemoryPath := filepath.Join(cacheDir, "memory.json")
	stateMemoryPath := filepath.Join(cacheDir, ".local", "state", "memory.json")
	if err := ensureGreeterMemoryCompatLink(logFunc, sudoPassword, legacyMemoryPath, stateMemoryPath); err != nil {
		return err
	}

	if isSELinuxEnforcing() && utils.CommandExists("restorecon") {
		if err := privesc.Run(context.Background(), sudoPassword, "restorecon", "-Rv", cacheDir); err != nil {
			logFunc(fmt.Sprintf("⚠ Warning: Failed to restore SELinux context for %s: %v", cacheDir, err))
		}
	}

	if created {
		logFunc(fmt.Sprintf("✓ Created cache directory %s (owner: %s, mode: 2770)", cacheDir, owner))
	} else {
		logFunc(fmt.Sprintf("✓ Ensured cache directory %s permissions (owner: %s, mode: 2770)", cacheDir, owner))
	}
	return nil
}

func isSELinuxEnforcing() bool {
	data, err := os.ReadFile("/sys/fs/selinux/enforce")
	if err != nil {
		return false
	}
	return strings.TrimSpace(string(data)) == "1"
}

func ensureGreeterMemoryCompatLink(logFunc func(string), sudoPassword, legacyPath, statePath string) error {
	info, err := os.Lstat(legacyPath)
	if err == nil && info.Mode().IsRegular() {
		if _, stateErr := os.Stat(statePath); os.IsNotExist(stateErr) {
			if copyErr := privesc.Run(context.Background(), sudoPassword, "cp", "-f", legacyPath, statePath); copyErr != nil {
				logFunc(fmt.Sprintf("⚠ Warning: Failed to migrate legacy greeter memory file to %s: %v", statePath, copyErr))
			}
		}
	}

	if err := privesc.Run(context.Background(), sudoPassword, "ln", "-sfn", statePath, legacyPath); err != nil {
		return fmt.Errorf("failed to create greeter memory compatibility symlink %s -> %s: %w", legacyPath, statePath, err)
	}

	return nil
}

// IsAppArmorEnabled reports whether AppArmor is active on the running kernel.
func IsAppArmorEnabled() bool {
	data, err := os.ReadFile("/sys/module/apparmor/parameters/enabled")
	if err != nil {
		return false
	}
	return strings.HasPrefix(strings.TrimSpace(strings.ToLower(string(data))), "y")
}

// InstallAppArmorProfile installs the bundled AppArmor profile and reloads it. No-op on NixOS or non-AppArmor systems.
func InstallAppArmorProfile(logFunc func(string), sudoPassword string) error {
	if IsNixOS() {
		logFunc("  ℹ Skipping AppArmor profile on NixOS (manage via security.apparmor.policies)")
		return nil
	}

	if !IsAppArmorEnabled() {
		return nil
	}

	if err := privesc.Run(context.Background(), sudoPassword, "mkdir", "-p", "/etc/apparmor.d"); err != nil {
		return fmt.Errorf("failed to create /etc/apparmor.d: %w", err)
	}

	tmp, err := os.CreateTemp("", "hgs-apparmor-*")
	if err != nil {
		return fmt.Errorf("failed to create temp file for AppArmor profile: %w", err)
	}
	tmpPath := tmp.Name()
	defer os.Remove(tmpPath)

	if _, err := tmp.Write(appArmorProfileData); err != nil {
		tmp.Close()
		return fmt.Errorf("failed to write AppArmor profile: %w", err)
	}
	tmp.Close()

	if err := privesc.Run(context.Background(), sudoPassword, "cp", tmpPath, appArmorProfileDest); err != nil {
		return fmt.Errorf("failed to install AppArmor profile to %s: %w", appArmorProfileDest, err)
	}
	if err := privesc.Run(context.Background(), sudoPassword, "chmod", "644", appArmorProfileDest); err != nil {
		return fmt.Errorf("failed to set AppArmor profile permissions: %w", err)
	}

	if utils.CommandExists("apparmor_parser") {
		if err := privesc.Run(context.Background(), sudoPassword, "apparmor_parser", "-r", appArmorProfileDest); err != nil {
			logFunc(fmt.Sprintf("  ⚠ AppArmor profile installed but reload failed: %v", err))
			logFunc("    Run: sudo apparmor_parser -r " + appArmorProfileDest)
		} else {
			logFunc("  ✓ AppArmor profile installed and loaded (complain mode)")
		}
	} else {
		logFunc("  ✓ AppArmor profile installed at " + appArmorProfileDest)
		logFunc("    apparmor_parser not found — profile will be loaded on next boot")
	}

	return nil
}

// UninstallAppArmorProfile removes the HGS AppArmor profile and reloads AppArmor.
// It is a no-op when AppArmor is not active or the profile does not exist.
func UninstallAppArmorProfile(logFunc func(string), sudoPassword string) error {
	if IsNixOS() {
		return nil
	}
	if _, err := os.Stat("/sys/module/apparmor"); os.IsNotExist(err) {
		return nil
	}
	if _, err := os.Stat(appArmorProfileDest); os.IsNotExist(err) {
		return nil
	}

	if utils.CommandExists("apparmor_parser") {
		_ = privesc.Run(context.Background(), sudoPassword, "apparmor_parser", "--remove", appArmorProfileDest)
	}
	if err := privesc.Run(context.Background(), sudoPassword, "rm", "-f", appArmorProfileDest); err != nil {
		return fmt.Errorf("failed to remove AppArmor profile: %w", err)
	}
	logFunc("  ✓ Removed HGS AppArmor profile")
	return nil
}

// EnsureACLInstalled installs the acl package (setfacl/getfacl) if not already present
func EnsureACLInstalled(logFunc func(string), sudoPassword string) error {
	if utils.CommandExists("setfacl") {
		return nil
	}

	logFunc("setfacl not found – installing acl package...")

	osInfo, err := distros.GetOSInfo()
	if err != nil {
		return fmt.Errorf("failed to detect OS: %w", err)
	}

	config, exists := distros.Registry[osInfo.Distribution.ID]
	if !exists {
		return fmt.Errorf("unsupported distribution for automatic acl installation: %s", osInfo.Distribution.ID)
	}

	ctx := context.Background()
	var installCmd *exec.Cmd

	switch config.Family {
	case distros.FamilyArch:
		installCmd = privesc.ExecCommand(ctx, sudoPassword, "pacman -S --needed --noconfirm acl")
	case distros.FamilyFedora:
		installCmd = privesc.ExecCommand(ctx, sudoPassword, "dnf install -y acl")
	case distros.FamilySUSE:
		installCmd = privesc.ExecCommand(ctx, sudoPassword, "zypper install -y acl")
	case distros.FamilyUbuntu, distros.FamilyDebian:
		installCmd = privesc.ExecCommand(ctx, sudoPassword, "apt-get install -y acl")
	case distros.FamilyGentoo:
		installCmd = privesc.ExecCommand(ctx, sudoPassword, "emerge --ask n sys-fs/acl")
	case distros.FamilyNix:
		return fmt.Errorf("on NixOS, please add pkgs.acl to your configuration.nix")
	default:
		return fmt.Errorf("unsupported distribution family for automatic acl installation: %s", config.Family)
	}

	installCmd.Stdout = os.Stdout
	installCmd.Stderr = os.Stderr
	if err := installCmd.Run(); err != nil {
		return fmt.Errorf("failed to install acl: %w", err)
	}

	logFunc("✓ acl package installed")
	return nil
}

// SetupParentDirectoryACLs sets ACLs on parent directories to allow traversal
func SetupParentDirectoryACLs(logFunc func(string), sudoPassword string) error {
	if err := EnsureACLInstalled(logFunc, sudoPassword); err != nil {
		logFunc(fmt.Sprintf("⚠ Warning: could not install acl package: %v", err))
		logFunc("  ACL permissions will be skipped; theme sync may not work correctly.")
		return nil
	}
	if !utils.CommandExists("setfacl") {
		logFunc("⚠ Warning: setfacl still not available after install attempt; skipping ACL setup.")
		return nil
	}

	homeDir, err := os.UserHomeDir()
	if err != nil {
		return fmt.Errorf("failed to get user home directory: %w", err)
	}

	parentDirs := []struct {
		path string
		desc string
	}{
		{homeDir, "home directory"},
		{filepath.Join(homeDir, ".config"), ".config directory"},
		{filepath.Join(homeDir, ".local"), ".local directory"},
		{filepath.Join(homeDir, ".cache"), ".cache directory"},
		{filepath.Join(homeDir, ".local", "state"), ".local/state directory"},
		{filepath.Join(homeDir, ".local", "share"), ".local/share directory"},
	}

	group := DetectGreeterGroup()

	logFunc("\nSetting up parent directory ACLs for greeter user access...")

	for _, dir := range parentDirs {
		if _, err := os.Stat(dir.path); os.IsNotExist(err) {
			if err := os.MkdirAll(dir.path, 0o755); err != nil {
				logFunc(fmt.Sprintf("⚠ Warning: Could not create %s: %v", dir.desc, err))
				continue
			}
		}

		// Group ACL covers daemon users regardless of username (e.g. greetd ≠ greeter on Fedora).
		if err := privesc.Run(context.Background(), sudoPassword, "setfacl", "-m", fmt.Sprintf("g:%s:rX", group), dir.path); err != nil {
			logFunc(fmt.Sprintf("⚠ Warning: Failed to set ACL on %s: %v", dir.desc, err))
			logFunc(fmt.Sprintf("  You may need to run manually: setfacl -m g:%s:rX %s", group, dir.path))
			continue
		}

		logFunc(fmt.Sprintf("✓ Set ACL on %s", dir.desc))
	}

	return nil
}

// RemediateStaleACLs removes user-based ACLs left by older binary versions. Best-effort.
func RemediateStaleACLs(logFunc func(string), sudoPassword string) {
	if !utils.CommandExists("setfacl") {
		return
	}

	homeDir, err := os.UserHomeDir()
	if err != nil {
		return
	}

	passwdData, err := os.ReadFile("/etc/passwd")
	if err != nil {
		return
	}

	dirs := []string{
		homeDir,
		filepath.Join(homeDir, ".config"),
		filepath.Join(homeDir, ".config", "HyprGlassShell"),
		filepath.Join(homeDir, ".cache"),
		filepath.Join(homeDir, ".cache", "HyprGlassShell"),
		filepath.Join(homeDir, ".local"),
		filepath.Join(homeDir, ".local", "state"),
		filepath.Join(homeDir, ".local", "share"),
	}

	passwdContent := string(passwdData)
	staleUsers := []string{"greeter", "greetd", "_greeter"}
	existingUsers := make([]string, 0, len(staleUsers))
	for _, user := range staleUsers {
		if hasPasswdUser(passwdContent, user) {
			existingUsers = append(existingUsers, user)
		}
	}
	if len(existingUsers) == 0 {
		return
	}

	cleaned := false
	for _, dir := range dirs {
		if _, err := os.Stat(dir); err != nil {
			continue
		}
		for _, user := range existingUsers {
			_ = privesc.Run(context.Background(), sudoPassword, "setfacl", "-x", fmt.Sprintf("u:%s", user), dir)
			cleaned = true
		}
	}
	if cleaned {
		logFunc("✓ Cleaned up stale user-based ACLs from previous versions")
	}
}

// RemediateStaleAppArmor removes any AppArmor profile installed by an older binary on
// systems where AppArmor is not active.
func RemediateStaleAppArmor(logFunc func(string), sudoPassword string) {
	if IsAppArmorEnabled() {
		return
	}
	if _, err := os.Stat(appArmorProfileDest); os.IsNotExist(err) {
		return
	}
	logFunc("ℹ Removing stale AppArmor profile (AppArmor is not active on this system)")
	_ = UninstallAppArmorProfile(logFunc, sudoPassword)
}

func SetupHGSGroup(logFunc func(string), sudoPassword string) error {
	homeDir, err := os.UserHomeDir()
	if err != nil {
		return fmt.Errorf("failed to get user home directory: %w", err)
	}

	currentUser := os.Getenv("USER")
	if currentUser == "" {
		currentUser = os.Getenv("LOGNAME")
	}
	if currentUser == "" {
		return fmt.Errorf("failed to determine current user")
	}

	group := DetectGreeterGroup()

	// Create the group if it doesn't exist yet (e.g. before greetd package is installed).
	if !utils.HasGroup(group) {
		if err := privesc.Run(context.Background(), sudoPassword, "groupadd", "-r", group); err != nil {
			return fmt.Errorf("failed to create %s group: %w", group, err)
		}
		logFunc(fmt.Sprintf("✓ Created system group %s", group))
	}

	groupsCmd := exec.Command("groups", currentUser)
	groupsOutput, err := groupsCmd.Output()
	if err == nil && strings.Contains(string(groupsOutput), group) {
		logFunc(fmt.Sprintf("✓ %s is already in %s group", currentUser, group))
	} else {
		if err := privesc.Run(context.Background(), sudoPassword, "usermod", "-aG", group, currentUser); err != nil {
			return fmt.Errorf("failed to add %s to %s group: %w", currentUser, group, err)
		}
		logFunc(fmt.Sprintf("✓ Added %s to %s group (logout/login required for changes to take effect)", currentUser, group))
	}

	// Also add the daemon user (e.g. greetd on Fedora) so group ACLs apply to the running process.
	daemonUser := DetectGreeterUser()
	if daemonUser != currentUser {
		daemonGroupsCmd := exec.Command("groups", daemonUser)
		daemonGroupsOutput, daemonGroupsErr := daemonGroupsCmd.Output()
		if daemonGroupsErr == nil {
			if strings.Contains(string(daemonGroupsOutput), group) {
				logFunc(fmt.Sprintf("✓ Greeter daemon user %s is already in %s group", daemonUser, group))
			} else {
				if err := privesc.Run(context.Background(), sudoPassword, "usermod", "-aG", group, daemonUser); err != nil {
					logFunc(fmt.Sprintf("⚠ Warning: could not add %s to %s group: %v", daemonUser, group, err))
				} else {
					logFunc(fmt.Sprintf("✓ Added greeter daemon user %s to %s group", daemonUser, group))
				}
			}
		}
	}

	configDirs := []struct {
		path string
		desc string
	}{
		{filepath.Join(homeDir, ".config", "HyprGlassShell"), "HyprGlassShell config"},
		{filepath.Join(homeDir, ".local", "state", "HyprGlassShell"), "HyprGlassShell state"},
		{filepath.Join(homeDir, ".cache", "HyprGlassShell"), "HyprGlassShell cache"},
		{filepath.Join(homeDir, ".cache", "quickshell"), "quickshell cache"},
		{filepath.Join(homeDir, ".config", "quickshell"), "quickshell config"},
		{filepath.Join(homeDir, ".local", "share", "wayland-sessions"), "wayland sessions"},
		{filepath.Join(homeDir, ".local", "share", "xsessions"), "xsessions"},
	}

	for _, dir := range configDirs {
		if _, err := os.Stat(dir.path); os.IsNotExist(err) {
			if err := os.MkdirAll(dir.path, 0o755); err != nil {
				logFunc(fmt.Sprintf("⚠ Warning: Could not create %s: %v", dir.path, err))
				continue
			}
		}

		if err := privesc.Run(context.Background(), sudoPassword, "chgrp", "-R", group, dir.path); err != nil {
			logFunc(fmt.Sprintf("⚠ Warning: Failed to set group for %s: %v", dir.desc, err))
			continue
		}

		if err := privesc.Run(context.Background(), sudoPassword, "chmod", "-R", "g+rX", dir.path); err != nil {
			logFunc(fmt.Sprintf("⚠ Warning: Failed to set permissions for %s: %v", dir.desc, err))
			continue
		}

		logFunc(fmt.Sprintf("✓ Set group permissions for %s", dir.desc))
	}

	if err := SetupParentDirectoryACLs(logFunc, sudoPassword); err != nil {
		return fmt.Errorf("failed to setup parent directory ACLs: %w", err)
	}

	return nil
}

type GreeterColorSyncInfo struct {
	SourcePath                   string
	ThemeName                    string
	UsesDynamicWallpaperOverride bool
}

type greeterThemeSyncSettings struct {
	CurrentThemeName     string `json:"currentThemeName"`
	GreeterWallpaperPath string `json:"greeterWallpaperPath"`
	MatugenScheme        string `json:"matugenScheme"`
	IconTheme            string `json:"iconTheme"`
}

type greeterThemeSyncSession struct {
	IsLightMode bool `json:"isLightMode"`
}

type greeterThemeSyncState struct {
	ThemeName                    string
	GreeterWallpaperPath         string
	ResolvedGreeterWallpaperPath string
	MatugenScheme                string
	IconTheme                    string
	IsLightMode                  bool
	UsesDynamicWallpaperOverride bool
}

func defaultGreeterColorsSource(homeDir string) string {
	return filepath.Join(homeDir, ".cache", "HyprGlassShell", "hgs-colors.json")
}

func greeterOverrideColorsStateDir(homeDir string) string {
	return filepath.Join(homeDir, ".cache", "HyprGlassShell", "greeter-colors")
}

func greeterOverrideColorsSource(homeDir string) string {
	return filepath.Join(greeterOverrideColorsStateDir(homeDir), "hgs-colors.json")
}

func readOptionalJSONFile(path string, dst any) error {
	data, err := os.ReadFile(path)
	if err != nil {
		if os.IsNotExist(err) {
			return nil
		}
		return err
	}
	if strings.TrimSpace(string(data)) == "" {
		return nil
	}
	return json.Unmarshal(data, dst)
}

func readGreeterThemeSyncSettings(homeDir string) (greeterThemeSyncSettings, error) {
	settings := greeterThemeSyncSettings{
		CurrentThemeName: "purple",
		MatugenScheme:    "scheme-tonal-spot",
		IconTheme:        "System Default",
	}
	settingsPath := filepath.Join(homeDir, ".config", "HyprGlassShell", "settings.json")
	if err := readOptionalJSONFile(settingsPath, &settings); err != nil {
		return greeterThemeSyncSettings{}, fmt.Errorf("failed to parse settings at %s: %w", settingsPath, err)
	}
	return settings, nil
}

func readGreeterThemeSyncSession(homeDir string) (greeterThemeSyncSession, error) {
	session := greeterThemeSyncSession{}
	sessionPath := filepath.Join(homeDir, ".local", "state", "HyprGlassShell", "session.json")
	if err := readOptionalJSONFile(sessionPath, &session); err != nil {
		return greeterThemeSyncSession{}, fmt.Errorf("failed to parse session at %s: %w", sessionPath, err)
	}
	return session, nil
}

func resolveGreeterThemeSyncState(homeDir string) (greeterThemeSyncState, error) {
	settings, err := readGreeterThemeSyncSettings(homeDir)
	if err != nil {
		return greeterThemeSyncState{}, err
	}
	session, err := readGreeterThemeSyncSession(homeDir)
	if err != nil {
		return greeterThemeSyncState{}, err
	}

	resolvedWallpaperPath := ""
	if settings.GreeterWallpaperPath != "" {
		resolvedWallpaperPath = settings.GreeterWallpaperPath
		if !filepath.IsAbs(resolvedWallpaperPath) {
			resolvedWallpaperPath = filepath.Join(homeDir, resolvedWallpaperPath)
		}
	}

	usesDynamicWallpaperOverride := strings.EqualFold(strings.TrimSpace(settings.CurrentThemeName), "dynamic") && resolvedWallpaperPath != ""

	return greeterThemeSyncState{
		ThemeName:                    settings.CurrentThemeName,
		GreeterWallpaperPath:         settings.GreeterWallpaperPath,
		ResolvedGreeterWallpaperPath: resolvedWallpaperPath,
		MatugenScheme:                settings.MatugenScheme,
		IconTheme:                    settings.IconTheme,
		IsLightMode:                  session.IsLightMode,
		UsesDynamicWallpaperOverride: usesDynamicWallpaperOverride,
	}, nil
}

func (s greeterThemeSyncState) effectiveColorsSource(homeDir string) string {
	if s.UsesDynamicWallpaperOverride {
		return greeterOverrideColorsSource(homeDir)
	}
	return defaultGreeterColorsSource(homeDir)
}

func ResolveGreeterColorSyncInfo(homeDir string) (GreeterColorSyncInfo, error) {
	state, err := resolveGreeterThemeSyncState(homeDir)
	if err != nil {
		return GreeterColorSyncInfo{}, err
	}
	return GreeterColorSyncInfo{
		SourcePath:                   state.effectiveColorsSource(homeDir),
		ThemeName:                    state.ThemeName,
		UsesDynamicWallpaperOverride: state.UsesDynamicWallpaperOverride,
	}, nil
}

func ensureGreeterSyncSourceFile(path string) error {
	sourceDir := filepath.Dir(path)
	if err := os.MkdirAll(sourceDir, 0o755); err != nil {
		return fmt.Errorf("failed to create source directory %s: %w", sourceDir, err)
	}

	if _, err := os.Stat(path); os.IsNotExist(err) {
		if err := os.WriteFile(path, []byte("{}"), 0o644); err != nil {
			return fmt.Errorf("failed to create source file %s: %w", path, err)
		}
	} else if err != nil {
		return fmt.Errorf("failed to inspect source file %s: %w", path, err)
	}

	return nil
}

func syncGreeterDynamicOverrideColors(hgsPath, homeDir string, state greeterThemeSyncState, logFunc func(string)) error {
	if !state.UsesDynamicWallpaperOverride {
		return nil
	}

	st, err := os.Stat(state.ResolvedGreeterWallpaperPath)
	if err != nil {
		return fmt.Errorf("configured greeter wallpaper not found at %s: %w", state.ResolvedGreeterWallpaperPath, err)
	}
	if st.IsDir() {
		return fmt.Errorf("configured greeter wallpaper path points to a directory: %s", state.ResolvedGreeterWallpaperPath)
	}

	mode := matugen.ColorModeDark
	if state.IsLightMode {
		mode = matugen.ColorModeLight
	}

	opts := matugen.Options{
		StateDir:         greeterOverrideColorsStateDir(homeDir),
		ShellDir:         hgsPath,
		ConfigDir:        filepath.Join(homeDir, ".config"),
		Kind:             "image",
		Value:            state.ResolvedGreeterWallpaperPath,
		Mode:             mode,
		IconTheme:        state.IconTheme,
		MatugenType:      state.MatugenScheme,
		RunUserTemplates: false,
		ColorsOnly:       true,
	}

	err = matugen.Run(opts)
	switch {
	case errors.Is(err, matugen.ErrNoChanges):
		logFunc("✓ Greeter dynamic override colors already up to date")
		return nil
	case err != nil:
		return fmt.Errorf("failed to generate greeter dynamic colors from wallpaper override: %w", err)
	default:
		logFunc("✓ Generated greeter dynamic colors from wallpaper override")
		return nil
	}
}

func syncGreeterColorSource(homeDir, cacheDir string, state greeterThemeSyncState, logFunc func(string), sudoPassword string) error {
	source := state.effectiveColorsSource(homeDir)
	if !state.UsesDynamicWallpaperOverride {
		if err := ensureGreeterSyncSourceFile(source); err != nil {
			return err
		}
	} else if _, err := os.Stat(source); err != nil {
		return fmt.Errorf("expected generated greeter colors at %s: %w", source, err)
	}

	target := filepath.Join(cacheDir, "colors.json")
	_ = privesc.Run(context.Background(), sudoPassword, "rm", "-f", target)
	if err := privesc.Run(context.Background(), sudoPassword, "ln", "-sf", source, target); err != nil {
		return fmt.Errorf("failed to create symlink for wallpaper based theming (%s -> %s): %w", target, source, err)
	}

	if state.UsesDynamicWallpaperOverride {
		logFunc("✓ Synced wallpaper based theming (greeter wallpaper override)")
	} else {
		logFunc("✓ Synced wallpaper based theming")
	}

	return nil
}

func SyncHGSConfigs(hgsPath, compositor string, logFunc func(string), sudoPassword string) error {
	homeDir, err := os.UserHomeDir()
	if err != nil {
		return fmt.Errorf("failed to get user home directory: %w", err)
	}

	cacheDir := GreeterCacheDir

	symlinks := []struct {
		source string
		target string
		desc   string
	}{
		{
			source: filepath.Join(homeDir, ".config", "HyprGlassShell", "settings.json"),
			target: filepath.Join(cacheDir, "settings.json"),
			desc:   "core settings (theme, clock formats, etc)",
		},
		{
			source: filepath.Join(homeDir, ".local", "state", "HyprGlassShell", "session.json"),
			target: filepath.Join(cacheDir, "session.json"),
			desc:   "state (wallpaper configuration)",
		},
	}

	for _, link := range symlinks {
		sourceDir := filepath.Dir(link.source)
		if _, err := os.Stat(sourceDir); os.IsNotExist(err) {
			if err := os.MkdirAll(sourceDir, 0o755); err != nil {
				return fmt.Errorf("failed to create source directory %s for %s: %w", sourceDir, link.desc, err)
			}
		}

		if _, err := os.Stat(link.source); os.IsNotExist(err) {
			if err := os.WriteFile(link.source, []byte("{}"), 0o644); err != nil {
				return fmt.Errorf("failed to create source file %s for %s: %w", link.source, link.desc, err)
			}
		}

		_ = privesc.Run(context.Background(), sudoPassword, "rm", "-f", link.target)

		if err := privesc.Run(context.Background(), sudoPassword, "ln", "-sf", link.source, link.target); err != nil {
			return fmt.Errorf("failed to create symlink for %s (%s -> %s): %w", link.desc, link.target, link.source, err)
		}

		logFunc(fmt.Sprintf("✓ Synced %s", link.desc))
	}

	state, err := resolveGreeterThemeSyncState(homeDir)
	if err != nil {
		return fmt.Errorf("failed to resolve greeter color source: %w", err)
	}

	if err := syncGreeterDynamicOverrideColors(hgsPath, homeDir, state, logFunc); err != nil {
		return err
	}

	if err := syncGreeterColorSource(homeDir, cacheDir, state, logFunc, sudoPassword); err != nil {
		return err
	}

	if err := syncGreeterWallpaperOverride(cacheDir, logFunc, sudoPassword, state); err != nil {
		return fmt.Errorf("greeter wallpaper override sync failed: %w", err)
	}

	currentUser, err := user.Current()
	if err != nil {
		return fmt.Errorf("failed to resolve syncing user for per-user greeter cache: %w", err)
	}
	if err := syncUserGreeterCacheSlot(homeDir, cacheDir, currentUser.Username, state, logFunc, userSlotSyncOpts{
		sudoPassword: sudoPassword,
	}); err != nil {
		return fmt.Errorf("per-user greeter cache sync failed: %w", err)
	}

	if err := SyncGreetdAutoLogin(cacheDir, homeDir, logFunc, sudoPassword); err != nil {
		logFunc(fmt.Sprintf("⚠ Warning: greeter auto-login sync failed: %v", err))
	}

	return nil
}

func syncGreeterWallpaperOverride(cacheDir string, logFunc func(string), sudoPassword string, state greeterThemeSyncState) error {
	destPath := filepath.Join(cacheDir, "greeter_wallpaper_override.jpg")
	if state.ResolvedGreeterWallpaperPath == "" {
		if err := privesc.Run(context.Background(), sudoPassword, "rm", "-f", destPath); err != nil {
			return fmt.Errorf("failed to clear override file %s: %w", destPath, err)
		}
		logFunc("✓ Cleared greeter wallpaper override")
		return nil
	}
	if err := privesc.Run(context.Background(), sudoPassword, "rm", "-f", destPath); err != nil {
		return fmt.Errorf("failed to remove old override file %s: %w", destPath, err)
	}
	src := state.ResolvedGreeterWallpaperPath
	st, err := os.Stat(src)
	if err != nil {
		return fmt.Errorf("configured greeter wallpaper not found at %s: %w", src, err)
	}
	if st.IsDir() {
		return fmt.Errorf("configured greeter wallpaper path points to a directory: %s", src)
	}
	if err := privesc.Run(context.Background(), sudoPassword, "cp", src, destPath); err != nil {
		return fmt.Errorf("failed to copy override wallpaper to %s: %w", destPath, err)
	}
	greeterGroup := DetectGreeterGroup()
	daemonUser := DetectGreeterUser()
	if err := privesc.Run(context.Background(), sudoPassword, "chown", daemonUser+":"+greeterGroup, destPath); err != nil {
		if fallbackErr := privesc.Run(context.Background(), sudoPassword, "chown", "root:"+greeterGroup, destPath); fallbackErr != nil {
			return fmt.Errorf("failed to set override ownership on %s: %w", destPath, err)
		}
	}
	if err := privesc.Run(context.Background(), sudoPassword, "chmod", "644", destPath); err != nil {
		return fmt.Errorf("failed to set override permissions on %s: %w", destPath, err)
	}
	logFunc("✓ Synced greeter wallpaper override")
	return nil
}

func backupFileIfExists(sudoPassword string, path string, suffix string) error {
	if _, err := os.Stat(path); os.IsNotExist(err) {
		return nil
	} else if err != nil {
		return err
	}

	backupPath := fmt.Sprintf("%s%s-%s", path, suffix, time.Now().Format("20060102-150405"))
	if err := privesc.Run(context.Background(), sudoPassword, "cp", path, backupPath); err != nil {
		return err
	}
	return privesc.Run(context.Background(), sudoPassword, "chmod", "644", backupPath)
}

func ConfigureGreetd(hgsPath, compositor string, logFunc func(string), sudoPassword string) error {
	configPath := "/etc/greetd/config.toml"

	backupPath := fmt.Sprintf("%s.backup-%s", configPath, time.Now().Format("20060102-150405"))
	if err := backupFileIfExists(sudoPassword, configPath, ".backup"); err != nil {
		return fmt.Errorf("failed to backup greetd config: %w", err)
	}
	if _, err := os.Stat(configPath); err == nil {
		logFunc(fmt.Sprintf("✓ Backed up existing config to %s", backupPath))
	}

	greeterUser := DetectGreeterUser()

	var configContent string
	if data, err := os.ReadFile(configPath); err == nil {
		configContent = string(data)
	} else if os.IsNotExist(err) {
		configContent = `[terminal]
vt = 1

[default_session]
`
	} else {
		return fmt.Errorf("failed to read greetd config: %w", err)
	}

	wrapperCmd := resolveGreeterWrapperPath()

	compositorLower := strings.ToLower(compositor)
	commandValue := fmt.Sprintf("%s --command %s --cache-dir %s", wrapperCmd, compositorLower, GreeterCacheDir)
	if hgsPath != "" {
		commandValue = fmt.Sprintf("%s -p %s", commandValue, hgsPath)
	}

	commandLine := fmt.Sprintf(`command = "%s"`, commandValue)
	newConfig := upsertDefaultSession(configContent, greeterUser, commandLine)

	if err := writeGreetdConfig(configPath, newConfig, logFunc, sudoPassword, fmt.Sprintf("✓ Updated greetd configuration (user: %s, command: %s)", greeterUser, commandValue)); err != nil {
		return err
	}

	return nil
}

// getDebianOBSSlug returns the OBS repository slug for the running Debian version.
func getDebianOBSSlug(osInfo *distros.OSInfo) string {
	versionID := strings.ToLower(osInfo.VersionID)
	codename := strings.ToLower(osInfo.VersionCodename)
	prettyName := strings.ToLower(osInfo.PrettyName)

	if strings.Contains(prettyName, "sid") || strings.Contains(prettyName, "unstable") ||
		codename == "sid" || versionID == "sid" {
		return "Debian_Unstable"
	}
	if versionID == "testing" || codename == "testing" {
		return "Debian_Testing"
	}
	if versionID != "" {
		return "Debian_" + versionID // "Debian_13"
	}
	return "Debian_Unstable"
}

// getOpenSUSEOBSRepoURL returns the OBS .repo file URL for the running openSUSE variant.
func getOpenSUSEOBSRepoURL(osInfo *distros.OSInfo) string {
	const base = "https://download.opensuse.org/repositories/home:AvengeMedia:coastlinesec"
	var slug string
	switch osInfo.Distribution.ID {
	case "opensuse-leap":
		v := osInfo.VersionID
		if v != "" && !strings.Contains(v, ".") {
			v += ".0" // "16" → "16.0"
		}
		if v == "" {
			v = "16.0"
		}
		slug = v
	case "opensuse-slowroll":
		slug = "openSUSE_Slowroll"
	default: // opensuse-tumbleweed || unknown version
		slug = "openSUSE_Tumbleweed"
	}
	return fmt.Sprintf("%s/%s/home:AvengeMedia:coastlinesec.repo", base, slug)
}

func checkSystemdEnabled(service string) (string, error) {
	cmd := exec.Command("systemctl", "is-enabled", service)
	output, _ := cmd.Output()
	return strings.TrimSpace(string(output)), nil
}

func DisableConflictingDisplayManagers(sudoPassword string, logFunc func(string)) error {
	conflictingDMs := []string{"gdm", "gdm3", "lightdm", "sddm", "lxdm", "xdm", "cosmic-greeter"}
	for _, dm := range conflictingDMs {
		state, err := checkSystemdEnabled(dm)
		if err != nil || state == "" || state == "not-found" {
			continue
		}
		switch state {
		case "enabled", "enabled-runtime", "static", "indirect", "alias":
			logFunc(fmt.Sprintf("Disabling conflicting display manager: %s", dm))
			if err := privesc.Run(context.Background(), sudoPassword, "systemctl", "disable", dm); err != nil {
				logFunc(fmt.Sprintf("⚠ Warning: Failed to disable %s: %v", dm, err))
			} else {
				logFunc(fmt.Sprintf("✓ Disabled %s", dm))
			}
		}
	}
	return nil
}

// EnableGreetd unmasks and enables greetd, forcing it over any other DM.
func EnableGreetd(sudoPassword string, logFunc func(string)) error {
	state, err := checkSystemdEnabled("greetd")
	if err != nil {
		return fmt.Errorf("failed to check greetd state: %w", err)
	}
	if state == "not-found" {
		return fmt.Errorf("greetd service not found; ensure greetd is installed")
	}
	if state == "masked" || state == "masked-runtime" {
		logFunc("  Unmasking greetd...")
		if err := privesc.Run(context.Background(), sudoPassword, "systemctl", "unmask", "greetd"); err != nil {
			return fmt.Errorf("failed to unmask greetd: %w", err)
		}
		logFunc("  ✓ Unmasked greetd")
	}
	logFunc("  Enabling greetd service (--force)...")
	if err := privesc.Run(context.Background(), sudoPassword, "systemctl", "enable", "--force", "greetd"); err != nil {
		return fmt.Errorf("failed to enable greetd: %w", err)
	}
	logFunc("✓ greetd enabled")
	return nil
}

func EnsureGraphicalTarget(sudoPassword string, logFunc func(string)) error {
	cmd := exec.Command("systemctl", "get-default")
	output, err := cmd.Output()
	if err != nil {
		logFunc(fmt.Sprintf("⚠ Warning: could not get default systemd target: %v", err))
		return nil
	}
	current := strings.TrimSpace(string(output))
	if current == "graphical.target" {
		logFunc("✓ Default target is already graphical.target")
		return nil
	}
	logFunc(fmt.Sprintf("  Setting default target to graphical.target (was: %s)...", current))
	if err := privesc.Run(context.Background(), sudoPassword, "systemctl", "set-default", "graphical.target"); err != nil {
		return fmt.Errorf("failed to set graphical target: %w", err)
	}
	logFunc("✓ Default target set to graphical.target")
	return nil
}

// AutoSetupGreeter performs the full non-interactive greeter setup
func AutoSetupGreeter(compositor, sudoPassword string, logFunc func(string)) error {
	if strings.ToLower(compositor) != "hyprland" {
		return fmt.Errorf("unsupported greeter compositor %q: HyprGlassShell currently supports Hyprland only", compositor)
	}

	if IsGreeterPackaged() && HasLegacyLocalGreeterWrapper() {
		return fmt.Errorf("legacy manual wrapper detected at /usr/local/bin/hgs-greeter; " +
			"remove it before using packaged hgs-greeter: sudo rm -f /usr/local/bin/hgs-greeter")
	}

	logFunc("Ensuring greetd is installed...")
	if err := EnsureGreetdInstalled(logFunc, sudoPassword); err != nil {
		return fmt.Errorf("greetd install failed: %w", err)
	}

	hgsPath := ""
	if !IsGreeterPackaged() {
		detected, err := DetectHGSPath()
		if err != nil {
			return fmt.Errorf("HGS installation not found: %w", err)
		}
		hgsPath = detected
		logFunc(fmt.Sprintf("✓ Found HGS at: %s", hgsPath))
	} else {
		logFunc("✓ Using packaged hgs-greeter (/usr/share/quickshell/hgs-greeter)")
	}

	logFunc("Setting up hgs-greeter group and permissions...")
	if err := SetupHGSGroup(logFunc, sudoPassword); err != nil {
		logFunc(fmt.Sprintf("⚠ Warning: group/permissions setup error: %v", err))
	}

	logFunc("Copying greeter files...")
	if err := CopyGreeterFiles(hgsPath, compositor, logFunc, sudoPassword); err != nil {
		return fmt.Errorf("failed to copy greeter files: %w", err)
	}

	logFunc("Configuring greetd...")
	greeterPathForConfig := ""
	if !IsGreeterPackaged() {
		greeterPathForConfig = hgsPath
	}
	if err := ConfigureGreetd(greeterPathForConfig, compositor, logFunc, sudoPassword); err != nil {
		return fmt.Errorf("failed to configure greetd: %w", err)
	}

	logFunc("Synchronizing HGS configurations...")
	if err := SyncHGSConfigs(hgsPath, compositor, logFunc, sudoPassword); err != nil {
		logFunc(fmt.Sprintf("⚠ Warning: config sync error: %v", err))
	}

	logFunc("Configuring authentication...")
	if err := sharedpam.SyncAuthConfig(logFunc, sudoPassword, sharedpam.SyncAuthOptions{}); err != nil {
		return fmt.Errorf("failed to sync authentication: %w", err)
	}

	logFunc("Checking for conflicting display managers...")
	if err := DisableConflictingDisplayManagers(sudoPassword, logFunc); err != nil {
		logFunc(fmt.Sprintf("⚠ Warning: %v", err))
	}

	logFunc("Enabling greetd service...")
	if err := EnableGreetd(sudoPassword, logFunc); err != nil {
		return fmt.Errorf("failed to enable greetd: %w", err)
	}

	logFunc("Ensuring graphical.target as default...")
	if err := EnsureGraphicalTarget(sudoPassword, logFunc); err != nil {
		logFunc(fmt.Sprintf("⚠ Warning: %v", err))
	}

	logFunc("✓ HGS greeter setup complete")
	return nil
}
