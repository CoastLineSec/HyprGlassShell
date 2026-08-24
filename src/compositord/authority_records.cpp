#include "authority_records.h"

#include "identity.h"

#include <QJsonObject>
#include <QSet>

#include <limits>
#include <utility>

namespace HyprShelld::Compositor {
namespace {

[[nodiscard]] CanonicalJson::Limits authorityLimits()
{
    return {
        .maximumBytes = maximumAuthorityRecordV2Bytes,
        .maximumDepth = maximumAuthorityRecordV2Depth,
        .maximumValues = maximumAuthorityRecordV2Values,
    };
}

[[nodiscard]] CanonicalJson::Limits appliedLimits()
{
    return {
        .maximumBytes = maximumAppliedRecordV2Bytes,
        .maximumDepth = maximumAppliedRecordV2Depth,
        .maximumValues = maximumAppliedRecordV2Values,
    };
}

[[nodiscard]] CanonicalJson::Error recordError(
    QString code,
    QString path,
    QString message
)
{
    return {
        .code = std::move(code),
        .path = std::move(path),
        .message = std::move(message),
    };
}

template<typename T>
[[nodiscard]] CanonicalJson::Result<T> failure(CanonicalJson::Error error)
{
    CanonicalJson::Result<T> result;
    result.errors.append(std::move(error));
    return result;
}

template<typename T, typename U>
[[nodiscard]] CanonicalJson::Result<T> propagate(
    CanonicalJson::Result<U> source
)
{
    CanonicalJson::Result<T> result;
    result.errors = std::move(source.errors);
    return result;
}

[[nodiscard]] QSet<QString> keysOf(const QJsonObject &object)
{
    QSet<QString> result;
    for (auto iterator = object.constBegin(); iterator != object.constEnd();
         ++iterator) {
        result.insert(iterator.key());
    }
    return result;
}

[[nodiscard]] bool exactFormatVersion(const QJsonValue &value)
{
    return value.isDouble()
        && value.toDouble(-1.0)
            == static_cast<double>(authorityRecordV2FormatVersion);
}

[[nodiscard]] std::optional<CanonicalJson::Error> validateAuthority(
    const AuthorityRecordV2 &record
)
{
    if (!isCanonicalIdentifier(record.authorityId)) {
        return recordError(
            QStringLiteral("authority-record.invalid-authority-id"),
            QStringLiteral("$.authorityId"),
            QStringLiteral(
                "Authority ID must be a nonzero 32-character lowercase hexadecimal value."
            )
        );
    }
    return std::nullopt;
}

[[nodiscard]] std::optional<CanonicalJson::Error> validateApplied(
    const AppliedRecordV2 &record
)
{
    if (!isCanonicalIdentifier(record.authorityId)) {
        return recordError(
            QStringLiteral("applied-record.invalid-authority-id"),
            QStringLiteral("$.authorityId"),
            QStringLiteral(
                "Applied authority ID must be a nonzero 32-character lowercase hexadecimal value."
            )
        );
    }
    if (!isCanonicalSha256Digest(record.snapshotDigest)) {
        return recordError(
            QStringLiteral("applied-record.invalid-snapshot-digest"),
            QStringLiteral("$.snapshotDigest"),
            QStringLiteral("Applied snapshot digest must be lowercase SHA-256.")
        );
    }
    if (!isCanonicalSha256Digest(record.generation)) {
        return recordError(
            QStringLiteral("applied-record.invalid-generation"),
            QStringLiteral("$.generation"),
            QStringLiteral("Applied generation must be lowercase SHA-256.")
        );
    }
    if (!isCanonicalIdentifier(record.activationNonce)) {
        return recordError(
            QStringLiteral("applied-record.invalid-activation-nonce"),
            QStringLiteral("$.activationNonce"),
            QStringLiteral(
                "Activation nonce must be a nonzero 32-character lowercase hexadecimal value."
            )
        );
    }
    if (record.activationNonce == record.authorityId) {
        return recordError(
            QStringLiteral("applied-record.identifier-collision"),
            QStringLiteral("$.activationNonce"),
            QStringLiteral(
                "Activation nonce cannot collide with its authority ID."
            )
        );
    }
    if (record.entrypoint != QStringLiteral("hyprland.lua")) {
        return recordError(
            QStringLiteral("applied-record.invalid-entrypoint"),
            QStringLiteral("$.entrypoint"),
            QStringLiteral(
                "Applied entrypoint must be the exact relative renderer token hyprland.lua."
            )
        );
    }
    if (durableActivationRequirementName(record.requiredActivation).isEmpty()) {
        return recordError(
            QStringLiteral("applied-record.invalid-activation-requirement"),
            QStringLiteral("$.requiredActivation"),
            QStringLiteral(
                "Durable activation must be reload, restart, or session."
            )
        );
    }
    return std::nullopt;
}

} // namespace

std::optional<quint64> parseCanonicalUint64(const QStringView text)
{
    if (text.isEmpty() || text.size() > 20) {
        return std::nullopt;
    }
    if (text.size() > 1 && text.front() == QLatin1Char('0')) {
        return std::nullopt;
    }

    quint64 result = 0;
    for (const auto character : text) {
        if (character < QLatin1Char('0')
            || character > QLatin1Char('9')) {
            return std::nullopt;
        }
        const auto digit = static_cast<quint64>(
            character.unicode() - QLatin1Char('0').unicode()
        );
        if (result
            > (std::numeric_limits<quint64>::max() - digit) / 10U) {
            return std::nullopt;
        }
        result = result * 10U + digit;
    }
    if (QString::number(result) != text) {
        return std::nullopt;
    }
    return result;
}

std::optional<ActivationRequirement> durableActivationRequirementFromName(
    const QStringView name
)
{
    if (name == QStringLiteral("reload")) {
        return ActivationRequirement::Reload;
    }
    if (name == QStringLiteral("restart")) {
        return ActivationRequirement::Restart;
    }
    if (name == QStringLiteral("session")) {
        return ActivationRequirement::Session;
    }
    return std::nullopt;
}

QString durableActivationRequirementName(
    const ActivationRequirement requirement
)
{
    switch (requirement) {
    case ActivationRequirement::Reload:
        return QStringLiteral("reload");
    case ActivationRequirement::Restart:
        return QStringLiteral("restart");
    case ActivationRequirement::Session:
        return QStringLiteral("session");
    case ActivationRequirement::None:
        return {};
    }
    return {};
}

CanonicalJson::Result<QByteArray> serializeAuthorityRecordV2(
    const AuthorityRecordV2 &record
)
{
    if (const auto invalid = validateAuthority(record)) {
        return failure<QByteArray>(*invalid);
    }
    return CanonicalJson::serialize(
        QJsonObject{
            {
                QStringLiteral("formatVersion"),
                static_cast<qint64>(authorityRecordV2FormatVersion),
            },
            {QStringLiteral("authorityId"), record.authorityId},
        },
        CanonicalJson::Framing::OneTrailingLineFeed,
        CanonicalJson::TextPolicy::Rfc8785,
        authorityLimits()
    );
}

CanonicalJson::Result<AuthorityRecordV2> parseAuthorityRecordV2(
    const QByteArrayView bytes
)
{
    auto parsed = CanonicalJson::parseCanonicalObject(
        bytes,
        CanonicalJson::Framing::OneTrailingLineFeed,
        CanonicalJson::TextPolicy::Rfc8785,
        authorityLimits()
    );
    if (!parsed) {
        return propagate<AuthorityRecordV2>(std::move(parsed));
    }

    static const QSet<QString> exactKeys{
        QStringLiteral("formatVersion"),
        QStringLiteral("authorityId"),
    };
    const auto &object = *parsed.value;
    if (keysOf(object) != exactKeys) {
        return failure<AuthorityRecordV2>(recordError(
            QStringLiteral("authority-record.invalid-fields"),
            QStringLiteral("$"),
            QStringLiteral("Authority record fields must be exact.")
        ));
    }
    if (!exactFormatVersion(object.value(QStringLiteral("formatVersion")))) {
        return failure<AuthorityRecordV2>(recordError(
            QStringLiteral("authority-record.invalid-version"),
            QStringLiteral("$.formatVersion"),
            QStringLiteral("Authority record format must be exactly 2.")
        ));
    }
    if (!object.value(QStringLiteral("authorityId")).isString()) {
        return failure<AuthorityRecordV2>(recordError(
            QStringLiteral("authority-record.invalid-authority-id"),
            QStringLiteral("$.authorityId"),
            QStringLiteral("Authority ID must be a string.")
        ));
    }

    AuthorityRecordV2 record{
        .authorityId =
            object.value(QStringLiteral("authorityId")).toString(),
    };
    if (const auto invalid = validateAuthority(record)) {
        return failure<AuthorityRecordV2>(*invalid);
    }
    return {
        .value = std::move(record),
        .errors = {},
    };
}

CanonicalJson::Result<QByteArray> serializeAppliedRecordV2(
    const AppliedRecordV2 &record
)
{
    if (const auto invalid = validateApplied(record)) {
        return failure<QByteArray>(*invalid);
    }
    return CanonicalJson::serialize(
        QJsonObject{
            {
                QStringLiteral("formatVersion"),
                static_cast<qint64>(authorityRecordV2FormatVersion),
            },
            {QStringLiteral("authorityId"), record.authorityId},
            {QStringLiteral("revision"), QString::number(record.revision)},
            {QStringLiteral("snapshotDigest"), record.snapshotDigest},
            {QStringLiteral("generation"), record.generation},
            {QStringLiteral("activationNonce"), record.activationNonce},
            {QStringLiteral("entrypoint"), record.entrypoint},
            {
                QStringLiteral("requiredActivation"),
                durableActivationRequirementName(record.requiredActivation),
            },
        },
        CanonicalJson::Framing::OneTrailingLineFeed,
        CanonicalJson::TextPolicy::Rfc8785,
        appliedLimits()
    );
}

CanonicalJson::Result<AppliedRecordV2> parseAppliedRecordV2(
    const QByteArrayView bytes
)
{
    auto parsed = CanonicalJson::parseCanonicalObject(
        bytes,
        CanonicalJson::Framing::OneTrailingLineFeed,
        CanonicalJson::TextPolicy::Rfc8785,
        appliedLimits()
    );
    if (!parsed) {
        return propagate<AppliedRecordV2>(std::move(parsed));
    }

    static const QSet<QString> exactKeys{
        QStringLiteral("formatVersion"),
        QStringLiteral("authorityId"),
        QStringLiteral("revision"),
        QStringLiteral("snapshotDigest"),
        QStringLiteral("generation"),
        QStringLiteral("activationNonce"),
        QStringLiteral("entrypoint"),
        QStringLiteral("requiredActivation"),
    };
    const auto &object = *parsed.value;
    if (keysOf(object) != exactKeys) {
        return failure<AppliedRecordV2>(recordError(
            QStringLiteral("applied-record.invalid-fields"),
            QStringLiteral("$"),
            QStringLiteral("Applied record fields must be exact.")
        ));
    }
    if (!exactFormatVersion(object.value(QStringLiteral("formatVersion")))) {
        return failure<AppliedRecordV2>(recordError(
            QStringLiteral("applied-record.invalid-version"),
            QStringLiteral("$.formatVersion"),
            QStringLiteral("Applied record format must be exactly 2.")
        ));
    }

    static const QSet<QString> stringKeys{
        QStringLiteral("authorityId"),
        QStringLiteral("revision"),
        QStringLiteral("snapshotDigest"),
        QStringLiteral("generation"),
        QStringLiteral("activationNonce"),
        QStringLiteral("entrypoint"),
        QStringLiteral("requiredActivation"),
    };
    for (const auto &key : stringKeys) {
        if (!object.value(key).isString()) {
            return failure<AppliedRecordV2>(recordError(
                QStringLiteral("applied-record.string-required"),
                QStringLiteral("$.") + key,
                QStringLiteral("Applied scalar fields must be strings.")
            ));
        }
    }

    const auto revisionText =
        object.value(QStringLiteral("revision")).toString();
    const auto revision = parseCanonicalUint64(revisionText);
    if (!revision) {
        return failure<AppliedRecordV2>(recordError(
            QStringLiteral("applied-record.invalid-revision"),
            QStringLiteral("$.revision"),
            QStringLiteral("Applied revision must be canonical uint64 text.")
        ));
    }
    const auto requirement = durableActivationRequirementFromName(
        object.value(QStringLiteral("requiredActivation")).toString()
    );
    if (!requirement) {
        return failure<AppliedRecordV2>(recordError(
            QStringLiteral("applied-record.invalid-activation-requirement"),
            QStringLiteral("$.requiredActivation"),
            QStringLiteral(
                "Durable activation must be reload, restart, or session."
            )
        ));
    }

    AppliedRecordV2 record{
        .authorityId =
            object.value(QStringLiteral("authorityId")).toString(),
        .revision = *revision,
        .snapshotDigest =
            object.value(QStringLiteral("snapshotDigest")).toString(),
        .generation =
            object.value(QStringLiteral("generation")).toString(),
        .activationNonce =
            object.value(QStringLiteral("activationNonce")).toString(),
        .entrypoint = object.value(QStringLiteral("entrypoint")).toString(),
        .requiredActivation = *requirement,
    };
    if (const auto invalid = validateApplied(record)) {
        return failure<AppliedRecordV2>(*invalid);
    }
    return {
        .value = std::move(record),
        .errors = {},
    };
}

} // namespace HyprShelld::Compositor
