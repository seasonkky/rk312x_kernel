/* drivers/input/ts/ts-i2c.c - touchscreen i2c handle
 *
 * Copyright (C) 2012-2015 ROCKCHIP.
 * Author: luowei <lw@rock-chips.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/miscdevice.h>
#include <linux/gpio.h>
#include <asm/uaccess.h>
#include <asm/atomic.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/freezer.h>
#include <mach/gpio.h>
#include <mach/board.h> 
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif
#include <linux/ts-auto.h>

#define TS_I2C_RATE 200*1000

#if 0
#define TS_DEBUG_ENABLE
#define DBG(x...) printk(x)
#else
#define DBG(x...)
#endif

static int ts_i2c_write(struct i2c_adapter *i2c_adap,
			    unsigned char address,
			    unsigned int len, unsigned char const *data)
{
	struct i2c_msg msgs[1];
	int res;

	if (!data || !i2c_adap) {
		printk("%s:line=%d,error\n",__func__,__LINE__);
		return -EINVAL;
	}

	msgs[0].addr = address;
	msgs[0].flags = 0;	/* write */
	msgs[0].buf = (unsigned char *)data;
	msgs[0].len = len;
	msgs[0].scl_rate = TS_I2C_RATE;

	res = i2c_transfer(i2c_adap, msgs, 1);
	if (res == 1)
		return 0;
	else if(res == 0)
		return -EBUSY;
	else
		return res;

}

static int senosr_i2c_read(struct i2c_adapter *i2c_adap,
			   unsigned char address, unsigned char reg,
			   unsigned int len, unsigned char *data)
{
	struct i2c_msg msgs[2];
	int res;

	if (!data || !i2c_adap) {
		printk("%s:line=%d,error\n",__func__,__LINE__);
		return -EINVAL;
	}

	msgs[0].addr = address;
	msgs[0].flags = 0;	/* write */
	msgs[0].buf = &reg;
	msgs[0].len = 1;
	msgs[0].scl_rate = TS_I2C_RATE;
	
	msgs[1].addr = address;
	msgs[1].flags = I2C_M_RD;
	msgs[1].buf = data;
	msgs[1].len = len;
	msgs[1].scl_rate = TS_I2C_RATE;	

	res = i2c_transfer(i2c_adap, msgs, 2);
	if (res == 2)
		return 0;
	else if(res == 0)
		return -EBUSY;
	else
		return res;

}


int ts_rx_data(struct i2c_client *client, char *rxData, int length)
{
#ifdef TS_DEBUG_ENABLE
	struct ts_private_data* ts = 
		(struct ts_private_data *)i2c_get_clientdata(client);
	int i = 0;
#endif
	int ret = 0;
	char reg = rxData[0];
	ret = senosr_i2c_read(client->adapter, client->addr, reg, length, rxData);
	
#ifdef TS_DEBUG_ENABLE
	DBG("addr=0x%x,len=%d,rxdata:",reg,length);
	for(i=0; i<length; i++)
		DBG("0x%x,",rxData[i]);
	DBG("\n");
#endif	
	return ret;
}
EXPORT_SYMBOL(ts_rx_data);

int ts_tx_data(struct i2c_client *client, char *txData, int length)
{
#ifdef TS_DEBUG_ENABLE	
	struct ts_private_data* ts = 
		(struct ts_private_data *)i2c_get_clientdata(client);
	int i = 0;
#endif
	int ret = 0;
#ifdef TS_DEBUG_ENABLE	
	DBG("addr=0x%x,len=%d,txdata:",txData[0],length);
	for(i=1; i<length; i++)
		DBG("0x%x,",txData[i]);
	DBG("\n");
#endif
	ret = ts_i2c_write(client->adapter, client->addr, length, txData);
	return ret;

}
EXPORT_SYMBOL(ts_tx_data);

int ts_write_reg(struct i2c_client *client, int addr, int value)
{
	char buffer[2];
	int ret = 0;
	struct ts_private_data* ts = 
		(struct ts_private_data *)i2c_get_clientdata(client);
	
	mutex_lock(&ts->i2c_mutex);	
	buffer[0] = addr;
	buffer[1] = value;
	ret = ts_tx_data(client, &buffer[0], 2);	
	mutex_unlock(&ts->i2c_mutex);	
	return ret;
}
EXPORT_SYMBOL(ts_write_reg);

int ts_read_reg(struct i2c_client *client, int addr)
{
	char tmp[1] = {0};
	int ret = 0;	
	struct ts_private_data* ts = 
		(struct ts_private_data *)i2c_get_clientdata(client);
	
	mutex_lock(&ts->i2c_mutex);	
	tmp[0] = addr;
	ret = ts_rx_data(client, tmp, 1);
	mutex_unlock(&ts->i2c_mutex);
	
	return tmp[0];
}

EXPORT_SYMBOL(ts_read_reg);


int ts_tx_data_normal(struct i2c_client *client, char *buf, int num)
{
	int ret = 0;
	ret = i2c_master_normal_send(client, buf, num, TS_I2C_RATE);
	
	return (ret == num) ? 0 : ret;
}
EXPORT_SYMBOL(ts_tx_data_normal);


int ts_rx_data_normal(struct i2c_client *client, char *buf, int num)
{
	int ret = 0;
	ret = i2c_master_normal_recv(client, buf, num, TS_I2C_RATE);
	
	return (ret == num) ? 0 : ret;
}

EXPORT_SYMBOL(ts_rx_data_normal);


int ts_write_reg_normal(struct i2c_client *client, char value)
{
	char buffer[2];
	int ret = 0;
	struct ts_private_data* ts = 
		(struct ts_private_data *)i2c_get_clientdata(client);
	
	mutex_lock(&ts->i2c_mutex);	
	buffer[0] = value;
	ret = ts_tx_data_normal(client, &buffer[0], 1);	
	mutex_unlock(&ts->i2c_mutex);	
	return ret;
}
EXPORT_SYMBOL(ts_write_reg_normal);

int ts_read_reg_normal(struct i2c_client *client)
{
	char tmp[1] = {0};
	int ret = 0;	
	struct ts_private_data* ts = 
		(struct ts_private_data *)i2c_get_clientdata(client);
	
	mutex_lock(&ts->i2c_mutex);	
	ret = ts_rx_data_normal(client, tmp, 1);
	mutex_unlock(&ts->i2c_mutex);
	
	return tmp[0];
}

EXPORT_SYMBOL(ts_read_reg_normal);
