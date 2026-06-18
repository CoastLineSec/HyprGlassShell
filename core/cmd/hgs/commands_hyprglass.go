package main

import (
	"encoding/json"
	"fmt"
	"os"
	"os/exec"
	"strings"

	"github.com/CoastLineSec/HyprGlassShell/core/internal/hyprglass"
	"github.com/spf13/cobra"
)

var hyprglassCmd = &cobra.Command{
	Use:   "hyprglass",
	Short: "HyprGlass compositor effect utilities",
	Long:  "Inspect and validate the descriptor contract used between HyprGlassShell and the hgs-hyprglass Hyprland plugin.",
}

var hyprglassValidateCmd = &cobra.Command{
	Use:   "validate <descriptor.json>",
	Short: "Validate and normalize a HyprGlass descriptor payload",
	Args:  cobra.ExactArgs(1),
	Run:   runHyprglassValidate,
}

var hyprglassStatusCmd = &cobra.Command{
	Use:   "status",
	Short: "Query hgs-hyprglass plugin status",
	Args:  cobra.NoArgs,
	Run:   runHyprglassStatus,
}

var hyprglassApplyCmd = &cobra.Command{
	Use:   "apply <descriptor.json>",
	Short: "Send a validated descriptor payload to hgs-hyprglass",
	Long:  "Validate a descriptor payload and send it to the hgs-hyprglass Hyprland plugin through hyprctl. This requires a plugin build that exposes the hyprglass apply-json command.",
	Args:  cobra.ExactArgs(1),
	Run:   runHyprglassApply,
}

var hyprglassApplyJSONCmd = &cobra.Command{
	Use:   "apply-json <descriptor-json>",
	Short: "Send a validated descriptor JSON string to hgs-hyprglass",
	Long:  "Validate a descriptor payload supplied as a command argument and send it to the hgs-hyprglass Hyprland plugin through hyprctl.",
	Args:  cobra.ExactArgs(1),
	Run:   runHyprglassApplyJSON,
}

var hyprglassClearCmd = &cobra.Command{
	Use:   "clear",
	Short: "Clear active hgs-hyprglass descriptors",
	Args:  cobra.NoArgs,
	Run:   runHyprglassClear,
}

var hyprglassDebugOverlayCmd = &cobra.Command{
	Use:   "debug-overlay <on|off|toggle|status>",
	Short: "Control hgs-hyprglass diagnostic bounds overlay",
	Args:  cobra.ExactArgs(1),
	Run:   runHyprglassDebugOverlay,
}

var hyprglassMaterialCmd = &cobra.Command{
	Use:   "material <off|flat|blur-native|glass-v1|status>",
	Short: "Control hgs-hyprglass compositor material mode",
	Args:  cobra.ExactArgs(1),
	Run:   runHyprglassMaterial,
}

func init() {
	hyprglassCmd.AddCommand(hyprglassValidateCmd, hyprglassStatusCmd, hyprglassApplyCmd, hyprglassApplyJSONCmd, hyprglassClearCmd, hyprglassDebugOverlayCmd, hyprglassMaterialCmd)
}

func runHyprglassValidate(cmd *cobra.Command, args []string) {
	set := readHyprglassDescriptorSet(args[0])
	output, err := set.JSON()
	if err != nil {
		fmt.Fprintf(os.Stderr, "invalid descriptor payload: %v\n", err)
		os.Exit(1)
	}
	fmt.Fprintln(os.Stdout, string(output))
}

func runHyprglassStatus(cmd *cobra.Command, args []string) {
	output, err := exec.Command("hyprctl", "-j", "hyprglass-status").CombinedOutput()
	if err == nil && hyprglassStatusRegistered(output) {
		fmt.Fprintln(os.Stdout, string(output))
		return
	}

	reason := "hyprctl command not registered"
	if err != nil {
		reason = "hyprctl command failed"
	}
	printHyprglassUnavailableStatus(reason, err, output)
}

func runHyprglassApply(cmd *cobra.Command, args []string) {
	set := readHyprglassDescriptorSet(args[0])
	applyHyprglassDescriptorSet(set)
}

func runHyprglassApplyJSON(cmd *cobra.Command, args []string) {
	set, err := hyprglass.DecodeDescriptorSet([]byte(args[0]))
	if err != nil {
		fmt.Fprintf(os.Stderr, "failed to decode descriptor payload: %v\n", err)
		os.Exit(1)
	}
	applyHyprglassDescriptorSet(set)
}

func runHyprglassClear(cmd *cobra.Command, args []string) {
	output, err := exec.Command("hyprctl", "hyprglass-clear").CombinedOutput()
	if err != nil || hyprctlUnknownRequest(output) {
		fmt.Fprintf(os.Stderr, "hgs-hyprglass clear failed: %v\n%s\n", err, output)
		os.Exit(1)
	}
	fmt.Fprintln(os.Stdout, string(output))
}

func runHyprglassDebugOverlay(cmd *cobra.Command, args []string) {
	mode := strings.ToLower(strings.TrimSpace(args[0]))
	switch mode {
	case "on", "off", "toggle", "status":
	default:
		fmt.Fprintln(os.Stderr, "debug-overlay mode must be on, off, toggle, or status")
		os.Exit(1)
	}

	output, err := exec.Command("hyprctl", "hyprglass-debug-overlay", mode).CombinedOutput()
	if err != nil || hyprctlUnknownRequest(output) {
		fmt.Fprintf(os.Stderr, "hgs-hyprglass debug-overlay failed: %v\n%s\n", err, output)
		os.Exit(1)
	}
	fmt.Fprintln(os.Stdout, string(output))
}

func runHyprglassMaterial(cmd *cobra.Command, args []string) {
	mode := strings.ToLower(strings.TrimSpace(args[0]))
	switch mode {
	case "off", "flat", "blur-native", "glass-v1", "status":
	default:
		fmt.Fprintln(os.Stderr, "material mode must be off, flat, blur-native, glass-v1, or status")
		os.Exit(1)
	}

	output, err := exec.Command("hyprctl", "hyprglass-material", mode).CombinedOutput()
	if err != nil || hyprctlUnknownRequest(output) {
		fmt.Fprintf(os.Stderr, "hgs-hyprglass material failed: %v\n%s\n", err, output)
		os.Exit(1)
	}
	fmt.Fprintln(os.Stdout, string(output))
}

func applyHyprglassDescriptorSet(set hyprglass.DescriptorSet) {
	normalized, err := set.JSON()
	if err != nil {
		fmt.Fprintf(os.Stderr, "invalid descriptor payload: %v\n", err)
		os.Exit(1)
	}

	var compacted map[string]any
	if err := json.Unmarshal(normalized, &compacted); err != nil {
		fmt.Fprintf(os.Stderr, "failed to compact descriptor payload: %v\n", err)
		os.Exit(1)
	}
	payload, err := json.Marshal(compacted)
	if err != nil {
		fmt.Fprintf(os.Stderr, "failed to compact descriptor payload: %v\n", err)
		os.Exit(1)
	}

	output, err := exec.Command("hyprctl", "hyprglass-apply-json", string(payload)).CombinedOutput()
	if err != nil || hyprctlUnknownRequest(output) {
		fmt.Fprintf(os.Stderr, "hgs-hyprglass apply failed: %v\n%s\n", err, output)
		os.Exit(1)
	}
	fmt.Fprintln(os.Stdout, string(output))
}

func readHyprglassDescriptorSet(path string) hyprglass.DescriptorSet {
	data, err := os.ReadFile(path)
	if err != nil {
		fmt.Fprintf(os.Stderr, "failed to read descriptor payload: %v\n", err)
		os.Exit(1)
	}
	set, err := hyprglass.DecodeDescriptorSet(data)
	if err != nil {
		fmt.Fprintf(os.Stderr, "failed to decode descriptor payload: %v\n", err)
		os.Exit(1)
	}
	return set
}

func hyprglassStatusRegistered(output []byte) bool {
	if hyprctlUnknownRequest(output) {
		return false
	}

	var status struct {
		Plugin       string `json:"plugin"`
		PluginLoaded bool   `json:"pluginLoaded"`
	}
	if err := json.Unmarshal(output, &status); err != nil {
		return false
	}
	return status.Plugin == hyprglass.PluginName && status.PluginLoaded
}

func hyprctlUnknownRequest(output []byte) bool {
	return strings.Contains(strings.ToLower(strings.TrimSpace(string(output))), "unknown request")
}

func printHyprglassUnavailableStatus(reason string, err error, output []byte) {
	status := hyprglass.Status{
		Version:             hyprglass.DescriptorSchemaVersion,
		Plugin:              hyprglass.PluginName,
		PluginLoaded:        false,
		Available:           false,
		CompositorRendering: false,
		DescriptorCount:     0,
		HasPayload:          false,
		Reason:              reason,
		Material: hyprglass.MaterialStatus{
			Enabled:             false,
			Mode:                "off",
			RenderHookInstalled: false,
			RenderStage:         "disabled",
			LastRenderStatus:    "disabled",
		},
	}
	if err != nil {
		status.LastError = fmt.Sprintf("%v: %s", err, strings.TrimSpace(string(output)))
	}
	fallback, marshalErr := json.MarshalIndent(status, "", "  ")
	if marshalErr != nil {
		fmt.Fprintf(os.Stderr, "failed to format plugin status: %v\n", marshalErr)
		os.Exit(1)
	}
	fmt.Fprintln(os.Stdout, string(fallback))
}
