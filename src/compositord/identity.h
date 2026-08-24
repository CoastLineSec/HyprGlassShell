#pragma once

#include <QString>
#include <QStringView>
#include <QtTypes>

#include <cstddef>
#include <expected>
#include <functional>
#include <span>

namespace HyprShelld::Compositor {

inline constexpr qsizetype canonicalIdentifierByteCount = 16;
inline constexpr qsizetype canonicalIdentifierHexLength = 32;
inline constexpr qsizetype canonicalSha256HexLength = 64;
inline constexpr int identifierMintAttemptLimit = 16;

// All authority, request, operation, plan, repair, repair-result, backup,
// anchor, bootstrap, effect, and display IDs share this grammar. Validation is
// deliberately pure and never mints.
[[nodiscard]] bool isCanonicalIdentifier(QStringView value);

// This is the shared grammar for SHA-256 content/status/result/request/plan/
// qualification digests. Semantic callers remain responsible for deciding
// whether an all-zero digest has meaning in their own record type.
[[nodiscard]] bool isCanonicalSha256Digest(QStringView value);

enum class IdentifierMintError {
    InvalidContext,
    LeaseNotHeld,
    EntropyFailure,
    CandidateLimitReached,
};

using IdentifierMintResult = std::expected<QString, IdentifierMintError>;
using LeaseHeldPredicate = std::function<bool()>;
using IdentifierCollisionPredicate = std::function<bool(QStringView)>;

// Dormant internal primitive: no production caller selects it yet. The
// collision predicate must aggregate every live and permanent ID namespace
// owned by the caller. Any fallible index reads must be completed and
// authenticated before constructing that total predicate. The caller must
// retain the owning lease for the whole call. Entropy comes only from Linux
// getrandom(2).
[[nodiscard]] IdentifierMintResult mintIdentifier(
    const LeaseHeldPredicate &leaseHeld,
    const IdentifierCollisionPredicate &collides
);

// Syscall-shaped injection seam for deterministic qualification tests. It is
// not installed and has no runtime call site. Negative counts report the
// supplied errno value, zero is a fatal unusable entropy read, and positive
// short reads are continued until exactly 16 bytes have been filled.
namespace IdentityTestSupport {

using EntropyReadFunction = std::function<qint64(
    std::span<std::byte> destination,
    int &errorNumber
)>;

[[nodiscard]] IdentifierMintResult mintIdentifierWithEntropy(
    const LeaseHeldPredicate &leaseHeld,
    const IdentifierCollisionPredicate &collides,
    const EntropyReadFunction &readEntropy
);

} // namespace IdentityTestSupport

} // namespace HyprShelld::Compositor
