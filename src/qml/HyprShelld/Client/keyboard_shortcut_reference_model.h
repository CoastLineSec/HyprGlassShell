#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

namespace HyprShelld {

class KeyboardShortcutReferenceModel final : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(bool available READ available CONSTANT)
    Q_PROPERTY(QString errorMessage READ errorMessage CONSTANT)
    Q_PROPERTY(QString sourceDigest READ sourceDigest CONSTANT)
    Q_PROPERTY(QString artifactDigest READ artifactDigest CONSTANT)
    Q_PROPERTY(int rowCount READ rowCount CONSTANT)
    Q_PROPERTY(QVariantList rows READ rows CONSTANT)

public:
    explicit KeyboardShortcutReferenceModel(QObject *parent = nullptr);

    [[nodiscard]] bool available() const;
    [[nodiscard]] QString errorMessage() const;
    [[nodiscard]] QString sourceDigest() const;
    [[nodiscard]] QString artifactDigest() const;
    [[nodiscard]] int rowCount() const;
    [[nodiscard]] QVariantList rows() const;

private:
    bool available_ = false;
    QString errorMessage_;
    QString sourceDigest_;
    QString artifactDigest_;
    QVariantList rows_;
};

} // namespace HyprShelld
