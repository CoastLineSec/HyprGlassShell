# Dormant Hyprland configuration authority envelope v2

Version 2 is a packaged dormant candidate that binds the exact patched
Hyprland 0.56.2 typed desired state to a durable authority epoch. Every v2 runtime document
requires `authorityId`: exactly 32 lowercase hexadecimal characters from 128
random bits. The all-zero value is reserved and invalid. The packaged template
intentionally omits that value; only the future lease-owning store may supply
it, and the codec never mints or substitutes an identifier.

The recovered version 1 schemas, defaults, catalog bytes, source manifest,
protected `f[1]` shared-spacing rule, and 1024-user-plus-one workspace bound
remain byte-identical and active. The v2 config and action catalogs both bind
the canonical v2 source-manifest digest. That ledger pins upstream v0.56.2,
the complete v0.56.1-to-v0.56.2 source delta, the Hyprutils dependency inputs,
and the exact private protected-`f[1]` patch, preimage, postimage, and hunk.

These files may be installed as non-authoritative qualification material, but
the production codec, renderer, generation verifier, store, transaction, and
runtime paths continue to select v1. Activation still requires a later atomic
lease-held migration and runtime-selection change; no packaged v2 artifact by
itself enables or authorizes that transition.
