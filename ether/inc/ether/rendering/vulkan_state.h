#ifndef ETHER_VULKAN_STATE_H
#define ETHER_VULKAN_STATE_H

#include "ether/app.h"
#include "ether/rc.h"

#define TARGET_VULKAN_VERSION VK_API_VERSION_1_4

ether_rc ether_vulkan_init(ether_app_t *app);

ether_rc ether_vulkan_destroy(ether_app_t *app);
#endif