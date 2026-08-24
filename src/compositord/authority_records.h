#pragma once

#include "activation_requirement.h"
#include "canonical_json.h"

#include <QByteArray>
#include <QByteArrayView>
#include <QString>
#include <QStringView>
#include <QtTypes>

#include <optional>

namespace HyprShelld::Compositor {

inline constexpr quint32 authorityRecordV2FormatVersion = 2;
inline constexpr qsizetype maximumAuthorityRecordV2Bytes = 256;
inline constexpr int maximumAuthorityRecordV2Depth = 1;
inline constexpr qsizetype maximumAuthorityRecordV2Values = 16;

inline constexpr qsizetype maximumAppliedRecordV2Bytes = 1024;
inline constexpr int maximumAppliedRecordV2Depth = 1;
inline constexpr qsizetype maximumAppliedRecordV2Values = 16;

struct AuthorityRecordV2 final {
    QString authorityId;

    friend bool operator==(const AuthorityRecordV2 &, const AuthorityRecordV2 &)
        = default;
};

struct AppliedRecordV2 final {
    QString authorityId;
    quint64 revision = 0;
    QString snapshotDigest;
    QString generation;
    QString activationNonce;
    QString entrypoint = QStringLiteral("hyprland.lua");
    ActivationRequirement requiredActivation = ActivationRequirement::Reload;

    friend bool operator==(const AppliedRecordV2 &, const AppliedRecordV2 &)
        = default;
};

// Accepts only the canonical decimal spelling of an unsigned 64-bit value:
// "0" or a nonzero ASCII digit followed by at most nineteen ASCII digits.
// Signs, whitespace, leading zeroes, aliases, and overflow are rejected.
[[nodiscard]] std::optional<quint64> parseCanonicalUint64(QStringView text);

// Durable Applied records deliberately exclude None. Publishing a generation
// always requires at least Reload; None remains an in-memory converged-view
// state and is never accepted as durable activation evidence.
[[nodiscard]] std::optional<ActivationRequirement>
durableActivationRequirementFromName(QStringView name);

[[nodiscard]] QString durableActivationRequirementName(
    ActivationRequirement requirement
);

[[nodiscard]] CanonicalJson::Result<QByteArray> serializeAuthorityRecordV2(
    const AuthorityRecordV2 &record
);

[[nodiscard]] CanonicalJson::Result<AuthorityRecordV2> parseAuthorityRecordV2(
    QByteArrayView bytes
);

[[nodiscard]] CanonicalJson::Result<QByteArray> serializeAppliedRecordV2(
    const AppliedRecordV2 &record
);

[[nodiscard]] CanonicalJson::Result<AppliedRecordV2> parseAppliedRecordV2(
    QByteArrayView bytes
);

} // namespace HyprShelld::Compositor
