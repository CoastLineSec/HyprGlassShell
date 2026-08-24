#pragma once

#include <QByteArray>
#include <QByteArrayView>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>
#include <QVector>
#include <QtTypes>

#include <optional>

namespace HyprShelld::Compositor::CanonicalJson {

// RFC 8785 canonical bytes never include transport framing. Durable and wire
// records that require a line terminator opt into exactly one trailing LF; the
// caller can therefore exclude that byte from a digest without guessing.
enum class Framing {
    Bare,
    OneTrailingLineFeed,
};

// RFC 8785 itself preserves valid Unicode exactly. Receipt records add the
// project contract that text is already NFC and contains no Unicode control or
// format-category scalar. The codec rejects rather than normalizes.
enum class TextPolicy {
    Rfc8785,
    ProjectReceipt,
};

struct Limits final {
    qsizetype maximumBytes = 4 * 1024 * 1024;
    int maximumDepth = 64;
    qsizetype maximumValues = 65'536;
};

struct Error final {
    QString code;
    QString path;
    QString message;
    // Best-effort context-local UTF-16 code-unit offset in the decoded input
    // or offending semantic string; -1 means an offset is unavailable.
    qsizetype characterOffset = -1;

    friend bool operator==(const Error &, const Error &) = default;
};

template<typename T>
struct Result final {
    std::optional<T> value;
    QVector<Error> errors;

    [[nodiscard]] explicit operator bool() const
    {
        return value.has_value() && errors.isEmpty();
    }
};

// Serializes a typed JSON value with the RFC 8785 JSON Canonicalization Scheme.
// Objects are ordered by UTF-16 code units and arrays retain their input order.
// Undefined, invalid Unicode, non-finite numbers, and lossy integer-backed Qt
// values fail without returning partial bytes.
[[nodiscard]] Result<QByteArray> serialize(
    const QJsonValue &value,
    Framing framing = Framing::Bare,
    TextPolicy textPolicy = TextPolicy::Rfc8785,
    Limits limits = {}
);

// Strictly parses an object without losing duplicate-key information, applies
// the selected I-JSON text policy, reserializes it, and accepts it only when the
// complete input is byte-for-byte canonical under the requested framing.
[[nodiscard]] Result<QJsonObject> parseCanonicalObject(
    QByteArrayView bytes,
    Framing framing = Framing::Bare,
    TextPolicy textPolicy = TextPolicy::Rfc8785,
    Limits limits = {}
);

} // namespace HyprShelld::Compositor::CanonicalJson
