#include <QProcess>
#include <QProcessEnvironment>
#include <QTemporaryDir>
#include <QTest>

class SettingsSmokeTest final : public QObject {
    Q_OBJECT

private slots:
    void startsWithoutConfigService();
};

void SettingsSmokeTest::startsWithoutConfigService()
{
    const auto executable = qEnvironmentVariable("HYPRSHELLD_SETTINGS_EXECUTABLE");
    QVERIFY2(!executable.isEmpty(), "HYPRSHELLD_SETTINGS_EXECUTABLE is not set");

    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    auto environment = QProcessEnvironment::systemEnvironment();
    environment.insert(QStringLiteral("QT_QPA_PLATFORM"), QStringLiteral("offscreen"));
    environment.insert(QStringLiteral("QT_FATAL_WARNINGS"), QStringLiteral("1"));
    environment.insert(
        QStringLiteral("XDG_CONFIG_HOME"),
        directory.path() + QStringLiteral("/config")
    );
    environment.insert(
        QStringLiteral("XDG_STATE_HOME"),
        directory.path() + QStringLiteral("/state")
    );
    environment.remove(QStringLiteral("QML_IMPORT_PATH"));
    environment.remove(QStringLiteral("QML2_IMPORT_PATH"));
    environment.remove(QStringLiteral("LD_LIBRARY_PATH"));
    environment.remove(QStringLiteral("QT_PLUGIN_PATH"));
    environment.remove(QStringLiteral("QT_QPA_PLATFORM_PLUGIN_PATH"));

    QProcess process;
    process.setProcessChannelMode(QProcess::MergedChannels);
    process.setProcessEnvironment(environment);
    process.start(executable);

    QVERIFY2(process.waitForStarted(), qPrintable(process.errorString()));
    QTest::qWait(750);

    const auto output = process.readAll();
    QCOMPARE(process.state(), QProcess::Running);
    QVERIFY2(!output.contains("QQmlApplicationEngine failed"), output.constData());
    QVERIFY2(!output.contains("module \"HyprShelld"), output.constData());

    process.terminate();
    if (!process.waitForFinished(3000)) {
        process.kill();
        QVERIFY(process.waitForFinished(3000));
    }
}

QTEST_GUILESS_MAIN(SettingsSmokeTest)

#include "settings_smoke_test.moc"
