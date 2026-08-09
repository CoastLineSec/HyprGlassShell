#pragma once

#include <QString>
#include <QVector>

#include <optional>

namespace HyprShelld::Hyprland {

struct ValidationError final {
    QString path;
    QString code;
    QString message;

    friend bool operator==(const ValidationError &, const ValidationError &)
        = default;
};

using ValidationErrors = QVector<ValidationError>;

template<typename T>
struct ValidationResult final {
    std::optional<T> value;
    ValidationErrors errors;

    [[nodiscard]] bool ok() const
    {
        return value.has_value() && errors.isEmpty();
    }

    explicit operator bool() const { return ok(); }
};

} // namespace HyprShelld::Hyprland
