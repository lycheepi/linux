// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) STMicroelectronics SA 2017
 *
 * Authors: Philippe Cornu <philippe.cornu@st.com>
 *          Yannick Fertre <yannick.fertre@st.com>
 */

#include <drm/drmP.h>
#include <drm/drm_mipi_dsi.h>
#include <drm/drm_panel.h>
#include <linux/backlight.h>
#include <linux/gpio/consumer.h>
#include <linux/regulator/consumer.h>
#include <video/mipi_display.h>

#define DRV_NAME "raydium_rm68200"

/*** Manufacturer Command Set ***/
#define MCS_CMD_MODE_SW	0xFE /* CMD Mode Switch */
#define MCS_CMD1_UCS	0x00 /* User Command Set (UCS = CMD1) */
#define MCS_CMD2_P0	0x01 /* Manufacture Command Set Page0 (CMD2 P0) */
#define MCS_CMD2_P1	0x02 /* Manufacture Command Set Page1 (CMD2 P1) */
#define MCS_CMD2_P2	0x03 /* Manufacture Command Set Page2 (CMD2 P2) */
#define MCS_CMD2_P3	0x04 /* Manufacture Command Set Page3 (CMD2 P3) */

/* CMD2 P0 commands (Display Options and Power) */
#define MCS_STBCTR	0x12 /* TE1 Output Setting Zig-Zag Connection */
#define MCS_SGOPCTR	0x16 /* Source Bias Current */
#define MCS_SDCTR	0x1A /* Source Output Delay Time */
#define MCS_INVCTR	0x1B /* Inversion Type */
#define MCS_EXT_PWR_IC	0x24 /* External PWR IC Control */
#define MCS_SETAVDD	0x27 /* PFM Control for AVDD Output */
#define MCS_SETAVEE	0x29 /* PFM Control for AVEE Output */
#define MCS_BT2CTR	0x2B /* DDVDL Charge Pump Control */
#define MCS_BT3CTR	0x2F /* VGH Charge Pump Control */
#define MCS_BT4CTR	0x34 /* VGL Charge Pump Control */
#define MCS_VCMCTR	0x46 /* VCOM Output Level Control */
#define MCS_SETVGN	0x52 /* VG M/S N Control */
#define MCS_SETVGP	0x54 /* VG M/S P Control */
#define MCS_SW_CTRL	0x5F /* Interface Control for PFM and MIPI */

/* CMD2 P2 commands (GOA Timing Control) - no description in datasheet */
#define GOA_VSTV1	0x00
#define GOA_VSTV2	0x07
#define GOA_VCLK1	0x0E
#define GOA_VCLK2	0x17
#define GOA_VCLK_OPT1	0x20
#define GOA_BICLK1	0x2A
#define GOA_BICLK2	0x37
#define GOA_BICLK3	0x44
#define GOA_BICLK4	0x4F
#define GOA_BICLK_OPT1	0x5B
#define GOA_BICLK_OPT2	0x60
#define MCS_GOA_GPO1	0x6D
#define MCS_GOA_GPO2	0x71
#define MCS_GOA_EQ	0x74
#define MCS_GOA_CLK_GALLON 0x7C
#define MCS_GOA_FS_SEL0	0x7E
#define MCS_GOA_FS_SEL1	0x87
#define MCS_GOA_FS_SEL2	0x91
#define MCS_GOA_FS_SEL3	0x9B
#define MCS_GOA_BS_SEL0	0xAC
#define MCS_GOA_BS_SEL1	0xB5
#define MCS_GOA_BS_SEL2	0xBF
#define MCS_GOA_BS_SEL3	0xC9
#define MCS_GOA_BS_SEL4	0xD3

/* CMD2 P3 commands (Gamma) */
#define MCS_GAMMA_VP	0x60 /* Gamma VP1~VP16 */
#define MCS_GAMMA_VN	0x70 /* Gamma VN1~VN16 */

struct rm68200 {
	struct device *dev;
	struct drm_panel panel;
	struct gpio_desc *reset_gpio;
	struct regulator *supply;
	struct backlight_device *bl_dev;
	bool prepared;
	bool enabled;
};

static const struct drm_display_mode default_mode = {
	.clock = 66000,
	.hdisplay = 720,
	.hsync_start = 720 + 10,
	.hsync_end = 720 + 10 + 16,
	.htotal = 720 + 10 + 16 + 10,
	.vdisplay = 1280,
	.vsync_start = 1280 + 10,
	.vsync_end = 1280 + 10 + 16,
	.vtotal = 1280 + 10 + 16 + 10,
	.vrefresh = 50,
	.flags = 0,
	.width_mm = 68,
	.height_mm = 122,
};

static inline struct rm68200 *panel_to_rm68200(struct drm_panel *panel)
{
	return container_of(panel, struct rm68200, panel);
}

static void rm68200_dcs_write_buf(struct rm68200 *ctx, const void *data,
				  size_t len)
{
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);

	if (mipi_dsi_dcs_write_buffer(dsi, data, len) < 0)
		DRM_WARN("mipi dsi dcs write buffer failed\n");
}

static void rm68200_dcs_write_cmd(struct rm68200 *ctx, u8 cmd, u8 value)
{
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);

	if (mipi_dsi_dcs_write(dsi, cmd, &value, 1) < 0)
		DRM_WARN("mipi dsi dcs write failed\n");
}

#define dcs_write_seq(ctx, seq...)				\
({								\
	static const u8 d[] = { seq };				\
	rm68200_dcs_write_buf(ctx, d, ARRAY_SIZE(d));		\
})

/*
 * This panel is not able to auto-increment all cmd addresses so for some of
 * them, we need to send them one by one...
 */
#define dcs_write_cmd_seq(ctx, cmd, seq...)			\
({								\
	static const u8 d[] = { seq };				\
	static int i;						\
	for (i = 0; i < ARRAY_SIZE(d) ; i++)			\
		rm68200_dcs_write_cmd(ctx, cmd + i, d[i]);	\
})

static void rm68200_init_sequence(struct rm68200 *ctx)
{
	dcs_write_seq(ctx, 0xF0, 0x55, 0xAA, 0x52, 0x08, 0x00);
	dcs_write_seq(ctx, 0xC0, 0xC7, 0x00, 0x00, 0x00, 0x1E, 0x10, 0x60, 0xE5); 
	dcs_write_seq(ctx, 0xC1, 0xC0, 0x01, 0x00, 0x00, 0x1D, 0x00, 0xF0, 0xC8); 
	dcs_write_seq(ctx, 0xC2, 0xC0, 0x02, 0x00, 0x00, 0x1D, 0x2A, 0xA0, 0x9F); 
	dcs_write_seq(ctx, 0xC3, 0xC0, 0x02, 0x00, 0x00, 0x1E, 0x2A, 0xA0, 0x9F); 
	dcs_write_seq(ctx, 0xC4, 0xC0, 0x02, 0x00, 0x00, 0x1D, 0x10, 0x80, 0xB8); 
	dcs_write_seq(ctx, 0xC5, 0xC0, 0x02, 0x00, 0x00, 0x1E, 0x10, 0xA0, 0xB8); 
	dcs_write_seq(ctx, 0xC6, 0xC7, 0x00, 0x02, 0x00, 0x1E, 0x10, 0xA0, 0xEC); 
	dcs_write_seq(ctx, 0xC7, 0xC7, 0x00, 0x00, 0x00, 0x1F, 0x10, 0x60, 0xE5); 
	dcs_write_seq(ctx, 0xC8, 0xFF);
	dcs_write_seq(ctx, 0xB0, 0x00, 0x08, 0x0C, 0x14, 0x14); 
	dcs_write_seq(ctx, 0xBA, 0x20);
	dcs_write_seq(ctx, 0xBB, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55); 
	dcs_write_seq(ctx, 0xF0, 0x55, 0xAA, 0x52, 0x08, 0x02); 
	dcs_write_seq(ctx, 0xE1, 0x00); 
	dcs_write_seq(ctx, 0xCA, 0x04); 
	dcs_write_seq(ctx, 0xE2, 0x0A); 
	dcs_write_seq(ctx, 0xE3, 0x00); 
	dcs_write_seq(ctx, 0xE7, 0x00); 
	dcs_write_seq(ctx, 0xED, 0x48, 0x00, 0xE0, 0x13, 0x08, 0x00, 0x92, 0x08);
	dcs_write_seq(ctx, 0xFD, 0x00, 0x08, 0x1C, 0x00, 0x00, 0x01); 
	dcs_write_seq(ctx, 0xC3, 0x11, 0x24, 0x04, 0x0A, 0x01, 0x04, 0x00, 0x1C, 0x10, 0xF0, 0x00); 
	dcs_write_seq(ctx, 0xEA, 0x7F, 0x20, 0x00, 0x00, 0x00);  
	dcs_write_seq(ctx, 0xF0, 0x55, 0xAA, 0x52, 0x08, 0x01);
	dcs_write_seq(ctx, 0xB0, 0x01, 0x01, 0x01); 
	dcs_write_seq(ctx, 0xB1, 0x05, 0x05, 0x05); 
	dcs_write_seq(ctx, 0xB2, 0xD0, 0xD0, 0xD0); 
	dcs_write_seq(ctx, 0xB4, 0x37, 0x37, 0x37); 
	dcs_write_seq(ctx, 0xB5, 0x05, 0x05, 0x05); 
	dcs_write_seq(ctx, 0xB6, 0x54, 0x54, 0x54); 
	dcs_write_seq(ctx, 0xB7, 0x24, 0x24, 0x24); 
	dcs_write_seq(ctx, 0xB8, 0x24, 0x24, 0x24); 
	dcs_write_seq(ctx, 0xB9, 0x14, 0x14, 0x14); 
	dcs_write_seq(ctx, 0xBA, 0x14, 0x14, 0x14); 
	dcs_write_seq(ctx, 0xBC, 0x00, 0xF8, 0xB2); 
	dcs_write_seq(ctx, 0xBE, 0x23, 0x00, 0x90); 
	dcs_write_seq(ctx, 0xCA, 0x80);
	dcs_write_seq(ctx, 0xCB, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);
	dcs_write_seq(ctx, 0xCC, 0x19, 0x19, 0x19, 0x19, 0x19, 0x19, 0x19, 0x19, 0x19, 0x19, 0x19, 0x19); 
	dcs_write_seq(ctx, 0xF0, 0x55, 0xAA, 0x52, 0x08, 0x03);
	dcs_write_seq(ctx, 0xF1, 0x10, 0x00, 0x00, 0x00, 0x01, 0x30);
	dcs_write_seq(ctx, 0xF6, 0x0A);
	dcs_write_seq(ctx, 0xF0, 0x55, 0xAA, 0x52, 0x08, 0x05); 
	dcs_write_seq(ctx, 0xC0, 0x06, 0x02, 0x02, 0x22, 0x00, 0x00, 0x01); 
	dcs_write_seq(ctx, 0xF0, 0x55, 0xAA, 0x52, 0x08, 0x02);
	dcs_write_seq(ctx, 0xFE, 0x00, 0x80);
	dcs_write_seq(ctx, 0x33, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00);
	dcs_write_seq(ctx, 0x36, 0x03);
	dcs_write_seq(ctx, 0x35, 0x00);
#ifndef CONFIG_IWG27M
	/* IWG27M: MIPI-DSI: Fix for 720p display flickering issue */
	dcs_write_seq(ctx, 0x11, 0x00);
	msleep (100);
	dcs_write_seq(ctx, 0x29, 0x00);
#endif
}

static int rm68200_disable(struct drm_panel *panel)
{
	struct rm68200 *ctx = panel_to_rm68200(panel);

	if (!ctx->enabled)
		return 0;

	if (ctx->bl_dev) {
		ctx->bl_dev->props.power = FB_BLANK_POWERDOWN;
		ctx->bl_dev->props.state |= BL_CORE_FBBLANK;
		backlight_update_status(ctx->bl_dev);
	}

	ctx->enabled = false;

	return 0;
}

static int rm68200_unprepare(struct drm_panel *panel)
{
	struct rm68200 *ctx = panel_to_rm68200(panel);
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);
	int ret;

	if (!ctx->prepared)
		return 0;

	ret = mipi_dsi_dcs_set_display_off(dsi);
	if (ret)
		DRM_WARN("failed to set display off: %d\n", ret);

	ret = mipi_dsi_dcs_enter_sleep_mode(dsi);
	if (ret)
		DRM_WARN("failed to enter sleep mode: %d\n", ret);

	msleep(120);

	if (ctx->reset_gpio) {
		gpiod_set_value_cansleep(ctx->reset_gpio, 1);
		msleep(20);
	}

	regulator_disable(ctx->supply);

	ctx->prepared = false;

	return 0;
}

static int rm68200_prepare(struct drm_panel *panel)
{
	struct rm68200 *ctx = panel_to_rm68200(panel);
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);
	int ret;

	if (ctx->prepared)
		return 0;

	ret = regulator_enable(ctx->supply);
	if (ret < 0) {
		DRM_ERROR("failed to enable supply: %d\n", ret);
		return ret;
	}

	if (ctx->reset_gpio) {
#ifndef CONFIG_IWG27M
		/* IWG27M: MIPI-DSI: Fix for 720p display flickering issue */
		gpiod_set_value_cansleep(ctx->reset_gpio, 0);
#endif
		gpiod_set_value_cansleep(ctx->reset_gpio, 1);
		msleep(20);
		gpiod_set_value_cansleep(ctx->reset_gpio, 0);
#ifdef CONFIG_IWG27M
		/* IWG27M: MIPI-DSI: Fix for 720p display flickering issue */
		msleep(20);
#else
		msleep(100);
#endif
	}

	rm68200_init_sequence(ctx);

	ret = mipi_dsi_dcs_exit_sleep_mode(dsi);
	if (ret)
		return ret;

#ifdef CONFIG_IWG27M
	/* IWG27M: MIPI-DSI: Fix for 720p display flickering issue */
	msleep(100);
#else
	msleep(125);
#endif

	ret = mipi_dsi_dcs_set_display_on(dsi);
	if (ret)
		return ret;

#ifndef CONFIG_IWG27M
	/* IWG27M: MIPI-DSI: Fix for 720p display flickering issue */
	msleep(20);
#endif

	ctx->prepared = true;

	return 0;
}

static int rm68200_enable(struct drm_panel *panel)
{
	struct rm68200 *ctx = panel_to_rm68200(panel);

	if (ctx->enabled)
		return 0;

	if (ctx->bl_dev) {
		ctx->bl_dev->props.state &= ~BL_CORE_FBBLANK;
		ctx->bl_dev->props.power = FB_BLANK_UNBLANK;
		backlight_update_status(ctx->bl_dev);
	}

	ctx->enabled = true;

	return 0;
}

static int rm68200_get_modes(struct drm_panel *panel)
{
	struct drm_display_mode *mode;

	mode = drm_mode_duplicate(panel->drm, &default_mode);
	if (!mode) {
		DRM_ERROR("failed to add mode %ux%ux@%u\n",
			  default_mode.hdisplay, default_mode.vdisplay,
			  default_mode.vrefresh);
		return -ENOMEM;
	}

	drm_mode_set_name(mode);

	mode->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	drm_mode_probed_add(panel->connector, mode);

	panel->connector->display_info.width_mm = mode->width_mm;
	panel->connector->display_info.height_mm = mode->height_mm;

	return 1;
}

static int rm68200_bl_get_brightness(struct backlight_device *bl)
{
	struct mipi_dsi_device *dsi = bl_get_data(bl);
	struct rm68200 *ctx = mipi_dsi_get_drvdata(dsi);
	struct device *dev = &dsi->dev;
	u16 brightness;
	int ret;

	if (!ctx->prepared)
		return 0;

	DRM_DEV_DEBUG_DRIVER(dev, "\n");

	dsi->mode_flags &= ~MIPI_DSI_MODE_LPM;

	ret = mipi_dsi_dcs_get_display_brightness(dsi, &brightness);
	if (ret < 0)
		return ret;

	bl->props.brightness = brightness;

	return brightness & 0xff;
}

static int rm68200_bl_update_status(struct backlight_device *bl)
{
	struct mipi_dsi_device *dsi = bl_get_data(bl);
	struct rm68200 *ctx = mipi_dsi_get_drvdata(dsi);
	struct device *dev = &dsi->dev;
	int ret = 0;

	if (!ctx->prepared)
		return 0;

	DRM_DEV_DEBUG_DRIVER(dev, "New brightness: %d\n", bl->props.brightness);

	dsi->mode_flags &= ~MIPI_DSI_MODE_LPM;

	ret = mipi_dsi_dcs_set_display_brightness(dsi, bl->props.brightness);
	if (ret < 0)
		return ret;

	return 0;
}

/*IWG27M : Fix for controlling brightness using dcs commands*/
static const struct backlight_ops rm68200_bl_ops = {
	.update_status = rm68200_bl_update_status,
	.get_brightness = rm68200_bl_get_brightness,
};
static const struct drm_panel_funcs rm68200_drm_funcs = {
	.disable   = rm68200_disable,
	.unprepare = rm68200_unprepare,
	.prepare   = rm68200_prepare,
	.enable    = rm68200_enable,
	.get_modes = rm68200_get_modes,
};

static int rm68200_probe(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
#ifndef CONFIG_IWG27M
	struct device_node *backlight;
#else
	struct backlight_properties bl_props;
#endif
	struct rm68200 *ctx;
	int ret;

	ctx = devm_kzalloc(dev, sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	/* IWG27M: Adding delay time for display reset */
	msleep(100);
	ctx->reset_gpio = devm_gpiod_get_optional(dev, "reset", GPIOD_OUT_LOW);
	if (IS_ERR(ctx->reset_gpio)) {
		dev_err(dev, "cannot get reset-gpio\n");
		return PTR_ERR(ctx->reset_gpio);
	}

	ctx->supply = devm_regulator_get(dev, "power");
	if (IS_ERR(ctx->supply)) {
		dev_err(dev, "cannot get regulator\n");
		return PTR_ERR(ctx->supply);
	}

#ifndef CONFIG_IWG27M
	backlight = of_parse_phandle(dev->of_node, "backlight", 0);
	if (backlight) {
		ctx->bl_dev = of_find_backlight_by_node(backlight);
		of_node_put(backlight);

		if (!ctx->bl_dev)
			return -EPROBE_DEFER;
		}

#else
	/*IWG27M: Panel brightness controlled using DCS commands*/
	memset(&bl_props, 0, sizeof(bl_props));
	bl_props.type = BACKLIGHT_RAW;
	bl_props.brightness = 255;
	bl_props.max_brightness = 255;

	ctx->bl_dev = devm_backlight_device_register(
				dev, dev_name(dev),
				dev, dsi,
				&rm68200_bl_ops, &bl_props);
	if (!ctx->bl_dev) {
		ret = PTR_ERR(ctx->bl_dev);
		dev_err(dev, "Failed to register backlight (%d)\n", ret);
		return ret;
	}
#endif

	mipi_dsi_set_drvdata(dsi, ctx);

	ctx->dev = dev;

	dsi->lanes = 4;
	dsi->format = MIPI_DSI_FMT_RGB888;
	dsi->mode_flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_VIDEO_SYNC_PULSE |
			  MIPI_DSI_CLOCK_NON_CONTINUOUS | MIPI_DSI_MODE_LPM;

	drm_panel_init(&ctx->panel);
	ctx->panel.dev = dev;
	ctx->panel.funcs = &rm68200_drm_funcs;

	drm_panel_add(&ctx->panel);

	ret = mipi_dsi_attach(dsi);
	if (ret < 0) {
		dev_err(dev, "mipi_dsi_attach failed. Is host ready?\n");
		drm_panel_remove(&ctx->panel);
		if (ctx->bl_dev)
			put_device(&ctx->bl_dev->dev);
		return ret;
	}

	DRM_INFO(DRV_NAME "_panel %ux%u@%u %ubpp dsi %udl - ready\n",
		 default_mode.hdisplay, default_mode.vdisplay,
		 default_mode.vrefresh,
		 mipi_dsi_pixel_format_to_bpp(dsi->format), dsi->lanes);

	printk("_panel %ux%u@%u %ubpp dsi %udl - ready\n",
		 default_mode.hdisplay, default_mode.vdisplay,
		 default_mode.vrefresh,
		 mipi_dsi_pixel_format_to_bpp(dsi->format), dsi->lanes);
	return 0;
}

static int rm68200_remove(struct mipi_dsi_device *dsi)
{
	struct rm68200 *ctx = mipi_dsi_get_drvdata(dsi);

	if (ctx->bl_dev)
		put_device(&ctx->bl_dev->dev);

	mipi_dsi_detach(dsi);
	drm_panel_remove(&ctx->panel);

	return 0;
}

static const struct of_device_id raydium_rm68200_of_match[] = {
	{ .compatible = "raydium,rm68200" },
	{ }
};
MODULE_DEVICE_TABLE(of, raydium_rm68200_of_match);

static struct mipi_dsi_driver raydium_rm68200_driver = {
	.probe  = rm68200_probe,
	.remove = rm68200_remove,
	.driver = {
		.name = DRV_NAME "_panel",
		.of_match_table = raydium_rm68200_of_match,
	},
};
module_mipi_dsi_driver(raydium_rm68200_driver);

MODULE_AUTHOR("Philippe Cornu <philippe.cornu@st.com>");
MODULE_AUTHOR("Yannick Fertre <yannick.fertre@st.com>");
MODULE_DESCRIPTION("DRM Driver for Raydium RM68200 MIPI DSI panel");
MODULE_LICENSE("GPL v2");
