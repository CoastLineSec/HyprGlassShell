#include "json_support.h"

#include "component/strict_json.h"

#include <QJsonArray>
#include <QJsonDocument>

namespace HyprShelld::Hyprland::JsonSupport {
namespace {

[[nodiscard]] QJsonValue recursivelyNormalized(const QJsonValue &value)
{
    if (value.isArray()) {
        QJsonArray result;
        const auto source = value.toArray();
        for (const auto &item : source) {
            result.append(recursivelyNormalized(item));
        }
        return result;
    }
    if (value.isObject()) {
        QJsonObject result;
        const auto source = value.toObject();
        // QJsonObject iteration is key-sorted. Rebuilding recursively also
        // avoids making the digest depend on a caller's insertion history.
        for (auto iterator = source.constBegin(); iterator != source.constEnd();
             ++iterator) {
            result.insert(iterator.key(), recursivelyNormalized(iterator.value()));
        }
        return result;
    }
    return value;
}

} // namespace

ValidationResult<QJsonObject> parseStrictObject(
    const QByteArrayView bytes,
    const qsizetype maximumBytes,
    const int maximumDepth
)
{
    const auto parsed = Components::parseStrictJsonObject(
        bytes,
        {
            .maximumBytes = maximumBytes,
            .maximumDepth = maximumDepth,
        }
    );

    ValidationResult<QJsonObject> result;
    if (parsed.value.has_value()) {
        result.value = *parsed.value;
    }
    result.errors.reserve(parsed.errors.size());
    for (const auto &error : parsed.errors) {
        result.errors.append({
            .path = error.path,
            .code = error.code,
            .message = error.message,
        });
    }
    return result;
}

QByteArray canonicalJson(const QJsonValue &value)
{
    const auto normalized = recursivelyNormalized(value);
    if (normalized.isObject()) {
        return QJsonDocument(normalized.toObject()).toJson(QJsonDocument::Compact);
    }
    if (normalized.isArray()) {
        return QJsonDocument(normalized.toArray()).toJson(QJsonDocument::Compact);
    }
    QJsonArray wrapper;
    wrapper.append(normalized);
    const auto encoded = QJsonDocument(wrapper).toJson(QJsonDocument::Compact);
    return encoded.sliced(1, encoded.size() - 2);
}

} // namespace HyprShelld::Hyprland::JsonSupport
