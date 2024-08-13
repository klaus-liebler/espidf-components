#include "bh1750.hh"


BH1750::M::M(i2c_master_bus_handle_t bus_handle, ADDRESS address, OPERATIONMODE operation):I2CSensor(bus_handle, (uint8_t)address), operation(operation){}

ErrorCode BH1750::M::Initialize(int64_t& waitTillFirstTrigger)
{
    waitTillFirstTrigger=100;
    uint8_t op = (uint8_t)operation;
    return Write8(&op, 1);

}

ErrorCode BH1750::M::Trigger(int64_t& waitTillReadout){
    waitTillReadout=100;
    return ErrorCode::OK;
}

ErrorCode BH1750::M::Readout(int64_t& waitTillNextTrigger){
    waitTillNextTrigger=100;
    uint8_t sensor_data[2];
    auto err=Read8(sensor_data, 2);
    this->recentValueLux = ((sensor_data[0] << 8 | sensor_data[1]) / 1.2);
    return err;
}