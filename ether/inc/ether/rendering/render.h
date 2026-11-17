#ifndef ETHER_RENDER_H
#define ETHER_RENDER_H

#include "ether/rc.h"
#include "ether/app.h"

ether_rc ether_render_init(ether_app_t *app);

ether_rc ether_render(ether_app_t *app);

void ether_render_destroy(ether_app_t *app);

#endif