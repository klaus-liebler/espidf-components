#pragma once
namespace REG {
    /* Memory Map */
    constexpr uint8_t CONFIG      = 0x00;
    constexpr uint8_t EN_AA       = 0x01;
    constexpr uint8_t EN_RXADDR   = 0x02;
    constexpr uint8_t SETUP_AW    = 0x03;
    constexpr uint8_t SETUP_RETR  = 0x04;
    constexpr uint8_t RF_CH       = 0x05;
    constexpr uint8_t RF_SETUP    = 0x06;
    constexpr uint8_t STATUS      = 0x07;
    constexpr uint8_t OBSERVE_TX  = 0x08;
    constexpr uint8_t CD          = 0x09;
    constexpr uint8_t RX_ADDR_P0  = 0x0A;
    constexpr uint8_t RX_ADDR_P1  = 0x0B;
    constexpr uint8_t RX_ADDR_P2  = 0x0C;
    constexpr uint8_t RX_ADDR_P3  = 0x0D;
    constexpr uint8_t RX_ADDR_P4  = 0x0E;
    constexpr uint8_t RX_ADDR_P5  = 0x0F;
    constexpr uint8_t TX_ADDR     = 0x10;
    constexpr uint8_t RX_PW_P0    = 0x11;
    constexpr uint8_t RX_PW_P1    = 0x12;
    constexpr uint8_t RX_PW_P2    = 0x13;
    constexpr uint8_t RX_PW_P3    = 0x14;
    constexpr uint8_t RX_PW_P4    = 0x15;
    constexpr uint8_t RX_PW_P5    = 0x16;
    constexpr uint8_t FIFO_STATUS = 0x17;
    constexpr uint8_t DYNPD       = 0x1C;
    constexpr uint8_t FEATURE     = 0x1D;

    /* Instruction Mnemonics */
    constexpr uint8_t R_REGISTER    = 0x00;
    constexpr uint8_t W_REGISTER    = 0x20;
    constexpr uint8_t REGISTER_MASK = 0x1F;
    constexpr uint8_t R_RX_PAYLOAD  = 0x61;
    constexpr uint8_t W_TX_PAYLOAD  = 0xA0;
    constexpr uint8_t FLUSH_TX      = 0xE1;
    constexpr uint8_t FLUSH_RX      = 0xE2;
    constexpr uint8_t REUSE_TX_PL   = 0xE3;
    constexpr uint8_t NOP           = 0xFF;

    /* P model memory Map */
    constexpr uint8_t RPD                  = 0x09;
    constexpr uint8_t W_TX_PAYLOAD_NO_ACK  = 0xB0;
}



/* Bit Mnemonics */
#define MASK_RX_DR  6
#define MASK_TX_DS  5
#define MASK_MAX_RT 4
#define EN_CRC      3
#define CRCO        2
#define PWR_UP      1
#define PRIM_RX     0
#define ENAA_P5     5
#define ENAA_P4     4
#define ENAA_P3     3
#define ENAA_P2     2
#define ENAA_P1     1
#define ENAA_P0     0
#define ERX_P5      5
#define ERX_P4      4
#define ERX_P3      3
#define ERX_P2      2
#define ERX_P1      1
#define ERX_P0      0
#define AW          0
#define ARD         4
#define ARC         0
#define RF_DR_LOW   5
#define PLL_LOCK    4
#define RF_DR_HIGH  3
#define RF_PWR      1
#define LNA_HCURR   0
#define RX_DR       6
#define TX_DS       5
#define MAX_RT      4
#define RX_P_NO     1
#define TX_FULL     0
#define PLOS_CNT    4
#define ARC_CNT     0
#define TX_REUSE    6
#define FIFO_FULL   5
#define TX_EMPTY    4
#define RX_FULL     1
#define RX_EMPTY    0

/* Instruction Mnemonics */
#define R_REGISTER    0x00
#define W_REGISTER    0x20
#define REGISTER_MASK 0x1F
#define R_RX_PAYLOAD  0x61
#define W_TX_PAYLOAD  0xA0
#define FLUSH_TX      0xE1
#define FLUSH_RX      0xE2
#define REUSE_TX_PL   0xE3
#define NOP           0xFF

/* Non-P omissions */
#define LNA_HCURR   0

/* P model memory Map */
#define RPD                  0x09
#define W_TX_PAYLOAD_NO_ACK  0xB0

/* P model bit Mnemonics */
#define RF_DR_LOW   5
#define RF_DR_HIGH  3
#define RF_PWR_LOW  1
#define RF_PWR_HIGH 2



