#include "compositord/identity.h"

#include <QByteArray>
#include <QSet>
#include <QStringList>
#include <QVector>
#include <QtTest>

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <span>
#include <utility>

using namespace HyprShelld::Compositor;

namespace {

struct EntropyStep final {
    QByteArray bytes;
    qint64 reportedCount = 0;
    int errorNumber = 0;
};

[[nodiscard]] EntropyStep bytesStep(QByteArray bytes)
{
    const auto size = bytes.size();
    return {
        .bytes = std::move(bytes),
        .reportedCount = size,
    };
}

[[nodiscard]] EntropyStep errorStep(const int errorNumber)
{
    return {
        .reportedCount = -1,
        .errorNumber = errorNumber,
    };
}

[[nodiscard]] EntropyStep zeroStep()
{
    return {};
}

[[nodiscard]] QByteArray candidateBytes(quint64 value)
{
    QByteArray result(canonicalIdentifierByteCount, '\0');
    for (qsizetype index = 0; index < 8; ++index) {
        result[canonicalIdentifierByteCount - 1 - index] = static_cast<char>(
            value & 0xffU
        );
        value >>= 8U;
    }
    return result;
}

[[nodiscard]] EntropyStep candidateStep(const quint64 value)
{
    return bytesStep(candidateBytes(value));
}

class ScriptedEntropy final {
public:
    explicit ScriptedEntropy(QVector<EntropyStep> steps)
        : steps_(std::move(steps))
    {
    }

    [[nodiscard]] qint64 read(
        const std::span<std::byte> destination,
        int &errorNumber
    )
    {
        ++calls;
        if (next_ >= steps_.size()) {
            errorNumber = EIO;
            return -1;
        }

        const auto &step = steps_.at(next_++);
        errorNumber = step.errorNumber;
        if (step.reportedCount <= 0) {
            return step.reportedCount;
        }

        const auto copyCount = std::min({
            static_cast<qsizetype>(step.bytes.size()),
            static_cast<qsizetype>(destination.size()),
            static_cast<qsizetype>(step.reportedCount),
        });
        if (copyCount > 0) {
            std::memcpy(
                destination.data(),
                step.bytes.constData(),
                static_cast<std::size_t>(copyCount)
            );
        }
        return step.reportedCount;
    }

    int calls = 0;

private:
    QVector<EntropyStep> steps_;
    qsizetype next_ = 0;
};

[[nodiscard]] IdentifierMintResult mintWith(
    const LeaseHeldPredicate &leaseHeld,
    const IdentifierCollisionPredicate &collides,
    ScriptedEntropy &entropy
)
{
    const IdentifierCollisionPredicate checkedCollision =
        [&collides](const QStringView candidate) {
            if (!isCanonicalIdentifier(candidate)) {
                QTest::qFail(
                    "The mint core passed a noncanonical value to the collision predicate",
                    __FILE__,
                    __LINE__
                );
                return true;
            }
            return collides(candidate);
        };
    return IdentityTestSupport::mintIdentifierWithEntropy(
        leaseHeld,
        checkedCollision,
        [&entropy](const std::span<std::byte> destination, int &errorNumber) {
            return entropy.read(destination, errorNumber);
        }
    );
}

[[nodiscard]] QString identifierFor(const quint64 value)
{
    return QString::fromLatin1(candidateBytes(value).toHex());
}

} // namespace

class CompositorIdentityTest final : public QObject {
    Q_OBJECT

private slots:
    void acceptsTheSharedIdentifierGrammarForEveryRole();
    void rejectsEveryIdentifierLengthAlphabetAndAliasDrift();
    void validatesTheIndependentDigestGrammar();
    void rejectsMissingPredicatesWithoutReadingEntropy();
    void rejectsAnAbsentLeaseBeforeReadingEntropy();
    void retriesEintrAndShortReadsAtTheExactOffset();
    void rejectsZeroAndOversizeEntropyReadsImmediately();
    void rejectsPermanentEntropyFailureImmediately();
    void regeneratesAllZeroCandidatesWithinTheSharedLimit();
    void regeneratesCollisionsOneThroughFifteen();
    void rejectsTheSixteenthCollisionWithoutASeventeenthDraw();
    void rejectsLeaseLossAfterACompleteCandidate();
    void rejectsLeaseLossDuringAShortRead();
    void rejectsLeaseLossInsideTheCollisionPredicate();
    void usesOneAggregateLiveAndPermanentCollisionPredicate();
    void neverAcceptsDuplicateZeroOrInvalidIdentifiers();
    void productionGetrandomMintsCanonicalUniqueIdentifiers();
};

void CompositorIdentityTest::acceptsTheSharedIdentifierGrammarForEveryRole()
{
    const QStringList roles{
        QStringLiteral("authority"),
        QStringLiteral("request"),
        QStringLiteral("operation"),
        QStringLiteral("plan"),
        QStringLiteral("repair"),
        QStringLiteral("repair-result"),
        QStringLiteral("backup"),
        QStringLiteral("anchor"),
        QStringLiteral("bootstrap"),
        QStringLiteral("effect"),
        QStringLiteral("display"),
    };
    const auto canonical = QStringLiteral("0123456789abcdef0123456789abcdef");
    for (const auto &role : roles) {
        QVERIFY2(isCanonicalIdentifier(canonical), qPrintable(role));
    }

    QVERIFY(isCanonicalIdentifier(QString(31, QLatin1Char('0'))
                                  + QLatin1Char('1')));
    QVERIFY(isCanonicalIdentifier(QString(32, QLatin1Char('f'))));
    QVERIFY(!isCanonicalIdentifier(QString(32, QLatin1Char('0'))));
}

void CompositorIdentityTest::rejectsEveryIdentifierLengthAlphabetAndAliasDrift()
{
    for (qsizetype length = 0; length <= 64; ++length) {
        const auto value = QString(length, QLatin1Char('1'));
        QCOMPARE(isCanonicalIdentifier(value), length == 32);
    }

    auto canonical = QStringLiteral("0123456789abcdef0123456789abcdef");
    for (qsizetype index = 0; index < canonical.size(); ++index) {
        auto uppercase = canonical;
        uppercase[index] = QLatin1Char('A');
        QVERIFY(!isCanonicalIdentifier(uppercase));

        auto punctuation = canonical;
        punctuation[index] = QLatin1Char('-');
        QVERIFY(!isCanonicalIdentifier(punctuation));

        for (const auto lower : QStringLiteral("0123456789abcdef")) {
            auto accepted = canonical;
            accepted[index] = lower;
            QVERIFY(isCanonicalIdentifier(accepted));
        }
    }

    const QStringList aliases{
        QStringLiteral("{0123456789abcdef0123456789abcdef}"),
        QStringLiteral("01234567-89ab-cdef-0123-456789abcdef"),
        QStringLiteral("0x0123456789abcdef0123456789abcdef"),
        QStringLiteral(" 0123456789abcdef0123456789abcdef"),
        QStringLiteral("0123456789abcdef0123456789abcdef "),
        QStringLiteral("0123456789abcdef0123456789abcdef\n"),
        QStringLiteral("0123456789abcdef0123456789abcdeg"),
    };
    for (const auto &alias : aliases) {
        QVERIFY(!isCanonicalIdentifier(alias));
    }

    auto embeddedNull = canonical;
    embeddedNull[7] = QChar::Null;
    QVERIFY(!isCanonicalIdentifier(embeddedNull));
    auto nonAscii = canonical;
    nonAscii[7] = QChar(0x00e9);
    QVERIFY(!isCanonicalIdentifier(nonAscii));
}

void CompositorIdentityTest::validatesTheIndependentDigestGrammar()
{
    QVERIFY(isCanonicalSha256Digest(QString(64, QLatin1Char('0'))));
    QVERIFY(isCanonicalSha256Digest(QString(64, QLatin1Char('f'))));
    QVERIFY(isCanonicalSha256Digest(
        QStringLiteral(
            "0123456789abcdef0123456789abcdef"
            "fedcba9876543210fedcba9876543210"
        )
    ));

    for (qsizetype length = 0; length <= 96; ++length) {
        const auto value = QString(length, QLatin1Char('a'));
        QCOMPARE(isCanonicalSha256Digest(value), length == 64);
    }

    auto canonical = QString(64, QLatin1Char('a'));
    const QString invalidCharacters = QStringLiteral("AG-_{ }/\\\n\t");
    for (const auto invalid : invalidCharacters) {
        auto value = canonical;
        value[31] = invalid;
        QVERIFY(!isCanonicalSha256Digest(value));
    }
    canonical[31] = QChar::Null;
    QVERIFY(!isCanonicalSha256Digest(canonical));
    canonical[31] = QChar(0xff41);
    QVERIFY(!isCanonicalSha256Digest(canonical));
    QVERIFY(!isCanonicalSha256Digest(
        QStringLiteral("sha256:") + QString(64, QLatin1Char('a'))
    ));
}

void CompositorIdentityTest::rejectsMissingPredicatesWithoutReadingEntropy()
{
    int leaseChecks = 0;
    int collisionChecks = 0;
    int entropyCalls = 0;
    const LeaseHeldPredicate held = [&leaseChecks] {
        ++leaseChecks;
        return true;
    };
    const IdentifierCollisionPredicate free = [&collisionChecks](QStringView) {
        ++collisionChecks;
        return false;
    };
    const IdentityTestSupport::EntropyReadFunction entropy =
        [&entropyCalls](std::span<std::byte>, int &) -> qint64 {
            ++entropyCalls;
            return -1;
        };

    const auto missingLease =
        IdentityTestSupport::mintIdentifierWithEntropy({}, free, entropy);
    QVERIFY(!missingLease);
    QVERIFY(missingLease.error() == IdentifierMintError::InvalidContext);

    const auto missingCollision =
        IdentityTestSupport::mintIdentifierWithEntropy(held, {}, entropy);
    QVERIFY(!missingCollision);
    QVERIFY(missingCollision.error() == IdentifierMintError::InvalidContext);

    const auto missingEntropy =
        IdentityTestSupport::mintIdentifierWithEntropy(held, free, {});
    QVERIFY(!missingEntropy);
    QVERIFY(missingEntropy.error() == IdentifierMintError::InvalidContext);

    QCOMPARE(leaseChecks, 0);
    QCOMPARE(collisionChecks, 0);
    QCOMPARE(entropyCalls, 0);
}

void CompositorIdentityTest::rejectsAnAbsentLeaseBeforeReadingEntropy()
{
    int leaseChecks = 0;
    int collisionChecks = 0;
    ScriptedEntropy entropy({candidateStep(1)});
    const auto result = mintWith(
        [&leaseChecks] {
            ++leaseChecks;
            return false;
        },
        [&collisionChecks](QStringView) {
            ++collisionChecks;
            return false;
        },
        entropy
    );

    QVERIFY(!result);
    QVERIFY(result.error() == IdentifierMintError::LeaseNotHeld);
    QCOMPARE(leaseChecks, 1);
    QCOMPARE(entropy.calls, 0);
    QCOMPARE(collisionChecks, 0);
}

void CompositorIdentityTest::retriesEintrAndShortReadsAtTheExactOffset()
{
    const QVector<QVector<EntropyStep>> scripts{
        {
            errorStep(EINTR),
            bytesStep(QByteArray::fromHex("001122")),
            bytesStep(QByteArray::fromHex("3344556677")),
            bytesStep(QByteArray::fromHex("8899aabbccddeeff")),
        },
        {
            bytesStep(QByteArray::fromHex("001122")),
            errorStep(EINTR),
            bytesStep(QByteArray::fromHex("3344556677")),
            bytesStep(QByteArray::fromHex("8899aabbccddeeff")),
        },
    };
    for (const auto &script : scripts) {
        ScriptedEntropy entropy(script);
        int leaseChecks = 0;
        int collisionChecks = 0;
        const auto result = mintWith(
            [&leaseChecks] {
                ++leaseChecks;
                return true;
            },
            [&collisionChecks](QStringView) {
                ++collisionChecks;
                return false;
            },
            entropy
        );

        QVERIFY(result);
        QCOMPARE(
            *result,
            QStringLiteral("00112233445566778899aabbccddeeff")
        );
        QCOMPARE(entropy.calls, 4);
        QCOMPARE(leaseChecks, 6);
        QCOMPARE(collisionChecks, 1);
    }
}

void CompositorIdentityTest::rejectsZeroAndOversizeEntropyReadsImmediately()
{
    for (const auto step : {
             zeroStep(),
             EntropyStep{
                 .bytes = QByteArray(16, '\1'),
                 .reportedCount = 17,
             },
         }) {
        ScriptedEntropy entropy({step, candidateStep(1)});
        int leaseChecks = 0;
        int collisionChecks = 0;
        const auto result = mintWith(
            [&leaseChecks] {
                ++leaseChecks;
                return true;
            },
            [&collisionChecks](QStringView) {
                ++collisionChecks;
                return false;
            },
            entropy
        );

        QVERIFY(!result);
        QVERIFY(result.error() == IdentifierMintError::EntropyFailure);
        QCOMPARE(entropy.calls, 1);
        QCOMPARE(leaseChecks, 1);
        QCOMPARE(collisionChecks, 0);
    }
}

void CompositorIdentityTest::rejectsPermanentEntropyFailureImmediately()
{
    const QVector<QVector<EntropyStep>> scripts{
        {errorStep(EIO), candidateStep(1)},
        {
            bytesStep(QByteArray::fromHex("00112233")),
            errorStep(EIO),
            candidateStep(1),
        },
    };
    for (qsizetype index = 0; index < scripts.size(); ++index) {
        ScriptedEntropy entropy(scripts.at(index));
        int leaseChecks = 0;
        int collisionChecks = 0;
        const auto result = mintWith(
            [&leaseChecks] {
                ++leaseChecks;
                return true;
            },
            [&collisionChecks](QStringView) {
                ++collisionChecks;
                return false;
            },
            entropy
        );

        QVERIFY(!result);
        QVERIFY(result.error() == IdentifierMintError::EntropyFailure);
        QCOMPARE(entropy.calls, index + 1);
        QCOMPARE(leaseChecks, index + 1);
        QCOMPARE(collisionChecks, 0);
    }
}

void CompositorIdentityTest::regeneratesAllZeroCandidatesWithinTheSharedLimit()
{
    {
        ScriptedEntropy entropy({
            bytesStep(QByteArray(16, '\0')),
            candidateStep(1),
        });
        int leaseChecks = 0;
        int collisionChecks = 0;
        const auto result = mintWith(
            [&leaseChecks] {
                ++leaseChecks;
                return true;
            },
            [&collisionChecks](QStringView) {
                ++collisionChecks;
                return false;
            },
            entropy
        );
        QVERIFY(result);
        QCOMPARE(*result, identifierFor(1));
        QCOMPARE(entropy.calls, 2);
        QCOMPARE(leaseChecks, 5);
        QCOMPARE(collisionChecks, 1);
    }

    QVector<EntropyStep> zeroCandidates;
    for (int attempt = 0; attempt < identifierMintAttemptLimit; ++attempt) {
        zeroCandidates.append(bytesStep(QByteArray(16, '\0')));
    }
    zeroCandidates.append(candidateStep(1));
    ScriptedEntropy entropy(std::move(zeroCandidates));
    int leaseChecks = 0;
    int collisionChecks = 0;
    const auto result = mintWith(
        [&leaseChecks] {
            ++leaseChecks;
            return true;
        },
        [&collisionChecks](QStringView) {
            ++collisionChecks;
            return false;
        },
        entropy
    );
    QVERIFY(!result);
    QVERIFY(result.error() == IdentifierMintError::CandidateLimitReached);
    QCOMPARE(entropy.calls, identifierMintAttemptLimit);
    QCOMPARE(leaseChecks, identifierMintAttemptLimit * 2);
    QCOMPARE(collisionChecks, 0);
}

void CompositorIdentityTest::regeneratesCollisionsOneThroughFifteen()
{
    for (int collisionCount = 1;
         collisionCount < identifierMintAttemptLimit;
         ++collisionCount) {
        QVector<EntropyStep> candidates;
        for (int candidate = 1; candidate <= collisionCount + 1; ++candidate) {
            candidates.append(candidateStep(static_cast<quint64>(candidate)));
        }
        ScriptedEntropy entropy(std::move(candidates));
        int leaseChecks = 0;
        int collisionChecks = 0;
        const auto result = mintWith(
            [&leaseChecks] {
                ++leaseChecks;
                return true;
            },
            [&collisionChecks, collisionCount](QStringView) {
                ++collisionChecks;
                return collisionChecks <= collisionCount;
            },
            entropy
        );

        QVERIFY2(result.has_value(), qPrintable(QString::number(collisionCount)));
        QCOMPARE(*result, identifierFor(collisionCount + 1));
        QCOMPARE(entropy.calls, collisionCount + 1);
        QCOMPARE(leaseChecks, (collisionCount + 1) * 3);
        QCOMPARE(collisionChecks, collisionCount + 1);
    }
}

void CompositorIdentityTest::rejectsTheSixteenthCollisionWithoutASeventeenthDraw()
{
    QVector<EntropyStep> candidates;
    for (int candidate = 1;
         candidate <= identifierMintAttemptLimit + 1;
         ++candidate) {
        candidates.append(candidateStep(static_cast<quint64>(candidate)));
    }
    ScriptedEntropy entropy(std::move(candidates));
    int leaseChecks = 0;
    int collisionChecks = 0;
    const auto result = mintWith(
        [&leaseChecks] {
            ++leaseChecks;
            return true;
        },
        [&collisionChecks](QStringView) {
            ++collisionChecks;
            return true;
        },
        entropy
    );

    QVERIFY(!result);
    QVERIFY(result.error() == IdentifierMintError::CandidateLimitReached);
    QCOMPARE(entropy.calls, identifierMintAttemptLimit);
    QCOMPARE(leaseChecks, identifierMintAttemptLimit * 3);
    QCOMPARE(collisionChecks, identifierMintAttemptLimit);
}

void CompositorIdentityTest::rejectsLeaseLossAfterACompleteCandidate()
{
    ScriptedEntropy entropy({candidateStep(1)});
    int leaseChecks = 0;
    int collisionChecks = 0;
    const auto result = mintWith(
        [&leaseChecks] {
            ++leaseChecks;
            return leaseChecks == 1;
        },
        [&collisionChecks](QStringView) {
            ++collisionChecks;
            return false;
        },
        entropy
    );

    QVERIFY(!result);
    QVERIFY(result.error() == IdentifierMintError::LeaseNotHeld);
    QCOMPARE(entropy.calls, 1);
    QCOMPARE(leaseChecks, 2);
    QCOMPARE(collisionChecks, 0);
}

void CompositorIdentityTest::rejectsLeaseLossDuringAShortRead()
{
    ScriptedEntropy entropy({
        bytesStep(QByteArray::fromHex("00112233")),
        bytesStep(QByteArray::fromHex("445566778899aabbccddeeff")),
    });
    int leaseChecks = 0;
    int collisionChecks = 0;
    const auto result = mintWith(
        [&leaseChecks] {
            ++leaseChecks;
            return leaseChecks == 1;
        },
        [&collisionChecks](QStringView) {
            ++collisionChecks;
            return false;
        },
        entropy
    );

    QVERIFY(!result);
    QVERIFY(result.error() == IdentifierMintError::LeaseNotHeld);
    QCOMPARE(entropy.calls, 1);
    QCOMPARE(leaseChecks, 2);
    QCOMPARE(collisionChecks, 0);
}

void CompositorIdentityTest::rejectsLeaseLossInsideTheCollisionPredicate()
{
    for (const auto reportsCollision : {false, true}) {
        ScriptedEntropy entropy({candidateStep(1), candidateStep(2)});
        bool held = true;
        int leaseChecks = 0;
        int collisionChecks = 0;
        const auto result = mintWith(
            [&held, &leaseChecks] {
                ++leaseChecks;
                return held;
            },
            [&held, &collisionChecks, reportsCollision](QStringView) {
                ++collisionChecks;
                held = false;
                return reportsCollision;
            },
            entropy
        );

        QVERIFY(!result);
        QVERIFY(result.error() == IdentifierMintError::LeaseNotHeld);
        QCOMPARE(entropy.calls, 1);
        QCOMPARE(leaseChecks, 3);
        QCOMPARE(collisionChecks, 1);
    }
}

void CompositorIdentityTest::usesOneAggregateLiveAndPermanentCollisionPredicate()
{
    const QSet<QString> live{identifierFor(1)};
    const QSet<QString> permanent{identifierFor(2)};
    ScriptedEntropy entropy({
        candidateStep(1),
        candidateStep(2),
        candidateStep(3),
    });
    int leaseChecks = 0;
    int collisionChecks = 0;
    const auto result = mintWith(
        [&leaseChecks] {
            ++leaseChecks;
            return true;
        },
        [&live, &permanent, &collisionChecks](const QStringView candidate) {
            ++collisionChecks;
            return live.contains(candidate.toString())
                || permanent.contains(candidate.toString());
        },
        entropy
    );

    QVERIFY(result);
    QCOMPARE(*result, identifierFor(3));
    QCOMPARE(entropy.calls, 3);
    QCOMPARE(leaseChecks, 9);
    QCOMPARE(collisionChecks, 3);
}

void CompositorIdentityTest::neverAcceptsDuplicateZeroOrInvalidIdentifiers()
{
    QVector<EntropyStep> candidates{
        bytesStep(QByteArray(16, '\0')),
    };
    constexpr int acceptedCount = 64;
    for (quint64 candidate = 1; candidate <= acceptedCount; ++candidate) {
        candidates.append(candidateStep(candidate));
    }
    ScriptedEntropy entropy(std::move(candidates));
    QSet<QString> accepted;
    int leaseChecks = 0;
    int collisionChecks = 0;

    for (int index = 0; index < acceptedCount; ++index) {
        const auto result = mintWith(
            [&leaseChecks] {
                ++leaseChecks;
                return true;
            },
            [&accepted, &collisionChecks](const QStringView candidate) {
                ++collisionChecks;
                return accepted.contains(candidate.toString());
            },
            entropy
        );
        QVERIFY(result);
        QVERIFY(isCanonicalIdentifier(*result));
        QVERIFY(*result != QString(32, QLatin1Char('0')));
        QVERIFY(!accepted.contains(*result));
        accepted.insert(*result);
    }

    QCOMPARE(accepted.size(), acceptedCount);
    QCOMPARE(entropy.calls, acceptedCount + 1);
    QCOMPARE(leaseChecks, 2 + acceptedCount * 3);
    QCOMPARE(collisionChecks, acceptedCount);
}

void CompositorIdentityTest::productionGetrandomMintsCanonicalUniqueIdentifiers()
{
    constexpr int acceptedCount = 32;
    QSet<QString> accepted;
    int leaseChecks = 0;
    int collisionChecks = 0;
    bool everyProbeWasCanonical = true;
    for (int index = 0; index < acceptedCount; ++index) {
        const auto result = mintIdentifier(
            [&leaseChecks] {
                ++leaseChecks;
                return true;
            },
            [&accepted,
             &collisionChecks,
             &everyProbeWasCanonical](const QStringView candidate) {
                ++collisionChecks;
                everyProbeWasCanonical = everyProbeWasCanonical
                    && isCanonicalIdentifier(candidate);
                return accepted.contains(candidate.toString());
            }
        );
        QVERIFY(result);
        QVERIFY(isCanonicalIdentifier(*result));
        QVERIFY(!accepted.contains(*result));
        accepted.insert(*result);
    }

    QCOMPARE(accepted.size(), acceptedCount);
    QVERIFY(everyProbeWasCanonical);
    QVERIFY(leaseChecks >= acceptedCount * 3);
    QVERIFY(collisionChecks >= acceptedCount);
}

QTEST_APPLESS_MAIN(CompositorIdentityTest)

#include "compositor_identity_test.moc"
