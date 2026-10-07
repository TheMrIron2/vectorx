#ifndef VX_SHIP_H
#define VX_SHIP_H
#include "core/math.h"
#include <stddef.h>

typedef struct { size_t a, b; } VxEdge;
/* Original swept-wing craft: pointed fuselage, raised wingtips, twin nacelles.
   +Z is the nose; 19 vertices and 30 edges retain the essential silhouette. */
static const VxVec3 vx_ship_vertices[] = {
    { 0.00f,  0.00f,  1.65f}, /* 0: nose */
    {-0.23f, -0.08f, -0.82f}, /* 1: rear fuselage left */
    { 0.23f, -0.08f, -0.82f}, /* 2: rear fuselage right */
    { 0.00f,  0.34f,  0.15f}, /* 3: canopy ridge */
    { 0.00f, -0.22f, -0.35f}, /* 4: keel */
    {-0.22f,  0.00f,  0.40f}, /* 5: left wing root */
    {-1.55f,  0.25f, -0.60f}, /* 6: left wingtip */
    {-0.74f, -0.04f, -0.87f}, /* 7: left trailing edge */
    { 0.22f,  0.00f,  0.40f}, /* 8: right wing root */
    { 1.55f,  0.25f, -0.60f}, /* 9: right wingtip */
    { 0.74f, -0.04f, -0.87f}, /* 10: right trailing edge */
    {-0.65f,  0.15f, -0.18f}, /* 11-14: left nacelle */
    {-0.85f,  0.08f, -0.83f},
    {-0.65f,  0.26f, -0.83f},
    {-0.45f,  0.08f, -0.83f},
    { 0.65f,  0.15f, -0.18f}, /* 15-18: right nacelle */
    { 0.85f,  0.08f, -0.83f},
    { 0.65f,  0.26f, -0.83f},
    { 0.45f,  0.08f, -0.83f}
};
static const VxEdge vx_ship_edges[] = {
    {0, 1}, {0, 2}, {1, 2}, {0, 3}, {3, 1}, {3, 2}, {0, 4}, {4, 1}, {4, 2},
    {5, 6}, {6, 7}, {7, 1}, {1, 5},
    {8, 9}, {9, 10}, {10, 2}, {2, 8},
    {11, 12}, {11, 13}, {11, 14}, {12, 13}, {13, 14}, {14, 12},
    {15, 16}, {15, 17}, {15, 18}, {16, 17}, {17, 18}, {18, 16}, {5, 8}
};
#define VX_SHIP_VERTEX_COUNT (sizeof(vx_ship_vertices) / sizeof(vx_ship_vertices[0]))
#define VX_SHIP_EDGE_COUNT (sizeof(vx_ship_edges) / sizeof(vx_ship_edges[0]))
#endif
