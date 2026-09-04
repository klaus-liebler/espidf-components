#include "hoymiles_protocol.hh"
#include <cstring>

namespace hoymiles
{
    uint8_t Crc8(const uint8_t* buf, uint8_t len)
    {
        uint8_t crc = 0x00;
        for (uint8_t i = 0; i < len; i++)
        {
            crc ^= buf[i];
            for (uint8_t b = 0; b < 8; b++)
            {
                crc = (uint8_t)((crc << 1) ^ ((crc & 0x80) ? 0x01 : 0x00));
            }
        }
        return crc;
    }

    uint16_t Crc16Modbus(const uint8_t* buf, uint8_t len, uint16_t start)
    {
        uint16_t crc = start;
        for (uint8_t i = 0; i < len; i++)
        {
            crc = (uint16_t)(crc ^ buf[i]);
            for (uint8_t bit = 0; bit < 8; bit++)
            {
                if (crc & 0x0001)
                    crc = (uint16_t)((crc >> 1) ^ 0xA001);
                else
                    crc = (uint16_t)(crc >> 1);
            }
        }
        return crc;
    }

    void SerialToPacketId(uint64_t serial, uint8_t out4[4])
    {
        uint32_t low = (uint32_t)(serial & 0xFFFFFFFFu);
        out4[0] = (uint8_t)(low >> 24);
        out4[1] = (uint8_t)(low >> 16);
        out4[2] = (uint8_t)(low >> 8);
        out4[3] = (uint8_t)(low);
    }

    void SerialToRadioId(uint64_t serial, uint8_t outAddr5[5])
    {
        outAddr5[0] = 0x01;
        SerialToPacketId(serial, outAddr5 + 1);
    }

    uint8_t BuildRealTimeRunDataRequest(uint64_t inverterSerial, uint64_t dtuSerial, uint32_t unixTimeSec, uint8_t outBuf[27])
    {
        outBuf[0] = 0x15;
        SerialToPacketId(inverterSerial, outBuf + 1); // Byte 1-4: Ziel
        SerialToPacketId(dtuSerial, outBuf + 5);       // Byte 5-8: Quelle
        outBuf[9] = 0x80;                              // Fragment-Index (fix fuer 1-Fragment-Request)
        outBuf[10] = 0x0B;                             // DT = request real-time run data
        outBuf[11] = 0x00;
        outBuf[12] = (uint8_t)(unixTimeSec >> 24);
        outBuf[13] = (uint8_t)(unixTimeSec >> 16);
        outBuf[14] = (uint8_t)(unixTimeSec >> 8);
        outBuf[15] = (uint8_t)(unixTimeSec);
        memset(outBuf + 16, 0, 8); // Gap (16-19) + Password (20-23)

        uint16_t crc16 = Crc16Modbus(outBuf + 10, 14); // ueber Byte 10..23
        outBuf[24] = (uint8_t)(crc16 >> 8);
        outBuf[25] = (uint8_t)(crc16);
        outBuf[26] = Crc8(outBuf, 26);
        return 27;
    }

    bool CheckFragmentCrc(const uint8_t* fragment, uint8_t len)
    {
        if (len < 1) return false;
        return Crc8(fragment, (uint8_t)(len - 1)) == fragment[len - 1];
    }

    namespace
    {
        // Liest 'n' Big-Endian-Bytes ab p; bei isSigned Two's-Complement-Interpretation fuer n==2/n==4.
        int32_t ReadBE(const uint8_t* p, uint8_t n, bool isSigned)
        {
            uint32_t v = 0;
            for (uint8_t i = 0; i < n; i++) v = (v << 8) | p[i];
            if (isSigned)
            {
                if (n == 2) return (int16_t)(uint16_t)v;
                if (n == 4) return (int32_t)v;
            }
            return (int32_t)v;
        }
    }

    bool DecodeHm4chStatistics(const uint8_t* p, size_t len, HoymilesLiveData& d)
    {
        constexpr size_t MIN_LEN = 62;
        if (len < MIN_LEN) return false;

        auto rd = [&](int off, int n, int div, bool sig) -> float {
            return ReadBE(p + off, (uint8_t)n, sig) / (float)div;
        };

        d.Dc[0].UdcV = rd(2, 2, 10, false);
        d.Dc[0].IdcA = rd(4, 2, 100, false);
        d.Dc[1].IdcA = rd(6, 2, 100, false);
        d.Dc[0].PdcW = rd(8, 2, 10, false);
        d.Dc[1].PdcW = rd(10, 2, 10, false);
        d.Dc[0].YieldTotalKwh = rd(12, 4, 1000, false);
        d.Dc[1].YieldTotalKwh = rd(16, 4, 1000, false);
        d.Dc[0].YieldDayWh = rd(20, 2, 1, false);
        d.Dc[1].YieldDayWh = rd(22, 2, 1, false);
        d.Dc[2].UdcV = rd(24, 2, 10, false);
        d.Dc[2].IdcA = rd(26, 2, 100, false);
        d.Dc[3].IdcA = rd(28, 2, 100, false);
        d.Dc[2].PdcW = rd(30, 2, 10, false);
        d.Dc[3].PdcW = rd(32, 2, 10, false);
        d.Dc[2].YieldTotalKwh = rd(34, 4, 1000, false);
        d.Dc[3].YieldTotalKwh = rd(38, 4, 1000, false);
        d.Dc[2].YieldDayWh = rd(42, 2, 1, false);
        d.Dc[3].YieldDayWh = rd(44, 2, 1, false);
        // HM-1200/1500 hat nur 2 physische Spannungssensoren fuer je 2 Strings.
        d.Dc[1].UdcV = d.Dc[0].UdcV;
        d.Dc[3].UdcV = d.Dc[2].UdcV;

        d.Ac.UacV = rd(46, 2, 10, false);
        d.Ac.FrequencyHz = rd(48, 2, 100, false);
        d.Ac.PacW = rd(50, 2, 10, false);
        d.Ac.ReactivePowerVar = rd(52, 2, 10, true);
        d.Ac.IacA = rd(54, 2, 100, false);
        d.Ac.PowerFactor = rd(56, 2, 1000, false);

        d.Totals.TemperatureC = rd(58, 2, 10, true);
        d.Totals.EventLogCount = (uint16_t)ReadBE(p + 60, 2, false);

        float sumYieldDay = 0, sumYieldTotal = 0, sumPdc = 0;
        for (int i = 0; i < DC_CHANNEL_COUNT; i++)
        {
            sumYieldDay += d.Dc[i].YieldDayWh;
            sumYieldTotal += d.Dc[i].YieldTotalKwh;
            sumPdc += d.Dc[i].PdcW;
        }
        d.Totals.YieldDayWh = sumYieldDay;
        d.Totals.YieldTotalKwh = sumYieldTotal;
        d.Totals.PdcW = sumPdc;
        d.Totals.EfficiencyPct = (sumPdc > 0.0f) ? (d.Ac.PacW / sumPdc * 100.0f) : 0.0f;

        return true;
    }
}
