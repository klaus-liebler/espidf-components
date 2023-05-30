#pragma once

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "driver/gpio.h"
#include "sdkconfig.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>

#include <shell_handler.hh>

#include "libtelnet.h"

constexpr int KEEPALIVE_IDLE{5};
constexpr int KEEPALIVE_INTERVAL{5};
constexpr int KEEPALIVE_COUNT{3};
constexpr const telnet_telopt_t my_telopts[] = {
        {TELNET_TELOPT_ECHO, TELNET_WONT, TELNET_DO}, // ich server sende kein echo, du client sollst ein echo produzieren!
        {TELNET_TELOPT_TTYPE, TELNET_WILL, TELNET_DONT},
        {TELNET_TELOPT_COMPRESS2, TELNET_WONT, TELNET_DO},
        {TELNET_TELOPT_ZMP, TELNET_WONT, TELNET_DO},
        {TELNET_TELOPT_MSSP, TELNET_WONT, TELNET_DO},
        {TELNET_TELOPT_BINARY, TELNET_WILL, TELNET_DO},
        {TELNET_TELOPT_NAWS, TELNET_WILL, TELNET_DONT},
        {-1, 0, 0}};

#define TAG "TELNET"

extern "C" void StaticTelnetHandler(telnet_t *thisTelnet, telnet_event_t *event, void *userData);//Ich habe es auch nach mehrern Versuchen nicht 

class ESPTelnet : IShellCallback
{
public:
  bool initAndRunInTask(IShellHandler *handler, int port = 23)
  {
    assert(handler!=nullptr);
    this->handler = handler;
    this->port = port;
    return xTaskCreate(ESPTelnet::StaticTask, "telnet_task", 8192, (void *)this, 5, nullptr) == 0;
  }

  void sendData(const uint8_t *buffer, size_t size)
  {
    if (!tnHandle)
      return;
    telnet_send(tnHandle, (char *)buffer, size);
  }

  void sendText(const char *buffer, size_t size)
  {
    if (!tnHandle)
      return;
    telnet_send_text(tnHandle, buffer, size);
  }

  int vprintf(const char *fmt, va_list va)
  {
    if (!tnHandle)
      return 0;
    return telnet_vprintf(tnHandle, fmt, va);
  }

  int printf(const char *format, ...) override
  {
    if (!tnHandle)
      return 0;
    int len = 0;
    va_list args;
    va_start(args, format);
    len = telnet_vprintf(tnHandle, format, args);
    va_end(args);
    return len;
  }

  int printChar(char c) override
  {
    if (!tnHandle)
      return 0;
    telnet_printf(tnHandle, "%c", c);
    return 1;
  }

  bool IsPrivilegedUser() override
  {
    return true;
  }
  const char *GetUsername() override
  {
    return "USER";
  }

  void closeSession()
  {
    if (!tnHandle)
      return;
    handler->endShell(0, this);
    shutdown(sockfd, 0);
    close(sockfd);
    telnet_free(tnHandle);
    tnHandle = nullptr;
  }

  void TelnetHandler(telnet_t *thisTelnet, telnet_event_t *event)
  {
    int rc;
    switch (event->type)
    {
    case TELNET_EV_SEND:
      rc = send(this->sockfd, event->data.buffer, event->data.size, 0);
      if (rc < 0)
      {
        ESP_LOGE(TAG, "send: %d", errno);
      }
      break;

    case TELNET_EV_DATA:
      this->handler->handleChars(event->data.buffer, (size_t)event->data.size, 0, this);
      break;
    default:
      ESP_LOGW(TAG, "We received an unhandled telnet event: %s", eventToString(event->type));
      break;
    } // End of switch event type
  }

private:
  int port{23};
  int sockfd{-1};
  telnet_t *tnHandle{nullptr};
  IShellHandler *handler{nullptr};

  const char *eventToString(telnet_event_type_t type)
  {
    switch (type)
    {
    case TELNET_EV_COMPRESS:
      return "TELNET_EV_COMPRESS";
    case TELNET_EV_DATA:
      return "TELNET_EV_DATA";
    case TELNET_EV_DO:
      return "TELNET_EV_DO";
    case TELNET_EV_DONT:
      return "TELNET_EV_DONT";
    case TELNET_EV_ENVIRON:
      return "TELNET_EV_ENVIRON";
    case TELNET_EV_ERROR:
      return "TELNET_EV_ERROR";
    case TELNET_EV_IAC:
      return "TELNET_EV_IAC";
    case TELNET_EV_MSSP:
      return "TELNET_EV_MSSP";
    case TELNET_EV_SEND:
      return "TELNET_EV_SEND";
    case TELNET_EV_SUBNEGOTIATION:
      return "TELNET_EV_SUBNEGOTIATION";
    case TELNET_EV_TTYPE:
      return "TELNET_EV_TTYPE";
    case TELNET_EV_WARNING:
      return "TELNET_EV_WARNING";
    case TELNET_EV_WILL:
      return "TELNET_EV_WILL";
    case TELNET_EV_WONT:
      return "TELNET_EV_WONT";
    case TELNET_EV_ZMP:
      return "TELNET_EV_ZMP";
    }
    return "Unknown type";
  }

  static int Static_readfn(void *data, char *buffer, int size)
  {
    int *partnerSocket = static_cast<int *>(data);
    return recv(*partnerSocket, buffer, size, 0);
  }

  static int Static_writefn(void *data, const char *buffer, int size)
  {
    int *partnerSocket = static_cast<int *>(data);
    return send(*partnerSocket, buffer, size, 0);
  }

  void doTelnetTest(int partnerSocket)
  {
    ESP_LOGI(TAG, ">> doTelnet TEST VARIANT");
    ESP_LOGI(TAG, ">> doTelnet TEST VARIANT");
    fflush(stdin);

    stdout = funopen(&partnerSocket, nullptr, &ESPTelnet::Static_writefn, nullptr, nullptr);
    stdin = funopen(&partnerSocket, ESPTelnet::Static_readfn, nullptr, nullptr, nullptr);
    // stderr = funopen(&partnerSocket, ESPTelnet::Static_writefn);
    setvbuf(stdin, NULL, _IONBF, 0);
    setvbuf(stdout, NULL, _IONBF, 0);
    char c;
    while (true)
    {
      c = getc(stdin);
      fprintf(stderr, "Read 0x%x\n", c);
      putc(c, stdout);
    }
  }

  void doTelnet()
  {
    tnHandle = telnet_init(my_telopts, StaticTelnetHandler, 0, (void *)this);
    assert(this);
    assert(this->handler);
    ESP_LOGI(TAG, "Handler is on address %lu", (uint32_t)this->handler);
    ESP_LOGI(TAG, "The magic number is %i", this->handler->GetMagicNumber());
    this->handler->beginShell(this);
    ESP_LOGI(TAG, "finished this->handler->beginShell");

    uint8_t buffer[1025];
    while (true)
    {
      ssize_t len = recv(this->sockfd, (char *)buffer, 1024, 0);
      ESP_LOGD(TAG, "Socket received something of length %d", len);
      if (len == 0)
        break;
      buffer[len] = '\0';
      telnet_recv(tnHandle, (char *)buffer, len);
    }
    this->handler->endShell(0, this);
    ESP_LOGI(TAG, "finished this->handler->endShell");
    telnet_free(tnHandle);
    tnHandle = nullptr;
  }

  void Task()
  {
    int listen_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listen_sock < 0)
    {
      ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
      vTaskDelete(NULL);
      return;
    }
    int opt = 1;
    setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    char addr_str[16];
    int keepAlive = 1;

    struct sockaddr_storage dest_addr;
    struct sockaddr_in *dest_addr_ip4 = (struct sockaddr_in *)&dest_addr;
    dest_addr_ip4->sin_family = AF_INET;
    dest_addr_ip4->sin_addr.s_addr = htonl(INADDR_ANY);
    dest_addr_ip4->sin_port = htons(this->port);
    int err = bind(listen_sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
    if (err != 0)
    {
      ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
      goto CLEAN_UP;
    }

    ESP_LOGI(TAG, "Socket bound, port %d", this->port);

    err = listen(listen_sock, 1);
    if (err != 0)
    {
      ESP_LOGE(TAG, "Error occurred during listen: errno %d", errno);
      goto CLEAN_UP;
    }

    while (true)
    {
      ESP_LOGI(TAG, "Socket listening");
      struct sockaddr_storage source_addr; // Large enough for both IPv4 or IPv6
      socklen_t addr_len = sizeof(source_addr);
      this->sockfd = accept(listen_sock, (struct sockaddr *)&source_addr, &addr_len);
      if (this->sockfd < 0)
      {
        ESP_LOGE(TAG, "Unable to accept connection: errno %d", errno);
        break;
      }

      // Set tcp keepalive option
      setsockopt(this->sockfd, SOL_SOCKET, SO_KEEPALIVE, &keepAlive, sizeof(int));
      setsockopt(this->sockfd, IPPROTO_TCP, TCP_KEEPIDLE, &KEEPALIVE_IDLE, sizeof(int));
      setsockopt(this->sockfd, IPPROTO_TCP, TCP_KEEPINTVL, &KEEPALIVE_INTERVAL, sizeof(int));
      setsockopt(this->sockfd, IPPROTO_TCP, TCP_KEEPCNT, &KEEPALIVE_COUNT, sizeof(int));
      // Convert ip address to string
      if (source_addr.ss_family == PF_INET)
      {
        inet_ntoa_r(((struct sockaddr_in *)&source_addr)->sin_addr, addr_str, sizeof(addr_str) - 1);
      }

      ESP_LOGI(TAG, "Socket accepted ip address: %s", addr_str);

      doTelnet();

      shutdown(this->sockfd, 0);
      close(this->sockfd);
    }
  CLEAN_UP:
    close(listen_sock);
    vTaskDelete(NULL);
  }
  static void StaticTask(void *args)
  {
    ESPTelnet *myself = static_cast<ESPTelnet *>(args);
    myself->Task();
  }
};

extern "C" void StaticTelnetHandler(telnet_t *thisTelnet, telnet_event_t *event, void *userData)
{
  ESPTelnet *myself = static_cast<ESPTelnet *>(userData);
  myself->TelnetHandler(thisTelnet, event);
}

#undef TAG