#pragma once
#include <cstdint>

namespace hoymiles
{
    constexpr int DC_CHANNEL_COUNT = 4;

    struct DcChannelData
    {
        float UdcV{0};
        float IdcA{0};
        float PdcW{0};
        float YieldDayWh{0};
        float YieldTotalKwh{0};
    };

    struct AcData
    {
        float UacV{0};
        float IacA{0};
        float PacW{0};
        float ReactivePowerVar{0};
        float FrequencyHz{0};
        float PowerFactor{0};
    };

    struct InverterTotals
    {
        float TemperatureC{0};
        uint16_t EventLogCount{0};
        float YieldDayWh{0};
        float YieldTotalKwh{0};
        float PdcW{0};
        float EfficiencyPct{0};
    };

    struct HoymilesLiveData
    {
        bool Reachable{false};
        bool Producing{false};
        // 0xFFFFFFFF, solange noch nie erfolgreich ein Datensatz empfangen wurde.
        uint32_t DataAgeMs{0xFFFFFFFFu};
        DcChannelData Dc[DC_CHANNEL_COUNT]{};
        AcData Ac{};
        InverterTotals Totals{};
    };

    class iHoymilesListener
    {
    public:
        virtual void OnLiveDataUpdated(const HoymilesLiveData& data) = 0;
        virtual ~iHoymilesListener() = default;
    };
}
