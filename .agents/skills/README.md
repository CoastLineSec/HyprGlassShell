# Agent Skills

This directory contains agent skills following the [Agent Skills](https://agentskills.io) open standard - a portable, version-controlled format for giving AI agents specialized capabilities.

Each skill is a directory with a `SKILL.md` entrypoint, optional reference docs, scripts, and templates. Agents load skills progressively: metadata at startup, full instructions on activation, and supporting files on demand.

## Available Skills

| Skill | Description |
|-------|-------------|
| [hgs-plugin-dev](hgs-plugin-dev/) | Develop plugins for HyprGlassShell - covers all 4 plugin types (widget, daemon, launcher, desktop), manifest creation, QML components, settings UI, data persistence, theme integration, and PopoutService usage. |

## Installation

The `.agents/skills/` directory at the project root is the standard location defined by the agentskills.io spec. Many agents discover skills from this path automatically. Some agents use their own directory conventions and need a symlink or copy.

### Claude Code

Claude Code discovers skills from `.claude/skills/` (project-level) or `~/.claude/skills/` (personal). To make skills from `.agents/skills/` available, symlink them into the Claude Code skills directory:

**Project-level** (this repo only):

```bash
mkdir -p .claude/skills
ln -s ../../.agents/skills/hgs-plugin-dev .claude/skills/hgs-plugin-dev
```

**Personal** (all your projects):

```bash
ln -s /path/to/HyprGlassShell/.agents/skills/hgs-plugin-dev ~/.claude/skills/hgs-plugin-dev
```

After linking, the skill appears in Claude Code's `/` menu as `/hgs-plugin-dev`, and Claude loads it automatically when you ask about HGS plugin development.

See the [Claude Code skills docs](https://code.claude.com/docs/en/skills) for more on skill configuration, invocation control, and frontmatter options.

### Cursor

Cursor discovers skills from `.cursor/skills/` in the project root:

```bash
mkdir -p .cursor/skills
ln -s ../../.agents/skills/hgs-plugin-dev .cursor/skills/hgs-plugin-dev
```

See [Cursor skills docs](https://cursor.com/docs/context/skills) for details.

### VS Code (Copilot)

VS Code Copilot discovers skills from `.github/skills/` or `.vscode/skills/`:

```bash
mkdir -p .github/skills
ln -s ../../.agents/skills/hgs-plugin-dev .github/skills/hgs-plugin-dev
```

See [VS Code skills docs](https://code.visualstudio.com/docs/copilot/customization/agent-skills) for details.

### Gemini CLI

Gemini CLI discovers skills from `.gemini/skills/` in the project root:

```bash
mkdir -p .gemini/skills
ln -s ../../.agents/skills/hgs-plugin-dev .gemini/skills/hgs-plugin-dev
```

See [Gemini CLI skills docs](https://geminicli.com/docs/cli/skills/) for details.

### OpenAI Codex

Codex discovers skills from `.codex/skills/` in the project root:

```bash
mkdir -p .codex/skills
ln -s ../../.agents/skills/hgs-plugin-dev .codex/skills/hgs-plugin-dev
```

See [Codex skills docs](https://developers.openai.com/codex/skills/) for details.

### Other Agents

The Agent Skills standard is supported by 30+ tools including Goose, Roo Code, JetBrains Junie, Amp, OpenCode, OpenHands, Kiro, and more. Most discover skills from a dot-directory at the project root (e.g., `.goose/skills/`, `.roo/skills/`). Some read `.agents/skills/` directly.

Check the [Agent Skills client showcase](https://agentskills.io/clients) for setup instructions specific to your agent.

The general pattern is:

```bash
mkdir -p .<agent>/skills
ln -s ../../.agents/skills/hgs-plugin-dev .<agent>/skills/hgs-plugin-dev
```

## Adding New Skills

To add a new skill to this directory:

1. Create a subdirectory named with lowercase letters, numbers, and hyphens (e.g., `my-new-skill/`)
2. Add a `SKILL.md` file with YAML frontmatter (`name`, `description`) and markdown instructions
3. Optionally add `references/`, `scripts/`, and `assets/` subdirectories
4. Keep `SKILL.md` under 500 lines - move detailed content to reference files

See the [Agent Skills specification](https://agentskills.io/specification) for the full format.
