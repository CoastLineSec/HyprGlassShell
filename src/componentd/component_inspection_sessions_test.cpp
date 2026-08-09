#include "component_inspection_sessions.h"

#include "component/component_package_bundle.h"

#include <QCryptographicHash>
#include <QDBusConnectionInterface>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <utility>

using namespace HyprShelld;

namespace {

const QString owner = QStringLiteral(":1.42");
const QString otherOwner = QStringLiteral(":1.43");

QString sha256(const QByteArray &bytes)
{
    return QString::fromLatin1(
        QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex()
    );
}

bool writeFile(const QString &path, const QByteArray &bytes)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly | QIODevice::Truncate)
        && file.write(bytes) == bytes.size() && file.flush();
}

bool createRecognizedSessionDirectory(
    const QString &spoolRoot,
    const QString &token
)
{
    const auto directory = QDir(spoolRoot).filePath(token);
    return QDir().mkpath(directory)
        && writeFile(
            QDir(directory).filePath(
                QStringLiteral("package.hyprshelld-component")
            ),
            QByteArrayLiteral("stale")
        )
        && writeFile(
            QDir(directory).filePath(
                QStringLiteral("inspection-report.json")
            ),
            QByteArrayLiteral("stale")
        )
        && writeFile(
            QDir(directory).filePath(
                QStringLiteral("materialized.bundle")
            ),
            QByteArrayLiteral("stale")
        );
}

int openReadOnly(const QString &path)
{
    const auto encoded = QFile::encodeName(path);
    return ::open(encoded.constData(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
}

QVector<Components::ComponentPackageBundleFile> packageFiles()
{
    const QJsonObject manifest{
        {QStringLiteral("manifestVersion"), 1},
        {QStringLiteral("id"), QStringLiteral("com.example.clock")},
        {QStringLiteral("version"), QStringLiteral("1.0.0")},
        {QStringLiteral("type"), QStringLiteral("bar-widget")},
        {QStringLiteral("name"), QStringLiteral("Example Clock")},
        {QStringLiteral("description"), QStringLiteral("A test clock.")},
        {QStringLiteral("authors"), QJsonArray{QJsonObject{
             {QStringLiteral("name"), QStringLiteral("Example Author")},
         }}},
        {QStringLiteral("license"), QStringLiteral("MIT")},
        {QStringLiteral("componentApiVersion"), QStringLiteral("1.0")},
        {QStringLiteral("runtime"), QJsonObject{
             {QStringLiteral("kind"), QStringLiteral("qml-full-trust-v1")},
             {QStringLiteral("entrypoint"),
              QStringLiteral("payload/main.qml")},
         }},
    };
    return {
        {
            .path = QStringLiteral("integrity.json"),
            .contents = QByteArrayLiteral("{}\n"),
        },
        {
            .path = QStringLiteral("manifest.json"),
            .contents = QJsonDocument(manifest).toJson(QJsonDocument::Compact),
        },
        {
            .path = QStringLiteral("payload/main.qml"),
            .contents = QByteArrayLiteral("import QtQuick\nItem {}\n"),
        },
    };
}

QByteArray acceptedReport(
    const QString &token,
    const QString &archiveDigest,
    const quint64 archiveSize,
    QVector<Components::ComponentPackageBundleFile> &files
)
{
    files = packageFiles();
    const auto manifest = QJsonDocument::fromJson(files.at(1).contents)
                              .object();
    Components::PackageInspectionReport report{
        .inspectionToken = token,
        .archiveSha256 = archiveDigest,
        .packageDigest = Components::deriveComponentPackageDigest(files),
        .archiveSize = archiveSize,
        .expandedSize = 0,
        .manifest = {},
        .normalizedManifest = manifest,
        .normalizedSettingsSchema = std::nullopt,
        .files = {},
    };
    for (const auto &file : files) {
        report.expandedSize += static_cast<quint64>(file.contents.size());
        report.files.append({
            .path = file.path,
            .size = static_cast<quint64>(file.contents.size()),
            .sha256 = sha256(file.contents),
        });
    }
    return Components::serializePackageInspectionReport(report);
}

class FakeInspectorLauncher final : public ComponentInspectorLauncher {
public:
    using ComponentInspectorLauncher::ComponentInspectorLauncher;

    bool acceptLaunch = true;
    QList<ComponentInspectorLaunchRequest> requests;
    QStringList canceledTokens;
    QHash<QString, Completion> completions;

    bool start(
        const ComponentInspectorLaunchRequest &request,
        Completion completion,
        QString &error
    ) override
    {
        requests.append(request);
        if (!acceptLaunch) {
            error = QStringLiteral("fake launch rejection");
            return false;
        }
        completions.insert(request.token, std::move(completion));
        return true;
    }

    void cancel(const QString &token) override
    {
        canceledTokens.append(token);
    }

    bool writeAcceptedOutputs(
        const QString &token,
        const QString &reportToken = {},
        const QString &reportDigest = {}
    )
    {
        const auto request = requestFor(token);
        if (request.token.isEmpty()) {
            return false;
        }
        QVector<Components::ComponentPackageBundleFile> files;
        const auto report = acceptedReport(
            reportToken.isEmpty() ? request.token : reportToken,
            reportDigest.isEmpty() ? request.archiveDigest : reportDigest,
            static_cast<quint64>(QFileInfo(request.spoolPath).size()),
            files
        );
        QFile bundle(request.materializedPath);
        QString bundleError;
        if (!writeFile(request.reportPath, report)
            || !bundle.open(QIODevice::WriteOnly | QIODevice::Truncate)
            || !Components::writeComponentPackageBundle(
                bundle,
                files,
                bundleError
            )
            || !bundle.flush()) {
            return false;
        }
        return true;
    }

    void finish(
        const QString &token,
        ComponentInspectorLaunchResult result
    )
    {
        const auto completion = completions.value(token);
        if (completion) {
            completion(std::move(result));
        }
    }

    ComponentInspectorLaunchRequest requestFor(const QString &token) const
    {
        for (const auto &request : requests) {
            if (request.token == token) {
                return request;
            }
        }
        return {};
    }
};

struct TestInput final {
    QTemporaryDir directory;
    QString path;
    int descriptor = -1;

    ~TestInput()
    {
        if (descriptor >= 0) {
            ::close(descriptor);
        }
    }

    bool create(const QByteArray &bytes)
    {
        if (!directory.isValid()) {
            return false;
        }
        path = directory.filePath(QStringLiteral("package.bin"));
        if (!writeFile(path, bytes)) {
            return false;
        }
        descriptor = openReadOnly(path);
        return descriptor >= 0;
    }
};

} // namespace

class ComponentInspectionSessionsTest final : public QObject {
    Q_OBJECT

private slots:
    void liveSystemdSandboxQualification()
    {
        if (qEnvironmentVariable("HYPRSHELLD_RUN_LIVE_INSPECTOR_TEST")
            != QStringLiteral("1")) {
            QSKIP("Set HYPRSHELLD_RUN_LIVE_INSPECTOR_TEST=1 for the live user-manager gate");
        }

        auto connection = QDBusConnection::sessionBus();
        QVERIFY(connection.isConnected());
        QVERIFY(connection.interface() != nullptr);
        QVERIFY(connection.interface()->isServiceRegistered(
            QStringLiteral("org.freedesktop.systemd1")
        ));

        QTemporaryDir root;
        TestInput input;
        QVERIFY(root.isValid());
        QVERIFY(input.create(QByteArrayLiteral("not a zip archive\n")));
        auto launcher = std::make_unique<SystemdComponentInspectorLauncher>(
            connection,
            QStringLiteral(HYPRSHELLD_TEST_COMPONENT_INSPECTOR_EXECUTABLE),
            QStringLiteral("app.slice")
        );
        ComponentInspectionSessions sessions(
            root.filePath(QStringLiteral("spool")),
            std::move(launcher)
        );
        QSignalSpy finished(
            &sessions,
            &ComponentInspectionSessions::inspectionFinished
        );

        const auto begun = sessions.begin(owner, input.descriptor);
        QVERIFY2(begun.success, qPrintable(begun.errorMessage));
        QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 1, 15000);
        const auto completion = finished.takeFirst();
        QCOMPARE(completion.at(0).toString(), owner);
        QCOMPARE(completion.at(1).toString(), begun.token);
        const auto review = completion.at(2).toByteArray();
        const auto diagnostic = QStringLiteral("%1: %2; report=%3")
                                    .arg(
                                        completion.at(5).toString(),
                                        completion.at(6).toString(),
                                        QString::fromUtf8(review.left(1024))
                                    );
        QVERIFY2(review.contains("errorVersion"), qPrintable(diagnostic));
        QVERIFY(!completion.at(5).toString().isEmpty());
        QVERIFY(!sessions.lookup(owner, begun.token).success);
    }

    void startupRemovesOnlyRecognizedStaleSessionDirectories()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        const auto spoolRoot = root.filePath(QStringLiteral("spool"));
        const auto staleToken = QStringLiteral(
            "00112233445566778899aabbccddeeff"
        );
        const auto stalePath = QDir(spoolRoot).filePath(staleToken);
        const auto unrelatedPath = QDir(spoolRoot).filePath(
            QStringLiteral("do-not-touch")
        );
        QVERIFY(QDir().mkpath(unrelatedPath));
        QVERIFY(createRecognizedSessionDirectory(spoolRoot, staleToken));

        auto launcher = std::make_unique<FakeInspectorLauncher>();
        ComponentInspectionSessions sessions(
            spoolRoot,
            std::move(launcher),
            1000
        );
        QVERIFY(!QFileInfo::exists(stalePath));
        QVERIFY(QFileInfo::exists(unrelatedPath));

        struct stat lockStatus {};
        const auto lockPath = QFile::encodeName(
            QDir(spoolRoot).filePath(
                QStringLiteral(".component-inspections.lock")
            )
        );
        QCOMPARE(::lstat(lockPath.constData(), &lockStatus), 0);
        QVERIFY(S_ISREG(lockStatus.st_mode));
        QCOMPARE(lockStatus.st_uid, geteuid());
        QCOMPARE(lockStatus.st_mode & 0777, mode_t(0600));
    }

    void concurrentInstanceCannotPurgeOrStartUntilLockIsReleased()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        TestInput input;
        QVERIFY(input.create(QByteArrayLiteral("package")));
        const auto spoolRoot = root.filePath(QStringLiteral("spool"));

        auto firstLauncher = std::make_unique<FakeInspectorLauncher>();
        auto *firstFake = firstLauncher.get();
        auto first = std::make_unique<ComponentInspectionSessions>(
            spoolRoot,
            std::move(firstLauncher),
            5000
        );
        const auto live = first->begin(owner, input.descriptor);
        QVERIFY2(live.success, qPrintable(live.errorMessage));
        const auto liveDirectory = QFileInfo(
            firstFake->requestFor(live.token).spoolPath
        ).absolutePath();
        QVERIFY(QFileInfo::exists(liveDirectory));

        const auto staleToken = QStringLiteral(
            "ffeeddccbbaa99887766554433221100"
        );
        const auto staleDirectory = QDir(spoolRoot).filePath(staleToken);
        QVERIFY(createRecognizedSessionDirectory(spoolRoot, staleToken));

        {
            auto secondLauncher =
                std::make_unique<FakeInspectorLauncher>();
            auto *secondFake = secondLauncher.get();
            ComponentInspectionSessions second(
                spoolRoot,
                std::move(secondLauncher),
                5000
            );
            const auto rejected = second.begin(owner, input.descriptor);
            QVERIFY(!rejected.success);
            QVERIFY(rejected.errorName.endsWith(
                QStringLiteral("InspectionUnavailable")
            ));
            QCOMPARE(secondFake->requests.size(), 0);
            second.expireNow();
            second.cancelAllForSender(owner);
            QVERIFY(QFileInfo::exists(liveDirectory));
            QVERIFY(QFileInfo::exists(staleDirectory));
        }
        QVERIFY(QFileInfo::exists(liveDirectory));
        QVERIFY(QFileInfo::exists(staleDirectory));

        first.reset();
        QVERIFY(!QFileInfo::exists(liveDirectory));
        QVERIFY(QFileInfo::exists(staleDirectory));

        auto thirdLauncher = std::make_unique<FakeInspectorLauncher>();
        auto *thirdFake = thirdLauncher.get();
        ComponentInspectionSessions third(
            spoolRoot,
            std::move(thirdLauncher),
            5000
        );
        QVERIFY(!QFileInfo::exists(staleDirectory));
        const auto reacquired = third.begin(owner, input.descriptor);
        QVERIFY2(reacquired.success, qPrintable(reacquired.errorMessage));
        QCOMPARE(thirdFake->requests.size(), 1);
    }

    void copiesValidDescriptorAndConsumesAcceptedArtifacts()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        TestInput input;
        const QByteArray packageBytes("not-a-real-archive\0with-bytes", 29);
        QVERIFY(input.create(packageBytes));
        QCOMPARE(::lseek(input.descriptor, 5, SEEK_SET), off_t(5));

        auto launcher = std::make_unique<FakeInspectorLauncher>();
        auto *fake = launcher.get();
        qint64 now = 1000;
        ComponentInspectionSessions sessions(
            root.filePath(QStringLiteral("spool")),
            std::move(launcher),
            5000,
            [&now] { return now; }
        );
        QSignalSpy finished(
            &sessions,
            &ComponentInspectionSessions::inspectionFinished
        );

        const auto begun = sessions.begin(owner, input.descriptor);
        QVERIFY2(begun.success, qPrintable(begun.errorMessage));
        QVERIFY(QRegularExpression(QStringLiteral("^[0-9a-f]{32}$"))
                    .match(begun.token)
                    .hasMatch());
        QCOMPARE(begun.archiveDigest, sha256(packageBytes));
        QCOMPARE(begun.archiveSize, quint64(packageBytes.size()));
        QCOMPARE(begun.expiresAtMs, qint64(6000));
        QCOMPARE(::lseek(input.descriptor, 0, SEEK_CUR), off_t(5));
        QCOMPARE(fake->requests.size(), 1);

        const auto request = fake->requests.first();
        QFile copied(request.spoolPath);
        QVERIFY(copied.open(QIODevice::ReadOnly));
        QCOMPARE(copied.readAll(), packageBytes);
        struct stat status {};
        const auto encodedSpool = QFile::encodeName(request.spoolPath);
        QCOMPARE(::stat(encodedSpool.constData(), &status), 0);
        QCOMPARE(status.st_mode & 0777, mode_t(0600));

        const auto stolen = sessions.lookup(otherOwner, begun.token);
        QVERIFY(!stolen.success);
        QVERIFY(stolen.spoolPath.isEmpty());
        QVERIFY(stolen.materializedPath.isEmpty());
        const auto pending = sessions.lookup(owner, begun.token);
        QVERIFY(pending.success);
        QVERIFY(pending.state == ComponentInspectionState::Pending);
        QVERIFY(pending.spoolPath.isEmpty());

        QVERIFY(fake->writeAcceptedOutputs(begun.token));
        fake->finish(begun.token, {
            .success = true,
            .errorName = {},
            .errorMessage = {},
        });
        QCOMPARE(finished.size(), 1);
        const auto completion = finished.takeFirst();
        QCOMPARE(completion.at(0).toString(), owner);
        QCOMPARE(completion.at(1).toString(), begun.token);
        QVERIFY(!completion.at(2).toByteArray().isEmpty());
        QCOMPARE(completion.at(3).toString(), request.spoolPath);
        QCOMPARE(completion.at(4).toString(), request.materializedPath);
        QVERIFY(completion.at(5).toString().isEmpty());

        const auto complete = sessions.lookup(owner, begun.token);
        QVERIFY2(complete.success, qPrintable(complete.errorMessage));
        QVERIFY(complete.state == ComponentInspectionState::Complete);
        QCOMPARE(complete.archiveDigest, begun.archiveDigest);
        QCOMPARE(complete.spoolPath, request.spoolPath);
        QCOMPARE(complete.materializedPath, request.materializedPath);

        auto wrongOwner = sessions.takeForInstall(
            otherOwner,
            begun.token,
            begun.archiveDigest
        );
        QVERIFY(!wrongOwner.success);
        QVERIFY(!wrongOwner.artifact.has_value());
        auto wrongDigest = sessions.takeForInstall(
            owner,
            begun.token,
            QString(64, QLatin1Char('f'))
        );
        QVERIFY(!wrongDigest.success);
        QVERIFY(!wrongDigest.artifact.has_value());

        auto taken = sessions.takeForInstall(
            owner,
            begun.token,
            begun.archiveDigest
        );
        QVERIFY2(taken.success, qPrintable(taken.errorMessage));
        QVERIFY(taken.artifact.has_value());
        QCOMPARE(taken.artifact->token, begun.token);
        QCOMPARE(taken.artifact->archiveDigest, begun.archiveDigest);
        QCOMPARE(taken.artifact->spoolPath, request.spoolPath);
        QVERIFY(QFileInfo::exists(taken.artifact->materializedPath));
        const auto sessionDirectory = QFileInfo(request.spoolPath).absolutePath();
        const auto consumed = sessions.lookup(owner, begun.token);
        QVERIFY(!consumed.success);

        taken.artifact.reset();
        QVERIFY(!QFileInfo::exists(sessionDirectory));
    }

    void rejectsUnsafeAndOversizeDescriptors()
    {
        QTemporaryDir root;
        QTemporaryDir inputs;
        QVERIFY(root.isValid());
        QVERIFY(inputs.isValid());
        auto launcher = std::make_unique<FakeInspectorLauncher>();
        auto *fake = launcher.get();
        ComponentInspectionSessions sessions(
            root.filePath(QStringLiteral("spool")),
            std::move(launcher)
        );

        const auto writablePath = inputs.filePath(QStringLiteral("rw"));
        QVERIFY(writeFile(writablePath, QByteArrayLiteral("bytes")));
        const auto encodedWritable = QFile::encodeName(writablePath);
        const auto writable = ::open(
            encodedWritable.constData(),
            O_RDWR | O_CLOEXEC
        );
        QVERIFY(writable >= 0);
        const auto writableResult = sessions.begin(owner, writable);
        QVERIFY(!writableResult.success);
        ::close(writable);

        const auto emptyPath = inputs.filePath(QStringLiteral("empty"));
        QVERIFY(writeFile(emptyPath, {}));
        const auto empty = openReadOnly(emptyPath);
        QVERIFY(empty >= 0);
        QVERIFY(!sessions.begin(owner, empty).success);
        ::close(empty);

        const auto directory = openReadOnly(inputs.path());
        QVERIFY(directory >= 0);
        QVERIFY(!sessions.begin(owner, directory).success);
        ::close(directory);

        const auto hugePath = inputs.filePath(QStringLiteral("huge"));
        const auto encodedHuge = QFile::encodeName(hugePath);
        auto huge = ::open(
            encodedHuge.constData(),
            O_RDWR | O_CREAT | O_EXCL | O_CLOEXEC,
            0600
        );
        QVERIFY(huge >= 0);
        QCOMPARE(
            ::ftruncate(
                huge,
                Components::maximumComponentArchiveBytes + 1
            ),
            0
        );
        ::close(huge);
        huge = openReadOnly(hugePath);
        QVERIFY(huge >= 0);
        QVERIFY(!sessions.begin(owner, huge).success);
        ::close(huge);

        QCOMPARE(fake->requests.size(), 0);
    }

    void cancellationCallerLossAndExpiryDeleteOnlyOwnedSessions()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        TestInput input;
        QVERIFY(input.create(QByteArrayLiteral("package")));
        auto launcher = std::make_unique<FakeInspectorLauncher>();
        auto *fake = launcher.get();
        qint64 now = 100;
        ComponentInspectionSessions sessions(
            root.filePath(QStringLiteral("spool")),
            std::move(launcher),
            50,
            [&now] { return now; }
        );

        const auto first = sessions.begin(owner, input.descriptor);
        QVERIFY(first.success);
        const auto firstDirectory = QFileInfo(
            fake->requestFor(first.token).spoolPath
        ).absolutePath();
        QVERIFY(!sessions.cancel(otherOwner, first.token).success);
        QVERIFY(QFileInfo::exists(firstDirectory));
        QVERIFY(sessions.cancel(owner, first.token).success);
        QCOMPARE(fake->canceledTokens, QStringList{first.token});
        QVERIFY(!QFileInfo::exists(firstDirectory));

        // A late helper callback after cancellation cannot resurrect a token.
        fake->finish(first.token, {
            .success = true,
            .errorName = {},
            .errorMessage = {},
        });
        QVERIFY(!sessions.lookup(owner, first.token).success);

        const auto expiring = sessions.begin(owner, input.descriptor);
        QVERIFY(expiring.success);
        const auto expiringDirectory = QFileInfo(
            fake->requestFor(expiring.token).spoolPath
        ).absolutePath();
        now = expiring.expiresAtMs;
        sessions.expireNow();
        QVERIFY(!QFileInfo::exists(expiringDirectory));
        const auto expired = sessions.lookup(owner, expiring.token);
        QVERIFY(!expired.success);
        QVERIFY(expired.errorName.endsWith(QStringLiteral("InspectionExpired")));
        const auto expiredStolen = sessions.lookup(otherOwner, expiring.token);
        QVERIFY(!expiredStolen.success);
        QVERIFY(expiredStolen.errorName.endsWith(
            QStringLiteral("InspectionOwnerMismatch")
        ));

        now += 1;
        const auto ownedA = sessions.begin(owner, input.descriptor);
        const auto ownedB = sessions.begin(owner, input.descriptor);
        const auto other = sessions.begin(otherOwner, input.descriptor);
        QVERIFY(ownedA.success);
        QVERIFY(ownedB.success);
        QVERIFY(other.success);
        sessions.cancelAllForSender(owner);
        QVERIFY(!sessions.lookup(owner, ownedA.token).success);
        QVERIFY(!sessions.lookup(owner, ownedB.token).success);
        QVERIFY(sessions.lookup(otherOwner, other.token).success);
    }

    void helperFailureReportsReviewAndReleasesSenderQuota()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        TestInput input;
        QVERIFY(input.create(QByteArrayLiteral("bad package")));
        auto launcher = std::make_unique<FakeInspectorLauncher>();
        auto *fake = launcher.get();
        ComponentInspectionSessions sessions(
            root.filePath(QStringLiteral("spool")),
            std::move(launcher)
        );
        QSignalSpy finished(
            &sessions,
            &ComponentInspectionSessions::inspectionFinished
        );

        const auto begun = sessions.begin(owner, input.descriptor);
        QVERIFY(begun.success);
        const auto request = fake->requestFor(begun.token);
        const QByteArray review = QByteArrayLiteral(
            "{\"errorVersion\":1,\"errors\":[{\"code\":\"package.bad\"}]}\n"
        );
        QVERIFY(writeFile(request.reportPath, review));
        fake->finish(begun.token, {
            .success = false,
            .errorName = QStringLiteral(
                "org.hyprshelld.ComponentInspector.Error.HelperFailed"
            ),
            .errorMessage = QStringLiteral("validation failed"),
        });

        QCOMPARE(finished.size(), 1);
        const auto completion = finished.takeFirst();
        QCOMPARE(completion.at(2).toByteArray(), review);
        QVERIFY(completion.at(3).toString().isEmpty());
        QVERIFY(completion.at(4).toString().isEmpty());
        const auto failed = sessions.lookup(owner, begun.token);
        QVERIFY(!failed.success);
        QVERIFY(!sessions.takeForInstall(
            owner,
            begun.token,
            begun.archiveDigest
        ).success);
        QVERIFY(!QFileInfo::exists(QFileInfo(request.spoolPath).absolutePath()));

        const auto second = sessions.begin(owner, input.descriptor);
        QVERIFY(second.success);
        fake->finish(second.token, {
            .success = false,
            .errorName = {},
            .errorMessage = QStringLiteral("second failure"),
        });
        const auto third = sessions.begin(owner, input.descriptor);
        QVERIFY2(third.success, qPrintable(third.errorMessage));
    }

    void rejectsMismatchedReportIdentityAndSymlinkedOutput()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        TestInput input;
        QVERIFY(input.create(QByteArrayLiteral("package")));
        auto launcher = std::make_unique<FakeInspectorLauncher>();
        auto *fake = launcher.get();
        ComponentInspectionSessions sessions(
            root.filePath(QStringLiteral("spool")),
            std::move(launcher)
        );

        const auto mismatch = sessions.begin(owner, input.descriptor);
        QVERIFY(mismatch.success);
        QVERIFY(fake->writeAcceptedOutputs(
            mismatch.token,
            QStringLiteral("0123456789abcdef0123456789abcdef")
        ));
        fake->finish(mismatch.token, {
            .success = true,
            .errorName = {},
            .errorMessage = {},
        });
        const auto mismatchState = sessions.lookup(owner, mismatch.token);
        QVERIFY(!mismatchState.success);

        const auto linked = sessions.begin(owner, input.descriptor);
        QVERIFY(linked.success);
        const auto request = fake->requestFor(linked.token);
        QVector<Components::ComponentPackageBundleFile> files;
        const auto report = acceptedReport(
            linked.token,
            linked.archiveDigest,
            linked.archiveSize,
            files
        );
        QFile::remove(request.reportPath);
        const auto outside = root.filePath(QStringLiteral("outside"));
        QVERIFY(writeFile(outside, report));
        const auto encodedOutside = QFile::encodeName(outside);
        const auto encodedReport = QFile::encodeName(request.reportPath);
        QCOMPARE(
            ::symlink(encodedOutside.constData(), encodedReport.constData()),
            0
        );
        QFile bundle(request.materializedPath);
        QString bundleError;
        QVERIFY(bundle.open(QIODevice::WriteOnly | QIODevice::Truncate));
        QVERIFY(Components::writeComponentPackageBundle(
            bundle,
            files,
            bundleError
        ));
        bundle.close();
        fake->finish(linked.token, {
            .success = true,
            .errorName = {},
            .errorMessage = {},
        });
        const auto linkedState = sessions.lookup(owner, linked.token);
        QVERIFY(!linkedState.success);
        QFile outsideFile(outside);
        QVERIFY(outsideFile.open(QIODevice::ReadOnly));
        QCOMPARE(outsideFile.readAll(), report);
    }

    void enforcesFixedTokenUnitNamesWithoutLaunchingSystemd()
    {
        const QString token = QStringLiteral(
            "0123456789abcdef0123456789abcdef"
        );
        QCOMPARE(
            SystemdComponentInspectorLauncher::unitNameForToken(token),
            QStringLiteral(
                "hyprshelld-component-inspect-0123456789abcdef0123456789abcdef.service"
            )
        );
        QVERIFY(SystemdComponentInspectorLauncher::unitNameForToken(
            QStringLiteral("ABC")
        ).isEmpty());
        QVERIFY(SystemdComponentInspectorLauncher::unitNameForToken(
            QStringLiteral("../../evil")
        ).isEmpty());

        const ComponentInspectorLaunchRequest request{
            .token = token,
            .archiveDigest = QString(64, QLatin1Char('a')),
            .spoolPath = QStringLiteral("/private/package"),
            .reportPath = QStringLiteral("/private/result"),
            .materializedPath = QStringLiteral("/private/materialized"),
        };
        const auto contract =
            SystemdComponentInspectorLauncher::sandboxContractForTesting(
                request,
                QStringLiteral("/usr/libexec/hyprshelld-component-inspector")
            );
        QCOMPARE(
            contract.startTransientArgumentSignature,
            QByteArrayLiteral("ssa(sv)a(sa(sv))")
        );
        QCOMPARE(
            contract.arguments,
            QStringList({
                QStringLiteral(
                    "/usr/libexec/hyprshelld-component-inspector"
                ),
                QStringLiteral("--inspect"),
                QStringLiteral(
                    "--expected-token=0123456789abcdef0123456789abcdef"
                ),
                QStringLiteral("--expected-archive-digest=%1")
                    .arg(QString(64, QLatin1Char('a'))),
            })
        );
        QCOMPARE(
            contract.propertySignatures.value(QStringLiteral("ExecStart")),
            QByteArrayLiteral("a(sasb)")
        );
        QCOMPARE(
            contract.propertySignatures.value(QStringLiteral("OpenFile")),
            QByteArrayLiteral("a(sst)")
        );
        QCOMPARE(
            contract.propertySignatures.value(QStringLiteral("PrivatePIDs")),
            QByteArrayLiteral("s")
        );
        QCOMPARE(
            contract.propertyValues.value(QStringLiteral("PrivatePIDs"))
                .toString(),
            QStringLiteral("yes")
        );
        QCOMPARE(
            contract.propertyValues.value(QStringLiteral("Type")).toString(),
            QStringLiteral("oneshot")
        );
        const auto unsetEnvironment = contract.propertyValues
                                          .value(QStringLiteral("UnsetEnvironment"))
                                          .toStringList();
        QVERIFY(unsetEnvironment.contains(QStringLiteral("LD_AUDIT")));
        QVERIFY(
            unsetEnvironment.contains(QStringLiteral("LD_DEBUG_OUTPUT"))
        );
        QVERIFY(unsetEnvironment.contains(QStringLiteral("LD_PRELOAD")));
        QCOMPARE(
            contract.propertySignatures.value(
                QStringLiteral("PrivateUsers")
            ),
            QByteArrayLiteral("b")
        );
        QCOMPARE(
            contract.propertySignatures.value(
                QStringLiteral("ProtectControlGroups")
            ),
            QByteArrayLiteral("b")
        );
        QCOMPARE(
            contract.propertySignatures.value(
                QStringLiteral("BindReadOnlyPaths")
            ),
            QByteArrayLiteral("a(ssbt)")
        );
        const QMap<QString, QString> expectedReadOnlyBindPaths{{
            QStringLiteral("/usr/libexec/hyprshelld-component-inspector"),
            QStringLiteral("/usr/libexec/hyprshelld-component-inspector"),
        }};
        QCOMPARE(contract.readOnlyBindPaths, expectedReadOnlyBindPaths);
        QCOMPARE(
            contract.propertySignatures.value(
                QStringLiteral("RestrictAddressFamilies")
            ),
            QByteArrayLiteral("(bas)")
        );
        QCOMPARE(
            contract.propertySignatures.value(
                QStringLiteral("SystemCallFilter")
            ),
            QByteArrayLiteral("(bas)")
        );
        QCOMPARE(
            contract.propertySignatures.value(
                QStringLiteral("RestrictNamespaces")
            ),
            QByteArrayLiteral("t")
        );
        QCOMPARE(
            contract.propertySignatures.value(QStringLiteral("UMask")),
            QByteArrayLiteral("u")
        );
        QVERIFY(!contract.propertySignatures.contains(
            QStringLiteral("LimitNPROC")
        ));
        QVERIFY(!contract.propertySignatures.contains(
            QStringLiteral("RuntimeMaxUSec")
        ));
        QCOMPARE(
            contract.propertySignatures.value(QStringLiteral("TasksMax")),
            QByteArrayLiteral("t")
        );
        QCOMPARE(
            contract.openFilePaths.value(QStringLiteral("package")),
            request.spoolPath
        );
        QCOMPARE(
            contract.openFilePaths.value(QStringLiteral("result")),
            request.reportPath
        );
        QCOMPARE(
            contract.openFilePaths.value(QStringLiteral("materialized")),
            request.materializedPath
        );
        QCOMPARE(
            contract.openFileFlags.value(QStringLiteral("package")),
            quint64(1U << 0)
        );
        QCOMPARE(
            contract.openFileFlags.value(QStringLiteral("result")),
            quint64(1U << 2)
        );
        QCOMPARE(
            contract.openFileFlags.value(QStringLiteral("materialized")),
            quint64(1U << 2)
        );
    }
};

QTEST_GUILESS_MAIN(ComponentInspectionSessionsTest)

#include "component_inspection_sessions_test.moc"
