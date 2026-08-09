#include <QFile>
#include <QHash>
#include <QProcess>
#include <QSet>
#include <QStringList>
#include <QtTest>

#include <memory>

namespace {

class IniFile {
public:
    explicit IniFile(const QString &path)
    {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            error_ = QStringLiteral("Cannot open %1: %2")
                         .arg(path, file.errorString());
            return;
        }

        QString section;
        QString continued;
        qsizetype lineNumber = 0;
        while (!file.atEnd()) {
            ++lineNumber;
            auto line = QString::fromUtf8(file.readLine());
            while (line.endsWith(QLatin1Char('\n'))
                   || line.endsWith(QLatin1Char('\r'))) {
                line.chop(1);
            }

            if (!continued.isEmpty()) {
                line = continued + line.trimmed();
                continued.clear();
            }
            if (line.endsWith(QLatin1Char('\\'))) {
                line.chop(1);
                continued = line;
                continue;
            }

            line = line.trimmed();
            if (line.isEmpty() || line.startsWith(QLatin1Char('#'))
                || line.startsWith(QLatin1Char(';'))) {
                continue;
            }

            if (line.startsWith(QLatin1Char('['))
                && line.endsWith(QLatin1Char(']'))) {
                section = line.mid(1, line.size() - 2).trimmed();
                if (section.isEmpty()) {
                    error_ = QStringLiteral("%1:%2: Empty section")
                                 .arg(path)
                                 .arg(lineNumber);
                    return;
                }
                sections_.insert(section);
                continue;
            }

            const auto separator = line.indexOf(QLatin1Char('='));
            if (section.isEmpty() || separator <= 0) {
                error_ = QStringLiteral("%1:%2: Invalid assignment")
                             .arg(path)
                             .arg(lineNumber);
                return;
            }

            const auto key = line.left(separator).trimmed();
            const auto value = line.mid(separator + 1).trimmed();
            assignments_[section][key].append(value);
        }

        if (!continued.isEmpty()) {
            error_ = QStringLiteral("%1: Unterminated continuation").arg(path);
        }
    }

    [[nodiscard]] QString error() const
    {
        return error_;
    }

    [[nodiscard]] bool hasSection(const QString &section) const
    {
        return sections_.contains(section);
    }

    [[nodiscard]] QStringList values(
        const QString &section,
        const QString &key
    ) const
    {
        return assignments_.value(section).value(key);
    }

    [[nodiscard]] QStringList words(
        const QString &section,
        const QString &key
    ) const
    {
        QStringList result;
        for (const auto &value : values(section, key)) {
            result.append(QProcess::splitCommand(value));
        }
        return result;
    }

private:
    QString error_;
    QSet<QString> sections_;
    QHash<QString, QHash<QString, QStringList>> assignments_;
};

QStringList sorted(QStringList values)
{
    values.sort();
    return values;
}

} // namespace

class SystemdUnitContractTest final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        const auto unitDirectory = QStringLiteral(HYPRSHELLD_SYSTEMD_UNIT_DIR);
        target_ = std::make_unique<IniFile>(
            unitDirectory + QStringLiteral("/hyprshelld.target")
        );
        slice_ = std::make_unique<IniFile>(
            unitDirectory + QStringLiteral("/session-hyprshelld.slice")
        );
        componentHelpersSlice_ = std::make_unique<IniFile>(
            unitDirectory
                + QStringLiteral("/session-hyprshelld-components.slice")
        );
        coordinator_ = std::make_unique<IniFile>(
            unitDirectory + QStringLiteral("/hyprshelld.service")
        );
        configd_ = std::make_unique<IniFile>(
            unitDirectory + QStringLiteral("/hyprshelld-configd.service")
        );
        componentd_ = std::make_unique<IniFile>(
            unitDirectory + QStringLiteral("/hyprshelld-componentd.service")
        );
        surfaced_ = std::make_unique<IniFile>(
            unitDirectory + QStringLiteral("/hyprshelld-surfaced.service")
        );
        activation_ = std::make_unique<IniFile>(
            QStringLiteral(HYPRSHELLD_CONFIG1_ACTIVATION_FILE)
        );
        componentActivation_ = std::make_unique<IniFile>(
            QStringLiteral(HYPRSHELLD_COMPONENT_MANAGER1_ACTIVATION_FILE)
        );

        for (const auto *file : {
                 target_.get(),
                 slice_.get(),
                 componentHelpersSlice_.get(),
                 coordinator_.get(),
                 configd_.get(),
                 componentd_.get(),
                 surfaced_.get(),
                 activation_.get(),
                 componentActivation_.get(),
             }) {
            QVERIFY2(file->error().isEmpty(), qPrintable(file->error()));
        }
    }

    void targetOwnsRuntimeServices()
    {
        QCOMPARE(
            sorted(target_->words(QStringLiteral("Unit"), QStringLiteral("Wants"))),
            sorted({
                QStringLiteral("hyprshelld.service"),
                QStringLiteral("hyprshelld-configd.service"),
                QStringLiteral("hyprshelld-componentd.service"),
                QStringLiteral("hyprshelld-surfaced.service"),
            })
        );
        QCOMPARE(
            target_->words(QStringLiteral("Unit"), QStringLiteral("After")),
            QStringList{QStringLiteral("graphical-session.target")}
        );
        QCOMPARE(
            target_->words(QStringLiteral("Unit"), QStringLiteral("PartOf")),
            QStringList{QStringLiteral("graphical-session.target")}
        );
        QCOMPARE(
            target_->words(QStringLiteral("Install"), QStringLiteral("WantedBy")),
            QStringList{QStringLiteral("graphical-session.target")}
        );

        QVERIFY(target_->values(QStringLiteral("Unit"), QStringLiteral("Requires")).isEmpty());
        QVERIFY(target_->values(QStringLiteral("Unit"), QStringLiteral("BindsTo")).isEmpty());
        QVERIFY(target_->values(QStringLiteral("Unit"), QStringLiteral("Upholds")).isEmpty());
        QVERIFY(!target_->words(QStringLiteral("Unit"), QStringLiteral("Wants"))
                     .contains(QStringLiteral("hyprshelld-settings.service")));
    }

    void servicesUseBoundedSystemdRecovery()
    {
        for (const auto *service : {
                 coordinator_.get(),
                 configd_.get(),
                 componentd_.get(),
                 surfaced_.get(),
             }) {
            QCOMPARE(
                sorted(service->words(QStringLiteral("Unit"), QStringLiteral("PartOf"))),
                sorted({
                    QStringLiteral("graphical-session.target"),
                    QStringLiteral("hyprshelld.target"),
                })
            );
            QCOMPARE(
                service->words(QStringLiteral("Unit"), QStringLiteral("After")),
                QStringList{QStringLiteral("graphical-session.target")}
            );
            QCOMPARE(
                service->values(
                    QStringLiteral("Unit"),
                    QStringLiteral("StartLimitIntervalSec")
                ),
                QStringList{QStringLiteral("30s")}
            );
            QCOMPARE(
                service->values(
                    QStringLiteral("Unit"),
                    QStringLiteral("StartLimitBurst")
                ),
                QStringList{service == surfaced_.get()
                    ? QStringLiteral("10") : QStringLiteral("5")}
            );
            QCOMPARE(
                service->values(QStringLiteral("Service"), QStringLiteral("Slice")),
                QStringList{QStringLiteral("session-hyprshelld.slice")}
            );
            QCOMPARE(
                service->values(QStringLiteral("Service"), QStringLiteral("Restart")),
                QStringList{QStringLiteral("always")}
            );
            QCOMPARE(
                service->values(
                    QStringLiteral("Service"),
                    QStringLiteral("RestartMode")
                ),
                QStringList{QStringLiteral("direct")}
            );
            QCOMPARE(
                service->values(
                    QStringLiteral("Service"),
                    QStringLiteral("RestartSec")
                ),
                QStringList{service == surfaced_.get()
                    ? QStringLiteral("2s") : QStringLiteral("1s")}
            );
            QCOMPARE(
                service->values(
                    QStringLiteral("Service"),
                    QStringLiteral("TimeoutStopSec")
                ),
                QStringList{QStringLiteral("5s")}
            );
            QVERIFY(!service->hasSection(QStringLiteral("Install")));
            QVERIFY(service->values(QStringLiteral("Unit"), QStringLiteral("Requires")).isEmpty());
            QVERIFY(service->values(QStringLiteral("Unit"), QStringLiteral("Requisite")).isEmpty());
        }

        // A bad declarative process is quarantined after eight seconds. With
        // two-second retries, the safe-plan restart occurs well before the
        // surfaced unit can consume its ten-attempt, thirty-second budget.
        constexpr auto activationDeadlineSeconds = 8;
        constexpr auto surfacedRestartSeconds = 2;
        constexpr auto surfacedStartLimitBurst = 10;
        const auto startsThroughSafePlan =
            activationDeadlineSeconds / surfacedRestartSeconds + 2;
        QVERIFY(startsThroughSafePlan < surfacedStartLimitBurst);

        QCOMPARE(
            slice_->values(
                QStringLiteral("Unit"),
                QStringLiteral("StopWhenUnneeded")
            ),
            QStringList{QStringLiteral("yes")}
        );
        QCOMPARE(
            componentHelpersSlice_->values(
                QStringLiteral("Unit"),
                QStringLiteral("StopWhenUnneeded")
            ),
            QStringList{QStringLiteral("yes")}
        );
        QCOMPARE(
            componentHelpersSlice_->values(
                QStringLiteral("Slice"),
                QStringLiteral("MemoryMax")
            ),
            QStringList{QStringLiteral("512M")}
        );
        QCOMPARE(
            componentHelpersSlice_->values(
                QStringLiteral("Slice"),
                QStringLiteral("MemorySwapMax")
            ),
            QStringList{QStringLiteral("0")}
        );
        QCOMPARE(
            componentHelpersSlice_->values(
                QStringLiteral("Slice"),
                QStringLiteral("TasksMax")
            ),
            QStringList{QStringLiteral("32")}
        );
    }

    void serviceTypesAndCommandsMatchProcesses()
    {
        QCOMPARE(
            coordinator_->values(QStringLiteral("Service"), QStringLiteral("Type")),
            QStringList{QStringLiteral("dbus")}
        );
        QCOMPARE(
            coordinator_->values(QStringLiteral("Service"), QStringLiteral("BusName")),
            QStringList{QStringLiteral("org.hyprshelld.Coordinator1")}
        );
        QCOMPARE(
            coordinator_->words(QStringLiteral("Service"), QStringLiteral("ExecStart")),
            QStringList{QStringLiteral(HYPRSHELLD_INSTALL_COORDINATOR)}
        );

        QCOMPARE(
            configd_->values(QStringLiteral("Service"), QStringLiteral("Type")),
            QStringList{QStringLiteral("dbus")}
        );
        QCOMPARE(
            configd_->values(QStringLiteral("Service"), QStringLiteral("BusName")),
            QStringList{QStringLiteral("org.hyprshelld.Config1")}
        );
        QCOMPARE(
            configd_->words(QStringLiteral("Service"), QStringLiteral("ExecStart")),
            QStringList{QStringLiteral(HYPRSHELLD_INSTALL_CONFIGD)}
        );

        QCOMPARE(
            componentd_->values(QStringLiteral("Service"), QStringLiteral("Type")),
            QStringList{QStringLiteral("dbus")}
        );
        QCOMPARE(
            componentd_->values(QStringLiteral("Service"), QStringLiteral("BusName")),
            QStringList{QStringLiteral("org.hyprshelld.ComponentManager1")}
        );
        QCOMPARE(
            componentd_->words(QStringLiteral("Service"), QStringLiteral("ExecStart")),
            QStringList{QStringLiteral(HYPRSHELLD_INSTALL_COMPONENTD)}
        );

        QCOMPARE(
            surfaced_->values(QStringLiteral("Service"), QStringLiteral("Type")),
            QStringList{QStringLiteral("exec")}
        );
        QVERIFY(surfaced_->values(QStringLiteral("Service"), QStringLiteral("BusName")).isEmpty());
        QCOMPARE(
            surfaced_->words(QStringLiteral("Service"), QStringLiteral("ExecStart")),
            QStringList({
                QStringLiteral(HYPRSHELLD_QUICKSHELL_EXECUTABLE),
                QStringLiteral("--path"),
                QStringLiteral(HYPRSHELLD_INSTALL_SURFACED),
            })
        );
        QCOMPARE(
            sorted(surfaced_->words(
                QStringLiteral("Service"),
                QStringLiteral("Environment")
            )),
            sorted({
                QStringLiteral("QML_IMPORT_PATH=")
                    + QStringLiteral(HYPRSHELLD_INSTALL_QML),
                QStringLiteral("QS_DISABLE_CRASH_HANDLER=1"),
            })
        );
    }

    void configActivationMatchesServiceUnit()
    {
        QVERIFY(activation_->hasSection(QStringLiteral("D-BUS Service")));
        QCOMPARE(
            activation_->values(
                QStringLiteral("D-BUS Service"),
                QStringLiteral("Name")
            ),
            configd_->values(QStringLiteral("Service"), QStringLiteral("BusName"))
        );
        QCOMPARE(
            activation_->values(
                QStringLiteral("D-BUS Service"),
                QStringLiteral("SystemdService")
            ),
            QStringList{QStringLiteral("hyprshelld-configd.service")}
        );
        QCOMPARE(
            activation_->words(
                QStringLiteral("D-BUS Service"),
                QStringLiteral("Exec")
            ),
            configd_->words(QStringLiteral("Service"), QStringLiteral("ExecStart"))
        );
        QVERIFY(activation_->values(
                                   QStringLiteral("D-BUS Service"),
                                   QStringLiteral("User")
        ).isEmpty());
    }

    void componentActivationMatchesServiceUnit()
    {
        QVERIFY(componentActivation_->hasSection(QStringLiteral("D-BUS Service")));
        QCOMPARE(
            componentActivation_->values(
                QStringLiteral("D-BUS Service"),
                QStringLiteral("Name")
            ),
            componentd_->values(QStringLiteral("Service"), QStringLiteral("BusName"))
        );
        QCOMPARE(
            componentActivation_->values(
                QStringLiteral("D-BUS Service"),
                QStringLiteral("SystemdService")
            ),
            QStringList{QStringLiteral("hyprshelld-componentd.service")}
        );
        QCOMPARE(
            componentActivation_->words(
                QStringLiteral("D-BUS Service"),
                QStringLiteral("Exec")
            ),
            componentd_->words(QStringLiteral("Service"), QStringLiteral("ExecStart"))
        );
        QVERIFY(componentActivation_->values(
                                            QStringLiteral("D-BUS Service"),
                                            QStringLiteral("User")
        ).isEmpty());
    }

private:
    std::unique_ptr<IniFile> target_;
    std::unique_ptr<IniFile> slice_;
    std::unique_ptr<IniFile> componentHelpersSlice_;
    std::unique_ptr<IniFile> coordinator_;
    std::unique_ptr<IniFile> configd_;
    std::unique_ptr<IniFile> componentd_;
    std::unique_ptr<IniFile> surfaced_;
    std::unique_ptr<IniFile> activation_;
    std::unique_ptr<IniFile> componentActivation_;
};

QTEST_GUILESS_MAIN(SystemdUnitContractTest)

#include "systemd_unit_contract_test.moc"
