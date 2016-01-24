/*
 * Copyright (C) 2010-2011 ARM Limited. All rights reserved.
 *
 * This program is free software and is provided to you under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation, and any use by you of this program is subject to the terms of such GNU licence.
 *
 * A copy of the licence is included with the program, and can also be obtained from Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

/**
 * @file mali_platform.c
 * Platform specific Mali driver functions for a sunxi platform
 */

#include <linux/platform_device.h>
#include <linux/version.h>
#include <linux/pm.h>
#ifdef CONFIG_PM_RUNTIME
#include <linux/pm_runtime.h>
#endif
#include <linux/mali/mali_utgard.h>
#include "mali_kernel_common.h"
#include <linux/dma-mapping.h>
#include <linux/module.h>
#include <linux/clk.h>
#include <mach/irqs.h>


/* NEXELL_FEATURE_PORTING */
//added by nexell
#include <asm/io.h>
#include <linux/clk.h>
#include <linux/delay.h>	/* mdelay */

#include <asm/mach/time.h>
#include <asm/mach/irq.h>
#include <asm/smp_twd.h>

#include <mach/platform.h>
#include <mach/devices.h>
#include <mach/soc.h>

#define MALI_MEM_SIZE_DEFAULT CFG_MEM_PHY_SYSTEM_SIZE
#if defined( CFG_MEM_PHY_DMAZONE_SIZE )
#undef  MALI_MEM_SIZE
#define MALI_MEM_SIZE 	(MALI_MEM_SIZE_DEFAULT + CFG_MEM_PHY_DMAZONE_SIZE)
#endif
#if defined( CONFIG_ION_NXP_CONTIGHEAP_SIZE )
#undef  MALI_MEM_SIZE
#define MALI_MEM_SIZE 	(MALI_MEM_SIZE_DEFAULT - (CONFIG_ION_NXP_CONTIGHEAP_SIZE * 1024))
#endif


//#define PHY_BASEADDR_MALI_GP			(0xC0070000 + 0x0)
//#define PHY_BASEADDR_MALI_MMU_GP		(0xC0070000 + 0x3000)
//#define PHY_BASEADDR_MALI_MMU_PP0		(0xC0070000 + 0x4000)
//#define PHY_BASEADDR_MALI_MMU_PP1		(0xC0070000 + 0x5000)
//#define PHY_BASEADDR_MALI_PP0			(0xC0070000 + 0x8000)
//#define PHY_BASEADDR_MALI_PP1			(0xC0070000 + 0xA000)
#define PHY_BASEADDR_MALI_PMU				(0xC0070000 + 0x2000) //~ + 0x10
#define PHY_BASEADDR_MALI_PMU_REG_SIZE		0x10
#define PHY_BASEADDR_PMU_ISOLATE			(0xC0010D00) //~ + 0x10
#define PHY_BASEADDR_PMU_ISOLATE_REG_SIZE	0x10
#define PHY_BASEADDR_POWER_GATE				(0xC0010800) //~ + 0x4
#define PHY_BASEADDR_POWER_GATE_REG_SIZE	0x4
#define PHY_BASEADDR_CLOCK_GATE				(0xC00C3000) //~ + 0x4
#define PHY_BASEADDR_CLOCK_GATE_REG_SIZE	0x4
#define PHY_BASEADDR_RESET  				(0xC0012000) //~ + 0xC
#define PHY_BASEADDR_RESET_REG_SIZE  		0xC
#ifdef CONFIG_ARCH_S5P6818
#define PHY_BASEADDR_LPI_ACTIVE				0xC001120C
#define PHY_BASEADDR_LPI_ACTIVE_REG_SIZE	0x4
#define PHY_BASEADDR_LPI_REQ				0xC0011114
#define PHY_BASEADDR_LPI_REQ_REG_SIZE		0x4
#endif

enum
{
	PHY_BASEADDR_MALI_PMU_IDX,
	PHY_BASEADDR_PMU_ISOLATE_IDX,
	PHY_BASEADDR_POWER_GATE_IDX,
	PHY_BASEADDR_CLOCK_GATE_IDX,
	PHY_BASEADDR_RESET_IDX,
#ifdef CONFIG_ARCH_S5P6818
	PHY_BASEADDR_LPI_ACTIVE_IDX,
	PHY_BASEADDR_LPI_REQ_IDX,
#endif
	PHY_BASEADDR_IDX_MAX
};
typedef struct
{
	unsigned int reg_addr;
	unsigned int reg_size;
}MALI_REG_MAPS;

static MALI_REG_MAPS __g_VRRegPhysMaps[PHY_BASEADDR_IDX_MAX] = {
	{PHY_BASEADDR_MALI_PMU, 		PHY_BASEADDR_MALI_PMU_REG_SIZE},
	{PHY_BASEADDR_PMU_ISOLATE, 	PHY_BASEADDR_PMU_ISOLATE_REG_SIZE},
	{PHY_BASEADDR_POWER_GATE, 	PHY_BASEADDR_POWER_GATE_REG_SIZE},
	{PHY_BASEADDR_CLOCK_GATE, 	PHY_BASEADDR_CLOCK_GATE_REG_SIZE},
	{PHY_BASEADDR_RESET, 		PHY_BASEADDR_RESET_REG_SIZE},
#ifdef CONFIG_ARCH_S5P6818
	{PHY_BASEADDR_LPI_ACTIVE,	PHY_BASEADDR_LPI_ACTIVE_REG_SIZE},
	{PHY_BASEADDR_LPI_REQ,		PHY_BASEADDR_LPI_REQ_REG_SIZE},
#endif
};
static unsigned int __g_VRRegVirtMaps[PHY_BASEADDR_IDX_MAX];

#define POWER_DELAY_MS	100
#if 1
#define MALI_DBG printk
#define MALI_PM_DBG PM_DBGOUT
#define MALI_IOCTL_DBG printk
#else
#define MALI_DBG
#define MALI_PM_DBG PM_DBGOUT
#define MALI_IOCTL_DBG
#endif


#if 1 /* new code */
static void nx_vr_make_reg_virt_maps(void)
{
	int i;
	for(i = 0 ; i < PHY_BASEADDR_IDX_MAX ; i++)
	{
		__g_VRRegVirtMaps[i] = ioremap_nocache(__g_VRRegPhysMaps[i].reg_addr, __g_VRRegPhysMaps[i].reg_size);
		if(!__g_VRRegVirtMaps[i])
		{
			MALI_PRINT(("ERROR! can't run 'ioremap_nocache()'\n"));
			break;
		}
	}
}

static void nx_vr_release_reg_virt_maps(void)
{
	int i;
	for(i = 0 ; i < PHY_BASEADDR_IDX_MAX ; i++)
	{
		iounmap(__g_VRRegVirtMaps[i]);
	}
}

#if defined( CONFIG_ARCH_S5P4418 )
static void nx_vr_power_down_all_s5p4418(void)
{
	u32 virt_addr_page;

	//reset
	virt_addr_page = __g_VRRegVirtMaps[PHY_BASEADDR_RESET_IDX] + 8;
	{
		unsigned int temp32 = ioread32(((u8*)virt_addr_page));
		unsigned int bit_mask = 1<<1; //65th
		MALI_DEBUG_PRINT(2,("setting Reset VR addr(0x%x)\n", (int)virt_addr_page));

		temp32 &= ~bit_mask;
		iowrite32(temp32, ((u8*)virt_addr_page));
	}

	//clk disalbe
	virt_addr_page = __g_VRRegVirtMaps[PHY_BASEADDR_CLOCK_GATE_IDX];
	{
		unsigned int read_val = ioread32((u8*)virt_addr_page);
		MALI_DEBUG_PRINT(2,("setting ClockGen, set 0\n"));
		iowrite32(read_val & ~0x3, ((u8*)virt_addr_page));
	}

	//ready
	virt_addr_page = __g_VRRegVirtMaps[PHY_BASEADDR_POWER_GATE_IDX];
	{
		MALI_DEBUG_PRINT(2,("setting PHY_BASEADDR_POWER_GATE, set 1\n"));
		iowrite32(0x1, ((u8*)virt_addr_page));
	}

	//enable ISolate
	virt_addr_page = __g_VRRegVirtMaps[PHY_BASEADDR_PMU_ISOLATE_IDX];
	{
		unsigned int read_val = ioread32((u8*)virt_addr_page);
		MALI_DEBUG_PRINT(2,("setting PHY_BASEADDR_PMU_ISOLATE, set 0\n"));
		iowrite32(read_val & ~1, ((u8*)virt_addr_page));
	}

	//pre charge down
	virt_addr_page = __g_VRRegVirtMaps[PHY_BASEADDR_PMU_ISOLATE_IDX] + 4;
	{
		unsigned int read_val = ioread32((u8*)virt_addr_page);
		MALI_DEBUG_PRINT(2,("setting PHY_BASEADDR_PMU_ISOLATE+4, set 1\n"));
		iowrite32(read_val | 1, ((u8*)virt_addr_page));
	}
	mdelay(1);

	//powerdown
	virt_addr_page = __g_VRRegVirtMaps[PHY_BASEADDR_PMU_ISOLATE_IDX] + 8;
	{
		unsigned int read_val = ioread32((u8*)virt_addr_page);
		MALI_DEBUG_PRINT(2,("setting PHY_BASEADDR_PMU_ISOLATE+8, set 1\n"));
		iowrite32(read_val | 1, ((u8*)virt_addr_page));
	}
	mdelay(1);

	//wait ack
	MALI_DBG("read PHY_BASEADDR_PMU_ISOLATE + 0xC\n");
	virt_addr_page = __g_VRRegVirtMaps[PHY_BASEADDR_PMU_ISOLATE_IDX] + 0xC;
	do{
		unsigned int powerUpAck = ioread32((u8*)virt_addr_page);
		MALI_DBG("Wait Power Down Ack(powerUpAck=0x%08x)\n", powerUpAck);
		if( (powerUpAck & 0x1) )
			break;
		MALI_DBG("Wait Power Down Ack(powerUpAck=0x%08x)\n", powerUpAck);
	}while(1);
}

static void nx_vr_power_up_all_s5p4418(void)
{
	u32 virt_addr_page;

	//ready
	virt_addr_page = __g_VRRegVirtMaps[PHY_BASEADDR_POWER_GATE_IDX];
	{
		MALI_DBG("setting PHY_BASEADDR_POWER_GATE, set 1\n");
		iowrite32(0x1, ((u8*)virt_addr_page));
	}

	//pre charge up
	virt_addr_page = __g_VRRegVirtMaps[PHY_BASEADDR_PMU_ISOLATE_IDX] + 4;
	{
		unsigned int read_val = ioread32((u8*)virt_addr_page);
		MALI_DBG("setting PHY_BASEADDR_PMU_ISOLATE+4, set 0\n");
		iowrite32(read_val & ~0x1, ((u8*)virt_addr_page));
	}

	//power up
	virt_addr_page = __g_VRRegVirtMaps[PHY_BASEADDR_PMU_ISOLATE_IDX] + 8;
	{
		unsigned int read_val = ioread32((u8*)virt_addr_page);
		MALI_DBG("setting PHY_BASEADDR_PMU_ISOLATE+8, set 0\n");
		iowrite32(read_val & ~0x1, ((u8*)virt_addr_page));
	}
	mdelay(1);

	//disable ISolate
	virt_addr_page = __g_VRRegVirtMaps[PHY_BASEADDR_PMU_ISOLATE_IDX];
	{
		unsigned int read_val = ioread32((u8*)virt_addr_page);
		MALI_DBG("setting PHY_BASEADDR_PMU_ISOLATE, set 1\n");
		iowrite32(read_val | 1, ((u8*)virt_addr_page));
	}
	mdelay(1);

	//wait ack
	MALI_DBG("read PHY_BASEADDR_PMU_ISOLATE + 0xC\n");
	virt_addr_page = __g_VRRegVirtMaps[PHY_BASEADDR_PMU_ISOLATE_IDX] + 0xC;
	do{
		unsigned int powerUpAck = ioread32((u8*)virt_addr_page);
		MALI_DBG("Wait Power UP Ack(powerUpAck=0x%08x)\n", powerUpAck);
		if( !(powerUpAck & 0x1) )
			break;
		MALI_DBG("Wait Power UP Ack(powerUpAck=0x%08x)\n", powerUpAck);
	}while(1);

	//clk enable
	virt_addr_page = __g_VRRegVirtMaps[PHY_BASEADDR_CLOCK_GATE_IDX];
	{
		unsigned int read_val = ioread32((u8*)virt_addr_page);
		MALI_DBG("setting ClockGen, set 1\n");
		iowrite32(0x3 | read_val, ((u8*)virt_addr_page));
	}

	//reset release
	virt_addr_page = __g_VRRegVirtMaps[PHY_BASEADDR_RESET_IDX] + 8;
	{
		unsigned int temp32 = ioread32(((u8*)virt_addr_page));
		unsigned int bit_mask = 1<<1; //65th
		MALI_DBG("setting Reset VR addr(0x%x)\n", (int)virt_addr_page);
		temp32 |= bit_mask;
		iowrite32(temp32, ((u8*)virt_addr_page));
	}
	mdelay(1);

	//mask vr400 PMU interrupt
	virt_addr_page = __g_VRRegVirtMaps[PHY_BASEADDR_MALI_PMU_IDX] + 0xC;
	{
		MALI_PM_DBG("mask PMU INT, addr(0x%x)\n", (int)virt_addr_page);
		iowrite32(0x0, ((u8*)virt_addr_page));
	}

	//power up vr400
	virt_addr_page = __g_VRRegVirtMaps[PHY_BASEADDR_MALI_PMU_IDX];
	{
		MALI_DBG("setting PHY_BASEADDR_MALI_PMU addr(0x%x)\n", (int)virt_addr_page);
		iowrite32(0xF/*GP, L2C, PP0, PP1*/, ((u8*)virt_addr_page));
	}
}
#endif
#if defined(CONFIG_ARCH_S5P6818)
static void nx_vr_power_down_enter_reset_s5p6818(void)
{
	u32 virt_addr_page, read32;

	//=========================
	// [ mali PBUS ]
	//=========================
	// wait until LPI ACTIVE HIGH
	//=========================
	virt_addr_page = __g_VRRegVirtMaps[PHY_BASEADDR_LPI_ACTIVE_IDX];
	//printk("LPI ACTIVE HIGH waitting start...\n");
	do{
		read32 = ioread32((u8*)virt_addr_page);
		if( (read32>>12) & 0x01 )
			break;
	}while(1);
	//printk("LPI ACTIVE HIGH waitting done\n");

	//==========================
	// LPI REQ, Set LOW
	//==========================
	virt_addr_page = __g_VRRegVirtMaps[PHY_BASEADDR_LPI_REQ_IDX];
	read32 = ioread32((u8*)virt_addr_page);
	read32 = read32 & (~(1<<2)); // CSYSREQ LOW
	iowrite32(read32, ((u8*)virt_addr_page));

	//==========================
	// wait until LPI ACK LOW
	//==========================
	virt_addr_page = __g_VRRegVirtMaps[PHY_BASEADDR_LPI_ACTIVE_IDX];
	//printk("LPI ACK LOW waitting start...\n");
	do{
		read32 = ioread32((u8*)virt_addr_page);
		if( !((read32>>13) & 0x01) )
			break;
	}while(1);
	//printk("LPI ACK LOW waitting done\n");

	//=========================
	// [ mali MBUS ]
	//=========================
	// wait until LPI ACTIVE HIGH
	//=========================
	virt_addr_page = __g_VRRegVirtMaps[PHY_BASEADDR_LPI_ACTIVE_IDX];
	//printk("LPI ACTIVE HIGH waitting start...\n");
	do{
		read32 = ioread32((u8*)virt_addr_page);
		if( (read32>>20) & 0x01 )
			break;
	}while(1);
	//printk("LPI ACTIVE HIGH waitting done\n");

	//==========================
	// LPI REQ, Set LOW
	//==========================
	virt_addr_page = __g_VRRegVirtMaps[PHY_BASEADDR_LPI_REQ_IDX];
	read32 = ioread32((u8*)virt_addr_page);
	read32 = read32 & (~(1<<1)); // CSYSREQ LOW
	iowrite32(read32, ((u8*)virt_addr_page));

	//==========================
	// wait until LPI ACK LOW
	//==========================
	virt_addr_page = __g_VRRegVirtMaps[PHY_BASEADDR_LPI_ACTIVE_IDX];
	//printk("LPI ACTIVE LOW waitting start...\n");
	do{
		read32 = ioread32((u8*)virt_addr_page);
		if( !((read32>>21) & 0x01) )
			break;
	}while(1);
	//printk("LPI ACTIVE LOW waitting done\n");
}

static void nx_vr_power_up_leave_reset_s5p6818(void)
{
	u32 virt_addr_page, read32;

	//==========================
	// [ mali PBUS ]
	//==========================
	// LPI REQ, Set HIGH
	//==========================
	virt_addr_page = __g_VRRegVirtMaps[PHY_BASEADDR_LPI_REQ_IDX];
	read32 = ioread32((u8*)virt_addr_page);
	read32 = read32 | (1<<2); // CSYSREQ HIGH
	iowrite32(read32, ((u8*)virt_addr_page));

	//==========================
	// wait until LPI ACK HIGH
	//==========================
	virt_addr_page = __g_VRRegVirtMaps[PHY_BASEADDR_LPI_ACTIVE_IDX];
	//printk("LPI ACK HIGH waitting start...\n");
	do{
		read32 = ioread32((u8*)virt_addr_page);
		if( (read32>>13) & 0x01 )
			break;
	}while(1);
	//printk("LPI ACK HIGH waitting done\n");

	//==========================
	// [ mali MBUS ]
	//==========================
	// LPI REQ, Set HIGH
	//==========================
	virt_addr_page = __g_VRRegVirtMaps[PHY_BASEADDR_LPI_REQ_IDX];
	read32 = ioread32((u8*)virt_addr_page);
	read32 = read32 | (1<<1); // CSYSREQ HIGH
	iowrite32(read32, ((u8*)virt_addr_page));

	//==========================
	// wait until LPI ACK HIGH
	//==========================
	virt_addr_page = __g_VRRegVirtMaps[PHY_BASEADDR_LPI_ACTIVE_IDX];
	//printk("LPI ACK ACK waitting start...\n");
	do{
		read32 = ioread32((u8*)virt_addr_page);
		if( (read32>>21) & 0x01 )
			break;
	}while(1);
	//printk("LPI ACK ACK waitting done\n");

}

static void nx_vr_power_down_all_s5p6818(void)
{
	u32 virt_addr_page, map_size;

	//reset
	nx_vr_power_down_enter_reset_s5p6818();
	virt_addr_page = __g_VRRegVirtMaps[PHY_BASEADDR_RESET_IDX] + 8;
	if (NULL != virt_addr_page)
	{
		unsigned int temp32 = ioread32(((u8*)virt_addr_page));
		unsigned int bit_mask = 1<<1; //65th
		MALI_DBG("setting Reset VR addr(0x%x)\n", (int)virt_addr_page);
		temp32 &= ~bit_mask;
		iowrite32(temp32, ((u8*)virt_addr_page));
	}
	mdelay(1);

	//clk disalbe
	virt_addr_page = __g_VRRegVirtMaps[PHY_BASEADDR_CLOCK_GATE_IDX];
	if (NULL != virt_addr_page)
	{
		unsigned int read_val = ioread32((u8*)virt_addr_page);
		MALI_DBG("setting ClockGen, set 0\n");
		iowrite32(read_val & ~0x3, ((u8*)virt_addr_page));
	}
	mdelay(1);

	//ready
	virt_addr_page = __g_VRRegVirtMaps[PHY_BASEADDR_POWER_GATE_IDX];
	if (NULL != virt_addr_page)
	{
		MALI_DBG("setting PHY_BASEADDR_POWER_GATE, set 1\n");
		iowrite32(0x1, ((u8*)virt_addr_page));
	}

	//enable ISolate
	virt_addr_page = __g_VRRegVirtMaps[PHY_BASEADDR_PMU_ISOLATE_IDX];
	if (NULL != virt_addr_page)
	{
		unsigned int read_val = ioread32((u8*)virt_addr_page);
		MALI_DBG("setting PHY_BASEADDR_PMU_ISOLATE, set 0\n");
		iowrite32(read_val & ~1, ((u8*)virt_addr_page));
	}

	//pre charge down
	virt_addr_page = __g_VRRegVirtMaps[PHY_BASEADDR_PMU_ISOLATE_IDX] + 4;
	if (NULL != virt_addr_page)
	{
		unsigned int read_val = ioread32((u8*)virt_addr_page);
		MALI_DBG("setting PHY_BASEADDR_PMU_ISOLATE+4, set 1\n");
		iowrite32(read_val | 1, ((u8*)virt_addr_page));
	}
	mdelay(1);

	//powerdown
	virt_addr_page = __g_VRRegVirtMaps[PHY_BASEADDR_PMU_ISOLATE_IDX] + 8;
	if (NULL != virt_addr_page)
	{
		unsigned int read_val = ioread32((u8*)virt_addr_page);
		MALI_DBG("setting PHY_BASEADDR_PMU_ISOLATE+8, set 1\n");
		iowrite32(read_val | 1, ((u8*)virt_addr_page));
	}
	mdelay(1);

	//wait ack
	MALI_DBG("read PHY_BASEADDR_PMU_ISOLATE + 0xC\n");
	virt_addr_page = __g_VRRegVirtMaps[PHY_BASEADDR_PMU_ISOLATE_IDX] + 0xC;
	do{
		unsigned int powerUpAck = ioread32((u8*)virt_addr_page);
		MALI_DBG("Wait Power Down Ack(powerUpAck=0x%08x)\n", powerUpAck);
		if( (powerUpAck & 0x1) )
			break;
		MALI_DBG("Wait Power Down Ack(powerUpAck=0x%08x)\n", powerUpAck);
	}while(1);
}

static void nx_vr_power_up_all_s5p6818(void)
{
	u32 virt_addr_page;

	//ready
	virt_addr_page = __g_VRRegVirtMaps[PHY_BASEADDR_POWER_GATE_IDX];
	if (NULL != virt_addr_page)
	{
		MALI_DBG("setting PHY_BASEADDR_POWER_GATE, set 1\n");
		iowrite32(0x1, ((u8*)virt_addr_page));
	}

	//pre charge up
	virt_addr_page = __g_VRRegVirtMaps[PHY_BASEADDR_PMU_ISOLATE_IDX] + 4;
	if (NULL != virt_addr_page)
	{
		unsigned int read_val = ioread32((u8*)virt_addr_page);
		MALI_DBG("setting PHY_BASEADDR_PMU_ISOLATE+4, set 0\n");
		iowrite32(read_val & ~0x1, ((u8*)virt_addr_page));
	}

	//power up
	virt_addr_page = __g_VRRegVirtMaps[PHY_BASEADDR_PMU_ISOLATE_IDX] + 8;
	if (NULL != virt_addr_page)
	{
		unsigned int read_val = ioread32((u8*)virt_addr_page);
		MALI_DBG("setting PHY_BASEADDR_PMU_ISOLATE+8, set 0\n");
		iowrite32(read_val & ~0x1, ((u8*)virt_addr_page));
	}
	mdelay(1);

	//disable ISolate
	virt_addr_page = __g_VRRegVirtMaps[PHY_BASEADDR_PMU_ISOLATE_IDX];
	if (NULL != virt_addr_page)
	{
		unsigned int read_val = ioread32((u8*)virt_addr_page);
		MALI_DBG("setting PHY_BASEADDR_PMU_ISOLATE, set 1\n");
		iowrite32(read_val | 1, ((u8*)virt_addr_page));
	}
	mdelay(1);

	//wait ack
	MALI_DBG("read PHY_BASEADDR_PMU_ISOLATE + 0xC\n");
	virt_addr_page = __g_VRRegVirtMaps[PHY_BASEADDR_PMU_ISOLATE_IDX] + 0xC;
	do{
		unsigned int powerUpAck = ioread32((u8*)virt_addr_page);
		MALI_DBG("Wait Power UP Ack(powerUpAck=0x%08x)\n", powerUpAck);
		if( !(powerUpAck & 0x1) )
			break;
		MALI_DBG("Wait Power UP Ack(powerUpAck=0x%08x)\n", powerUpAck);
	}while(1);

	//clk enable
	virt_addr_page = __g_VRRegVirtMaps[PHY_BASEADDR_CLOCK_GATE_IDX];
	if (NULL != virt_addr_page)
	{
		unsigned int read_val = ioread32((u8*)virt_addr_page);
		MALI_DBG("setting ClockGen, set 1\n");
		iowrite32(0x3 | read_val, ((u8*)virt_addr_page));
	}

	//reset
	virt_addr_page = __g_VRRegVirtMaps[PHY_BASEADDR_RESET_IDX] + 8;
	if (NULL != virt_addr_page)
	{
		unsigned int temp32 = ioread32(((u8*)virt_addr_page));
		unsigned int bit_mask = 1<<1; //65th
		MALI_DBG("setting Reset VR addr(0x%x)\n", (int)virt_addr_page);

		//reset leave
		nx_vr_power_up_leave_reset_s5p6818();
		temp32 |= bit_mask;
		iowrite32(temp32, ((u8*)virt_addr_page));
	}
	mdelay(1);

	//mask vr400 PMU interrupt
	virt_addr_page = __g_VRRegVirtMaps[PHY_BASEADDR_MALI_PMU_IDX] + 0xC;
	if (NULL != virt_addr_page)
	{
		MALI_PM_DBG("mask PMU INT, addr(0x%x)\n", (int)virt_addr_page);
		iowrite32(0x0, ((u8*)virt_addr_page));
	}

	//power up vr400
	virt_addr_page = __g_VRRegVirtMaps[PHY_BASEADDR_MALI_PMU_IDX];
	if (NULL != virt_addr_page)
	{
		MALI_DBG("setting PHY_BASEADDR_MALI_PMU addr(0x%x)\n", (int)virt_addr_page);
		iowrite32(0x3F/*GP, L2C, PP0, PP1, PP2, PP3*/, ((u8*)virt_addr_page));
	}
}
#endif

#else /* legacy code */
#if defined( CONFIG_ARCH_S5P4418 )
static void nx_vr_power_down_all_s5p4418(void)
{
	u32 phys_addr_page, map_size;
	void *mem_mapped;

	//temp test
#if 0
	//clear vr400 pmu interrupt
	phys_addr_page = PHY_BASEADDR_MALI_PMU + 0x18;
	map_size	   = sizeof(u32);
	mem_mapped = ioremap_nocache(phys_addr_page, map_size);
	if (NULL != mem_mapped)
	{
		MALI_PM_DBG("clear PMU int, addr(0x%x)\n", (int)mem_mapped);
		iowrite32(1, ((u8*)mem_mapped));
		iounmap(mem_mapped);
	}
#endif
#if 0
	//clear s5p4418 interrupt controller
	phys_addr_page = 0xC0003014;
	map_size	   = sizeof(u32);
	mem_mapped = ioremap_nocache(phys_addr_page, map_size);
	if (NULL != mem_mapped)
	{
		MALI_PM_DBG("clear INTCLEAR, addr(0x%x)\n", (int)mem_mapped);
		iowrite32(0x100, ((u8*)mem_mapped));
		iounmap(mem_mapped);
	}
#else
#endif

	//clk disalbe
	phys_addr_page = PHY_BASEADDR_CLOCK_GATE;
	map_size	   = sizeof(u32);
	mem_mapped = ioremap_nocache(phys_addr_page, map_size);
	if (NULL != mem_mapped)
	{
		unsigned int read_val = ioread32((u8*)mem_mapped);
		MALI_DBG("setting ClockGen, set 0\n");
		iowrite32(read_val & ~0x3, ((u8*)mem_mapped));
	}
	iounmap(mem_mapped);

	//ready
	phys_addr_page = PHY_BASEADDR_POWER_GATE;
	map_size	   = sizeof(u32);
	mem_mapped = ioremap_nocache(phys_addr_page, map_size);
	if (NULL != mem_mapped)
	{
		MALI_DBG("setting PHY_BASEADDR_POWER_GATE, set 1\n");
		iowrite32(0x1, ((u8*)mem_mapped));
	}
	iounmap(mem_mapped);

	//enable ISolate
	phys_addr_page = PHY_BASEADDR_PMU_ISOLATE;
	map_size       = sizeof(u32);
	mem_mapped = ioremap_nocache(phys_addr_page, map_size);
	if (NULL != mem_mapped)
	{
		unsigned int read_val = ioread32((u8*)mem_mapped);
		MALI_DBG("setting PHY_BASEADDR_PMU_ISOLATE, set 0\n");
		iowrite32(read_val & ~1, ((u8*)mem_mapped));
	}
	iounmap(mem_mapped);

	//pre charge down
	phys_addr_page = PHY_BASEADDR_PMU_ISOLATE + 4;
	map_size       = sizeof(u32);
	mem_mapped = ioremap_nocache(phys_addr_page, map_size);
	if (NULL != mem_mapped)
	{
		unsigned int read_val = ioread32((u8*)mem_mapped);
		MALI_DBG("setting PHY_BASEADDR_PMU_ISOLATE+4, set 1\n");
		iowrite32(read_val | 1, ((u8*)mem_mapped));
	}
	iounmap(mem_mapped);
	mdelay(1);

#if 1
	//powerdown
	phys_addr_page = PHY_BASEADDR_PMU_ISOLATE + 8;
	map_size       = sizeof(u32);
	mem_mapped = ioremap_nocache(phys_addr_page, map_size);
	if (NULL != mem_mapped)
	{
		unsigned int read_val = ioread32((u8*)mem_mapped);
		MALI_DBG("setting PHY_BASEADDR_PMU_ISOLATE+8, set 1\n");
		iowrite32(read_val | 1, ((u8*)mem_mapped));
	}
	iounmap(mem_mapped);
	mdelay(1);

	//wait ack
	MALI_DBG("read PHY_BASEADDR_PMU_ISOLATE + 0xC\n");
	phys_addr_page = PHY_BASEADDR_PMU_ISOLATE + 0xC;
	map_size       = sizeof(u32);
	mem_mapped = ioremap_nocache(phys_addr_page, map_size);
	do{
		unsigned int powerUpAck = ioread32((u8*)mem_mapped);
		MALI_DBG("Wait Power Down Ack(powerUpAck=0x%08x)\n", powerUpAck);
		if( (powerUpAck & 0x1) )
			break;
		MALI_DBG("Wait Power Down Ack(powerUpAck=0x%08x)\n", powerUpAck);
	}while(1);
	iounmap(mem_mapped);
#endif
}

static void nx_vr_power_up_all_s5p4418(void)
{
	u32 phys_addr_page, map_size;
	void *mem_mapped;

	//ready
	phys_addr_page = PHY_BASEADDR_POWER_GATE;
	map_size	   = sizeof(u32);
	mem_mapped = ioremap_nocache(phys_addr_page, map_size);
	if (NULL != mem_mapped)
	{
		MALI_DBG("setting PHY_BASEADDR_POWER_GATE, set 1\n");
		iowrite32(0x1, ((u8*)mem_mapped));
	}
	iounmap(mem_mapped);

	//pre charge up
	phys_addr_page = PHY_BASEADDR_PMU_ISOLATE + 4;
	map_size	   = sizeof(u32);
	mem_mapped = ioremap_nocache(phys_addr_page, map_size);
	if (NULL != mem_mapped)
	{
		unsigned int read_val = ioread32((u8*)mem_mapped);
		MALI_DBG("setting PHY_BASEADDR_PMU_ISOLATE+4, set 0\n");
		iowrite32(read_val & ~0x1, ((u8*)mem_mapped));
	}
	iounmap(mem_mapped);

	//power up
	phys_addr_page = PHY_BASEADDR_PMU_ISOLATE + 8;
	map_size	   = sizeof(u32);
	mem_mapped = ioremap_nocache(phys_addr_page, map_size);
	if (NULL != mem_mapped)
	{
		unsigned int read_val = ioread32((u8*)mem_mapped);
		MALI_DBG("setting PHY_BASEADDR_PMU_ISOLATE+8, set 0\n");
		iowrite32(read_val & ~0x1, ((u8*)mem_mapped));
	}
	iounmap(mem_mapped);
	mdelay(1);

	//disable ISolate
	phys_addr_page = PHY_BASEADDR_PMU_ISOLATE;
	map_size	   = sizeof(u32);
	mem_mapped = ioremap_nocache(phys_addr_page, map_size);
	if (NULL != mem_mapped)
	{
		unsigned int read_val = ioread32((u8*)mem_mapped);
		MALI_DBG("setting PHY_BASEADDR_PMU_ISOLATE, set 1\n");
		iowrite32(read_val | 1, ((u8*)mem_mapped));
	}
	iounmap(mem_mapped);
	mdelay(1);

	//wait ack
	MALI_DBG("read PHY_BASEADDR_PMU_ISOLATE + 0xC\n");
	phys_addr_page = PHY_BASEADDR_PMU_ISOLATE + 0xC;
	map_size	   = sizeof(u32);
	mem_mapped = ioremap_nocache(phys_addr_page, map_size);
	do{
		unsigned int powerUpAck = ioread32((u8*)mem_mapped);
		MALI_DBG("Wait Power UP Ack(powerUpAck=0x%08x)\n", powerUpAck);
		if( !(powerUpAck & 0x1) )
			break;
		MALI_DBG("Wait Power UP Ack(powerUpAck=0x%08x)\n", powerUpAck);
	}while(1);
	iounmap(mem_mapped);

	//clk enable
	phys_addr_page = PHY_BASEADDR_CLOCK_GATE;
	map_size	   = sizeof(u32);
	mem_mapped = ioremap_nocache(phys_addr_page, map_size);
	if (NULL != mem_mapped)
	{
		unsigned int read_val = ioread32((u8*)mem_mapped);
		MALI_DBG("setting ClockGen, set 1\n");
		iowrite32(0x3 | read_val, ((u8*)mem_mapped));
	}
	iounmap(mem_mapped);

	//reset release
	phys_addr_page = PHY_BASEADDR_RESET + 8;
	map_size	   = sizeof(u32);
	mem_mapped = ioremap_nocache(phys_addr_page, map_size);
	if (NULL != mem_mapped)
	{
		unsigned int temp32 = ioread32(((u8*)mem_mapped));
		unsigned int bit_mask = 1<<1; //65th
		MALI_DBG("setting Reset VR addr(0x%x)\n", (int)mem_mapped);
		temp32 &= ~bit_mask;
		iowrite32(temp32, ((u8*)mem_mapped));
		temp32 |= bit_mask;
		iowrite32(temp32, ((u8*)mem_mapped));
	}
	iounmap(mem_mapped);
	mdelay(1);

	//mask vr400 PMU interrupt
	phys_addr_page = PHY_BASEADDR_MALI_PMU + 0xC;
	map_size	   = sizeof(u32);
	mem_mapped = ioremap_nocache(phys_addr_page, map_size);
	if (NULL != mem_mapped)
	{
		MALI_PM_DBG("mask PMU INT, addr(0x%x)\n", (int)mem_mapped);
		iowrite32(0x0, ((u8*)mem_mapped));
	}
	iounmap(mem_mapped);

	//power up vr400
	phys_addr_page = PHY_BASEADDR_MALI_PMU;
	map_size	   = sizeof(u32);
	mem_mapped = ioremap_nocache(phys_addr_page, map_size);
	if (NULL != mem_mapped)
	{
		MALI_DBG("setting PHY_BASEADDR_MALI_PMU addr(0x%x)\n", (int)mem_mapped);
		iowrite32(0xF/*GP, L2C, PP0, PP1*/, ((u8*)mem_mapped));
	}
	iounmap(mem_mapped);

	//ocurr ERROR ! at 10ms delay
	//MALI_PM_DBG("mdelay 20ms\n");
	//mdelay(20);

#if 0
	//mask vr400 GP2 AXI_BUS_ERROR
	phys_addr_page = PHY_BASEADDR_MALI_GP + 0x2C;
	map_size	   = sizeof(u32);
	mem_mapped = ioremap_nocache(phys_addr_page, map_size);
	if (NULL != mem_mapped)
	{
		unsigned int temp32 = ioread32(((u8*)mem_mapped));
		MALI_PM_DBG("mask GP2 BUSERR_INT, addr(0x%x, 0x%x)\n", (int)mem_mapped, temp32);
		iounmap(mem_mapped);
	}

	//temp test
	MALI_PM_DBG("\n============================\n", (int)mem_mapped);
	//clear vr400 pmu interrupt
	//PHY_BASEADDR_RESET + 8 에 reset 해줘야만 한다
	phys_addr_page = PHY_BASEADDR_MALI_PMU + 0x18;
	map_size	   = sizeof(u32);
	mem_mapped = ioremap_nocache(phys_addr_page, map_size);
	if (NULL != mem_mapped)
	{
		MALI_PM_DBG("clear PMU int, addr(0x%x)\n", (int)mem_mapped);
		iowrite32(1, ((u8*)mem_mapped));
		iounmap(mem_mapped);
	}
#endif
#if 0
	//clear s5p4418 interrupt controller
	phys_addr_page = 0xC0003014;
	map_size	   = sizeof(u32);
	mem_mapped = ioremap_nocache(phys_addr_page, map_size);
	if (NULL != mem_mapped)
	{
		MALI_PM_DBG("clear INTCLEAR, addr(0x%x)\n", (int)mem_mapped);
		iowrite32(0x100, ((u8*)mem_mapped));
		iounmap(mem_mapped);
	}
	MALI_PM_DBG("============================\n", (int)mem_mapped);
#else
#endif

	#if 0
	//temp test
	phys_addr_page = 0xC0010000;
	map_size       = 0x30;
	mem_mapped = ioremap_nocache(phys_addr_page, map_size);
	if (NULL != mem_mapped)
	{
		int i = 0;
		for(i = 0 ; i < 12 ; i++)
		{
			unsigned int read_val = ioread32((u8*)mem_mapped + (i*4));
			MALI_DBG("read reg addr(0x%x), data(0x%x)\n", (int)mem_mapped + (i*4), read_val);
		}
		iounmap(mem_mapped);
	}
	phys_addr_page = 0xC0010D00;
	map_size       = 0x10;
	mem_mapped = ioremap_nocache(phys_addr_page, map_size);
	if (NULL != mem_mapped)
	{
		int i = 0;
		for(i = 0 ; i < 4 ; i++)
		{
			unsigned int read_val = ioread32((u8*)mem_mapped + (i*4));
			MALI_DBG("read reg addr(0x%x), data(0x%x)\n", (int)mem_mapped + (i*4), read_val);
		}
		iounmap(mem_mapped);
	}
	#endif
}
#endif

#if defined(CONFIG_ARCH_S5P6818)
#if 0
static void NX_RESET_EnterReset ( uint RSTIndex )
{
	//=========================
	// mali LPI protocol use
	//=========================
	volatile uint * reg;

	//=========================
	// [ mali PBUS ]
	//=========================
	// wait until LPI ACTIVE HIGH
	//=========================
	reg =(uint*) PHY_BASEADDR_LPI_ACTIVE;
	while( (*reg>>12) & 0x01 != 1 ) ;

	//==========================
	// LPI REQ, Set LOW
	//==========================
	reg =(uint*) PHY_BASEADDR_LPI_REQ;
	*reg = *reg & (~(1<<2)); // CSYSREQ LOW

	//==========================
	// wait until LPI ACK LOW
	//==========================
	reg =(uint*) PHY_BASEADDR_LPI_ACTIVE;
	while( (*reg>>13) & 0x01 != 0 );

	//=========================
	// [ mali MBUS ]
	//=========================
	// wait until LPI ACTIVE HIGH
	//=========================
	reg =(uint*) PHY_BASEADDR_LPI_ACTIVE;
	while( (*reg>>20) & 0x01 != 1 ) ;
	//==========================
	// LPI REQ, Set LOW
	//==========================
	reg =(uint*) PHY_BASEADDR_LPI_REQ;
	*reg = *reg & (~(1<<1)); // CSYSREQ LOW
	//==========================
	// wait until LPI ACK LOW
	//==========================
	reg =(uint*) PHY_BASEADDR_LPI_ACTIVE;
	while( (*reg>>21) & 0x01 != 0 );

	//not eco
	//NX_CLKPWR_SetGRP3DClockEnable(CFALSE);
}

static void NX_RESET_LeaveReset ( uint RSTIndex )
{
	//=========================
	// mali LPI protocol use
	//=========================
	volatile uint *reg;

	//==========================
	// [ mali PBUS ]
	//==========================
	// LPI REQ, Set HIGH
	//==========================
	reg =(uint*) PHY_BASEADDR_LPI_REQ;
	*reg = *reg | (1<<2); // CSYSREQ HIGH
	//==========================
	// wait until LPI ACK HIGH
	//==========================
	reg =(uint*) PHY_BASEADDR_LPI_ACTIVE;
	while( (*reg>>13) & 0x01 != 1 );

	//==========================
	// [ mali MBUS ]
	//==========================
	// LPI REQ, Set HIGH
	//==========================
	reg =(uint*) PHY_BASEADDR_LPI_REQ;
	*reg = *reg | (1<<1); // CSYSREQ HIGH
	//==========================
	// wait until LPI ACK HIGH
	//==========================
	reg =(uint*) PHY_BASEADDR_LPI_ACTIVE;
	while( (*reg>>21) & 0x01 != 1 );
}
#endif
static void nx_vr_power_down_enter_reset_s5p6818(void)
{
	u32 phys_addr_page, map_size, read32;
	void *mem_mapped;

	//=========================
	// [ mali PBUS ]
	//=========================
	// wait until LPI ACTIVE HIGH
	//=========================
	phys_addr_page = PHY_BASEADDR_LPI_ACTIVE;
	map_size       = sizeof(u32);
	mem_mapped = ioremap_nocache(phys_addr_page, map_size);
	//printk("LPI ACTIVE HIGH waitting start...\n");
	do{
		read32 = ioread32((u8*)mem_mapped);
		if( (read32>>12) & 0x01 )
			break;
	}while(1);
	//printk("LPI ACTIVE HIGH waitting done\n");
	iounmap(mem_mapped);

	//==========================
	// LPI REQ, Set LOW
	//==========================
	phys_addr_page = PHY_BASEADDR_LPI_REQ;
	map_size       = sizeof(u32);
	mem_mapped = ioremap_nocache(phys_addr_page, map_size);
	read32 = ioread32((u8*)mem_mapped);
	read32 = read32 & (~(1<<2)); // CSYSREQ LOW
	iowrite32(read32, ((u8*)mem_mapped));
	iounmap(mem_mapped);

	//==========================
	// wait until LPI ACK LOW
	//==========================
	phys_addr_page = PHY_BASEADDR_LPI_ACTIVE;
	map_size       = sizeof(u32);
	mem_mapped = ioremap_nocache(phys_addr_page, map_size);
	//printk("LPI ACK LOW waitting start...\n");
	do{
		read32 = ioread32((u8*)mem_mapped);
		if( !((read32>>13) & 0x01) )
			break;
	}while(1);
	//printk("LPI ACK LOW waitting done\n");
	iounmap(mem_mapped);

	//=========================
	// [ mali MBUS ]
	//=========================
	// wait until LPI ACTIVE HIGH
	//=========================
	phys_addr_page = PHY_BASEADDR_LPI_ACTIVE;
	map_size       = sizeof(u32);
	mem_mapped = ioremap_nocache(phys_addr_page, map_size);
	//printk("LPI ACTIVE HIGH waitting start...\n");
	do{
		read32 = ioread32((u8*)mem_mapped);
		if( (read32>>20) & 0x01 )
			break;
	}while(1);
	//printk("LPI ACTIVE HIGH waitting done\n");
	iounmap(mem_mapped);

	//==========================
	// LPI REQ, Set LOW
	//==========================
	phys_addr_page = PHY_BASEADDR_LPI_REQ;
	map_size       = sizeof(u32);
	mem_mapped = ioremap_nocache(phys_addr_page, map_size);
	read32 = ioread32((u8*)mem_mapped);
	read32 = read32 & (~(1<<1)); // CSYSREQ LOW
	iowrite32(read32, ((u8*)mem_mapped));
	iounmap(mem_mapped);

	//==========================
	// wait until LPI ACK LOW
	//==========================
	phys_addr_page = PHY_BASEADDR_LPI_ACTIVE;
	map_size       = sizeof(u32);
	mem_mapped = ioremap_nocache(phys_addr_page, map_size);
	//printk("LPI ACTIVE LOW waitting start...\n");
	do{
		read32 = ioread32((u8*)mem_mapped);
		if( !((read32>>21) & 0x01) )
			break;
	}while(1);
	//printk("LPI ACTIVE LOW waitting done\n");
	iounmap(mem_mapped);
}

static void nx_vr_power_up_leave_reset_s5p6818(void)
{
	u32 phys_addr_page, map_size, read32;
	void *mem_mapped;

	//==========================
	// [ mali PBUS ]
	//==========================
	// LPI REQ, Set HIGH
	//==========================
	phys_addr_page = PHY_BASEADDR_LPI_REQ;
	map_size       = sizeof(u32);
	mem_mapped = ioremap_nocache(phys_addr_page, map_size);
	read32 = ioread32((u8*)mem_mapped);
	read32 = read32 | (1<<2); // CSYSREQ HIGH
	iowrite32(read32, ((u8*)mem_mapped));
	iounmap(mem_mapped);

	//==========================
	// wait until LPI ACK HIGH
	//==========================
	phys_addr_page = PHY_BASEADDR_LPI_ACTIVE;
	map_size       = sizeof(u32);
	mem_mapped = ioremap_nocache(phys_addr_page, map_size);
	//printk("LPI ACK HIGH waitting start...\n");
	do{
		read32 = ioread32((u8*)mem_mapped);
		if( (read32>>13) & 0x01 )
			break;
	}while(1);
	//printk("LPI ACK HIGH waitting done\n");
	iounmap(mem_mapped);

	//==========================
	// [ mali MBUS ]
	//==========================
	// LPI REQ, Set HIGH
	//==========================
	phys_addr_page = PHY_BASEADDR_LPI_REQ;
	map_size       = sizeof(u32);
	mem_mapped = ioremap_nocache(phys_addr_page, map_size);
	read32 = ioread32((u8*)mem_mapped);
	read32 = read32 | (1<<1); // CSYSREQ HIGH
	iowrite32(read32, ((u8*)mem_mapped));
	iounmap(mem_mapped);

	//==========================
	// wait until LPI ACK HIGH
	//==========================
	phys_addr_page = PHY_BASEADDR_LPI_ACTIVE;
	map_size       = sizeof(u32);
	mem_mapped = ioremap_nocache(phys_addr_page, map_size);
	//printk("LPI ACK ACK waitting start...\n");
	do{
		read32 = ioread32((u8*)mem_mapped);
		if( (read32>>21) & 0x01 )
			break;
	}while(1);
	//printk("LPI ACK ACK waitting done\n");
	iounmap(mem_mapped);

}

static void nx_vr_power_down_all_s5p6818(void)
{
	u32 phys_addr_page, map_size;
	void *mem_mapped;

	//reset
	nx_vr_power_down_enter_reset_s5p6818();
	phys_addr_page = PHY_BASEADDR_RESET + 8;
	map_size	   = sizeof(u32);
	mem_mapped = ioremap_nocache(phys_addr_page, map_size);
	if (NULL != mem_mapped)
	{
		unsigned int temp32 = ioread32(((u8*)mem_mapped));
		unsigned int bit_mask = 1<<1; //65th
		MALI_DBG("setting Reset VR addr(0x%x)\n", (int)mem_mapped);
		temp32 &= ~bit_mask;
		iowrite32(temp32, ((u8*)mem_mapped));
	}
	iounmap(mem_mapped);
	mdelay(1);

	//clk disalbe
	phys_addr_page = PHY_BASEADDR_CLOCK_GATE;
	map_size	   = sizeof(u32);
	mem_mapped = ioremap_nocache(phys_addr_page, map_size);
	if (NULL != mem_mapped)
	{
		unsigned int read_val = ioread32((u8*)mem_mapped);
		MALI_DBG("setting ClockGen, set 0\n");
		iowrite32(read_val & ~0x3, ((u8*)mem_mapped));
	}
	iounmap(mem_mapped);
	mdelay(1);

	//ready
	phys_addr_page = PHY_BASEADDR_POWER_GATE;
	map_size	   = sizeof(u32);
	mem_mapped = ioremap_nocache(phys_addr_page, map_size);
	if (NULL != mem_mapped)
	{
		MALI_DBG("setting PHY_BASEADDR_POWER_GATE, set 1\n");
		iowrite32(0x1, ((u8*)mem_mapped));
	}
	iounmap(mem_mapped);

	//enable ISolate
	phys_addr_page = PHY_BASEADDR_PMU_ISOLATE;
	map_size       = sizeof(u32);
	mem_mapped = ioremap_nocache(phys_addr_page, map_size);
	if (NULL != mem_mapped)
	{
		unsigned int read_val = ioread32((u8*)mem_mapped);
		MALI_DBG("setting PHY_BASEADDR_PMU_ISOLATE, set 0\n");
		iowrite32(read_val & ~1, ((u8*)mem_mapped));
	}
	iounmap(mem_mapped);

	//pre charge down
	phys_addr_page = PHY_BASEADDR_PMU_ISOLATE + 4;
	map_size       = sizeof(u32);
	mem_mapped = ioremap_nocache(phys_addr_page, map_size);
	if (NULL != mem_mapped)
	{
		unsigned int read_val = ioread32((u8*)mem_mapped);
		MALI_DBG("setting PHY_BASEADDR_PMU_ISOLATE+4, set 1\n");
		iowrite32(read_val | 1, ((u8*)mem_mapped));
	}
	iounmap(mem_mapped);
	mdelay(1);

	//powerdown
	phys_addr_page = PHY_BASEADDR_PMU_ISOLATE + 8;
	map_size       = sizeof(u32);
	mem_mapped = ioremap_nocache(phys_addr_page, map_size);
	if (NULL != mem_mapped)
	{
		unsigned int read_val = ioread32((u8*)mem_mapped);
		MALI_DBG("setting PHY_BASEADDR_PMU_ISOLATE+8, set 1\n");
		iowrite32(read_val | 1, ((u8*)mem_mapped));
	}
	iounmap(mem_mapped);
	mdelay(1);

	//wait ack
	MALI_DBG("read PHY_BASEADDR_PMU_ISOLATE + 0xC\n");
	phys_addr_page = PHY_BASEADDR_PMU_ISOLATE + 0xC;
	map_size       = sizeof(u32);
	mem_mapped = ioremap_nocache(phys_addr_page, map_size);
	do{
		unsigned int powerUpAck = ioread32((u8*)mem_mapped);
		MALI_DBG("Wait Power Down Ack(powerUpAck=0x%08x)\n", powerUpAck);
		if( (powerUpAck & 0x1) )
			break;
		MALI_DBG("Wait Power Down Ack(powerUpAck=0x%08x)\n", powerUpAck);
	}while(1);
	iounmap(mem_mapped);
}

static void nx_vr_power_up_all_s5p6818(void)
{
	u32 phys_addr_page, map_size;
	void *mem_mapped;

	//ready
	phys_addr_page = PHY_BASEADDR_POWER_GATE;
	map_size	   = sizeof(u32);
	mem_mapped = ioremap_nocache(phys_addr_page, map_size);
	if (NULL != mem_mapped)
	{
		MALI_DBG("setting PHY_BASEADDR_POWER_GATE, set 1\n");
		iowrite32(0x1, ((u8*)mem_mapped));
	}
	iounmap(mem_mapped);

	//pre charge up
	phys_addr_page = PHY_BASEADDR_PMU_ISOLATE + 4;
	map_size	   = sizeof(u32);
	mem_mapped = ioremap_nocache(phys_addr_page, map_size);
	if (NULL != mem_mapped)
	{
		unsigned int read_val = ioread32((u8*)mem_mapped);
		MALI_DBG("setting PHY_BASEADDR_PMU_ISOLATE+4, set 0\n");
		iowrite32(read_val & ~0x1, ((u8*)mem_mapped));
	}
	iounmap(mem_mapped);

	//power up
	phys_addr_page = PHY_BASEADDR_PMU_ISOLATE + 8;
	map_size	   = sizeof(u32);
	mem_mapped = ioremap_nocache(phys_addr_page, map_size);
	if (NULL != mem_mapped)
	{
		unsigned int read_val = ioread32((u8*)mem_mapped);
		MALI_DBG("setting PHY_BASEADDR_PMU_ISOLATE+8, set 0\n");
		iowrite32(read_val & ~0x1, ((u8*)mem_mapped));
	}
	iounmap(mem_mapped);
	mdelay(1);

	//disable ISolate
	phys_addr_page = PHY_BASEADDR_PMU_ISOLATE;
	map_size	   = sizeof(u32);
	mem_mapped = ioremap_nocache(phys_addr_page, map_size);
	if (NULL != mem_mapped)
	{
		unsigned int read_val = ioread32((u8*)mem_mapped);
		MALI_DBG("setting PHY_BASEADDR_PMU_ISOLATE, set 1\n");
		iowrite32(read_val | 1, ((u8*)mem_mapped));
	}
	iounmap(mem_mapped);
	mdelay(1);

	//wait ack
	MALI_DBG("read PHY_BASEADDR_PMU_ISOLATE + 0xC\n");
	phys_addr_page = PHY_BASEADDR_PMU_ISOLATE + 0xC;
	map_size	   = sizeof(u32);
	mem_mapped = ioremap_nocache(phys_addr_page, map_size);
	do{
		unsigned int powerUpAck = ioread32((u8*)mem_mapped);
		MALI_DBG("Wait Power UP Ack(powerUpAck=0x%08x)\n", powerUpAck);
		if( !(powerUpAck & 0x1) )
			break;
		MALI_DBG("Wait Power UP Ack(powerUpAck=0x%08x)\n", powerUpAck);
	}while(1);

	//clk enable
	phys_addr_page = PHY_BASEADDR_CLOCK_GATE;
	map_size	   = sizeof(u32);
	mem_mapped = ioremap_nocache(phys_addr_page, map_size);
	if (NULL != mem_mapped)
	{
		unsigned int read_val = ioread32((u8*)mem_mapped);
		MALI_DBG("setting ClockGen, set 1\n");
		iowrite32(0x3 | read_val, ((u8*)mem_mapped));
	}
	iounmap(mem_mapped);

	//reset
	phys_addr_page = PHY_BASEADDR_RESET + 8;
	map_size	   = sizeof(u32);
	mem_mapped = ioremap_nocache(phys_addr_page, map_size);
	if (NULL != mem_mapped)
	{
		unsigned int temp32 = ioread32(((u8*)mem_mapped));
		unsigned int bit_mask = 1<<1; //65th
		MALI_DBG("setting Reset VR addr(0x%x)\n", (int)mem_mapped);

		//reset leave
		nx_vr_power_up_leave_reset_s5p6818();
		temp32 |= bit_mask;
		iowrite32(temp32, ((u8*)mem_mapped));
	}
	iounmap(mem_mapped);
	mdelay(1);

	//mask vr400 PMU interrupt
	phys_addr_page = PHY_BASEADDR_MALI_PMU + 0xC;
	map_size	   = sizeof(u32);
	mem_mapped = ioremap_nocache(phys_addr_page, map_size);
	if (NULL != mem_mapped)
	{
		MALI_PM_DBG("mask PMU INT, addr(0x%x)\n", (int)mem_mapped);
		iowrite32(0x0, ((u8*)mem_mapped));
	}
	iounmap(mem_mapped);

	//power up vr400
	phys_addr_page = PHY_BASEADDR_MALI_PMU;
	map_size	   = sizeof(u32);
	mem_mapped = ioremap_nocache(phys_addr_page, map_size);
	if (NULL != mem_mapped)
	{
		MALI_DBG("setting PHY_BASEADDR_MALI_PMU addr(0x%x)\n", (int)mem_mapped);
		iowrite32(0x3F/*GP, L2C, PP0, PP1, PP2, PP3*/, ((u8*)mem_mapped));
	}
	iounmap(mem_mapped);
}
#endif
#endif

void nx_vr_power_down_all(void)
{
	#if defined( CONFIG_ARCH_S5P4418 )
	nx_vr_power_down_all_s5p4418();
	#elif defined(CONFIG_ARCH_S5P6818)
	nx_vr_power_down_all_s5p6818();
	#else
	printk("=============================================================\n");
	printk("ERROR!!! No Supported Platform\n");
	printk("=============================================================\n");
	#endif
}

void nx_vr_power_up_first(void)
{
#if defined( CONFIG_ARCH_S5P4418 )
	nx_vr_power_up_all_s5p4418();
#elif defined(CONFIG_ARCH_S5P6818)
	nx_vr_power_up_all_s5p6818();
#else
	printk("=============================================================\n");
	printk("ERROR!!! No Supported Platform\n");
	printk("=============================================================\n");
#endif
}

void nx_vr_power_up_all(void)
{
	#if defined( CONFIG_ARCH_S5P4418 )
	nx_vr_power_up_all_s5p4418();
	#elif defined(CONFIG_ARCH_S5P6818)
	nx_vr_power_up_all_s5p6818();
	#else
	printk("=============================================================\n");
	printk("ERROR!!! select platform type at build.sh(s5p4418 or s5p6818)\n");
	printk("=============================================================\n");
	#endif
}


#define S5P4418_DTK_3D_IRQ				(40)

static u32 mali_read_phys(u32 phys_addr);
static void mali_write_phys(u32 phys_addr, u32 value);


#ifdef CONFIG_RESERVED_MEM
extern unsigned long fb_start;
extern unsigned long fb_size;
#endif


_mali_osk_errcode_t mali_platform_init(void)
{
	/* NEXELL_FEATURE_PORTING */
	//added by nexell
	nx_vr_make_reg_virt_maps();
	nx_vr_power_up_first();
	//vr_pmu_powerup();
#if defined( CONFIG_ARCH_S5P4418 )
	MALI_PRINT(("MALI device driver loaded  for s5p4418\n"));
#elif defined( CONFIG_ARCH_S5P6818 )
	MALI_PRINT(("MALI device driver loaded  for s5p6818\n"));
#endif


    MALI_SUCCESS;
}

_mali_osk_errcode_t mali_platform_deinit(void)
{

	/* NEXELL_FEATURE_PORTING */
	//added by nexell
	/*
	this function occurs segfualt error in case of rmmod.
        but it's ok because nx_vr_power_down_all() will shut down all power.
	vr_pmu_powerdown();
	*/

	//added by nexell
	nx_vr_power_down_all();
	nx_vr_release_reg_virt_maps();
	MALI_PRINT(("MALI device driver unloaded\n"));

    MALI_SUCCESS;
}

static void mali_platform_device_release(struct device *device);


static struct resource mali_gpu_resources_m400_mp2[] =
{
	MALI_GPU_RESOURCES_MALI400_MP2_PMU(PHY_BASEADDR_VR, S5P4418_DTK_3D_IRQ,
			S5P4418_DTK_3D_IRQ, S5P4418_DTK_3D_IRQ, S5P4418_DTK_3D_IRQ,
			S5P4418_DTK_3D_IRQ, S5P4418_DTK_3D_IRQ)

};

static struct platform_device mali_gpu_device =
{
	.name = MALI_GPU_NAME_UTGARD,
	.id = 0,
	.dev		= {
		.coherent_dma_mask	= DMA_BIT_MASK(32),
		.release		= mali_platform_device_release,
	},
};

static struct mali_gpu_device_data mali_gpu_data =
{
	.shared_mem_size = 256*1024*1024
};

int mali_platform_device_register(void)
{
	int err = -1;

	MALI_DEBUG_PRINT(4, ("mali_platform_device_register() called\n"));

#ifdef CONFIG_FB_RESERVED_MEM
	mali_gpu_data.fb_start = fb_start;
	mali_gpu_data.fb_size = fb_size;
#endif

	mali_platform_init();

	
	MALI_DEBUG_PRINT(4, ("Registering Mali-400 MP2 device\n"));
		err = platform_device_add_resources(
				&mali_gpu_device,
				mali_gpu_resources_m400_mp2,
				ARRAY_SIZE(mali_gpu_resources_m400_mp2));

	if (0 == err)
	{
		err = platform_device_add_data(&mali_gpu_device, &mali_gpu_data, sizeof(mali_gpu_data));
		if (0 == err)
		{
			/* Register the platform device */
			err = platform_device_register(&mali_gpu_device);
			if (0 == err)
			{
#ifdef CONFIG_PM_RUNTIME
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,37))
				pm_runtime_set_autosuspend_delay(&(mali_gpu_device.dev), 1000);
				pm_runtime_use_autosuspend(&(mali_gpu_device.dev));
#endif
				pm_runtime_enable(&(mali_gpu_device.dev));
#endif

				return 0;
			}
		}

		platform_device_unregister(&mali_gpu_device);
	}

	return err;
}

void mali_platform_device_unregister(void)
{
	MALI_DEBUG_PRINT(4, ("mali_platform_device_unregister() called\n"));

	platform_device_unregister(&mali_gpu_device);
	mali_platform_deinit();
}

static void mali_platform_device_release(struct device *device)
{
	MALI_DEBUG_PRINT(4, ("mali_platform_device_release() called\n"));
}

static u32 mali_read_phys(u32 phys_addr)
{
	u32 phys_addr_page = phys_addr & 0xFFFFE000;
	u32 phys_offset    = phys_addr & 0x00001FFF;
	u32 map_size       = phys_offset + sizeof(u32);
	u32 ret = 0xDEADBEEF;
	void *mem_mapped = ioremap_nocache(phys_addr_page, map_size);
	if (NULL != mem_mapped)
	{
		ret = (u32)ioread32(((u8*)mem_mapped) + phys_offset);
		iounmap(mem_mapped);
	}

	return ret;
}


static void mali_write_phys(u32 phys_addr, u32 value)
{
	u32 phys_addr_page = phys_addr & 0xFFFFE000;
	u32 phys_offset    = phys_addr & 0x00001FFF;
	u32 map_size       = phys_offset + sizeof(u32);
	void *mem_mapped = ioremap_nocache(phys_addr_page, map_size);
	if (NULL != mem_mapped)
	{
		iowrite32(value, ((u8*)mem_mapped) + phys_offset);
		iounmap(mem_mapped);
	}
}

