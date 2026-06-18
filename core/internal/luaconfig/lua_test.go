package luaconfig

import (
	"os"
	"path/filepath"
	"testing"
)

func TestModuleToRelPath(t *testing.T) {
	tests := map[string]string{
		"hgs.keybinds":      filepath.Join("hgs", "keybinds.lua"),
		"hgs/user-keybinds": filepath.Join("hgs", "user-keybinds.lua"),
		"awesome/anim":      filepath.Join("awesome", "anim.lua"),
		"awesome.colors":    filepath.Join("awesome", "colors.lua"),
		" awesome.binds ":   filepath.Join("awesome", "binds.lua"),
	}

	for input, want := range tests {
		if got := ModuleToRelPath(input); got != want {
			t.Fatalf("ModuleToRelPath(%q) = %q, want %q", input, got, want)
		}
	}
}

func TestRequiresSkipsComments(t *testing.T) {
	if modules := Requires(`-- require("hgs.keybinds")`); len(modules) != 0 {
		t.Fatalf("expected commented require to be ignored, got %#v", modules)
	}

	modules := Requires(`print("-- not a comment") require("hgs.keybinds") -- require("ignored")`)
	if len(modules) != 1 || modules[0] != "hgs.keybinds" {
		t.Fatalf("unexpected modules: %#v", modules)
	}
}

func TestRequiresTargetRecurses(t *testing.T) {
	tmpDir := t.TempDir()
	hgsDir := filepath.Join(tmpDir, "hgs")
	if err := os.MkdirAll(hgsDir, 0o755); err != nil {
		t.Fatal(err)
	}
	target := filepath.Join(hgsDir, "rules.lua")
	if err := os.WriteFile(filepath.Join(tmpDir, "hyprland.lua"), []byte(`require("hgs.extra")`), 0o644); err != nil {
		t.Fatal(err)
	}
	if err := os.WriteFile(filepath.Join(hgsDir, "extra.lua"), []byte(`require("hgs.rules")`), 0o644); err != nil {
		t.Fatal(err)
	}
	if err := os.WriteFile(target, []byte(`-- rules`), 0o644); err != nil {
		t.Fatal(err)
	}

	if !RequiresTarget(filepath.Join(tmpDir, "hyprland.lua"), target, make(map[string]bool)) {
		t.Fatal("expected recursive require lookup to find target")
	}
}
