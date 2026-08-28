#pragma once

#include <QDateTime>
#include <QString>
#include <QtTypes>

namespace HyprShelld {

class AppearanceSchedule final {
public:
  enum class Status {
    Invalid,
    Ready,
    NoTransition,
  };

  enum class SolarCondition {
    NotApplicable,
    Normal,
    PolarDay,
    PolarNight,
  };

  struct Evaluation final {
    bool valid = false;
    bool isDark = false;
    Status status = Status::Invalid;
    SolarCondition solarCondition = SolarCondition::NotApplicable;
    QDateTime nextTransition;
    QDateTime nextRefresh;
    QDateTime sunrise;
    QDateTime sunset;
    double solarElevationDegrees = 0.0;
    // Zero is full night temperature and one is full day temperature.
    double daylightBlend = 0.0;
  };

  [[nodiscard]] static Evaluation evaluateTime(const QDateTime &now,
                                               quint32 darkStartMinute,
                                               quint32 lightStartMinute);
  [[nodiscard]] static Evaluation
  evaluateLocation(const QDateTime &now, double latitude, double longitude);
  [[nodiscard]] static QString statusName(Status status);
  [[nodiscard]] static quint32 targetTemperature(const Evaluation &evaluation,
                                                 quint32 nightTemperature,
                                                 quint32 dayTemperature,
                                                 bool gradual);
};

} // namespace HyprShelld
