/**
  ******************************************************************************
  * @file    web_task.h
  * @brief   Web server task module — SSI handler registration
  *
  *          Responsibilities:
  *            - Register SSI tag handler with LWIP HTTPD
  *            - Provide dynamic system data for web monitoring pages
  *
  *          SSI Tags (usable in HTML as <!--#tag-->):
  *            uptime   — system uptime in HH:MM:SS
  *            version  — firmware version string
  *            bldate   — build date
  *            sysst    — system state (RUNNING/ERROR/etc.)
  *            heap     — free heap bytes
  *            heapmin  — minimum free heap ever
  *            tasks    — FreeRTOS task table (HTML rows)
  *            ip       — IP address
  *            mask     — netmask
  *            gw       — gateway
  *            mac      — MAC address
  *            link     — link status (UP/DOWN)
  *            storfr   — storage free space (bytes)
  *            storpct  — storage usage percentage
  *            storwr   — storage write counter
  *            storrec  — storage record count
  *            modst    — module status HTML
  ******************************************************************************
  */

#ifndef __WEB_TASK_H__
#define __WEB_TASK_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "app_task.h"

extern const App_Module_t g_web_task_module;

#ifdef __cplusplus
}
#endif

#endif /* __WEB_TASK_H__ */
