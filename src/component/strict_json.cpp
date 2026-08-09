#include "strict_json.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonValue>
#include <QSet>
#include <QStringDecoder>

#include <cmath>
#include <limits>
#include <optional>

namespace HyprShelld::Components {
namespace {

[[nodiscard]] bool isAsciiDigit(const QChar character)
{
    return character >= QLatin1Char('0') && character <= QLatin1Char('9');
}

struct NormalizedJsonNumber final {
    bool negative = false;
    QString significantDigits;
    qint64 decimalExponent = 0;

    friend bool operator==(
        const NormalizedJsonNumber &,
        const NormalizedJsonNumber &
    ) = default;
};

[[nodiscard]] std::optional<NormalizedJsonNumber> normalizeJsonNumber(
    const QStringView lexeme
)
{
    qsizetype position = 0;
    const auto negative = !lexeme.isEmpty()
        && lexeme.front() == QLatin1Char('-');
    if (negative) {
        ++position;
    }

    auto exponentPosition = lexeme.indexOf(QLatin1Char('e'), position);
    if (exponentPosition < 0) {
        exponentPosition = lexeme.indexOf(QLatin1Char('E'), position);
    }
    const auto significandEnd = exponentPosition < 0
        ? lexeme.size()
        : exponentPosition;

    QString digits;
    digits.reserve(significandEnd - position);
    qsizetype fractionalDigits = 0;
    bool afterDecimalPoint = false;
    for (; position < significandEnd; ++position) {
        const auto character = lexeme.at(position);
        if (character == QLatin1Char('.')) {
            afterDecimalPoint = true;
            continue;
        }
        digits.append(character);
        if (afterDecimalPoint) {
            ++fractionalDigits;
        }
    }

    qsizetype firstNonzero = 0;
    while (firstNonzero < digits.size()
           && digits.at(firstNonzero) == QLatin1Char('0')) {
        ++firstNonzero;
    }
    if (firstNonzero == digits.size()) {
        return NormalizedJsonNumber{
            .significantDigits = QStringLiteral("0"),
        };
    }
    digits.remove(0, firstNonzero);

    qint64 explicitExponent = 0;
    if (exponentPosition >= 0) {
        bool converted = false;
        explicitExponent = lexeme.sliced(exponentPosition + 1)
                               .toLongLong(&converted, 10);
        if (!converted) {
            return std::nullopt;
        }
    }
    if (fractionalDigits
        > static_cast<quint64>(std::numeric_limits<qint64>::max())
        || explicitExponent
            < std::numeric_limits<qint64>::min()
                + static_cast<qint64>(fractionalDigits)) {
        return std::nullopt;
    }
    auto decimalExponent = explicitExponent
        - static_cast<qint64>(fractionalDigits);

    qsizetype trailingZeros = 0;
    while (digits.size() > 1 && digits.back() == QLatin1Char('0')) {
        digits.chop(1);
        ++trailingZeros;
    }
    if (trailingZeros
        > static_cast<quint64>(std::numeric_limits<qint64>::max())
        || decimalExponent
            > std::numeric_limits<qint64>::max()
                - static_cast<qint64>(trailingZeros)) {
        return std::nullopt;
    }
    decimalExponent += static_cast<qint64>(trailingZeros);

    return NormalizedJsonNumber{
        .negative = negative,
        .significantDigits = std::move(digits),
        .decimalExponent = decimalExponent,
    };
}

[[nodiscard]] QString serializedJsonNumber(const double number)
{
    const auto encoded = QJsonDocument(QJsonArray{number})
                             .toJson(QJsonDocument::Compact);
    if (encoded.size() < 2
        || encoded.front() != '['
        || encoded.back() != ']') {
        return {};
    }
    return QString::fromLatin1(encoded.sliced(1, encoded.size() - 2));
}

class StrictJsonReader final {
public:
    StrictJsonReader(QString text, const StrictJsonLimits limits)
        : text_(std::move(text))
        , limits_(limits)
    {
    }

    [[nodiscard]] ValidationResult<QJsonObject> read()
    {
        ValidationResult<QJsonObject> result;
        skipWhitespace();

        if (atEnd() || current() != QLatin1Char('{')) {
            fail(
                QStringLiteral("$"),
                QStringLiteral("json.root-object-required"),
                QStringLiteral("The JSON document root must be an object.")
            );
        } else {
            const auto value = readObject(QStringLiteral("$"), 1);
            skipWhitespace();
            if (!failed_ && !atEnd()) {
                fail(
                    QStringLiteral("$"),
                    QStringLiteral("json.trailing-data"),
                    QStringLiteral("Unexpected data follows the JSON value.")
                );
            }
            if (!failed_) {
                result.value = value;
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
        return atEnd() ? QChar() : text_.at(position_);
    }

    void skipWhitespace()
    {
        while (!atEnd()) {
            const auto character = current();
            if (character != QLatin1Char(' ')
                && character != QLatin1Char('\t')
                && character != QLatin1Char('\r')
                && character != QLatin1Char('\n')) {
                break;
            }
            ++position_;
        }
    }

    void fail(QString path, QString code, QString message)
    {
        if (failed_) {
            return;
        }
        failed_ = true;
        error_ = {
            .path = std::move(path),
            .code = std::move(code),
            .message = std::move(message),
        };
    }

    [[nodiscard]] static QString childPath(
        const QString &parent,
        const QString &key
    )
    {
        return parent + QLatin1Char('.') + key;
    }

    [[nodiscard]] QJsonValue readValue(const QString &path, const int depth)
    {
        skipWhitespace();
        if (atEnd()) {
            fail(
                path,
                QStringLiteral("json.unexpected-end"),
                QStringLiteral("The JSON value is incomplete.")
            );
            return {};
        }

        switch (current().unicode()) {
        case '{':
            return readObject(path, depth);
        case '[':
            return readArray(path, depth);
        case '"':
            return readString(path);
        case 't':
            return readLiteral(path, QStringLiteral("true"), QJsonValue(true));
        case 'f':
            return readLiteral(path, QStringLiteral("false"), QJsonValue(false));
        case 'n':
            return readLiteral(
                path,
                QStringLiteral("null"),
                QJsonValue(QJsonValue::Null)
            );
        default:
            if (current() == QLatin1Char('-') || isAsciiDigit(current())) {
                return readNumber(path);
            }
            fail(
                path,
                QStringLiteral("json.invalid-token"),
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
                path,
                QStringLiteral("json.depth-limit"),
                QStringLiteral("The JSON nesting limit was exceeded.")
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
                    path,
                    QStringLiteral("json.object-key-required"),
                    QStringLiteral("Expected a quoted object key.")
                );
                break;
            }

            const auto keyValue = readString(path);
            if (failed_) {
                break;
            }
            const auto key = keyValue.toString();
            const auto keyPath = childPath(path, key);
            if (keys.contains(key)) {
                fail(
                    keyPath,
                    QStringLiteral("json.duplicate-key"),
                    QStringLiteral("Duplicate JSON object key: %1").arg(key)
                );
                break;
            }
            keys.insert(key);

            skipWhitespace();
            if (atEnd() || current() != QLatin1Char(':')) {
                fail(
                    keyPath,
                    QStringLiteral("json.colon-required"),
                    QStringLiteral("Expected ':' after the object key.")
                );
                break;
            }
            ++position_;

            object.insert(key, readValue(keyPath, depth + 1));
            if (failed_) {
                break;
            }

            skipWhitespace();
            if (!atEnd() && current() == QLatin1Char('}')) {
                ++position_;
                return object;
            }
            if (atEnd() || current() != QLatin1Char(',')) {
                fail(
                    path,
                    QStringLiteral("json.object-separator-required"),
                    QStringLiteral("Expected ',' or '}' in the object.")
                );
                break;
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
                path,
                QStringLiteral("json.depth-limit"),
                QStringLiteral("The JSON nesting limit was exceeded.")
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
            array.append(readValue(
                path + QLatin1Char('[') + QString::number(index)
                    + QLatin1Char(']'),
                depth + 1
            ));
            if (failed_) {
                break;
            }
            ++index;

            skipWhitespace();
            if (!atEnd() && current() == QLatin1Char(']')) {
                ++position_;
                return array;
            }
            if (atEnd() || current() != QLatin1Char(',')) {
                fail(
                    path,
                    QStringLiteral("json.array-separator-required"),
                    QStringLiteral("Expected ',' or ']' in the array.")
                );
                break;
            }
            ++position_;
        }
        return array;
    }

    [[nodiscard]] QJsonValue readString(const QString &path)
    {
        QString value;
        ++position_; // opening quote

        while (!atEnd()) {
            const auto character = current();
            ++position_;

            if (character == QLatin1Char('"')) {
                return value;
            }
            if (character.unicode() < 0x20) {
                fail(
                    path,
                    QStringLiteral("json.string-control-character"),
                    QStringLiteral("Unescaped control character in string.")
                );
                return {};
            }
            if (character != QLatin1Char('\\')) {
                if (character.isHighSurrogate()) {
                    if (atEnd() || !current().isLowSurrogate()) {
                        fail(
                            path,
                            QStringLiteral("json.invalid-unicode"),
                            QStringLiteral("Unpaired Unicode surrogate in string.")
                        );
                        return {};
                    }
                    value.append(character);
                    value.append(current());
                    ++position_;
                } else if (character.isLowSurrogate()) {
                    fail(
                        path,
                        QStringLiteral("json.invalid-unicode"),
                        QStringLiteral("Unpaired Unicode surrogate in string.")
                    );
                    return {};
                } else {
                    value.append(character);
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
                    return {};
                }
                if (QChar::isHighSurrogate(high)) {
                    if (position_ + 2 > text_.size()
                        || text_.at(position_) != QLatin1Char('\\')
                        || text_.at(position_ + 1) != QLatin1Char('u')) {
                        fail(
                            path,
                            QStringLiteral("json.invalid-unicode"),
                            QStringLiteral("High surrogate requires a low surrogate.")
                        );
                        return {};
                    }
                    position_ += 2;
                    const auto low = readHexCodeUnit(path);
                    if (failed_ || !QChar::isLowSurrogate(low)) {
                        if (!failed_) {
                            fail(
                                path,
                                QStringLiteral("json.invalid-unicode"),
                                QStringLiteral("High surrogate requires a low surrogate.")
                            );
                        }
                        return {};
                    }
                    value.append(QChar(high));
                    value.append(QChar(low));
                } else if (QChar::isLowSurrogate(high)) {
                    fail(
                        path,
                        QStringLiteral("json.invalid-unicode"),
                        QStringLiteral("Unpaired low surrogate in string.")
                    );
                    return {};
                } else {
                    value.append(QChar(high));
                }
                break;
            }
            default:
                fail(
                    path,
                    QStringLiteral("json.invalid-escape"),
                    QStringLiteral("Invalid JSON string escape.")
                );
                return {};
            }
        }

        fail(
            path,
            QStringLiteral("json.unterminated-string"),
            QStringLiteral("Unterminated JSON string.")
        );
        return {};
    }

    [[nodiscard]] ushort readHexCodeUnit(const QString &path)
    {
        if (position_ + 4 > text_.size()) {
            fail(
                path,
                QStringLiteral("json.invalid-unicode-escape"),
                QStringLiteral("Unicode escape must contain four hex digits.")
            );
            return 0;
        }

        ushort value = 0;
        for (int count = 0; count < 4; ++count) {
            const auto character = text_.at(position_++);
            int digit = -1;
            if (character >= QLatin1Char('0') && character <= QLatin1Char('9')) {
                digit = character.unicode() - '0';
            } else if (character >= QLatin1Char('a')
                       && character <= QLatin1Char('f')) {
                digit = character.unicode() - 'a' + 10;
            } else if (character >= QLatin1Char('A')
                       && character <= QLatin1Char('F')) {
                digit = character.unicode() - 'A' + 10;
            }
            if (digit < 0) {
                fail(
                    path,
                    QStringLiteral("json.invalid-unicode-escape"),
                    QStringLiteral("Unicode escape contains a non-hex digit.")
                );
                return 0;
            }
            value = static_cast<ushort>((value << 4) | digit);
        }
        return value;
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

        if (current() == QLatin1Char('0')) {
            ++position_;
            if (!atEnd() && isAsciiDigit(current())) {
                return invalidNumber(path);
            }
        } else if (current() >= QLatin1Char('1')
                   && current() <= QLatin1Char('9')) {
            while (!atEnd() && isAsciiDigit(current())) {
                ++position_;
            }
        } else {
            return invalidNumber(path);
        }

        if (!atEnd() && current() == QLatin1Char('.')) {
            ++position_;
            if (atEnd() || !isAsciiDigit(current())) {
                return invalidNumber(path);
            }
            while (!atEnd() && isAsciiDigit(current())) {
                ++position_;
            }
        }

        if (!atEnd()
            && (current() == QLatin1Char('e')
                || current() == QLatin1Char('E'))) {
            ++position_;
            if (!atEnd()
                && (current() == QLatin1Char('+')
                    || current() == QLatin1Char('-'))) {
                ++position_;
            }
            if (atEnd() || !isAsciiDigit(current())) {
                return invalidNumber(path);
            }
            while (!atEnd() && isAsciiDigit(current())) {
                ++position_;
            }
        }

        const auto lexeme = text_.mid(start, position_ - start);
        bool converted = false;
        const auto number = lexeme.toDouble(&converted);
        if (!converted || !std::isfinite(number)) {
            return invalidNumber(path);
        }
        const auto normalizedAuthored = normalizeJsonNumber(lexeme);
        const auto normalizedSerialized = normalizeJsonNumber(
            serializedJsonNumber(number)
        );
        if (!normalizedAuthored || !normalizedSerialized
            || *normalizedAuthored != *normalizedSerialized) {
            fail(
                path,
                QStringLiteral("json.lossy-number"),
                QStringLiteral("The JSON number cannot be parsed and persisted without changing its value.")
            );
            return {};
        }
        return number;
    }

    [[nodiscard]] QJsonValue invalidNumber(const QString &path)
    {
        fail(
            path,
            QStringLiteral("json.invalid-number"),
            QStringLiteral("Invalid or non-finite JSON number.")
        );
        return {};
    }

    [[nodiscard]] QJsonValue readLiteral(
        const QString &path,
        const QString &literal,
        const QJsonValue value
    )
    {
        if (text_.mid(position_, literal.size()) != literal) {
            fail(
                path,
                QStringLiteral("json.invalid-token"),
                QStringLiteral("Invalid JSON literal.")
            );
            return {};
        }
        position_ += literal.size();
        return value;
    }

    QString text_;
    StrictJsonLimits limits_;
    qsizetype position_ = 0;
    bool failed_ = false;
    ValidationError error_;
};

} // namespace

ValidationResult<QJsonObject> parseStrictJsonObject(
    const QByteArrayView bytes,
    const StrictJsonLimits limits
)
{
    ValidationResult<QJsonObject> result;
    if (limits.maximumBytes <= 0 || limits.maximumDepth <= 0) {
        result.errors.append({
            .path = QStringLiteral("$"),
            .code = QStringLiteral("json.invalid-limits"),
            .message = QStringLiteral("JSON limits must be positive."),
        });
        return result;
    }
    if (bytes.size() > limits.maximumBytes) {
        result.errors.append({
            .path = QStringLiteral("$"),
            .code = QStringLiteral("json.size-limit"),
            .message = QStringLiteral("The JSON document exceeds its byte limit."),
        });
        return result;
    }

    if (!bytes.isValidUtf8()) {
        result.errors.append({
            .path = QStringLiteral("$"),
            .code = QStringLiteral("json.invalid-utf8"),
            .message = QStringLiteral("The JSON document is not valid UTF-8."),
        });
        return result;
    }

    QStringDecoder decoder(QStringDecoder::Utf8);
    auto text = decoder.decode(bytes);
    return StrictJsonReader(std::move(text), limits).read();
}

} // namespace HyprShelld::Components
