#pragma once
#include <stdint.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "tinyusb.h"
#include "tusb_cdc_acm.h"
#include "sdkconfig.h"
#include "esp_timer.h"
#include <common.hh>
#include <vector>
#include "modbuscommons.hh"
constexpr char MBLOG[] = "MB_SV";

namespace modbus
{

    constexpr size_t RX_BUF_MAX_SIZE=256;
    template<time_t TIMEOUT>
    class M
    {
    private:
        uint8_t slaveId;
        void (*callback)(uint8_t, uint16_t, size_t);
        std::vector<bool> *outputCoils;
        std::vector<bool> *inputContacts;
        std::vector<uint16_t> *inputRegisters;
        std::vector<uint16_t> *holdingRegisters;

        ModbusMessageParsingResult processFC01orFC02(uint8_t *tx_buf, size_t &tx_size, std::vector<bool> *theBitVector)
        {
            if (rx_pos != 8)
                return  SendErrorMessageBack(tx_buf, tx_size, ModbusMessageParsingResult::LENGTH_ERROR);
            if (!validCRCInLastTwoBytes(rx_buf, rx_pos))
                return SendErrorMessageBack(tx_buf, tx_size, ModbusMessageParsingResult::CRC_ERROR);
            uint16_t startCoil = ParseUInt16BigEndian(rx_buf, 2);
            if (startCoil >= theBitVector->size())
                return SendErrorMessageBack(tx_buf, tx_size, ModbusMessageParsingResult::OUT_OF_RANGE);
            uint16_t coilCount = ParseUInt16BigEndian(rx_buf, 4);
            if (coilCount==0 || (startCoil + coilCount > theBitVector->size()))
                return SendErrorMessageBack(tx_buf, tx_size, ModbusMessageParsingResult::OUT_OF_RANGE);
            tx_buf[0]=rx_buf[0];
            tx_buf[1]=rx_buf[1];
            uint8_t byteCount=get_number_of_bytes(coilCount);
            tx_buf[2]=byteCount;
            for(int i=0;i<byteCount;i++){
                tx_buf[3+i]=0;
            }
            for(int i=0;i<coilCount;i++){
                bool bit = theBitVector->at(startCoil+i);
                tx_buf[3+i/8]+=bit<<(i%8);
            }
            WriteCRC(tx_buf, 3+byteCount);
            tx_size=3+byteCount+2;
            rx_pos=0;
            return ModbusMessageParsingResult::OK;
        }

        /**
         * Write Single Coil
        */
        ModbusMessageParsingResult processFC05(uint8_t *tx_buf, size_t &tx_size)
        {
            if (rx_pos != 8)
                return  SendErrorMessageBack(tx_buf, tx_size, ModbusMessageParsingResult::LENGTH_ERROR);
            if (!validCRCInLastTwoBytes(rx_buf, rx_pos))
                return SendErrorMessageBack(tx_buf, tx_size, ModbusMessageParsingResult::CRC_ERROR);
            uint16_t coilIndex = ParseUInt16BigEndian(rx_buf, 2);
            if (coilIndex >= outputCoils->size())
                return SendErrorMessageBack(tx_buf, tx_size, ModbusMessageParsingResult::OUT_OF_RANGE);
            uint16_t registerValue = ParseUInt16BigEndian(rx_buf, 4);
            outputCoils->at(coilIndex) = registerValue!=0;
            if (callback)
                callback(5, coilIndex, 1);
            return SendOKMessageBack(tx_buf, tx_size);
        }
       
        ModbusMessageParsingResult processFC06(uint8_t *tx_buf, size_t &tx_size)
        {
            if (rx_pos != 8)
                return  SendErrorMessageBack(tx_buf, tx_size, ModbusMessageParsingResult::LENGTH_ERROR);
            if (!validCRCInLastTwoBytes(rx_buf, rx_pos))
                return SendErrorMessageBack(tx_buf, tx_size, ModbusMessageParsingResult::CRC_ERROR);
            uint16_t registerIndex = ParseUInt16BigEndian(rx_buf, 2);
            if (registerIndex >= holdingRegisters->size())
                return SendErrorMessageBack(tx_buf, tx_size, ModbusMessageParsingResult::OUT_OF_RANGE);
            uint16_t registerValue = ParseUInt16BigEndian(rx_buf, 4);
            holdingRegisters->at(registerIndex) = registerValue;
            if (callback)
                callback(6, registerIndex, 1);
            return SendOKMessageBack(tx_buf, tx_size);
        }

        ModbusMessageParsingResult processFC15(uint8_t *tx_buf, size_t &tx_size)
        {
            uint16_t coilStart = ParseUInt16BigEndian(rx_buf, 2);
            if (coilStart >= outputCoils->size())
                return SendErrorMessageBack(tx_buf, tx_size, ModbusMessageParsingResult::OUT_OF_RANGE);
            uint16_t coilCount = ParseUInt16BigEndian(rx_buf, 4);
            if (coilStart + coilCount > outputCoils->size())
                return SendErrorMessageBack(tx_buf, tx_size, ModbusMessageParsingResult::OUT_OF_RANGE);
            uint8_t byteCount = rx_buf[6];
            if (byteCount != get_number_of_bytes(coilCount))
            {
                ESP_LOGE(MBLOG, "byteCount %d vs registerCount %d", byteCount, coilCount);
                return SendErrorMessageBack(tx_buf, tx_size, ModbusMessageParsingResult::LENGTH_ERROR);
            }
            size_t necessaryLength = 9 + byteCount;
            if(rx_pos<necessaryLength)
            {
                return ModbusMessageParsingResult::MESSAGE_NOT_YET_COMPLETE;
            } 
            else if(rx_pos>necessaryLength){
                return SendErrorMessageBack(tx_buf, tx_size, ModbusMessageParsingResult::LENGTH_ERROR);
            }
            
            if (!validCRCInLastTwoBytes(rx_buf, rx_pos))
                return SendErrorMessageBack(tx_buf, tx_size, ModbusMessageParsingResult::CRC_ERROR);

            for(int i=0;i<coilCount;i++){
                bool value = GetBitInU8Buf(rx_buf, 7, i);
                outputCoils->at(coilStart+i)=value;
            }
            if (callback)
                callback(15, coilStart, coilCount);
            return SendPartialCopyOKMessageWithNewCRCBack(tx_buf, tx_size, 6);
        }
        //Write Holding Registers
        ModbusMessageParsingResult processFC16(uint8_t *tx_buf, size_t &tx_size)
        {
            uint16_t registerIndex = ParseUInt16BigEndian(rx_buf, 2);
            if (registerIndex >= holdingRegisters->size())
                return SendErrorMessageBack(tx_buf, tx_size, ModbusMessageParsingResult::OUT_OF_RANGE);
            uint16_t registerCount = ParseUInt16BigEndian(rx_buf, 4);
            if (registerIndex + registerCount > holdingRegisters->size())
                return SendErrorMessageBack(tx_buf, tx_size, ModbusMessageParsingResult::OUT_OF_RANGE);
            uint8_t byteCount = rx_buf[6];
            if (byteCount != 2 * registerCount)
            {
                ESP_LOGE(MBLOG, "byteCount %d vs registerCount %d", byteCount, registerCount);
                return SendErrorMessageBack(tx_buf, tx_size, ModbusMessageParsingResult::LENGTH_ERROR);
            }
            size_t necessaryLength = 9 + 2*registerCount;
            if(rx_pos<necessaryLength)
            {
                return ModbusMessageParsingResult::MESSAGE_NOT_YET_COMPLETE;
            } 
            else if(rx_pos>necessaryLength){
                return SendErrorMessageBack(tx_buf, tx_size, ModbusMessageParsingResult::LENGTH_ERROR);
            }
            
            if (!validCRCInLastTwoBytes(rx_buf, rx_pos))
                return SendErrorMessageBack(tx_buf, tx_size, ModbusMessageParsingResult::CRC_ERROR);
            
            for (int reg = 0; reg < registerCount; reg++)
            {
                uint16_t registerValue = ParseUInt16BigEndian(rx_buf, 7 + 2 * reg);
                holdingRegisters->at(registerIndex + reg) = registerValue;
            }
            if (callback)
                callback(16, registerIndex, registerCount);
            return SendPartialCopyOKMessageWithNewCRCBack(tx_buf, tx_size, 6);
        }

        ModbusMessageParsingResult SendErrorMessageBack(uint8_t *tx_buf, size_t &tx_size, ModbusMessageParsingResult reason)
        {
            tx_buf[0] = rx_buf[0] + 128;
            for (size_t i = 1; i < rx_pos - 2; i++)
            {
                tx_buf[i] = rx_buf[i];
            }
            WriteCRC(tx_buf, rx_pos - 2);
            tx_size = rx_pos;
            rx_pos=0;
            return reason;
        }
        ModbusMessageParsingResult SendOKMessageBack(uint8_t *tx_buf, size_t &tx_size)
        {
            for (size_t i = 0; i < rx_pos; i++)
            {
                tx_buf[i] = rx_buf[i];
            }
            tx_size = rx_pos;
            rx_pos=0;
            return ModbusMessageParsingResult::OK;
        }

        ModbusMessageParsingResult SendPartialCopyOKMessageWithNewCRCBack(uint8_t *tx_buf, size_t &tx_size, size_t size=6)
        {
            for (size_t i = 0; i < size; i++)
            {
                tx_buf[i] = rx_buf[i];
            }
            WriteCRC(tx_buf, size);
            tx_size = size+2;
            rx_pos=0;
            return ModbusMessageParsingResult::OK;
        }


        uint8_t rx_buf[256];
        uint8_t rx_pos=0;
        time_t rx_time=0;
        SemaphoreHandle_t xBinarySemaphore;
    public:
        M(uint8_t slaveId, void (*callback)(uint8_t, uint16_t, size_t), std::vector<bool> *outputCoils, std::vector<bool> *inputContacts, std::vector<uint16_t> *inputRegisters, std::vector<uint16_t> *holdingRegisters)
            : slaveId(slaveId), callback(callback), outputCoils(outputCoils), inputContacts(inputContacts), inputRegisters(inputRegisters), holdingRegisters(holdingRegisters)
        {
            xBinarySemaphore = xSemaphoreCreateBinary();
        }

        void ReceiveBytesPhase1(uint8_t **rx_buf, size_t* remaining){
            time_t now=esp_timer_get_time();
            if(rx_pos!=0 && now-rx_time>TIMEOUT){
                ESP_LOGW(MBLOG, "Reception of previous data was long time ago. We start at the beginning (rx_pos=%d, rx_time=%d).", rx_pos, (int)rx_time);
                rx_pos=0;
            }
            *rx_buf=&this->rx_buf[rx_pos];
            *remaining=RX_BUF_MAX_SIZE-rx_pos;
            rx_time=now;
        }

        void ReceiveBytesPhase2(size_t rx_size, uint8_t *tx_buf, size_t &tx_size_max){
            rx_pos+=rx_size;
            modbus::ModbusMessageParsingResult res = Parse(tx_buf, tx_size_max);
            if(res==modbus::ModbusMessageParsingResult::OK){
                ESP_LOGI(MBLOG, "%d-Message parsed and processed successfully", rx_buf[1]); 
            }else if((int)res<0){
                ESP_LOGW(MBLOG, "Parse returned %d", (int)res);
            }
            else{//>0
                ESP_LOGI(MBLOG, "Parse returned %d", (int)res); 
            }
        }

        ModbusMessageParsingResult Parse(uint8_t *tx_buf, size_t &tx_size)
        {
            if(rx_pos<8)//Es gibt keinen Request mit weniger als 8 Bytes!
                return ModbusMessageParsingResult::MESSAGE_NOT_YET_COMPLETE;
            
            if (rx_buf[0] != slaveId)
                return ModbusMessageParsingResult::REQUEST_NOT_FOR_ME;

            switch (rx_buf[1])
            {
            case 1:return processFC01orFC02(tx_buf, tx_size, outputCoils);
            case 2:return processFC01orFC02(tx_buf, tx_size, inputContacts);
            case 5:return processFC05(tx_buf, tx_size);
            case 6:return processFC06(tx_buf, tx_size);
            case 15:return processFC15(tx_buf, tx_size);
            case 16: return processFC16(tx_buf, tx_size);
            default: return SendErrorMessageBack(tx_buf, tx_size, ModbusMessageParsingResult::FUNCTION_CODE_NOT_SUPPORTED);
            }
        }
    };
}