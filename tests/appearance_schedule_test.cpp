#include "configd/appearance_schedule.h"

#include <QDate>
#include <QDateTime>
#include <QTest>
#include <QTime>
#include <QTimeZone>

#include <cmath>
#include <limits>

namespace {

QDateTime localDateTime(const QDate &date, const QTime &time,
                        const QTimeZone &timeZone) {
  return QDateTime(date, time, timeZone,
                   QDateTime::TransitionResolution::RelativeToBefore);
}

int minuteOfDay(const QDateTime &dateTime) {
  return dateTime.time().hour() * 60 + dateTime.time().minute();
}

} // namespace

class AppearanceScheduleTest final : public QObject {
  Q_OBJECT

private slots:
  void timeScheduleWrapsMidnight();
  void timeScheduleHonorsBothBoundaries();
  void timeScheduleRejectsInvalidInputs();
  void timeScheduleUsesCalendarDaysAcrossDst();
  void locationScheduleMatchesKnownSolarTimes();
  void locationScheduleAcceptsTheNullIslandCoordinates();
  void locationScheduleHandlesPolarDayAndNight();
  void locationScheduleRejectsInvalidCoordinates();
  void temperatureProjectionIsNeutralAndDeterministic();
};

void AppearanceScheduleTest::timeScheduleWrapsMidnight() {
  const auto zone = QTimeZone("America/New_York");
  QVERIFY(zone.isValid());

  const auto evening = HyprShelld::AppearanceSchedule::evaluateTime(
      localDateTime(QDate(2026, 8, 27), QTime(21, 0), zone), 18U * 60U,
      6U * 60U);
  QVERIFY(evening.valid);
  QVERIFY(evening.isDark);
  QCOMPARE(evening.status, HyprShelld::AppearanceSchedule::Status::Ready);
  QCOMPARE(evening.nextTransition.date(), QDate(2026, 8, 28));
  QCOMPARE(evening.nextTransition.time(), QTime(6, 0));
  QCOMPARE(evening.nextRefresh, evening.nextTransition);

  const auto afternoon = HyprShelld::AppearanceSchedule::evaluateTime(
      localDateTime(QDate(2026, 8, 27), QTime(13, 0), zone), 18U * 60U,
      6U * 60U);
  QVERIFY(afternoon.valid);
  QVERIFY(!afternoon.isDark);
  QCOMPARE(afternoon.nextTransition.date(), QDate(2026, 8, 27));
  QCOMPARE(afternoon.nextTransition.time(), QTime(18, 0));
}

void AppearanceScheduleTest::timeScheduleHonorsBothBoundaries() {
  const auto zone = QTimeZone::utc();
  const auto date = QDate(2026, 4, 12);
  const auto atDark = HyprShelld::AppearanceSchedule::evaluateTime(
      localDateTime(date, QTime(18, 0), zone), 18U * 60U, 6U * 60U);
  const auto atLight = HyprShelld::AppearanceSchedule::evaluateTime(
      localDateTime(date, QTime(6, 0), zone), 18U * 60U, 6U * 60U);
  QVERIFY(atDark.valid);
  QVERIFY(atDark.isDark);
  QVERIFY(atLight.valid);
  QVERIFY(!atLight.isDark);

  // Non-wrapping intervals are valid too: this schedule is dark from 06:00
  // until 18:00 and light for the rest of the day.
  const auto midday = HyprShelld::AppearanceSchedule::evaluateTime(
      localDateTime(date, QTime(12, 0), zone), 6U * 60U, 18U * 60U);
  QVERIFY(midday.valid);
  QVERIFY(midday.isDark);
  QCOMPARE(midday.nextTransition.time(), QTime(18, 0));
}

void AppearanceScheduleTest::timeScheduleRejectsInvalidInputs() {
  const auto now = QDateTime::currentDateTimeUtc();
  QVERIFY(!HyprShelld::AppearanceSchedule::evaluateTime(now, 60, 60).valid);
  QVERIFY(
      !HyprShelld::AppearanceSchedule::evaluateTime(now, 24U * 60U, 60).valid);
  QVERIFY(!HyprShelld::AppearanceSchedule::evaluateTime({}, 18U * 60U, 6U * 60U)
               .valid);
}

void AppearanceScheduleTest::timeScheduleUsesCalendarDaysAcrossDst() {
  const auto zone = QTimeZone("America/New_York");
  QVERIFY(zone.isValid());
  const auto now = localDateTime(QDate(2024, 3, 9), QTime(20, 0), zone);
  const auto result = HyprShelld::AppearanceSchedule::evaluateTime(
      now, 18U * 60U, 2U * 60U + 30U);
  QVERIFY(result.valid);
  QVERIFY(result.isDark);
  QCOMPARE(result.nextTransition.date(), QDate(2024, 3, 10));
  // 02:30 is skipped when DST starts. Qt's explicit forward resolution
  // retains the intended wall-clock offset at 03:30 instead of adding 24h.
  QCOMPARE(result.nextTransition.time(), QTime(3, 30));
  QCOMPARE(now.secsTo(result.nextTransition), 6 * 60 * 60 + 30 * 60);
}

void AppearanceScheduleTest::locationScheduleMatchesKnownSolarTimes() {
  const auto zone = QTimeZone("America/New_York");
  QVERIFY(zone.isValid());
  const auto noon = localDateTime(QDate(2024, 6, 21), QTime(12, 0), zone);
  const auto result =
      HyprShelld::AppearanceSchedule::evaluateLocation(noon, 40.7128, -74.0060);
  QVERIFY(result.valid);
  QVERIFY(!result.isDark);
  QCOMPARE(result.solarCondition,
           HyprShelld::AppearanceSchedule::SolarCondition::Normal);
  QCOMPARE(result.status, HyprShelld::AppearanceSchedule::Status::Ready);
  QVERIFY(result.sunrise.isValid());
  QVERIFY(result.sunset.isValid());
  QCOMPARE(result.sunrise.date(), QDate(2024, 6, 21));
  QCOMPARE(result.sunset.date(), QDate(2024, 6, 21));
  QVERIFY(std::abs(minuteOfDay(result.sunrise) - (5 * 60 + 25)) <= 15);
  QVERIFY(std::abs(minuteOfDay(result.sunset) - (20 * 60 + 30)) <= 15);
  QCOMPARE(result.nextTransition, result.sunset);
  QVERIFY(result.daylightBlend > 0.95);

  const auto atSunrise = HyprShelld::AppearanceSchedule::evaluateLocation(
      result.sunrise, 40.7128, -74.0060);
  const auto atSunset = HyprShelld::AppearanceSchedule::evaluateLocation(
      result.sunset, 40.7128, -74.0060);
  QVERIFY(atSunrise.valid);
  QVERIFY(!atSunrise.isDark);
  QVERIFY(atSunset.valid);
  QVERIFY(atSunset.isDark);
}

void AppearanceScheduleTest::locationScheduleAcceptsTheNullIslandCoordinates() {
  const auto result = HyprShelld::AppearanceSchedule::evaluateLocation(
      localDateTime(QDate(2026, 3, 20), QTime(12, 0), QTimeZone::utc()), 0.0,
      0.0);
  QVERIFY(result.valid);
  QCOMPARE(result.solarCondition,
           HyprShelld::AppearanceSchedule::SolarCondition::Normal);
  QVERIFY(result.sunrise.isValid());
  QVERIFY(result.sunset.isValid());
}

void AppearanceScheduleTest::locationScheduleHandlesPolarDayAndNight() {
  const auto zone = QTimeZone("Europe/Oslo");
  QVERIFY(zone.isValid());
  const auto summer = HyprShelld::AppearanceSchedule::evaluateLocation(
      localDateTime(QDate(2024, 6, 21), QTime(12, 0), zone), 69.6492, 18.9553);
  QVERIFY(summer.valid);
  QVERIFY(!summer.isDark);
  QCOMPARE(summer.solarCondition,
           HyprShelld::AppearanceSchedule::SolarCondition::PolarDay);
  QCOMPARE(summer.status, HyprShelld::AppearanceSchedule::Status::NoTransition);
  QVERIFY(!summer.sunrise.isValid());
  QVERIFY(!summer.sunset.isValid());
  QCOMPARE(summer.nextRefresh.date(), QDate(2024, 6, 22));
  QVERIFY(summer.nextTransition.isValid());
  QVERIFY(summer.nextTransition > summer.nextRefresh);

  const auto winter = HyprShelld::AppearanceSchedule::evaluateLocation(
      localDateTime(QDate(2024, 12, 21), QTime(12, 0), zone), 69.6492, 18.9553);
  QVERIFY(winter.valid);
  QVERIFY(winter.isDark);
  QCOMPARE(winter.solarCondition,
           HyprShelld::AppearanceSchedule::SolarCondition::PolarNight);
  QCOMPARE(winter.status, HyprShelld::AppearanceSchedule::Status::NoTransition);
  QVERIFY(winter.nextTransition.isValid());
}

void AppearanceScheduleTest::locationScheduleRejectsInvalidCoordinates() {
  const auto now = QDateTime::currentDateTimeUtc();
  QVERIFY(
      !HyprShelld::AppearanceSchedule::evaluateLocation(now, 90.1, 0.0).valid);
  QVERIFY(!HyprShelld::AppearanceSchedule::evaluateLocation(now, 0.0, -180.1)
               .valid);
  QVERIFY(!HyprShelld::AppearanceSchedule::evaluateLocation(
               now, std::numeric_limits<double>::quiet_NaN(), 0.0)
               .valid);
}

void AppearanceScheduleTest::temperatureProjectionIsNeutralAndDeterministic() {
  HyprShelld::AppearanceSchedule::Evaluation evaluation;
  QCOMPARE(HyprShelld::AppearanceSchedule::targetTemperature(evaluation, 4000,
                                                             6500, true),
           6500U);

  evaluation.valid = true;
  evaluation.isDark = true;
  evaluation.daylightBlend = 0.5;
  QCOMPARE(HyprShelld::AppearanceSchedule::targetTemperature(evaluation, 4000,
                                                             6500, false),
           4000U);
  QCOMPARE(HyprShelld::AppearanceSchedule::targetTemperature(evaluation, 4000,
                                                             6500, true),
           5250U);
  QCOMPARE(HyprShelld::AppearanceSchedule::statusName(
               HyprShelld::AppearanceSchedule::Status::NoTransition),
           QStringLiteral("no-transition"));
}

QTEST_GUILESS_MAIN(AppearanceScheduleTest)

#include "appearance_schedule_test.moc"
