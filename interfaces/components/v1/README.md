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
system components. The host activates the protected `workspace-switcher` and a
strict, data-only `declarative-v1` subset for third-party bar widgets. The
`qml-full-trust-v1`, `process-v1`, and desktop-widget runtimes remain inert;
reserving a name does not mean that runtime has shipped.

The `declarative-v1` entrypoint is a JSON document governed by
`declarative.schema.json`, limited to 16 KiB and nesting depth 8. Version one
has exactly one trusted primitive, `text-pill`. Its text is either a bounded
literal or a reference to a component-scoped string or enumeration setting in
the same package. Literal, resolved string, and enumeration values are limited
to 128 characters; a referenced string definition must declare a maximum no
larger than that bound and a minimum of at least one. Resolved text remains
subject to the same NFC, padding, and control/format-character checks as a
literal. A pill may add a literal tooltip of at most 256 characters and a
maximum width from 48 through 512 logical pixels. Unknown fields are rejected.
Documents cannot contain QML, JavaScript, actions, assets, URLs, commands, styles, or
resource paths. The isolated inspector validates and normalizes the entrypoint;
only canonical JSON bytes—not the package path—cross the manager/runtime
boundary. Declarative widgets request no capabilities and declare no component
dependencies in this first activation slice. A bound setting's default and
every possible enumeration value must satisfy the same final renderer-text
boundary, so a package cannot advertise compatibility with an unusable default.
Active declarative instances are hosted only from the `main` bar layout; an
off-main, disabled, unknown, or digest-mismatched placement remains inert and
preserved rather than being treated as active.

All JSON is UTF-8, has an object root, is limited to nesting depth 32, and must
not contain duplicate object keys. Manifests are limited to 128 KiB,
settings schemas to 256 KiB, and integrity metadata to 512 KiB. A package is
limited to 32 MiB of archive bytes, 128 MiB expanded, 32 MiB per regular file,
and 512 total ZIP entries. Unknown fields are rejected. Presentation strings
are NFC-normalized; outer whitespace, control characters, and Unicode format
controls such as bidi overrides and zero-width joiners are rejected. Settings
must render them as plain text.
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
