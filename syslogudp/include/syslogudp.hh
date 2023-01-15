#pragma once
#include <esp_err.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <lwip/sockets.h>
#include <lwip/netdb.h>

enum class LogProtocol
{
    rfc5424,
    rfc3164,
    lieblerlog,
};

enum class Facility:int8_t
{
    kernel = 0,
    user_level = 1,
    mail_system = 2,
    system_daemons = 3,
    security_authorization4 = 4,
    syslogd_internal = 5,
    line_printer_subsystem = 6,
    network_news_subsystem = 7,
    UUCP_subsystem = 8,
    clock_daemon9 = 9,
    security_authorization10 = 10,
    FTP_daemon = 11,
    NTP_subsystem = 12,
    log_audit = 13,
    log_alert = 14,
    clock_daemon15 = 15,
    local0 = 16,
    local1 = 17,
    local2 = 18,
    local3 = 19,
    local4 = 20,
    local5 = 21,
    local6 = 22,
    local7 = 23,
};

enum class Severity:int8_t
{
    OUTPUT_NOTHING_FOR_FILTER_ONLY = -1,
    Emergency = 0,
    Alert = 1,
    Critical = 2,
    Error = 3,
    Warning = 4,
    Notice = 5,
    Informational = 6,
    Debug = 7,
    UNKNOWN=INT8_MAX,
};

class SyslogUdp
{
private:
    static SemaphoreHandle_t semaphore;
    static int sock;
    static struct sockaddr_in dest_addr;
    static char BUFFER[200];
    static vprintf_like_t serialHandler;
    static const char *deviceHostname;
    static Severity severityFilter[24];
    static LogProtocol protocol;
    static int SyslogInternal(const char *appName, Facility fac, Severity sev, const char *fmt, va_list data);

public:
    static esp_err_t Setup(const char *host_ip4_addr, uint16_t port, LogProtocol protocol, const char *deviceHostname, Severity defaultFilter, bool redirectNormalLog);
    static int system_log_message_route(const char *fmt, va_list tag);
    static int Syslog(const char *appName, Facility fac, Severity sev, const char *fmt, ...);
    static void Binlog(int64_t timestamp, Severity sev, const char *tag, int64_t additionaIntegerData1, int64_t additionaIntegerData2, const char *fmt, ...);
    static void SetFilter(Facility fac, Severity sev);
};