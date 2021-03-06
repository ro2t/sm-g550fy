/*
 * Samsung Exynos5 SoC series Flash driver
 *
 *
 * Copyright (c) 2014 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/i2c.h>
#include <linux/gpio.h>
#include <plat/gpio-cfg.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/platform_device.h>
#include <linux/of_gpio.h>

#include <linux/videodev2.h>
#include <linux/videodev2_exynos_camera.h>

#include "fimc-is-device-flash-gpio.h"
#include "fimc-is-device-sensor.h"
#include "fimc-is-device-sensor-peri.h"
#include "fimc-is-core.h"

extern bool assistive_light;

static int flash_gpio_init(struct v4l2_subdev *subdev, u32 val)
{
	int ret = 0;
	struct fimc_is_flash *flash;

	BUG_ON(!subdev);

	flash = (struct fimc_is_flash *)v4l2_get_subdevdata(subdev);

	BUG_ON(!flash);

	/* TODO: init flash driver */
	memset(&flash->flash_data, 0, sizeof(struct fimc_is_flash_data));

	return ret;
}

int control_flash_gpio(u32 gpio, int val) {
	int ret = 0;

	__gpio_set_value(gpio, val);

	return ret;
}

static int sensor_gpio_flash_control(struct v4l2_subdev *subdev, enum flash_mode mode, u32 intensity)
{
	int ret = 0;
	struct fimc_is_flash *flash = NULL;

	BUG_ON(!subdev);

	flash = (struct fimc_is_flash *)v4l2_get_subdevdata(subdev);
	BUG_ON(!flash);

	pr_info("%s : mode (%d)", __func__, mode);

	if (assistive_light == true && (mode == CAM2_FLASH_MODE_OFF)) {
		pr_info("%s : Don't Flash OFF.. assistive_light turn on \n", __func__);
		return 0;
	}

	if (mode == CAM2_FLASH_MODE_OFF) {
		ret = control_flash_gpio(flash->flash_gpio, 0);
		if (ret)
			err("capture flash off fail");
		ret = control_flash_gpio(flash->torch_gpio, 0);
		if (ret)
			err("torch flash off fail");
	} else if (mode == CAM2_FLASH_MODE_SINGLE) {
		ret = control_flash_gpio(flash->flash_gpio, intensity);
		if (ret)
			err("capture flash on fail");
	} else if (mode == CAM2_FLASH_MODE_TORCH) {
		ret = control_flash_gpio(flash->torch_gpio, intensity);
		if (ret)
			err("torch flash on fail");
	} else {
		err("Invalid flash mode");
		ret = -EINVAL;
		goto p_err;
	}

p_err:
	return ret;
}

int flash_gpio_s_ctrl(struct v4l2_subdev *subdev, struct v4l2_control *ctrl)
{
	int ret = 0;
	struct fimc_is_flash *flash = NULL;

	BUG_ON(!subdev);

	flash = (struct fimc_is_flash *)v4l2_get_subdevdata(subdev);
	BUG_ON(!flash);

	switch(ctrl->id) {
	case V4L2_CID_FLASH_SET_INTENSITY:
		/* TODO : Check min/max intensity */
		if (ctrl->value < 0) {
			err("failed to flash set intensity: %d\n", ctrl->value);
			ret = -EINVAL;
			goto p_err;
		}
		flash->flash_data.intensity = ctrl->value;
		break;
	case V4L2_CID_FLASH_SET_FIRING_TIME:
		/* TODO : Check min/max firing time */
		if (ctrl->value < 0) {
			err("failed to flash set firing time: %d\n", ctrl->value);
			ret = -EINVAL;
			goto p_err;
		}
		flash->flash_data.firing_time_us = ctrl->value;
		break;
	case V4L2_CID_FLASH_SET_FIRE:
		ret =  sensor_gpio_flash_control(subdev, flash->flash_data.mode, ctrl->value);
		if (ret) {
			err("sensor_gpio_flash_control(mode:%d, val:%d) is fail(%d)",
					(int)flash->flash_data.mode, ctrl->value, ret);
			goto p_err;
		}
		break;
	default:
		err("err!!! Unknown CID(%#x)", ctrl->id);
		ret = -EINVAL;
		goto p_err;
	}

p_err:
	return ret;
}

static const struct v4l2_subdev_core_ops core_ops = {
	.init = flash_gpio_init,
	.s_ctrl = flash_gpio_s_ctrl,
};

static const struct v4l2_subdev_ops subdev_ops = {
	.core = &core_ops,
};

int flash_gpio_probe(struct device *dev, struct i2c_client *client)
{
	int ret = 0;
	struct fimc_is_core *core;
	struct v4l2_subdev *subdev_flash;
	struct fimc_is_device_sensor *device;
	struct fimc_is_device_sensor_peri *sensor_peri;
	struct fimc_is_flash *flash;
	u32 sensor_id = 0;
	struct device_node *dnode;

	BUG_ON(!fimc_is_dev);
	BUG_ON(!dev);

	dnode = dev->of_node;

	ret = of_property_read_u32(dnode, "id", &sensor_id);
	if (ret) {
		err("id read is fail(%d)", ret);
		goto p_err;
	}

	core = (struct fimc_is_core *)dev_get_drvdata(fimc_is_dev);
	if (!core) {
		probe_err("core device is not yet probed");
		ret = -EPROBE_DEFER;
		goto p_err;
	}

	device = &core->sensor[sensor_id];
	if (!device) {
		err("sensor device is NULL");
		ret = -ENOMEM;
		goto p_err;
	}

	sensor_peri = find_peri_by_flash_id(device, FLADRV_NAME_DRV_FLASH_GPIO);
	if (!sensor_peri) {
		err("sensor peri is NULL");
		ret = -ENOMEM;
		goto p_err;
	}

	flash = &sensor_peri->flash;
	if (!flash) {
		err("flash is NULL");
		ret = -ENOMEM;
		goto p_err;
	}

	subdev_flash = kzalloc(sizeof(struct v4l2_subdev), GFP_KERNEL);
	if (!subdev_flash) {
		err("subdev_flash is NULL");
		ret = -ENOMEM;
		goto p_err;
	}
	sensor_peri->subdev_flash = subdev_flash;

	flash = &sensor_peri->flash;
	flash->id = FLADRV_NAME_DRV_FLASH_GPIO;
	flash->subdev = subdev_flash;
	flash->client = client;

	flash->flash_gpio = of_get_named_gpio(dnode, "flash-gpio", 0);
	if (!gpio_is_valid(flash->flash_gpio)) {
		dev_err(dev, "failed to get PIN_RESET\n");
		return -EINVAL;
	}

	flash->torch_gpio = of_get_named_gpio(dnode, "torch-gpio", 0);
	if (!gpio_is_valid(flash->torch_gpio)) {
		dev_err(dev, "failed to get PIN_RESET\n");
		return -EINVAL;
	}

	flash->flash_data.mode = CAM2_FLASH_MODE_OFF;
	flash->flash_data.intensity = 100; /* TODO: Need to figure out min/max range */
	flash->flash_data.firing_time_us = 1 * 1000 * 1000; /* Max firing time is 1sec */

	if (client)
		v4l2_i2c_subdev_init(subdev_flash, client, &subdev_ops);
	else
		v4l2_subdev_init(subdev_flash, &subdev_ops);

	v4l2_set_subdevdata(subdev_flash, flash);
	v4l2_set_subdev_hostdata(subdev_flash, device);
	snprintf(subdev_flash->name, V4L2_SUBDEV_NAME_SIZE, "flash-subdev.%d", flash->id);

p_err:
	return ret;
}

int flash_gpio_platform_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct device *dev;

	BUG_ON(!pdev);

	dev = &pdev->dev;

	ret = flash_gpio_probe(dev, NULL);
	if (ret < 0) {
		probe_err("flash gpio probe fail(%d)\n", ret);
		goto p_err;
	}

	probe_info("%s done\n", __func__);

p_err:
	return ret;
}

static int flash_gpio_platform_remove(struct platform_device *pdev)
{
        int ret = 0;

        info("%s\n", __func__);

        return ret;
}

static const struct of_device_id exynos_fimc_is_sensor_flash_gpio_match[] = {
	{
		.compatible = "samsung,sensor-flash-gpio",
	},
	{},
};
MODULE_DEVICE_TABLE(of, exynos_fimc_is_sensor_flash_gpio_match);

/* register platform driver */
static struct platform_driver sensor_flash_gpio_platform_driver = {
	.probe  = flash_gpio_platform_probe,
	.remove = flash_gpio_platform_remove,
	.driver = {
		.name   = "FIMC-IS-SENSOR-FLASH-GPIO-PLATFORM",
		.owner  = THIS_MODULE,
		.of_match_table = exynos_fimc_is_sensor_flash_gpio_match,
	}
};
module_platform_driver(sensor_flash_gpio_platform_driver);
