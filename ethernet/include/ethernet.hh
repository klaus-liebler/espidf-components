
#if not defined(CONFIG_ETH_SPI_ETHERNET_W5500)
    #error CONFIG_ETH_SPI_ETHERNET_W5500 must be activated in idf menuconfig
#endif

#pragma once

#include "esp_netif.h"
#include "esp_eth.h"
#include "esp_event.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_err.h"
#include <esp_sntp.h>
#include <esp_eth_mac_spi.h>
#include <ctime>

#define TAG "ETH"
namespace ETHERNET
{
    esp_netif_t *eth_netif{nullptr};
    
    void eth_event_handler(void *arg, esp_event_base_t event_base,
                           int32_t event_id, void *event_data)
    {
        uint8_t mac_addr[6] = {0};
        /* we can get the ethernet driver handle from event data */
        esp_eth_handle_t eth_handle = *(esp_eth_handle_t *)event_data;

        switch (event_id)
        {
        case ETHERNET_EVENT_CONNECTED:
            esp_eth_ioctl(eth_handle, ETH_CMD_G_MAC_ADDR, mac_addr);
            ESP_LOGI(TAG, "Ethernet Link Up");
            ESP_LOGI(TAG, "Ethernet HW Addr %02x:%02x:%02x:%02x:%02x:%02x",
                     mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
            break;
        case ETHERNET_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "Ethernet Link Down");
            break;
        case ETHERNET_EVENT_START:
            ESP_LOGI(TAG, "Ethernet Started");
            break;
        case ETHERNET_EVENT_STOP:
            ESP_LOGI(TAG, "Ethernet Stopped");
            break;
        default:
            break;
        }
    }





    void initETH_W5500(bool already_called_netif_init_and_event_loop, spi_host_device_t spiHost, gpio_num_t miso, gpio_num_t mosi, gpio_num_t sclk, uint32_t SPI_MASTER_FREQ_X, gpio_num_t cs, gpio_num_t reset, gpio_num_t irq, uint32_t phyAddress, int intr_flags, const char* hostname)
    {
        if (!already_called_netif_init_and_event_loop)
        {
            ESP_ERROR_CHECK(esp_netif_init());
            ESP_ERROR_CHECK(esp_event_loop_create_default());
        }

        esp_netif_inherent_config_t esp_netif_config = ESP_NETIF_INHERENT_DEFAULT_ETH();

        esp_netif_config_t cfg_spi = {};
        cfg_spi.base = &esp_netif_config;
        cfg_spi.stack = ESP_NETIF_NETSTACK_DEFAULT_ETH;
  
        eth_netif = esp_netif_new(&cfg_spi);

        ESP_ERROR_CHECK(esp_netif_set_hostname(eth_netif, hostname));



        eth_mac_config_t mac_config_spi = ETH_MAC_DEFAULT_CONFIG();

        eth_phy_config_t phy_config_spi = ETH_PHY_DEFAULT_CONFIG();
        phy_config_spi.phy_addr = phyAddress;
        phy_config_spi.reset_gpio_num = (int)reset;
        gpio_install_isr_service(0);

        spi_bus_config_t buscfg = {};
        buscfg.miso_io_num = (int)miso; // 13;
        buscfg.mosi_io_num = (int)mosi; // 11;
        buscfg.sclk_io_num = (int)sclk; // 12;
        buscfg.quadwp_io_num = -1;
        buscfg.quadhd_io_num = -1;
        buscfg.intr_flags=intr_flags;
        ESP_ERROR_CHECK(spi_bus_initialize(spiHost, &buscfg, SPI_DMA_CH_AUTO));
        // Configure SPI interface and Ethernet driver for specific SPI module
        spi_device_interface_config_t spi_devcfg = {};
        spi_devcfg.mode = 0;
        spi_devcfg.clock_speed_hz = SPI_MASTER_FREQ_X;
        spi_devcfg.queue_size = 20;
        spi_devcfg.spics_io_num = (int)cs; // 10;
        
        eth_w5500_config_t w5500_config = ETH_W5500_DEFAULT_CONFIG(spiHost, &spi_devcfg);
        w5500_config.int_gpio_num = (int)irq; // 14;
        esp_eth_mac_t *mac_spi = esp_eth_mac_new_w5500(&w5500_config, &mac_config_spi);
        esp_eth_phy_t *phy_spi = esp_eth_phy_new_w5500(&phy_config_spi);

        esp_eth_config_t eth_config_spi = ETH_DEFAULT_CONFIG(mac_spi, phy_spi);

        esp_eth_handle_t eth_handle_spi{nullptr};
        ESP_ERROR_CHECK(esp_eth_driver_install(&eth_config_spi, &eth_handle_spi));

        /* The SPI Ethernet module might not have a burned factory MAC address, we cat to set it manually.
        02:00:00 is a Locally Administered OUI range so should not be used except when testing on a LAN under your control.
        */
        uint8_t MAC[]{0x02, 0x00, 0x00, 0x12, 0x34, 0x56};
        ESP_ERROR_CHECK(esp_eth_ioctl(eth_handle_spi, ETH_CMD_S_MAC_ADDR, MAC));

        // attach Ethernet driver to TCP/IP stack
        ESP_ERROR_CHECK(esp_netif_attach(eth_netif, esp_eth_new_netif_glue(eth_handle_spi)));

        // Register user defined event handers
        ESP_ERROR_CHECK(esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID, &eth_event_handler, NULL));
        ESP_ERROR_CHECK(esp_eth_start(eth_handle_spi));
    }
}
#undef TAG