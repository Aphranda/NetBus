/**
  ******************************************************************************
  * @file    net_scpi.h
  * @brief   SCPI-over-TCP server -- listens on port 5025
  ******************************************************************************
  */

#ifndef __NET_SCPI_H__
#define __NET_SCPI_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

void   NetSCPI_Init(void);
int    NetSCPI_IsConnected(void);
size_t NetSCPI_Write(const char *data, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* __NET_SCPI_H__ */
