// SPDX-License-Identifier: BSD-3-Clause
/* Copyright (c) 2019-2022, Stephan Gerhold <stephan@gerhold.net> */

#include <bits.h>
#include <debug.h>
#include <dev/fbcon.h>
#include <reg.h>

#include "cont-splash.h"
#include "mdp.h"

#if MDP4
/* Defines already in mdp4.h, only make names consistent */
#define PIPE_SRC_IMG_SIZE	PIPE_SRC_SIZE
#define PIPE_SRC_OUT_SIZE	PIPE_OUT_SIZE
#define PIPE_SRC0_ADDR		PIPE_SRCP0_ADDR
#define PIPE_SRC_YSTRIDE	PIPE_SRC_YSTRIDE1
#elif MDP5
#define PIPE_SRC_SIZE		PIPE_SSPP_SRC_SIZE
#define PIPE_SRC_IMG_SIZE	PIPE_SSPP_SRC_IMG_SIZE
#define PIPE_SRC_XY		PIPE_SSPP_SRC_XY
#define PIPE_SRC_OUT_SIZE	PIPE_SSPP_SRC_OUT_SIZE
#define PIPE_OUT_XY		PIPE_SSPP_OUT_XY
#define PIPE_SRC0_ADDR		PIPE_SSPP_SRC0_ADDR
#define PIPE_SRC_YSTRIDE	PIPE_SSPP_SRC_YSTRIDE
#define PIPE_SRC_FORMAT		PIPE_SSPP_SRC_FORMAT
#endif

struct mdp_pipe {
	const char *name;
	uint32_t base;
};

static const struct mdp_pipe mdp_pipes[] = {
#if MDP4
	{
		.name = "RGB1",
		.base = MDP_RGB1_BASE,
	},
#elif MDP5
	{
		.name = "VIG_0",
		.base = MDP_VP_0_VIG_0_BASE,
	},
	{
		.name = "RGB_0",
		.base = MDP_VP_0_RGB_0_BASE,
	},
	{
		.name = "DMA_0",
		.base = MDP_VP_0_DMA_0_BASE,
	},
#endif
};

static const struct mdp_pipe *mdp_find_pipe(struct fbcon_config *fb)
{
	const struct mdp_pipe *pipe = mdp_pipes;
	const struct mdp_pipe *pipe_end = pipe + ARRAY_SIZE(mdp_pipes);

	for (; pipe < pipe_end; pipe++) {
		fb->base = (void *)readl(pipe->base + PIPE_SRC0_ADDR);
		if (fb->base)
			return pipe;
	}
	return NULL;
}

bool mdp_read_pipe_config(struct fbcon_config *fb)
{
	uint32_t src_size, img_size, src_xy, out_size, out_xy, stride, format, bpp, var;
	const struct mdp_pipe *pipe;

#if MDP4
	if (readl(MDP_LAYERMIXER_IN_CFG) == 0)
		return false; /* No pipes active */
#endif

	pipe = mdp_find_pipe(fb);
	if (!pipe)
		return false;

	#define TARGET_FB_FORMAT 0x000236FF//0x000237FF

	if (format != TARGET_FB_FORMAT) {
		dprintf(CRITICAL, "Changing FB format %#x -> %#x\n", format, TARGET_FB_FORMAT);
		/*writel(0x000237FF, pipe->base + PIPE_SSPP_SRC_FORMAT);
		writel(0x03020001, pipe->base + PIPE_SSPP_SRC_UNPACK_PATTERN);
		writel(1440, pipe->base + PIPE_SSPP_SRC_YSTRIDE);
		writel(BIT(3), MDP_CTL_0_BASE + CTL_FLUSH);*/

		writel(0x000236FF, 0xfd905030);
  		writel(0x03020001, 0xfd905034);
  		//writel(1440*3, 0xfd905024);

		writel(BIT(3), 0xfd902018);

		stride = readl(pipe->base + PIPE_SSPP_SRC_YSTRIDE);
		fb->stride = stride / (fb->bpp/8);
		fb->width = fb->stride;
	}

	src_size = readl(pipe->base + PIPE_SRC_SIZE);
	img_size = readl(pipe->base + PIPE_SRC_IMG_SIZE);
	src_xy = readl(pipe->base + PIPE_SRC_XY);
	out_size = readl(pipe->base + PIPE_SRC_OUT_SIZE);
	out_xy = readl(pipe->base + PIPE_OUT_XY);
	stride = readl(pipe->base + PIPE_SRC_YSTRIDE);
	format = readl(pipe->base + PIPE_SRC_FORMAT);
	bpp = BITS_SHIFT(format, 10, 9) + 1; /* SRC_BPP */
	var = MDP_CTL_0_BASE + CTL_START;

	dprintf(INFO, "MDP continuous splash detected: pipe %s, base: %p, stride: %d, "
		"src: %dx%d (%d,%d), img: %dx%d, out: %dx%d (%d,%d), format: %#x (bpp: %d), mdp_start : %s, ystride = %p, format = %p, unpack_pattern = %p\n, fb-> height = %d, fb->width = %d, MDP_CTL_0_BASE + CTL_FLUSH = %p\n",
		pipe->name, pipe->base, stride,
		MDP_X(src_size), MDP_Y(src_size), MDP_X(src_xy), MDP_Y(src_xy),
		MDP_X(img_size), MDP_Y(img_size),
		MDP_X(out_size), MDP_Y(out_size), MDP_X(out_xy), MDP_Y(out_xy),
		format, bpp, var, pipe->base + PIPE_SRC_YSTRIDE, pipe->base + PIPE_SRC_FORMAT, 
		pipe->base + PIPE_SSPP_SRC_UNPACK_PATTERN, fb->height, fb->width, MDP_CTL_0_BASE + CTL_FLUSH
	);

	fb->width = MDP_X(img_size);
	fb->height = MDP_Y(img_size);
	fb->stride = stride / bpp;
	fb->bpp = bpp * 8;

	dprintf(INFO, "fb-> height = %d, fb->width = %d\n", fb->height, fb->width);

	if (bpp == 2)
		fb->format = FB_FORMAT_RGB565;
	else
		fb->format = FB_FORMAT_RGB888;

	return true;
}
