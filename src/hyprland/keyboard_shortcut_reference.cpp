#include "keyboard_shortcut_reference.h"

#include "json_support.h"

#include <QCryptographicHash>
#include <QFile>
#include <QJsonArray>
#include <QJsonObject>
#include <QResource>
#include <QSet>

#include <cmath>
#include <utility>

void initializeKeyboardShortcutReferenceResource()
{
    Q_INIT_RESOURCE(hyprshelld_keyboard_shortcut_reference);
}

namespace HyprShelld::Hyprland {
namespace {

constexpr qsizetype maximumReferenceBytes = 64 * 1024;
constexpr int maximumReferenceDepth = 5;
constexpr int maximumSectionCharacters = 128;
constexpr int maximumChordCharacters = 128;
constexpr int maximumActionCharacters = 2048;
constexpr int maximumSourceTextCharacters = 3072;
constexpr int maximumDescriptionCharacters = 512;

void addError(
    ValidationErrors &errors,
    const QString &path,
    const QString &code,
    const QString &message
)
{
    errors.append({.path = path, .code = code, .message = message});
}

[[nodiscard]] bool hasExactKeys(
    const QJsonObject &object,
    const QSet<QString> &expected
)
{
    const auto keys = object.keys();
    return QSet<QString>(keys.cbegin(), keys.cend()) == expected;
}

[[nodiscard]] std::optional<int> exactInteger(
    const QJsonValue &value,
    const int minimum,
    const int maximum
)
{
    if (!value.isDouble()) {
        return std::nullopt;
    }
    const auto number = value.toDouble();
    if (!std::isfinite(number) || std::trunc(number) != number
        || number < minimum || number > maximum) {
        return std::nullopt;
    }
    return static_cast<int>(number);
}

[[nodiscard]] bool presentationTextValid(
    const QString &value,
    const int maximumCharacters
)
{
    if (value.isEmpty() || value.size() > maximumCharacters) {
        return false;
    }
    for (const auto character : value) {
        if (character.unicode() < 0x20) {
            return false;
        }
    }
    return true;
}

void parseBooleanOption(
    const QJsonObject &object,
    const QString &key,
    const QString &path,
    std::optional<bool> &target,
    ValidationErrors &errors
)
{
    if (!object.contains(key)) {
        return;
    }
    const auto value = object.value(key);
    if (!value.isBool() || !value.toBool()) {
        addError(
            errors,
            path + QLatin1Char('.') + key,
            QStringLiteral("legacy-shortcuts.invalid-option"),
            QStringLiteral("The reference option must be the present true flag.")
        );
        return;
    }
    target = true;
}

} // namespace

ValidationResult<KeyboardShortcutReference>
parseKeyboardShortcutReference(const QByteArrayView bytes)
{
    ValidationResult<KeyboardShortcutReference> result;
    auto parsed = JsonSupport::parseStrictObject(
        bytes,
        maximumReferenceBytes,
        maximumReferenceDepth
    );
    result.errors = parsed.errors;
    if (!parsed.value.has_value()) {
        return result;
    }
    const auto artifactDigest = QCryptographicHash::hash(
        bytes,
        QCryptographicHash::Sha256
    ).toHex();
    if (artifactDigest != QByteArray(legacyShortcutArtifactDigest)) {
        addError(
            result.errors,
            QStringLiteral("$"),
            QStringLiteral("legacy-shortcuts.artifact-digest-mismatch"),
            QStringLiteral("The keyboard shortcut reference failed its compiled receipt.")
        );
    }
    const auto root = *parsed.value;
    const auto canonical = JsonSupport::canonicalJson(root) + '\n';
    if (canonical != bytes) {
        addError(
            result.errors,
            QStringLiteral("$"),
            QStringLiteral("legacy-shortcuts.non-canonical"),
            QStringLiteral("The reference artifact must be canonical JSON with one final line feed.")
        );
    }
    if (!hasExactKeys(
            root,
            {
                QStringLiteral("contractVersion"),
                QStringLiteral("rows"),
                QStringLiteral("source"),
            }
        )) {
        addError(
            result.errors,
            QStringLiteral("$"),
            QStringLiteral("legacy-shortcuts.invalid-fields"),
            QStringLiteral("The reference artifact has missing or unknown root fields.")
        );
    }

    const auto contractVersion = exactInteger(
        root.value(QStringLiteral("contractVersion")),
        1,
        1
    );
    if (!contractVersion.has_value()) {
        addError(
            result.errors,
            QStringLiteral("$.contractVersion"),
            QStringLiteral("legacy-shortcuts.unsupported-version"),
            QStringLiteral("Keyboard shortcut reference contract version 1 is required.")
        );
    }

    QString sourceDigest;
    const auto sourceValue = root.value(QStringLiteral("source"));
    if (!sourceValue.isObject()) {
        addError(
            result.errors,
            QStringLiteral("$.source"),
            QStringLiteral("legacy-shortcuts.invalid-source"),
            QStringLiteral("The reference source receipt must be an object.")
        );
    } else {
        const auto source = sourceValue.toObject();
        if (!hasExactKeys(
                source,
                {
                    QStringLiteral("bindingCount"),
                    QStringLiteral("byteLength"),
                    QStringLiteral("lineCount"),
                    QStringLiteral("path"),
                    QStringLiteral("sha256"),
                }
            )) {
            addError(
                result.errors,
                QStringLiteral("$.source"),
                QStringLiteral("legacy-shortcuts.invalid-fields"),
                QStringLiteral("The reference source receipt has missing or unknown fields.")
            );
        }
        if (exactInteger(
                source.value(QStringLiteral("bindingCount")),
                legacyShortcutReferenceRows,
                legacyShortcutReferenceRows
            ) == std::nullopt) {
            addError(
                result.errors,
                QStringLiteral("$.source.bindingCount"),
                QStringLiteral("legacy-shortcuts.invalid-source"),
                QStringLiteral("The legacy binding count must be 117.")
            );
        }
        if (exactInteger(
                source.value(QStringLiteral("byteLength")),
                9738,
                9738
            ) == std::nullopt
            || exactInteger(
                source.value(QStringLiteral("lineCount")),
                171,
                171
            ) == std::nullopt) {
            addError(
                result.errors,
                QStringLiteral("$.source"),
                QStringLiteral("legacy-shortcuts.invalid-source"),
                QStringLiteral("The legacy source byte or line receipt changed.")
            );
        }
        if (source.value(QStringLiteral("path")).toString()
            != QStringLiteral(
                "core/internal/config/embedded/hyprland/hgs/keybinds.lua"
            )) {
            addError(
                result.errors,
                QStringLiteral("$.source.path"),
                QStringLiteral("legacy-shortcuts.invalid-source"),
                QStringLiteral("The legacy source path changed.")
            );
        }
        sourceDigest = source.value(QStringLiteral("sha256")).toString();
        if (sourceDigest != QString::fromLatin1(legacyShortcutSourceDigest)) {
            addError(
                result.errors,
                QStringLiteral("$.source.sha256"),
                QStringLiteral("legacy-shortcuts.invalid-source"),
                QStringLiteral("The legacy source digest changed.")
            );
        }
    }

    QVector<KeyboardShortcutReferenceRow> rows;
    const auto rowsValue = root.value(QStringLiteral("rows"));
    if (!rowsValue.isArray()) {
        addError(
            result.errors,
            QStringLiteral("$.rows"),
            QStringLiteral("legacy-shortcuts.invalid-rows"),
            QStringLiteral("The reference rows must be an array.")
        );
    } else {
        const auto array = rowsValue.toArray();
        if (array.size() != legacyShortcutReferenceRows) {
            addError(
                result.errors,
                QStringLiteral("$.rows"),
                QStringLiteral("legacy-shortcuts.invalid-count"),
                QStringLiteral("The reference must contain exactly 117 rows.")
            );
        }
        rows.reserve(array.size());
        int previousSourceLine = 0;
        for (qsizetype index = 0; index < array.size(); ++index) {
            const auto path = QStringLiteral("$.rows[")
                + QString::number(index) + QLatin1Char(']');
            if (!array.at(index).isObject()) {
                addError(
                    result.errors,
                    path,
                    QStringLiteral("legacy-shortcuts.invalid-row"),
                    QStringLiteral("Each reference row must be an object.")
                );
                continue;
            }
            const auto object = array.at(index).toObject();
            if (!hasExactKeys(
                    object,
                    {
                        QStringLiteral("action"),
                        QStringLiteral("chord"),
                        QStringLiteral("options"),
                        QStringLiteral("ordinal"),
                        QStringLiteral("section"),
                        QStringLiteral("sourceLine"),
                        QStringLiteral("sourceText"),
                    }
                )) {
                addError(
                    result.errors,
                    path,
                    QStringLiteral("legacy-shortcuts.invalid-fields"),
                    QStringLiteral("A reference row has missing or unknown fields.")
                );
            }

            KeyboardShortcutReferenceRow row;
            const auto ordinal = exactInteger(
                object.value(QStringLiteral("ordinal")),
                1,
                legacyShortcutReferenceRows
            );
            if (!ordinal.has_value()
                || *ordinal != static_cast<int>(index) + 1) {
                addError(
                    result.errors,
                    path + QStringLiteral(".ordinal"),
                    QStringLiteral("legacy-shortcuts.invalid-order"),
                    QStringLiteral("Reference ordinals must match source order.")
                );
            } else {
                row.ordinal = *ordinal;
            }

            const auto sourceLine = exactInteger(
                object.value(QStringLiteral("sourceLine")),
                1,
                171
            );
            if (!sourceLine.has_value()
                || *sourceLine <= previousSourceLine) {
                addError(
                    result.errors,
                    path + QStringLiteral(".sourceLine"),
                    QStringLiteral("legacy-shortcuts.invalid-order"),
                    QStringLiteral("Reference source lines must increase strictly.")
                );
            } else {
                row.sourceLine = *sourceLine;
                previousSourceLine = *sourceLine;
            }

            row.section = object.value(QStringLiteral("section")).toString();
            row.chord = object.value(QStringLiteral("chord")).toString();
            row.action = object.value(QStringLiteral("action")).toString();
            row.sourceText = object.value(QStringLiteral("sourceText")).toString();
            if (!object.value(QStringLiteral("section")).isString()
                || !presentationTextValid(
                    row.section,
                    maximumSectionCharacters
                )) {
                addError(
                    result.errors,
                    path + QStringLiteral(".section"),
                    QStringLiteral("legacy-shortcuts.invalid-text"),
                    QStringLiteral("The legacy section is invalid.")
                );
            }
            if (!object.value(QStringLiteral("chord")).isString()
                || !presentationTextValid(
                    row.chord,
                    maximumChordCharacters
                )) {
                addError(
                    result.errors,
                    path + QStringLiteral(".chord"),
                    QStringLiteral("legacy-shortcuts.invalid-text"),
                    QStringLiteral("The legacy chord is invalid.")
                );
            }
            if (!object.value(QStringLiteral("action")).isString()
                || !presentationTextValid(
                    row.action,
                    maximumActionCharacters
                )) {
                addError(
                    result.errors,
                    path + QStringLiteral(".action"),
                    QStringLiteral("legacy-shortcuts.invalid-text"),
                    QStringLiteral("The legacy action expression is invalid.")
                );
            }
            if (!object.value(QStringLiteral("sourceText")).isString()
                || !presentationTextValid(
                    row.sourceText,
                    maximumSourceTextCharacters
                )
                || !row.sourceText.startsWith(QStringLiteral("hl.bind("))) {
                addError(
                    result.errors,
                    path + QStringLiteral(".sourceText"),
                    QStringLiteral("legacy-shortcuts.invalid-text"),
                    QStringLiteral("The legacy source line is invalid.")
                );
            }

            const auto optionsValue = object.value(QStringLiteral("options"));
            if (!optionsValue.isObject()) {
                addError(
                    result.errors,
                    path + QStringLiteral(".options"),
                    QStringLiteral("legacy-shortcuts.invalid-options"),
                    QStringLiteral("Reference options must be an object.")
                );
            } else {
                const auto options = optionsValue.toObject();
                const QSet<QString> allowed{
                    QStringLiteral("description"),
                    QStringLiteral("locked"),
                    QStringLiteral("mouse"),
                    QStringLiteral("repeating"),
                };
                const auto optionKeys = options.keys();
                auto unknown = QSet<QString>(
                    optionKeys.cbegin(),
                    optionKeys.cend()
                );
                unknown.subtract(allowed);
                if (!unknown.isEmpty()) {
                    addError(
                        result.errors,
                        path + QStringLiteral(".options"),
                        QStringLiteral("legacy-shortcuts.invalid-fields"),
                        QStringLiteral("A reference option is unknown.")
                    );
                }
                parseBooleanOption(
                    options,
                    QStringLiteral("locked"),
                    path + QStringLiteral(".options"),
                    row.options.locked,
                    result.errors
                );
                parseBooleanOption(
                    options,
                    QStringLiteral("repeating"),
                    path + QStringLiteral(".options"),
                    row.options.repeating,
                    result.errors
                );
                parseBooleanOption(
                    options,
                    QStringLiteral("mouse"),
                    path + QStringLiteral(".options"),
                    row.options.mouse,
                    result.errors
                );
                if (options.contains(QStringLiteral("description"))) {
                    const auto description = options.value(
                        QStringLiteral("description")
                    );
                    if (!description.isString()
                        || !presentationTextValid(
                            description.toString(),
                            maximumDescriptionCharacters
                        )) {
                        addError(
                            result.errors,
                            path + QStringLiteral(".options.description"),
                            QStringLiteral("legacy-shortcuts.invalid-option"),
                            QStringLiteral("The source description is invalid.")
                        );
                    } else {
                        row.options.description = description.toString();
                    }
                }
            }
            rows.append(std::move(row));
        }
    }

    if (!result.errors.isEmpty()) {
        return result;
    }

    result.value = KeyboardShortcutReference{
        .sourceDigest = sourceDigest,
        .artifactDigest = QString::fromLatin1(artifactDigest),
        .rows = std::move(rows),
    };
    return result;
}

ValidationResult<KeyboardShortcutReference>
embeddedKeyboardShortcutReference()
{
    initializeKeyboardShortcutReferenceResource();
    ValidationResult<KeyboardShortcutReference> result;
    QFile file(QStringLiteral(
        ":/hyprshelld/hyprland/keyboard-shortcuts-reference-v1.json"
    ));
    if (!file.open(QIODevice::ReadOnly)) {
        addError(
            result.errors,
            QStringLiteral("$"),
            QStringLiteral("legacy-shortcuts.resource-unavailable"),
            QStringLiteral("The embedded keyboard shortcut reference is unavailable.")
        );
        return result;
    }
    const auto bytes = file.readAll();
    if (bytes.size() != legacyShortcutArtifactBytes
        || QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex()
            != QByteArray(legacyShortcutArtifactDigest)) {
        addError(
            result.errors,
            QStringLiteral("$"),
            QStringLiteral("legacy-shortcuts.artifact-digest-mismatch"),
            QStringLiteral("The embedded keyboard shortcut reference failed its compiled receipt.")
        );
        return result;
    }
    return parseKeyboardShortcutReference(bytes);
}

} // namespace HyprShelld::Hyprland
