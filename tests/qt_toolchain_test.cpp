#include <QVersionNumber>
#include <QtGlobal>
#include <QtTest>

class ToolchainTest final : public QObject {
    Q_OBJECT

private slots:
    void usesSupportedQtVersion()
    {
        const auto runtimeVersion = QVersionNumber::fromString(qVersion());
        const QVersionNumber minimumVersion(6, 8);

        QVERIFY2(
            QVersionNumber::compare(runtimeVersion, minimumVersion) >= 0,
            "HyprShelld requires Qt 6.8 or newer"
        );
    }
};

QTEST_APPLESS_MAIN(ToolchainTest)

#include "qt_toolchain_test.moc"
