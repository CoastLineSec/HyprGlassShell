package main

import (
	"encoding/json"
	"fmt"
	"os"
	"path/filepath"
	"strings"
	"time"

	"github.com/CoastLineSec/HyprGlassShell/core/internal/log"
	"github.com/CoastLineSec/HyprGlassShell/core/internal/utils"
	"github.com/CoastLineSec/HyprGlassShell/core/internal/windowrules"
	"github.com/CoastLineSec/HyprGlassShell/core/internal/windowrules/providers"
	"github.com/spf13/cobra"
)

var windowrulesCmd = &cobra.Command{
	Use:   "windowrules",
	Short: "Manage window rules",
}

var windowrulesListCmd = &cobra.Command{
	Use:   "list [compositor]",
	Short: "List all window rules",
	Long:  "List all window rules from compositor config file. Returns JSON with rules and HGS status.",
	Args:  cobra.MaximumNArgs(1),
	ValidArgsFunction: func(cmd *cobra.Command, args []string, toComplete string) ([]string, cobra.ShellCompDirective) {
		if len(args) == 0 {
			return []string{"hyprland"}, cobra.ShellCompDirectiveNoFileComp
		}
		return nil, cobra.ShellCompDirectiveNoFileComp
	},
	Run: runWindowrulesList,
}

var windowrulesAddCmd = &cobra.Command{
	Use:   "add <compositor> '<json>'",
	Short: "Add a window rule to HGS file",
	Long:  "Add a new window rule to the HGS-managed rules file.",
	Args:  cobra.ExactArgs(2),
	ValidArgsFunction: func(cmd *cobra.Command, args []string, toComplete string) ([]string, cobra.ShellCompDirective) {
		if len(args) == 0 {
			return []string{"hyprland"}, cobra.ShellCompDirectiveNoFileComp
		}
		return nil, cobra.ShellCompDirectiveNoFileComp
	},
	Run: runWindowrulesAdd,
}

var windowrulesUpdateCmd = &cobra.Command{
	Use:   "update <compositor> <id> '<json>'",
	Short: "Update a window rule in HGS file",
	Long:  "Update an existing window rule in the HGS-managed rules file.",
	Args:  cobra.ExactArgs(3),
	ValidArgsFunction: func(cmd *cobra.Command, args []string, toComplete string) ([]string, cobra.ShellCompDirective) {
		if len(args) == 0 {
			return []string{"hyprland"}, cobra.ShellCompDirectiveNoFileComp
		}
		return nil, cobra.ShellCompDirectiveNoFileComp
	},
	Run: runWindowrulesUpdate,
}

var windowrulesRemoveCmd = &cobra.Command{
	Use:   "remove <compositor> <id>",
	Short: "Remove a window rule from HGS file",
	Long:  "Remove a window rule from the HGS-managed rules file.",
	Args:  cobra.ExactArgs(2),
	ValidArgsFunction: func(cmd *cobra.Command, args []string, toComplete string) ([]string, cobra.ShellCompDirective) {
		if len(args) == 0 {
			return []string{"hyprland"}, cobra.ShellCompDirectiveNoFileComp
		}
		return nil, cobra.ShellCompDirectiveNoFileComp
	},
	Run: runWindowrulesRemove,
}

var windowrulesReorderCmd = &cobra.Command{
	Use:   "reorder <compositor> '<json-array-of-ids>'",
	Short: "Reorder window rules in HGS file",
	Long:  "Reorder window rules by providing a JSON array of rule IDs in the desired order.",
	Args:  cobra.ExactArgs(2),
	ValidArgsFunction: func(cmd *cobra.Command, args []string, toComplete string) ([]string, cobra.ShellCompDirective) {
		if len(args) == 0 {
			return []string{"hyprland"}, cobra.ShellCompDirectiveNoFileComp
		}
		return nil, cobra.ShellCompDirectiveNoFileComp
	},
	Run: runWindowrulesReorder,
}

func init() {
	configCmd.AddCommand(windowrulesCmd)
	windowrulesCmd.AddCommand(windowrulesListCmd)
	windowrulesCmd.AddCommand(windowrulesAddCmd)
	windowrulesCmd.AddCommand(windowrulesUpdateCmd)
	windowrulesCmd.AddCommand(windowrulesRemoveCmd)
	windowrulesCmd.AddCommand(windowrulesReorderCmd)
}

type WindowRulesListResult struct {
	Rules     []windowrules.WindowRule    `json:"rules"`
	HGSStatus *windowrules.HGSRulesStatus `json:"hgsStatus,omitempty"`
}

type WindowRuleWriteResult struct {
	Success bool   `json:"success"`
	ID      string `json:"id,omitempty"`
	Path    string `json:"path,omitempty"`
	Error   string `json:"error,omitempty"`
}

func getCompositor(args []string) string {
	if len(args) > 0 {
		return strings.ToLower(args[0])
	}
	if os.Getenv("HYPRLAND_INSTANCE_SIGNATURE") != "" {
		return "hyprland"
	}
	return ""
}

func writeRuleError(errMsg string) {
	result := WindowRuleWriteResult{Success: false, Error: errMsg}
	output, _ := json.Marshal(result)
	fmt.Fprintln(os.Stdout, string(output))
	os.Exit(1)
}

func writeRuleSuccess(id, path string) {
	result := WindowRuleWriteResult{Success: true, ID: id, Path: path}
	output, _ := json.Marshal(result)
	fmt.Fprintln(os.Stdout, string(output))
}

func runWindowrulesList(cmd *cobra.Command, args []string) {
	compositor := getCompositor(args)
	if compositor == "" {
		log.Fatalf("Could not detect Hyprland. Please specify: hyprland")
	}

	var result WindowRulesListResult

	switch compositor {
	case "hyprland":
		configDir := filepath.Join(utils.XDGConfigHome(), "hypr")

		parseResult, err := providers.ParseHyprlandWindowRules(configDir)
		if err != nil {
			log.Fatalf("Failed to parse hyprland window rules: %v", err)
		}

		allRules := providers.ConvertHyprlandRulesToWindowRules(parseResult.Rules)

		provider := providers.NewHyprlandWritableProvider(configDir)
		hgsRulesPath := provider.GetOverridePath()
		hgsRules, _ := provider.LoadHGSRules()

		hgsRuleMap := make(map[int]windowrules.WindowRule)
		for i, dr := range hgsRules {
			hgsRuleMap[i] = dr
		}

		hgsIdx := 0
		for i, r := range allRules {
			if r.Source == hgsRulesPath {
				if dmr, ok := hgsRuleMap[hgsIdx]; ok {
					allRules[i].ID = dmr.ID
					allRules[i].Name = dmr.Name
				}
				hgsIdx++
			}
		}

		result.Rules = allRules
		result.HGSStatus = parseResult.HGSStatus

	default:
		log.Fatalf("Unsupported compositor %q: HyprGlassShell currently supports Hyprland only", compositor)
	}

	output, _ := json.Marshal(result)
	fmt.Fprintln(os.Stdout, string(output))
}

func runWindowrulesAdd(cmd *cobra.Command, args []string) {
	compositor := strings.ToLower(args[0])
	ruleJSON := args[1]

	var rule windowrules.WindowRule
	if err := json.Unmarshal([]byte(ruleJSON), &rule); err != nil {
		writeRuleError(fmt.Sprintf("Invalid JSON: %v", err))
	}

	if rule.ID == "" {
		rule.ID = generateRuleID()
	}
	rule.Enabled = true

	provider := getWindowRulesProvider(compositor)
	if provider == nil {
		writeRuleError(fmt.Sprintf("Unsupported compositor %q: HyprGlassShell currently supports Hyprland only", compositor))
	}

	if err := provider.SetRule(rule); err != nil {
		writeRuleError(err.Error())
	}

	writeRuleSuccess(rule.ID, provider.GetOverridePath())
}

func runWindowrulesUpdate(cmd *cobra.Command, args []string) {
	compositor := strings.ToLower(args[0])
	ruleID := args[1]
	ruleJSON := args[2]

	var rule windowrules.WindowRule
	if err := json.Unmarshal([]byte(ruleJSON), &rule); err != nil {
		writeRuleError(fmt.Sprintf("Invalid JSON: %v", err))
	}

	rule.ID = ruleID

	provider := getWindowRulesProvider(compositor)
	if provider == nil {
		writeRuleError(fmt.Sprintf("Unsupported compositor %q: HyprGlassShell currently supports Hyprland only", compositor))
	}

	if err := provider.SetRule(rule); err != nil {
		writeRuleError(err.Error())
	}

	writeRuleSuccess(rule.ID, provider.GetOverridePath())
}

func runWindowrulesRemove(cmd *cobra.Command, args []string) {
	compositor := strings.ToLower(args[0])
	ruleID := args[1]

	provider := getWindowRulesProvider(compositor)
	if provider == nil {
		writeRuleError(fmt.Sprintf("Unsupported compositor %q: HyprGlassShell currently supports Hyprland only", compositor))
	}

	if err := provider.RemoveRule(ruleID); err != nil {
		writeRuleError(err.Error())
	}

	writeRuleSuccess(ruleID, provider.GetOverridePath())
}

func runWindowrulesReorder(cmd *cobra.Command, args []string) {
	compositor := strings.ToLower(args[0])
	idsJSON := args[1]

	var ids []string
	if err := json.Unmarshal([]byte(idsJSON), &ids); err != nil {
		writeRuleError(fmt.Sprintf("Invalid JSON array: %v", err))
	}

	provider := getWindowRulesProvider(compositor)
	if provider == nil {
		writeRuleError(fmt.Sprintf("Unsupported compositor %q: HyprGlassShell currently supports Hyprland only", compositor))
	}

	if err := provider.ReorderRules(ids); err != nil {
		writeRuleError(err.Error())
	}

	writeRuleSuccess("", provider.GetOverridePath())
}

func getWindowRulesProvider(compositor string) windowrules.WritableProvider {
	switch compositor {
	case "hyprland":
		configDir := filepath.Join(utils.XDGConfigHome(), "hypr")
		return providers.NewHyprlandWritableProvider(configDir)
	default:
		return nil
	}
}

func generateRuleID() string {
	return fmt.Sprintf("wr_%d", time.Now().UnixNano())
}
