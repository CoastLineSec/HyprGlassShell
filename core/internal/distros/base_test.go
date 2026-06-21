package distros

import (
	"os"
	"os/exec"
	"path/filepath"
	"strings"
	"testing"

	"github.com/CoastLineSec/HyprGlassShell/core/internal/deps"
	"github.com/CoastLineSec/HyprGlassShell/core/internal/utils"
)

func TestBaseDistribution_detectHGS_NotInstalled(t *testing.T) {
	originalHome := os.Getenv("HOME")
	defer os.Setenv("HOME", originalHome)

	tempDir := t.TempDir()
	os.Setenv("HOME", tempDir)

	logChan := make(chan string, 10)
	defer close(logChan)

	base := NewBaseDistribution(logChan)
	dep := base.detectHGS()

	if dep.Status != deps.StatusMissing {
		t.Errorf("Expected StatusMissing, got %d", dep.Status)
	}

	if dep.Name != "hgs (HyprGlassShell)" {
		t.Errorf("Expected name 'hgs (HyprGlassShell)', got %s", dep.Name)
	}

	if !dep.Required {
		t.Error("Expected Required to be true")
	}
}

func TestBaseDistribution_WriteHyprlandSessionTargetWantsHGS(t *testing.T) {
	tempDir := t.TempDir()
	t.Setenv("HOME", tempDir)

	logChan := make(chan string, 10)
	defer close(logChan)

	base := NewBaseDistribution(logChan)
	if err := base.WriteHyprlandSessionTarget(); err != nil {
		t.Fatalf("WriteHyprlandSessionTarget() error = %v", err)
	}

	targetPath := filepath.Join(tempDir, ".config", "systemd", "user", "hyprland-session.target")
	data, err := os.ReadFile(targetPath)
	if err != nil {
		t.Fatalf("failed to read target: %v", err)
	}

	content := string(data)
	if !strings.Contains(content, "Wants=hgs.service") {
		t.Fatalf("target should pull in hgs.service, got:\n%s", content)
	}
}

func TestBaseDistribution_detectHGS_Installed(t *testing.T) {
	if !utils.CommandExists("git") {
		t.Skip("git not available")
	}

	tempDir := t.TempDir()
	hgsPath := filepath.Join(tempDir, ".config", "quickshell", "hgs")
	os.MkdirAll(hgsPath, 0o755)

	originalHome := os.Getenv("HOME")
	defer os.Setenv("HOME", originalHome)
	os.Setenv("HOME", tempDir)

	exec.Command("git", "init", hgsPath).Run()
	exec.Command("git", "-C", hgsPath, "config", "user.email", "test@test.com").Run()
	exec.Command("git", "-C", hgsPath, "config", "user.name", "Test User").Run()
	exec.Command("git", "-C", hgsPath, "checkout", "-b", "master").Run()

	testFile := filepath.Join(hgsPath, "test.txt")
	os.WriteFile(testFile, []byte("test"), 0o644)
	exec.Command("git", "-C", hgsPath, "add", ".").Run()
	exec.Command("git", "-C", hgsPath, "commit", "-m", "initial").Run()

	logChan := make(chan string, 10)
	defer close(logChan)

	base := NewBaseDistribution(logChan)
	dep := base.detectHGS()

	if dep.Status == deps.StatusMissing {
		t.Error("Expected HGS to be detected as installed")
	}

	if dep.Name != "hgs (HyprGlassShell)" {
		t.Errorf("Expected name 'hgs (HyprGlassShell)', got %s", dep.Name)
	}

	if !dep.Required {
		t.Error("Expected Required to be true")
	}

	t.Logf("Status: %d, Version: %s", dep.Status, dep.Version)
}

func TestBaseDistribution_detectHGS_NeedsUpdate(t *testing.T) {
	if !utils.CommandExists("git") {
		t.Skip("git not available")
	}

	tempDir := t.TempDir()
	hgsPath := filepath.Join(tempDir, ".config", "quickshell", "hgs")
	os.MkdirAll(hgsPath, 0o755)

	originalHome := os.Getenv("HOME")
	defer os.Setenv("HOME", originalHome)
	os.Setenv("HOME", tempDir)

	exec.Command("git", "init", hgsPath).Run()
	exec.Command("git", "-C", hgsPath, "config", "user.email", "test@test.com").Run()
	exec.Command("git", "-C", hgsPath, "config", "user.name", "Test User").Run()
	exec.Command("git", "-C", hgsPath, "remote", "add", "origin", "https://github.com/CoastLineSec/HyprGlassShell.git").Run()

	testFile := filepath.Join(hgsPath, "test.txt")
	os.WriteFile(testFile, []byte("test"), 0o644)
	exec.Command("git", "-C", hgsPath, "add", ".").Run()
	exec.Command("git", "-C", hgsPath, "commit", "-m", "initial").Run()
	exec.Command("git", "-C", hgsPath, "tag", "v0.0.1").Run()
	exec.Command("git", "-C", hgsPath, "checkout", "v0.0.1").Run()

	logChan := make(chan string, 10)
	defer close(logChan)

	base := NewBaseDistribution(logChan)
	dep := base.detectHGS()

	if dep.Name != "hgs (HyprGlassShell)" {
		t.Errorf("Expected name 'hgs (HyprGlassShell)', got %s", dep.Name)
	}

	if !dep.Required {
		t.Error("Expected Required to be true")
	}

	t.Logf("Status: %d, Version: %s", dep.Status, dep.Version)
}

func TestBaseDistribution_detectHGS_DirectoryWithoutGit(t *testing.T) {
	tempDir := t.TempDir()
	hgsPath := filepath.Join(tempDir, ".config", "quickshell", "hgs")
	os.MkdirAll(hgsPath, 0o755)

	originalHome := os.Getenv("HOME")
	defer os.Setenv("HOME", originalHome)
	os.Setenv("HOME", tempDir)

	logChan := make(chan string, 10)
	defer close(logChan)

	base := NewBaseDistribution(logChan)
	dep := base.detectHGS()

	if dep.Status == deps.StatusMissing {
		t.Error("Expected HGS to be detected as present")
	}

	if dep.Name != "hgs (HyprGlassShell)" {
		t.Errorf("Expected name 'hgs (HyprGlassShell)', got %s", dep.Name)
	}

	if !dep.Required {
		t.Error("Expected Required to be true")
	}
}

func TestBaseDistribution_NewBaseDistribution(t *testing.T) {
	logChan := make(chan string, 10)
	defer close(logChan)

	base := NewBaseDistribution(logChan)

	if base == nil {
		t.Fatal("NewBaseDistribution returned nil")
	}

	if base.logChan == nil {
		t.Error("logChan was not set")
	}
}

func TestBaseDistribution_versionCompare(t *testing.T) {
	logChan := make(chan string, 10)
	defer close(logChan)

	base := NewBaseDistribution(logChan)

	tests := []struct {
		v1       string
		v2       string
		expected int
	}{
		{"0.1.0", "0.1.0", 0},
		{"0.1.0", "0.1.1", -1},
		{"0.1.1", "0.1.0", 1},
		{"0.2.0", "0.1.9", 1},
		{"1.0.0", "0.9.9", 1},
	}

	for _, tt := range tests {
		result := base.versionCompare(tt.v1, tt.v2)
		if result != tt.expected {
			t.Errorf("versionCompare(%q, %q) = %d; want %d", tt.v1, tt.v2, result, tt.expected)
		}
	}
}

func TestBaseDistribution_versionCompare_WithPrefix(t *testing.T) {
	logChan := make(chan string, 10)
	defer close(logChan)

	base := NewBaseDistribution(logChan)

	tests := []struct {
		v1       string
		v2       string
		expected int
	}{
		{"v0.1.0", "v0.1.0", 0},
		{"v0.1.0", "v0.1.1", -1},
		{"v0.1.1", "v0.1.0", 1},
	}

	for _, tt := range tests {
		result := base.versionCompare(tt.v1, tt.v2)
		if result != tt.expected {
			t.Errorf("versionCompare(%q, %q) = %d; want %d", tt.v1, tt.v2, result, tt.expected)
		}
	}
}
