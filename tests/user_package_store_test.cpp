#include "component/component_package_bundle.h"
#include "component/package_inspection_report.h"
#include "component/strict_json.h"
#include "componentd/user_package_store.h"

#include <QBuffer>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QtTest>

#include <algorithm>
#include <memory>

using namespace HyprShelld::Components;

namespace {

struct PackageFixture final {
    PackageInspectionReport report;
    QVector<ComponentPackageBundleFile> files;
    QByteArray bundle;
};

QString sha256(const QByteArray &bytes)
{
    return QString::fromLatin1(
        QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex()
    );
}

PackageFixture makePackage(
    const QString &componentId,
    const QString &version,
    const QByteArray &payload,
    const QString &dependencyId = {}
)
{
    QJsonObject manifest{
        {QStringLiteral("manifestVersion"), 1},
        {QStringLiteral("id"), componentId},
        {QStringLiteral("version"), version},
        {QStringLiteral("type"), QStringLiteral("bar-widget")},
        {QStringLiteral("name"), QStringLiteral("Fixture component")},
        {QStringLiteral("description"),
         QStringLiteral("A package-store transaction fixture.")},
        {QStringLiteral("authors"), QJsonArray{
             QJsonObject{{QStringLiteral("name"), QStringLiteral("Fixture")}},
         }},
        {QStringLiteral("license"), QStringLiteral("MIT")},
        {QStringLiteral("componentApiVersion"), QStringLiteral("1.0")},
        {QStringLiteral("runtime"), QJsonObject{
             {QStringLiteral("kind"), QStringLiteral("declarative-v1")},
             {QStringLiteral("entrypoint"),
              QStringLiteral("payload/widget.json")},
         }},
        {QStringLiteral("requestedCapabilities"), QJsonArray{}},
    };
    if (!dependencyId.isEmpty()) {
        manifest.insert(
            QStringLiteral("dependencies"),
            QJsonArray{QJsonObject{
                {QStringLiteral("id"), dependencyId},
                {QStringLiteral("version"), QStringLiteral(">=1.0.0")},
            }}
        );
    }

    const auto manifestBytes = QJsonDocument(manifest).toJson(
        QJsonDocument::Compact
    );
    QJsonObject integrityFiles{
        {QStringLiteral("manifest.json"), sha256(manifestBytes)},
        {QStringLiteral("payload/widget.json"), sha256(payload)},
    };
    const auto integrityBytes = QJsonDocument(QJsonObject{
        {QStringLiteral("integrityVersion"), 1},
        {QStringLiteral("algorithm"), QStringLiteral("sha256")},
        {QStringLiteral("files"), integrityFiles},
    }).toJson(QJsonDocument::Compact);

    QVector<ComponentPackageBundleFile> files{
        {QStringLiteral("manifest.json"), manifestBytes},
        {QStringLiteral("integrity.json"), integrityBytes},
        {QStringLiteral("payload/widget.json"), payload},
    };
    std::ranges::sort(files, {}, &ComponentPackageBundleFile::path);

    const auto parsedManifest = parseComponentManifest(
        QByteArrayView(manifestBytes),
        ComponentOrigin::User
    );
    const auto parsedManifestObject = parseStrictJsonObject(
        QByteArrayView(manifestBytes),
        {.maximumBytes = 128 * 1024, .maximumDepth = 32}
    );
    Q_ASSERT(parsedManifest);
    Q_ASSERT(parsedManifestObject);

    PackageInspectionReport report{
        .inspectionToken = QString(32, QLatin1Char('a')),
        .archiveSha256 = QString(64, QLatin1Char('b')),
        .packageDigest = deriveComponentPackageDigest(files),
        .archiveSize = 1024,
        .manifest = *parsedManifest.value,
        .normalizedManifest = *parsedManifestObject.value,
    };
    for (const auto &file : files) {
        report.expandedSize += static_cast<quint64>(file.contents.size());
        report.files.append({
            .path = file.path,
            .size = static_cast<quint64>(file.contents.size()),
            .sha256 = sha256(file.contents),
        });
    }
    Q_ASSERT(parsePackageInspectionReport(
        serializePackageInspectionReport(report)
    ));

    QByteArray bundleBytes;
    QBuffer bundle(&bundleBytes);
    Q_ASSERT(bundle.open(QIODevice::WriteOnly));
    QString error;
    Q_ASSERT(writeComponentPackageBundle(bundle, files, error));
    bundle.close();
    return {std::move(report), std::move(files), std::move(bundleBytes)};
}

PackageInspectionReport makeLargeDeclaredReport(const QString &componentId)
{
    auto fixture = makePackage(
        componentId,
        QStringLiteral("1.0.0"),
        QByteArrayLiteral("not-materialized")
    );
    auto payload = std::ranges::find_if(
        fixture.report.files,
        [](const InspectedPackageFile &file) {
            return file.path == QStringLiteral("payload/widget.json");
        }
    );
    Q_ASSERT(payload != fixture.report.files.end());
    const auto metadataSize = fixture.report.expandedSize - payload->size;
    Q_ASSERT(metadataSize < static_cast<quint64>(maximumComponentFileBytes));
    payload->size = static_cast<quint64>(maximumComponentFileBytes)
        - metadataSize;
    fixture.report.expandedSize = maximumComponentFileBytes;
    Q_ASSERT(parsePackageInspectionReport(
        serializePackageInspectionReport(fixture.report)
    ));
    return std::move(fixture.report);
}

bool writeBundle(const QString &path, const QByteArray &bytes)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly | QIODevice::Truncate)
        && file.write(bytes) == bytes.size() && file.flush();
}

bool writePrivateFile(const QString &path, const QByteArray &bytes)
{
    if (!QDir().mkpath(QFileInfo(path).absolutePath())) {
        return false;
    }
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)
        || file.write(bytes) != bytes.size() || !file.flush()) {
        return false;
    }
    file.close();
    return QFile::setPermissions(
        path,
        QFileDevice::ReadOwner | QFileDevice::WriteOwner
    );
}

QString installedRoot(
    const QString &dataRoot,
    const QString &componentId,
    const QString &version
);

bool seedInstalledPackage(
    const QString &dataRoot,
    const QString &stateRoot,
    const PackageFixture &fixture
)
{
    const auto root = installedRoot(
        dataRoot,
        fixture.report.manifest.id,
        fixture.report.manifest.version
    );
    for (const auto &file : fixture.files) {
        if (!writePrivateFile(QDir(root).filePath(file.path), file.contents)) {
            return false;
        }
    }
    return writePrivateFile(
        QDir(stateRoot).filePath(
            QStringLiteral("receipts/") + fixture.report.manifest.id
                + QStringLiteral(".json")
        ),
        serializePackageInspectionReport(fixture.report)
    );
}

bool seedInstalledTree(
    const QString &dataRoot,
    const PackageFixture &fixture
)
{
    const auto root = installedRoot(
        dataRoot,
        fixture.report.manifest.id,
        fixture.report.manifest.version
    );
    for (const auto &file : fixture.files) {
        if (!writePrivateFile(QDir(root).filePath(file.path), file.contents)) {
            return false;
        }
    }
    return true;
}

QString installedRoot(
    const QString &dataRoot,
    const QString &componentId,
    const QString &version
)
{
    return QDir(dataRoot).filePath(
        componentId + QStringLiteral("/versions/") + version
    );
}

} // namespace

class UserPackageStoreTest final : public QObject {
    Q_OBJECT

private slots:
    void lifetimeLeaseExcludesSecondStore()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        const auto dataRoot = root.filePath(QStringLiteral("data"));
        const auto stateRoot = root.filePath(QStringLiteral("state"));
        auto first = std::make_unique<UserPackageStore>(
            dataRoot,
            stateRoot
        );
        QVERIFY2(first->load().ok(), qPrintable(first->load().error));
        const QFileInfo lease(QDir(stateRoot).filePath(
            QStringLiteral(".package-store.lock")
        ));
        QVERIFY(lease.isFile());
        QVERIFY(!lease.isSymLink());
        QVERIFY(lease.permissions() & QFileDevice::ReadOwner);
        QVERIFY(lease.permissions() & QFileDevice::WriteOwner);
        QVERIFY(!(lease.permissions() & (
            QFileDevice::ExeOwner | QFileDevice::ReadGroup
            | QFileDevice::WriteGroup | QFileDevice::ExeGroup
            | QFileDevice::ReadOther | QFileDevice::WriteOther
            | QFileDevice::ExeOther
        )));

        UserPackageStore excluded(dataRoot, stateRoot);
        const auto excludedLoad = excluded.load();
        QVERIFY(!excludedLoad.ok());
        QVERIFY(excludedLoad.error.contains(QStringLiteral("already owned")));
        QString recoveryError;
        QVERIFY(!excluded.recover(recoveryError));
        auto fixture = makePackage(
            QStringLiteral("org.example.clock"),
            QStringLiteral("1.0.0"),
            QByteArrayLiteral("lease")
        );
        const auto bundlePath = root.filePath(QStringLiteral("bundle.bin"));
        QVERIFY(writeBundle(bundlePath, fixture.bundle));
        QVERIFY(!excluded.install(fixture.report, bundlePath).success);
        QVERIFY(!excluded.remove(
            fixture.report.manifest.id,
            fixture.report.packageDigest
        ).success);

        first.reset();
        UserPackageStore replacement(dataRoot, stateRoot);
        const auto installed = replacement.install(
            fixture.report,
            bundlePath
        );
        QVERIFY2(installed.success, qPrintable(installed.errorMessage));
    }

    void installLoadAndIdempotence()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        const auto dataRoot = root.filePath(QStringLiteral("data"));
        const auto stateRoot = root.filePath(QStringLiteral("state"));
        UserPackageStore store(dataRoot, stateRoot);
        auto fixture = makePackage(
            QStringLiteral("org.example.clock"),
            QStringLiteral("1.0.0"),
            QByteArrayLiteral("{\"text\":\"12:34\"}\n")
        );
        const auto bundlePath = root.filePath(QStringLiteral("bundle.bin"));
        QVERIFY(writeBundle(bundlePath, fixture.bundle));

        const auto installed = store.install(fixture.report, bundlePath);
        QVERIFY2(installed.success, qPrintable(installed.errorMessage));
        QCOMPARE(installed.componentId, QStringLiteral("org.example.clock"));
        QCOMPARE(installed.packageDigest, fixture.report.packageDigest);

        const auto loaded = store.load();
        QVERIFY2(loaded.ok(), qPrintable(loaded.error));
        QCOMPARE(loaded.entries.size(), 1);
        QCOMPARE(loaded.entries.first().manifest.id, installed.componentId);
        QVERIFY(
            loaded.entries.first().manifest.origin == ComponentOrigin::User
        );
        QCOMPARE(loaded.entries.first().packageDigest, installed.packageDigest);

        const QFileInfo payload(QDir(installedRoot(
            dataRoot,
            installed.componentId,
            QStringLiteral("1.0.0")
        )).filePath(QStringLiteral("payload/widget.json")));
        QVERIFY(payload.isFile());
        QVERIFY(!payload.isSymLink());
        QVERIFY(!(payload.permissions() & QFileDevice::ExeOwner));
        QVERIFY(!(payload.permissions() & QFileDevice::ReadGroup));

        const auto repeated = store.install(fixture.report, bundlePath);
        QVERIFY2(repeated.success, qPrintable(repeated.errorMessage));
        QCOMPARE(store.load().entries.size(), 1);
    }

    void updateRetainsOnlyCommittedVersionAndPublishesNewReceipt()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        const auto dataRoot = root.filePath(QStringLiteral("data"));
        UserPackageStore store(
            dataRoot,
            root.filePath(QStringLiteral("state"))
        );
        auto first = makePackage(
            QStringLiteral("org.example.clock"),
            QStringLiteral("1.0.0"),
            QByteArrayLiteral("one")
        );
        auto second = makePackage(
            QStringLiteral("org.example.clock"),
            QStringLiteral("2.0.0"),
            QByteArrayLiteral("two")
        );
        auto third = makePackage(
            QStringLiteral("org.example.clock"),
            QStringLiteral("3.0.0"),
            QByteArrayLiteral("three")
        );
        const auto firstBundle = root.filePath(QStringLiteral("first.bin"));
        const auto secondBundle = root.filePath(QStringLiteral("second.bin"));
        const auto thirdBundle = root.filePath(QStringLiteral("third.bin"));
        QVERIFY(writeBundle(firstBundle, first.bundle));
        QVERIFY(writeBundle(secondBundle, second.bundle));
        QVERIFY(writeBundle(thirdBundle, third.bundle));
        QVERIFY(store.install(first.report, firstBundle).success);
        QVERIFY(store.install(second.report, secondBundle).success);
        QVERIFY(!QFileInfo::exists(installedRoot(
            dataRoot,
            QStringLiteral("org.example.clock"),
            QStringLiteral("1.0.0")
        )));
        QVERIFY(store.install(third.report, thirdBundle).success);

        const auto loaded = store.load();
        QVERIFY2(loaded.ok(), qPrintable(loaded.error));
        QCOMPARE(loaded.entries.size(), 1);
        QCOMPARE(loaded.entries.first().manifest.version, QStringLiteral("3.0.0"));
        QCOMPARE(loaded.entries.first().packageDigest, third.report.packageDigest);
        QVERIFY(!QFileInfo::exists(installedRoot(
            dataRoot,
            QStringLiteral("org.example.clock"),
            QStringLiteral("2.0.0")
        )));
        QVERIFY(QFileInfo::exists(installedRoot(
            dataRoot,
            QStringLiteral("org.example.clock"),
            QStringLiteral("3.0.0")
        )));
    }

    void exactReinstallRepairsCorruptInstalledTree()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        const auto dataRoot = root.filePath(QStringLiteral("data"));
        UserPackageStore store(
            dataRoot,
            root.filePath(QStringLiteral("state"))
        );
        auto fixture = makePackage(
            QStringLiteral("org.example.clock"),
            QStringLiteral("1.0.0"),
            QByteArrayLiteral("reviewed-content")
        );
        const auto bundlePath = root.filePath(QStringLiteral("bundle.bin"));
        QVERIFY(writeBundle(bundlePath, fixture.bundle));
        QVERIFY(store.install(fixture.report, bundlePath).success);

        const auto payloadPath = QDir(installedRoot(
            dataRoot,
            fixture.report.manifest.id,
            fixture.report.manifest.version
        )).filePath(QStringLiteral("payload/widget.json"));
        QVERIFY(writePrivateFile(payloadPath, QByteArrayLiteral("bad-content")));
        const auto quarantined = store.load();
        QVERIFY2(quarantined.ok(), qPrintable(quarantined.error));
        QVERIFY(quarantined.entries.isEmpty());
        QVERIFY(!quarantined.warnings.isEmpty());

        const auto repaired = store.install(fixture.report, bundlePath);
        QVERIFY2(repaired.success, qPrintable(repaired.errorMessage));
        const auto loaded = store.load();
        QVERIFY2(loaded.ok(), qPrintable(loaded.error));
        QCOMPARE(loaded.entries.size(), 1);
        QCOMPARE(loaded.entries.first().packageDigest, fixture.report.packageDigest);
        QFile payload(payloadPath);
        QVERIFY(payload.open(QIODevice::ReadOnly));
        QCOMPARE(payload.readAll(), QByteArrayLiteral("reviewed-content"));
    }

    void exactReinstallRepairsCorruptReceipt()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        const auto stateRoot = root.filePath(QStringLiteral("state"));
        UserPackageStore store(
            root.filePath(QStringLiteral("data")),
            stateRoot
        );
        auto fixture = makePackage(
            QStringLiteral("org.example.clock"),
            QStringLiteral("1.0.0"),
            QByteArrayLiteral("reviewed-content")
        );
        const auto bundlePath = root.filePath(QStringLiteral("bundle.bin"));
        QVERIFY(writeBundle(bundlePath, fixture.bundle));
        QVERIFY(store.install(fixture.report, bundlePath).success);

        const auto receiptPath = QDir(stateRoot).filePath(
            QStringLiteral("receipts/org.example.clock.json")
        );
        QVERIFY(writePrivateFile(receiptPath, QByteArrayLiteral("{broken")));
        const auto quarantined = store.load();
        QVERIFY2(quarantined.ok(), qPrintable(quarantined.error));
        QVERIFY(quarantined.entries.isEmpty());
        QCOMPARE(quarantined.receiptCount, 1);

        const auto repaired = store.install(fixture.report, bundlePath);
        QVERIFY2(repaired.success, qPrintable(repaired.errorMessage));
        const auto loaded = store.load();
        QVERIFY2(loaded.ok(), qPrintable(loaded.error));
        QCOMPARE(loaded.entries.size(), 1);
        QCOMPARE(loaded.entries.first().manifest.id, fixture.report.manifest.id);
        QCOMPARE(loaded.entries.first().packageDigest, fixture.report.packageDigest);
    }

    void explicitRecoveryCleansTransactionsAndUnreferencedVersions()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        const auto dataRoot = root.filePath(QStringLiteral("data"));
        const auto stateRoot = root.filePath(QStringLiteral("state"));
        UserPackageStore store(dataRoot, stateRoot);
        auto current = makePackage(
            QStringLiteral("org.example.clock"),
            QStringLiteral("2.0.0"),
            QByteArrayLiteral("current")
        );
        auto old = makePackage(
            QStringLiteral("org.example.clock"),
            QStringLiteral("1.0.0"),
            QByteArrayLiteral("old")
        );
        auto orphan = makePackage(
            QStringLiteral("org.example.orphan"),
            QStringLiteral("1.0.0"),
            QByteArrayLiteral("orphan")
        );
        auto corruptReceipt = makePackage(
            QStringLiteral("org.example.repairable"),
            QStringLiteral("1.0.0"),
            QByteArrayLiteral("preserve-for-repair")
        );
        const auto bundlePath = root.filePath(QStringLiteral("current.bin"));
        QVERIFY(writeBundle(bundlePath, current.bundle));
        QVERIFY(store.install(current.report, bundlePath).success);
        QVERIFY(seedInstalledTree(dataRoot, old));
        QVERIFY(seedInstalledTree(dataRoot, orphan));
        QVERIFY(seedInstalledTree(dataRoot, corruptReceipt));
        QVERIFY(writePrivateFile(
            QDir(stateRoot).filePath(
                QStringLiteral("receipts/org.example.repairable.json")
            ),
            QByteArrayLiteral("{broken")
        ));
        const auto staleStaging = QDir(dataRoot).filePath(
            QStringLiteral(".staging/stale/file")
        );
        const auto staleDataTrash = QDir(dataRoot).filePath(
            QStringLiteral(".trash/stale/file")
        );
        const auto staleStateTrash = QDir(stateRoot).filePath(
            QStringLiteral(".trash/stale/file")
        );
        QVERIFY(writePrivateFile(staleStaging, QByteArrayLiteral("stale")));
        QVERIFY(writePrivateFile(staleDataTrash, QByteArrayLiteral("stale")));
        QVERIFY(writePrivateFile(staleStateTrash, QByteArrayLiteral("stale")));

        QString error;
        QVERIFY2(store.recover(error), qPrintable(error));
        QVERIFY(!QFileInfo::exists(staleStaging));
        QVERIFY(!QFileInfo::exists(staleDataTrash));
        QVERIFY(!QFileInfo::exists(staleStateTrash));
        QVERIFY(QFileInfo::exists(installedRoot(
            dataRoot,
            current.report.manifest.id,
            current.report.manifest.version
        )));
        QVERIFY(!QFileInfo::exists(installedRoot(
            dataRoot,
            old.report.manifest.id,
            old.report.manifest.version
        )));
        QVERIFY(!QFileInfo::exists(QDir(dataRoot).filePath(
            QStringLiteral("org.example.orphan/versions")
        )));
        QVERIFY(QFileInfo::exists(installedRoot(
            dataRoot,
            corruptReceipt.report.manifest.id,
            corruptReceipt.report.manifest.version
        )));
    }

    void recoveryIsolatesUnsafePerComponentVersionRoots()
    {
        QTemporaryDir root;
        QTemporaryDir outside;
        QVERIFY(root.isValid());
        QVERIFY(outside.isValid());
        const auto dataRoot = root.filePath(QStringLiteral("data"));
        const auto stateRoot = root.filePath(QStringLiteral("state"));
        UserPackageStore store(dataRoot, stateRoot);
        auto healthy = makePackage(
            QStringLiteral("org.example.healthy"),
            QStringLiteral("1.0.0"),
            QByteArrayLiteral("healthy")
        );
        const auto bundlePath = root.filePath(QStringLiteral("healthy.bin"));
        QVERIFY(writeBundle(bundlePath, healthy.bundle));
        QVERIFY(store.install(healthy.report, bundlePath).success);

        const auto badFileRoot = QDir(dataRoot).filePath(
            QStringLiteral("org.example.bad-file")
        );
        QVERIFY(QDir().mkpath(badFileRoot));
        QVERIFY(writePrivateFile(
            QDir(badFileRoot).filePath(QStringLiteral("versions")),
            QByteArrayLiteral("not-a-directory")
        ));
        const auto outsideSentinel = outside.filePath(
            QStringLiteral("sentinel")
        );
        QVERIFY(writePrivateFile(outsideSentinel, QByteArrayLiteral("keep")));
        const auto badLinkRoot = QDir(dataRoot).filePath(
            QStringLiteral("org.example.bad-link")
        );
        QVERIFY(QDir().mkpath(badLinkRoot));
        QVERIFY(QFile::link(
            outside.path(),
            QDir(badLinkRoot).filePath(QStringLiteral("versions"))
        ));

        QString error;
        QStringList warnings;
        QVERIFY2(store.recover(error, &warnings), qPrintable(error));
        QVERIFY(warnings.size() >= 2);
        QFile sentinel(outsideSentinel);
        QVERIFY(sentinel.open(QIODevice::ReadOnly));
        QCOMPARE(sentinel.readAll(), QByteArrayLiteral("keep"));
        const auto loaded = store.load();
        QVERIFY2(loaded.ok(), qPrintable(loaded.error));
        QCOMPARE(loaded.entries.size(), 1);
        QCOMPARE(
            loaded.entries.first().manifest.id,
            healthy.report.manifest.id
        );
    }

    void componentRootSymlinkFailsClosedForLifecycleOperations()
    {
        QTemporaryDir root;
        QTemporaryDir outside;
        QVERIFY(root.isValid());
        QVERIFY(outside.isValid());
        const auto dataRoot = root.filePath(QStringLiteral("data"));
        const auto stateRoot = root.filePath(QStringLiteral("state"));
        auto fixture = makePackage(
            QStringLiteral("org.example.clock"),
            QStringLiteral("1.0.0"),
            QByteArrayLiteral("outside-package")
        );
        const auto outsideComponent = outside.filePath(
            QStringLiteral("component")
        );
        for (const auto &file : fixture.files) {
            QVERIFY(writePrivateFile(
                QDir(outsideComponent).filePath(
                    QStringLiteral("versions/1.0.0/") + file.path
                ),
                file.contents
            ));
        }
        const auto sentinelPath = QDir(outsideComponent).filePath(
            QStringLiteral("sentinel")
        );
        QVERIFY(writePrivateFile(sentinelPath, QByteArrayLiteral("keep")));
        QVERIFY(QDir().mkpath(dataRoot));
        QVERIFY(QFile::link(
            outsideComponent,
            QDir(dataRoot).filePath(fixture.report.manifest.id)
        ));
        QVERIFY(writePrivateFile(
            QDir(stateRoot).filePath(
                QStringLiteral("receipts/org.example.clock.json")
            ),
            serializePackageInspectionReport(fixture.report)
        ));
        const auto bundlePath = root.filePath(QStringLiteral("bundle.bin"));
        QVERIFY(writeBundle(bundlePath, fixture.bundle));

        UserPackageStore store(dataRoot, stateRoot);
        const auto loaded = store.load();
        QVERIFY2(loaded.ok(), qPrintable(loaded.error));
        QVERIFY(loaded.entries.isEmpty());
        QVERIFY(!loaded.warnings.isEmpty());
        const auto installResult = store.install(fixture.report, bundlePath);
        QVERIFY(!installResult.success);
        QCOMPARE(
            installResult.errorCode,
            QStringLiteral("PackageTransactionFailed")
        );
        const auto removeResult = store.remove(
            fixture.report.manifest.id,
            fixture.report.packageDigest
        );
        QVERIFY(!removeResult.success);

        QFile sentinel(sentinelPath);
        QVERIFY(sentinel.open(QIODevice::ReadOnly));
        QCOMPARE(sentinel.readAll(), QByteArrayLiteral("keep"));
        QVERIFY(QFileInfo(
            QDir(dataRoot).filePath(fixture.report.manifest.id)
        ).isSymLink());
    }

    void sameVersionDifferentBytesAreRejected()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        UserPackageStore store(
            root.filePath(QStringLiteral("data")),
            root.filePath(QStringLiteral("state"))
        );
        auto first = makePackage(
            QStringLiteral("org.example.clock"),
            QStringLiteral("1.0.0"),
            QByteArrayLiteral("one")
        );
        auto replacement = makePackage(
            QStringLiteral("org.example.clock"),
            QStringLiteral("1.0.0"),
            QByteArrayLiteral("different")
        );
        const auto firstBundle = root.filePath(QStringLiteral("first.bin"));
        const auto nextBundle = root.filePath(QStringLiteral("next.bin"));
        QVERIFY(writeBundle(firstBundle, first.bundle));
        QVERIFY(writeBundle(nextBundle, replacement.bundle));
        QVERIFY(store.install(first.report, firstBundle).success);

        const auto rejected = store.install(replacement.report, nextBundle);
        QVERIFY(!rejected.success);
        QCOMPARE(rejected.errorCode, QStringLiteral("SameVersionDifferentDigest"));
        const auto loaded = store.load();
        QVERIFY(loaded.ok());
        QCOMPARE(loaded.entries.first().packageDigest, first.report.packageDigest);
    }

    void loadRejectsTamperedContentAndReceiptDigest()
    {
        QTemporaryDir contentRoot;
        QVERIFY(contentRoot.isValid());
        const auto contentData = contentRoot.filePath(QStringLiteral("data"));
        const auto contentState = contentRoot.filePath(QStringLiteral("state"));
        UserPackageStore contentStore(contentData, contentState);
        auto fixture = makePackage(
            QStringLiteral("org.example.clock"),
            QStringLiteral("1.0.0"),
            QByteArrayLiteral("original")
        );
        auto healthy = makePackage(
            QStringLiteral("org.example.healthy"),
            QStringLiteral("1.0.0"),
            QByteArrayLiteral("healthy")
        );
        const auto bundlePath = contentRoot.filePath(QStringLiteral("bundle.bin"));
        const auto healthyBundlePath = contentRoot.filePath(
            QStringLiteral("healthy.bin")
        );
        QVERIFY(writeBundle(bundlePath, fixture.bundle));
        QVERIFY(writeBundle(healthyBundlePath, healthy.bundle));
        QVERIFY(contentStore.install(fixture.report, bundlePath).success);
        QVERIFY(contentStore.install(
            healthy.report, healthyBundlePath
        ).success);
        QFile payload(QDir(installedRoot(
            contentData,
            fixture.report.manifest.id,
            fixture.report.manifest.version
        )).filePath(QStringLiteral("payload/widget.json")));
        QVERIFY(payload.open(QIODevice::WriteOnly | QIODevice::Truncate));
        QCOMPARE(payload.write(QByteArrayLiteral("tampered")), 8);
        payload.close();
        const auto contentLoad = contentStore.load();
        QVERIFY(contentLoad.ok());
        QCOMPARE(contentLoad.entries.size(), 1);
        QCOMPARE(
            contentLoad.entries.first().manifest.id,
            healthy.report.manifest.id
        );
        QVERIFY(!contentLoad.warnings.isEmpty());

        QTemporaryDir receiptRoot;
        QVERIFY(receiptRoot.isValid());
        const auto receiptData = receiptRoot.filePath(QStringLiteral("data"));
        const auto receiptState = receiptRoot.filePath(QStringLiteral("state"));
        UserPackageStore receiptStore(receiptData, receiptState);
        const auto receiptBundle = receiptRoot.filePath(QStringLiteral("bundle.bin"));
        const auto healthyReceiptBundle = receiptRoot.filePath(
            QStringLiteral("healthy.bin")
        );
        QVERIFY(writeBundle(receiptBundle, fixture.bundle));
        QVERIFY(writeBundle(healthyReceiptBundle, healthy.bundle));
        QVERIFY(receiptStore.install(fixture.report, receiptBundle).success);
        QVERIFY(receiptStore.install(
            healthy.report, healthyReceiptBundle
        ).success);
        const auto receiptPath = QDir(receiptState).filePath(
            QStringLiteral("receipts/org.example.clock.json")
        );
        QFile receipt(receiptPath);
        QVERIFY(receipt.open(QIODevice::ReadOnly));
        auto receiptObject = QJsonDocument::fromJson(receipt.readAll()).object();
        receipt.close();
        receiptObject.insert(
            QStringLiteral("packageDigest"),
            QString(64, QLatin1Char('c'))
        );
        QVERIFY(receipt.open(QIODevice::WriteOnly | QIODevice::Truncate));
        const auto changed = QJsonDocument(receiptObject).toJson(
            QJsonDocument::Compact
        );
        QCOMPARE(receipt.write(changed), changed.size());
        receipt.close();
        const auto receiptLoad = receiptStore.load();
        QVERIFY(receiptLoad.ok());
        QCOMPARE(receiptLoad.entries.size(), 1);
        QCOMPARE(
            receiptLoad.entries.first().manifest.id,
            healthy.report.manifest.id
        );
        QVERIFY(!receiptLoad.warnings.isEmpty());
    }

    void corruptTreesDoNotConsumePublishedCatalogQuota()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        const auto dataRoot = root.filePath(QStringLiteral("data"));
        const auto stateRoot = root.filePath(QStringLiteral("state"));
        for (int index = 0; index < 16; ++index) {
            const auto report = makeLargeDeclaredReport(
                QStringLiteral("org.example.a-corrupt-%1")
                    .arg(index, 2, 10, QLatin1Char('0'))
            );
            QVERIFY(writePrivateFile(
                QDir(stateRoot).filePath(
                    QStringLiteral("receipts/") + report.manifest.id
                        + QStringLiteral(".json")
                ),
                serializePackageInspectionReport(report)
            ));
        }
        auto healthy = makePackage(
            QStringLiteral("org.example.z-healthy"),
            QStringLiteral("1.0.0"),
            QByteArrayLiteral("healthy")
        );
        QVERIFY(seedInstalledPackage(dataRoot, stateRoot, healthy));

        UserPackageStore store(dataRoot, stateRoot);
        const auto loaded = store.load();
        QVERIFY2(loaded.ok(), qPrintable(loaded.error));
        QCOMPARE(loaded.receiptCount, 17);
        QCOMPARE(loaded.entries.size(), 1);
        QCOMPARE(
            loaded.entries.first().manifest.id,
            healthy.report.manifest.id
        );
        QCOMPARE(
            loaded.totalExpandedBytes,
            healthy.report.expandedSize
        );
        QVERIFY(loaded.warnings.size() >= 16);
    }

    void removalIsDigestGuardedAndBlocksDependents()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        const auto dataRoot = root.filePath(QStringLiteral("data"));
        UserPackageStore store(
            dataRoot,
            root.filePath(QStringLiteral("state"))
        );
        auto base = makePackage(
            QStringLiteral("org.example.base"),
            QStringLiteral("1.0.0"),
            QByteArrayLiteral("base")
        );
        auto dependent = makePackage(
            QStringLiteral("org.example.dependent"),
            QStringLiteral("1.0.0"),
            QByteArrayLiteral("dependent"),
            QStringLiteral("org.example.base")
        );
        const auto baseBundle = root.filePath(QStringLiteral("base.bin"));
        const auto dependentBundle = root.filePath(
            QStringLiteral("dependent.bin")
        );
        QVERIFY(writeBundle(baseBundle, base.bundle));
        QVERIFY(writeBundle(dependentBundle, dependent.bundle));
        QVERIFY(store.install(base.report, baseBundle).success);
        QVERIFY(store.install(dependent.report, dependentBundle).success);

        const auto wrongDigest = store.remove(
            base.report.manifest.id,
            QString(64, QLatin1Char('f'))
        );
        QVERIFY(!wrongDigest.success);
        QCOMPARE(wrongDigest.errorCode, QStringLiteral("PackageDigestMismatch"));

        const auto blocked = store.remove(
            base.report.manifest.id,
            base.report.packageDigest
        );
        QVERIFY(!blocked.success);
        QCOMPARE(blocked.errorCode, QStringLiteral("PackageHasDependents"));

        const auto removedDependent = store.remove(
            dependent.report.manifest.id,
            dependent.report.packageDigest
        );
        QVERIFY2(
            removedDependent.success,
            qPrintable(removedDependent.errorMessage)
        );
        const auto removedBase = store.remove(
            base.report.manifest.id,
            base.report.packageDigest
        );
        QVERIFY2(removedBase.success, qPrintable(removedBase.errorMessage));
        const auto loaded = store.load();
        QVERIFY2(loaded.ok(), qPrintable(loaded.error));
        QVERIFY(loaded.entries.isEmpty());
        QVERIFY(!QFileInfo::exists(QDir(dataRoot).filePath(
            QStringLiteral("org.example.base")
        )));
    }

    void installRejectsMismatchedMaterializationBundle()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        UserPackageStore store(
            root.filePath(QStringLiteral("data")),
            root.filePath(QStringLiteral("state"))
        );
        auto reviewed = makePackage(
            QStringLiteral("org.example.clock"),
            QStringLiteral("1.0.0"),
            QByteArrayLiteral("reviewed")
        );
        auto other = makePackage(
            QStringLiteral("org.example.other"),
            QStringLiteral("1.0.0"),
            QByteArrayLiteral("other")
        );
        const auto bundlePath = root.filePath(QStringLiteral("wrong.bin"));
        QVERIFY(writeBundle(bundlePath, other.bundle));
        const auto rejected = store.install(reviewed.report, bundlePath);
        QVERIFY(!rejected.success);
        QCOMPARE(rejected.errorCode, QStringLiteral("PackageTransactionFailed"));
        QVERIFY(store.load().entries.isEmpty());
    }

    void fullCatalogRejectsNewPackageBeforeMutation()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        const auto dataRoot = root.filePath(QStringLiteral("data"));
        const auto stateRoot = root.filePath(QStringLiteral("state"));
        for (int index = 0; index < 511; ++index) {
            const auto fixture = makePackage(
                QStringLiteral("org.example.fixture-%1")
                    .arg(index, 3, 10, QLatin1Char('0')),
                QStringLiteral("1.0.0"),
                QByteArrayLiteral("payload")
            );
            QVERIFY(seedInstalledPackage(
                dataRoot,
                stateRoot,
                fixture
            ));
        }

        UserPackageStore store(dataRoot, stateRoot);
        const auto before = store.load();
        QVERIFY2(before.ok(), qPrintable(before.error));
        QCOMPARE(before.entries.size(), 511);

        auto extra = makePackage(
            QStringLiteral("org.example.too-many"),
            QStringLiteral("1.0.0"),
            QByteArrayLiteral("extra")
        );
        const auto bundlePath = root.filePath(QStringLiteral("extra.bin"));
        QVERIFY(writeBundle(bundlePath, extra.bundle));
        const auto rejected = store.install(extra.report, bundlePath);
        QVERIFY(!rejected.success);
        QCOMPARE(rejected.errorCode, QStringLiteral("PackageCatalogFull"));
        QVERIFY(!QFileInfo::exists(QDir(stateRoot).filePath(
            QStringLiteral("receipts/org.example.too-many.json")
        )));
        const auto after = store.load();
        QVERIFY2(after.ok(), qPrintable(after.error));
        QCOMPARE(after.entries.size(), 511);

        auto repair = makePackage(
            QStringLiteral("org.example.fixture-000"),
            QStringLiteral("1.0.0"),
            QByteArrayLiteral("payload")
        );
        const auto repairReceipt = QDir(stateRoot).filePath(
            QStringLiteral("receipts/org.example.fixture-000.json")
        );
        QVERIFY(writePrivateFile(repairReceipt, QByteArrayLiteral("{broken")));
        const auto quarantined = store.load();
        QVERIFY2(quarantined.ok(), qPrintable(quarantined.error));
        QCOMPARE(quarantined.receiptCount, 511);
        QCOMPARE(quarantined.entries.size(), 510);

        const auto repairBundle = root.filePath(QStringLiteral("repair.bin"));
        QVERIFY(writeBundle(repairBundle, repair.bundle));
        const auto repaired = store.install(repair.report, repairBundle);
        QVERIFY2(repaired.success, qPrintable(repaired.errorMessage));
        const auto repairedCatalog = store.load();
        QVERIFY2(repairedCatalog.ok(), qPrintable(repairedCatalog.error));
        QCOMPARE(repairedCatalog.receiptCount, 511);
        QCOMPARE(repairedCatalog.entries.size(), 511);
    }
};

QTEST_GUILESS_MAIN(UserPackageStoreTest)

#include "user_package_store_test.moc"
