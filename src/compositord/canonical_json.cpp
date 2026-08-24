#include "canonical_json.h"

#include <double-conversion/double-conversion.h>

#include <QJsonArray>
#include <QMetaType>
#include <QSet>
#include <QStringDecoder>
#include <QVariant>

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <limits>
#include <utility>

namespace HyprShelld::Compositor::CanonicalJson {
namespace {

[[nodiscard]] Error makeError(
    QString code,
    QString path,
    QString message,
    const qsizetype characterOffset = -1
)
{
    return {
        .code = std::move(code),
        .path = std::move(path),
        .message = std::move(message),
        .characterOffset = characterOffset,
    };
}

[[nodiscard]] std::optional<Error> limitsError(const Limits &limits)
{
    if (limits.maximumBytes < 0 || limits.maximumDepth < 0
        || limits.maximumValues < 0) {
        return makeError(
            QStringLiteral("jcs.invalid-limits"),
            QStringLiteral("$"),
            QStringLiteral("Canonical JSON limits cannot be negative.")
        );
    }
    return std::nullopt;
}

[[nodiscard]] bool validFraming(const Framing framing)
{
    return framing == Framing::Bare
        || framing == Framing::OneTrailingLineFeed;
}

[[nodiscard]] bool validTextPolicy(const TextPolicy policy)
{
    return policy == TextPolicy::Rfc8785
        || policy == TextPolicy::ProjectReceipt;
}

[[nodiscard]] QString childPath(const QString &parent, const QString &member)
{
    return parent + QLatin1Char('.') + member;
}

[[nodiscard]] bool utf16Less(const QString &left, const QString &right)
{
    const auto common = std::min(left.size(), right.size());
    for (qsizetype index = 0; index < common; ++index) {
        const auto leftUnit = left.at(index).unicode();
        const auto rightUnit = right.at(index).unicode();
        if (leftUnit != rightUnit) {
            return leftUnit < rightUnit;
        }
    }
    return left.size() < right.size();
}

[[nodiscard]] bool validateText(
    const QStringView text,
    const TextPolicy policy,
    const QString &path,
    Error &error
)
{
    if (!validTextPolicy(policy)) {
        error = makeError(
            QStringLiteral("jcs.unknown-text-policy"),
            path,
            QStringLiteral("The canonical JSON text policy is unknown.")
        );
        return false;
    }

    for (qsizetype index = 0; index < text.size(); ++index) {
        const auto unit = text.at(index);
        char32_t scalar = unit.unicode();
        if (unit.isHighSurrogate()) {
            if (index + 1 >= text.size()
                || !text.at(index + 1).isLowSurrogate()) {
                error = makeError(
                    QStringLiteral("jcs.invalid-unicode"),
                    path,
                    QStringLiteral("A JSON string contains a lone high surrogate."),
                    index
                );
                return false;
            }
            scalar = QChar::surrogateToUcs4(unit, text.at(index + 1));
            ++index;
        } else if (unit.isLowSurrogate()) {
            error = makeError(
                QStringLiteral("jcs.invalid-unicode"),
                path,
                QStringLiteral("A JSON string contains a lone low surrogate."),
                index
            );
            return false;
        }

        if (policy == TextPolicy::ProjectReceipt) {
            const auto category = QChar::category(scalar);
            if (category == QChar::Other_Control
                || category == QChar::Other_Format) {
                error = makeError(
                    QStringLiteral("jcs.forbidden-control"),
                    path,
                    QStringLiteral(
                        "Project receipt text cannot contain Unicode control or format characters."
                    ),
                    index
                );
                return false;
            }
        }
    }

    if (policy == TextPolicy::ProjectReceipt
        && text.toString().normalized(QString::NormalizationForm_C)
            != text) {
        error = makeError(
            QStringLiteral("jcs.non-nfc-text"),
            path,
            QStringLiteral("Project receipt text must already be NFC."),
            -1
        );
        return false;
    }
    return true;
}

[[nodiscard]] quint64 unsignedMagnitude(const qint64 value)
{
    if (value >= 0) {
        return static_cast<quint64>(value);
    }
    return static_cast<quint64>(-(value + 1)) + 1;
}

[[nodiscard]] bool exactlyRepresentableAsBinary64(const quint64 magnitude)
{
    if (magnitude == 0) {
        return true;
    }
    const auto width = std::bit_width(magnitude);
    if (width <= 53) {
        return true;
    }
    const auto discardedBits = width - 53;
    const auto mask = (quint64{1} << discardedBits) - 1;
    return (magnitude & mask) == 0;
}

[[nodiscard]] std::optional<double> jsonNumber(
    const QJsonValue &value,
    const QString &path,
    Error &error
)
{
    const auto variant = value.toVariant();
    const auto type = variant.metaType().id();
    if (type == QMetaType::LongLong || type == QMetaType::Long
        || type == QMetaType::Int || type == QMetaType::Short
        || type == QMetaType::SChar) {
        const auto integer = variant.toLongLong();
        if (!exactlyRepresentableAsBinary64(unsignedMagnitude(integer))) {
            error = makeError(
                QStringLiteral("jcs.lossy-integer"),
                path,
                QStringLiteral(
                    "The integer-backed JSON value is not exactly representable as IEEE-754 binary64."
                )
            );
            return std::nullopt;
        }
        return static_cast<double>(integer);
    }
    if (type == QMetaType::ULongLong || type == QMetaType::ULong
        || type == QMetaType::UInt || type == QMetaType::UShort
        || type == QMetaType::UChar) {
        const auto integer = variant.toULongLong();
        if (!exactlyRepresentableAsBinary64(integer)) {
            error = makeError(
                QStringLiteral("jcs.lossy-integer"),
                path,
                QStringLiteral(
                    "The integer-backed JSON value is not exactly representable as IEEE-754 binary64."
                )
            );
            return std::nullopt;
        }
        return static_cast<double>(integer);
    }

    const auto number = value.toDouble(
        std::numeric_limits<double>::quiet_NaN()
    );
    if (!std::isfinite(number)) {
        error = makeError(
            QStringLiteral("jcs.non-finite-number"),
            path,
            QStringLiteral("RFC 8785 cannot serialize NaN or infinity.")
        );
        return std::nullopt;
    }
    return number;
}

[[nodiscard]] std::optional<QByteArray> ecmaScriptNumber(
    const double number
)
{
    if (!std::isfinite(number)) {
        return std::nullopt;
    }
    std::array<
        char,
        double_conversion::DoubleToStringConverter::
            kMaxCharsEcmaScriptShortest
            + 1
    > buffer{};
    double_conversion::StringBuilder builder(
        buffer.data(), static_cast<int>(buffer.size())
    );
    const auto converted = double_conversion::DoubleToStringConverter::
        EcmaScriptConverter()
            .ToShortest(number, &builder);
    if (!converted) {
        return std::nullopt;
    }
    const auto length = builder.position();
    builder.Finalize();
    return QByteArray(buffer.data(), length);
}

class Serializer final {
public:
    Serializer(
        const Framing framing,
        const TextPolicy textPolicy,
        const Limits limits
    )
        : framing_(framing)
        , textPolicy_(textPolicy)
        , limits_(limits)
    {
    }

    [[nodiscard]] Result<QByteArray> run(const QJsonValue &value)
    {
        Result<QByteArray> result;
        if (const auto invalid = limitsError(limits_)) {
            result.errors.append(*invalid);
            return result;
        }
        if (!validFraming(framing_)) {
            result.errors.append(makeError(
                QStringLiteral("jcs.unknown-framing"),
                QStringLiteral("$"),
                QStringLiteral("The canonical JSON framing is unknown.")
            ));
            return result;
        }
        if (!validTextPolicy(textPolicy_)) {
            result.errors.append(makeError(
                QStringLiteral("jcs.unknown-text-policy"),
                QStringLiteral("$"),
                QStringLiteral("The canonical JSON text policy is unknown.")
            ));
            return result;
        }
        if (!writeValue(value, QStringLiteral("$"), 1)) {
            result.errors.append(error_);
            return result;
        }
        if (framing_ == Framing::OneTrailingLineFeed
            && !append(QByteArrayView("\n", 1), QStringLiteral("$"))) {
            result.errors.append(error_);
            return result;
        }
        result.value = std::move(output_);
        return result;
    }

private:
    [[nodiscard]] bool append(
        const QByteArrayView bytes,
        const QString &path
    )
    {
        if (bytes.size() > limits_.maximumBytes - output_.size()) {
            error_ = makeError(
                QStringLiteral("jcs.output-limit"),
                path,
                QStringLiteral("Canonical JSON exceeds the configured byte limit.")
            );
            return false;
        }
        output_.append(bytes.data(), bytes.size());
        return true;
    }

    [[nodiscard]] bool countValue(const QString &path)
    {
        if (valueCount_ >= limits_.maximumValues) {
            error_ = makeError(
                QStringLiteral("jcs.value-limit"),
                path,
                QStringLiteral("Canonical JSON exceeds the configured value limit.")
            );
            return false;
        }
        ++valueCount_;
        return true;
    }

    [[nodiscard]] bool writeValue(
        const QJsonValue &value,
        const QString &path,
        const int depth
    )
    {
        if (!countValue(path)) {
            return false;
        }
        switch (value.type()) {
        case QJsonValue::Null:
            return append(QByteArrayView("null", 4), path);
        case QJsonValue::Bool:
            return append(
                value.toBool() ? QByteArrayView("true", 4)
                               : QByteArrayView("false", 5),
                path
            );
        case QJsonValue::Double:
            return writeNumber(value, path);
        case QJsonValue::String:
            return writeString(value.toString(), path);
        case QJsonValue::Array:
            return writeArray(value.toArray(), path, depth);
        case QJsonValue::Object:
            return writeObject(value.toObject(), path, depth);
        case QJsonValue::Undefined:
            error_ = makeError(
                QStringLiteral("jcs.undefined-value"),
                path,
                QStringLiteral("Undefined is not an I-JSON value.")
            );
            return false;
        }
        error_ = makeError(
            QStringLiteral("jcs.unknown-value"),
            path,
            QStringLiteral("The typed JSON value is unknown.")
        );
        return false;
    }

    [[nodiscard]] bool writeNumber(
        const QJsonValue &value,
        const QString &path
    )
    {
        Error numberError;
        const auto number = jsonNumber(value, path, numberError);
        if (!number) {
            error_ = std::move(numberError);
            return false;
        }
        const auto encoded = ecmaScriptNumber(*number);
        if (!encoded) {
            error_ = makeError(
                QStringLiteral("jcs.number-conversion-failed"),
                path,
                QStringLiteral("The finite binary64 value could not be serialized.")
            );
            return false;
        }
        return append(*encoded, path);
    }

    [[nodiscard]] bool writeString(
        const QStringView text,
        const QString &path
    )
    {
        Error textError;
        if (!validateText(text, textPolicy_, path, textError)) {
            error_ = std::move(textError);
            return false;
        }
        if (!append(QByteArrayView("\"", 1), path)) {
            return false;
        }

        static constexpr char hexadecimal[] = "0123456789abcdef";
        for (qsizetype index = 0; index < text.size(); ++index) {
            const auto unit = text.at(index);
            QByteArray encoded;
            switch (unit.unicode()) {
            case '\b': encoded = QByteArrayLiteral("\\b"); break;
            case '\t': encoded = QByteArrayLiteral("\\t"); break;
            case '\n': encoded = QByteArrayLiteral("\\n"); break;
            case '\f': encoded = QByteArrayLiteral("\\f"); break;
            case '\r': encoded = QByteArrayLiteral("\\r"); break;
            case '"': encoded = QByteArrayLiteral("\\\""); break;
            case '\\': encoded = QByteArrayLiteral("\\\\"); break;
            default:
                if (unit.unicode() < 0x20) {
                    encoded = QByteArrayLiteral("\\u00");
                    encoded.append(hexadecimal[(unit.unicode() >> 4) & 0xf]);
                    encoded.append(hexadecimal[unit.unicode() & 0xf]);
                } else if (unit.isHighSurrogate()) {
                    QString pair;
                    pair.reserve(2);
                    pair.append(unit);
                    pair.append(text.at(++index));
                    encoded = pair.toUtf8();
                } else {
                    encoded = QString(unit).toUtf8();
                }
                break;
            }
            if (!append(encoded, path)) {
                return false;
            }
        }
        return append(QByteArrayView("\"", 1), path);
    }

    [[nodiscard]] bool writeArray(
        const QJsonArray &array,
        const QString &path,
        const int depth
    )
    {
        if (depth > limits_.maximumDepth) {
            error_ = makeError(
                QStringLiteral("jcs.depth-limit"),
                path,
                QStringLiteral("Canonical JSON exceeds the configured depth limit.")
            );
            return false;
        }
        if (!append(QByteArrayView("[", 1), path)) {
            return false;
        }
        for (qsizetype index = 0; index < array.size(); ++index) {
            if (index > 0 && !append(QByteArrayView(",", 1), path)) {
                return false;
            }
            const auto elementPath = path + QLatin1Char('[')
                + QString::number(index) + QLatin1Char(']');
            if (!writeValue(array.at(index), elementPath, depth + 1)) {
                return false;
            }
        }
        return append(QByteArrayView("]", 1), path);
    }

    [[nodiscard]] bool writeObject(
        const QJsonObject &object,
        const QString &path,
        const int depth
    )
    {
        if (depth > limits_.maximumDepth) {
            error_ = makeError(
                QStringLiteral("jcs.depth-limit"),
                path,
                QStringLiteral("Canonical JSON exceeds the configured depth limit.")
            );
            return false;
        }
        if (!append(QByteArrayView("{", 1), path)) {
            return false;
        }

        auto keys = object.keys();
        std::sort(keys.begin(), keys.end(), utf16Less);
        for (qsizetype index = 0; index < keys.size(); ++index) {
            const auto &key = keys.at(index);
            const auto memberPath = childPath(path, key);
            if (index > 0 && !append(QByteArrayView(",", 1), path)) {
                return false;
            }
            if (!writeString(key, memberPath)
                || !append(QByteArrayView(":", 1), memberPath)
                || !writeValue(object.value(key), memberPath, depth + 1)) {
                return false;
            }
        }
        return append(QByteArrayView("}", 1), path);
    }

    Framing framing_;
    TextPolicy textPolicy_;
    Limits limits_;
    QByteArray output_;
    qsizetype valueCount_ = 0;
    Error error_;
};

class Reader final {
public:
    Reader(QString text, const TextPolicy textPolicy, const Limits limits)
        : text_(std::move(text))
        , textPolicy_(textPolicy)
        , limits_(limits)
    {
    }

    [[nodiscard]] Result<QJsonObject> run()
    {
        Result<QJsonObject> result;
        skipWhitespace();
        if (atEnd() || current() != QLatin1Char('{')) {
            fail(
                QStringLiteral("jcs.root-object-required"),
                QStringLiteral("$"),
                QStringLiteral("A canonical authority record must have an object root.")
            );
        } else {
            if (!countValue(QStringLiteral("$"))) {
                result.errors.append(error_);
                return result;
            }
            const auto object = readObject(QStringLiteral("$"), 1);
            skipWhitespace();
            if (!failed_ && !atEnd()) {
                fail(
                    QStringLiteral("jcs.trailing-data"),
                    QStringLiteral("$"),
                    QStringLiteral("Unexpected data follows the JSON object.")
                );
            }
            if (!failed_) {
                result.value = object;
            }
        }
        if (failed_) {
            result.errors.append(error_);
        }
        return result;
    }

private:
    [[nodiscard]] bool atEnd() const
    {
        return position_ >= text_.size();
    }

    [[nodiscard]] QChar current() const
    {
        return atEnd() ? QChar{} : text_.at(position_);
    }

    void skipWhitespace()
    {
        while (!atEnd()) {
            const auto unit = current();
            if (unit != QLatin1Char(' ') && unit != QLatin1Char('\t')
                && unit != QLatin1Char('\r') && unit != QLatin1Char('\n')) {
                return;
            }
            ++position_;
        }
    }

    void fail(QString code, QString path, QString message)
    {
        if (failed_) {
            return;
        }
        failed_ = true;
        error_ = makeError(
            std::move(code), std::move(path), std::move(message), position_
        );
    }

    [[nodiscard]] bool countValue(const QString &path)
    {
        if (valueCount_ >= limits_.maximumValues) {
            fail(
                QStringLiteral("jcs.value-limit"),
                path,
                QStringLiteral("JSON exceeds the configured value limit.")
            );
            return false;
        }
        ++valueCount_;
        return true;
    }

    [[nodiscard]] QJsonValue readValue(const QString &path, const int depth)
    {
        if (!countValue(path)) {
            return {};
        }
        skipWhitespace();
        if (atEnd()) {
            fail(
                QStringLiteral("jcs.unexpected-end"),
                path,
                QStringLiteral("The JSON value is incomplete.")
            );
            return {};
        }
        switch (current().unicode()) {
        case '{': return readObject(path, depth);
        case '[': return readArray(path, depth);
        case '"': {
            const auto string = readString(path);
            return string ? QJsonValue(*string) : QJsonValue{};
        }
        case 't': return readLiteral(path, QStringLiteral("true"), true);
        case 'f': return readLiteral(path, QStringLiteral("false"), false);
        case 'n': return readNull(path);
        default:
            if (current() == QLatin1Char('-')
                || (current() >= QLatin1Char('0')
                    && current() <= QLatin1Char('9'))) {
                return readNumber(path);
            }
            fail(
                QStringLiteral("jcs.invalid-token"),
                path,
                QStringLiteral("Expected a JSON value.")
            );
            return {};
        }
    }

    [[nodiscard]] QJsonObject readObject(
        const QString &path,
        const int depth
    )
    {
        QJsonObject object;
        if (depth > limits_.maximumDepth) {
            fail(
                QStringLiteral("jcs.depth-limit"),
                path,
                QStringLiteral("JSON exceeds the configured depth limit.")
            );
            return object;
        }
        ++position_; // {
        skipWhitespace();
        if (!atEnd() && current() == QLatin1Char('}')) {
            ++position_;
            return object;
        }

        QSet<QString> keys;
        while (!failed_) {
            skipWhitespace();
            if (atEnd() || current() != QLatin1Char('"')) {
                fail(
                    QStringLiteral("jcs.object-key-required"),
                    path,
                    QStringLiteral("Expected a quoted object member name.")
                );
                return object;
            }
            const auto key = readString(path);
            if (!key) {
                return object;
            }
            const auto memberPath = childPath(path, *key);
            if (keys.contains(*key)) {
                fail(
                    QStringLiteral("jcs.duplicate-key"),
                    memberPath,
                    QStringLiteral("Duplicate JSON object member name.")
                );
                return object;
            }
            keys.insert(*key);

            skipWhitespace();
            if (atEnd() || current() != QLatin1Char(':')) {
                fail(
                    QStringLiteral("jcs.colon-required"),
                    memberPath,
                    QStringLiteral("Expected ':' after the object member name.")
                );
                return object;
            }
            ++position_;
            object.insert(*key, readValue(memberPath, depth + 1));
            if (failed_) {
                return object;
            }

            skipWhitespace();
            if (!atEnd() && current() == QLatin1Char('}')) {
                ++position_;
                return object;
            }
            if (atEnd() || current() != QLatin1Char(',')) {
                fail(
                    QStringLiteral("jcs.object-separator-required"),
                    path,
                    QStringLiteral("Expected ',' or '}' in the object.")
                );
                return object;
            }
            ++position_;
        }
        return object;
    }

    [[nodiscard]] QJsonArray readArray(
        const QString &path,
        const int depth
    )
    {
        QJsonArray array;
        if (depth > limits_.maximumDepth) {
            fail(
                QStringLiteral("jcs.depth-limit"),
                path,
                QStringLiteral("JSON exceeds the configured depth limit.")
            );
            return array;
        }
        ++position_; // [
        skipWhitespace();
        if (!atEnd() && current() == QLatin1Char(']')) {
            ++position_;
            return array;
        }

        qsizetype index = 0;
        while (!failed_) {
            const auto elementPath = path + QLatin1Char('[')
                + QString::number(index) + QLatin1Char(']');
            array.append(readValue(elementPath, depth + 1));
            if (failed_) {
                return array;
            }
            ++index;
            skipWhitespace();
            if (!atEnd() && current() == QLatin1Char(']')) {
                ++position_;
                return array;
            }
            if (atEnd() || current() != QLatin1Char(',')) {
                fail(
                    QStringLiteral("jcs.array-separator-required"),
                    path,
                    QStringLiteral("Expected ',' or ']' in the array.")
                );
                return array;
            }
            ++position_;
        }
        return array;
    }

    [[nodiscard]] std::optional<QString> readString(const QString &path)
    {
        QString value;
        ++position_; // opening quote
        while (!atEnd()) {
            const auto unit = current();
            ++position_;
            if (unit == QLatin1Char('"')) {
                Error textError;
                if (!validateText(value, textPolicy_, path, textError)) {
                    failed_ = true;
                    error_ = std::move(textError);
                    return std::nullopt;
                }
                return value;
            }
            if (unit.unicode() < 0x20) {
                fail(
                    QStringLiteral("jcs.unescaped-control"),
                    path,
                    QStringLiteral("A JSON string contains an unescaped control character.")
                );
                return std::nullopt;
            }
            if (unit != QLatin1Char('\\')) {
                if (unit.isHighSurrogate()) {
                    if (atEnd() || !current().isLowSurrogate()) {
                        fail(
                            QStringLiteral("jcs.invalid-unicode"),
                            path,
                            QStringLiteral("A JSON string contains a lone high surrogate.")
                        );
                        return std::nullopt;
                    }
                    value.append(unit);
                    value.append(current());
                    ++position_;
                } else if (unit.isLowSurrogate()) {
                    fail(
                        QStringLiteral("jcs.invalid-unicode"),
                        path,
                        QStringLiteral("A JSON string contains a lone low surrogate.")
                    );
                    return std::nullopt;
                } else {
                    value.append(unit);
                }
                continue;
            }

            if (atEnd()) {
                break;
            }
            const auto escape = current();
            ++position_;
            switch (escape.unicode()) {
            case '"': value.append(QLatin1Char('"')); break;
            case '\\': value.append(QLatin1Char('\\')); break;
            case '/': value.append(QLatin1Char('/')); break;
            case 'b': value.append(QLatin1Char('\b')); break;
            case 'f': value.append(QLatin1Char('\f')); break;
            case 'n': value.append(QLatin1Char('\n')); break;
            case 'r': value.append(QLatin1Char('\r')); break;
            case 't': value.append(QLatin1Char('\t')); break;
            case 'u': {
                const auto high = readHexCodeUnit(path);
                if (failed_) {
                    return std::nullopt;
                }
                if (QChar::isHighSurrogate(high)) {
                    if (position_ + 2 > text_.size()
                        || text_.at(position_) != QLatin1Char('\\')
                        || text_.at(position_ + 1) != QLatin1Char('u')) {
                        fail(
                            QStringLiteral("jcs.invalid-unicode"),
                            path,
                            QStringLiteral("A high surrogate must be followed by a low surrogate.")
                        );
                        return std::nullopt;
                    }
                    position_ += 2;
                    const auto low = readHexCodeUnit(path);
                    if (failed_ || !QChar::isLowSurrogate(low)) {
                        if (!failed_) {
                            fail(
                                QStringLiteral("jcs.invalid-unicode"),
                                path,
                                QStringLiteral("A high surrogate must be followed by a low surrogate.")
                            );
                        }
                        return std::nullopt;
                    }
                    value.append(QChar(high));
                    value.append(QChar(low));
                } else if (QChar::isLowSurrogate(high)) {
                    fail(
                        QStringLiteral("jcs.invalid-unicode"),
                        path,
                        QStringLiteral("A JSON string contains a lone low surrogate.")
                    );
                    return std::nullopt;
                } else {
                    value.append(QChar(high));
                }
                break;
            }
            default:
                fail(
                    QStringLiteral("jcs.invalid-escape"),
                    path,
                    QStringLiteral("A JSON string contains an invalid escape.")
                );
                return std::nullopt;
            }
        }

        fail(
            QStringLiteral("jcs.unterminated-string"),
            path,
            QStringLiteral("The JSON string is unterminated.")
        );
        return std::nullopt;
    }

    [[nodiscard]] ushort readHexCodeUnit(const QString &path)
    {
        if (position_ + 4 > text_.size()) {
            fail(
                QStringLiteral("jcs.invalid-unicode-escape"),
                path,
                QStringLiteral("A Unicode escape must contain four hexadecimal digits.")
            );
            return 0;
        }
        ushort value = 0;
        for (int count = 0; count < 4; ++count) {
            const auto unit = text_.at(position_++);
            int digit = -1;
            if (unit >= QLatin1Char('0') && unit <= QLatin1Char('9')) {
                digit = unit.unicode() - '0';
            } else if (unit >= QLatin1Char('a') && unit <= QLatin1Char('f')) {
                digit = unit.unicode() - 'a' + 10;
            } else if (unit >= QLatin1Char('A') && unit <= QLatin1Char('F')) {
                digit = unit.unicode() - 'A' + 10;
            }
            if (digit < 0) {
                fail(
                    QStringLiteral("jcs.invalid-unicode-escape"),
                    path,
                    QStringLiteral("A Unicode escape contains a non-hexadecimal digit.")
                );
                return 0;
            }
            value = static_cast<ushort>((value << 4) | digit);
        }
        return value;
    }

    [[nodiscard]] QJsonValue readLiteral(
        const QString &path,
        const QStringView literal,
        const bool value
    )
    {
        if (literal.size() > text_.size() - position_
            || text_.sliced(position_, literal.size()) != literal) {
            fail(
                QStringLiteral("jcs.invalid-literal"),
                path,
                QStringLiteral("The JSON literal is invalid.")
            );
            return {};
        }
        position_ += literal.size();
        return value;
    }

    [[nodiscard]] QJsonValue readNull(const QString &path)
    {
        constexpr auto literal = u"null";
        if (4 > text_.size() - position_
            || text_.sliced(position_, 4) != literal) {
            fail(
                QStringLiteral("jcs.invalid-literal"),
                path,
                QStringLiteral("The JSON literal is invalid.")
            );
            return {};
        }
        position_ += 4;
        return QJsonValue(QJsonValue::Null);
    }

    [[nodiscard]] QJsonValue readNumber(const QString &path)
    {
        const auto start = position_;
        if (current() == QLatin1Char('-')) {
            ++position_;
        }
        if (atEnd()) {
            return invalidNumber(path);
        }

        bool significandNonzero = false;
        if (current() == QLatin1Char('0')) {
            ++position_;
            if (!atEnd() && current() >= QLatin1Char('0')
                && current() <= QLatin1Char('9')) {
                return invalidNumber(path);
            }
        } else if (current() >= QLatin1Char('1')
                   && current() <= QLatin1Char('9')) {
            significandNonzero = true;
            do {
                ++position_;
            } while (!atEnd() && current() >= QLatin1Char('0')
                     && current() <= QLatin1Char('9'));
        } else {
            return invalidNumber(path);
        }

        if (!atEnd() && current() == QLatin1Char('.')) {
            ++position_;
            if (atEnd() || current() < QLatin1Char('0')
                || current() > QLatin1Char('9')) {
                return invalidNumber(path);
            }
            do {
                significandNonzero = significandNonzero
                    || current() != QLatin1Char('0');
                ++position_;
            } while (!atEnd() && current() >= QLatin1Char('0')
                     && current() <= QLatin1Char('9'));
        }

        if (!atEnd() && (current() == QLatin1Char('e')
                         || current() == QLatin1Char('E'))) {
            ++position_;
            if (!atEnd() && (current() == QLatin1Char('+')
                             || current() == QLatin1Char('-'))) {
                ++position_;
            }
            if (atEnd() || current() < QLatin1Char('0')
                || current() > QLatin1Char('9')) {
                return invalidNumber(path);
            }
            do {
                ++position_;
            } while (!atEnd() && current() >= QLatin1Char('0')
                     && current() <= QLatin1Char('9'));
        }

        const auto lexeme = text_.sliced(start, position_ - start).toLatin1();
        if (lexeme.size() > std::numeric_limits<int>::max()) {
            fail(
                QStringLiteral("jcs.non-ijson-number"),
                path,
                QStringLiteral("The JSON number lexeme is too large to parse safely.")
            );
            return {};
        }
        static const double_conversion::StringToDoubleConverter converter(
            double_conversion::StringToDoubleConverter::NO_FLAGS,
            std::numeric_limits<double>::quiet_NaN(),
            std::numeric_limits<double>::quiet_NaN(),
            nullptr,
            nullptr
        );
        int processed = 0;
        const auto number = converter.StringToDouble(
            lexeme.constData(), static_cast<int>(lexeme.size()), &processed
        );
        if (processed != lexeme.size() || !std::isfinite(number)
            || (number == 0.0 && significandNonzero)) {
            fail(
                QStringLiteral("jcs.non-ijson-number"),
                path,
                QStringLiteral(
                    "The JSON number is not representable as finite IEEE-754 binary64."
                )
            );
            return {};
        }
        return QJsonValue(number);
    }

    [[nodiscard]] QJsonValue invalidNumber(const QString &path)
    {
        fail(
            QStringLiteral("jcs.invalid-number"),
            path,
            QStringLiteral("The JSON number grammar is invalid.")
        );
        return {};
    }

    QString text_;
    TextPolicy textPolicy_;
    Limits limits_;
    qsizetype position_ = 0;
    qsizetype valueCount_ = 0;
    bool failed_ = false;
    Error error_;
};

} // namespace

Result<QByteArray> serialize(
    const QJsonValue &value,
    const Framing framing,
    const TextPolicy textPolicy,
    const Limits limits
)
{
    return Serializer(framing, textPolicy, limits).run(value);
}

Result<QJsonObject> parseCanonicalObject(
    const QByteArrayView bytes,
    const Framing framing,
    const TextPolicy textPolicy,
    const Limits limits
)
{
    Result<QJsonObject> result;
    if (const auto invalid = limitsError(limits)) {
        result.errors.append(*invalid);
        return result;
    }
    if (!validFraming(framing)) {
        result.errors.append(makeError(
            QStringLiteral("jcs.unknown-framing"),
            QStringLiteral("$"),
            QStringLiteral("The canonical JSON framing is unknown.")
        ));
        return result;
    }
    if (!validTextPolicy(textPolicy)) {
        result.errors.append(makeError(
            QStringLiteral("jcs.unknown-text-policy"),
            QStringLiteral("$"),
            QStringLiteral("The canonical JSON text policy is unknown.")
        ));
        return result;
    }
    if (bytes.size() > limits.maximumBytes) {
        result.errors.append(makeError(
            QStringLiteral("jcs.input-limit"),
            QStringLiteral("$"),
            QStringLiteral("Canonical JSON exceeds the configured byte limit.")
        ));
        return result;
    }

    auto payload = bytes;
    if (framing == Framing::OneTrailingLineFeed) {
        if (payload.isEmpty() || payload.back() != '\n') {
            result.errors.append(makeError(
                QStringLiteral("jcs.missing-line-feed"),
                QStringLiteral("$"),
                QStringLiteral("The selected framing requires exactly one trailing LF.")
            ));
            return result;
        }
        payload = payload.first(payload.size() - 1);
    }

    if (payload.startsWith(QByteArrayView("\xef\xbb\xbf", 3))) {
        result.errors.append(makeError(
            QStringLiteral("jcs.bom-forbidden"),
            QStringLiteral("$"),
            QStringLiteral("Canonical JSON cannot contain a UTF-8 byte-order mark.")
        ));
        return result;
    }
    QStringDecoder decoder(QStringDecoder::Utf8);
    // QStringDecoder::decode returns a lazy conversion proxy. Materialize it
    // before querying hasError(), otherwise malformed UTF-8 could be observed
    // only after the error check.
    QString text = decoder.decode(payload);
    if (decoder.hasError()) {
        result.errors.append(makeError(
            QStringLiteral("jcs.invalid-utf8"),
            QStringLiteral("$"),
            QStringLiteral("Canonical JSON must be valid UTF-8.")
        ));
        return result;
    }

    auto parsed = Reader(std::move(text), textPolicy, limits).run();
    if (!parsed) {
        return parsed;
    }
    const auto encoded = serialize(
        QJsonValue(*parsed.value), framing, textPolicy, limits
    );
    if (!encoded) {
        result.errors = encoded.errors;
        return result;
    }
    if (QByteArrayView(*encoded.value) != bytes) {
        result.errors.append(makeError(
            QStringLiteral("jcs.noncanonical"),
            QStringLiteral("$"),
            QStringLiteral(
                "The JSON object is valid but is not byte-for-byte RFC 8785 canonical under the selected framing."
            )
        ));
        return result;
    }
    return parsed;
}

} // namespace HyprShelld::Compositor::CanonicalJson
