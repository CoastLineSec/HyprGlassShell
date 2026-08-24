#!/usr/bin/env python3
"""Derive and verify the immutable legacy shortcut reference.

The legacy Lua file is treated as data. This module never evaluates Lua.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Iterable


SOURCE_SHA256 = "47bbde429980d2fa9817c88915cac595ec887573802ed162980613f576b9979d"
SOURCE_PATH = "core/internal/config/embedded/hyprland/hgs/keybinds.lua"
SOURCE_BYTE_LENGTH = 9738
SOURCE_LINE_COUNT = 171
BINDING_COUNT = 117

ACTION_PATHS = {
    "hl.dsp.dpms",
    "hl.dsp.exec_cmd",
    "hl.dsp.exit",
    "hl.dsp.focus",
    "hl.dsp.group.toggle",
    "hl.dsp.layout",
    "hl.dsp.window.drag",
    "hl.dsp.window.float",
    "hl.dsp.window.fullscreen",
    "hl.dsp.window.kill",
    "hl.dsp.window.move",
    "hl.dsp.window.resize",
}
OPTION_KEYS = {"description", "locked", "mouse", "repeating"}
IDENTIFIER = re.compile(r"[A-Za-z_][A-Za-z0-9_]*\Z")
INTEGER = re.compile(r"-?(?:0|[1-9][0-9]*)\Z")


@dataclass(frozen=True)
class ParsedBinding:
    ordinal: int
    source_line: int
    section: str
    source_text: str
    chord: str
    action: str
    options: dict[str, bool | str]

    def document(self) -> dict[str, Any]:
        return {
            "action": self.action,
            "chord": self.chord,
            "options": self.options,
            "ordinal": self.ordinal,
            "section": self.section,
            "sourceLine": self.source_line,
            "sourceText": self.source_text,
        }


class LegacyShortcutError(ValueError):
    pass


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def canonical_json_bytes(document: Any) -> bytes:
    return (
        json.dumps(
            document,
            ensure_ascii=False,
            allow_nan=False,
            sort_keys=True,
            separators=(",", ":"),
        )
        + "\n"
    ).encode("utf-8")


def _split_top_level(text: str) -> list[str]:
    parts: list[str] = []
    start = 0
    round_depth = 0
    brace_depth = 0
    in_quote = False
    in_long = False
    index = 0
    while index < len(text):
        if in_quote:
            if text[index] == "\\":
                raise LegacyShortcutError("escaped strings are outside the reviewed grammar")
            if text[index] == '"':
                in_quote = False
            index += 1
            continue
        if in_long:
            if text.startswith("]]", index):
                in_long = False
                index += 2
            else:
                index += 1
            continue
        if text.startswith("[[", index):
            in_long = True
            index += 2
            continue
        character = text[index]
        if character == '"':
            in_quote = True
        elif character == "(":
            round_depth += 1
        elif character == ")":
            round_depth -= 1
            if round_depth < 0:
                raise LegacyShortcutError("unbalanced closing parenthesis")
        elif character == "{":
            brace_depth += 1
        elif character == "}":
            brace_depth -= 1
            if brace_depth < 0:
                raise LegacyShortcutError("unbalanced closing brace")
        elif character == "," and round_depth == 0 and brace_depth == 0:
            parts.append(text[start:index].strip())
            start = index + 1
        index += 1
    if in_quote or in_long or round_depth != 0 or brace_depth != 0:
        raise LegacyShortcutError("unterminated string or unbalanced expression")
    parts.append(text[start:].strip())
    if any(not part for part in parts):
        raise LegacyShortcutError("empty or trailing argument")
    return parts


def _quoted_string(text: str) -> str:
    if len(text) < 2 or text[0] != '"' or text[-1] != '"':
        raise LegacyShortcutError("a reviewed double-quoted string is required")
    value = text[1:-1]
    if '"' in value or "\\" in value or "\n" in value or "\r" in value:
        raise LegacyShortcutError("the reviewed string grammar changed")
    return value


def _scalar(text: str) -> bool | int | str:
    if text.startswith('"'):
        return _quoted_string(text)
    if text.startswith("[["):
        if (
            len(text) < 4
            or not text.endswith("]]")
            or "[[" in text[2:-2]
            or "]]" in text[2:-2]
        ):
            raise LegacyShortcutError("the reviewed long-string grammar changed")
        return text[2:-2]
    if text == "true":
        return True
    if text == "false":
        return False
    if INTEGER.fullmatch(text):
        return int(text)
    raise LegacyShortcutError(f"unsupported scalar {text!r}")


def _flat_table(text: str) -> dict[str, bool | int | str]:
    if len(text) < 2 or text[0] != "{" or text[-1] != "}":
        raise LegacyShortcutError("a flat named table is required")
    body = text[1:-1].strip()
    if not body:
        return {}
    result: dict[str, bool | int | str] = {}
    for entry in _split_top_level(body):
        if "=" not in entry:
            raise LegacyShortcutError("positional table entries are not allowed")
        key, value = entry.split("=", 1)
        key = key.strip()
        if not IDENTIFIER.fullmatch(key):
            raise LegacyShortcutError(f"invalid table key {key!r}")
        if key in result:
            raise LegacyShortcutError(f"duplicate table key {key!r}")
        result[key] = _scalar(value.strip())
    return result


def _action_expression(text: str) -> None:
    opening = text.find("(")
    if opening <= 0 or not text.endswith(")"):
        raise LegacyShortcutError("the binding action must be one dispatcher call")
    path = text[:opening]
    if path not in ACTION_PATHS:
        raise LegacyShortcutError(f"unsupported dispatcher path {path!r}")
    body = text[opening + 1 : -1].strip()
    if not body:
        return
    arguments = _split_top_level(body)
    if len(arguments) != 1:
        raise LegacyShortcutError("dispatchers accept at most one reviewed argument")
    argument = arguments[0]
    if argument.startswith("{"):
        _flat_table(argument)
    else:
        value = _scalar(argument)
        if not isinstance(value, str):
            raise LegacyShortcutError("a dispatcher scalar argument must be a string")


def _options(text: str) -> dict[str, bool | str]:
    parsed = _flat_table(text)
    unknown = set(parsed) - OPTION_KEYS
    if unknown:
        raise LegacyShortcutError(
            f"unsupported binding option {sorted(unknown)[0]!r}"
        )
    for key, value in parsed.items():
        if key == "description":
            if not isinstance(value, str):
                raise LegacyShortcutError("description must be a string")
        elif value is not True:
            raise LegacyShortcutError(f"{key} must be the present true flag")
    return parsed


def _section_title(comment: str) -> str | None:
    body = comment[2:].strip()
    if not body.startswith("==="):
        return None
    title = body[3:].strip()
    if title.endswith("==="):
        title = title[:-3].rstrip()
    if not title:
        raise LegacyShortcutError("empty section heading")
    return title


def extract_bindings(
    source: bytes,
    *,
    expected_digest: str = SOURCE_SHA256,
) -> list[ParsedBinding]:
    if sha256_bytes(source) != expected_digest:
        raise LegacyShortcutError("legacy source digest mismatch")
    if expected_digest == SOURCE_SHA256:
        if len(source) != SOURCE_BYTE_LENGTH:
            raise LegacyShortcutError("legacy source byte length mismatch")
        if not source.endswith(b"\n") or b"\r" in source:
            raise LegacyShortcutError("legacy source must use LF and end with LF")
    try:
        text = source.decode("ascii")
    except UnicodeDecodeError as error:
        raise LegacyShortcutError("legacy source must be ASCII") from error
    lines = text.splitlines()
    if expected_digest == SOURCE_SHA256 and len(lines) != SOURCE_LINE_COUNT:
        raise LegacyShortcutError("legacy source line count mismatch")

    section = ""
    result: list[ParsedBinding] = []
    for line_number, line in enumerate(lines, start=1):
        if not line:
            continue
        if line.startswith("--"):
            if re.match(r"--\[(?:=*)\[", line):
                raise LegacyShortcutError(
                    f"line {line_number}: Lua long comments are outside the reviewed grammar"
                )
            candidate = _section_title(line)
            if candidate is not None:
                section = candidate
            continue
        prefix = "hl.bind("
        if not line.startswith(prefix) or not line.endswith(")"):
            raise LegacyShortcutError(
                f"line {line_number}: unexpected executable source"
            )
        if not section:
            raise LegacyShortcutError(
                f"line {line_number}: binding has no legacy section"
            )
        arguments = _split_top_level(line[len(prefix) : -1])
        if len(arguments) not in (2, 3):
            raise LegacyShortcutError(
                f"line {line_number}: binding requires two or three arguments"
            )
        chord = _quoted_string(arguments[0])
        _action_expression(arguments[1])
        options = _options(arguments[2]) if len(arguments) == 3 else {}
        result.append(
            ParsedBinding(
                ordinal=len(result) + 1,
                source_line=line_number,
                section=section,
                source_text=line,
                chord=chord,
                action=arguments[1],
                options=options,
            )
        )

    if len(result) != BINDING_COUNT:
        raise LegacyShortcutError(
            f"expected {BINDING_COUNT} bindings, found {len(result)}"
        )
    return result


def build_document(
    source: bytes,
    *,
    expected_digest: str = SOURCE_SHA256,
) -> dict[str, Any]:
    rows = extract_bindings(source, expected_digest=expected_digest)
    return {
        "contractVersion": 1,
        "rows": [row.document() for row in rows],
        "source": {
            "bindingCount": len(rows),
            "byteLength": len(source),
            "lineCount": len(source.splitlines()),
            "path": SOURCE_PATH,
            "sha256": expected_digest,
        },
    }


def _reject_duplicate_keys(pairs: Iterable[tuple[str, Any]]) -> dict[str, Any]:
    result: dict[str, Any] = {}
    for key, value in pairs:
        if key in result:
            raise LegacyShortcutError(f"duplicate JSON key {key!r}")
        result[key] = value
    return result


def _reject_json_constant(token: str) -> Any:
    raise LegacyShortcutError(f"unsupported JSON constant {token!r}")


def parse_artifact(data: bytes) -> dict[str, Any]:
    try:
        document = json.loads(
            data,
            object_pairs_hook=_reject_duplicate_keys,
            parse_constant=_reject_json_constant,
        )
    except LegacyShortcutError:
        raise
    except (ValueError, UnicodeDecodeError, OverflowError) as error:
        raise LegacyShortcutError("legacy reference artifact is not valid JSON") from error
    if not isinstance(document, dict):
        raise LegacyShortcutError("legacy reference artifact root must be an object")
    try:
        canonical = canonical_json_bytes(document)
    except (ValueError, OverflowError) as error:
        raise LegacyShortcutError(
            "legacy reference artifact contains an unsupported numeric value"
        ) from error
    if canonical != data:
        raise LegacyShortcutError("legacy reference artifact is not canonical")
    return document


def check_artifact(source: bytes, artifact: bytes) -> None:
    expected = build_document(source)
    actual = parse_artifact(artifact)
    if actual != expected:
        raise LegacyShortcutError("legacy reference artifact differs from source")
    if artifact != canonical_json_bytes(expected):
        raise LegacyShortcutError("legacy reference artifact bytes differ from source")


def _arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Verify the immutable legacy shortcut reference"
    )
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--artifact", type=Path, required=True)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--check", action="store_true")
    mode.add_argument("--write", action="store_true")
    return parser.parse_args()


def main() -> int:
    arguments = _arguments()
    try:
        source = arguments.source.read_bytes()
        expected = canonical_json_bytes(build_document(source))
        if arguments.write:
            arguments.artifact.parent.mkdir(parents=True, exist_ok=True)
            arguments.artifact.write_bytes(expected)
        else:
            check_artifact(source, arguments.artifact.read_bytes())
    except (LegacyShortcutError, OSError) as error:
        print(f"legacy shortcut validation failed: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
