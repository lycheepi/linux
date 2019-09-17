/*
 * Copyright (c) 2018 iWave Systems Technologies Pvt. Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; If not, see <http://www.gnu.org/licenses/>.
 */
#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/ctype.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/of_device.h>
#include <linux/i2c.h>
#include <linux/v4l2-mediabus.h>
#include <linux/of_gpio.h>
#include <linux/pinctrl/consumer.h>
#include <linux/regulator/consumer.h>
#include <media/v4l2-subdev.h>

#include "imx230.h"

#define MAX9271_MAX_SENSOR_NUM	4
#define CAMERA_USES_24HZ

#define ADDR_IMX230_SENSOR	0x10

static unsigned int g_imx230_width = 1920;
static unsigned int g_imx230_height = 1080;

#define IMX230_SENSOR_ID	0x0230
#define IMX230_SENSOR_REG	0x0016

struct reg_value {
	unsigned short reg_addr;
	unsigned char val;
	unsigned int delay_ms;
};

enum imx230_frame_rate {
	IMX230_15_FPS,
	IMX230_30_FPS,
};

static struct reg_value imx230_init_data[] = {
	{ 0x0136, 0x18, 0 },
	{ 0x0137, 0x00, 0 },
	{ 0x4800, 0x0E, 0 },
	{ 0x4890, 0x01, 0 },
	{ 0x4D1E, 0x01, 0 },
	{ 0x4D1F, 0xFF, 0 },
	{ 0x4FA0, 0x00, 0 },
	{ 0x4FA1, 0x00, 0 },
	{ 0x4FA2, 0x00, 0 },
	{ 0x4FA3, 0x83, 0 },
	{ 0x6153, 0x01, 0 },
	{ 0x6156, 0x01, 0 },
	{ 0x69BB, 0x01, 0 },
	{ 0x69BC, 0x05, 0 },
	{ 0x69BD, 0x05, 0 },
	{ 0x69C1, 0x00, 0 },
	{ 0x69C4, 0x01, 0 },
	{ 0x69C6, 0x01, 0 },
	{ 0x7300, 0x00, 0 },
	{ 0x9009, 0x1A, 0 },
	{ 0xB040, 0x90, 0 },
	{ 0xB041, 0x14, 0 },
	{ 0xB042, 0x6B, 0 },
	{ 0xB043, 0x43, 0 },
	{ 0xB044, 0x63, 0 },
	{ 0xB045, 0x2A, 0 },
	{ 0xB046, 0x68, 0 },
	{ 0xB047, 0x06, 0 },
	{ 0xB048, 0x68, 0 },
	{ 0xB049, 0x07, 0 },
	{ 0xB04A, 0x68, 0 },
	{ 0xB04B, 0x04, 0 },
	{ 0xB04C, 0x68, 0 },
	{ 0xB04D, 0x05, 0 },
	{ 0xB04E, 0x68, 0 },
	{ 0xB04F, 0x16, 0 },
	{ 0xB050, 0x68, 0 },
	{ 0xB051, 0x17, 0 },
	{ 0xB052, 0x68, 0 },
	{ 0xB053, 0x74, 0 },
	{ 0xB054, 0x68, 0 },
	{ 0xB055, 0x75, 0 },
	{ 0xB056, 0x68, 0 },
	{ 0xB057, 0x76, 0 },
	{ 0xB058, 0x68, 0 },
	{ 0xB059, 0x77, 0 },
	{ 0xB05A, 0x68, 0 },
	{ 0xB05B, 0x7A, 0 },
	{ 0xB05C, 0x68, 0 },
	{ 0xB05D, 0x7B, 0 },
	{ 0xB05E, 0x68, 0 },
	{ 0xB05F, 0x0A, 0 },
	{ 0xB060, 0x68, 0 },
	{ 0xB061, 0x0B, 0 },
	{ 0xB062, 0x68, 0 },
	{ 0xB063, 0x08, 0 },
	{ 0xB064, 0x68, 0 },
	{ 0xB065, 0x09, 0 },
	{ 0xB066, 0x68, 0 },
	{ 0xB067, 0x0E, 0 },
	{ 0xB068, 0x68, 0 },
	{ 0xB069, 0x0F, 0 },
	{ 0xB06A, 0x68, 0 },
	{ 0xB06B, 0x0C, 0 },
	{ 0xB06C, 0x68, 0 },
	{ 0xB06D, 0x0D, 0 },
	{ 0xB06E, 0x68, 0 },
	{ 0xB06F, 0x13, 0 },
	{ 0xB070, 0x68, 0 },
	{ 0xB071, 0x12, 0 },
	{ 0xB072, 0x90, 0 },
	{ 0xB073, 0x0E, 0 },
	{ 0xD000, 0xDA, 0 },
	{ 0xD001, 0xDA, 0 },
	{ 0xD002, 0x7B, 0 },
	{ 0xD003, 0x00, 0 },
	{ 0xD004, 0x55, 0 },
	{ 0xD005, 0x34, 0 },
	{ 0xD006, 0x21, 0 },
	{ 0xD007, 0x00, 0 },
	{ 0xD008, 0x1C, 0 },
	{ 0xD009, 0x80, 0 },
	{ 0xD00A, 0xFE, 0 },
	{ 0xD00B, 0xC5, 0 },
	{ 0xD00C, 0x55, 0 },
	{ 0xD00D, 0xDC, 0 },
	{ 0xD00E, 0xB6, 0 },
	{ 0xD00F, 0x00, 0 },
	{ 0xD010, 0x31, 0 },
	{ 0xD011, 0x02, 0 },
	{ 0xD012, 0x4A, 0 },
	{ 0xD013, 0x0E, 0 },
	{ 0x5869, 0x01, 0 },
	{ 0x0114, 0x03, 0 },
	{ 0x0220, 0x00, 0 },
	{ 0x0221, 0x11, 0 },
	{ 0x0222, 0x01, 0 },
	{ 0x0340, 0x06, 0 },
	{ 0x0341, 0x28, 0 },
	{ 0x0342, 0x17, 0 },
	{ 0x0343, 0x88, 0 },
	{ 0x0344, 0x00, 0 },
	{ 0x0345, 0x00, 0 },
	{ 0x0346, 0x02, 0 },
	{ 0x0347, 0x0C, 0 },
	{ 0x0348, 0x14, 0 },
	{ 0x0349, 0xDF, 0 },
	{ 0x034A, 0x0D, 0 },
	{ 0x034B, 0xA7, 0 },
	{ 0x0381, 0x01, 0 },
	{ 0x0383, 0x01, 0 },
	{ 0x0385, 0x01, 0 },
	{ 0x0387, 0x01, 0 },
	{ 0x0900, 0x01, 0 },
	{ 0x0901, 0x22, 0 },
	{ 0x0902, 0x00, 0 },
	{ 0x3000, 0x74, 0 },
	{ 0x3001, 0x00, 0 },
	{ 0x305C, 0x11, 0 },
/*	{ 0x0112, 0x0A, 0 },
	{ 0x0113, 0x0A, 0 }, */
	{ 0x0112, 0x0A, 0 },
	{ 0x0113, 0x0A, 0 },
	{ 0x034C, 0x07, 0 },
	{ 0x034D, 0x80, 0 },
	{ 0x034E, 0x04, 0 },
	{ 0x034F, 0x38, 0 },
	{ 0x0401, 0x02, 0 },
	{ 0x0404, 0x00, 0 },
	{ 0x0405, 0x16, 0 },
	{ 0x0408, 0x00, 0 },
	{ 0x0409, 0x10, 0 },
	{ 0x040A, 0x00, 0 },
	{ 0x040B, 0x00, 0 },
	{ 0x040C, 0x0A, 0 },
	{ 0x040D, 0x52, 0 },
	{ 0x040E, 0x05, 0 },
	{ 0x040F, 0xCE, 0 },
	{ 0x0301, 0x04, 0 },
	{ 0x0303, 0x02, 0 },
	{ 0x0305, 0x04, 0 },
	{ 0x0306, 0x00, 0 },
	{ 0x0307, 0x5F, 0 },
	{ 0x0309, 0x0A, 0 },
	{ 0x030B, 0x01, 0 },
	{ 0x030D, 0x0C, 0 },
	{ 0x030E, 0x01, 0 },
	{ 0x030F, 0x90, 0 },
	{ 0x0310, 0x01, 0 },
/*	{ 0x0820, 0x0C, 0 },
	{ 0x0821, 0x80, 0 }, */
	{ 0x0820, 0x0C, 0 },
	{ 0x0821, 0x80, 0 },
	{ 0x0822, 0x00, 0 },
	{ 0x0823, 0x00, 0 },
	{ 0x0202, 0x06, 0 },
	{ 0x0203, 0x1E, 0 },
	{ 0x0224, 0x01, 0 },
	{ 0x0225, 0xF4, 0 },
	{ 0x0204, 0x00, 0 },
	{ 0x0205, 0x00, 0 },
	{ 0x0216, 0x00, 0 },
	{ 0x0217, 0x00, 0 },
	{ 0x020E, 0x01, 0 },
	{ 0x020F, 0x00, 0 },
	{ 0x0210, 0x01, 0 },
	{ 0x0211, 0x00, 0 },
	{ 0x0212, 0x01, 0 },
	{ 0x0213, 0x00, 0 },
	{ 0x0214, 0x01, 0 },
	{ 0x0215, 0x00, 0 },
	{ 0x3006, 0x01, 0 },
	{ 0x3007, 0x02, 0 },
	{ 0x31E0, 0x03, 0 },
	{ 0x31E1, 0xFF, 0 },
	{ 0x31E4, 0x02, 0 },
	{ 0x3A22, 0x20, 0 },
	{ 0x3A23, 0x14, 0 },
	{ 0x3A24, 0xE0, 0 },
	{ 0x3A25, 0x05, 0 },
	{ 0x3A26, 0xCE, 0 },
	{ 0x3A2F, 0x00, 0 },
	{ 0x3A30, 0x00, 0 },
	{ 0x3A31, 0x02, 0 },
	{ 0x3A32, 0x0C, 0 },
	{ 0x3A33, 0x14, 0 },
	{ 0x3A34, 0xDF, 0 },
	{ 0x3A35, 0x0D, 0 },
	{ 0x3A36, 0xA7, 0 },
	{ 0x3A37, 0x00, 0 },
	{ 0x3A38, 0x01, 0 },
	{ 0x3A39, 0x00, 0 },
	{ 0x3A21, 0x00, 0 },
	{ 0x3011, 0x00, 0 },
	{ 0x3013, 0x01, 0 },
	{ 0x080A, 0x00, 0 }, 
	{ 0x080B, 0xA7, 0 },
	{ 0x080C, 0x00, 0 },
	{ 0x080D, 0x6F, 0 },
	{ 0x080E, 0x00, 0 },
	{ 0x080F, 0x9F, 0 },
	{ 0x0810, 0x00, 0 },
	{ 0x0811, 0x5F, 0 },
	{ 0x0812, 0x00, 0 },
	{ 0x0813, 0x5F, 0 },
	{ 0x0814, 0x00, 0 },
	{ 0x0815, 0x6F, 0 },
	{ 0x0816, 0x01, 0 },
	{ 0x0817, 0x7F, 0 },
	{ 0x0818, 0x00, 0 },
	{ 0x0819, 0x4F, 0 },
};

static inline struct sensor_data *subdev_to_sensor_data(struct v4l2_subdev *sd)
{
	return container_of(sd, struct sensor_data, subdev);
}

static inline int imx230_read_reg(struct sensor_data *imx230_data, int index,
					unsigned short reg, unsigned char *val)
{
	unsigned char u8_buf[2] = { 0 };
	unsigned int buf_len = 2;
	int retry, timeout = 10;
	unsigned char u8_val = 0;

	u8_buf[0] = (reg >> 8) & 0xFF;
	u8_buf[1] = reg & 0xFF;

	imx230_data->i2c_client->addr = ADDR_IMX230_SENSOR;

	for (retry = 0; retry < timeout; retry++) {
		if (i2c_master_send(imx230_data->i2c_client, u8_buf, buf_len) < 0) {
			dev_dbg(&imx230_data->i2c_client->dev,
				"%s:read reg error on send: reg=0x%x, retry = %d.\n", __func__, reg, retry);
			msleep(5);
			continue;
		}
		if (i2c_master_recv(imx230_data->i2c_client, &u8_val, 1) != 1) {
			dev_dbg(&imx230_data->i2c_client->dev,
				"%s:read reg error on recv: reg=0x%x, retry = %d.\n", __func__, reg, retry);
			msleep(5);
			continue;
		}
		break;
	}

	if (retry >= timeout) {
		dev_info(&imx230_data->i2c_client->dev,
			"%s:read reg error: reg=0x%x.\n", __func__, reg);
		return -1;
	}

	*val = u8_val;

	return u8_val;
}

static inline int imx230_write_reg(struct sensor_data *imx230_data, int index,
					unsigned short reg, unsigned char val)
{
	unsigned char u8_buf[3] = { 0 };
	unsigned int buf_len = 3;
	int retry, timeout = 10;

	u8_buf[0] = (reg >> 8) & 0xFF;
	u8_buf[1] = reg & 0xFF;
	u8_buf[2] = val;

	imx230_data->i2c_client->addr = ADDR_IMX230_SENSOR;
	for (retry = 0; retry < timeout; retry++) {
		if (i2c_master_send(imx230_data->i2c_client, u8_buf, buf_len) < 0) {
			dev_dbg(&imx230_data->i2c_client->dev,
				"%s:write reg error: reg=0x%x, val=0x%x, retry = %d.\n", __func__, reg, val, retry);
			msleep(5);
			continue;
		}
		break;
	}

	if (retry >= timeout) {
		dev_info(&imx230_data->i2c_client->dev,
			"%s:write reg error: reg=0x%x, val=0x%x.\n", __func__, reg, val);
		return -1;
	}

	return 0;
}

static int imx230_check_device(struct sensor_data *imx230_data, int index)
{
	unsigned char reg = 0;

	imx230_read_reg(imx230_data, index, 0x0016, &reg);
	if (reg != 0x02) {
		dev_err(&imx230_data->i2c_client->dev,
			"%s: IMX230 hasn't been found, reg[0x%x] = 0x%x., index=%d\n",
			__func__, IMX230_SENSOR_REG, reg, index);
		return -1;
	}
	imx230_read_reg(imx230_data, index, 0x0017, &reg);
	if (reg != 0x30) {
		dev_err(&imx230_data->i2c_client->dev,
			"%s: IMX230 hasn't been found, reg[0x%x] = 0x%x.\n", __func__, IMX230_SENSOR_REG+1, reg);
		return -1;
	}
	dev_info(&imx230_data->i2c_client->dev, "%s: IMX230 was found.\n", __func__);

	return 0;
}

static int imx230_initialize(struct sensor_data *imx230_data, int index)
{
	int i, array_size;
	int retval;

	dev_info(&imx230_data->i2c_client->dev, "%s: index = %d.\n", __func__, index);
	array_size = ARRAY_SIZE(imx230_init_data);
	for (i = 0; i < array_size; i++) {
		retval = imx230_write_reg(imx230_data, index,
					imx230_init_data[i].reg_addr, imx230_init_data[i].val);
		if (retval < 0)
			break;
		if (imx230_init_data[i].delay_ms != 0)
			msleep(imx230_init_data[i].delay_ms);
	}

	return 0;
}


#ifdef debug
static void imx230_dump_registers(struct sensor_data *imx230_data)
{
}
#else
static void imx230_dump_registers(struct sensor_data *imx230_data, int index)
{
        unsigned char i, reg;
        printk("Dump IMX230 registers:\r\n");
        for (i = 0; i < 0x87; i++)
                printk("IMX230 Reg 0x%02x = 0x%x.\r\n", i, imx230_read_reg(imx230_data, index, i, &reg));
}
#endif

static int imx230_g_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *a)
{
	struct v4l2_captureparm *cparm = &a->parm.capture;
	struct sensor_data *imx230_data = subdev_to_sensor_data(sd);
	int ret = 0;

	switch (a->type) {
	/* This is the only case currently handled. */
	case V4L2_BUF_TYPE_VIDEO_CAPTURE:
	case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
		memset(a, 0, sizeof(*a));
		a->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		cparm->capability = imx230_data->streamcap.capability;
		cparm->timeperframe = imx230_data->streamcap.timeperframe;
		cparm->capturemode = imx230_data->streamcap.capturemode;
		ret = 0;
		break;

	/* These are all the possible cases. */
	case V4L2_BUF_TYPE_VIDEO_OUTPUT:
	case V4L2_BUF_TYPE_VIDEO_OVERLAY:
	case V4L2_BUF_TYPE_VBI_CAPTURE:
	case V4L2_BUF_TYPE_VBI_OUTPUT:
	case V4L2_BUF_TYPE_SLICED_VBI_CAPTURE:
	case V4L2_BUF_TYPE_SLICED_VBI_OUTPUT:
		ret = -EINVAL;
		break;
	default:
		pr_debug("   type is unknown - %d\n", a->type);
		ret = -EINVAL;
		break;
	}

	return ret;
}

/*!
 * ioctl_s_parm - V4L2 sensor interface handler for VIDIOC_S_PARM ioctl
 * @s: pointer to standard V4L2 device structure
 * @a: pointer to standard V4L2 VIDIOC_S_PARM ioctl structure
 *
 * Configures the sensor to use the input parameters, if possible.  If
 * not possible, reverts to the old parameters and returns the
 * appropriate error code.
 */
static int imx230_s_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *a)
{
	struct sensor_data *imx230_data = subdev_to_sensor_data(sd);
	struct v4l2_fract *timeperframe = &a->parm.capture.timeperframe;
	enum imx230_frame_rate frame_rate;
	u32 tgt_fps;
	int ret = 0;

	switch (a->type) {
	/* This is the only case currently handled. */
	case V4L2_BUF_TYPE_VIDEO_CAPTURE:
	case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
		/* Check that the new frame rate is allowed. */
		if ((timeperframe->numerator == 0) ||
		    (timeperframe->denominator == 0)) {
			timeperframe->denominator = DEFAULT_FPS;
			timeperframe->numerator = 1;
		}

		tgt_fps = timeperframe->denominator /
			  timeperframe->numerator;

		if (tgt_fps > MAX_FPS) {
			timeperframe->denominator = MAX_FPS;
			timeperframe->numerator = 1;
		} else if (tgt_fps < MIN_FPS) {
			timeperframe->denominator = MIN_FPS;
			timeperframe->numerator = 1;
		}

		/* Actual frame rate we use */
		tgt_fps = timeperframe->denominator /
			  timeperframe->numerator;

		if (tgt_fps == 15)
			frame_rate = IMX230_15_FPS;
		else if (tgt_fps == 30)
			frame_rate = IMX230_30_FPS;
		else {
			pr_err(" The camera frame rate is not supported!\n");
			return -EINVAL;
		}

		 /* TODO Reserved to extension */

		imx230_data->streamcap.timeperframe = *timeperframe;
		imx230_data->streamcap.capturemode = a->parm.capture.capturemode;


		break;

	/* These are all the possible cases. */
	case V4L2_BUF_TYPE_VIDEO_OUTPUT:
	case V4L2_BUF_TYPE_VIDEO_OVERLAY:
	case V4L2_BUF_TYPE_VBI_CAPTURE:
	case V4L2_BUF_TYPE_VBI_OUTPUT:
	case V4L2_BUF_TYPE_SLICED_VBI_CAPTURE:
	case V4L2_BUF_TYPE_SLICED_VBI_OUTPUT:
		pr_debug("   type is not "\
				 "V4L2_BUF_TYPE_VIDEO_CAPTURE but %d\n",
			a->type);
		ret = -EINVAL;
		break;

	default:
		pr_debug("   type is unknown - %d\n", a->type);
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int imx230_enum_mbus_code(struct v4l2_subdev *sd,
				struct v4l2_subdev_pad_config *cfg,
				struct v4l2_subdev_mbus_code_enum *code)
{
	struct sensor_data *imx230_data = subdev_to_sensor_data(sd);

	code->code = imx230_data->format.code;

	return 0;
}

/*!
 * imx230_enum_framesizes - V4L2 sensor interface handler for
 *			   VIDIOC_ENUM_FRAMESIZES ioctl
 * @s: pointer to standard V4L2 device structure
 * @fsize: standard V4L2 VIDIOC_ENUM_FRAMESIZES ioctl structure
 *
 * Return 0 if successful, otherwise -EINVAL.
 */
static int imx230_enum_framesizes(struct v4l2_subdev *sd,
			       struct v4l2_subdev_pad_config *cfg,
			       struct v4l2_subdev_frame_size_enum *fse)
{
	struct sensor_data *imx230_data = subdev_to_sensor_data(sd);

	if (fse->index > 1)
		return -EINVAL;

	fse->max_width = imx230_data->format.width;
	fse->min_width = fse->max_width;

	fse->max_height = imx230_data->format.height;
	fse->min_height = fse->max_height;

	return 0;
}
static int imx230_enum_frame_interval(struct v4l2_subdev *sd,
				   struct v4l2_subdev_pad_config *cfg,
				   struct v4l2_subdev_frame_interval_enum *fie)
{
	if (fie->index < 0 || fie->index > 9)
		return -EINVAL;

	if (fie->width == 0 || fie->height == 0 ||
	    fie->code == 0) {
		pr_warning("Please assign pixel format, width and height.\n");
		return -EINVAL;
	}

	fie->interval.numerator = 1;

	 /* TODO Reserved to extension */

	fie->interval.denominator = 30;
	return 0;
}

static int imx230_get_fmt(struct v4l2_subdev *sd,
			   struct v4l2_subdev_pad_config *cfg,
			   struct v4l2_subdev_format *fmt)
{
	struct sensor_data *imx230_data = subdev_to_sensor_data(sd);
	struct v4l2_mbus_framefmt *mf = &fmt->format;

	if (fmt->pad)
		return -EINVAL;

	mf->code = imx230_data->format.code;
	mf->width =  imx230_data->format.width;
	mf->height = imx230_data->format.height;
	mf->colorspace = imx230_data->format.colorspace;
	mf->field = imx230_data->format.field;

	return 0;
}

static int imx230_set_fmt(struct v4l2_subdev *sd,
			 struct v4l2_subdev_pad_config *cfg,
			 struct v4l2_subdev_format *fmt)
{
	return 0;
}

static int imx230_get_frame_desc(struct v4l2_subdev *sd, unsigned int pad,
				  struct v4l2_mbus_frame_desc *fd)
{
	return 0;
}

static int imx230_set_frame_desc(struct v4l2_subdev *sd,
					unsigned int pad,
					struct v4l2_mbus_frame_desc *fd)
{
	return 0;
}

static int imx230_set_power(struct v4l2_subdev *sd, int on)
{
	return 0;
}

static int imx230_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct sensor_data *imx230_data = subdev_to_sensor_data(sd);

	dev_dbg(sd->dev, "%s\n", __func__);
	if (enable) {
		if (!imx230_data->running) {
			/* Enable CSI output, set virtual channel according to the link number */
			imx230_write_reg(imx230_data, 0, 0x0100, 0x01);
		}
		imx230_data->running++;

	} else {

		if (imx230_data->running) {
			/* Disable CSI Output */
			imx230_write_reg(imx230_data, 0, 0x0100, 0x00);
		}
		imx230_data->running--;
	}

	return 0;
}

static int imx230_link_setup(struct media_entity *entity,
			   const struct media_pad *local,
			   const struct media_pad *remote, u32 flags)
{
	return 0;
}

static const struct v4l2_subdev_pad_ops imx230_pad_ops = {
	.enum_mbus_code		= imx230_enum_mbus_code,
	.enum_frame_size	= imx230_enum_framesizes,
	.enum_frame_interval	= imx230_enum_frame_interval,
	.get_fmt		= imx230_get_fmt,
	.set_fmt		= imx230_set_fmt,
	.get_frame_desc		= imx230_get_frame_desc,
	.set_frame_desc		= imx230_set_frame_desc,
};

static const struct v4l2_subdev_core_ops imx230_core_ops = {
	.s_power	= imx230_set_power,
};

static const struct v4l2_subdev_video_ops imx230_video_ops = {
	.s_parm =	imx230_s_parm,
	.g_parm =	imx230_g_parm,
	.s_stream		= imx230_s_stream,
};

static const struct v4l2_subdev_ops imx230_subdev_ops = {
	.core	= &imx230_core_ops,
	.pad	= &imx230_pad_ops,
	.video	= &imx230_video_ops,
};

static const struct media_entity_operations imx230_sd_media_ops = {
	.link_setup = imx230_link_setup,
};

/*!
 * imx230 I2C probe function
 *
 * @param adapter            struct i2c_adapter *
 * @return  Error code indicating success or failure
 */
static int imx230_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	struct device *dev = &client->dev;
	struct sensor_data *imx230_data;
	struct v4l2_subdev *sd;
	int retval;

	imx230_data = devm_kzalloc(dev, sizeof(*imx230_data), GFP_KERNEL);
	if (!imx230_data)
		return -ENOMEM;

	/* Set initial values for the sensor struct. */
	imx230_data->sensor_clk = devm_clk_get(dev, "capture_mclk");
	if (IS_ERR(imx230_data->sensor_clk)) {
		/* assuming clock enabled by default */
		imx230_data->sensor_clk = NULL;
		dev_err(dev, "clock-frequency missing or invalid\n");
		return PTR_ERR(imx230_data->sensor_clk);
	}

	retval = of_property_read_u32(dev->of_node, "mclk",
		&(imx230_data->mclk));
	if (retval) {
		dev_err(dev, "mclk missing or invalid\n");
		return retval;
	}

	retval = of_property_read_u32(dev->of_node, "mclk_source",
		(u32 *)&(imx230_data->mclk_source));
	if (retval) {
		dev_err(dev, "mclk_source missing or invalid\n");
		return retval;
	}

	/* request power down pin */
	imx230_data->pwn_gpio = of_get_named_gpio(dev->of_node, "pwn-gpios", 0);
	if (!gpio_is_valid(imx230_data->pwn_gpio)) {
		dev_err(dev, "no sensor pwdn pin available\n");
		return -ENODEV;
	}
	retval = devm_gpio_request_one(dev, imx230_data->pwn_gpio, GPIOF_OUT_INIT_HIGH,
					"imx230_pwd");
	if (retval < 0)
		return retval;

	clk_prepare_enable(imx230_data->sensor_clk);

	imx230_data->i2c_client = client;
//	imx230_data->format.code = MEDIA_BUS_FMT_SBGGR8_1X8;
//	imx230_data->format.code = MEDIA_BUS_FMT_SBGGR10_2X8_PADHI_LE;
	imx230_data->format.code = MEDIA_BUS_FMT_SRGGB10_1X10;
//	imx230_data->format.code = MEDIA_BUS_FMT_SRGGB8_1X8;
//	imx230_data->format.code = MEDIA_BUS_FMT_YUYV8_1X16;
	imx230_data->format.width = g_imx230_width;
	imx230_data->format.height = g_imx230_height;
//	imx230_data->format.colorspace = V4L2_COLORSPACE_JPEG;
//	imx230_data->format.colorspace = V4L2_COLORSPACE_SRGB;
	imx230_data->format.colorspace = V4L2_COLORSPACE_RAW;
	/*****************************************
	 * Pass mipi phy clock rate Mbps
	 * fcsi2 = PCLk * WIDTH * CHANNELS / LANES
	 * fsci2 = 80MPCLK * 10 bit * 4 channels / 4 lanes
	 ****************************************/
//	imx230_data->format.reserved[0] = 80 * 10;
	imx230_data->format.reserved[0] = 80 * 40;
	imx230_data->format.field = V4L2_FIELD_NONE;
	imx230_data->streamcap.capturemode = 0;
	imx230_data->streamcap.timeperframe.denominator = 30;
	imx230_data->streamcap.timeperframe.numerator = 1;
	imx230_data->is_mipi = 1;

	retval = imx230_check_device(imx230_data, 1);
	if (retval < 0) {
		pr_warning("imx230 is not found, chip id = 0x%x.\n", retval);
		clk_disable_unprepare(imx230_data->sensor_clk);
		devm_gpio_free(dev, imx230_data->pwn_gpio);
		return -ENODEV;
	}

	imx230_initialize(imx230_data, 0);
	
	imx230_data->streamcap.capability = V4L2_CAP_TIMEPERFRAME;
	imx230_data->streamcap.timeperframe.denominator = 30;
	imx230_data->streamcap.timeperframe.numerator = 1;
	imx230_data->v_channel = 0;
	imx230_data->cap_mode.clip_top = 0;
	imx230_data->cap_mode.clip_left = 0;

	imx230_data->cap_mode.clip_height = 1080;
	imx230_data->cap_mode.clip_width = 1920;

	imx230_data->cap_mode.hlen = imx230_data->cap_mode.clip_width;

	imx230_data->cap_mode.hfp = 0;
	imx230_data->cap_mode.hbp = 0;
	imx230_data->cap_mode.hsync = 0;
	imx230_data->cap_mode.vlen = 1080;
	imx230_data->cap_mode.vfp = 0;
	imx230_data->cap_mode.vbp = 0;
	imx230_data->cap_mode.vsync = 0;
	imx230_data->cap_mode.vlen1 = 0;
	imx230_data->cap_mode.vfp1 = 0;
	imx230_data->cap_mode.vbp1 = 0;
	imx230_data->cap_mode.vsync1 = 0;
	imx230_data->cap_mode.pixelclock = 24000000;

	sd = &imx230_data->subdev;
	v4l2_i2c_subdev_init(sd, client, &imx230_subdev_ops);
	sd->flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;

	sd->entity.function = MEDIA_ENT_F_CAM_SENSOR;
	imx230_data->pads[MIPI_CSI2_SENS_VC0_PAD_SOURCE].flags = MEDIA_PAD_FL_SOURCE;
	imx230_data->pads[MIPI_CSI2_SENS_VC1_PAD_SOURCE].flags = MEDIA_PAD_FL_SOURCE;
	imx230_data->pads[MIPI_CSI2_SENS_VC2_PAD_SOURCE].flags = MEDIA_PAD_FL_SOURCE;
	imx230_data->pads[MIPI_CSI2_SENS_VC3_PAD_SOURCE].flags = MEDIA_PAD_FL_SOURCE;
	retval = media_entity_pads_init(&sd->entity, MIPI_CSI2_SENS_VCX_PADS_NUM,
							imx230_data->pads);
	if (retval < 0)
		return retval;

	imx230_data->subdev.entity.ops = &imx230_sd_media_ops;
	retval = v4l2_async_register_subdev(&imx230_data->subdev);
	if (retval < 0) {
		dev_err(&client->dev,
					"%s--Async register failed, ret=%d\n", __func__, retval);
		media_entity_cleanup(&sd->entity);
	}

	imx230_data->running = 0;

        /* Disable CSI Output */
        imx230_write_reg(imx230_data, 0, 0x0100, 0x00);

	return retval;
}

/*!
 * imx230 I2C detach function
 *
 * @param client            struct i2c_client *
 * @return  Error code indicating success or failure
 */
static int imx230_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct sensor_data *imx230_data = subdev_to_sensor_data(sd);

	clk_disable_unprepare(imx230_data->sensor_clk);
	media_entity_cleanup(&sd->entity);
	v4l2_async_unregister_subdev(sd);

	return 0;
}
static const struct i2c_device_id imx230_id[] = {
	{},
};

MODULE_DEVICE_TABLE(i2c, imx230_id);

static const struct of_device_id imx230_of_match[] = {
	{ .compatible = "sony,imx230" },
	{ /* sentinel */ }
};

static struct i2c_driver imx230_i2c_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name   = "imx230",
		.of_match_table	= of_match_ptr(imx230_of_match),
	},
	.probe  = imx230_probe,
	.remove = imx230_remove,
	.id_table = imx230_id,
};

module_i2c_driver(imx230_i2c_driver);

MODULE_AUTHOR("iWave Systems Technologies Pvt. Ltd.");
MODULE_DESCRIPTION("IMX230 Camera Sensor Driver");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.0");
MODULE_ALIAS("CSI");
