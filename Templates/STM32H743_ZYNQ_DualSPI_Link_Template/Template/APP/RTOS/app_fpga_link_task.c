/**
  ******************************************************************************
  * @file    app_fpga_link_task.c
  * @brief   CubeMX FpgaLinkTask 简短入口（UTF-8）。
  ******************************************************************************
  */

#include "app_fpga_link_task.h"

#include "fpga_link_service.h"

void StartFpgaLinkTask(void *argument)
{
    (void)argument;
    FpgaLink_Run();
}


