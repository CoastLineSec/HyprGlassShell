#include "appearance_schedule.h"

#include <QDate>
#include <QTime>
#include <QTimeZone>
#include <QVector>

#include <algorithm>
#include <cmath>
#include <limits>

namespace HyprShelld {
namespace {

constexpr quint32 minutesPerDay = 24U * 60U;
constexpr qint64 millisecondsPerDay = 24LL * 60LL * 60LL * 1000LL;
constexpr double pi = 3.14159265358979323846;
constexpr double radiansPerDegree = pi / 180.0;
constexpr double julianAtUnixEpoch = 2440588.0;
constexpr double julianAtJ2000 = 2451545.0;
constexpr double solarCycleOffset = 0.0009;
constexpr double civilSunriseAltitude = -0.833 * radiansPerDegree;
constexpr int solarTransitionSearchDays = 370;

struct TimeBoundary final {
  QDateTime at;
  bool darkAfter = false;
};

struct SolarTimes final {
  AppearanceSchedule::SolarCondition condition =
      AppearanceSchedule::SolarCondition::Normal;
  QDateTime sunrise;
  QDateTime sunset;
};

[[nodiscard]] double toJulian(const QDateTime &dateTime) {
  return static_cast<double>(dateTime.toMSecsSinceEpoch()) /
             static_cast<double>(millisecondsPerDay) -
         0.5 + julianAtUnixEpoch;
}

[[nodiscard]] QDateTime fromJulian(const double julian,
                                   const QTimeZone &timeZone) {
  const auto milliseconds =
      std::llround((julian + 0.5 - julianAtUnixEpoch) *
                   static_cast<double>(millisecondsPerDay));
  return QDateTime::fromMSecsSinceEpoch(milliseconds, QTimeZone::utc())
      .toTimeZone(timeZone);
}

[[nodiscard]] double solarMeanAnomaly(const double days) {
  return radiansPerDegree * (357.5291 + 0.98560028 * days);
}

[[nodiscard]] double eclipticLongitude(const double anomaly) {
  const auto equationOfCenter =
      radiansPerDegree *
      (1.9148 * std::sin(anomaly) + 0.0200 * std::sin(2.0 * anomaly) +
       0.0003 * std::sin(3.0 * anomaly));
  constexpr double perihelion = 102.9372 * radiansPerDegree;
  return anomaly + equationOfCenter + perihelion + pi;
}

[[nodiscard]] double declination(const double longitude) {
  constexpr double obliquity = 23.4397 * radiansPerDegree;
  return std::asin(std::sin(longitude) * std::sin(obliquity));
}

[[nodiscard]] double rightAscension(const double longitude) {
  constexpr double obliquity = 23.4397 * radiansPerDegree;
  return std::atan2(std::sin(longitude) * std::cos(obliquity),
                    std::cos(longitude));
}

[[nodiscard]] double solarTransitJulian(const double approximateTransit,
                                        const double anomaly,
                                        const double longitude) {
  return julianAtJ2000 + approximateTransit + 0.0053 * std::sin(anomaly) -
         0.0069 * std::sin(2.0 * longitude);
}

[[nodiscard]] QDateTime localBoundary(const QDate &date,
                                      const quint32 minuteOfDay,
                                      const QTimeZone &timeZone) {
  const auto time = QTime(static_cast<int>(minuteOfDay / 60U),
                          static_cast<int>(minuteOfDay % 60U));
  // A wall-clock time inside a DST gap moves forward by that gap. During a
  // repeated hour, the first occurrence wins. Each configured boundary is
  // therefore applied exactly once while retaining calendar-day semantics.
  return QDateTime(date, time, timeZone,
                   QDateTime::TransitionResolution::RelativeToBefore);
}

[[nodiscard]] QDateTime nextLocalMidnight(const QDateTime &now,
                                          const QTimeZone &timeZone) {
  auto date = now.toTimeZone(timeZone).date().addDays(1);
  for (auto attempt = 0; attempt < 3; ++attempt) {
    const QDateTime midnight(date, QTime(0, 0), timeZone,
                             QDateTime::TransitionResolution::RelativeToBefore);
    if (midnight.isValid() && midnight > now) {
      return midnight;
    }
    date = date.addDays(1);
  }
  return {};
}

[[nodiscard]] SolarTimes calculateSolarTimes(const QDate &date,
                                             const QTimeZone &timeZone,
                                             const double latitude,
                                             const double longitude) {
  const QDateTime localNoon(date, QTime(12, 0), timeZone,
                            QDateTime::TransitionResolution::RelativeToBefore);
  const auto days = toJulian(localNoon) - julianAtJ2000;
  const auto westLongitude = -longitude * radiansPerDegree;
  const auto latitudeRadians = latitude * radiansPerDegree;
  const auto cycle =
      std::round(days - solarCycleOffset - westLongitude / (2.0 * pi));
  const auto approximateTransit =
      solarCycleOffset + westLongitude / (2.0 * pi) + cycle;
  const auto anomaly = solarMeanAnomaly(approximateTransit);
  const auto solarLongitude = eclipticLongitude(anomaly);
  const auto solarDeclination = declination(solarLongitude);
  const auto solarNoon =
      solarTransitJulian(approximateTransit, anomaly, solarLongitude);

  const auto numerator = std::sin(civilSunriseAltitude) -
                         std::sin(latitudeRadians) * std::sin(solarDeclination);
  const auto denominator =
      std::cos(latitudeRadians) * std::cos(solarDeclination);
  const auto cosineHourAngle = numerator / denominator;
  if (cosineHourAngle > 1.0) {
    return {
        .condition = AppearanceSchedule::SolarCondition::PolarNight,
        .sunrise = {},
        .sunset = {},
    };
  }
  if (cosineHourAngle < -1.0) {
    return {
        .condition = AppearanceSchedule::SolarCondition::PolarDay,
        .sunrise = {},
        .sunset = {},
    };
  }

  const auto hourAngle = std::acos(std::clamp(cosineHourAngle, -1.0, 1.0));
  const auto setApproximation =
      solarCycleOffset + (hourAngle + westLongitude) / (2.0 * pi) + cycle;
  const auto sunsetJulian =
      solarTransitJulian(setApproximation, anomaly, solarLongitude);
  const auto sunriseJulian = solarNoon - (sunsetJulian - solarNoon);
  return {
      .condition = AppearanceSchedule::SolarCondition::Normal,
      .sunrise = fromJulian(sunriseJulian, timeZone),
      .sunset = fromJulian(sunsetJulian, timeZone),
  };
}

[[nodiscard]] double solarElevation(const QDateTime &now, const double latitude,
                                    const double longitude) {
  const auto days = toJulian(now.toUTC()) - julianAtJ2000;
  const auto anomaly = solarMeanAnomaly(days);
  const auto solarLongitude = eclipticLongitude(anomaly);
  const auto solarDeclination = declination(solarLongitude);
  const auto ascension = rightAscension(solarLongitude);
  const auto westLongitude = -longitude * radiansPerDegree;
  const auto siderealTime =
      radiansPerDegree * (280.16 + 360.9856235 * days) - westLongitude;
  const auto hourAngle = siderealTime - ascension;
  const auto latitudeRadians = latitude * radiansPerDegree;
  return std::asin(std::sin(latitudeRadians) * std::sin(solarDeclination) +
                   std::cos(latitudeRadians) * std::cos(solarDeclination) *
                       std::cos(hourAngle));
}

[[nodiscard]] QDateTime findNextSolarTransition(const QDateTime &now,
                                                const QTimeZone &timeZone,
                                                const double latitude,
                                                const double longitude,
                                                const bool initiallyDark) {
  QVector<TimeBoundary> boundaries;
  boundaries.reserve((solarTransitionSearchDays + 2) * 2);
  const auto firstDate = now.toTimeZone(timeZone).date().addDays(-1);
  for (auto offset = 0; offset <= solarTransitionSearchDays + 1; ++offset) {
    const auto times = calculateSolarTimes(firstDate.addDays(offset), timeZone,
                                           latitude, longitude);
    if (times.condition != AppearanceSchedule::SolarCondition::Normal) {
      continue;
    }
    boundaries.push_back({times.sunrise, false});
    boundaries.push_back({times.sunset, true});
  }
  std::sort(boundaries.begin(), boundaries.end(),
            [](const TimeBoundary &left, const TimeBoundary &right) {
              if (left.at == right.at) {
                return left.darkAfter < right.darkAfter;
              }
              return left.at < right.at;
            });

  auto dark = initiallyDark;
  qint64 previousMilliseconds = std::numeric_limits<qint64>::min();
  bool previousDarkAfter = false;
  for (const auto &boundary : boundaries) {
    if (!boundary.at.isValid() || boundary.at <= now) {
      continue;
    }
    const auto milliseconds = boundary.at.toMSecsSinceEpoch();
    if (milliseconds == previousMilliseconds &&
        boundary.darkAfter == previousDarkAfter) {
      continue;
    }
    previousMilliseconds = milliseconds;
    previousDarkAfter = boundary.darkAfter;
    if (boundary.darkAfter != dark) {
      return boundary.at;
    }
    dark = boundary.darkAfter;
  }
  return {};
}

} // namespace

AppearanceSchedule::Evaluation
AppearanceSchedule::evaluateTime(const QDateTime &now,
                                 const quint32 darkStartMinute,
                                 const quint32 lightStartMinute) {
  if (!now.isValid() || darkStartMinute >= minutesPerDay ||
      lightStartMinute >= minutesPerDay ||
      darkStartMinute == lightStartMinute) {
    return {};
  }

  const auto timeZone = now.timeZone();
  if (!timeZone.isValid()) {
    return {};
  }
  const auto localNow = now.toTimeZone(timeZone);
  const auto today = localNow.date();
  QVector<TimeBoundary> boundaries;
  boundaries.reserve(10);
  for (auto offset = -2; offset <= 2; ++offset) {
    const auto date = today.addDays(offset);
    boundaries.push_back({
        localBoundary(date, darkStartMinute, timeZone),
        true,
    });
    boundaries.push_back({
        localBoundary(date, lightStartMinute, timeZone),
        false,
    });
  }
  std::sort(boundaries.begin(), boundaries.end(),
            [](const TimeBoundary &left, const TimeBoundary &right) {
              return left.at < right.at;
            });

  auto foundCurrent = false;
  auto isDark = false;
  QDateTime nextTransition;
  for (const auto &boundary : boundaries) {
    if (!boundary.at.isValid()) {
      continue;
    }
    if (boundary.at <= localNow) {
      isDark = boundary.darkAfter;
      foundCurrent = true;
      continue;
    }
    nextTransition = boundary.at;
    break;
  }
  if (!foundCurrent || !nextTransition.isValid()) {
    return {};
  }

  return {
      .valid = true,
      .isDark = isDark,
      .status = Status::Ready,
      .solarCondition = SolarCondition::NotApplicable,
      .nextTransition = nextTransition,
      .nextRefresh = nextTransition,
      .sunrise = localBoundary(today, lightStartMinute, timeZone),
      .sunset = localBoundary(today, darkStartMinute, timeZone),
      .solarElevationDegrees = 0.0,
      .daylightBlend = isDark ? 0.0 : 1.0,
  };
}

AppearanceSchedule::Evaluation AppearanceSchedule::evaluateLocation(
    const QDateTime &now, const double latitude, const double longitude) {
  if (!now.isValid() || !std::isfinite(latitude) || !std::isfinite(longitude) ||
      latitude < -90.0 || latitude > 90.0 || longitude < -180.0 ||
      longitude > 180.0) {
    return {};
  }
  const auto timeZone = now.timeZone();
  if (!timeZone.isValid()) {
    return {};
  }
  const auto localNow = now.toTimeZone(timeZone);
  const auto times =
      calculateSolarTimes(localNow.date(), timeZone, latitude, longitude);
  const auto elevationRadians = solarElevation(localNow, latitude, longitude);
  const auto elevationDegrees = elevationRadians / radiansPerDegree;
  const auto daylightBlend =
      std::clamp((elevationDegrees + 6.0) / 9.0, 0.0, 1.0);

  auto isDark = false;
  auto status = Status::Ready;
  if (times.condition == SolarCondition::Normal) {
    isDark = localNow < times.sunrise || localNow >= times.sunset;
  } else if (times.condition == SolarCondition::PolarNight) {
    isDark = true;
    status = Status::NoTransition;
  } else {
    isDark = false;
    status = Status::NoTransition;
  }

  const auto nextTransition =
      findNextSolarTransition(localNow, timeZone, latitude, longitude, isDark);
  auto nextRefresh = nextTransition;
  if (status == Status::NoTransition || !nextRefresh.isValid()) {
    nextRefresh = nextLocalMidnight(localNow, timeZone);
  }

  return {
      .valid = true,
      .isDark = isDark,
      .status = status,
      .solarCondition = times.condition,
      .nextTransition = nextTransition,
      .nextRefresh = nextRefresh,
      .sunrise = times.sunrise,
      .sunset = times.sunset,
      .solarElevationDegrees = elevationDegrees,
      .daylightBlend = daylightBlend,
  };
}

QString AppearanceSchedule::statusName(const Status status) {
  switch (status) {
  case Status::Invalid:
    return QStringLiteral("invalid");
  case Status::Ready:
    return QStringLiteral("ready");
  case Status::NoTransition:
    return QStringLiteral("no-transition");
  }
  Q_UNREACHABLE_RETURN(QStringLiteral("invalid"));
}

quint32 AppearanceSchedule::targetTemperature(const Evaluation &evaluation,
                                              const quint32 nightTemperature,
                                              const quint32 dayTemperature,
                                              const bool gradual) {
  // Invalid or unavailable schedules stay neutral instead of unexpectedly
  // warming the display.
  if (!evaluation.valid) {
    return dayTemperature;
  }
  const auto blend = gradual ? std::clamp(evaluation.daylightBlend, 0.0, 1.0)
                             : (evaluation.isDark ? 0.0 : 1.0);
  const auto night = static_cast<double>(nightTemperature);
  const auto day = static_cast<double>(dayTemperature);
  const auto temperature = std::llround(night + (day - night) * blend);
  return static_cast<quint32>(
      std::clamp<qint64>(temperature, 0, std::numeric_limits<quint32>::max()));
}

} // namespace HyprShelld
