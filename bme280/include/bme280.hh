#include "bme280.h"
#include "bme280_defs.h"
#include <inttypes.h>
#include <i2c_sensor.hh>
#include <common-esp32.hh>
namespace BME280
{

    enum class ADDRESS : uint8_t
    {
        PRIM = UINT8_C(0x76),
        SEC = (0x77),
    };

    class M : public I2CSensor
    {
    private:
        struct bme280_dev dev;
        float tempDegCel{0.0};
        float pressurePa{0.0};
        float relHumidityPercent{0.0};
        uint32_t calculatedDelayMs{1000};

        static int8_t user_i2c_read(uint8_t reg_addr, uint8_t *reg_data, uint32_t len, void *intf_ptr)
        {
            M* myself = (M*)intf_ptr;
            return (uint8_t)myself->ReadRegs8(reg_addr, reg_data, len);
        }

        static int8_t user_i2c_write(uint8_t reg_addr, const uint8_t *reg_data, uint32_t len, void *intf_ptr)
        {
            M* myself = (M*)intf_ptr;
            return (uint8_t)myself->WriteRegs8(reg_addr, reg_data, len);
        }

        static void user_delay_us(uint32_t period, void *intf_ptr)
        {
            delayMs((period / 1000) + 1);
        }

    public:
        M(i2c_master_bus_handle_t bus_handle, ADDRESS adress);
        ~M();
        ErrorCode Initialize(int64_t &waitTillFirstTrigger) override;
        ErrorCode Trigger(int64_t &waitTillReadout) override;
        ErrorCode Readout(int64_t &waitTillNextTrigger) override;
        ErrorCode GetData(float *tempDegCel, float *pressurePa, float *relHumidityPercent);
    };

}