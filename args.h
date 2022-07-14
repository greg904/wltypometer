#ifndef WLTYPOMETER_ARGS_H
#define WLTYPOMETER_ARGS_H

#include <stdint.h>

extern uint32_t args_x;
extern uint32_t args_y;
extern uint32_t args_width;
extern uint32_t args_height;

int args_parse(int argc, char **argv);

#endif
