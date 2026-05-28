/**
  ******************************************************************************
  * @file    net_scpi.c
  * @brief   SCPI-over-TCP server — LWIP sockets, port 5025
  * @note    Thread safety: SCPI_TryParse() handles its own mutex internally.
  ******************************************************************************
  */

#include <string.h>
#include "cmsis_os2.h"
#include "lwip/sockets.h"
#include "net_scpi.h"
#include "log.h"
#include "scpi-def.h"

#define SCPI_TCP_PORT        5025
#define SCPI_TCP_STACK_SIZE  2048
#define SCPI_TCP_BUF_SIZE    512

static int tcp_fd    = -1;   /* listening socket */
static int client_fd = -1;   /* active client (only one) */

/* ── Write to TCP client ────────────────────────────────────────── */

size_t NetSCPI_Write(const char *data, size_t len)
{
    if (client_fd < 0) return 0;
    int sent = lwip_send(client_fd, data, len, 0);
    return (sent > 0) ? (size_t)sent : 0;
}

int NetSCPI_IsConnected(void)
{
    return (client_fd >= 0) ? 1 : 0;
}

/* ── Receive and parse SCPI commands from TCP client ────────────── */

static void NetSCPI_ProcessClient(void)
{
    char buf[SCPI_TCP_BUF_SIZE];
    static char line_buf[SCPI_INPUT_BUFFER_LENGTH];
    static size_t line_pos = 0;

    int n = lwip_recv(client_fd, buf, sizeof(buf) - 1, 0);
    if (n <= 0)
    {
        LOG_INFO("SCPI-TCP: client disconnected");
        lwip_close(client_fd);
        client_fd = -1;
        line_pos = 0;
        return;
    }
    buf[n] = '\0';

    for (int i = 0; i < n; i++)
    {
        char c = buf[i];
        if (c == '\r') continue;
        if (c == '\n')
        {
            if (line_pos == 0) continue;
            line_buf[line_pos] = '\0';
            line_pos = 0;
            SCPI_TryParse(line_buf);  /* mutex handled inside */
        }
        else if (line_pos < sizeof(line_buf) - 1)
        {
            line_buf[line_pos++] = c;
        }
    }
}

/* ── TCP server task ───────────────────────────────────────────── */

static void NetSCPI_Task(void *arg)
{
    (void)arg;

    osDelay(2000);  /* wait for LWIP to be ready */

    tcp_fd = lwip_socket(AF_INET, SOCK_STREAM, 0);
    if (tcp_fd < 0)
    {
        LOG_ERROR("SCPI-TCP: socket create failed");
        goto exit;
    }

    int opt = 1;
    lwip_setsockopt(tcp_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_port        = lwip_htons(SCPI_TCP_PORT);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (lwip_bind(tcp_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        LOG_ERROR("SCPI-TCP: bind port %d failed", SCPI_TCP_PORT);
        lwip_close(tcp_fd);
        goto exit;
    }

    if (lwip_listen(tcp_fd, 1) < 0)
    {
        LOG_ERROR("SCPI-TCP: listen failed");
        lwip_close(tcp_fd);
        goto exit;
    }

    LOG_INFO("SCPI-TCP: listening on port %d", SCPI_TCP_PORT);

    for (;;)
    {
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);

        client_fd = lwip_accept(tcp_fd, (struct sockaddr *)&client_addr, &addr_len);
        if (client_fd < 0)
        {
            osDelay(100);
            continue;
        }

        LOG_INFO("SCPI-TCP: client connected");

        while (client_fd >= 0)
        {
            NetSCPI_ProcessClient();
            osDelay(10);
        }
    }

exit:
    if (tcp_fd >= 0) { lwip_close(tcp_fd); tcp_fd = -1; }
    if (client_fd >= 0) { lwip_close(client_fd); client_fd = -1; }
    LOG_ERROR("SCPI-TCP: task error, sleeping");
    for (;;) { osDelay(1000); }
}

/* ── Public init ────────────────────────────────────────────────── */

void NetSCPI_Init(void)
{
    osThreadAttr_t attr = {
        .name       = "SCPI_TCP",
        .stack_size = SCPI_TCP_STACK_SIZE,
        .priority   = osPriorityNormal,
    };
    osThreadNew(NetSCPI_Task, NULL, &attr);
}
