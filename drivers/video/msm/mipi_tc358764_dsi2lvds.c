/* Copyright (c) 2011-2012, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */




#define DRV_NAME "mipi_tc358764"

#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/pwm.h>
#include <linux/gpio.h>
#include "msm_fb.h"
#include "mdp4.h"
#include "mipi_dsi.h"
#include "mipi_tc358764_dsi2lvds.h"


#define D0W_DPHYCONTTX	0x0004	
#define CLW_DPHYCONTRX	0x0020	
#define D0W_DPHYCONTRX	0x0024	
#define D1W_DPHYCONTRX	0x0028	
#define D2W_DPHYCONTRX	0x002C	
#define D3W_DPHYCONTRX	0x0030	
#define COM_DPHYCONTRX	0x0038	
#define CLW_CNTRL	0x0040	
#define D0W_CNTRL	0x0044	
#define D1W_CNTRL	0x0048	
#define D2W_CNTRL	0x004C	
#define D3W_CNTRL	0x0050	
#define DFTMODE_CNTRL	0x0054	

#define PPI_STARTPPI	0x0104	
#define PPI_BUSYPPI	0x0108
#define PPI_LINEINITCNT	0x0110	
#define PPI_LPTXTIMECNT	0x0114
#define PPI_LANEENABLE	0x0134	
#define PPI_TX_RX_TA	0x013C	

#define PPI_CLS_ATMR	0x0140	
#define PPI_D0S_ATMR	0x0144	
#define PPI_D1S_ATMR	0x0148	
#define PPI_D2S_ATMR	0x014C	
#define PPI_D3S_ATMR	0x0150	
#define PPI_D0S_CLRSIPOCOUNT	0x0164

#define PPI_D1S_CLRSIPOCOUNT	0x0168	
#define PPI_D2S_CLRSIPOCOUNT	0x016C	
#define PPI_D3S_CLRSIPOCOUNT	0x0170	

#define CLS_PRE		0x0180	
#define D0S_PRE		0x0184	
#define D1S_PRE		0x0188	
#define D2S_PRE		0x018C	
#define D3S_PRE		0x0190	
#define CLS_PREP	0x01A0	
#define D0S_PREP	0x01A4	
#define D1S_PREP	0x01A8	
#define D2S_PREP	0x01AC	
#define D3S_PREP	0x01B0	
#define CLS_ZERO	0x01C0	
#define D0S_ZERO	0x01C4	
#define D1S_ZERO	0x01C8	
#define D2S_ZERO	0x01CC	
#define D3S_ZERO	0x01D0	

#define PPI_CLRFLG	0x01E0	
#define PPI_CLRSIPO	0x01E4	
#define HSTIMEOUT	0x01F0	
#define HSTIMEOUTENABLE	0x01F4	
#define DSI_STARTDSI	0x0204	
#define DSI_BUSYDSI	0x0208
#define DSI_LANEENABLE	0x0210	
#define DSI_LANESTATUS0	0x0214	
#define DSI_LANESTATUS1	0x0218	

#define DSI_INTSTATUS	0x0220	
#define DSI_INTMASK	0x0224	
#define DSI_INTCLR	0x0228	
#define DSI_LPTXTO	0x0230	

#define DSIERRCNT	0x0300	
#define APLCTRL		0x0400	
#define RDPKTLN		0x0404	
#define VPCTRL		0x0450	
#define HTIM1		0x0454	
#define HTIM2		0x0458	
#define VTIM1		0x045C	
#define VTIM2		0x0460	
#define VFUEN		0x0464	

#define LVMX0003	0x0480	
#define LVMX0407	0x0484	
#define LVMX0811	0x0488	
#define LVMX1215	0x048C	
#define LVMX1619	0x0490	
#define LVMX2023	0x0494	
#define LVMX2427	0x0498	

#define LVCFG		0x049C	
#define LVPHY0		0x04A0	
#define LVPHY1		0x04A4	
#define SYSSTAT		0x0500	
#define SYSRST		0x0504	

#define GPIOC		0x0520	
#define GPIOO		0x0524	
#define GPIOI		0x0528	

#define I2CTIMCTRL	0x0540	
#define I2CMADDR	0x0544	
#define WDATAQ		0x0548	
#define RDATAQ		0x054C	

#define IDREG		0x0580

#define TC358764XBG_ID	0x00006500

#define DEBUG00		0x05A0	
#define DEBUG01		0x05A4	

static u32 d2l_pwm_freq_hz = (3.921*1000);

#define PWM_FREQ_HZ	(d2l_pwm_freq_hz)
#define PWM_PERIOD_USEC (USEC_PER_SEC / PWM_FREQ_HZ)
#define PWM_DUTY_LEVEL (PWM_PERIOD_USEC / PWM_LEVEL)

#define CMD_DELAY 100
#define DSI_MAX_LANES 4
#define KHZ 1000
#define MHZ (1000*1000)

struct wr_cmd_payload {
	u16 addr;
	u32 data;
} __packed;

static struct msm_panel_common_pdata *d2l_common_pdata;
struct msm_fb_data_type *d2l_mfd;
static struct dsi_buf d2l_tx_buf;
static struct dsi_buf d2l_rx_buf;
static int led_pwm;
static struct pwm_device *bl_pwm;
static struct pwm_device *tn_pwm;
static int bl_level;
static u32 d2l_gpio_out_mask;
static u32 d2l_gpio_out_val;
static u32 d2l_3d_gpio_enable;
static u32 d2l_3d_gpio_mode;
static int d2l_enable_3d;
static struct i2c_client *d2l_i2c_client;
static struct i2c_driver d2l_i2c_slave_driver;

static int mipi_d2l_init(void);
static int mipi_d2l_enable_3d(struct msm_fb_data_type *mfd,
			      bool enable, bool mode);
static u32 d2l_i2c_read_reg(struct i2c_client *client, u16 reg);
static u32 d2l_i2c_write_reg(struct i2c_client *client, u16 reg, u32 val);

static u32 mipi_d2l_read_reg(struct msm_fb_data_type *mfd, u16 reg)
{
	u32 data;
	int len = 4;
	struct dsi_cmd_desc cmd_read_reg = {
		DTYPE_GEN_READ2, 1, 0, 1, 0, 
			sizeof(reg), (char *) &reg};

	mipi_dsi_buf_init(&d2l_tx_buf);
	mipi_dsi_buf_init(&d2l_rx_buf);

	
	len = mipi_dsi_cmds_rx(mfd, &d2l_tx_buf, &d2l_rx_buf,
			       &cmd_read_reg, len);

	data = *(u32 *)d2l_rx_buf.data;

	if (len != 4)
		pr_err("%s: invalid rlen=%d, expecting 4.\n", __func__, len);

	pr_debug("%s: reg=0x%x.data=0x%08x.\n", __func__, reg, data);

	return data;
}

static int mipi_d2l_write_reg(struct msm_fb_data_type *mfd, u16 reg, u32 data)
{
	struct wr_cmd_payload payload;
	struct dsi_cmd_desc cmd_write_reg = {
		DTYPE_GEN_LWRITE, 1, 0, 0, 0,
			sizeof(payload), (char *)&payload};

	payload.addr = reg;
	payload.data = data;

	
	mipi_dsi_cmds_tx(&d2l_tx_buf, &cmd_write_reg, 1);

	pr_debug("%s: reg=0x%x. data=0x%x.\n", __func__, reg, data);

	return 0;
}

static void mipi_d2l_read_status(struct msm_fb_data_type *mfd)
{
	mipi_d2l_read_reg(mfd, DSI_LANESTATUS0);	
	mipi_d2l_read_reg(mfd, DSI_LANESTATUS1);	
	mipi_d2l_read_reg(mfd, DSI_INTSTATUS);		
	mipi_d2l_read_reg(mfd, SYSSTAT);		
}

static void mipi_d2l_read_status_via_i2c(struct i2c_client *client)
{
	u32 tmp = 0;

	tmp = d2l_i2c_read_reg(client, DSIERRCNT);
	d2l_i2c_write_reg(client, DSIERRCNT, 0xFFFF0000);

	d2l_i2c_read_reg(client, DSI_LANESTATUS0);	
	d2l_i2c_read_reg(client, DSI_LANESTATUS1);	
	d2l_i2c_read_reg(client, DSI_INTSTATUS);	
	d2l_i2c_read_reg(client, SYSSTAT);		

	d2l_i2c_write_reg(client, DSIERRCNT, tmp);
}
static int mipi_d2l_dsi_init_sequence(struct msm_fb_data_type *mfd)
{
	struct mipi_panel_info *mipi = &mfd->panel_info.mipi;
	u32 lanes_enable;
	u32 vpctrl;
	u32 htime1;
	u32 vtime1;
	u32 htime2;
	u32 vtime2;
	u32 ppi_tx_rx_ta; 
	u32 lvcfg;
	u32 hbpr;	
	u32 hpw;	
	u32 vbpr;	
	u32 vpw;	

	u32 hfpr;	
	u32 hsize;	
	u32 vfpr;	
	u32 vsize;	
	bool vesa_rgb888 = false;

	lanes_enable = 0x01; 
	lanes_enable |= (mipi->data_lane0 << 1);
	lanes_enable |= (mipi->data_lane1 << 2);
	lanes_enable |= (mipi->data_lane2 << 3);
	lanes_enable |= (mipi->data_lane3 << 4);

	if (mipi->traffic_mode == DSI_NON_BURST_SYNCH_EVENT)
		vpctrl = 0x01000120;
	else if (mipi->traffic_mode == DSI_NON_BURST_SYNCH_PULSE)
		vpctrl = 0x01000100;
	else {
		pr_err("%s.unsupported traffic_mode %d.\n",
		       __func__, mipi->traffic_mode);
		return -EINVAL;
	}

	if (mfd->panel_info.clk_rate > 800*1000*1000) {
		pr_err("%s.unsupported clk_rate %d.\n",
		       __func__, mfd->panel_info.clk_rate);
		return -EINVAL;
	}

	pr_debug("%s.xres=%d.yres=%d.fps=%d.dst_format=%d.\n",
		__func__,
		 mfd->panel_info.xres,
		 mfd->panel_info.yres,
		 mfd->panel_info.mipi.frame_rate,
		 mfd->panel_info.mipi.dst_format);

	hbpr = mfd->panel_info.lcdc.h_back_porch;
	hpw	= mfd->panel_info.lcdc.h_pulse_width;
	vbpr = mfd->panel_info.lcdc.v_back_porch;
	vpw	= mfd->panel_info.lcdc.v_pulse_width;

	htime1 = (hbpr << 16) + hpw;
	vtime1 = (vbpr << 16) + vpw;

	hfpr = mfd->panel_info.lcdc.h_front_porch;
	hsize = mfd->panel_info.xres;
	vfpr = mfd->panel_info.lcdc.v_front_porch;
	vsize = mfd->panel_info.yres;

	htime2 = (hfpr << 16) + hsize;
	vtime2 = (vfpr << 16) + vsize;

	lvcfg = 0x0003; 
	vpctrl = 0x01000120; 
	ppi_tx_rx_ta = 0x00040004;

	if (mfd->panel_info.xres == 1366) {
		ppi_tx_rx_ta = 0x00040004;
		lvcfg = 0x01; 
		vesa_rgb888 = true;
	}

	if (mfd->panel_info.xres == 1200) {
		lvcfg = 0x0103; 
		vesa_rgb888 = true;
	}

	pr_debug("%s.htime1=0x%x.\n", __func__, htime1);
	pr_debug("%s.vtime1=0x%x.\n", __func__, vtime1);
	pr_debug("%s.vpctrl=0x%x.\n", __func__, vpctrl);
	pr_debug("%s.lvcfg=0x%x.\n", __func__, lvcfg);

	mipi_d2l_write_reg(mfd, SYSRST, 0xFF);
	msleep(30);

	if (vesa_rgb888) {
		
		mipi_d2l_write_reg(mfd, LVMX0003, 0x03020100);
		mipi_d2l_write_reg(mfd, LVMX0407, 0x08050704);
		mipi_d2l_write_reg(mfd, LVMX0811, 0x0F0E0A09);
		mipi_d2l_write_reg(mfd, LVMX1215, 0x100D0C0B);
		mipi_d2l_write_reg(mfd, LVMX1619, 0x12111716);
		mipi_d2l_write_reg(mfd, LVMX2023, 0x1B151413);
		mipi_d2l_write_reg(mfd, LVMX2427, 0x061A1918);
	}

	mipi_d2l_write_reg(mfd, PPI_TX_RX_TA, ppi_tx_rx_ta); 
	mipi_d2l_write_reg(mfd, PPI_LPTXTIMECNT, 0x00000004);
	mipi_d2l_write_reg(mfd, PPI_D0S_CLRSIPOCOUNT, 0x00000003);
	mipi_d2l_write_reg(mfd, PPI_D1S_CLRSIPOCOUNT, 0x00000003);
	mipi_d2l_write_reg(mfd, PPI_D2S_CLRSIPOCOUNT, 0x00000003);
	mipi_d2l_write_reg(mfd, PPI_D3S_CLRSIPOCOUNT, 0x00000003);
	mipi_d2l_write_reg(mfd, PPI_LANEENABLE, lanes_enable);
	mipi_d2l_write_reg(mfd, DSI_LANEENABLE, lanes_enable);
	mipi_d2l_write_reg(mfd, PPI_STARTPPI, 0x00000001);
	mipi_d2l_write_reg(mfd, DSI_STARTDSI, 0x00000001);

	mipi_d2l_write_reg(mfd, VPCTRL, vpctrl); 
	mipi_d2l_write_reg(mfd, HTIM1, htime1);
	mipi_d2l_write_reg(mfd, VTIM1, vtime1);
	mipi_d2l_write_reg(mfd, HTIM2, htime2);
	mipi_d2l_write_reg(mfd, VTIM2, vtime2);
	mipi_d2l_write_reg(mfd, VFUEN, 0x00000001);
	mipi_d2l_write_reg(mfd, LVCFG, lvcfg); 

	return 0;
}

static int mipi_d2l_set_backlight_level(struct pwm_device *pwm, int level)
{
	int ret = 0;

	pr_debug("%s: level=%d.\n", __func__, level);

	if ((pwm == NULL) || (level > PWM_LEVEL) || (level < 0)) {
		pr_err("%s.pwm=NULL.\n", __func__);
		return -EINVAL;
	}

	ret = pwm_config(pwm, PWM_DUTY_LEVEL * level, PWM_PERIOD_USEC);
	if (ret) {
		pr_err("%s: pwm_config() failed err=%d.\n", __func__, ret);
		return ret;
	}

	ret = pwm_enable(pwm);
	if (ret) {
		pr_err("%s: pwm_enable() failed err=%d\n",
		       __func__, ret);
		return ret;
	}

	return 0;
}

static int mipi_d2l_set_tn_clk(struct pwm_device *pwm, u32 usec)
{
	int ret = 0;

	pr_debug("%s: usec=%d.\n", __func__, usec);

	ret = pwm_config(pwm, usec/2 , usec);
	if (ret) {
		pr_err("%s: pwm_config() failed err=%d.\n", __func__, ret);
		return ret;
	}

	ret = pwm_enable(pwm);
	if (ret) {
		pr_err("%s: pwm_enable() failed err=%d\n",
		       __func__, ret);
		return ret;
	}

	return 0;
}

static int mipi_d2l_lcd_on(struct platform_device *pdev)
{
	int ret = 0;
	u32 chip_id;
	struct msm_fb_data_type *mfd;

	pr_info("%s.\n", __func__);

	
	msleep(30);

	mfd = platform_get_drvdata(pdev);
	d2l_mfd = mfd;

	if (!mfd)
		return -ENODEV;
	if (mfd->key != MFD_KEY)
		return -EINVAL;

	chip_id = mipi_d2l_read_reg(mfd, IDREG);


	if (chip_id != TC358764XBG_ID) {
		pr_err("%s: invalid chip_id=0x%x", __func__, chip_id);
		return -ENODEV;
	}

	ret = mipi_d2l_dsi_init_sequence(mfd);
	if (ret)
		return ret;

	mipi_d2l_write_reg(mfd, GPIOC, d2l_gpio_out_mask);
	
	mipi_d2l_write_reg(mfd, GPIOO, d2l_gpio_out_val);

	d2l_pwm_freq_hz = (3.921*1000);

	if (bl_level == 0)
		bl_level = PWM_LEVEL * 2 / 3 ; 

	
	if (bl_pwm) {
		ret = mipi_d2l_set_backlight_level(bl_pwm, bl_level);
		if (ret)
			pr_err("%s.mipi_d2l_set_backlight_level.ret=%d",
			       __func__, ret);
	}

	mipi_d2l_read_status(mfd);

	mipi_d2l_enable_3d(mfd, false, false);

	
	if (d2l_i2c_client == NULL)
		i2c_add_driver(&d2l_i2c_slave_driver);

	pr_info("%s.ret=%d.\n", __func__, ret);

	return ret;
}

static int mipi_d2l_lcd_off(struct platform_device *pdev)
{
	int ret;
	struct msm_fb_data_type *mfd;

	pr_info("%s.\n", __func__);

	mfd = platform_get_drvdata(pdev);

	if (!mfd)
		return -ENODEV;
	if (mfd->key != MFD_KEY)
		return -EINVAL;

	ret = mipi_d2l_set_backlight_level(bl_pwm, 1);

	pr_info("%s.ret=%d.\n", __func__, ret);

	return ret;
}

static void mipi_d2l_set_backlight(struct msm_fb_data_type *mfd)
{
	int level = mfd->bl_level;

	pr_debug("%s.lvl=%d.\n", __func__, level);

	mipi_d2l_set_backlight_level(bl_pwm, level);

	bl_level = level;
}

static struct msm_fb_panel_data d2l_panel_data = {
	.on = mipi_d2l_lcd_on,
	.off = mipi_d2l_lcd_off,
	.set_backlight = mipi_d2l_set_backlight,
};

static u32 d2l_i2c_read_reg(struct i2c_client *client, u16 reg)
{
	int rc;
	u32 val = 0;
	u8 buf[6];

	if (client == NULL) {
		pr_err("%s.invalid i2c client.\n", __func__);
		return -EINVAL;
	}

	buf[0] = reg >> 8;
	buf[1] = reg & 0xFF;

	rc = i2c_master_send(client, buf, sizeof(reg));
	rc = i2c_master_recv(client, buf, 4);

	if (rc >= 0) {
		val = buf[0] + (buf[1] << 8) + (buf[2] << 16) + (buf[3] << 24);
		pr_debug("%s.reg=0x%x.val=0x%x.\n", __func__, reg, val);
	} else
		pr_err("%s.fail.reg=0x%x.\n", __func__, reg);

	return val;
}

static u32 d2l_i2c_write_reg(struct i2c_client *client, u16 reg, u32 val)
{
	int rc;
	u8 buf[6];

	if (client == NULL) {
		pr_err("%s.invalid i2c client.\n", __func__);
		return -EINVAL;
	}

	buf[0] = reg >> 8;
	buf[1] = reg & 0xFF;

	buf[2] = (val >> 0) & 0xFF;
	buf[3] = (val >> 8) & 0xFF;
	buf[4] = (val >> 16) & 0xFF;
	buf[5] = (val >> 24) & 0xFF;

	rc = i2c_master_send(client, buf, sizeof(buf));

	if (rc >= 0)
		pr_debug("%s.reg=0x%x.val=0x%x.\n", __func__, reg, val);
	else
		pr_err("%s.fail.reg=0x%x.\n", __func__, reg);

	return val;
}

static int __devinit d2l_i2c_slave_probe(struct i2c_client *client,
					 const struct i2c_device_id *id)
{
	static const u32 i2c_funcs = I2C_FUNC_I2C;

	d2l_i2c_client = client;

	if (!i2c_check_functionality(client->adapter, i2c_funcs)) {
		pr_err("%s.i2c_check_functionality failed.\n", __func__);
		return -ENOSYS;
	} else {
		pr_debug("%s.i2c_check_functionality OK.\n", __func__);
	}

	d2l_i2c_read_reg(client, IDREG);

	mipi_d2l_read_status_via_i2c(d2l_i2c_client);

	return 0;
}

static __devexit int d2l_i2c_slave_remove(struct i2c_client *client)
{
	d2l_i2c_client = NULL;

	return 0;
}

static const struct i2c_device_id d2l_i2c_id[] = {
	{"tc358764-i2c", 0},
	{}
};

static struct i2c_driver d2l_i2c_slave_driver = {
	.driver = {
		.name = "tc358764-i2c",
		.owner = THIS_MODULE
	},
	.probe    = d2l_i2c_slave_probe,
	.remove   = __devexit_p(d2l_i2c_slave_remove),
	.id_table = d2l_i2c_id,
};

static int mipi_d2l_enable_3d(struct msm_fb_data_type *mfd,
			      bool enable, bool mode)
{
	u32 tn_usec = 1000000 / 66; 

	pr_debug("%s.enable=%d.mode=%d.\n", __func__, enable, mode);

	gpio_direction_output(d2l_3d_gpio_enable, enable);
	gpio_direction_output(d2l_3d_gpio_mode, mode);

	mipi_d2l_set_tn_clk(tn_pwm, tn_usec);

	return 0;
}

static ssize_t mipi_d2l_enable_3d_read(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	return snprintf ((char *)buf, sizeof (*buf), "%u\n", d2l_enable_3d);
}

static ssize_t mipi_d2l_enable_3d_write(struct device *dev,
				struct device_attribute *attr,
				const char *buf,
				size_t count)
{
	int ret = -1;
	u32 data = 0;

	if (sscanf((char *)buf, "%u", &data) != 1) {
		dev_err(dev, "%s. Invalid input.\n", __func__);
		ret = -EINVAL;
	} else {
		d2l_enable_3d = data;
		if (data == 1) 
			mipi_d2l_enable_3d(d2l_mfd, true, true);
		else if (data == 2) 
			mipi_d2l_enable_3d(d2l_mfd, true, false);
		else if (data == 0)
			mipi_d2l_enable_3d(d2l_mfd, false, false);
		else if (data == 9)
			mipi_d2l_read_status_via_i2c(d2l_i2c_client);
		else
			pr_err("%s.Invalid value=%d.\n", __func__, data);
	}

	return count;
}

static struct device_attribute mipi_d2l_3d_barrier_attributes[] = {
	__ATTR(enable_3d_barrier, 0666,
	       mipi_d2l_enable_3d_read,
	       mipi_d2l_enable_3d_write),
};

static int mipi_dsi_3d_barrier_sysfs_register(struct device *dev)
{
	int ret;

	pr_debug("%s.d2l_3d_gpio_enable=%d.\n", __func__, d2l_3d_gpio_enable);
	pr_debug("%s.d2l_3d_gpio_mode=%d.\n", __func__, d2l_3d_gpio_mode);

	ret  = device_create_file(dev, mipi_d2l_3d_barrier_attributes);
	if (ret) {
		pr_err("%s.failed to create 3D sysfs.\n", __func__);
		goto err_device_create_file;
	}

	ret = gpio_request(d2l_3d_gpio_enable, "d2l_3d_gpio_enable");
	if (ret) {
		pr_err("%s.failed to get d2l_3d_gpio_enable=%d.\n",
		       __func__, d2l_3d_gpio_enable);
		goto err_d2l_3d_gpio_enable;
	}

	ret = gpio_request(d2l_3d_gpio_mode, "d2l_3d_gpio_mode");
	if (ret) {
		pr_err("%s.failed to get d2l_3d_gpio_mode=%d.\n",
		       __func__, d2l_3d_gpio_mode);
		goto err_d2l_3d_gpio_mode;
	}

	return 0;

err_d2l_3d_gpio_mode:
	gpio_free(d2l_3d_gpio_enable);
err_d2l_3d_gpio_enable:
	device_remove_file(dev, mipi_d2l_3d_barrier_attributes);
err_device_create_file:

	return ret;
}

static int __devinit mipi_d2l_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct msm_panel_info *pinfo = NULL;

	pr_debug("%s.id=%d.\n", __func__, pdev->id);

	if (pdev->id == 0) {
		d2l_common_pdata = pdev->dev.platform_data;

		if (d2l_common_pdata == NULL) {
			pr_err("%s: no PWM gpio specified.\n", __func__);
			return 0;
		}

		led_pwm = d2l_common_pdata->gpio_num[0];
		d2l_gpio_out_mask = d2l_common_pdata->gpio_num[1] >> 8;
		d2l_gpio_out_val = d2l_common_pdata->gpio_num[1] & 0xFF;
		d2l_3d_gpio_enable = d2l_common_pdata->gpio_num[2];
		d2l_3d_gpio_mode = d2l_common_pdata->gpio_num[3];

		mipi_dsi_buf_alloc(&d2l_tx_buf, DSI_BUF_SIZE);
		mipi_dsi_buf_alloc(&d2l_rx_buf, DSI_BUF_SIZE);

		return 0;
	}

	if (d2l_common_pdata == NULL) {
		pr_err("%s: d2l_common_pdata is NULL.\n", __func__);
		return -ENODEV;
	}

	bl_pwm = NULL;
	if (led_pwm >= 0) {
		bl_pwm = pwm_request(led_pwm, "lcd-backlight");
		if (bl_pwm == NULL || IS_ERR(bl_pwm)) {
			pr_err("%s pwm_request() failed.id=%d.bl_pwm=%d.\n",
			       __func__, led_pwm, (int) bl_pwm);
			bl_pwm = NULL;
			return -EIO;
		} else {
			pr_debug("%s.pwm_request() ok.pwm-id=%d.\n",
			       __func__, led_pwm);

		}
	} else {
		pr_err("%s. led_pwm is invalid.\n", __func__);
	}

	tn_pwm = pwm_request(1, "3D_TN_clk");
	if (tn_pwm == NULL || IS_ERR(tn_pwm)) {
		pr_err("%s pwm_request() failed.id=%d.tn_pwm=%d.\n",
		       __func__, 1, (int) tn_pwm);
		tn_pwm = NULL;
		return -EIO;
	} else {
		pr_debug("%s.pwm_request() ok.pwm-id=%d.\n", __func__, 1);

	}

	pinfo = pdev->dev.platform_data;

	if (pinfo == NULL) {
		pr_err("%s: pinfo is NULL.\n", __func__);
		return -ENODEV;
	}

	d2l_panel_data.panel_info = *pinfo;

	pdev->dev.platform_data = &d2l_panel_data;

	msm_fb_add_device(pdev);

	if (pinfo->is_3d_panel)
		mipi_dsi_3d_barrier_sysfs_register(&(pdev->dev));

	return ret;
}

static int __devexit mipi_d2l_remove(struct platform_device *pdev)
{
	
	pr_debug("%s.\n", __func__);

	if (bl_pwm) {
		pwm_free(bl_pwm);
		bl_pwm = NULL;
	}

	return 0;
}

int mipi_tc358764_dsi2lvds_register(struct msm_panel_info *pinfo,
					   u32 channel_id, u32 panel_id)
{
	struct platform_device *pdev = NULL;
	int ret;
	
	const char driver_name[] = "mipi_tc358764";

	pr_debug("%s.\n", __func__);
	ret = mipi_d2l_init();
	if (ret) {
		pr_err("mipi_d2l_init() failed with ret %u\n", ret);
		return ret;
	}

	
	pdev = platform_device_alloc(driver_name, (panel_id << 8)|channel_id);
	if (pdev == NULL)
		return -ENOMEM;

	pdev->dev.platform_data = pinfo;

	ret = platform_device_add(pdev);
	if (ret) {
		pr_err("%s: platform_device_register failed!\n", __func__);
		goto err_device_put;
	}

	return 0;

err_device_put:
	platform_device_put(pdev);
	return ret;
}

static struct platform_driver d2l_driver = {
	.probe  = mipi_d2l_probe,
	.remove = __devexit_p(mipi_d2l_remove),
	.driver = {
		.name   = DRV_NAME,
	},
};

static int mipi_d2l_init(void)
{
	pr_debug("%s.\n", __func__);

	d2l_i2c_client = NULL;

	return platform_driver_register(&d2l_driver);
}

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Toshiba MIPI-DSI-to-LVDS bridge driver");
MODULE_AUTHOR("Amir Samuelov <amirs@codeaurora.org>");
