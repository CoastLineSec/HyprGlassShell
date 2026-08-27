# HyprShelld D-Bus Contracts

The XML files in this directory are the machine-readable authority for
HyprShelld's public per-user D-Bus interfaces. Generated Qt bindings and
contract tests lock their wire signatures. A bounded client that decodes a
reply manually must still match the authoritative XML exactly and reject any
different types, field counts, or ordering.

| Service | Bus name | Object path | Interface |
| --- | --- | --- | --- |
| Coordinator | `org.hyprshelld.Coordinator1` | `/org/hyprshelld/Coordinator1` | `org.hyprshelld.Coordinator1` |
| Component runtime | `org.hyprshelld.Coordinator1` | `/org/hyprshelld/Coordinator1/Components` | `org.hyprshelld.ComponentRuntime1` |
| Configuration | `org.hyprshelld.Config1` | `/org/hyprshelld/Config1` | `org.hyprshelld.Config1` |
| Component configuration | `org.hyprshelld.Config1` | `/org/hyprshelld/Config1/Components` | `org.hyprshelld.ComponentConfig1` |
| Component manager | `org.hyprshelld.ComponentManager1` | `/org/hyprshelld/ComponentManager1` | `org.hyprshelld.ComponentManager1` |
| Compositor configuration | `org.hyprshelld.Compositor1` | `/org/hyprshelld/Compositor2` | `org.hyprshelld.Compositor2` |
| Legacy compositor compatibility | `org.hyprshelld.Compositor1` | `/org/hyprshelld/Compositor1` | `org.hyprshelld.Compositor1` |

All services use the session bus. Configuration, compositor configuration, and
the component manager may be D-Bus activated so Settings can remain independent
when the shell target is stopped. Settings treats an absent coordinator bus
name as coordinator unavailability.

## Coordinator

`FailedUnits` contains sorted, unique HyprShelld systemd user-unit names whose
persistent failure requires action. `Healthy` is true exactly when that list is
empty. Successful `RestartComponent` means the bounded recovery request was
accepted; health changes only after systemd reports recovery.

Coordinator1 accepts only `hyprshelld-configd.service`,
`hyprshelld-componentd.service`, `hyprshelld-compositord.service`, and
`hyprshelld-surfaced.service`. The method cannot be used as a generic systemd
restart endpoint.

Restart errors have these meanings:

- `org.hyprshelld.Coordinator1.Error.UnknownComponent`: the supplied unit name
  is not in the coordinator's allowlist;
- `org.hyprshelld.Coordinator1.Error.ComponentNotFailed`: the unit is known but
  has no persistent failure eligible for a manual restart; and
- `org.hyprshelld.Coordinator1.Error.RestartFailed`: systemd did not accept the
  bounded restart request.

## Configuration

`BarHeight` is measured in logical pixels, defaults to 40, and accepts values
from 24 through 96. `SetBarHeight` and `ResetBarHeight` persist a real change
atomically before publishing it and return the resulting revision. An
idempotent request returns the existing revision without writing or emitting a
change.

The shared shell-border tuple contains `ShellBorderEnabled`,
`ShellBorderWidth`, `ShellBorderRadius`, and
`SyncHyprlandWindowBorders`. Width and radius are measured in logical pixels
and each accepts values from 0 through 20. New state defaults to an enabled
1-pixel border, radius 15, with Hyprland window-border synchronization enabled.
`SetSharedBorder` changes the complete tuple atomically, while
`ResetSharedBorder` restores those defaults. A real tuple change is persisted
before one combined property notification and revision increment; an
idempotent request performs neither.

The shared spacing tuple contains `ShellInnerSpacing`, `ShellOuterSpacing`, and
`SyncHyprlandWindowSpacing`. Inner and outer spacing are logical pixels from 0
through 32. New and reset state uses inner 8, outer 12, and synchronization
enabled. `SetSharedSpacing` changes the complete tuple atomically and
`ResetSharedSpacing` restores those defaults, with the same persistence,
revision, and combined-notification semantics as the border tuple.

`AppearanceMode` is exactly one of `automatic`, `light`, or `dark`.
`automatic` follows the current desktop color scheme; the explicit modes do
not. `SetAppearanceMode` persists a real change before publishing it, while
`ResetAppearanceMode` restores `dark`.

The current snapshot format is version 4. Valid version-1 through version-3
snapshots migrate during loading without incrementing their revision. Version
1 receives the established border migration; later versions retain their
complete border tuple. Versions 1 and 2 receive inner 8, outer 12, and spacing
synchronization disabled so an upgrade does not replace existing managed gaps;
version 3 retains its complete spacing tuple. All three receive `dark`, which
preserves the presentation used before appearance modes existed. The recovery
snapshot is rewritten before the active snapshot and before publication.

The active snapshot follows the XDG base directories at
`$XDG_CONFIG_HOME/hyprshelld/settings.json`, normally
`~/.config/hyprshelld/settings.json`. Its last-known-good recovery snapshot is
internal state at `$XDG_STATE_HOME/hyprshelld/settings.last-good.json`, normally
`~/.local/state/hyprshelld/settings.last-good.json`.

`RecoveryState` describes startup loading for the current service lifetime:

- `normal`: valid state loaded, or no state existed on an ordinary first run;
- `recovered`: damaged active state was replaced by last-known-good state; or
- `defaulted`: damaged active state existed and no valid last-known-good state
  was available, so defaults were used.

Clients treat unknown future values as non-normal and display a recovery
warning.

Configuration errors have these meanings:

- `org.hyprshelld.Config1.Error.InvalidBarHeight`: the requested height is
  outside the accepted range;
- `org.hyprshelld.Config1.Error.InvalidSharedBorder`: the requested border
  width or radius is outside the accepted range;
- `org.hyprshelld.Config1.Error.InvalidSharedSpacing`: the requested inner or
  outer spacing is outside the accepted range;
- `org.hyprshelld.Config1.Error.InvalidAppearanceMode`: the requested mode is
  not `automatic`, `light`, or `dark`; and
- `org.hyprshelld.Config1.Error.PersistenceFailed`: the new state could not be
  persisted atomically, so the active value and revision remain unchanged.

## Compositor configuration

Compositor2 is the authority-epoch contract for the sole desired-state and
generated-Lua authority. It is served by the existing
`org.hyprshelld.Compositor1` destination at `/org/hyprshelld/Compositor2`
with interface `org.hyprshelld.Compositor2`. This is one activation name, one
compositord process, one store lease, and one writer; the numeric interface
version does not introduce another daemon.

The `AuthorityId` is a random durable 128-bit authority epoch encoded as
exactly 32 lowercase hexadecimal characters. `GetSnapshot` returns it with
the canonical desired bytes, revision, and both catalog digests. Every ordinary
mutation and explicit shared-visual retry accepts
`expectedAuthorityId` and `expectedRevision`; each display terminal action
accepts `expectedAuthorityId` and `expectedPreviewRevision`. After
availability and writability, the authority ID is compared before catalog
digests and revision.
The candidate passed to `ReplaceSnapshot` must embed the exact expected
authority ID and revision. A revision, digest, retry, draft, or private display
token from another authority is never accepted even when its numeric values
happen to match.

The legacy `/org/hyprshelld/Compositor1` object is a compatibility sentinel.
From the first public instant its `Available` and `Writable` properties
remain permanently false, so no legacy desired-state property tuple is
authoritative. Its exact readable-method allowlist is
`GetConnectedDisplays` and `GetConnectedInputDevices`. Every other method,
including all desired-state reads, catalog reads, confirmation-capability
reads, and every mutation, permanently returns
`org.hyprshelld.Compositor1.Error.UpgradeRequired`. New clients use
Compositor2 and must not fall back to Compositor1 after `UpgradeRequired`.

The Compositor2 XML and tests are dormant contract material in this source
slice: no runtime adaptor or registered Compositor2 object is inferred by their
presence. The reviewed Restart methods are part of that dormant ABI, but no
public phase-advance, Cancel, or RestartUnit method exists and `restart` remains
an unavailable activation requirement in the selected runtime.

The compositor bus name is acquired before compositord opens the persistent
store, takes its exclusive lease, or performs recovery. Until reconciliation
finishes, `Available` and `Writable` are false and ordinary authority
methods fail `Unavailable`. Losing the D-Bus name race therefore cannot
repair or mutate another instance's store.
`Writable` describes only whether that desired-state authority accepts
mutations; it does not imply that the installed activation executor can satisfy
the activation reported by `RequiredActivation`.

`GetSnapshot` returns one complete canonical Hyprland desired-state document,
its authority ID, revision, and the exact scalar and action catalog digests
that own those bytes. `GetOptionCatalog` returns five values in this exact
order: `optionCatalog`, `authorityId`, `revision`, `catalogDigest`, and
`actionCatalogDigest`. The catalog reply is bounded to 4 MiB and fails
`Unavailable` unless its SHA-256 equals the returned `catalogDigest`.

`GetActionCatalog` returns seven values in this exact order: `actionCatalog`,
`configSchema`, `authorityId`, `revision`, `catalogDigest`,
`actionCatalogDigest`, and `configSchemaDigest`. The canonical action catalog
is bounded to 1 MiB and the schema to 2 MiB. The schema digest is the SHA-256
of those exact schema bytes. The combined action digest is the SHA-256 authority
over the canonical action-catalog JSON, one newline, and those exact schema
bytes; it must equal the returned `actionCatalogDigest`.
Compositord serves both catalog methods from the already parsed, retained
authority and never reopens a catalog or schema path to answer either request.
Each getter returns one coherent retained in-memory authority tuple; clients
reject any mismatch with `GetSnapshot` or the coherent properties and reacquire
all authority material. These are read-only authority views: they do not change
desired state, the persistent store, generated files, the managed entrypoint,
or live Hyprland configuration. `ReplaceSnapshot` compares the authority ID, both catalog
digests, and revision before parsing the candidate. The candidate embeds the
current expected authority ID and revision; a real change is assigned exactly
the next revision within the same authority and is made durable before one
coherent property tuple is published. If that successful response is lost, an
exact retry using the immediately preceding tuple returns the already-committed
revision only inside that authority, without incrementing twice. Replacement
only changes desired state. It never generates Lua, changes the compositor
entrypoint, or reloads Hyprland.
Managed cross-field safety is checked only for a real changed candidate, after
the exact no-op and immediately preceding lost-reply cases. In particular,
inner glow may be enabled only when its effective range is at least 10. A
disabled range from 0 through 9 remains an exact writable compatibility value;
an enabled low-range candidate returns `InvalidSnapshot` without a revision or
write. Existing structurally valid low-range state remains readable so it can
be repaired by disabling glow or raising the range.
Before persistence, it resolves shared-visual authority from the current
verified Config1 projection, then the retained last verified projection, then
the current desired resolved values. It preserves synchronized border and
spacing values unless the resolved group policy explicitly permits an override,
and always preserves the exact unique final protected maximized-window rule.
Failure returns `ControlledByHyprShelld` before desired state is replaced.

The durable desired snapshot and recovery transaction records live below
`$XDG_STATE_HOME/hyprshelld/compositor`; they are service authority, not Lua
source. The generated tree and the user-owned customization file remain in the
Hyprland configuration root.

`ManagementState` is `unmanaged`, `managed`, `preview`, or `conflict`.
`preview` is reserved for a receipt-bound transient display target; the other
states are derived from the exact committed ownership record and entrypoint
digest, never from a comment in a Lua file. The stable entrypoint is
`$XDG_CONFIG_HOME/hypr/hyprland.lua`; generated immutable trees live below
`$XDG_CONFIG_HOME/hypr/hyprshelld`. Compositord watches the config root and its
parent for entrypoint creation, deletion, replacement, and path invalidation,
and it independently re-probes before every live activation, adoption, or
recovery operation. A `managed` observation therefore requires the ownership
record, current regular-file digest, verified generation, and authority
`GenerationDigest` to agree; any mismatch is `conflict`. Startup never claims
an existing entrypoint.
`AdoptManagedConfiguration` is the only ownership transition. The caller must
bind the exact bytes of an existing regular file, or assert actual absence with
an empty digest. Unsafe, unreadable, non-regular, or concurrently changed paths
fail closed. Before staging, adoption requires a currently verified, available
Config1 shared-visual authority. It returns `ControlledByHyprShelld` when that
authority is unavailable or unverified, even when the last verified border or
spacing policy was an override. A current group synchronization override
permits divergent desired values for that group. Synchronized-value divergence
or a missing or altered exact protected maximized-window rule also returns
`ControlledByHyprShelld`. It returns before any generation is prepared or
entrypoint is adopted. Adoption also verifies managed cross-field safety
immediately before staging; an unsafe desired target returns
`VerificationFailed` before a generation or entrypoint change. Before changing the
stable path, compositord durably preserves a
recoverable original, writes a live-activation journal that binds the prior and
target entrypoints, and stages and verifies the entire managed generation. It
then publishes a regular managed entrypoint with an atomic no-replace or
exchange operation and synchronizes the config directory. Ownership is not
finalized until live proof and the authority commit both succeed.
`user-custom.lua` remains outside generated generations and is created only
when absent.

The public `org.hyprshelld.Compositor1` D-Bus name is the production singleton:
only after acquiring it does the authority take its store lease and duplicate
retained descriptors for the state, config, managed, and generation roots into
the publisher. Publisher operations remain descriptor-relative and revalidate
that every canonical path still names the retained inode before and after each
transition phase. If a canonical tree is renamed and recreated, the replacement
tree is never selected for mutation and the old owner fails closed when it
detects the mismatch. This does not claim atomic prevention of a hostile
same-UID rename in the instant between a name check and an fd-relative syscall;
that actor is inside the local-user trust boundary, and a detected post-phase
mismatch remains an explicit conflict with its recovery journal retained.

`Apply` compares the authority ID, both catalog digests, and revision before
rendering. It then requires a currently verified, available Config1
shared-visual authority.
It returns `ControlledByHyprShelld` when that authority is unavailable or
unverified, even when the last verified border or spacing policy was an
override. A current group synchronization override permits divergent saved
values for that group. Synchronized-value divergence or a missing or altered
exact protected maximized-window rule also returns
`ControlledByHyprShelld`. It returns before any generation is prepared or
activated. Apply
verifies every generated file and manifest, then asks the activation executor
to satisfy the strongest required mode across the snapshot: `reload`,
`restart`, or `session`. The installed live executor confirms only `reload`;
`restart` and `session` return
`ActivationRequired` before the stable entrypoint is changed. They remain
visible through `RequiredActivation` and are never weakened to a reload. Only a
confirmed activation is committed as `AppliedRevision` and `GenerationDigest`;
abort retains the prior managed entrypoint, generation, last-good snapshot, and
applied tuple. Desired state remains saved, and `RequiredActivation` reports
`none`, `reload`, `restart`, or `session` for the pending difference. Enabled
broker-dependent bindings and UWSM environment changes remain fail-closed in
this slice rather than being rendered as invented shell commands. Any generic
`bindings` or `submaps` collection difference is Restart-required in both
directions. Exact unchanged collections do not elevate another delta, and
Recovery can remain Reload only when both collections exactly match the applied
baseline. This classification adds no shortcut editor, broker method, restart
executor, or live action.

Apply also verifies managed cross-field safety before staging. The check
includes an explicit Apply of an already-current generation, which otherwise
uses an idempotent shortcut. An unsafe target returns `VerificationFailed`
without rendering, publishing a generation, writing a pending transaction, or
changing the applied tuple.

Reload activation is bound to one exact running Hyprland instance. Compositord
resolves the current instance signature before each prepare from the bounded
systemd user-manager environment lookup (with its startup environment only as
an unavailable-provider fallback), then pins that signature for the whole
proof. It validates the runtime tree, lock PID, socket peer credentials, Lua
config provider, compatible `0.56.x` runtime, default config path, and absence
of `--safe-mode`; it never enumerates instances or selects the newest one.
Before publishing a target it connects to socket2, reloads the unchanged
baseline, observes the exact
`configreloaded` boundary, and requires an empty JSON `configerrors` array. This
both proves that the event connection is accepted and establishes a clean
rollback baseline without evaluating Lua or changing the entrypoint.

After atomic publication, an ordinary managed update reloads Hyprland and must
observe, in order, the generated exact
`custom>>hyprshelld:<activation-nonce>` proof and the following
`configreloaded` event. Adoption uses Hyprland's full config reset so a newly
created Lua entrypoint can replace a previously selected legacy provider. In
both cases the reload reply must succeed, the exact process identity and target
entrypoint must remain unchanged, and a subsequent strict JSON `configerrors`
query must be an empty array. A successful reload reply without the exact nonce,
or a nonce followed by any config error, is not convergence.

Rollback restores the exact journaled prior entrypoint before it aborts the
authority transaction. A prior managed generation must emit its own exact
nonce and finish with empty config errors. An unmanaged rollback instead must
restore the exact prior bytes or absence, reselect the exact journaled baseline
config provider, emit the generic reload boundary, and finish with empty config
errors. Errors from the failed candidate are allowed while rollback is being
prepared; the empty-error requirement applies again after the prior baseline
has been restored.

If the `committing` marker definitely was not published, compositord rolls live
activation back before aborting the prepared transaction. Once its atomic
publication may be visible, compositord never guesses that it is absent: a
parent-directory sync failure remains unavailable/conflict for startup
reconciliation, just like a fully durable one-way marker. This prevents a
rollback from contradicting a commit record that may survive a crash.

The durable live-activation journal bridges entrypoint publication and the
authority transaction across service crashes. On startup, a journal whose
target differs from the authority's applied generation is rolled back and
proved before cleanup. A journal whose target equals the applied generation is
never rolled back; compositord requires the exact target entrypoint, finalizes
ownership durably, and then removes the journal. If the stable path matches
neither journaled side, or ownership finalization fails after an authority
commit, the service exposes unavailable/failed authority with management
`conflict` and keeps the journal for deterministic reconciliation.

`Recover` is an explicit rollback-as-new-state operation. At the caller's
current revision it copies the last successfully applied content into exactly
the next desired revision, prepares a new immutable generation, and activates
that exact content. It never decrements or reuses a revision, never adopts an
unmanaged entrypoint, and never silently discards current bytes. Recover is
intentionally exempt from the shared-visual ownership gate so it can restore
the whole-compositor last-known-good state. Normal reconciliation then reasserts
the current shared-border and shared-spacing policies and the exact protected
maximized-window rule.
The recovered last-good content must also satisfy current managed cross-field
safety. An unsafe recovery target returns `VerificationFailed` before a new
generation or pending transaction is created. Safety is evaluated against the
recovery target rather than current desired state, so a safe last-good snapshot
can still repair unsafe desired state.
Startup recovery only reconciles an interrupted transaction; it does not invoke
this public rollback operation.

The installed backend performs the bounded live Reload protocol above for
`Apply`, `Recover`, and `AdoptManagedConfiguration`. It never claims a stronger
transition: snapshots requiring compositor restart or a new session remain
saved but return `ActivationRequired` without changing the stable entrypoint or
applied tuple.

Display discovery and preview use a narrower live contract.
`GetConnectedDisplays` opens a fresh authenticated session and returns filtered,
canonical topology JSON rather than forwarding Hyprland's reply. A display
profile is bound to that topology digest, covers every connected connector
exactly once, and leaves at least one enabled non-mirrored output. Compositord
merges those exact connector records after untouched offline and
description-selected desired records, so disconnected profiles are preserved
and the live connector rules retain Hyprland's winning order.
Hyprland 0.56 does not expose a reliable physical-versus-virtual classifier in
this IPC reply, so the guarantee is deliberately an enabled connected output,
not a claim that the output is a physical panel.
During `awaiting-confirmation`, discovery returns the cached exact post-proof
topology and its original observation time instead of opening another blocking
runtime query; failed or reverting reconciliation is unavailable.

`GetConnectedInputDevices` is a separate authenticated read-only authority.
It performs a fresh bounded `j/devices` query under the configured reviewed
Hyprland 0.56.x policy and returns only a canonical v1 inventory plus its
observation time. The public records contain the session-assigned selector,
coarse observed kind, and nullable reported active-keymap text; pointer
addresses and other raw runtime fields never cross D-Bus. The opaque inventory
digest is bound privately to the authenticated runtime identity, instance
addresses, and a random compositord-process epoch, and excludes active-keymap
diagnostics. The selector is diagnostic session identity rather than stable
hardware identity.

Input-device discovery does not require an available desired snapshot, managed
entrypoint ownership, activation filesystem binding, or successful activation
finalization. It is rejected before a runtime query while a display
confirmation capability still exists or its durable commit is active. A
terminal failed state without a retained capability does not permanently block
later discovery. No old device inventory is cached or returned as current.

`PreviewDisplayConfiguration` is permitted only from an exact current managed
baseline inside one expected authority and revision. It stages a monitor-only
N+1, activates and proves the realized
topology, but leaves desired, applied, and last-good authority at N. The
complete merged preview target must satisfy managed cross-field safety before
rendering or publication; an unsafe target returns `VerificationFailed` without
creating a confirmation capability. The initiating unique bus owner alone
receives the 128-bit token and may recover an
ambiguous reply through `GetPendingDisplayConfirmation`, which also returns
the authority ID that binds the capability; no readable property exposes the
token. Confirm and Revert compare the expected authority ID and expected
preview revision before using their private capability; explicit shared-border
and shared-spacing retry compare the expected authority ID and current desired
revision. While the server-owned monotonic deadline is active,
`ManagementState` is `preview`, ordinary mutations fail `ConfirmationPending`,
and owner loss, timeout, runtime/topology drift, or explicit Revert restores N
before aborting the prepared transaction. Confirm reopens a fresh authenticated
runtime session and repeats the topology and realization proof. As soon as the
authority commit may exist, the service revokes the rollback capability and
publishes non-actionable `committing`; a finalization failure then remains
unavailable/conflict for startup roll-forward and is never contradicted by a
late timer or Revert.

Shared window-border synchronization consumes Config1 directly. A verified
`SyncHyprlandWindowBorders=true` projection derives only
`general:border_size` (zero when the shell border is disabled, otherwise the
shell width) and `decoration:rounding` from the shared shell border radius.
Compositord clones the canonical desired snapshot, preserves every unrelated
surface, and removes either override when the derived value equals the protected
catalog default. It uses the existing whole-snapshot CAS and applies only an
exact revision that this reconciliation created while the entrypoint is already
managed; it never adopts. A display confirmation defers reconciliation.

`SharedBorderSyncState` is one of `unavailable`, `override`, `pending`, `saved`,
`current`, or `failed`. `SharedBorderSourceRevision` is the exact verified
Config1 revision and is zero while that source is unavailable. Source loss does
not rewrite or roll back the last live compositor configuration. A failed
effective tuple does not poll. It retries only after the effective Config1
synchronization policy or derived compositor border values change, compositor
authority changes, or an explicit `RetrySharedBorderSync` call. A verified
Config1 projection Revision-only change that leaves the policy and values
identical advances `SharedBorderSourceRevision` but does not retry. While
synchronization owns the two values, unrelated whole-snapshot replacements
remain valid only when they preserve both resolved values; disabling
synchronization is the explicit override. `Apply` and
`AdoptManagedConfiguration` require a current verified Config1 projection
before preparation or activation even when the last verified policy was an
override. Once verified, they enforce synchronized-value preservation unless
that current projection explicitly disables synchronization. `Recover` remains
exempt as whole-compositor recovery, after which enabled synchronization
reasserts the current shared border projection as a new normal CAS/apply
operation.

Shared window-spacing synchronization derives only global `general:gaps_in`
and `general:gaps_out`. Inner spacing becomes all four `gaps_in` sides. Outer
spacing becomes `gaps_out` in top, right, bottom, left order with a zero top
side so the Bar reservation is not counted twice. Absent overrides resolve to
the authenticated catalog defaults, and synchronized values equal to those
defaults are elided. `float_gaps`, `gaps_workspaces`, and layout-specific gaps
remain outside this authority.

Compositord also owns exactly one final protected workspace rule with ID
`hyprshelld.internal.shared-spacing.maximized` and selector `f[1]`. Its sole
override is zero `gaps_out`. The ID and selector are admitted only as the exact
protected record, and ordinary global-spacing override does not remove it.
Replace, Apply, and adoption fail closed unless the candidate contains this
exact unique final record. Whole-compositor Recover is the exception and is
followed by normal reassertion.

`SharedSpacingSyncState`, `SharedSpacingSourceRevision`,
`SharedSpacingSyncError`, and `RetrySharedSpacingSync` mirror the border status
vocabulary and lossless source revision. Border and spacing retain independent
status and error attribution, while one coherent Config1 projection lets their
edits coalesce into at most one whole-snapshot CAS and one verified reload. A
revision-only source change advances both source revisions without retrying an
unchanged failed effective tuple.

Both explicit retry methods require and retain the exact current authority ID,
revision, catalog digest, and action-catalog digest. That complete tuple is
rechecked immediately before any Desired write. A token captured before an
authority rotation, catalog change, or intervening desired-state change cannot
re-enter either retry path.

`LoadState` is one of `normal`, `recovered`, `defaulted`, `unsupported`, or
`unavailable`. `ApplyState` is one of `unavailable`, `inactive`, `current`,
`retained`, or `failed`; in-progress staging is never published. An
`AppliedRevision` of zero is interpreted only with `ApplyState` and
`GenerationDigest`, because revision zero may itself be active. Unknown future
values are treated as unsafe.
`EntrypointDigest` is the exact digest of a safely read regular entrypoint and is
empty when absent or unsafe. Config-root changes and every mutating call
re-probe the path, so an empty property value is never treated by itself as
proof of absence.

Compositor errors have these meanings:

- `Unavailable` or `ReadOnly`: no authoritative snapshot or writable leased
  transaction is available;
- `StaleAuthority`, `StaleRevision`, or `StaleCatalogDigest`: an
  authority-epoch or CAS input changed;
- `InvalidSnapshot`, `RevisionExhausted`, or `PersistenceFailed`: desired state
  could not be validated, did not satisfy a managed cross-field safety rule,
  exhausted its monotonic revision, or could not be durably replaced;
- `ControlledByHyprShelld`: `ReplaceSnapshot` could not prove that its
  candidate preserves synchronized shared-border and shared-spacing values and
  the exact protected maximized-window rule, or `Apply` or
  `AdoptManagedConfiguration` lacked a current verified Config1 shared-visual
  projection, found divergent synchronized values, or found the protected rule
  missing or altered; `Recover` is exempt and normal reconciliation reasserts
  the shared policies and protected rule afterward;
- `AdoptionRequired`: the caller tried to apply or recover before explicit
  ownership;
- `EntrypointChanged`: the stable entrypoint no longer matches the committed or
  caller-supplied digest;
- `ActivationRequired`: the executor cannot confirm the snapshot's required
  reload, restart, or session transition;
- `VerificationFailed` or `ReloadFailed`: the requested activation or recovery
  target did not satisfy managed safety, staged bytes failed verification, or
  the live executor did not confirm the exact reload, rollback, or empty-error
  proof;
- `RuntimeUnavailable` or `UnsupportedVersion`: the authenticated compositor
  runtime could not be queried under the reviewed version contract;
- `InvalidDisplayProfile`, `DisplayScopeConflict`, or
  `DisplayTopologyChanged`: a display profile, its exact managed baseline, or
  its fresh connected/realized topology failed the preview contract;
- `ConfirmationPending`, `NoDisplayConfirmation`, `ConfirmationExpired`, or
  `InvalidCaller`: the private display-confirmation capability cannot be used
  for the requested operation; and
- `ApplyFailed`, `RecoveryUnavailable`, or `RecoveryFailed`: the corresponding
  bounded transaction could not complete without weakening its guarantees.

### Restart contract

The dormant Restart ABI authorizes only operation kind `apply` or `recovery`.
`GetRestartPlan` returns canonical plan bytes, plan ID, plan digest, and
disclosure version after binding the client request ID and complete authority,
runtime, preflight, warning, disruption, prior, candidate, and monotonic-deadline
tuple. Plan retrieval is read-only. Restart and repair use one lease-owned
volatile plan cache with at most 64 total unexpired entries, at most 4 MiB of
encoded state, and at most 64 KiB per entry. An unexpired entry is never evicted;
capacity exhaustion returns `PlanCapacity` without a durable write.

`AuthorizeRestart` revalidates and consumes that exact issued entry with durable
acceptance, creates the `prepared-authorized` pending record, and returns its
operation ID and latest canonical active status. Missing, expired, process-lost,
or mismatched plan capability returns `StalePlan` without an effect. For every
request-bearing Restart or repair mutation, durable request-map and tombstone
lookup precedes current-authority CAS. An exact request from an older authority
returns its original operation or result; changed reuse returns
`RequestIdConflict`. There is no public phase-advance, Cancel, or RestartUnit
method.

`GetActiveRestart`, `GetRestartOperation`, and `LookupRestartRequest` expose
durable operation identity without recreating a private receipt. Lookup
`availability` is exactly `active`, `unread-result`, `latest-acknowledged`, or
`tombstone-only`. For `active`, the operation ID and accepted authority tuple
are durable, `payload` is the latest canonical active status, and
`payloadDigest` is its domain-separated status digest. For terminal
availability, `payload` is the exact immutable terminal public status and
`payloadDigest` is its result digest. When `found=false`, every string and byte
array is empty and revision is zero.

`GetRestartResults` returns rows `(sequence, authorityId, revision, operationId,
resultDigest, canonicalStatus)`. `limit` is 1 through 64 and the complete reply
is at most 4 MiB; `hasMore` requires pagination rather than truncation. The
latest acknowledged result is returned explicitly even when no unread result
remains. `AcknowledgeRestartResult` binds the exact authority/revision/operation/
digest tuple and is idempotent for the same tuple. Acknowledgment removes only
the unread slot and retains the explicit latest-acknowledged result.

### Repair-only contract

`GetRepairStatus` remains callable in degraded repair-only mode. It returns
one bounded canonical status document plus `repairId`,
`observedAuthorityKind`, `observedAuthorityId`, `observedRevision`, and
`statusDigest`. The kind is exactly `v1`, `v2`, `absent`, or
`unreadable`. V1 carries its exact revision without an invented authority ID;
V2 carries its exact authority ID and revision; absent and unreadable carry an
empty ID and zero revision. The empty ID never supplies meaning by itself.

`GetExactPriorRestorePlan` has priority whenever the complete previous
authority is provably restorable. Its read-only, strongly warned disclosure
binds the expected repair incident, authority kind, authority ID, revision,
status digest, and client-generated request ID before confirmation. It returns
the plan ID, plan digest, and disclosure version. `RestoreExactPrior`
compares that complete CAS and request tuple again together with the plan tuple,
then permanently tombstones the request before a successful effect can be
repeated. Success reports `restoredAuthorityKind`, `restoredAuthorityId`,
`restoredRevision`, and the already reserved durable `repairResultId` and
`repairResultDigest`; it never starts Hyprland.

`GetResetPlan` is a separate read-only, strongly warned disclosure bound to
the same repair incident, authority-kind tuple, status digest, and
client-generated request ID. It returns a plan ID, plan digest, and disclosure
version only when no complete exact prior is provably restorable.
`ResetAuthorityToDefaults` compares that complete CAS and request tuple again
together with the plan tuple. The request ID appears exactly once and precedes
the plan fields. Reset may rotate the authority only after the complete
descriptor-relative, fsynced archive succeeds. Success creates a random
v2 authority at revision 1, reports `newAuthorityKind`, `newAuthorityId`,
`newRevision`, the durable backup ID, and the already reserved repair-result ID
and digest, claims no Applied or LastGood state, and never starts Hyprland.
Exact-prior availability makes both reset methods fail with
`ExactPriorRequired`.

`LookupRepairRequest` exposes the durable request mapping. Its `availability`
is exactly `active`, `unread-result`, `latest-acknowledged`, or
`tombstone-only`. At durable repair acceptance, `repairResultId` is already
reserved. For `active`, `outcome` is `active`, the authority-kind/ID/revision
tuple is the accepted observed CAS tuple, and `payload` is the latest canonical
active status with its domain-separated `payloadDigest`. This derived active
status is never stored as an immutable result or unread slot. Terminal
availability returns the resulting authority tuple and exact immutable terminal
public result with its result digest. When `found=false`, every string and byte
array is empty and revision is zero.

`GetRepairResults` returns rows `(sequence, repairResultId, repairId, requestId,
outcome, authorityKind, authorityId, revision, resultDigest, canonicalResult)`.
`limit` is 1 through 64 and the complete reply is at most 4 MiB; `hasMore`
requires pagination rather than truncation. The latest acknowledged result is
returned explicitly even when no unread result remains.
`AcknowledgeRepairResult` binds the exact repair/repair-result/authority-kind/
authority-ID/revision/digest tuple and is idempotent for the same tuple.
Acknowledgment removes only the unread slot and retains the explicit latest-
acknowledged result.

Repair methods are served by the same compositord process and store lease. An
empty expected authority ID is accepted only with the explicit `v1`,
`absent`, or `unreadable` kind. V1 still requires its exact revision;
absent and unreadable require revision zero. No second repair daemon or writer
exists.

## Component configuration

ComponentConfig1 is a separate recovery and revision domain owned by configd.
Failure or an unsupported future component snapshot does not unregister core
Config1 or block bar-height reads and writes. Its active file is
`$XDG_CONFIG_HOME/hyprshelld/components.json`; its last-known-good file is
`$XDG_STATE_HOME/hyprshelld/components.last-good.json`.

`Available` means one authoritative desired-state snapshot can be read.
`CatalogAvailable` separately means a complete live ComponentManager1 join is
available for validating writes. Ordinary componentd loss retains the last
authoritative snapshot but makes mutations fail closed. `LoadState` is one of
`normal`, `recovered`, `defaulted`, `unsupported`, or `unavailable`.
When the active snapshot is valid but its recovery file is unreadable or uses
a future format, the active snapshot remains readable while writes are
disabled and the recovery bytes are left untouched. A later successful reload
can restore writability without discarding that last-known-good active state.

`GetSnapshot` returns the complete strict JSON snapshot, its monotonic
configuration revision, and the full catalog digest against which it was last
validated. `ReplaceSnapshot` is the single whole-state compare-and-swap
operation. It checks both the desired-state revision and the live catalog
digest, validates every current component value against manager-owned package
digests and schemas, persists atomically, and only then publishes the new
revision. Caller-supplied digests are equality assertions and never establish
package or schema authority.
Missing packages and package-digest mismatches remain inert, structurally
bounded desired-state records. ReplaceSnapshot cannot edit, delete, advance,
or relocate those dormant records and their instances, except for one explicit
inert adoption of an installed user package: the proposed digest must equal the
exact live catalog entry, its component record must be disabled with no grants,
and every instance and placement must remain unchanged. The normal parser also
validates its new trusted settings schema. Configd may advance a protected
built-in record only through a narrowly pinned, catalog-joined data migration.
Such a migration validates every old instance shape, preserves unrelated state,
increments the configuration revision once, and commits the migrated snapshot
to recovery before active storage. Other unrecognized packages remain dormant
for future package-lifecycle handling.
Dormant numeric values remain canonical JSON numbers and must be finite with a
magnitude no greater than 9007199254740991, so parsing and persistence cannot
silently round an unknown component's retained values.

Component configuration errors have these meanings:

- `Unavailable`: no writable authoritative component state is available;
- `CatalogUnavailable`: live catalog/schema truth is unavailable;
- `StaleRevision`: another desired-state mutation won the comparison;
- `StaleCatalogDigest`: the component catalog changed;
- `InvalidSnapshot`: the proposed complete state violates its structural,
  schema, capability, instance, or placement contract;
- `RevisionExhausted`: the unsigned revision cannot advance; and
- `PersistenceFailed`: active state could not be atomically committed.

## Component runtime

ComponentRuntime1 is the coordinator-owned activation boundary used by
surfaced. Its plan joins one complete ComponentManager1 catalog generation
with one complete ComponentConfig1 desired-state revision. It never publishes
a partially hydrated host plan or a package path, package QML, or an
author-controlled loader target.

`SurfacePlanRevision` is an opaque unsigned equality token for the complete
normalized plan; zero means no authoritative plan has been produced in this
coordinator lifetime. `SurfacePlanDigest` is the full lowercase SHA-256 of the
exact plan bytes and is empty exactly while the revision is zero.
`SurfacePlanState` is one of:

- `hydrating`: no valid complete plan has been accepted yet;
- `authoritative`: the published plan is joined to the current catalog and
  configuration snapshots;
- `retained`: an earlier authoritative plan remains the last-known-good plan
  while one of its authorities is unavailable or malformed; or
- `unavailable`: no authoritative plan can be served.

`GetSurfacePlan` accepts the caller's expected revision and returns the exact
immutable strict-JSON plan bytes plus their digest. A mismatched token fails
with `StaleSurfacePlanRevision`; revision zero or a missing plan fails with
`PlanUnavailable`. Consumers validate the bytes atomically against both the
returned digest and the installed `surface-plan.schema.json` contract.

Before its first authoritative plan, surfaced may render only the compiled
first-run built-in plan. Once any authoritative plan is accepted, including an
authoritative empty plan, surfaced retains that last-known-good authority
through ordinary coordinator loss and never resurrects the compiled fallback.
Retained, unavailable, malformed, and locally stale plans strip every
third-party instance; protected built-ins may remain. An authoritative plan
may request only factories compiled into surfaced's built-in allowlist or the
trusted `declarative-v1` text-pill renderer. Declarative plan entries inline
only coordinator-resolved plain text, an optional plain-text tooltip, and a
bounded maximum width. Desired enablement, component settings, output
matching, and bar placement still come from ComponentConfig1. Version one
keeps declarative placements in the active `main` bar layout; other layouts are
dormant for this runtime.

Surfaced must explicitly authorize the current complete plan before it exposes
any declarative entries. The coordinator writes all renderable declarative
instances to its private runtime-health recovery snapshot in one transaction
before authorization succeeds. The trusted renderer reports success only
after a two-second stabilization window, or reports a bounded render failure.
An eight-second coordinator deadline quarantines the exact component package
digest when activation does not complete. A normal last-screen removal or
graceful surfaced shutdown cancels the whole plan authorization; an interrupted
activation retained across coordinator restart is conservatively quarantined
without claiming which process caused the interruption. The surfaced systemd
restart budget is sized so quarantine and a safe-plan restart occur before its
start limit is exhausted.

`RuntimeHealthRevision` is the comparison token for listing and retrying this
state. Runtime-state rows are sorted by component ID and package digest, and
healthy packages are omitted. A probation row has an empty reason and zero
failures. A quarantined row has one of `incomplete-startup`, `timeout`,
`render-failed`, or `protocol-invalid` and a failure count from 1 through
1,000,000. `RetryComponent` removes only the exact quarantined package digest
at the expected health revision. A new package digest starts clean. Unsafe,
unsupported, exhausted, or unreadable health storage enters third-party safe
mode instead of activating untracked content. The recovery snapshot is the
durable commit point; a failed active-file mirror is repaired from its newer
revision on load.

## Component manager

ComponentManager1 exposes one joined catalog containing protected built-ins and
locally installed per-user packages. `ListComponents` returns the complete
sorted ID set and the catalog digest that owns it. A caller passes that digest
to `GetComponent` so metadata and the trusted settings schema cannot be combined
across catalog generations. For `declarative-v1`, the caller then passes both
that catalog digest and the entry's exact package digest to
`GetDeclarativeRuntime`. The reply contains only compact canonical JSON for the
manager-validated trusted primitive; it never exposes or asks surfaced to open
the author-controlled package entrypoint.
`CatalogDigest` is the exact 64-character lowercase hexadecimal SHA-256 of the
catalog entries sorted by component ID. Each entry is framed as an unsigned
64-bit big-endian ID byte length, the UTF-8 ID bytes, an unsigned 64-bit
big-endian package-digest byte length, and the lowercase hexadecimal package
digest bytes. It is an equality token, not a counter.

Origin and removability are derived from the catalog root rather than manifest
claims. For a built-in, `packageDigest` covers the exact manifest and settings
schema bytes. For a user package, it covers every validated regular package
file, including `integrity.json`, using the same length-framed path-and-content
algorithm.

`BeginPackageInspection` accepts an already-open read-only Unix file
descriptor. componentd copies and hashes those bytes into a private bounded
runtime spool before returning a random sender-bound token; it never reopens a
Settings pathname. A short-lived systemd transient helper parses only ZIP
Store/Deflate entries under strict file, expanded-size, path, type, integrity,
and metadata limits. Its normalized report is delivered only to the caller's
unique bus name through `PackageInspectionFinished`. Tokens expire, cannot be
used by another sender, and are consumed by the first install attempt.

`InstallInspectedPackage` commits only the exact reviewed archive digest into
the exact catalog generation shown during review; a concurrent catalog change
fails closed without consuming the inspection. Selecting another package with the same component ID uses
that operation for an update, reinstall, or downgrade. Installation never
mutates enablement, grants, or placements. A first install is inert; an exact
reinstall may make already-authorized retained state for that identical digest
effective again. Compatible data-only declarative bar widgets can otherwise be
activated later through ComponentConfig1;
reserved QML, process, and desktop-widget runtimes remain explicitly
unsupported and inert. After an update changes a package digest, the old
component settings remain dormant. A separate explicit adoption snapshot may
advance only to the exact live user-package digest; it must force the proposed
record disabled, clear every grant, and provide settings valid under the new
trusted schema. The old dormant record may have been enabled or granted because
its mismatched digest is already unsurfaceable. Configd preserves every
instance and placement, and adoption never activates the update.
`RemovePackage` is digest- and
catalog-guarded, applies only to user
packages without installed dependents, and removes package bytes while configd
retains any settings and placements as dormant recovery data. There is no
remote repository, downloader, updater, signature authority, or package code
execution in this boundary.

The user catalog is bounded to 511 receipts and 512 MiB of expanded active
package data. At startup, each receipt and installed tree is revalidated. An
invalid receipt or tree is excluded from the published catalog without hiding
healthy third-party packages; its dormant configd state is preserved. Selecting
the same reviewed component again repairs corrupt contents or a missing tree
under safe directory ancestry. Unsafe non-regular receipts, symlinked storage
ancestors, and non-directory storage entries remain rejected and require manual
removal. Package transactions
retain only the committed version, and post-ownership recovery removes bounded
staging/trash leftovers and receipt-unreachable versions from interrupted
transactions.

Component manager errors have these meanings:

- `org.hyprshelld.ComponentManager1.Error.StaleCatalogDigest`: the caller's
  digest no longer identifies the active catalog; and
- `org.hyprshelld.ComponentManager1.Error.UnknownComponent`: the requested ID
  is not present in that catalog;
- `org.hyprshelld.ComponentManager1.Error.PackageDigestMismatch`: the requested
  package generation is no longer installed; and
- `org.hyprshelld.ComponentManager1.Error.RuntimeUnavailable`: the component
  has no manager-validated declarative runtime document.

Package lifecycle errors additionally distinguish invalid descriptors,
unavailable, unknown, expired, or wrong-owner inspections, archive and package
digest mismatches, protected IDs or built-in removal, same-version byte
changes, installed dependents, and persistence failures. The exact typed names
are documented beside their methods in
`org.hyprshelld.ComponentManager1.xml`.

## Change publication and versioning

Changing properties are published together through the standard
`org.freedesktop.DBus.Properties.PropertiesChanged` signal after persistence
succeeds. The project interfaces do not redeclare that standard interface or
duplicate it with interface-specific change signals.

The numeric suffix is the contract's major version. Before the first public
release, an accepted design decision may revise version 1 while its source,
generated bindings, tests, and documentation change together; compatibility
with older pre-release builds is not promised. After version 1 ships,
compatible additions may extend it. Removing or renaming members, changing
signatures or accepted input, changing default or reset behavior, or changing
existing error meaning requires a new bus name, object path, interface, and XML
filename with the next major version.
