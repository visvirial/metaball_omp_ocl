#ifndef METABALL_COMMON_H
#define METABALL_COMMON_H

#define KERNEL_NAME            ("metaball")
#define KERNEL_SOURCE_NAME     ("metaball.cl")
#define KERNEL_SOURCE_BUF_SIZE (0x100000)

// Window size
#define WIDTH  (1200)
#define HEIGHT (800)
#define SPEED  (200) // pixels per second
// The number of balls
#define N_BALLS (256)
// Ball setting
#define FACTOR (100.0)
#define THRESHOLD (0.008)
#define COLOR_R (255)
#define COLOR_G (51)
#define COLOR_B (102)
#define INNER_COLOR_R (255)
#define INNER_COLOR_G (255)
#define INNER_COLOR_B (255)
#define TEXT_COLOR_R (255)
#define TEXT_COLOR_G (255)
#define TEXT_COLOR_B (255)
#define FONT_SIZE (10)

#endif
