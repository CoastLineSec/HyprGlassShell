#include "identity.h"

#include <array>
#include <cerrno>

#include <sys/random.h>

namespace HyprShelld::Compositor {
namespace {

using CandidateBytes =
    std::array<std::byte, canonicalIdentifierByteCount>;

[[nodiscard]] bool isLowerHex(const QChar character)
{
    return (character >= QLatin1Char('0')
            && character <= QLatin1Char('9'))
        || (character >= QLatin1Char('a')
            && character <= QLatin1Char('f'));
}

[[nodiscard]] bool allZero(const CandidateBytes &bytes)
{
    for (const auto byte : bytes) {
        if (byte != std::byte{0}) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] QString encodeLowerHex(const CandidateBytes &bytes)
{
    static constexpr char digits[] = "0123456789abcdef";
    QString encoded;
    encoded.reserve(canonicalIdentifierHexLength);
    for (const auto byte : bytes) {
        const auto value = std::to_integer<unsigned int>(byte);
        encoded.append(QLatin1Char(digits[(value >> 4U) & 0x0fU]));
        encoded.append(QLatin1Char(digits[value & 0x0fU]));
    }
    return encoded;
}

[[nodiscard]] IdentifierMintResult mintIdentifierImpl(
    const LeaseHeldPredicate &leaseHeld,
    const IdentifierCollisionPredicate &collides,
    const IdentityTestSupport::EntropyReadFunction &readEntropy
)
{
    if (!leaseHeld || !collides || !readEntropy) {
        return std::unexpected(IdentifierMintError::InvalidContext);
    }

    for (int attempt = 0; attempt < identifierMintAttemptLimit; ++attempt) {
        CandidateBytes bytes{};
        qsizetype filled = 0;
        while (filled < canonicalIdentifierByteCount) {
            // A retry after EINTR or a short read is another entropy call, so
            // it receives the same precondition check as the first call.
            if (!leaseHeld()) {
                return std::unexpected(IdentifierMintError::LeaseNotHeld);
            }

            int errorNumber = 0;
            const auto destination = std::span<std::byte>(bytes).subspan(
                static_cast<std::size_t>(filled)
            );
            const auto count = readEntropy(destination, errorNumber);
            if (count < 0) {
                if (errorNumber == EINTR) {
                    continue;
                }
                return std::unexpected(IdentifierMintError::EntropyFailure);
            }
            if (count == 0
                || count > static_cast<qint64>(destination.size())) {
                return std::unexpected(IdentifierMintError::EntropyFailure);
            }
            filled += static_cast<qsizetype>(count);
        }

        // The candidate is not inspected against caller-owned indexes or
        // accepted after a lease has been lost.
        if (!leaseHeld()) {
            return std::unexpected(IdentifierMintError::LeaseNotHeld);
        }

        if (allZero(bytes)) {
            continue;
        }
        const auto candidate = encodeLowerHex(bytes);
        if (!isCanonicalIdentifier(candidate)) {
            continue;
        }
        const auto occupied = collides(candidate);
        // The caller-owned probe may itself observe or cause lease loss. It is
        // never the final acceptance boundary, even when it reports free.
        if (!leaseHeld()) {
            return std::unexpected(IdentifierMintError::LeaseNotHeld);
        }
        if (occupied) {
            continue;
        }
        return candidate;
    }

    return std::unexpected(IdentifierMintError::CandidateLimitReached);
}

[[nodiscard]] qint64 readSystemEntropy(
    const std::span<std::byte> destination,
    int &errorNumber
)
{
    errno = 0;
    const auto count = ::getrandom(
        destination.data(),
        destination.size(),
        0
    );
    errorNumber = count < 0 ? errno : 0;
    return static_cast<qint64>(count);
}

} // namespace

bool isCanonicalIdentifier(const QStringView value)
{
    if (value.size() != canonicalIdentifierHexLength) {
        return false;
    }

    bool hasNonzeroNibble = false;
    for (const auto character : value) {
        if (!isLowerHex(character)) {
            return false;
        }
        hasNonzeroNibble = hasNonzeroNibble
            || character != QLatin1Char('0');
    }
    return hasNonzeroNibble;
}

bool isCanonicalSha256Digest(const QStringView value)
{
    if (value.size() != canonicalSha256HexLength) {
        return false;
    }
    for (const auto character : value) {
        if (!isLowerHex(character)) {
            return false;
        }
    }
    return true;
}

IdentifierMintResult mintIdentifier(
    const LeaseHeldPredicate &leaseHeld,
    const IdentifierCollisionPredicate &collides
)
{
    return mintIdentifierImpl(leaseHeld, collides, readSystemEntropy);
}

namespace IdentityTestSupport {

IdentifierMintResult mintIdentifierWithEntropy(
    const LeaseHeldPredicate &leaseHeld,
    const IdentifierCollisionPredicate &collides,
    const EntropyReadFunction &readEntropy
)
{
    return mintIdentifierImpl(leaseHeld, collides, readEntropy);
}

} // namespace IdentityTestSupport

} // namespace HyprShelld::Compositor
