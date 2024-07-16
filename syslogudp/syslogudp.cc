#include <syslogudp.hh>
#include <common-esp32.hh>
#include <string.h>
#define TAG "SYSLOG"

constexpr const char *SYSLOG_NILVALUE{"-"};


SemaphoreHandle_t SyslogUdp::semaphore;
int SyslogUdp::sock;
struct sockaddr_in SyslogUdp::dest_addr;
char SyslogUdp::BUFFER[200];
vprintf_like_t SyslogUdp::serialHandler;
const char *SyslogUdp::deviceHostname;
Severity SyslogUdp::severityFilter[24];
LogProtocol SyslogUdp::protocol{LogProtocol::rfc3164};

esp_err_t SyslogUdp::Setup(const char *host_ip4_addr, uint16_t port, LogProtocol protocol, const char *deviceHostname, Severity defaultFilter, bool redirectNormalLog)
{
    SyslogUdp::deviceHostname = (deviceHostname == NULL) ? SYSLOG_NILVALUE : deviceHostname;
    SyslogUdp::protocol=protocol;
    semaphore = xSemaphoreCreateBinary();
    xSemaphoreGive(semaphore);
    sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (sock < 0)
    {
        ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
        vSemaphoreDelete(semaphore);
        return ESP_FAIL;
    }
    dest_addr.sin_addr.s_addr = inet_addr(host_ip4_addr);
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(port);
    if(redirectNormalLog){
        serialHandler = esp_log_set_vprintf(SyslogUdp::system_log_message_route);
    }
    for(int i=0;i<14;i++){
        severityFilter[i]=defaultFilter;
    }
    return ESP_OK;
}

void SyslogUdp::SetFilter(Facility fac, Severity sev){
    severityFilter[(int)fac]=sev;
}

void SyslogUdp::Binlog(int64_t timestamp, Severity sev, const char *tag, int64_t additionaIntegerData1, int64_t additionaIntegerData2, const char *fmt, ...){
    xSemaphoreTake(semaphore, portMAX_DELAY);
    WriteI64(timestamp, (uint8_t*)BUFFER, 0);
    WriteI64(additionaIntegerData1, (uint8_t*)BUFFER, 8);
    WriteI64(additionaIntegerData2, (uint8_t*)BUFFER, 16);
    WriteI8((int8_t)sev, (uint8_t*)BUFFER, 24);
    size_t pos{28};
    pos+=snprintf(BUFFER+pos, sizeof(BUFFER)-pos, tag);
    va_list ap;
    va_start(ap, fmt);
    pos+=vsnprintf(BUFFER + pos+1, sizeof(BUFFER)-pos-1, fmt, ap);
    va_end(ap);
    sendto(sock, BUFFER, pos+1, 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr)); //pos+1 to include the termination 
    xSemaphoreGive(semaphore);
    return;
}

int SyslogUdp::Syslog(const char *appName, Facility fac, Severity sev,  const char *fmt, ...){
    xSemaphoreTake(semaphore, portMAX_DELAY);
    va_list ap;
    va_start(ap, fmt);
    int len = SyslogInternal(appName, fac, sev, fmt, ap);
    va_end(ap);
    xSemaphoreGive(semaphore);
    return len;
}

//wenn sev größer ist, als der Filter, dann wird nicht ausgegeben: Die "Filter"-Severity und alle "schlimmeren" werden ausgegeben
int SyslogUdp::SyslogInternal(const char *appName, Facility fac, Severity sev, const char *fmt, va_list data)
{
    if ((int)sev>(int)severityFilter[(int)fac])
        return 0;
    appName = (appName == NULL) ? SYSLOG_NILVALUE : appName;
    
    size_t n = sizeof(BUFFER);
    int pri = (int)fac*8+(int)sev;
    int len1;
    if(protocol==LogProtocol::rfc5424){
        len1=snprintf(BUFFER, n, "<%d>1 - %s %s - - - ", pri, deviceHostname, appName);
    } else{
        len1=snprintf(BUFFER, n, "<%d>Sep  2 09:46:37 %s %s: ", pri, deviceHostname, appName);
    }
    if (len1 < 0 || len1 >= n)
    { // see https://cplusplus.com/reference/cstdio/vsnprintf/
        xSemaphoreGive(semaphore);
        return 0;
    }
    n -= len1;
    
    int len2 = vsnprintf(BUFFER + len1, n, fmt, data);
    
    if (len2 < 0 || len2 >= n)
    { // see https://cplusplus.com/reference/cstdio/vsnprintf/
        xSemaphoreGive(semaphore);
        return 0;
    }
    ESP_LOGI(TAG, "Logging2Syslog: %s", BUFFER);
    sendto(sock, BUFFER, len1 + len2, 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr)); // len1 +len2 is WITHOUT terminating \0-character
    
    return len1 + len2;
}

Severity char2sev(const char* ptr){
    switch (*ptr)
    {
    case 'V': return Severity::Debug;
    case 'D': return Severity::Debug;
    case 'I': return Severity::Informational;
    case 'W': return Severity::Warning;
    case 'E': return Severity::Error;
    default: return Severity::UNKNOWN;
    }
}

#if defined (CONFIG_LOG_TIMESTAMP_SOURCE_SYSTEM)
    #error("defined(CONFIG_LOG_COLORS) || defined (CONFIG_LOG_TIMESTAMP_SOURCE_SYSTEM)")
#endif

int SyslogUdp::system_log_message_route(const char *fmt, va_list data)
{
    //int64_t timestamp;
    char* pos{nullptr};
    const char* tag{nullptr};
    const char* message{nullptr};
    xSemaphoreTake(semaphore, portMAX_DELAY);
    int len = vsnprintf(BUFFER, sizeof(BUFFER), fmt, data);
    BUFFER[len] = '\0';
    auto ptr = BUFFER;
#if defined(CONFIG_LOG_COLORS)
    ptr+=7;//length of "\033[0;" COLOR "m"
#endif
    Severity sev = char2sev(ptr+0);
    Facility fac = Facility::local0;
    GOTO_ERROR_ON_FALSE(ptr[2]=='(', "No ( character found on pos 2");
    GOTO_ERROR_ON_FALSE((pos=strchr(ptr+3, ')'))!=nullptr,"No ) character found"); //pos points to )
    //timestamp = strtoull(ptr+3, nullptr, 10);
    GOTO_ERROR_ON_FALSE(*(pos+1)==' ', "No SP character found after timestamp");
    tag=pos+2;
    GOTO_ERROR_ON_FALSE((pos=strchr(tag, ':'))!=nullptr,"No : character found"); //pos points to :
    GOTO_ERROR_ON_FALSE(*(pos+1)==' ', "No SP character found after TAG");
    message=pos+2;
    *(pos)='\0';
    SyslogInternal(tag, fac, sev, message, data);
    serialHandler(fmt, data);
error:
    xSemaphoreGive(semaphore);
    return len;
}
