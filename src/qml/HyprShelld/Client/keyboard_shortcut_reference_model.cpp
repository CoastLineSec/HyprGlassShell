#include "keyboard_shortcut_reference_model.h"

#include "hyprland/keyboard_shortcut_reference.h"

#include <QVariantMap>

namespace HyprShelld {

KeyboardShortcutReferenceModel::KeyboardShortcutReferenceModel(QObject *parent)
    : QObject(parent)
{
    const auto parsed = Hyprland::embeddedKeyboardShortcutReference();
    if (!parsed) {
        errorMessage_ = parsed.errors.isEmpty()
            ? QStringLiteral("The embedded keyboard shortcut reference is unavailable.")
            : parsed.errors.constFirst().message.left(1024);
        return;
    }

    const auto &reference = *parsed.value;
    sourceDigest_ = reference.sourceDigest;
    artifactDigest_ = reference.artifactDigest;
    rows_.reserve(reference.rows.size());
    for (const auto &row : reference.rows) {
        QVariantMap options;
        if (row.options.locked.has_value()) {
            options.insert(
                QStringLiteral("locked"),
                *row.options.locked
            );
        }
        if (row.options.repeating.has_value()) {
            options.insert(
                QStringLiteral("repeating"),
                *row.options.repeating
            );
        }
        if (row.options.mouse.has_value()) {
            options.insert(QStringLiteral("mouse"), *row.options.mouse);
        }
        if (row.options.description.has_value()) {
            options.insert(
                QStringLiteral("description"),
                *row.options.description
            );
        }

        rows_.append(QVariantMap{
            {QStringLiteral("ordinal"), row.ordinal},
            {QStringLiteral("sourceLine"), row.sourceLine},
            {QStringLiteral("section"), row.section},
            {QStringLiteral("chord"), row.chord},
            {QStringLiteral("action"), row.action},
            {QStringLiteral("sourceText"), row.sourceText},
            {QStringLiteral("options"), options},
        });
    }
    available_ = true;
}

bool KeyboardShortcutReferenceModel::available() const
{
    return available_;
}

QString KeyboardShortcutReferenceModel::errorMessage() const
{
    return errorMessage_;
}

QString KeyboardShortcutReferenceModel::sourceDigest() const
{
    return sourceDigest_;
}

QString KeyboardShortcutReferenceModel::artifactDigest() const
{
    return artifactDigest_;
}

int KeyboardShortcutReferenceModel::rowCount() const
{
    return rows_.size();
}

QVariantList KeyboardShortcutReferenceModel::rows() const
{
    return rows_;
}

} // namespace HyprShelld
