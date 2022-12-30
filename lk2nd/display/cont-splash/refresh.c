// SPDX-License-Identifier: BSD-3-Clause
/* Copyright (c) 2020-2022, Stephan Gerhold <stephan@gerhold.net> */

#include <bits.h>
#include <debug.h>
#include <dev/fbcon.h>
#include <kernel/event.h>
#include <kernel/thread.h>
#include <reg.h>

#include "cont-splash.h"
#include "mdp.h"

static event_t refresh_event;

static void mdp_refresh(void)
{
#if MDP3 || MDP4
	writel(1, MDP_DMA_P_START);
#elif MDP5
	writel(1, 0xFD900000);
#endif
}

#define MDP_PP_SYNC_CONFIG_VSYNC	0x004
#define MDP_PP_AUTOREFRESH_CONFIG	0x030

static void mdp5_enable_auto_refresh(struct fbcon_config *fb)
{
	uint32_t vsync_count = 19200000 / (fb->height * 60); /* 60 fps */
	uint32_t mdss_mdp_rev = readl(MDP_HW_REV);
	uint32_t pp0_base;

	if (mdss_mdp_rev >= MDSS_MDP_HW_REV_105)
		pp0_base = REG_MDP(0x71000);
	else if (mdss_mdp_rev >= MDSS_MDP_HW_REV_102)
		pp0_base = REG_MDP(0x12D00);
	else
		pp0_base = REG_MDP(0x21B00);

	writel(vsync_count | BIT(19), pp0_base + MDP_PP_SYNC_CONFIG_VSYNC);
	writel(BIT(31) | 1, pp0_base + MDP_PP_AUTOREFRESH_CONFIG);
	writel(1, MDP_CTL_0_BASE + CTL_START);
}

bool mdp_start_refresh(struct fbcon_config *fb)
{
	bool cmd_mode, auto_refresh = false;
	uint32_t sel;

#if MDP3
	sel = readl(MDP_DMA_P_CONFIG);
	cmd_mode = BITS_SHIFT(sel, 20, 19) == 0x1; /* OUT_SEL == DSI_CMD? */
#elif MDP4
	sel = readl(MDP_DISP_INTF_SEL);
	cmd_mode = BITS_SHIFT(sel, 1, 0) == 0x2; /* PRIM_INTF_SEL == DSI_CMD? */
#elif MDP5
	sel = readl(MDP_CTL_0_BASE + CTL_TOP);
	cmd_mode = !!(sel & BIT(17)); /* MODE_SEL == Command Mode? */
#endif

#ifdef MDP_AUTOREFRESH_CONFIG_P /* MDP3/MDP4 */
	auto_refresh = !!(readl(MDP_AUTOREFRESH_CONFIG_P) & BIT(28));
#endif
#ifdef MDSS_MDP_REG_PP_AUTOREFRESH_CONFIG /* MDP5 */
	auto_refresh = !!(readl(MDP_PP_0_BASE + MDSS_MDP_REG_PP_AUTOREFRESH_CONFIG) & BIT(31));
#endif

	dprintf(INFO, "Display refresh: cmd mode: %d, auto refresh: %d (sel: %#x)\n",
		cmd_mode, auto_refresh, sel);

#ifdef MDP_DISPLAY_STATUS /* MDP4 */
	if (!cmd_mode && readl(MDP_DISPLAY_STATUS) == 0) {
		dprintf(CRITICAL, "Cannot use continuous splash: display not active\n");
		return false;
	}
#endif

	if (cmd_mode && !auto_refresh)
		mdp5_enable_auto_refresh(fb);

	return true;
}
