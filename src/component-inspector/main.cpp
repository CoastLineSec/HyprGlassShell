#include "package_inspector.h"

#include "component/package_inspection_report.h"

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>

#include <systemd/sd-daemon.h>

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace {

constexpr auto packageDescriptorName = "package";
constexpr auto resultDescriptorName = "result";
constexpr auto materializedDescriptorName = "materialized";
constexpr qsizetype maximumErrorReportBytes = 64 * 1024;

struct DescriptorSet final {
    int package = -1;
    int result = -1;
    int materialized = -1;
};

void freeNames(char **names, const int count)
{
    if (names == nullptr) {
        return;
    }
    for (int index = 0; index < count; ++index) {
        std::free(names[index]);
    }
    std::free(names);
}

bool acquireDescriptors(DescriptorSet &descriptors, QString &error)
{
    char **names = nullptr;
    const auto count = sd_listen_fds_with_names(1, &names);
    if (count < 0) {
        error = QStringLiteral("Cannot receive inspector descriptors: %1")
                    .arg(QString::fromLocal8Bit(std::strerror(-count)));
        return false;
    }
    if (count != 3 || names == nullptr) {
        freeNames(names, std::max(count, 0));
        error = QStringLiteral(
                    "Exactly package, result, and materialized descriptors are required (received %1)."
                )
                    .arg(count);
        return false;
    }

    for (int index = 0; index < count; ++index) {
        const auto descriptor = SD_LISTEN_FDS_START + index;
        const QByteArray name(names[index]);
        int *slot = nullptr;
        if (name == packageDescriptorName) {
            slot = &descriptors.package;
        } else if (name == resultDescriptorName) {
            slot = &descriptors.result;
        } else if (name == materializedDescriptorName) {
            slot = &descriptors.materialized;
        } else {
            freeNames(names, count);
            error = QStringLiteral("The inspector received an unknown named descriptor.");
            return false;
        }
        if (*slot >= 0) {
            freeNames(names, count);
            error = QStringLiteral("The inspector received a duplicate named descriptor.");
            return false;
        }
        *slot = descriptor;
    }
    freeNames(names, count);
    if (descriptors.package < 0 || descriptors.result < 0
        || descriptors.materialized < 0) {
        error = QStringLiteral("A required named descriptor is missing.");
        return false;
    }
    return true;
}

bool descriptorHasMode(const int descriptor, const bool writable)
{
    const auto flags = fcntl(descriptor, F_GETFL);
    if (flags < 0) {
        return false;
    }
    const auto access = flags & O_ACCMODE;
    return writable ? access != O_RDONLY : access == O_RDONLY;
}

bool prepareOutputDescriptor(const int descriptor, QString &error)
{
    struct stat status {};
    if (!descriptorHasMode(descriptor, true)
        || fstat(descriptor, &status) != 0 || !S_ISREG(status.st_mode)
        || ftruncate(descriptor, 0) != 0 || lseek(descriptor, 0, SEEK_SET) < 0) {
        error = QStringLiteral("An inspector output descriptor is not a writable regular file.");
        return false;
    }
    return true;
}

bool writeBytes(const int descriptor, const QByteArrayView bytes)
{
    qsizetype offset = 0;
    while (offset < bytes.size()) {
        const auto count = write(
            descriptor,
            bytes.data() + offset,
            static_cast<size_t>(bytes.size() - offset)
        );
        if (count < 0 && errno == EINTR) {
            continue;
        }
        if (count <= 0) {
            return false;
        }
        offset += count;
    }
    return fsync(descriptor) == 0;
}

QByteArray errorReport(
    const HyprShelld::Components::ValidationErrors &errors
)
{
    QJsonArray values;
    for (const auto &error : errors) {
        values.append(QJsonObject{
            {QStringLiteral("path"), error.path.left(512)},
            {QStringLiteral("code"), error.code.left(128)},
            {QStringLiteral("message"), error.message.left(2048)},
        });
    }
    auto bytes = QJsonDocument(QJsonObject{
        {QStringLiteral("errorVersion"), 1},
        {QStringLiteral("errors"), values},
    }).toJson(QJsonDocument::Compact);
    if (bytes.size() > maximumErrorReportBytes) {
        bytes = QByteArrayLiteral(
            "{\"errorVersion\":1,\"errors\":[{\"path\":\"$\",\"code\":\"package.error-report-limit\",\"message\":\"The package produced too many validation errors.\"}]}"
        );
    }
    bytes.append('\n');
    return bytes;
}

} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication application(argc, argv);
    QCoreApplication::setApplicationName(
        QStringLiteral("hyprshelld-component-inspector")
    );
    QCoreApplication::setOrganizationName(QStringLiteral("CoastLineSec"));

    QCommandLineParser parser;
    parser.setApplicationDescription(
        QStringLiteral("Internal HyprShelld component package inspector")
    );
    parser.addHelpOption();
    const QCommandLineOption inspectOption(QStringLiteral("inspect"));
    const QCommandLineOption tokenOption(
        QStringLiteral("expected-token"),
        QStringLiteral("Expected inspection nonce."),
        QStringLiteral("token")
    );
    const QCommandLineOption digestOption(
        QStringLiteral("expected-archive-digest"),
        QStringLiteral("Expected package archive SHA-256."),
        QStringLiteral("digest")
    );
    parser.addOption(inspectOption);
    parser.addOption(tokenOption);
    parser.addOption(digestOption);
    parser.process(application);

    static const QRegularExpression tokenPattern(
        QStringLiteral("^[0-9a-f]{32}$")
    );
    static const QRegularExpression digestPattern(
        QStringLiteral("^[0-9a-f]{64}$")
    );
    const auto token = parser.value(tokenOption);
    const auto digest = parser.value(digestOption);
    if (!parser.isSet(inspectOption) || !parser.positionalArguments().isEmpty()
        || !tokenPattern.match(token).hasMatch()
        || !digestPattern.match(digest).hasMatch()) {
        return EXIT_FAILURE;
    }

    DescriptorSet descriptors;
    QString descriptorError;
    if (!acquireDescriptors(descriptors, descriptorError)) {
        QString ignored;
        constexpr auto expectedResultDescriptor = SD_LISTEN_FDS_START + 1;
        if (prepareOutputDescriptor(expectedResultDescriptor, ignored)) {
            const HyprShelld::Components::ValidationErrors errors{{
                .path = QStringLiteral("$"),
                .code = QStringLiteral("package.inspector-descriptors"),
                .message = descriptorError,
            }};
            writeBytes(expectedResultDescriptor, errorReport(errors));
        }
        return EXIT_FAILURE;
    }
    if (!prepareOutputDescriptor(descriptors.result, descriptorError)) {
        return EXIT_FAILURE;
    }
    const auto failSetup = [&](const QString &message) {
        const HyprShelld::Components::ValidationErrors errors{{
            .path = QStringLiteral("$"),
            .code = QStringLiteral("package.inspector-setup"),
            .message = message,
        }};
        writeBytes(descriptors.result, errorReport(errors));
        return EXIT_FAILURE;
    };
    if (!descriptorHasMode(descriptors.package, false)) {
        return failSetup(
            QStringLiteral("The package descriptor is not read-only.")
        );
    }
    if (!prepareOutputDescriptor(
            descriptors.materialized,
            descriptorError
        )) {
        return failSetup(descriptorError);
    }

    const auto materializedCopy = fcntl(
        descriptors.materialized,
        F_DUPFD_CLOEXEC,
        3
    );
    if (materializedCopy < 0) {
        return failSetup(
            QStringLiteral("The materialized output descriptor cannot be duplicated.")
        );
    }
    QFile materialized;
    if (!materialized.open(
            materializedCopy,
            QIODevice::WriteOnly,
            QFileDevice::AutoCloseHandle
        )) {
        close(materializedCopy);
        return failSetup(
            QStringLiteral("The materialized output descriptor cannot be opened.")
        );
    }

    const auto inspected = HyprShelld::Components::inspectComponentPackage(
        descriptors.package,
        token,
        digest,
        &materialized
    );
    if (!materialized.flush()) {
        return failSetup(
            QStringLiteral("The materialized package output cannot be flushed.")
        );
    }
    if (!inspected) {
        writeBytes(descriptors.result, errorReport(inspected.errors));
        return 2;
    }
    if (fsync(materialized.handle()) != 0) {
        return failSetup(
            QStringLiteral("The materialized package output cannot be synchronized.")
        );
    }

    const auto report = HyprShelld::Components::serializePackageInspectionReport(
        *inspected.report
    );
    if (report.size() > 1024 * 1024
        || !writeBytes(descriptors.result, report)) {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
