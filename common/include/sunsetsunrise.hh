#pragma once
// Sonnenauf- und -untergang (auch buergerliche/nautische/astronomische Daemmerung) fuer einen Ort,
// nach der Naeherung von Arnold Barmettler (Genauigkeit: wenige Minuten fuer mittlere Breiten).
// Alle Zeiten in Unix-Sekunden (UTC); Breite/Laenge im Bogenmass (Ost/Nord positiv).
#include <cmath>
#include <cstdint>
#include <ctime>
#include <numbers>

namespace sunsetsunrise
{
    constexpr double PI = std::numbers::pi;
    constexpr double DEG = PI / 180.0; // Grad -> Bogenmass
    constexpr double JD1970 = 2440587.5; // julianisches Datum am 1.1.1970 0:00 UTC
    constexpr int64_t UNIX_TIME_SECS_1_1_2000 = 946684800;
    constexpr int64_t ONE_DAY = 24 * 60 * 60;

    // Hoehe des Sonnenmittelpunkts ueber dem Horizont, ab der "Tag" ist, in Tausendstel Grad
    enum class eDawn : int32_t
    {
        NORMAL = -833,         // Sonnenauf-/-untergang (Refraktion + Sonnenradius)
        CIVIL = -6000,         // buergerliche Daemmerung
        NAUTIC = -12000,       // nautische Daemmerung
        ASTRONOMIC = -18000,   // astronomische Daemmerung
    };

    inline double JulianDate(time_t unixSecs)
    {
        return (unixSecs / 86400.0) + JD1970;
    }

    inline int32_t DaysSinceJan1_2000UTC(time_t unixSecs)
    {
        return (int32_t)((unixSecs - UNIX_TIME_SECS_1_1_2000) / ONE_DAY);
    }

    // Zeitgleichung in Stunden; DK = Deklination der Sonne. Tdays = Tage seit 1.1.2000.
    template <typename T>
    T TimeFormulaHours(T &DK, int32_t Tdays)
    {
        T RA_Mean = std::fmod(18.71506921 + (2400.0513369 / 36525.0) * Tdays, 24.);
        T M = std::fmod(((2. * PI * 0.993133) + (2. * PI * 99.997361 / 36525.0) * Tdays), 2. * PI);
        T L = std::fmod((2. * PI * 0.7859453) + M + (6893.0 * std::sin(M) + 72.0 * std::sin(M + M) + (6191.2 / 36525.0) * Tdays) * (2. * PI / 1296.0e3), 2. * PI);
        const T cos_eps = 0.91750; // Neigung der Erdachse (eps ~ 23.44 Grad)
        const T sin_eps = 0.39773;
        T RA = std::atan(std::tan(L) * cos_eps);
        if (RA < 0.0)
            RA += PI;
        if (L > PI)
            RA += PI;
        RA = RA * (24. / (2. * PI));
        DK = std::asin(sin_eps * std::sin(L));
        T dRA = RA_Mean - RA;
        if (dRA < -12.0)
            dRA += 24.0;
        if (dRA > 12.0)
            dRA -= 24.0;
        return dRA * 1.0027379;
    }

    // Auf- und Untergang am UTC-Kalendertag, in dem unixSecs liegt. false, wenn die Sonne an diesem
    // Tag die Hoehe dawnLevel nicht kreuzt (Polartag/-nacht); die Ausgaben sind dann unveraendert.
    template <typename T>
    bool SunriseAndSunsetOfDay(time_t unixSecs, T latitudeRadiant, T longitudeRadiant, eDawn dawnLevel, time_t &sunriseUnixSecs, time_t &sunsetUnixSecs)
    {
        const time_t midnight = (unixSecs / ONE_DAY) * ONE_DAY;
        T DK{0};
        const T discrepancy = TimeFormulaHours<T>(DK, DaysSinceJan1_2000UTC(midnight));
        const T trueSunPeak = 12.0 - discrepancy;
        const T localTimeShift = longitudeRadiant / (DEG * 15.0);
        const T sin_h = std::sin(((T)(int32_t)dawnLevel / 1000.0) * DEG);
        const T cosHourAngle = (sin_h - std::sin(latitudeRadiant) * std::sin(DK)) / (std::cos(latitudeRadiant) * std::cos(DK));
        if (cosHourAngle < -1.0 || cosHourAngle > 1.0)
        {
            return false;
        }
        const T timeDifference = 12.0 * std::acos(cosHourAngle) / PI;
        sunriseUnixSecs = midnight + (time_t)std::lround((trueSunPeak - timeDifference - localTimeShift) * 3600.0);
        sunsetUnixSecs = midnight + (time_t)std::lround((trueSunPeak + timeDifference - localTimeShift) * 3600.0);
        return true;
    }

    // Der naechste Aufgang und der naechste Untergang NACH unixSecs (jeweils heute oder morgen).
    template <typename T>
    bool NextSunriseAndSunset(time_t unixSecs, T latitudeRadiant, T longitudeRadiant, eDawn dawnLevel, time_t &nextSunriseUnixSecs, time_t &nextSunsetUnixSecs)
    {
        time_t rise, set, riseTomorrow, setTomorrow;
        if (!SunriseAndSunsetOfDay<T>(unixSecs, latitudeRadiant, longitudeRadiant, dawnLevel, rise, set) ||
            !SunriseAndSunsetOfDay<T>(unixSecs + ONE_DAY, latitudeRadiant, longitudeRadiant, dawnLevel, riseTomorrow, setTomorrow))
        {
            return false;
        }
        nextSunriseUnixSecs = rise > unixSecs ? rise : riseTomorrow;
        nextSunsetUnixSecs = set > unixSecs ? set : setTomorrow;
        return true;
    }
}
