#pragma once
#include <cinttypes>
#include <cmath>
#include <bit>
#include <cstdio>
#include <cstdint>

typedef int64_t tms_t;

namespace sunsetsunrise
{
    
    // Sunrise and Sunset DawnType
    enum class eDawn:int32_t{ //Value is 1E9*sin(dawn angle)
        NORMAL    = std::bit_cast<std::int32_t>(sinf( -0.8333 * M_PI / 180.0)),
        CIVIL     = std::bit_cast<std::int32_t>(sinf( -6.0000 * M_PI / 180.0)),
        NAUTIC    = std::bit_cast<std::int32_t>(sinf(-12.0000 * M_PI / 180.0)),
        ASTRONOMIC= std::bit_cast<std::int32_t>(sinf(-18.0000 * M_PI / 180.0)),
    };

    #define RAD (M_PI / 180.0)
    #define JD2000 (2451544.5)
    #define JD1970 (2440587.5) //2440587.5 is the julian day at 1/1/1970 0:00 UTC.
    #define UNIX_TIME_SECS_1_1_2000 (946684800)
    float JulianDate(time_t unixSecs)
    {
        return (unixSecs / 86400.0) + JD1970; 
    }

    int32_t DaysSinceJan1_2020UTC(time_t unixSecs){
        return (unixSecs-UNIX_TIME_SECS_1_1_2000) / 86400.0; 
    }
    
    template <typename T>
    T eps(T Tdays) // Neigung der Erdachse
    {
        return RAD*(23.43929111 + (-46.8150*(Tdays/36525.0f) - 0.00059*(Tdays/36525.0f)*(Tdays/36525.0f) + 0.001813*(Tdays/36525.0f)*(Tdays/36525.0f)*(Tdays/36525.0f))/3600.0);
    }

    // Time formula
    // Tdays is the number of days since Jan 1 2000, and replaces T as the Tropical Century. T = Tdays / 36525.0
    //DK=declination
    //RA = right ascension
    template <typename T>
    T TimeFormulaHours(T &DK, int32_t Tdays)
    {
        T RA_Mean = std::fmod(18.71506921 + (2400.0513369 / 36525.0) * Tdays, 24.);//+(2.5862e-5 - 1.72e-9*T)*T*T;; // we keep only first order value as T is between 0.20 and 0.30
        T M = std::fmod(((2. * M_PI * 0.993133) + (2. * M_PI * 99.997361 / 36525.0) * Tdays), 2. * M_PI);
        T L = std::fmod((2. * M_PI * 0.7859453) + M + (6893.0 * sin(M) + 72.0 * sin(M + M) + (6191.2 / 36525.0) * Tdays) * (2. * M_PI / 1296.0e3), 2. * M_PI);

        //double e = eps(Tdays);
        //double cos_eps=cos(e);
        //double sin_eps=sin(e);
        T cos_eps = 0.91750f;     // precompute cos(eps) ->okkl!
        T sin_eps = 0.39773f;     // precompute sin(eps) -->okkl
	    T RA = atan(tan(L)*cos_eps);
        

        if (RA < 0.0f) RA += M_PI;
        if (L > M_PI) RA += M_PI;
        
        RA = RA * (24. / (2. * M_PI));
        
        printf("M=%f, L=%f, RA_mean=%f, RA=%f\n", M, L, RA_Mean, RA);
        
        DK = asin(sin_eps * sin(L));
        // Damit 0<=RA_Mittel<24 -->integared in first line
        T dRA = RA_Mean - RA;
        if (dRA < -12.0)
            dRA += 24.0;
        if (dRA > 12.0)
            dRA -= 24.0;

        dRA = dRA * 1.0027379;
        return dRA;
    }

    constexpr tms_t ONE_DAY{24*60*60};

    template <typename T>
    void NextSunriseAndSunset(time_t unixSecs, T latitudeRadiant, T longitudeRadiant, eDawn dawnLevel, tms_t& nextSunrise, tms_t& nextSunset)
    {
        //wir gehen dann einen Tag nach vorne, runden durch ganzzahlige division auf Mitternacht und multiplizieren, um wieder die Sekunden zu bekommen
        tms_t nextMidnight=((unixSecs+ONE_DAY)/ONE_DAY)*ONE_DAY;
        T DK{0};
        int32_t Tdays = DaysSinceJan1_2020UTC(nextMidnight);
        T discrepancy = TimeFormulaHours<T>(DK, Tdays);
        T trueSunPeak = 12.0 - discrepancy;
        const T localTimeShift=longitudeRadiant/(RAD * 15.0f);
        const float sin_h = std::bit_cast<float>(dawnLevel);
        T timeDifference = 12.0*acos((sin_h - sin(latitudeRadiant) * sin(DK)) / (cos(latitudeRadiant) * cos(DK)))/M_PI;
        T sunriseWorldTimeHours = trueSunPeak - timeDifference - localTimeShift ;
        T sunsetWorldTimeHours  = trueSunPeak + timeDifference - localTimeShift;
        nextSunrise=nextMidnight+sunriseWorldTimeHours*60*60;
        nextSunset=nextMidnight+sunsetWorldTimeHours*60*60;
        //printf("nextMidnight=%ld, nextSunrise=%ld, nextSunset=%ld", nextMidnight, nextSunrise, nextSunset);
    }
    template <typename T>
    void DuskTillDawn(time_t unixSecs, T latitudeRadiant, T longitudeRadiant, eDawn dawnLevel)
    {

        T DK{0};
        int32_t Tdays = DaysSinceJan1_2020UTC(unixSecs);
        printf("Tdays=%d; T=%f\n", Tdays, Tdays/36525.0);
        T discrepancy = TimeFormulaHours<T>(DK, Tdays);
        T trueSunPeak = 12.0 - discrepancy;
        const T localTimeShift=longitudeRadiant/(RAD * 15.0f);
        const float sin_h = std::bit_cast<float>(dawnLevel);
        T timeDifference = 12.0*acos((sin_h - sin(latitudeRadiant) * sin(DK)) / (cos(latitudeRadiant) * cos(DK)))/M_PI;
        
        T sunriseWorldTime = trueSunPeak - timeDifference - localTimeShift ;
        T sunsetWorldTime  = trueSunPeak + timeDifference - localTimeShift;
        printf("DK=%f; discrepancy=%f; timeDifference=%f; sin_h=%f; sin(lat)=%f; sunriseWorld=%f; sunsetWorld=%f\n", DK, discrepancy, timeDifference, sin_h, sin(latitudeRadiant), sunriseWorldTime, sunsetWorldTime);

        
        const float Zeitzone = 1;

        T Aufgang = sunriseWorldTime + Zeitzone + (1 / 120.0f); // In hours, with rounding to nearest minute (1/60 * .5)
        Aufgang = std::fmod(Aufgang, 24.0f); // force 0 <= x < 24.0
        int AufgangStunden = (int)Aufgang;
        int AufgangMinuten = (int)(60.0f * std::fmod(Aufgang, 1.0f));
        
        T Untergang = sunsetWorldTime + Zeitzone;
        Untergang = std::fmod(Untergang, 24.0f);
        int UntergangStunden = (int)Untergang;
        int UntergangMinuten = (int)(60.0f * std::fmod(Untergang, 1.0f));
       
        //printf("Aufgang: %d:%d, Untergang %d:%d", AufgangStunden, AufgangMinuten, UntergangStunden, UntergangMinuten);
    }
}

/*
int main()
{
    double today = 1128038400;//30. September 2005
    double latDeg = 50;//52.0965;
    double lonDeg = 10;//7.6171;
    tms_t nextSunrise{0};
    tms_t nextSunset{0};
    sunsetsunrise::NextSunriseAndSunset(today, latDeg * RAD, lonDeg * RAD, sunsetsunrise::eDawn::NORMAL, nextSunrise, nextSunset);
    return 0;
}
*/