//go:build !distro_binary

package main

import (
	"bufio"
	"context"
	"errors"
	"fmt"
	"os"
	"os/exec"
	"path/filepath"
	"strings"
	"time"

	"github.com/CoastLineSec/HyprGlassShell/core/internal/distros"
	"github.com/CoastLineSec/HyprGlassShell/core/internal/errdefs"
	"github.com/CoastLineSec/HyprGlassShell/core/internal/log"
	"github.com/CoastLineSec/HyprGlassShell/core/internal/privesc"
	"github.com/CoastLineSec/HyprGlassShell/core/internal/utils"
	"github.com/CoastLineSec/HyprGlassShell/core/internal/version"
	"github.com/spf13/cobra"
)

var updateCmd = &cobra.Command{
	Use:     "update",
	Short:   "Update HyprGlassShell to the latest version",
	Long:    "Update HyprGlassShell to the latest version using the appropriate package manager for your distribution",
	PreRunE: findConfig,
	Run: func(cmd *cobra.Command, args []string) {
		runUpdate()
	},
}

var updateCheckCmd = &cobra.Command{
	Use:   "check",
	Short: "Check if updates are available for HyprGlassShell",
	Long:  "Check for available updates without performing the actual update",
	Run: func(cmd *cobra.Command, args []string) {
		runUpdateCheck()
	},
}

func runUpdateCheck() {
	fmt.Println("Checking for HyprGlassShell updates...")
	fmt.Println()

	versionInfo, err := version.GetHGSVersionInfo()
	if err != nil {
		log.Fatalf("Error checking for updates: %v", err)
	}

	fmt.Printf("Current version: %s\n", versionInfo.Current)
	fmt.Printf("Latest version:  %s\n", versionInfo.Latest)
	fmt.Println()

	if versionInfo.HasUpdate {
		fmt.Println("✓ Update available!")
		fmt.Println()
		fmt.Println("Run 'hgs update' to install the latest version.")
		os.Exit(0)
	} else {
		fmt.Println("✓ You are running the latest version.")
		os.Exit(0)
	}
}

func runUpdate() {
	osInfo, err := distros.GetOSInfo()
	if err != nil {
		log.Fatalf("Error detecting OS: %v", err)
	}

	config, exists := distros.Registry[osInfo.Distribution.ID]
	if !exists {
		log.Fatalf("Unsupported distribution: %s", osInfo.Distribution.ID)
	}

	var updateErr error
	switch config.Family {
	case distros.FamilyArch:
		updateErr = updateArchLinux()
	case distros.FamilySUSE:
		updateErr = updateOtherDistros()
	default:
		updateErr = updateOtherDistros()
	}

	if updateErr != nil {
		if errors.Is(updateErr, errdefs.ErrUpdateCancelled) {
			log.Info("Update cancelled.")
			return
		}
		if errors.Is(updateErr, errdefs.ErrNoUpdateNeeded) {
			return
		}
		log.Fatalf("Error updating HGS: %v", updateErr)
	}

	log.Info("Update complete! Restarting HGS...")
	restartShell()
}

func updateArchLinux() error {
	homeDir, err := os.UserHomeDir()
	if err == nil {
		hgsPath := filepath.Join(homeDir, ".config", "quickshell", "hgs")
		if _, err := os.Stat(hgsPath); err == nil {
			return updateOtherDistros()
		}
	}

	var packageName string
	var isAUR bool
	if isArchPackageInstalled("hgs-shell") {
		packageName = "hgs-shell"
	} else if isArchPackageInstalled("hgs-shell-git") {
		packageName = "hgs-shell-git"
		isAUR = true
	} else if isArchPackageInstalled("hgs-shell-bin") {
		packageName = "hgs-shell-bin"
		isAUR = true
	} else {
		fmt.Println("Info: No hgs-shell package found.")
		fmt.Println("Info: Falling back to git-based update method...")
		return updateOtherDistros()
	}

	if !isAUR {
		fmt.Printf("This will update %s using pacman.\n", packageName)
		if !confirmUpdate() {
			return errdefs.ErrUpdateCancelled
		}

		fmt.Printf("\nRunning: pacman -S %s\n", packageName)
		if err := privesc.Run(context.Background(), "", "pacman", "-S", "--noconfirm", packageName); err != nil {
			fmt.Printf("Error: Failed to update using pacman: %v\n", err)
			return err
		}

		fmt.Println("hgs successfully updated")
		return nil
	}

	var helper string
	var updateCmd *exec.Cmd

	if utils.CommandExists("yay") {
		helper = "yay"
		updateCmd = exec.Command("yay", "-S", packageName)
	} else if utils.CommandExists("paru") {
		helper = "paru"
		updateCmd = exec.Command("paru", "-S", packageName)
	} else {
		fmt.Println("Error: Neither yay nor paru found - please install an AUR helper")
		fmt.Println("Info: Falling back to git-based update method...")
		return updateOtherDistros()
	}

	fmt.Printf("This will update HyprGlassShell using %s.\n", helper)
	if !confirmUpdate() {
		return errdefs.ErrUpdateCancelled
	}

	fmt.Printf("\nRunning: %s -S %s\n", helper, packageName)
	updateCmd.Stdout = os.Stdout
	updateCmd.Stderr = os.Stderr
	err = updateCmd.Run()
	if err != nil {
		fmt.Printf("Error: Failed to update using %s: %v\n", helper, err)
	}

	fmt.Println("hgs successfully updated")
	return nil
}

func updateOtherDistros() error {
	homeDir, err := os.UserHomeDir()
	if err != nil {
		return fmt.Errorf("failed to get user home directory: %w", err)
	}

	hgsPath := filepath.Join(homeDir, ".config", "quickshell", "hgs")

	if _, err := os.Stat(hgsPath); os.IsNotExist(err) {
		return fmt.Errorf("HGS configuration directory not found at %s", hgsPath)
	}

	fmt.Printf("Found HGS configuration at %s\n", hgsPath)

	versionInfo, err := version.GetHGSVersionInfo()
	if err == nil && !versionInfo.HasUpdate {
		fmt.Println()
		fmt.Printf("Current version: %s\n", versionInfo.Current)
		fmt.Printf("Latest version:  %s\n", versionInfo.Latest)
		fmt.Println()
		fmt.Println("✓ You are already running the latest version.")
		return errdefs.ErrNoUpdateNeeded
	}

	fmt.Println("\nThis will update:")
	fmt.Println("  1. The hgs binary from GitHub releases")
	fmt.Println("  2. HyprGlassShell configuration using git")
	if !confirmUpdate() {
		return errdefs.ErrUpdateCancelled
	}

	fmt.Println("\n=== Updating hgs binary ===")
	if err := updateHGSBinary(); err != nil {
		fmt.Printf("Warning: Failed to update hgs binary: %v\n", err)
		fmt.Println("Continuing with shell configuration update...")
	} else {
		fmt.Println("hgs binary successfully updated")
	}

	fmt.Println("\n=== Updating HGS shell configuration ===")

	if err := os.Chdir(hgsPath); err != nil {
		return fmt.Errorf("failed to change to HGS directory: %w", err)
	}

	statusCmd := exec.Command("git", "status", "--porcelain")
	statusOutput, _ := statusCmd.Output()
	hasLocalChanges := len(strings.TrimSpace(string(statusOutput))) > 0

	currentRefCmd := exec.Command("git", "symbolic-ref", "-q", "HEAD")
	currentRefOutput, _ := currentRefCmd.Output()
	onBranch := len(currentRefOutput) > 0

	var currentTag string
	var currentBranch string

	if !onBranch {
		tagCmd := exec.Command("git", "describe", "--exact-match", "--tags", "HEAD")
		if tagOutput, err := tagCmd.Output(); err == nil {
			currentTag = strings.TrimSpace(string(tagOutput))
		}
	} else {
		branchCmd := exec.Command("git", "rev-parse", "--abbrev-ref", "HEAD")
		if branchOutput, err := branchCmd.Output(); err == nil {
			currentBranch = strings.TrimSpace(string(branchOutput))
		}
	}

	fmt.Println("Fetching latest changes...")
	fetchCmd := exec.Command("git", "fetch", "origin", "--tags", "--force")
	fetchCmd.Stdout = os.Stdout
	fetchCmd.Stderr = os.Stderr
	if err := fetchCmd.Run(); err != nil {
		return fmt.Errorf("failed to fetch changes: %w", err)
	}

	if currentTag != "" {
		latestTagCmd := exec.Command("git", "tag", "-l", "v*", "--sort=-version:refname")
		latestTagOutput, err := latestTagCmd.Output()
		if err != nil {
			return fmt.Errorf("failed to get latest tag: %w", err)
		}

		tags := strings.Split(strings.TrimSpace(string(latestTagOutput)), "\n")
		if len(tags) == 0 || tags[0] == "" {
			return fmt.Errorf("no version tags found")
		}
		latestTag := tags[0]

		if latestTag == currentTag {
			fmt.Printf("Already on latest tag: %s\n", currentTag)
			return nil
		}

		fmt.Printf("Current tag: %s\n", currentTag)
		fmt.Printf("Latest tag: %s\n", latestTag)

		if hasLocalChanges {
			fmt.Println("\nWarning: You have local changes in your HGS configuration.")
			if offerReclone(hgsPath) {
				return nil
			}
			return errdefs.ErrUpdateCancelled
		}

		fmt.Printf("Updating to %s...\n", latestTag)
		checkoutCmd := exec.Command("git", "checkout", latestTag)
		checkoutCmd.Stdout = os.Stdout
		checkoutCmd.Stderr = os.Stderr
		if err := checkoutCmd.Run(); err != nil {
			fmt.Printf("Error: Failed to checkout %s: %v\n", latestTag, err)
			if offerReclone(hgsPath) {
				return nil
			}
			return fmt.Errorf("update cancelled")
		}

		fmt.Printf("\nUpdate complete! Updated from %s to %s\n", currentTag, latestTag)
		return nil
	}

	if currentBranch == "" {
		currentBranch = "master"
	}

	fmt.Printf("Current branch: %s\n", currentBranch)

	if hasLocalChanges {
		fmt.Println("\nWarning: You have local changes in your HGS configuration.")
		if offerReclone(hgsPath) {
			return nil
		}
		return errdefs.ErrUpdateCancelled
	}

	pullCmd := exec.Command("git", "pull", "origin", currentBranch)
	pullCmd.Stdout = os.Stdout
	pullCmd.Stderr = os.Stderr
	if err := pullCmd.Run(); err != nil {
		fmt.Printf("Error: Failed to pull latest changes: %v\n", err)
		if offerReclone(hgsPath) {
			return nil
		}
		return fmt.Errorf("update cancelled")
	}

	fmt.Println("\nUpdate complete!")
	return nil
}

func offerReclone(hgsPath string) bool {
	fmt.Println("\nWould you like to backup and re-clone the repository? (y/N): ")
	reader := bufio.NewReader(os.Stdin)
	response, err := reader.ReadString('\n')
	if err != nil || !strings.HasPrefix(strings.ToLower(strings.TrimSpace(response)), "y") {
		return false
	}

	timestamp := time.Now().Unix()
	backupPath := fmt.Sprintf("%s.backup-%d", hgsPath, timestamp)

	fmt.Printf("Backing up current directory to %s...\n", backupPath)
	if err := os.Rename(hgsPath, backupPath); err != nil {
		fmt.Printf("Error: Failed to backup directory: %v\n", err)
		return false
	}

	fmt.Println("Cloning fresh copy...")
	cloneCmd := exec.Command("git", "clone", "https://github.com/CoastLineSec/HyprGlassShell.git", hgsPath)
	cloneCmd.Stdout = os.Stdout
	cloneCmd.Stderr = os.Stderr
	if err := cloneCmd.Run(); err != nil {
		fmt.Printf("Error: Failed to clone repository: %v\n", err)
		fmt.Printf("Restoring backup...\n")
		os.Rename(backupPath, hgsPath)
		return false
	}

	fmt.Printf("Successfully re-cloned repository (backup at %s)\n", backupPath)
	return true
}

func confirmUpdate() bool {
	fmt.Print("Do you want to proceed with the update? (y/N): ")
	reader := bufio.NewReader(os.Stdin)
	response, err := reader.ReadString('\n')
	if err != nil {
		fmt.Printf("Error reading input: %v\n", err)
		return false
	}
	response = strings.TrimSpace(strings.ToLower(response))
	return response == "y" || response == "yes"
}

func updateHGSBinary() error {
	arch := ""
	switch strings.ToLower(os.Getenv("HOSTTYPE")) {
	case "x86_64", "amd64":
		arch = "amd64"
	case "aarch64", "arm64":
		arch = "arm64"
	default:
		cmd := exec.Command("uname", "-m")
		output, err := cmd.Output()
		if err != nil {
			return fmt.Errorf("failed to detect architecture: %w", err)
		}
		archStr := strings.TrimSpace(string(output))
		switch archStr {
		case "x86_64":
			arch = "amd64"
		case "aarch64":
			arch = "arm64"
		default:
			return fmt.Errorf("unsupported architecture: %s", archStr)
		}
	}

	fmt.Println("Fetching latest release version...")
	cmd := exec.Command("curl", "-s", "https://api.github.com/repos/CoastLineSec/HyprGlassShell/releases/latest")
	output, err := cmd.Output()
	if err != nil {
		return fmt.Errorf("failed to fetch latest release: %w", err)
	}

	version := ""
	for line := range strings.SplitSeq(string(output), "\n") {
		if strings.Contains(line, "\"tag_name\"") {
			parts := strings.Split(line, "\"")
			if len(parts) >= 4 {
				version = parts[3]
				break
			}
		}
	}

	if version == "" {
		return fmt.Errorf("could not determine latest version")
	}

	fmt.Printf("Latest version: %s\n", version)

	tempDir, err := os.MkdirTemp("", "hgs-update-*")
	if err != nil {
		return fmt.Errorf("failed to create temp directory: %w", err)
	}
	defer os.RemoveAll(tempDir)

	binaryURL := fmt.Sprintf("https://github.com/CoastLineSec/HyprGlassShell/releases/download/%s/hgs-cli-%s.gz", version, arch)
	checksumURL := fmt.Sprintf("https://github.com/CoastLineSec/HyprGlassShell/releases/download/%s/hgs-cli-%s.gz.sha256", version, arch)

	binaryPath := filepath.Join(tempDir, "hgs.gz")
	checksumPath := filepath.Join(tempDir, "hgs.gz.sha256")

	fmt.Println("Downloading hgs binary...")
	downloadCmd := exec.Command("curl", "-L", binaryURL, "-o", binaryPath)
	if err := downloadCmd.Run(); err != nil {
		return fmt.Errorf("failed to download binary: %w", err)
	}

	fmt.Println("Downloading checksum...")
	downloadCmd = exec.Command("curl", "-L", checksumURL, "-o", checksumPath)
	if err := downloadCmd.Run(); err != nil {
		return fmt.Errorf("failed to download checksum: %w", err)
	}

	fmt.Println("Verifying checksum...")
	checksumData, err := os.ReadFile(checksumPath)
	if err != nil {
		return fmt.Errorf("failed to read checksum file: %w", err)
	}
	expectedChecksum := strings.Fields(string(checksumData))[0]

	actualCmd := exec.Command("sha256sum", binaryPath)
	actualOutput, err := actualCmd.Output()
	if err != nil {
		return fmt.Errorf("failed to calculate checksum: %w", err)
	}
	actualChecksum := strings.Fields(string(actualOutput))[0]

	if expectedChecksum != actualChecksum {
		return fmt.Errorf("checksum verification failed\nExpected: %s\nGot: %s", expectedChecksum, actualChecksum)
	}

	fmt.Println("Decompressing binary...")
	decompressCmd := exec.Command("gunzip", binaryPath)
	if err := decompressCmd.Run(); err != nil {
		return fmt.Errorf("failed to decompress binary: %w", err)
	}

	decompressedPath := filepath.Join(tempDir, "hgs")

	if err := os.Chmod(decompressedPath, 0o755); err != nil {
		return fmt.Errorf("failed to make binary executable: %w", err)
	}

	currentPath, err := exec.LookPath("hgs")
	if err != nil {
		return fmt.Errorf("could not find current hgs binary: %w", err)
	}

	fmt.Printf("Installing to %s...\n", currentPath)

	if err := privesc.Run(context.Background(), "", "install", "-m", "0755", decompressedPath, currentPath); err != nil {
		return fmt.Errorf("failed to replace binary: %w", err)
	}

	return nil
}
