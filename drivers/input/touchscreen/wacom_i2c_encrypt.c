/*
 * Wacom Penabled Driver for I2C
 *
 * Copyright (c) 2011 - 2013 Tatsunosuke Tobita, Wacom.
 * <tobita.tatsunosuke@wacom.co.jp>
 *
 * This program is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General
 * Public License as published by the Free Software
 * Foundation; either version of 2 of the License,
 * or (at your option) any later version.
 */

#include <linux/module.h>
#include <linux/input.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <asm/unaligned.h>
#include <linux/delay.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/fcntl.h>
#include <linux/input.h>
#include <asm/uaccess.h>

#define WACOM_CMD_QUERY0	0x04
#define WACOM_CMD_QUERY1	0x00
#define WACOM_CMD_QUERY2	0x33
#define WACOM_CMD_QUERY3	0x02
#define WACOM_CMD_THROW0	0x05
#define WACOM_CMD_THROW1	0x00
#define WACOM_QUERY_SIZE	32

int irq_pin;
int rst_pin;
int ctl_pin;
#define SCR_X 1280
#define SCR_Y 720
#define MAX_PRESSURE  4095
struct wacom_features {
	int x_max;
	int y_max;
	int pressure_max;
	char fw_version;
};

struct wacom_i2c {
	struct i2c_client *client;
	struct input_dev *input;
	u8 data[WACOM_QUERY_SIZE];
	bool prox;
	int tool;
};

struct wacom_i2c *gwac_i2c;
#define WACOM_RST                                0x4800
static long wacom_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	printk("%s cmd = %d arg = %ld\n",__FUNCTION__,cmd, arg);

	switch (cmd){
		case WACOM_RST:
			printk("wacom rst!\n");
			disable_irq(gwac_i2c->client->irq);
			gpio_set_value(rst_pin,0);
        		mdelay(100);
        		gpio_set_value(ctl_pin,1); // close power
			mdelay(500);
			//resume power
		        gpio_set_value(ctl_pin,0);
        		mdelay(100);

        		gpio_set_value(rst_pin,0);
        		mdelay(500);
        		gpio_set_value(rst_pin,1);
        		mdelay(100);
			enable_irq(gwac_i2c->client->irq);
			break;
		default:
			break;
	}
	return 0;
}

static struct file_operations wacom_control_fops = {
	.owner   = THIS_MODULE,
	.unlocked_ioctl   = wacom_ioctl,
};

static struct miscdevice wacom_control_misc =
{
    .minor = MISC_DYNAMIC_MINOR,
    .name = "wacom_control",
    .fops = &wacom_control_fops,
};


int y_max1 = 0;
int x_max1 = 0;
//static volatile int isPenDetected = 0;
unsigned int isPenDetected = 0;
static int wacom_query_device(struct i2c_client *client,
			      struct wacom_features *features)
{
	//Wint ret;
			int i = 100;
	u8 cmd1[] = { WACOM_CMD_QUERY0, WACOM_CMD_QUERY1,
			WACOM_CMD_QUERY2, WACOM_CMD_QUERY3 };
	u8 cmd2[] = { WACOM_CMD_THROW0, WACOM_CMD_THROW1 };
	//u8 data[WACOM_QUERY_SIZE];
	u8 data[WACOM_QUERY_SIZE]={0x15,0x00,0x03,0xa8,0x2b,0x60,0x1a,0x0c,0x18,0xc8,0x2a,0xff,0x0f,0x03,0x03,0xff,0xff,0x28,0x23,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0};
	/* struct i2c_msg msgs[] = {
		{
			.addr = client->addr,
			.flags = 0,
			.len = sizeof(cmd1),
			.buf = cmd1,
			.scl_rate = 200*1000,
		},
		{
			.addr = client->addr,
			.flags = 0,
			.len = sizeof(cmd2),
			.buf = cmd2,
			.scl_rate = 200*1000,
		},
		{
			.addr = client->addr,
			.flags = I2C_M_RD,
			.len = sizeof(data),
			.buf = data,
		},
	};

	
		int ret = i2c_transfer(client->adapter, &msgs[0], 1);
	if (ret < 0){
		printk("1--------shy wacom%d----hpy---%s!!!!  addr:%02x\n",__LINE__,__FUNCTION__,client->addr);
		return ret;
	}
	
			ret = i2c_transfer(client->adapter, &msgs[1], 1);
	if (ret < 0){
		printk("2--------shy wacom%d----hpy---%s!!!!  addr:%02x\n",__LINE__,__FUNCTION__,client->addr);
		return ret;
	}

	ret = i2c_transfer(client->adapter, &msgs[2], 1);
	if (ret < 0){
		printk("3--------shy wacom%d----hpy---%s!!!!  addr:%02x\n",__LINE__,__FUNCTION__,client->addr);
		return ret;
	}*/

	features->y_max = get_unaligned_le16(&data[3]);
	features->x_max = get_unaligned_le16(&data[5]);
	x_max1 = features->x_max;
	y_max1 = features->y_max;
	features->pressure_max = get_unaligned_le16(&data[11]);
	features->fw_version = get_unaligned_le16(&data[13]);

	dev_dbg(&client->dev,
		"x_max:%d, y_max:%d, pressure:%d, fw:%d\n",
		features->x_max, features->y_max,
		features->pressure_max, features->fw_version);
	printk("x_max:%d, y_max:%d, pressure:%d, fw:%d\n",
		features->x_max, features->y_max,
		features->pressure_max, features->fw_version);

	return 0;
}

static irqreturn_t wacom_i2c_irq(int irq, void *dev_id)
{
	struct wacom_i2c *wac_i2c = dev_id;
	struct input_dev *input = wac_i2c->input;
	u8 *data = wac_i2c->data;
	unsigned int x, y, pressure;
	unsigned char tsw, f1, f2, ers;
	int error,i = 0;

	printk("--------shy wacom1-------!!!!\n");
	error = i2c_master_recv(wac_i2c->client,
				wac_i2c->data, sizeof(wac_i2c->data));
	if (error < 0)
		goto out;

	
	/*printk("*********wacom i2c data ");
	for(i = 0;i < 10;i++){
		printk("%x ",data[i]);
	}
	printk("\n");*/
	

	tsw = data[3] & 0x01;
	ers = data[3] & 0x04;
	f1 = data[3] & 0x02;
	f2 = data[3] & 0x10;
	x = le16_to_cpup((__le16 *)&data[4]);
	x = x + x/10;
	y = le16_to_cpup((__le16 *)&data[6]);
    y = y + 130;
	/*if (0x1300 > y)
  {
			y = y - (0x1300 - y)*12/100;	
	}else if(y > 0x1300){
			y = y + (y - 0x1300)*12/100;
	}*/
	pressure = le16_to_cpup((__le16 *)&data[8]);

	x = (x * SCR_X)/x_max1;
	y = (y * SCR_Y)/y_max1;


	if(pressure > 0)
		isPenDetected = 1;
	else
		isPenDetected = 0;

	if(pressure > 4096)
		goto out;

	if(x < 0 || x > 6771 || y < 0|| y > 11012)
		goto out;

	if (!wac_i2c->prox)
		wac_i2c->tool = (data[3] & 0x0c) ?
			BTN_TOOL_RUBBER : BTN_TOOL_PEN;

	wac_i2c->prox = data[3] & 0x20;

	input_report_key(input, BTN_TOUCH, tsw || ers);
	input_report_key(input, wac_i2c->tool, wac_i2c->prox);
	input_report_key(input, BTN_STYLUS, f1);
	input_report_key(input, BTN_STYLUS2, f2);
	//input_report_abs(input, ABS_X, y);
	//input_report_abs(input, ABS_Y, x);
	input_report_abs(input, ABS_X, x); 
	input_report_abs(input, ABS_Y, y); 
	
	input_report_abs(input, ABS_PRESSURE, pressure);
	input_sync(input);

out:
	return IRQ_HANDLED;
}

static int wacom_i2c_open(struct input_dev *dev)
{
	struct wacom_i2c *wac_i2c = input_get_drvdata(dev);
	struct i2c_client *client = wac_i2c->client;

	printk("--------shy wacom2-------!!!!\n");
	enable_irq(client->irq);

	return 0;
}

static void wacom_i2c_close(struct input_dev *dev)
{
	struct wacom_i2c *wac_i2c = input_get_drvdata(dev);
	struct i2c_client *client = wac_i2c->client;

	disable_irq(client->irq);
}

static int wacom_i2c_probe(struct i2c_client *client,
				     const struct i2c_device_id *id)
{
	struct wacom_i2c *wac_i2c;
	struct input_dev *input;
	struct wacom_features features = { 0 };
	int error;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		dev_err(&client->dev, "i2c_check_functionality error\n");
		return -EIO;
	}
	error = wacom_query_device(client, &features);
	if (error)
		return error;
	printk("--------shy wacom%d----hpy---%s!!!!\n",__LINE__,__FUNCTION__);
	wac_i2c = kzalloc(sizeof(*wac_i2c), GFP_KERNEL);
	input = input_allocate_device();
	if (!wac_i2c || !input) {
		error = -ENOMEM;
		goto err_free_mem;
	}

	struct device_node *np = client->dev.of_node;
	unsigned int irq_flags;
	wac_i2c->client = client;
	wac_i2c->input = input;

	input->name = "wacom_i2c";
	input->id.bustype = BUS_I2C;
	input->id.vendor = 0x56a;
	input->id.version = features.fw_version;
	input->dev.parent = &client->dev;
	input->open = wacom_i2c_open;
	input->close = wacom_i2c_close;

	input->evbit[0] |= BIT_MASK(EV_KEY) | BIT_MASK(EV_ABS);

	__set_bit(BTN_TOOL_PEN, input->keybit);
	__set_bit(BTN_TOOL_RUBBER, input->keybit);
	__set_bit(BTN_STYLUS, input->keybit);
	__set_bit(BTN_STYLUS2, input->keybit);
	__set_bit(BTN_TOUCH, input->keybit);
	
	__set_bit(INPUT_PROP_DIRECT, input->propbit);
	input_set_abs_params(input, ABS_X, 0, SCR_X, 0, 0);
	input_set_abs_params(input, ABS_Y, 0, SCR_Y, 0, 0);
        input_set_abs_params(input, ABS_PRESSURE,
                             0, MAX_PRESSURE, 0, 0);

	input_set_drvdata(input, wac_i2c);

	irq_pin = of_get_named_gpio_flags(np, "irq_gpios", 0, (enum of_gpio_flags *)&irq_flags);
	rst_pin = of_get_named_gpio_flags(np, "reset_gpios", 0, (enum of_gpio_flags *)&irq_flags);
	ctl_pin = of_get_named_gpio_flags(np, "ctl_gpios", 0, (enum of_gpio_flags *)&irq_flags);

        gpio_request(ctl_pin,"wacom_ctl");
        gpio_direction_output(ctl_pin,0);
    	gpio_set_value(ctl_pin,0);
        mdelay(100);

	gpio_request(rst_pin,"wacom_rst");
    	gpio_direction_output(rst_pin,0);
	gpio_set_value(rst_pin,0);
	mdelay(500);	
	gpio_set_value(rst_pin,1);
	mdelay(100);
	printk("##########leison wacom###rst low 500 high 100 ###### %s,%d\n",__func__,__LINE__);
	//msleep(100);
	printk("wacom irq_gpio is :%d\n",irq_pin);
	gpio_request(irq_pin,"wacom_irq");
	gpio_direction_input(irq_pin);
	client->irq = gpio_to_irq(irq_pin);

	error = request_threaded_irq(client->irq, NULL, wacom_i2c_irq,
				     IRQF_TRIGGER_LOW | IRQF_ONESHOT,
				     "wacom_i2c", wac_i2c);
	if (error) {
		dev_err(&client->dev,
			"Failed to enable IRQ, error: %d\n", error);
		goto err_free_mem;
	}

	/* Disable the IRQ, we'll enable it in wac_i2c_open() */
	disable_irq(client->irq);

	error = input_register_device(wac_i2c->input);
	if (error) {
		dev_err(&client->dev,
			"Failed to register input device, error: %d\n", error);
		goto err_free_irq;
	}

	i2c_set_clientdata(client, wac_i2c);
	gwac_i2c = wac_i2c;
	return 0;

err_free_irq:
	free_irq(client->irq, wac_i2c);
err_free_mem:
	input_free_device(input);
	kfree(wac_i2c);

	return error;
}

static int wacom_i2c_remove(struct i2c_client *client)
{
	struct wacom_i2c *wac_i2c = i2c_get_clientdata(client);

	free_irq(client->irq, wac_i2c);
	input_unregister_device(wac_i2c->input);
	kfree(wac_i2c);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int wacom_i2c_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);

	disable_irq(client->irq);
	
	gpio_set_value(rst_pin,0);
	mdelay(100);
	gpio_set_value(ctl_pin,1); // close power

	return 0;
}

static int wacom_i2c_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);

	//resume power
        gpio_set_value(ctl_pin,0);
        mdelay(100);

        gpio_set_value(rst_pin,0);
        mdelay(500);
        gpio_set_value(rst_pin,1);
        mdelay(100);

	enable_irq(client->irq);

	return 0;
}
#endif

static SIMPLE_DEV_PM_OPS(wacom_i2c_pm, wacom_i2c_suspend, wacom_i2c_resume);

static const struct i2c_device_id wacom_i2c_id[] = {
	{ "wacom_i2c", 0 },
	{ },
};
MODULE_DEVICE_TABLE(i2c, wacom_i2c_id);

static void wacom_i2c_shutdown(struct i2c_client *client)
{
	printk("wacom i2c shutdown\n");
	gpio_set_value(ctl_pin,1); 
	gpio_set_value(rst_pin,0); 	
}

static struct i2c_driver wacom_i2c_driver = {
	.driver	= {
		.name	= "wacom_i2c",
		.owner	= THIS_MODULE,
		.pm	= &wacom_i2c_pm,
	},
	.probe		= wacom_i2c_probe,
	.remove		= wacom_i2c_remove,
	.id_table	= wacom_i2c_id,
 	//.shutdown       = wacom_i2c_shutdown,
};

static void wacom_i2c_exit(void)
{
	printk(KERN_INFO "wacom driver exit.\n");
	i2c_del_driver(&wacom_i2c_driver);
}

static int wacom_i2c_init(void)
{
	printk(KERN_INFO "wacom chip initializing ....\n");
	int ret;
	ret = misc_register(&wacom_control_misc);
	return i2c_add_driver(&wacom_i2c_driver);
}

late_initcall(wacom_i2c_init);
module_exit(wacom_i2c_exit);
//module_i2c_driver(wacom_i2c_driver);

MODULE_AUTHOR("Tatsunosuke Tobita <tobita.tatsunosuke@wacom.co.jp>");
MODULE_DESCRIPTION("WACOM EMR I2C Driver");
MODULE_LICENSE("GPL");
