#include <i86.h>
#include <stdint.h>
#include "biosint.h"
#include "inlines.h"
#include <string.h>
#include <stdbool.h>

typedef struct {
	uint16_t es;
	uint16_t ds;
	uint16_t di;
	uint16_t si;
	uint16_t bp;
	uint16_t bx;
	uint16_t dx;
	uint16_t cx;
	uint16_t ax;
	uint16_t f;
	uint16_t ph1;
	uint16_t ph2;
	uint16_t ip;
	uint16_t cs;
	uint16_t rf;
} IRQ_DATA;

int __cdecl start(uint16_t irq, IRQ_DATA far * params);

void __cdecl install13h();

#define CH375_CMD_ADDR 0x261
#define CH375_DATA_ADDR 0x260

#if 0
void uglydelay(uint16_t loops);
#pragma aux  uglydelay  =   \
    "again: dec ax"         \
    "jnz again"             \
    parm [ax] modify exact [] nomemory;
// cycles 8088 3+16 = 19
//         V20 2+14 = 16 // 40ms is 0x2e95 exactly so we are good

#define MS_TO_LOOPS(ms) ms*299 // 298.125 exacly but we want int
#endif

uint8_t get_status()
{
	outb(CH375_CMD_ADDR,0x22);//GET_STATUS
	return inb(CH375_DATA_ADDR);
}

void wfi()
{
	while(inb(CH375_CMD_ADDR)&0x80);
}

void restart_ch()
{
	outb(CH375_CMD_ADDR,0x05);//reset
	while(inb(CH375_CMD_ADDR)!=0x80); // hack to not use delay
	outb(CH375_CMD_ADDR,0x15);//SET_USB_MODE
	outb(CH375_DATA_ADDR,0x06);//host autosof
	uint8_t s;
	do{
		s= get_status();
	}while(s!=0x15 && s!=0x16); // hack to not use delay
	outb(CH375_CMD_ADDR,0x51);//DISK_INIT
	wfi();
	get_status();
}

uint32_t to_lba(IRQ_DATA far * params)
{
	uint32_t c = (params->cx>>8) | ((params->cx &0xC0)<<(8-6));
	uint32_t h = (params->dx &0xff00)>>8;
	uint32_t s = params->cx & 0x3f;
	return ((c*255+h)*63+ s-1);
}

bool checkdisk()
{
	while(1)
	{
		uint8_t stat = get_status();
		if(stat==0x15)
		{
			restart_ch();
			continue;
		}
		if(stat==0x16)
		{
			return false;
		}
		break;
	}
	return true;
}

bool readdisk(uint32_t lba, uint8_t far * data,uint8_t len)
{
	outb(CH375_CMD_ADDR,0x54);//DISK_READ
	outb(CH375_DATA_ADDR,lba);
	outb(CH375_DATA_ADDR,lba>>8);
	outb(CH375_DATA_ADDR,lba>>16);
	outb(CH375_DATA_ADDR,lba>>24);
	outb(CH375_DATA_ADDR,len);
	uint16_t l2=((uint16_t)len)*8;
	while(l2--)
	{
		wfi();
		if(get_status() !=0x1d)
		{
			return false;
		}
		outb(CH375_CMD_ADDR,0x28);//RD_USB_DATA
		inb(CH375_DATA_ADDR); // should always get 64
		for(uint8_t i=0;i<64;i++)
			*data++=inb(CH375_DATA_ADDR);
		outb(CH375_CMD_ADDR,0x55);//DISK_RD_GO
	}
	wfi();
	if(get_status() !=0x14)
	{
		return false;
	}
	return true;
}

uint32_t get_max_lba()
{
	outb(CH375_CMD_ADDR,0x53);//DISK_SIZE
	wfi();
	uint8_t stat = get_status();
	if(stat!=0x14)
	{
		return 0xffffffff; //error
	}
	outb(CH375_CMD_ADDR,0x28);//RD_USB_DATA
	inb(CH375_DATA_ADDR); // should always get 8
	uint32_t s24=inb(CH375_DATA_ADDR);
	uint32_t s16=inb(CH375_DATA_ADDR);
	uint32_t s8=inb(CH375_DATA_ADDR);
	uint32_t s0=inb(CH375_DATA_ADDR);
	inb(CH375_DATA_ADDR); // should check
	inb(CH375_DATA_ADDR); // if it is really 512
	inb(CH375_DATA_ADDR);
	inb(CH375_DATA_ADDR);
	uint32_t lba = (s24<<24) | (s16<<16) |
			       (s8 << 8) | (s0 << 0);
	return lba;
}

int start(uint16_t irq, IRQ_DATA far * params)
{
	(void) params;
	if (irq==0)
	{
		bios_printf(BIOS_PRINTF_ALL,"CH375 BIOS by Kaede BETA!!!\n");
		outb(CH375_CMD_ADDR,0x06);//check exists
		outb(CH375_DATA_ADDR,0xA5);
		if(inb(CH375_DATA_ADDR)!=0x5A)
		{
			bios_printf(BIOS_PRINTF_ALL,"CH375 not found\n");
			return 1;
		}
		bios_printf(BIOS_PRINTF_ALL,"CH375 found\n");
		restart_ch();
		if(get_status() !=0x14)
		{
			bios_printf(BIOS_PRINTF_ALL,"Disk not found\n");
			return 1;
		}
		bios_printf(BIOS_PRINTF_ALL,"Disk found! Installing\n");
		uint8_t __far * bdahdcount = 0x0000:>(uint8_t __far*)0x475;
		*bdahdcount+=1;
		install13h();
		return 1;
	}
	else
	{
		if(((uint8_t)params->dx) != 0x80) // not our drive
		{
			return 1;
		}
		bios_printf(BIOS_PRINTF_ALL,"but %x\n",(params->ax>>8));

		switch(params->ax>>8)
		{
		default:
			bios_printf(BIOS_PRINTF_ALL,"bu %x\n",(params->ax>>8));
			params->ax = 0x0101;
			params->rf |= 0x0001; //cf failure
			return 0;
			break;
		case 0x00:
		case 0x01:
		case 0x0d:
			params->ax = 0;
			break;
		case 0x02:
		{
			if(!checkdisk())
			{
				params->ax = 0x0600;
				params->rf |= 0x0001; //cf failure
				return 0;
			}
			uint8_t len = params->ax;
			uint32_t lba = to_lba(params);
			__segment es = params->es;
			uint8_t __far * data = es:>(uint8_t __far*)params->bx;
			if(!readdisk(lba,data,len))
			{
				params->ax = 0x4000;
				params->rf |= 0x0001; //cf failure
				return 0;
			}
			break;
		}
		case 0x08:
		{
			if(!checkdisk())
			{
				params->ax = 0x0600;
				params->rf |= 0x0001; //cf failure
				return 0;
			}
			uint32_t lba = get_max_lba();
			if (lba == 0xffffffff)
			{
				params->ax = 0x0600;
				params->rf |= 0x0001; //cf failure
				return 0;
			}
			uint32_t cyls = lba / (63*255);
			if(cyls > 1024)
				cyls = 1024;
			cyls-=1;
			params->bx = 0;
			params->cx = ((cyls&0xff) <<8) | ((cyls&0x300)>>(8-6)) | 63;
			params->dx = (254<<8) | 0x01;
			params->di = 0;
			params->es = 0;
			break;
		}
		}
		params->ax = (params->ax & 0x00ff);
		params->rf &= ~0x0001; //cf success
		return 0;
	}
	return 1;
}
