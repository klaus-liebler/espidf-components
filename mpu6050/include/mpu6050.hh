#pragma once

#include <stdio.h>
#include <stdint.h>
#include <i2c.hh>
#include <i2c_sensor.hh>
#include <common-esp32.hh>

namespace MPU6050
{

    enum class I2C_ADDRESS
    {
        AD0_LOW = 0x68,
        AD0_HIGH = 0x69u,
    };

    constexpr uint8_t WHO_AM_I_VAL{0x68u};
    constexpr float ALPHA{0.99f};             /*!< Weight of gyroscope */
    constexpr float RAD_TO_DEG{57.27272727f}; /*!< Radians to degrees */

    /* MPU6050 register */
    constexpr uint8_t MPU6050_GYRO_CONFIG{0x1Bu};
    constexpr uint8_t MPU6050_ACCEL_CONFIG{0x1Cu};
    constexpr uint8_t MPU6050_INTR_PIN_CFG{0x37u};
    constexpr uint8_t MPU6050_INTR_ENABLE{0x38u};
    constexpr uint8_t MPU6050_INTR_STATUS{0x3Au};
    constexpr uint8_t MPU6050_ACCEL_XOUT_H{0x3Bu};
    constexpr uint8_t MPU6050_GYRO_XOUT_H{0x43u};
    constexpr uint8_t MPU6050_TEMP_XOUT_H{0x41u};
    constexpr uint8_t MPU6050_PWR_MGMT_1{0x6Bu};
    constexpr uint8_t MPU6050_WHO_AM_I{0x75u};

    constexpr uint8_t MPU6050_DATA_RDY_INT_BIT = (uint8_t)BIT0;
    constexpr uint8_t MPU6050_I2C_MASTER_INT_BIT = (uint8_t)BIT3;
    constexpr uint8_t MPU6050_FIFO_OVERFLOW_INT_BIT = (uint8_t)BIT4;
    constexpr uint8_t MPU6050_MOT_DETECT_INT_BIT = (uint8_t)BIT6;
    constexpr uint8_t MPU6050_ALL_INTERRUPTS = (MPU6050_DATA_RDY_INT_BIT | MPU6050_I2C_MASTER_INT_BIT | MPU6050_FIFO_OVERFLOW_INT_BIT | MPU6050_MOT_DETECT_INT_BIT);

    enum class ACCELERATION_FS
    {
        _2G = 0,  /*!< Accelerometer full scale range is +/- 2g */
        _4G = 1,  /*!< Accelerometer full scale range is +/- 4g */
        _8G = 2,  /*!< Accelerometer full scale range is +/- 8g */
        _16G = 3, /*!< Accelerometer full scale range is +/- 16g */
    };

    enum class GYRO_FS
    {
        _250DPS = 0,  /*!< Gyroscope full scale range is +/- 250 degree per sencond */
        _500DPS = 1,  /*!< Gyroscope full scale range is +/- 500 degree per sencond */
        _1000DPS = 2, /*!< Gyroscope full scale range is +/- 1000 degree per sencond */
        _2000DPS = 3, /*!< Gyroscope full scale range is +/- 2000 degree per sencond */
    };

    enum class INTERRUPT_PIN_ACTIVE_LEVEL
    {
        HIGH = 0, /*!< The mpu6050 sets its INT pin HIGH on interrupt */
        LOW = 1   /*!< The mpu6050 sets its INT pin LOW on interrupt */
    };

    enum class INTERRUPT_PIN_MODE
    {
        PUSH_PULL = 0, /*!< The mpu6050 configures its INT pin as push-pull */
        OPEN_DRAIN = 1 /*!< The mpu6050 configures its INT pin as open drain*/
    };

    enum class INTERRUPT_LATCH
    {
        _50US = 0,         /*!< The mpu6050 produces a 50 microsecond pulse on interrupt */
        _UNTIL_CLEARED = 1 /*!< The mpu6050 latches its INT pin to its active level, until interrupt is cleared */
    };

    enum class INTERRUPT_CLEAR
    {
        ON_ANY_READ = 0,   /*!< INT_STATUS register bits are cleared on any register read */
        ON_STATUS_READ = 1 /*!< INT_STATUS register bits are cleared only by reading INT_STATUS value*/
    };

    extern const uint8_t MPU6050_DATA_RDY_INT_BIT;      /*!< DATA READY interrupt bit               */
    extern const uint8_t MPU6050_I2C_MASTER_INT_BIT;    /*!< I2C MASTER interrupt bit               */
    extern const uint8_t MPU6050_FIFO_OVERFLOW_INT_BIT; /*!< FIFO Overflow interrupt bit            */
    extern const uint8_t MPU6050_MOT_DETECT_INT_BIT;    /*!< MOTION DETECTION interrupt bit         */
    extern const uint8_t MPU6050_ALL_INTERRUPTS;        /*!< All interrupts supported by mpu6050    */

    struct xyz_i16
    {
        int16_t x;
        int16_t y;
        int16_t z;
    };

    struct xyz_f32
    {
        float x;
        float y;
        float z;
    };

    struct rollpitch_f32
    {
        float roll;
        float pitch;
    };

    class M
    {
    private:
        iI2CPort *i2c_port;
        uint8_t address;
        gpio_num_t interrupt_pin;
        uint32_t counter;
        float dt; /*!< delay time between two measurements, dt should be small (ms level) */
        struct timeval *timer;
        float acce_sensitivity;
        float gyro_sensitivity;

    public:
        M(iI2CPort *i2c_port, I2C_ADDRESS address = I2C_ADDRESS::AD0_LOW, gpio_num_t interrupt_pin = GPIO_NUM_NC) : i2c_port(i2c_port), address((uint8_t)address), interrupt_pin(interrupt_pin), counter(0), dt(0)
        {
            timer = (struct timeval *)calloc(1, sizeof(struct timeval));
        }

        ErrorCode GetDeviceId(uint8_t *const deviceid)
        {
            return i2c_port->ReadReg(address, MPU6050_WHO_AM_I, deviceid, 1);
        }

        ErrorCode WakeUp()
        {
            uint8_t tmp;
            RETURN_ON_ERRORCODE(i2c_port->ReadReg(address, MPU6050_PWR_MGMT_1, &tmp, 1));
            tmp &= (~BIT6);
            return i2c_port->WriteSingleReg(address, MPU6050_PWR_MGMT_1, tmp);
        }

        ErrorCode Sleep()
        {
            uint8_t tmp;
            RETURN_ON_ERRORCODE(i2c_port->ReadReg(address, MPU6050_PWR_MGMT_1, &tmp, 1));
            tmp |= BIT6;
            return i2c_port->WriteSingleReg(address, MPU6050_PWR_MGMT_1, tmp);
        }

        ErrorCode Config(const ACCELERATION_FS acce_fs, const GYRO_FS gyro_fs)
        {
            uint8_t config_regs[2] = {(uint8_t)((uint8_t)gyro_fs << 3), (uint8_t)((uint8_t)acce_fs << 3)};
            RETURN_ON_ERRORCODE(i2c_port->WriteReg(address, MPU6050_GYRO_CONFIG, config_regs, 2));
            switch (acce_fs)
            {
            case ACCELERATION_FS::_2G:
                acce_sensitivity = 16384;
                break;

                case ACCELERATION_FS::_4G:
                acce_sensitivity = 8192;
                break;

            case ACCELERATION_FS::_8G:
                acce_sensitivity = 4096;
                break;

            case ACCELERATION_FS::_16G:
                acce_sensitivity = 2048;
                break;
            }
            switch (gyro_fs)
            {
            case GYRO_FS::_250DPS:
                gyro_sensitivity = 131;
                break;

            case GYRO_FS::_500DPS:
                gyro_sensitivity = 65.5;
                break;

            case GYRO_FS::_1000DPS:
                gyro_sensitivity = 32.8;
                break;

            case GYRO_FS::_2000DPS:
                gyro_sensitivity = 16.4;
                break;

            default:
                break;
            }
            return ErrorCode::OK;
        }

        ErrorCode ConfigInterrupts(INTERRUPT_PIN_ACTIVE_LEVEL activeLevel, INTERRUPT_PIN_MODE pinMode, INTERRUPT_LATCH latch, INTERRUPT_CLEAR clear_behavior)
        {
            if (!GPIO_IS_VALID_GPIO(interrupt_pin))
            {
                return ErrorCode::PIN_NOT_AVAILABLE;
            }

            uint8_t int_pin_cfg = 0x00;

            RETURN_ON_ERRORCODE(i2c_port->ReadReg(address, MPU6050_INTR_PIN_CFG, &int_pin_cfg, 1));

            if (INTERRUPT_PIN_ACTIVE_LEVEL::LOW == activeLevel)
            {
                int_pin_cfg |= BIT7;
            }

            if (INTERRUPT_PIN_MODE::OPEN_DRAIN == pinMode)
            {
                int_pin_cfg |= BIT6;
            }

            if (INTERRUPT_LATCH::_UNTIL_CLEARED == latch)
            {
                int_pin_cfg |= BIT5;
            }

            if (INTERRUPT_CLEAR::ON_ANY_READ == clear_behavior)
            {
                int_pin_cfg |= BIT4;
            }
            RETURN_ON_ERRORCODE(i2c_port->WriteSingleReg(address, MPU6050_INTR_PIN_CFG, int_pin_cfg));

            gpio_int_type_t gpio_intr_type = INTERRUPT_PIN_ACTIVE_LEVEL::LOW == activeLevel ? GPIO_INTR_NEGEDGE : GPIO_INTR_POSEDGE;

            gpio_config_t int_gpio_config = {};

            int_gpio_config.mode = GPIO_MODE_INPUT;
            int_gpio_config.intr_type = gpio_intr_type;
            int_gpio_config.pin_bit_mask = (BIT0 << interrupt_pin);

            RETURN_ERRORCODE_ON_ERROR(gpio_config(&int_gpio_config), ErrorCode::GENERIC_ERROR);

            return ErrorCode::OK;
        }

        ErrorCode EnableInterrupts(uint8_t interrupt_sources)
        {

            uint8_t enabled_interrupts = 0x00;

            RETURN_ON_ERRORCODE(i2c_port->ReadReg(address, MPU6050_INTR_ENABLE, &enabled_interrupts, 1));

            if (enabled_interrupts != interrupt_sources)
            {

                enabled_interrupts |= interrupt_sources;
                RETURN_ON_ERRORCODE(i2c_port->WriteSingleReg(address, MPU6050_INTR_ENABLE, enabled_interrupts));
            }

            return ErrorCode::OK;
        }
        ErrorCode GetInterruptStatus(uint8_t *const out_intr_status)
        {
            return NULL == out_intr_status ? ErrorCode::INVALID_ARGUMENT_VALUES : i2c_port->ReadReg(address, MPU6050_INTR_STATUS, out_intr_status, 1);
        }

        bool IsDataReadyInterrupt(uint8_t interrupt_status)
        {
            return (MPU6050_DATA_RDY_INT_BIT == (MPU6050_DATA_RDY_INT_BIT & interrupt_status));
        }
        bool IsI2CMasterInterrupt(uint8_t interrupt_status)
        {
            return (uint8_t)(MPU6050_I2C_MASTER_INT_BIT == (MPU6050_I2C_MASTER_INT_BIT & interrupt_status));
        }
        bool IsFifoOverflowInterrupt(uint8_t interrupt_status)
        {
            return (uint8_t)(MPU6050_FIFO_OVERFLOW_INT_BIT == (MPU6050_FIFO_OVERFLOW_INT_BIT & interrupt_status));
        }

        ErrorCode GetRawAcce(xyz_i16 *const raw_acce_value)
        {
            uint8_t data_rd[6];
            RETURN_ON_ERRORCODE(i2c_port->ReadReg(address, MPU6050_ACCEL_XOUT_H, data_rd, sizeof(data_rd)));
            raw_acce_value->x = (int16_t)((data_rd[0] << 8) + (data_rd[1]));
            raw_acce_value->y = (int16_t)((data_rd[2] << 8) + (data_rd[3]));
            raw_acce_value->z = (int16_t)((data_rd[4] << 8) + (data_rd[5]));
            return ErrorCode::OK;
        }
        ErrorCode GetRawGyro(xyz_i16 *const raw_gyro_value)
        {
            uint8_t data_rd[6];
            RETURN_ON_ERRORCODE(i2c_port->ReadReg(address, MPU6050_GYRO_XOUT_H, data_rd, sizeof(data_rd)));

            raw_gyro_value->x = (int16_t)((data_rd[0] << 8) + (data_rd[1]));
            raw_gyro_value->y = (int16_t)((data_rd[2] << 8) + (data_rd[3]));
            raw_gyro_value->z = (int16_t)((data_rd[4] << 8) + (data_rd[5]));
            return ErrorCode::OK;
        }

        ErrorCode GetAcce(xyz_f32 *const acce_value)
        {
            xyz_i16 raw_acce;
            RETURN_ON_ERRORCODE(GetRawAcce(&raw_acce));
            acce_value->x = raw_acce.x / acce_sensitivity;
            acce_value->y = raw_acce.y / acce_sensitivity;
            acce_value->z = raw_acce.z / acce_sensitivity;
            return ErrorCode::OK;
        }

        ErrorCode GetGyro(xyz_f32 *const gyro_value)
        {
            xyz_i16 raw;
            RETURN_ON_ERRORCODE(GetRawGyro(&raw));
            gyro_value->x = raw.x / gyro_sensitivity;
            gyro_value->y = raw.y / gyro_sensitivity;
            gyro_value->z = raw.z / gyro_sensitivity;
            return ErrorCode::OK;
        }

        ErrorCode GetTemp(float *const temp_value)
        {
            uint8_t data_rd[2];
            RETURN_ON_ERRORCODE(i2c_port->ReadReg(address, MPU6050_TEMP_XOUT_H, data_rd, sizeof(data_rd)));
            *temp_value = (int16_t)((data_rd[0] << 8) | (data_rd[1])) / 340.00 + 36.53;
            return ErrorCode::OK;
        }

        ErrorCode ComplimentaryFilter(const xyz_f32 *const acce_value, const xyz_f32 *const gyro_value, rollpitch_f32 *const complimentary_angle)
        {
            /*float acce_angle[2];
            float gyro_angle[2];
            float gyro_rate[2];
            mpu6050_dev_t *sens = (mpu6050_dev_t *)sensor;

            sens->counter++;
            if (sens->counter == 1)
            {
                acce_angle[0] = (atan2(acce_value->acce_y, acce_value->acce_z) * RAD_TO_DEG);
                acce_angle[1] = (atan2(acce_value->acce_x, acce_value->acce_z) * RAD_TO_DEG);
                complimentary_angle->roll = acce_angle[0];
                complimentary_angle->pitch = acce_angle[1];
                gettimeofday(sens->timer, NULL);
                return ESP_OK;
            }

            struct timeval now, dt_t;
            gettimeofday(&now, NULL);
            timersub(&now, sens->timer, &dt_t);
            sens->dt = (float)(dt_t.tv_sec) + (float)dt_t.tv_usec / 1000000;
            gettimeofday(sens->timer, NULL);

            acce_angle[0] = (atan2(acce_value->acce_y, acce_value->acce_z) * RAD_TO_DEG);
            acce_angle[1] = (atan2(acce_value->acce_x, acce_value->acce_z) * RAD_TO_DEG);

            gyro_rate[0] = gyro_value->gyro_x;
            gyro_rate[1] = gyro_value->gyro_y;
            gyro_angle[0] = gyro_rate[0] * sens->dt;
            gyro_angle[1] = gyro_rate[1] * sens->dt;

            complimentary_angle->roll = (ALPHA * (complimentary_angle->roll + gyro_angle[0])) + ((1 - ALPHA) * acce_angle[0]);
            complimentary_angle->pitch = (ALPHA * (complimentary_angle->pitch + gyro_angle[1])) + ((1 - ALPHA) * acce_angle[1]);*/

            return ErrorCode::NOT_YET_IMPLEMENTED;
        }
    };

}