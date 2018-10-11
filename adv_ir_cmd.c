#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <libusb-1.0/libusb.h>


#define VENDOR_ID  0x22ea
#define PRODUCT_ID 0x003a
#define BTO_EP_IN  0x84
#define BTO_EP_OUT 0x04

libusb_context *ctx = NULL; 
libusb_device_handle *devh = NULL;

#define BUFF_SIZE 64
#define IR_FREQ_DEFAULT					38000
static uint frequency = IR_FREQ_DEFAULT;

int trans_count = 0;
unsigned char trans_data[9600+1];	

int adv_open()
{
	int ret;
	
	ret = libusb_init(&ctx);
	if(ret < 0)
	{
		fprintf(stderr, "ERROR:libusb initialize failed. %s\n",libusb_error_name(ret));
		return -1;
	}
	
	libusb_set_debug(NULL, 3);
	
	devh = libusb_open_device_with_vid_pid(NULL, VENDOR_ID, PRODUCT_ID);
	if(!devh)
	{
		fprintf(stderr, "ERROR:Can't Open irtoy. Device not found.n");
		return -1;
	}
	
	for (int if_num = 3; if_num < 4; if_num++) 
	{
		if (libusb_kernel_driver_active(devh, if_num)) 
		{
			libusb_detach_kernel_driver(devh, if_num);
		}
		ret = libusb_claim_interface(devh, if_num);
		if (ret < 0) 
		{
			fprintf(stderr, "ERROR:Can't claim interface. %s\n",libusb_error_name(ret));
			return -1;
		}
	}
	
	return 0;
}

int adv_close() 
{
    if(!devh)
    {
        return -1;
    }
    libusb_close(devh);
    libusb_exit(ctx);
    ctx = NULL; 
    devh = NULL;
    return 0;
}

int adv_version()
{
	int ret,len;
	unsigned char obuf[BUFF_SIZE];
	unsigned char ibuf[BUFF_SIZE];

	obuf[0] = 0x56;
	ret = libusb_interrupt_transfer(devh, BTO_EP_OUT, obuf, 1 ,&len, 5000);
	if(ret != 0)
	{
		fprintf(stderr, "ERROR:Can't write get version. %s\n",libusb_error_name(ret));
		return -1;
	}
	ret = libusb_interrupt_transfer(devh, BTO_EP_IN, ibuf, BUFF_SIZE ,&len, 5000);
	if(ret != 0)
	{
		fprintf(stderr, "ERROR:Can't read  get version. %s\n",libusb_error_name(ret));
		return -1;
	}
	fprintf(stderr, "Firmware version %5s.\n",&ibuf[1]);
	return 0;
}

int adv_recv_start()
{
	int ret,len;
	unsigned char obuf[BUFF_SIZE];
	unsigned char ibuf[BUFF_SIZE];

	obuf[0] = 0x31;
    obuf[1] = (unsigned char)((frequency >> 8) & 0xFF);
    obuf[2] = (unsigned char)((frequency >> 0) & 0xFF);
    obuf[3] = 0;
    obuf[4] = 0;     // 読み込み停止ON時間MSB
    obuf[5] = 0;     // 読み込み停止ON時間LSB
    obuf[6] = 0;     // 読み込み停止OFF時間MSB
    obuf[7] = 0;     // 読み込み停止OFF時間LSB
	ret = libusb_interrupt_transfer(devh, BTO_EP_OUT, obuf, BUFF_SIZE ,&len, 5000);
	if(ret != 0)
	{
		fprintf(stderr, "ERROR:Can't write receord start. %s\n",libusb_error_name(ret));
		return -1;
	}
	ret = libusb_interrupt_transfer(devh, BTO_EP_IN, ibuf, BUFF_SIZE ,&len, 5000);
	if(ret != 0)
	{
		fprintf(stderr, "ERROR:Can't read receord start. %s\n",libusb_error_name(ret));
		return -1;
	}
	if((ibuf[0] != 0x31) || (ibuf[1] != 0x00))
	{
		fprintf(stderr, "ERROR:failed to receord start.\n");
		return -1;
	}
	return 0;
}

int adv_recv_stop()
{
	int ret,len;
	unsigned char obuf[BUFF_SIZE];
	unsigned char ibuf[BUFF_SIZE];

	obuf[0] = 0x32;
	ret = libusb_interrupt_transfer(devh, BTO_EP_OUT, obuf, BUFF_SIZE ,&len, 5000);
	if(ret != 0)
	{
			fprintf(stderr, "ERROR:Can't write receord stop. %s\n",libusb_error_name(ret));
			return -1;
	}
	ret = libusb_interrupt_transfer(devh, BTO_EP_IN, ibuf, BUFF_SIZE ,&len, 5000);
	if(ret != 0)
	{
			fprintf(stderr, "ERROR:Can't read receord stop. %s\n",libusb_error_name(ret));
			return -1;
	}
	if(ibuf[0] == 0x32) 
	{
		return 0;
	}
	fprintf(stderr, "ERROR:failed to receord stop.\n");
	return -1;
}

int adv_recv_read()
{
	int ret,len;
	unsigned char obuf[BUFF_SIZE];
	unsigned char ibuf[BUFF_SIZE];
	int all,off,sz;

	for(int i = 0;;i++)
	{
		obuf[0] = 0x33;
		ret = libusb_interrupt_transfer(devh, BTO_EP_OUT, obuf, BUFF_SIZE ,&len, 5000);
		if(ret != 0)
		{
				fprintf(stderr, "ERROR:Can't write receord read. %s\n",libusb_error_name(ret));
				return -1;
		}
		ret = libusb_interrupt_transfer(devh, BTO_EP_IN, ibuf, BUFF_SIZE ,&len, 5000);
		if(ret != 0)
		{
				fprintf(stderr, "ERROR:Can't read receord read. %s\n",libusb_error_name(ret));
				return -1;
		}
		if(ibuf[0] != 0x33) 
		{
			fprintf(stderr, "ERROR:failed to receord read.\n");
			return -1;
		}
		all = ((((int)ibuf[1]) & 0xff) << 8) + (((int)ibuf[2]) & 0xff);
		off = ((((int)ibuf[3]) & 0xff) << 8) + (((int)ibuf[4]) & 0xff);
		sz  = (((int)ibuf[5]) & 0xff);

		if(all == 0)
		{
			fprintf(stderr,"Timeout.\n");
			break;
		}

		if(all == off)
		{
			printf("\n");
			break;
		}
		if(sz != 0)
		{
			for(int j = 0;j < sz;j++)
			{
				for(int k = 0;k < 4;k++)
				{
					printf("0x%02x",((int)ibuf[6 + (j * 4) + k]) & 0xff);
					if(((off + sz) == all) && (j == (sz - 1)) && (k == 3))
					{
						break;
					}
					printf(",");
				}
			}
		}
	}
	return 0;
}

int adv_receive(int varbose)
{
	int res = -1;
	int ret;
	ret = adv_open();
	if(ret)
	{
		goto EXIT_PATH;
	}

	if(varbose)
	{
		ret = adv_version();
		if(ret)
		{
			goto EXIT_PATH;
		}
	}

	if(varbose)
	{
		fprintf(stderr,"receord start.\n");
	}
	ret = adv_recv_start();
	if(ret)
	{
		goto EXIT_PATH;
	}
	sleep(3);
	if(varbose)
	{
		fprintf(stderr,"receord stop.\n");
	}
	ret = adv_recv_stop();
	if(ret)
	{
		goto EXIT_PATH;
	}
	usleep(100 * 1000);
	if(varbose)
	{
		fprintf(stderr,"receord read.\n");
	}
	ret = adv_recv_read();
	if(ret)
	{
		goto EXIT_PATH;
	}
	res = 0;
EXIT_PATH:
	ret = adv_close();
	return res;
}

#define PACKET_MAX_NUM ((BUFF_SIZE - 5) / 4)
int _adv_transfer(unsigned char sdata[],int size,int varbose)
{
	int res = -1;
	int ret,len;
	unsigned char obuf[BUFF_SIZE];
	unsigned char ibuf[BUFF_SIZE];
	int all,sz;

	ret = adv_open();
	if(ret)
	{
		goto EXIT_PATH;
	}

	if(varbose)
	{
		ret = adv_version();
		if(ret)
		{
			goto EXIT_PATH;
		}
	}
	
	all = (size / 4) + ((size % 4) ? 1 : 0);
	for(int i = 0;i < all;i += PACKET_MAX_NUM)
	{
		if(varbose)
		{
			fprintf(stderr,"transfer data set.\n");
		}

		sz = ((i + PACKET_MAX_NUM) <= all) ? PACKET_MAX_NUM : (all - i);
		obuf[0] = 0x34;
		obuf[1] = (unsigned char)((all >> 8) & 0xff);
		obuf[2] = (unsigned char)((all >> 0) & 0xff);
		obuf[3] = (unsigned char)((i >> 8) & 0xff);
		obuf[4] = (unsigned char)((i >> 0) & 0xff);
		obuf[5] = (unsigned char)sz;

		for(int j = 0;j < sz;j++)
		{
			for(int k = 0;k < 5;k++)
			{
				obuf[6 + (j * 4) + k] = sdata[((i + j) * 4) + k];
			}
		}
		ret = libusb_interrupt_transfer(devh, BTO_EP_OUT, obuf, BUFF_SIZE ,&len, 5000);
		if(ret != 0)
		{
			fprintf(stderr, "ERROR:Can't write transfer data set. %s\n",libusb_error_name(ret));
			goto EXIT_PATH;
		}
		ret = libusb_interrupt_transfer(devh, BTO_EP_IN, ibuf, BUFF_SIZE ,&len, 5000);
		if(ret != 0)
		{
			fprintf(stderr, "ERROR:Can't read transfer data set. %s\n",libusb_error_name(ret));
			goto EXIT_PATH;
		}
		if((ibuf[0] != 0x34) || (ibuf[1] != 0x00)) 
		{
			fprintf(stderr, "ERROR:failed to transfer data set.\n");
			goto EXIT_PATH;
		}
	}

	if(varbose)
	{
		fprintf(stderr,"transfer execution.\n");
	}
	obuf[0] = 0x35;
	obuf[1] = (unsigned char)((frequency >> 8) & 0xff);
	obuf[2] = (unsigned char)((frequency >> 0) & 0xff);
	obuf[3] = (unsigned char)((all >> 8) & 0xff);
	obuf[4] = (unsigned char)((all >> 0) & 0xff);
	ret = libusb_interrupt_transfer(devh, BTO_EP_OUT, obuf, BUFF_SIZE ,&len, 5000);
	if(ret != 0)
	{
			fprintf(stderr, "ERROR:Can't write transfer execution %s\n",libusb_error_name(ret));
		goto EXIT_PATH;
	}
	ret = libusb_interrupt_transfer(devh, BTO_EP_IN, ibuf, BUFF_SIZE ,&len, 5000);
	if(ret != 0)
	{
			fprintf(stderr, "ERROR:Can't read transfer execution. %s\n",libusb_error_name(ret));
		goto EXIT_PATH;
	}
	if((ibuf[0] != 0x35) || (ibuf[1] != 0x00)) 
	{
		fprintf(stderr, "ERROR:failed to transfer execution.\n");
		goto EXIT_PATH;
	}
	res = 0;
EXIT_PATH:
	ret = adv_close();
	return 0;
}

int hex2ary(const char* hex)
{
	int ret,len;
	unsigned int x;
	char c;
	char tmp[16+1];
	
	trans_count = 0;
	memset(trans_data,0,sizeof(trans_data));
	
	memset(tmp,0,sizeof(tmp));
	len = strlen(hex);
	for(int i = 0;i < (len + 1);i++)
	{
		if((*hex == ',') || 
		   i == len)
		{
			if(strlen(tmp))
			{
				ret = sscanf(tmp,"%x%c",&x,&c);
				if(ret == 1)
				{
					trans_data[trans_count] = (unsigned char)x;
					trans_count++;
					if(trans_count >= sizeof(trans_data))
					{
						fprintf(stderr,"ERROR:Hex string is too long.\n");
						return -1;
					}
					memset(tmp,0,sizeof(tmp));
				}
				else
				{
					fprintf(stderr,"ERROR:Incorrect hex number. %s\n",tmp);
					return -1;
				}
			}
		}
		else
		{
			int len = strlen(tmp);
			tmp[len] = *hex;
			if(len >= 16)
			{
				fprintf(stderr,"ERROR:Hex digit is too long.\n");
				return -1;
			}
		}
		hex++;
	}
	fprintf(stderr,"%d bytes\n",trans_count);
	return 0;
}

int adv_transfer(char* hex,int varbose)
{
	int ret;
	ret = hex2ary(hex);
	if(ret)
	{
		return -1;
	}
	ret = _adv_transfer(trans_data, trans_count, varbose);
	return ret;
}


void usage() 
{
  fprintf(stderr, "usage: adv_ir_cmd <option>\n");
  fprintf(stderr, "  -r       \tReceive Infrared code.\n");
  fprintf(stderr, "  -t 'hex'\tTransfer Infrared code.\n");
  fprintf(stderr, "  -t \"$(cat XXX.txt)\"\n");
  fprintf(stderr, "  -t \"`cat XXX.txt`\"\n");
  fprintf(stderr, "  -v       \t varbose mode.\n");
}

int main(int argc,char* argv[])
{
	int ret = -1;
	int varbose = 0;
	char cmd = '\0';
	char* hex = NULL;
	
	int opt;
	while ((opt = getopt(argc, argv, "vrt:")) != -1) 
	{
        switch (opt) 
		{
            case 'r':
				cmd = opt;
                break;
            case 'v':
				varbose = 1;
                break;
            case 't':
				cmd = opt;
				hex = optarg;
                break;
        }
    }

	if(cmd == '\0')
	{
		usage();
		return 0;
	}
	
	switch(cmd)
	{
		case 'r': 
			ret = adv_receive(varbose);
			break;
		case 't': 
			ret = adv_transfer(hex,varbose);
			break;
	}
	
	return 0;
}

