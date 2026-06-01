/**
  ******************************************************************************
  * @file    web_task.c
  * @brief   Web server task — SSI-based system monitoring
  *
  *          Registers an SSI tag handler with LWIP HTTPD so that HTML pages
  *          containing <!--#tag--> placeholders get dynamic system data
  *          injected at request time.
  *
  *          Thread safety: the SSI handler runs in the LWIP tcpip thread.
  *          All accessed APIs are read-only and safe from any thread.
  ******************************************************************************
  */

#include "web_task.h"
#include "lwip/apps/httpd.h"
#include "lwip/apps/fs.h"
#include "lwip/netif.h"
#include "lwip/prot/iana.h"
#include "FreeRTOS.h"
#include "task.h"
#include "cmsis_os2.h"
#include "stm32h7xx_hal.h"
#include "log.h"
#include "storage_task.h"
#include "rfsw_task.h"
#include "detector_task.h"
#include "io.h"
#include "scpi_queue.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

/* ── External declarations ────────────────────────────────────────────────── */

extern struct netif gnetif;       /* from lwip.c */

/* ── Firmware identity ────────────────────────────────────────────────────── */

#define FW_VERSION    "1.0.0"
#define FW_BUILD_DATE __DATE__ " " __TIME__

/* ── SSI tag name table (index-based handler) ─────────────────────────────── */

enum {
    SSI_TAG_UPTIME = 0,
    SSI_TAG_VERSION,
    SSI_TAG_SYSST,
    SSI_TAG_HEAP,
    SSI_TAG_IP,
    SSI_TAG_MASK,
    SSI_TAG_GW,
    SSI_TAG_MAC,
    SSI_TAG_LINK,
    SSI_TAG_STORFR,
    SSI_TAG_STORPCT,
    SSI_TAG_STORWR,
    SSI_TAG_STORREC,
    SSI_TAG_ATT1,
    SSI_TAG_ATT2,
    SSI_TAG_RFSW1,
    SSI_TAG_RFSW2,
    SSI_TAG_DET_H,
    SSI_TAG_DET_V,
    SSI_TAG_LOG,
    SSI_TAG_LOGTEXT,
    SSI_TAG_SCPI_LAST,
    SSI_TAG_COUNT
};

static const char *g_ssi_tags[] = {
    "uptime",       /* SSI_TAG_UPTIME   */
    "version",      /* SSI_TAG_VERSION  */
    "sysst",        /* SSI_TAG_SYSST    */
    "heap",         /* SSI_TAG_HEAP     */
    "ip",           /* SSI_TAG_IP       */
    "mask",         /* SSI_TAG_MASK     */
    "gw",           /* SSI_TAG_GW       */
    "mac",          /* SSI_TAG_MAC      */
    "link",         /* SSI_TAG_LINK     */
    "storfr",       /* SSI_TAG_STORFR   */
    "storpct",      /* SSI_TAG_STORPCT  */
    "storwr",       /* SSI_TAG_STORWR   */
    "storrec",      /* SSI_TAG_STORREC  */
    "att1",         /* SSI_TAG_ATT1     */
    "att2",         /* SSI_TAG_ATT2     */
    "rfsw1",        /* SSI_TAG_RFSW1    */
    "rfsw2",        /* SSI_TAG_RFSW2    */
    "det_h",        /* SSI_TAG_DET_H    */
    "det_v",        /* SSI_TAG_DET_V    */
    "log",          /* SSI_TAG_LOG      */
    "logtext",      /* SSI_TAG_LOGTEXT  */
    "lastcmd",      /* SSI_TAG_SCPI_LAST */
};

/* ── System info helpers ──────────────────────────────────────────────────── */

/**
  * @brief  Format uptime as "HH:MM:SS" into buf, return char count.
  */
static int fmt_uptime(char *buf, int len)
{
    uint32_t sec = xTaskGetTickCount() / configTICK_RATE_HZ;
    return snprintf(buf, len, "%lu:%02lu:%02lu",
                    sec / 3600U, (sec % 3600U) / 60U, sec % 60U);
}

/**
  * @brief  Get system state as a short label.
  */
static const char *sys_state_label(void)
{
    App_State_t s = App_GetState();
    switch (s) {
    case APP_STATE_RESET:   return "RESET";
    case APP_STATE_INIT:    return "INIT";
    case APP_STATE_RUNNING: return "RUNNING";
    case APP_STATE_ERROR:   return "ERROR";
    case APP_STATE_STOPPED: return "STOPPED";
    default:                return "UNKNOWN";
    }
}

/**
  * @brief  Get CSS color class for a given system state.
  */
static const char *sys_state_css(void)
{
    App_State_t s = App_GetState();
    switch (s) {
    case APP_STATE_RUNNING: return "green";
    case APP_STATE_ERROR:   return "red";
    default:                return "yellow";
    }
}

/**
  * @brief  Format IPv4 address from ip4_addr_t into dotted-decimal.
  */
static int fmt_ip4(const ip4_addr_t *addr, char *buf, int len)
{
    const uint8_t *p = (const uint8_t *)addr;
    return snprintf(buf, len, "%u.%u.%u.%u", p[0], p[1], p[2], p[3]);
}

/**
  * @brief  Format MAC address.
  */
static int fmt_mac(const uint8_t *mac, char *buf, int len)
{
    return snprintf(buf, len, "%02X:%02X:%02X:%02X:%02X:%02X",
                    mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

/* ── Log ring buffer + SCPI capture (used by SSI handler) ───────────────── */

#define WEB_LOG_BUF_SIZE  2048
static char g_web_log_buf[WEB_LOG_BUF_SIZE];
static int  g_web_log_pos = 0;
static int  g_web_log_total = 0; /* total bytes written, used to skip old data */
static char g_last_scpi_cmd[128] = "";

/* ── SSI handler ──────────────────────────────────────────────────────────── */

/**
  * @brief  LWIP HTTPD SSI tag handler — called for each <!--#tag--> in a page.
  * @param  iIndex      Zero-based index into g_ssi_tags[]
  * @param  pcInsert    Output buffer for replacement text
  * @param  iInsertLen  Size of pcInsert buffer
  * @return Number of characters written to pcInsert (excluding NUL)
  */
static u16_t Web_SSIHandler(int iIndex, char *pcInsert, int iInsertLen)
{
    if (iIndex < 0 || iIndex >= SSI_TAG_COUNT) {
        return 0;
    }

    switch (iIndex) {

    case SSI_TAG_UPTIME:
        return (u16_t)fmt_uptime(pcInsert, iInsertLen);

    case SSI_TAG_VERSION:
        return (u16_t)snprintf(pcInsert, iInsertLen, FW_VERSION);

    case SSI_TAG_SYSST:
        return (u16_t)snprintf(pcInsert, iInsertLen,
                               "<span class=\"led %s\"></span>%s",
                               sys_state_css(), sys_state_label());

    case SSI_TAG_HEAP:
        return (u16_t)snprintf(pcInsert, iInsertLen, "%u",
                               (unsigned)xPortGetFreeHeapSize());

    case SSI_TAG_IP:
        return (u16_t)fmt_ip4(netif_ip4_addr(&gnetif), pcInsert, iInsertLen);

    case SSI_TAG_MASK:
        return (u16_t)fmt_ip4(netif_ip4_netmask(&gnetif), pcInsert, iInsertLen);

    case SSI_TAG_GW:
        return (u16_t)fmt_ip4(netif_ip4_gw(&gnetif), pcInsert, iInsertLen);

    case SSI_TAG_MAC:
        return (u16_t)fmt_mac(gnetif.hwaddr, pcInsert, iInsertLen);

    case SSI_TAG_LINK:
        return (u16_t)snprintf(pcInsert, iInsertLen,
                               "<span class=\"led %s\"></span>%s",
                               netif_is_link_up(&gnetif) ? "green" : "red",
                               netif_is_link_up(&gnetif) ? "UP 100M" : "DOWN");

    case SSI_TAG_STORFR:
        return (u16_t)snprintf(pcInsert, iInsertLen, "%lu",
                               (unsigned long)Storage_FreeSpace());

    case SSI_TAG_STORPCT: {
        uint32_t free = Storage_FreeSpace();
        /* 128KB total, 32B header */
        uint32_t used = (STORAGE_SECTOR_SIZE - STORAGE_HEADER_SIZE) - free;
        uint32_t pct  = (used * 100U) / (STORAGE_SECTOR_SIZE - STORAGE_HEADER_SIZE);
        return (u16_t)snprintf(pcInsert, iInsertLen, "%lu%%", (unsigned long)pct);
    }

    case SSI_TAG_STORWR:
        return (u16_t)snprintf(pcInsert, iInsertLen, "%lu",
                               (unsigned long)Storage_GetWriteCount());

    case SSI_TAG_STORREC:
        return (u16_t)snprintf(pcInsert, iInsertLen, "%lu",
                               (unsigned long)Storage_GetRecordCount());

    case SSI_TAG_ATT1:
        return (u16_t)snprintf(pcInsert, iInsertLen, "%u", IO_GetAttenuatorA());

    case SSI_TAG_ATT2:
        return (u16_t)snprintf(pcInsert, iInsertLen, "%u", IO_GetAttenuatorB());

    case SSI_TAG_RFSW1: {
        uint8_t ch = RFSW_GetChannel(1U);
        if (ch >= 1 && ch <= 10) return (u16_t)snprintf(pcInsert, iInsertLen, "%u", ch);
        return (u16_t)snprintf(pcInsert, iInsertLen, "?");
    }
    case SSI_TAG_RFSW2: {
        uint8_t ch = RFSW_GetChannel(2U);
        if (ch >= 1 && ch <= 10) return (u16_t)snprintf(pcInsert, iInsertLen, "%u", ch);
        return (u16_t)snprintf(pcInsert, iInsertLen, "?");
    }

    case SSI_TAG_DET_H: {
        if (Detector_IsDataValid()) {
            int16_t val = Detector_GetLastHPower();
            return (u16_t)snprintf(pcInsert, iInsertLen, "%.2f dBm", val / 100.0);
        }
        return (u16_t)snprintf(pcInsert, iInsertLen, "---");
    }

    case SSI_TAG_DET_V: {
        if (Detector_IsDataValid()) {
            int16_t val = Detector_GetLastVPower();
            return (u16_t)snprintf(pcInsert, iInsertLen, "%.2f dBm", val / 100.0);
        }
        return (u16_t)snprintf(pcInsert, iInsertLen, "---");
    }

    case SSI_TAG_SCPI_LAST: {
        /* HTML-escape for safe use in attribute value */
        int n = 0;
        for (const char *s = g_last_scpi_cmd; *s && n < iInsertLen - 7; s++) {
            char c = *s;
            if (c == '"')      { memcpy(pcInsert + n, "&quot;", 6); n += 6; }
            else if (c == '<') { memcpy(pcInsert + n, "&lt;", 4); n += 4; }
            else if (c == '>') { memcpy(pcInsert + n, "&gt;", 4); n += 4; }
            else if (c == '&') { memcpy(pcInsert + n, "&amp;", 5); n += 5; }
            else pcInsert[n++] = c;
        }
        return (u16_t)n;
    }

    case SSI_TAG_LOG: {
        int n = 0;
        int start = 0;
        int total = g_web_log_pos;
        if (total > 800) start = total - 800;
        for (int i = start; i < total && n < iInsertLen - 7; i++) {
            char c = g_web_log_buf[i];
            if (c == '\0') c = ' ';
            if (c == '<')      { memcpy(pcInsert + n, "&lt;", 4); n += 4; }
            else if (c == '>') { memcpy(pcInsert + n, "&gt;", 4); n += 4; }
            else if (c == '&') { memcpy(pcInsert + n, "&amp;", 5); n += 5; }
            else pcInsert[n++] = c;
        }
        if (n == 0) { pcInsert[0] = '-'; n = 1; }
        return (u16_t)n;
    }

    case SSI_TAG_LOGTEXT: {
        int n = 0;
        int start = 0;
        int total = g_web_log_pos; /* chars in buffer */
        if (total > 600) start = total - 600; /* last 600 chars only */
        for (int i = start; i < total && n < iInsertLen - 7; i++) {
            char c = g_web_log_buf[i];
            if (c == '\0') c = ' ';
            if (c == '<')      { memcpy(pcInsert + n, "&lt;", 4); n += 4; }
            else if (c == '>') { memcpy(pcInsert + n, "&gt;", 4); n += 4; }
            else if (c == '&') { memcpy(pcInsert + n, "&amp;", 5); n += 5; }
            else pcInsert[n++] = c;
        }
        if (n == 0) { pcInsert[0] = '-'; n = 1; }
        return (u16_t)n;
    }

    default:
        return 0;
    }
}

/* ── CGI handlers ──────────────────────────────────────────────────────────── */

/* CGI URI table (GET-based) */
enum {
    CGI_NETCFG = 0,
    CGI_REBOOT,
    CGI_SCPI,
    CGI_LOG_CLEAR,
    CGI_COUNT
};

/**
  * @brief  Find a CGI parameter value by name.
  * @return Pointer to value string, or NULL if not found.
  */
static const char *cgi_param(const char *name, int n, char *pcParam[], char *pcValue[])
{
    for (int i = 0; i < n; i++) {
        if (pcParam[i] != NULL && pcValue[i] != NULL
            && strcmp(pcParam[i], name) == 0) {
            return pcValue[i];
        }
    }
    return NULL;
}

/**
  * @brief  Parse an IPv4 dotted-decimal string into octets.
  * @retval 1 on success, 0 on malformed input.
  */
static int parse_ip4(const char *s, uint8_t *octets)
{
    unsigned int o[4];
    int n = sscanf(s, "%u.%u.%u.%u", &o[0], &o[1], &o[2], &o[3]);
    if (n != 4) return 0;
    for (int i = 0; i < 4; i++) {
        if (o[i] > 255) return 0;
        octets[i] = (uint8_t)o[i];
    }
    return 1;
}

/**
  * @brief  Validate a netmask (must be contiguous 1-bits from MSB).
  */
static int is_valid_mask(const uint8_t *octets)
{
    uint32_t m = ((uint32_t)octets[0] << 24) | ((uint32_t)octets[1] << 16)
               | ((uint32_t)octets[2] << 8)  |  (uint32_t)octets[3];
    if (m == 0) return 0;
    /* A valid netmask is all-1s then all-0s: flip all bits, add 1 → power of 2 */
    uint32_t inv = ~m;
    return ((inv + 1) & inv) == 0;
}

/**
  * @brief  CGI: /netcfg.cgi — save network configuration to flash.
  *
  * Expected params: ip, mask, gw, scpi_port
  * Saves to storage keys: net.ip, net.mask, net.gw, net.port
  * Settings take effect after reboot.
  */
static const char *Web_NetConfigCGI(int iIndex, int iNumParams,
                                     char *pcParam[], char *pcValue[])
{
    (void)iIndex;
    const char *ip_str  = cgi_param("ip",  iNumParams, pcParam, pcValue);
    const char *mask_str = cgi_param("mask", iNumParams, pcParam, pcValue);
    const char *gw_str  = cgi_param("gw",  iNumParams, pcParam, pcValue);
    const char *port_str = cgi_param("port", iNumParams, pcParam, pcValue);

    /* ── Validate IP ────────────────────────────────────────────────── */
    if (ip_str == NULL || mask_str == NULL || gw_str == NULL) {
        return "/netcfg_err.html";
    }

    uint8_t ip[4], mask[4], gw[4];
    if (!parse_ip4(ip_str, ip))   return "/netcfg_err.html";
    if (!parse_ip4(mask_str, mask)) return "/netcfg_err.html";
    if (!parse_ip4(gw_str, gw))   return "/netcfg_err.html";
    if (!is_valid_mask(mask))     return "/netcfg_err.html";

    /* ── Validate SCPI port ──────────────────────────────────────────── */
    uint32_t port = 5025;
    if (port_str != NULL && strlen(port_str) > 0) {
        char *end;
        long p = strtol(port_str, &end, 10);
        if (*end != '\0' || p < 1 || p > 65535) return "/netcfg_err.html";
        port = (uint32_t)p;
    }

    /* ── Save to flash ───────────────────────────────────────────────── */
    Storage_Set("net.ip",   STORAGE_TYPE_STR, ip_str,  (uint16_t)strlen(ip_str));
    Storage_Set("net.mask", STORAGE_TYPE_STR, mask_str,(uint16_t)strlen(mask_str));
    Storage_Set("net.gw",   STORAGE_TYPE_STR, gw_str,  (uint16_t)strlen(gw_str));
    Storage_Set("net.port", STORAGE_TYPE_U32, &port,   sizeof(port));
    Storage_Commit();

    return "/netcfg_ok.html";
}

/**
  * @brief  CGI: /reboot.cgi — trigger system reset.
  * @note   NVIC_SystemReset() is synchronous; the response page may not
  *         reach the browser before the MCU resets.
  */
static const char *Web_RebootCGI(int iIndex, int iNumParams,
                                  char *pcParam[], char *pcValue[])
{
    (void)iIndex;
    (void)iNumParams;
    (void)pcParam;
    (void)pcValue;
    LOG_WARN("Web: reboot requested — resetting");
    NVIC_SystemReset();
    return "/index.html";   /* unreachable */
}

static void Web_LogWrite(const char *data, size_t len)
{
    for (size_t i = 0; i < len; i++) {
        char c = data[i];
        if (c == '\r') continue;
        /* Advance pos, null-terminate at new pos BEFORE writing char
           to old pos — ensures \0 is always present for concurrent reads. */
        g_web_log_pos = (g_web_log_pos + 1) % WEB_LOG_BUF_SIZE;
        g_web_log_buf[g_web_log_pos] = '\0';
        int prev = (g_web_log_pos - 1 + WEB_LOG_BUF_SIZE) % WEB_LOG_BUF_SIZE;
        g_web_log_buf[prev] = c;
        g_web_log_total++;
    }
}

/* ── SCPI CGI: enqueue command into existing SCPI task pipeline ──────────── */

/**
  * @brief  CGI: /scpi.cgi?cmd=... — enqueue SCPI command, return to dashboard.
  *         Command is processed asynchronously by SCPI task; output appears
  *         in the system log (right panel via Log_WebHook).
  */
static const char *Web_SCPI_CGI(int iIndex, int iNumParams,
                                 char *pcParam[], char *pcValue[])
{
    (void)iIndex;
    const char *cmd = cgi_param("cmd", iNumParams, pcParam, pcValue);
    if (cmd == NULL || strlen(cmd) == 0) return "/index.html";

    /* URL-decode %XX escapes */
    char decoded[128];
    int di = 0;
    for (const char *s = cmd; *s && di < (int)sizeof(decoded) - 1; s++) {
        if (*s == '%' && s[1] && s[2]) {
            char hex[3] = { s[1], s[2], '\0' };
            decoded[di++] = (char)strtol(hex, NULL, 16);
            s += 2;
        } else if (*s == '+') {
            decoded[di++] = ' ';
        } else {
            decoded[di++] = *s;
        }
    }
    decoded[di] = '\0';

    /* Save for input field retention */
    strncpy(g_last_scpi_cmd, decoded, sizeof(g_last_scpi_cmd) - 1);
    g_last_scpi_cmd[sizeof(g_last_scpi_cmd) - 1] = '\0';

    /* Append \r\n terminator */
    if (di < (int)sizeof(decoded) - 3) {
        decoded[di++] = '\r';
        decoded[di++] = '\n';
        decoded[di] = '\0';
    }

    SCPI_EnqueueLine(decoded);
    return "/scpi_done.html";
}

static const char *Web_LogClearCGI(int iIndex, int iNumParams,
                                    char *pcParam[], char *pcValue[])
{
    (void)iIndex; (void)iNumParams; (void)pcParam; (void)pcValue;
    g_web_log_pos = 0;
    g_web_log_buf[0] = '\0';
    g_web_log_pos = 0;
    return "/log_view.html";
}

static const tCGI g_webCGIs[] = {
    { "/netcfg.cgi",     Web_NetConfigCGI },
    { "/reboot.cgi",     Web_RebootCGI },
    { "/scpi.cgi",       Web_SCPI_CGI },
    { "/log_clear.cgi",  Web_LogClearCGI },
};

/* ── Module interface ─────────────────────────────────────────────────────── */

static App_Status_t _web_task_init(void)
{
    http_set_ssi_handler(Web_SSIHandler, g_ssi_tags, SSI_TAG_COUNT);
    LOG_INFO("Web: SSI handler registered (%d tags)", (int)SSI_TAG_COUNT);

    http_set_cgi_handlers(g_webCGIs, CGI_COUNT);
    LOG_INFO("Web: CGI handlers registered (%d URIs)", (int)CGI_COUNT);

    Log_SetWebHook(Web_LogWrite);
    return APP_OK;
}

static App_Status_t _web_task_process(void)
{
    /* No polling — all values read on-demand in SSI handler (page refresh). */
    return APP_OK;
}

static void _web_task_on_error(App_Status_t err)
{
    LOG_ERROR("Web: error %d", (int)err);
    (void)err;
}

const App_Module_t g_web_task_module = {
    .name     = "Web",
    .init     = _web_task_init,
    .process  = _web_task_process,
    .on_error = _web_task_on_error,
};
