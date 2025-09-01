#pragma once
#include <stdint.h>
#include "host/ble_gatt.h"   // <-- this gives struct ble_gatt_svc_def

extern const struct ble_gatt_svc_def gatt_svcs[];
extern uint8_t ble_addr_type;

void ble_app_advertise(void);
void ble_app_on_sync(void);
void host_task(void *param);
