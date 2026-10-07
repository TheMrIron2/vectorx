#ifndef VX_SCENE_H
#define VX_SCENE_H
#include "core/vector_frame.h"
#include "game/flight.h"
#include "game/weapons.h"
VxCamera vx_scene_camera(const VxFlight *flight);
void vx_build_scene(VxVectorFrame *frame, const VxFlight *flight, const VxWeapons *weapons);
#endif
