#ifndef ETHER_WINDOW_H
#define ETHER_WINDOW_H

#include "ether/rc.h"
#include "ether/app.h"

ether_rc ether_window_init(ether_app_t *app, const char *window_title, int width, int height);

ether_rc ether_window_destroy(ether_app_t *app);

#endif