#ifndef ETHER_SWAPCHAIN_H
#define ETHER_SWAPCHAIN_H

#include "ether/rc.h"
#include "ether/app.h"

ether_rc ether_generate_swapchain(ether_app_t *app);

void ether_swapchain_destroy(ether_app_t *app);

#endif