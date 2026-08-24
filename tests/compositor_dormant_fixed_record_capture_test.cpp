#include "compositord/dormant_fixed_record_capture.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

#include <algorithm>
#include <array>
#include <cerrno>
#include <type_traits>
#include <utility>

#include <fcntl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

using namespace HyprShelld::Compositor;
using namespace HyprShelld::Compositor::DormantFixedRecordCaptureTestSupport;

namespace {

constexpr std::array<const char *, 5> names{{
    "authority.json",
    "desired.json",
    "last-good.json",
    "activation.json",
    "pending.json",
}};

constexpr std::array<qsizetype, 5> caps{{
    maximumDormantAuthorityCaptureBytes,
    maximumDormantDesiredCaptureBytes,
    maximumDormantLastGoodCaptureBytes,
    maximumDormantAppliedCaptureBytes,
    maximumDormantPendingCaptureBytes,
}};

constexpr std::array<DormantFixedRecordCaptureSubject, 5> subjects{{
    DormantFixedRecordCaptureSubject::Authority,
    DormantFixedRecordCaptureSubject::Desired,
    DormantFixedRecordCaptureSubject::LastGood,
    DormantFixedRecordCaptureSubject::Applied,
    DormantFixedRecordCaptureSubject::Pending,
}};

class StateDirectory final {
public:
  StateDirectory() {
    valid = temporary.isValid();
    path = QDir(temporary.path()).filePath(QStringLiteral("state"));
    valid = valid && QDir().mkdir(path) &&
            ::chmod(encoded().constData(), 0700) == 0;
    if (valid) {
      descriptor = ::open(encoded().constData(),
                          O_RDONLY | O_CLOEXEC | O_DIRECTORY | O_NOFOLLOW);
      valid = descriptor >= 0;
    }
  }

  ~StateDirectory() {
    if (descriptor >= 0)
      ::close(descriptor);
  }

  StateDirectory(const StateDirectory &) = delete;
  StateDirectory &operator=(const StateDirectory &) = delete;

  [[nodiscard]] QByteArray encoded() const { return QFile::encodeName(path); }
  [[nodiscard]] QString file(const size_t index) const {
    return QDir(path).filePath(QString::fromLatin1(names[index]));
  }

  QTemporaryDir temporary;
  QString path;
  int descriptor = -1;
  bool valid = false;
};

[[nodiscard]] bool closeChecked(int &descriptor) {
  if (descriptor < 0)
    return true;
  const auto value = descriptor;
  descriptor = -1;
  return ::close(value) == 0;
}

[[nodiscard]] bool writeAll(const int descriptor, const QByteArray &bytes) {
  qsizetype offset = 0;
  while (offset < bytes.size()) {
    const auto count = ::write(descriptor, bytes.constData() + offset,
                               static_cast<size_t>(bytes.size() - offset));
    if (count < 0 && errno == EINTR)
      continue;
    if (count <= 0)
      return false;
    offset += static_cast<qsizetype>(count);
  }
  return true;
}

[[nodiscard]] bool writeFile(const QString &path, const QByteArray &bytes) {
  const auto encoded = QFile::encodeName(path);
  auto descriptor = ::open(encoded.constData(),
                           O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0600);
  if (descriptor < 0)
    return false;
  const auto wrote = writeAll(descriptor, bytes);
  const auto closed = closeChecked(descriptor);
  return wrote && closed && ::chmod(encoded.constData(), 0600) == 0;
}

[[nodiscard]] bool sparseFile(const QString &path, const qsizetype size) {
  const auto encoded = QFile::encodeName(path);
  auto descriptor = ::open(encoded.constData(),
                           O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0600);
  if (descriptor < 0)
    return false;
  const auto truncated = ::ftruncate(descriptor, static_cast<off_t>(size)) == 0;
  const auto closed = closeChecked(descriptor);
  return truncated && closed && ::chmod(encoded.constData(), 0600) == 0;
}

void clearRecords(const StateDirectory &root) {
  for (size_t index = 0; index < names.size(); ++index) {
    const auto encoded = QFile::encodeName(root.file(index));
    ::unlink(encoded.constData());
    ::rmdir(encoded.constData());
  }
}

[[nodiscard]] bool replaceFile(const QString &path, const QByteArray &bytes) {
  const auto encoded = QFile::encodeName(path);
  if (::unlink(encoded.constData()) != 0)
    return false;
  return writeFile(path, bytes);
}

[[nodiscard]] bool mutateFirstByte(const QString &path, const char value) {
  const auto encoded = QFile::encodeName(path);
  auto descriptor = ::open(encoded.constData(), O_WRONLY | O_CLOEXEC);
  if (descriptor < 0)
    return false;
  const auto wrote = ::pwrite(descriptor, &value, 1, 0) == 1;
  const auto closed = closeChecked(descriptor);
  return wrote && closed;
}

[[nodiscard]] const DormantFixedRecordField &
fieldAt(const DormantFixedRecordCapture &capture, const size_t index) {
  switch (index) {
  case 0:
    return capture.authority();
  case 1:
    return capture.desired();
  case 2:
    return capture.lastGood();
  case 3:
    return capture.applied();
  default:
    return capture.pending();
  }
}

[[nodiscard]] bool captured(const DormantFixedRecordCaptureResult &result) {
  return result.disposition() ==
             DormantFixedRecordCaptureDisposition::Captured &&
         result.reason() == DormantFixedRecordCaptureReason::None &&
         result.subject() == DormantFixedRecordCaptureSubject::None &&
         result.capture().has_value();
}

[[nodiscard]] bool failedClosed(const DormantFixedRecordCaptureResult &result) {
  return result.disposition() ==
             DormantFixedRecordCaptureDisposition::FailedClosed &&
         result.reason() != DormantFixedRecordCaptureReason::None &&
         !result.capture().has_value();
}

[[nodiscard]] qsizetype openDescriptorCount() {
  return QDir(QStringLiteral("/proc/self/fd"))
      .entryList(QDir::AllEntries | QDir::NoDotAndDotDot)
      .size();
}

} // namespace

class CompositorDormantFixedRecordCaptureTest final : public QObject {
  Q_OBJECT

private slots:
  void cleanup() { clearHooks(); }

  void constantsAndUnforgeableBoundary() {
    static_assert(!std::is_default_constructible_v<DormantFixedRecordField>);
    static_assert(!std::is_default_constructible_v<DormantFixedRecordCapture>);
    static_assert(
        !std::is_default_constructible_v<DormantFixedRecordCaptureResult>);
    static_assert(maximumDormantPendingCaptureBytes == 8404992);
    static_assert(maximumDormantFixedRecordRetainedPayloadBytes == 20988160ULL);
    static_assert(maximumDormantFixedRecordTwoPassPayloadBytes == 41976320ULL);
    static_assert(maximumDormantFixedRecordPreadReturnedBytes == 41976321ULL);
    static_assert(maximumDormantFixedRecordStreamingPayloadBytes ==
                  29393152ULL);
    static_assert(maximumDormantFixedRecordWorkingPayloadBytes == 29397248ULL);
    static_assert(maximumDormantFixedRecordProofSyscallAttempts == 12000ULL);
    static_assert(dormantFixedRecordCleanupAttemptReserve == 32ULL);
    static_assert(maximumDormantFixedRecordSyscallAttempts == 12032ULL);
    QVERIFY(true);
  }

  void allMissingPresentVectorsAndEmptyPresent() {
    StateDirectory root;
    QVERIFY(root.valid);
    for (quint32 mask = 0; mask < 32; ++mask) {
      clearRecords(root);
      std::array<QByteArray, 5> expected{};
      for (size_t index = 0; index < names.size(); ++index) {
        if ((mask & (1U << index)) == 0)
          continue;
        expected[index] = index == 2
                              ? QByteArray{}
                              : QByteArray(1, static_cast<char>('a' + index));
        QVERIFY(writeFile(root.file(index), expected[index]));
      }
      const auto result = captureDormantFixedRecords(root.descriptor);
      QVERIFY2(captured(result),
               qPrintable(QStringLiteral("mask %1").arg(mask)));
      for (size_t index = 0; index < names.size(); ++index) {
        const auto &field = fieldAt(*result.capture(), index);
        if ((mask & (1U << index)) != 0) {
          QCOMPARE(field.kind(), DormantFixedRecordFieldKind::PresentBytes);
          QCOMPARE(field.bytes(), expected[index]);
        } else {
          QCOMPARE(field.kind(), DormantFixedRecordFieldKind::Missing);
          QVERIFY(field.bytes().isEmpty());
        }
      }
    }
  }

  void subjectBoundsAndMaximumAggregate() {
    for (size_t index = 0; index < names.size(); ++index) {
      StateDirectory root;
      QVERIFY(root.valid);
      QVERIFY(sparseFile(root.file(index), caps[index]));
      auto atCap = captureDormantFixedRecords(root.descriptor);
      QVERIFY(captured(atCap));
      QCOMPARE(fieldAt(*atCap.capture(), index).bytes().size(), caps[index]);

      QVERIFY(sparseFile(root.file(index), caps[index] + 1));
      const auto above = captureDormantFixedRecords(root.descriptor);
      QVERIFY(failedClosed(above));
      QCOMPARE(above.reason(), DormantFixedRecordCaptureReason::UnsafeRecord);
      QCOMPARE(above.subject(), subjects[index]);
    }

    StateDirectory aggregate;
    QVERIFY(aggregate.valid);
    for (size_t index = 0; index < names.size(); ++index)
      QVERIFY(sparseFile(aggregate.file(index), caps[index]));
    const auto maximum = captureDormantFixedRecords(aggregate.descriptor);
    QVERIFY(captured(maximum));
    quint64 retained = 0;
    for (size_t index = 0; index < names.size(); ++index)
      retained += static_cast<quint64>(
          fieldAt(*maximum.capture(), index).bytes().size());
    QCOMPARE(retained, maximumDormantFixedRecordRetainedPayloadBytes);
  }

  void invalidRootDescriptorsAndMetadata() {
    QVERIFY(failedClosed(captureDormantFixedRecords(-1)));
    QVERIFY(failedClosed(captureDormantFixedRecords(AT_FDCWD)));

    StateDirectory root;
    QVERIFY(root.valid);
    auto regular = ::open(QFile::encodeName(root.file(0)).constData(),
                          O_WRONLY | O_CREAT | O_CLOEXEC, 0600);
    QVERIFY(regular >= 0);
    QVERIFY(failedClosed(captureDormantFixedRecords(regular)));
    QVERIFY(closeChecked(regular));

    auto pathOnly = ::open(root.encoded().constData(), O_PATH | O_CLOEXEC);
    QVERIFY(pathOnly >= 0);
    QVERIFY(failedClosed(captureDormantFixedRecords(pathOnly)));
    QVERIFY(closeChecked(pathOnly));

    QVERIFY(::chmod(root.encoded().constData(), 0755) == 0);
    QVERIFY(failedClosed(captureDormantFixedRecords(root.descriptor)));
    QVERIFY(::chmod(root.encoded().constData(), 04700) == 0);
    QVERIFY(failedClosed(captureDormantFixedRecords(root.descriptor)));
    QVERIFY(::chmod(root.encoded().constData(), 0700) == 0);

    setSyscallHook([](const SyscallEvent &event) {
      return event.syscall == Syscall::GetDescriptorFlags ? Fault::FailIo
                                                          : Fault::None;
    });
    const auto badFlags = captureDormantFixedRecords(root.descriptor);
    QVERIFY(failedClosed(badFlags));
    QCOMPARE(badFlags.reason(),
             DormantFixedRecordCaptureReason::UnsafeStateDirectory);
  }

  void ownedDuplicateCallerReuseAndRenamedPathlessness() {
    StateDirectory root;
    QVERIFY(root.valid);
    auto caller = root.descriptor;
    const auto originalCaller = caller;
    int reused = -1;
    setCheckpointHook([&](const CheckpointEvent &event) {
      if (event.checkpoint != Checkpoint::AfterRootDuplicate || caller < 0)
        return;
      ::close(caller);
      root.descriptor = -1;
      caller = -1;
      reused = ::open("/dev/null", O_RDONLY | O_CLOEXEC);
    });
    const auto retained = captureDormantFixedRecords(
        root.descriptor >= 0 ? root.descriptor : caller);
    // The call argument is evaluated before the hook closes/reuses it.
    QVERIFY(captured(retained));
    QVERIFY(reused >= 0);
    QCOMPARE(reused, originalCaller);
    QVERIFY(closeChecked(reused));
    clearHooks();

    StateDirectory renamed;
    QVERIFY(renamed.valid);
    QVERIFY(writeFile(renamed.file(0), QByteArrayLiteral("pathless")));
    const auto oldPath = renamed.path;
    const auto newPath = QDir(renamed.temporary.path())
                             .filePath(QStringLiteral("state-renamed"));
    QVERIFY(QDir().rename(oldPath, newPath));
    renamed.path = newPath;
    const auto result = captureDormantFixedRecords(renamed.descriptor);
    QVERIFY(captured(result));
    QCOMPARE(result.capture()->authority().bytes(),
             QByteArrayLiteral("pathless"));
  }

  void unsafeFileTypesModesLinksAndNonblockingSpecials() {
    {
      StateDirectory root;
      QVERIFY(root.valid);
      QVERIFY(writeFile(root.file(1), QByteArrayLiteral("target")));
      QVERIFY(QFile::link(root.file(1), root.file(0)));
      QVERIFY(failedClosed(captureDormantFixedRecords(root.descriptor)));
    }
    {
      StateDirectory root;
      QVERIFY(root.valid);
      QVERIFY(::symlink("desired.json",
                        QFile::encodeName(root.file(0)).constData()) == 0);
      QVERIFY(failedClosed(captureDormantFixedRecords(root.descriptor)));
    }
    {
      StateDirectory root;
      QVERIFY(root.valid);
      QVERIFY(::symlink("missing-target",
                        QFile::encodeName(root.file(0)).constData()) == 0);
      QVERIFY(failedClosed(captureDormantFixedRecords(root.descriptor)));
    }
    {
      StateDirectory root;
      QVERIFY(root.valid);
      QVERIFY(QDir().mkdir(root.file(0)));
      QVERIFY(failedClosed(captureDormantFixedRecords(root.descriptor)));
    }
    {
      StateDirectory root;
      QVERIFY(root.valid);
      QVERIFY(::mkfifo(QFile::encodeName(root.file(0)).constData(), 0600) == 0);
      QVERIFY(failedClosed(captureDormantFixedRecords(root.descriptor)));
    }
    {
      StateDirectory root;
      QVERIFY(root.valid);
      QVERIFY(writeFile(root.file(0), QByteArrayLiteral("regular-first")));
      bool becameFifo = false;
      setCheckpointHook([&](const CheckpointEvent &event) {
        if (!becameFifo && event.pass == Pass::First &&
            event.subject == DormantFixedRecordCaptureSubject::Authority &&
            event.checkpoint == Checkpoint::AfterLookup) {
          const auto encoded = QFile::encodeName(root.file(0));
          becameFifo = ::unlink(encoded.constData()) == 0 &&
                       ::mkfifo(encoded.constData(), 0600) == 0;
        }
      });
      // The lookup/open substitution cannot block because openat carries
      // O_NONBLOCK; the opened/named mismatch then fails closed.
      QVERIFY(failedClosed(captureDormantFixedRecords(root.descriptor)));
      QVERIFY(becameFifo);
      clearHooks();
    }
    {
      StateDirectory root;
      QVERIFY(root.valid);
      auto socketFd = ::socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
      QVERIFY(socketFd >= 0);
      sockaddr_un address{};
      address.sun_family = AF_UNIX;
      const auto encoded = QFile::encodeName(root.file(0));
      QVERIFY(static_cast<size_t>(encoded.size()) < sizeof(address.sun_path));
      std::copy(encoded.cbegin(), encoded.cend(), address.sun_path);
      const auto addressSize = static_cast<socklen_t>(
          offsetof(sockaddr_un, sun_path) + encoded.size() + 1);
      const auto bound =
          ::bind(socketFd, reinterpret_cast<sockaddr *>(&address),
                 addressSize) == 0;
      if (bound) {
        QVERIFY(failedClosed(captureDormantFixedRecords(root.descriptor)));
      } else {
        // This managed sandbox rejects AF_UNIX bind with EPERM. The socket
        // branch is constructed when the kernel policy permits it; otherwise
        // direct socket-dirent coverage is an explicit environment gap.
        QCOMPARE(errno, EPERM);
      }
      QVERIFY(closeChecked(socketFd));
    }
    {
      StateDirectory root;
      QVERIFY(root.valid);
      QVERIFY(writeFile(root.file(0), QByteArrayLiteral("mode")));
      QVERIFY(::chmod(QFile::encodeName(root.file(0)).constData(), 0644) == 0);
      QVERIFY(failedClosed(captureDormantFixedRecords(root.descriptor)));
      QVERIFY(::chmod(QFile::encodeName(root.file(0)).constData(), 04600) == 0);
      QVERIFY(failedClosed(captureDormantFixedRecords(root.descriptor)));
    }
    {
      StateDirectory root;
      QVERIFY(root.valid);
      QVERIFY(writeFile(root.file(0), QByteArrayLiteral("linked")));
      const auto alias = QDir(root.path).filePath(QStringLiteral("alias"));
      QVERIFY(::link(QFile::encodeName(root.file(0)).constData(),
                     QFile::encodeName(alias).constData()) == 0);
      QVERIFY(failedClosed(captureDormantFixedRecords(root.descriptor)));
    }
    // A direct same-name cross-device regular file and a pairwise nlink-one
    // inode alias require mount/bind-mount privileges. The production checks
    // exist; this unprivileged focused test intentionally claims no direct
    // construction coverage for those two kernel topologies.
  }

  void exactOrderRootGuardsAndHeldFirstPassDescriptors() {
    StateDirectory root;
    QVERIFY(root.valid);
    for (size_t index = 0; index < names.size(); ++index)
      QVERIFY(writeFile(root.file(index),
                        QByteArray(8, static_cast<char>('a' + index))));

    QVector<QPair<Pass, DormantFixedRecordCaptureSubject>> lookups;
    qsizetype beforeGuards = 0;
    qsizetype afterGuards = 0;
    QVector<qsizetype> firstHeld;
    QVector<qsizetype> secondHeld;
    QVector<qsizetype> finalHeld;
    setCheckpointHook([&](const CheckpointEvent &event) {
      if (event.checkpoint == Checkpoint::BeforeLookup)
        lookups.push_back({event.pass, event.subject});
      if (event.checkpoint == Checkpoint::BeforeRootGuard)
        ++beforeGuards;
      if (event.checkpoint == Checkpoint::AfterRootGuard)
        ++afterGuards;
      if (event.checkpoint == Checkpoint::AfterRecord) {
        (event.pass == Pass::First ? firstHeld : secondHeld)
            .push_back(event.heldFirstPassDescriptors);
      }
      if (event.checkpoint == Checkpoint::BeforeHeldFirstPassFinalization)
        finalHeld.push_back(event.heldFirstPassDescriptors);
    });
    const auto result = captureDormantFixedRecords(root.descriptor);
    QVERIFY(captured(result));
    QCOMPARE(lookups.size(), 10);
    for (qsizetype index = 0; index < 10; ++index) {
      QCOMPARE(lookups[index].first, index < 5 ? Pass::First : Pass::Second);
      QCOMPARE(lookups[index].second, subjects[static_cast<size_t>(index % 5)]);
    }
    QCOMPARE(beforeGuards, 23);
    QCOMPARE(afterGuards, 23);
    QCOMPARE(firstHeld, QVector<qsizetype>({1, 2, 3, 4, 5}));
    QCOMPARE(secondHeld, QVector<qsizetype>({5, 5, 5, 5, 5}));
    QCOMPARE(finalHeld, QVector<qsizetype>({5, 5, 5, 5, 5}));
  }

  void everySubjectPassAndRecordRaceBoundaryFailsClosed() {
    constexpr std::array<Checkpoint, 7> boundaries{{
        Checkpoint::AfterLookup,
        Checkpoint::BeforeOpen,
        Checkpoint::AfterOpen,
        Checkpoint::AfterOpenedFstat,
        Checkpoint::AfterPread,
        Checkpoint::BeforeFinalNamedCheck,
        Checkpoint::AfterRecord,
    }};
    for (const auto pass : {Pass::First, Pass::Second}) {
      for (size_t subjectIndex = 0; subjectIndex < subjects.size();
           ++subjectIndex) {
        for (const auto boundary : boundaries) {
          StateDirectory root;
          QVERIFY(root.valid);
          for (size_t index = 0; index < names.size(); ++index)
            QVERIFY(writeFile(root.file(index),
                              QByteArray(128, static_cast<char>('a' + index))));
          struct stat rootInfo{};
          struct stat fileInfo{};
          QVERIFY(::fstat(root.descriptor, &rootInfo) == 0);
          QVERIFY(::fstatat(root.descriptor, names[subjectIndex], &fileInfo,
                            AT_SYMLINK_NOFOLLOW) == 0);
          QCOMPARE(fileInfo.st_dev, rootInfo.st_dev);
          QCOMPARE(fileInfo.st_uid, ::geteuid());
          QCOMPARE(fileInfo.st_nlink, static_cast<nlink_t>(1));
          QCOMPARE(fileInfo.st_mode & 07777, static_cast<mode_t>(0600));
          bool mutated = false;
          bool mutationOkay = true;
          setCheckpointHook([&](const CheckpointEvent &event) {
            if (mutated || event.pass != pass ||
                event.subject != subjects[subjectIndex] ||
                event.checkpoint != boundary ||
                (boundary == Checkpoint::AfterPread &&
                 event.preadInvocation != 1)) {
              return;
            }
            mutated = true;
            if (boundary == Checkpoint::AfterLookup ||
                boundary == Checkpoint::BeforeOpen ||
                boundary == Checkpoint::AfterOpen) {
              mutationOkay = replaceFile(
                  root.file(subjectIndex),
                  QByteArray(128, static_cast<char>('z' - subjectIndex)));
            } else {
              mutationOkay = mutateFirstByte(root.file(subjectIndex), 'Z');
            }
          });
          const auto result = captureDormantFixedRecords(root.descriptor);
          QVERIFY2(mutated,
                   qPrintable(QStringLiteral(
                                  "not reached pass=%1 subject=%2 boundary=%3")
                                  .arg(static_cast<int>(pass))
                                  .arg(subjectIndex)
                                  .arg(static_cast<int>(boundary)) +
                              QStringLiteral(" result reason=%1 subject=%2")
                                  .arg(static_cast<int>(result.reason()))
                                  .arg(static_cast<int>(result.subject()))));
          QVERIFY(mutationOkay);
          QVERIFY2(failedClosed(result),
                   qPrintable(QStringLiteral("pass=%1 subject=%2 boundary=%3")
                                  .arg(static_cast<int>(pass))
                                  .arg(subjectIndex)
                                  .arg(static_cast<int>(boundary))));
          clearHooks();
        }
      }
    }
  }

  void passTransitionsIdentityBytesMetadataAndRootRacesFailClosed() {
    const auto runBetween =
        [](const bool initiallyPresent,
           const std::function<bool(StateDirectory &)> &mutate) {
          StateDirectory root;
          if (!root.valid)
            return false;
          if (initiallyPresent &&
              !writeFile(root.file(0), QByteArrayLiteral("same")))
            return false;
          bool mutated = false;
          bool mutationOkay = true;
          setCheckpointHook([&](const CheckpointEvent &event) {
            if (!mutated && event.checkpoint == Checkpoint::BetweenPasses) {
              mutated = true;
              mutationOkay = mutate(root);
            }
          });
          const auto result = captureDormantFixedRecords(root.descriptor);
          clearHooks();
          return mutated && mutationOkay && failedClosed(result);
        };

    QVERIFY(runBetween(false, [](StateDirectory &root) {
      return writeFile(root.file(0), QByteArrayLiteral("appeared"));
    }));
    QVERIFY(runBetween(true, [](StateDirectory &root) {
      return ::unlink(QFile::encodeName(root.file(0)).constData()) == 0;
    }));
    QVERIFY(runBetween(true, [](StateDirectory &root) {
      return replaceFile(root.file(0), QByteArrayLiteral("same"));
    }));
    QVERIFY(runBetween(true, [](StateDirectory &root) {
      return mutateFirstByte(root.file(0), 'X');
    }));
    QVERIFY(runBetween(true, [](StateDirectory &root) {
      return ::chmod(QFile::encodeName(root.file(0)).constData(), 0400) == 0;
    }));

    StateDirectory betweenRoot;
    QVERIFY(betweenRoot.valid);
    setCheckpointHook([&](const CheckpointEvent &event) {
      if (event.checkpoint == Checkpoint::BetweenPasses)
        ::chmod(betweenRoot.encoded().constData(), 0755);
    });
    QVERIFY(failedClosed(captureDormantFixedRecords(betweenRoot.descriptor)));
    clearHooks();

    StateDirectory finalRoot;
    QVERIFY(finalRoot.valid);
    setCheckpointHook([&](const CheckpointEvent &event) {
      if (event.checkpoint == Checkpoint::BeforeFinalRootGuard)
        ::chmod(finalRoot.encoded().constData(), 0755);
    });
    const auto finalRace = captureDormantFixedRecords(finalRoot.descriptor);
    QVERIFY(failedClosed(finalRace));
    QCOMPARE(finalRace.reason(),
             DormantFixedRecordCaptureReason::StateDirectoryChanged);
  }

  void boundedEintrShortReadProofBudgetCloseAndNoLeaks() {
    StateDirectory root;
    QVERIFY(root.valid);
    QVERIFY(writeFile(root.file(0), QByteArray(64, 'a')));

    bool interrupted = false;
    setSyscallHook([&](const SyscallEvent &event) {
      if (!interrupted && event.syscall == Syscall::Pread) {
        interrupted = true;
        return Fault::FailEintr;
      }
      return Fault::None;
    });
    QVERIFY(captured(captureDormantFixedRecords(root.descriptor)));
    QVERIFY(interrupted);
    clearHooks();

    setSyscallHook([](const SyscallEvent &event) {
      return event.syscall == Syscall::Pread ? Fault::ShortReadOneByte
                                             : Fault::None;
    });
    QVERIFY(captured(captureDormantFixedRecords(root.descriptor)));
    clearHooks();

    setSyscallHook([](const SyscallEvent &event) {
      return event.syscall == Syscall::Pread ? Fault::FailEintr : Fault::None;
    });
    const auto bounded = captureDormantFixedRecords(root.descriptor);
    QVERIFY(failedClosed(bounded));
    QCOMPARE(bounded.reason(),
             DormantFixedRecordCaptureReason::ReadBudgetExceeded);
    clearHooks();

    setSyscallHook([](const SyscallEvent &event) {
      return event.syscall == Syscall::Openat ? Fault::ExhaustProofBudget
                                              : Fault::None;
    });
    const auto exhausted = captureDormantFixedRecords(root.descriptor);
    QVERIFY(failedClosed(exhausted));
    QCOMPARE(exhausted.reason(),
             DormantFixedRecordCaptureReason::ProofBudgetExceeded);
    clearHooks();

    const auto descriptorsBefore = openDescriptorCount();
    bool closeFaulted = false;
    setSyscallHook([&](const SyscallEvent &event) {
      if (!closeFaulted && event.syscall == Syscall::CloseFile &&
          event.pass == Pass::Second &&
          event.subject == DormantFixedRecordCaptureSubject::Authority) {
        closeFaulted = true;
        return Fault::ReportCloseFailure;
      }
      return Fault::None;
    });
    const auto closeFailure = captureDormantFixedRecords(root.descriptor);
    QVERIFY(closeFaulted);
    QVERIFY(failedClosed(closeFailure));
    QCOMPARE(closeFailure.reason(),
             DormantFixedRecordCaptureReason::CleanupFailed);
    QCOMPARE(closeFailure.subject(),
             DormantFixedRecordCaptureSubject::Authority);
    QCOMPARE(openDescriptorCount(), descriptorsBefore);
  }

  void missingSentinelHeldCleanupAllocationAndNoAtimeRegressions() {
    {
      StateDirectory root;
      QVERIFY(root.valid);
      bool appeared = false;
      setCheckpointHook([&](const CheckpointEvent &event) {
        if (!appeared && event.pass == Pass::First &&
            event.subject == DormantFixedRecordCaptureSubject::Authority &&
            event.checkpoint == Checkpoint::AfterLookup) {
          appeared = writeFile(root.file(0), QByteArrayLiteral("appeared"));
        }
      });
      const auto result = captureDormantFixedRecords(root.descriptor);
      QVERIFY(appeared);
      QVERIFY(failedClosed(result));
      clearHooks();
    }

    {
      StateDirectory root;
      QVERIFY(root.valid);
      QVERIFY(writeFile(root.file(0), QByteArrayLiteral("x")));
      qsizetype authorityPreads = 0;
      setSyscallHook([&](const SyscallEvent &event) {
        if (event.syscall == Syscall::Pread && event.pass == Pass::First &&
            event.subject == DormantFixedRecordCaptureSubject::Authority &&
            ++authorityPreads == 2) {
          return Fault::FailEintr; // the first EOF-sentinel attempt
        }
        return Fault::None;
      });
      const auto result = captureDormantFixedRecords(root.descriptor);
      QVERIFY(captured(result));
      QVERIFY(authorityPreads >= 3);
      clearHooks();
    }

    {
      StateDirectory root;
      QVERIFY(root.valid);
      QVERIFY(writeFile(root.file(0), QByteArrayLiteral("x")));
      bool appended = false;
      setCheckpointHook([&](const CheckpointEvent &event) {
        if (!appended && event.pass == Pass::First &&
            event.subject == DormantFixedRecordCaptureSubject::Authority &&
            event.checkpoint == Checkpoint::AfterOpenedFstat) {
          auto descriptor = ::open(QFile::encodeName(root.file(0)).constData(),
                                   O_WRONLY | O_APPEND | O_CLOEXEC);
          appended = descriptor >= 0 && ::write(descriptor, "y", 1) == 1 &&
                     closeChecked(descriptor);
        }
      });
      const auto result = captureDormantFixedRecords(root.descriptor);
      QVERIFY(appended);
      QVERIFY(failedClosed(result));
      clearHooks();
    }

    {
      StateDirectory root;
      QVERIFY(root.valid);
      for (size_t index = 0; index < names.size(); ++index)
        QVERIFY(writeFile(root.file(index), QByteArrayLiteral("same")));
      bool replaced = false;
      qsizetype heldAtMutation = 0;
      setCheckpointHook([&](const CheckpointEvent &event) {
        if (!replaced &&
            event.checkpoint == Checkpoint::BeforeHeldFirstPassFinalization &&
            event.subject == DormantFixedRecordCaptureSubject::Authority) {
          heldAtMutation = event.heldFirstPassDescriptors;
          replaced = replaceFile(root.file(0), QByteArrayLiteral("same"));
        }
      });
      const auto result = captureDormantFixedRecords(root.descriptor);
      QVERIFY(replaced);
      QCOMPARE(heldAtMutation, static_cast<qsizetype>(5));
      QVERIFY(failedClosed(result));
      clearHooks();
    }

    {
      StateDirectory root;
      QVERIFY(root.valid);
      for (size_t index = 0; index < names.size(); ++index)
        QVERIFY(writeFile(root.file(index), QByteArrayLiteral("held")));
      const auto before = openDescriptorCount();
      setSyscallHook([](const SyscallEvent &event) {
        return event.syscall == Syscall::Openat && event.pass == Pass::Second &&
                       event.subject ==
                           DormantFixedRecordCaptureSubject::Authority
                   ? Fault::ExhaustProofBudget
                   : Fault::None;
      });
      const auto result = captureDormantFixedRecords(root.descriptor);
      QVERIFY(failedClosed(result));
      QCOMPARE(result.reason(),
               DormantFixedRecordCaptureReason::ProofBudgetExceeded);
      QCOMPARE(openDescriptorCount(), before);
      clearHooks();
    }

    {
      StateDirectory root;
      QVERIFY(root.valid);
      QVERIFY(writeFile(root.file(0), QByteArrayLiteral("close")));
      const auto before = openDescriptorCount();
      qsizetype authorityCloses = 0;
      setSyscallHook([&](const SyscallEvent &event) {
        if (event.syscall == Syscall::CloseFile && event.pass == Pass::Second &&
            event.subject == DormantFixedRecordCaptureSubject::Authority &&
            ++authorityCloses == 2) {
          return Fault::ReportCloseFailure; // held pass-A final close
        }
        return Fault::None;
      });
      const auto result = captureDormantFixedRecords(root.descriptor);
      QVERIFY(failedClosed(result));
      QCOMPARE(authorityCloses, static_cast<qsizetype>(2));
      QCOMPARE(result.reason(), DormantFixedRecordCaptureReason::CleanupFailed);
      QCOMPARE(result.subject(), DormantFixedRecordCaptureSubject::Authority);
      QCOMPARE(openDescriptorCount(), before);
      clearHooks();
    }

    {
      StateDirectory root;
      QVERIFY(root.valid);
      QVERIFY(writeFile(root.file(0), QByteArrayLiteral("allocation")));
      const auto before = openDescriptorCount();
      bool injected = false;
      setPayloadAllocationFailureHook(
          [&](const Pass pass, const DormantFixedRecordCaptureSubject subject) {
            if (!injected && pass == Pass::First &&
                subject == DormantFixedRecordCaptureSubject::Authority) {
              injected = true;
              return true;
            }
            return false;
          });
      const auto result = captureDormantFixedRecords(root.descriptor);
      QVERIFY(injected);
      QVERIFY(failedClosed(result));
      QCOMPARE(result.subject(), DormantFixedRecordCaptureSubject::Authority);
      QCOMPARE(openDescriptorCount(), before);
      clearHooks();
    }

    {
      StateDirectory root;
      QVERIFY(root.valid);
      QVERIFY(writeFile(root.file(0), QByteArrayLiteral("no-atime")));
      const auto encoded = QFile::encodeName(root.file(0));
      std::array<timespec, 2> times{{
          {.tv_sec = 1000000000, .tv_nsec = 123456789},
          {.tv_sec = 1700000000, .tv_nsec = 987654321},
      }};
      QVERIFY(::utimensat(AT_FDCWD, encoded.constData(), times.data(), 0) == 0);
      struct stat before{};
      struct stat after{};
      QVERIFY(::stat(encoded.constData(), &before) == 0);
      QVERIFY(captured(captureDormantFixedRecords(root.descriptor)));
      QVERIFY(::stat(encoded.constData(), &after) == 0);
      QCOMPARE(after.st_atim.tv_sec, before.st_atim.tv_sec);
      QCOMPARE(after.st_atim.tv_nsec, before.st_atim.tv_nsec);
    }
  }

  void moveConstructionAndAssignmentPreserveBothEvidenceValues() {
    StateDirectory root;
    QVERIFY(root.valid);
    QVERIFY(writeFile(root.file(0), QByteArrayLiteral("evidence")));
    auto sourceResult = captureDormantFixedRecords(root.descriptor);
    QVERIFY(captured(sourceResult));

    auto movedResult = std::move(sourceResult);
    QVERIFY(captured(sourceResult));
    QVERIFY(captured(movedResult));
    QCOMPARE(*sourceResult.capture(), *movedResult.capture());

    auto assignedResult = captureDormantFixedRecords(root.descriptor);
    assignedResult = std::move(movedResult);
    QVERIFY(captured(movedResult));
    QVERIFY(captured(assignedResult));
    QCOMPARE(*movedResult.capture(), *assignedResult.capture());

    auto sourceCapture = *sourceResult.capture();
    auto movedCapture = std::move(sourceCapture);
    QCOMPARE(sourceCapture.authority().bytes(), QByteArrayLiteral("evidence"));
    QCOMPARE(sourceCapture, movedCapture);
    auto assignedCapture = *assignedResult.capture();
    assignedCapture = std::move(movedCapture);
    QCOMPARE(movedCapture.authority().bytes(), QByteArrayLiteral("evidence"));
    QCOMPARE(assignedCapture, movedCapture);

    auto sourceField = sourceCapture.authority();
    auto movedField = std::move(sourceField);
    QCOMPARE(sourceField.kind(), DormantFixedRecordFieldKind::PresentBytes);
    QCOMPARE(sourceField.bytes(), QByteArrayLiteral("evidence"));
    QCOMPARE(sourceField, movedField);
    auto assignedField = sourceCapture.desired();
    assignedField = std::move(movedField);
    QCOMPARE(movedField.bytes(), QByteArrayLiteral("evidence"));
    QCOMPARE(assignedField, movedField);
  }

  void explicitAtomicityAbaFreshnessAndProvenanceNonclaims() {
    StateDirectory root;
    QVERIFY(root.valid);
    QVERIFY(writeFile(root.file(0), QByteArrayLiteral("before")));
    bool changedBeforeOwnObservation = false;
    setCheckpointHook([&](const CheckpointEvent &event) {
      if (!changedBeforeOwnObservation && event.pass == Pass::First &&
          event.subject == DormantFixedRecordCaptureSubject::Authority &&
          event.checkpoint == Checkpoint::BeforeLookup) {
        changedBeforeOwnObservation = mutateFirstByte(root.file(0), 'A');
      }
    });
    auto observed = captureDormantFixedRecords(root.descriptor);
    QVERIFY(changedBeforeOwnObservation);
    QVERIFY(captured(observed));
    QCOMPARE(observed.capture()->authority().bytes(),
             QByteArrayLiteral("Aefore"));

    // A mutation after return does not alter the owned capture and demonstrates
    // that no post-return freshness/capability is carried by the value.
    clearHooks();
    QVERIFY(mutateFirstByte(root.file(0), 'Z'));
    QCOMPARE(observed.capture()->authority().bytes(),
             QByteArrayLiteral("Aefore"));

    // No deterministic unit expectation can prove the absence of an
    // undetectable same-inode ABA by an actor capable of restoring every
    // compared field. The implementation deliberately makes no atomicity,
    // co-temporality, interval, FUSE wall-time, mount/bind provenance, lease,
    // or CAS claim; these comments are negative scope, not direct coverage.
  }
};

QTEST_MAIN(CompositorDormantFixedRecordCaptureTest)

#include "compositor_dormant_fixed_record_capture_test.moc"
