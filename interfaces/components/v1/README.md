# HyprShelld component contracts, version 1

This directory is the data contract for local HyprShelld component packages.
The schemas document the wire format; the shared component core performs the
authoritative validation and normalization.

A package is a ZIP-compatible `*.hyprshelld-component` archive containing one
component. `manifest.json` and `integrity.json` are required. A manifest may
declare the exact root file `settings.schema.json`. Packages cannot contain
install, update, migration, or removal scripts.

Component IDs are lowercase reverse-DNS identifiers. The complete prefix
`io.github.coastlinesec.hyprshelld.` is reserved for manifests loaded from the
protected system catalog. Origin, removability, review status, and trust are
derived from that catalog location and are never accepted as manifest claims.

Version 1 reserves these component and runtime names:

- `bar-widget`, `desktop-widget`, `shell-application`, `shell-service`;
- `builtin-v1`, `declarative-v1`, `qml-full-trust-v1`, `process-v1`.

Visual component types may use only visual runtime kinds. Applications and
services may use only `process-v1`. `builtin-v1` is available only to protected
system components. The first host implementation intentionally supports only
the built-in `workspace-switcher` factory; reserving a name does not mean the
runtime has shipped.

All JSON is UTF-8, has an object root, is limited to nesting depth 32, and must
not contain duplicate object keys. Manifests are limited to 128 KiB and
settings schemas to 256 KiB. Unknown fields are rejected. Presentation strings
are NFC-normalized, outer whitespace and control characters are rejected, and
Settings must render them as plain text.
Numeric lexemes must also survive parsing and canonical JSON serialization
without changing their decimal value; lossy halfway, overflow, and underflow
representations are rejected.

Integer settings, including their bounds, steps, defaults, and stored values,
are limited to the exact JSON-safe range -9007199254740991 through
9007199254740991. Values outside that range are rejected rather than rounded.

`integrity.json` uses SHA-256 and covers every regular archive file except
itself and the reserved future `signature.json`. The inspector additionally
requires its normalized key set to exactly match the archive. Author-provided
digests establish content consistency, not review or safety.

Settings schemas contain data only. They cannot reference QML, JavaScript,
commands, regular expressions, secrets, remote resources, or custom controls.
The trusted Settings application renders the supported controls.
