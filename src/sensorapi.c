/*
 * pcsensor.c by Philipp Adelt (c) 2012 (info@philipp.adelt.net)
 * based on Juan Carlos Perez (c) 2011 (cray@isp-sl.com)
 * based on Temper.c by Robert Kavaler (c) 2009 (relavak.com)
 * All rights reserved.
 *
 * Temper driver for linux. This program can be compiled either as a library
 * or as a standalone program (-DUNIT_TEST). The driver will work with some
 * TEMPer usb devices from RDing (www.PCsensor.com).
 *
 * This driver works with USB devices presenting ID 0c45:7401.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 * 
 * THIS SOFTWARE IS PROVIDED BY Philipp Adelt (and other contributors) ''AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL Philipp Adelt (or other contributors) BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * 
 */


#include "sensorapi.h"

#include <stdio.h>
#include <string.h>
#include <errno.h>


#define USB_TEMPER_INTERFACE1 (0x00)
#define USB_TEMPER_INTERFACE2 (0x01)
#define USB_TEMPER_VENDOR_ID  (0x0c45)
#define USB_TEMPER_PRODUCT_ID (0x7401)

 
static const char uTemperatura[] = { 0x01, 0x80, 0x33, 0x01, 0x00, 0x00, 0x00, 0x00 };
static const char uIni1[] = { 0x01, 0x82, 0x77, 0x01, 0x00, 0x00, 0x00, 0x00 };
static const char uIni2[] = { 0x01, 0x86, 0xff, 0x01, 0x00, 0x00, 0x00, 0x00 };

 
const static int reqIntLen=8;
const static int reqBulkLen=8;
const static int endpoint_Int_in=0x82; /* endpoint 0x81 address for IN */
const static int endpoint_Int_out=0x00; /* endpoint 1 address for OUT */
const static int endpoint_Bulk_in=0x82; /* endpoint 0x81 address for IN */
const static int endpoint_Bulk_out=0x00; /* endpoint 1 address for OUT */
const static int timeout=5000; /* timeout in ms */
 
static int debug=0;
static int calibration=0;


void bad(const char *why) {
        fprintf(stderr,"Fatal error> %s\n",why);
        exit(17);
}
 
 
usb_dev_handle *find_lvr_winusb();
 
void usb_detach(usb_dev_handle *lvr_winusb, int iInterface) {
        int ret;
 
	ret = usb_detach_kernel_driver_np(lvr_winusb, iInterface);
	if(ret) {
		if(errno == ENODATA) {
			if(debug) {
				printf("Device already detached\n");
			}
		} else {
			if(debug) {
				printf("Detach failed: %s[%d]\n",
				       strerror(errno), errno);
				printf("Continuing anyway\n");
			}
		}
	} else {
		if(debug) {
			printf("detach successful\n");
		}
	}
} 

usb_dev_handle* setup_libusb_access(int devicenum) {
     usb_dev_handle *lvr_winusb;

     if(debug) {
        usb_set_debug(255);
     } else {
        usb_set_debug(0);
     }
     usb_init();
     usb_find_busses();
     usb_find_devices();
             
 
     if(!(lvr_winusb = find_lvr_winusb(devicenum))) {
                printf("Couldn't find the USB device, Exiting\n");
                return NULL;
        }
        
        
        usb_detach(lvr_winusb, USB_TEMPER_INTERFACE1);
        

        usb_detach(lvr_winusb, USB_TEMPER_INTERFACE2);
        
 
        if (usb_set_configuration(lvr_winusb, 0x01) < 0) {
                printf("Could not set configuration 1\n");
                return NULL;
        }
 

        // Microdia tiene 2 interfaces
        if (usb_claim_interface(lvr_winusb, USB_TEMPER_INTERFACE1) < 0) {
                printf("Could not claim interface\n");
                return NULL;
        }
 
        if (usb_claim_interface(lvr_winusb, USB_TEMPER_INTERFACE2) < 0) {
                printf("Could not claim interface\n");
                return NULL;
        }
 
        return lvr_winusb;
}
 
 
 
usb_dev_handle *find_lvr_winusb(int devicenum) {
        // iterates to the devicenum'th device for installations with multiple sensors
        struct usb_bus *bus;
        struct usb_device *dev;
 
        for (bus = usb_busses; bus; bus = bus->next) {
        for (dev = bus->devices; dev; dev = dev->next) {
                        if (dev->descriptor.idVendor == USB_TEMPER_VENDOR_ID && 
                                dev->descriptor.idProduct == USB_TEMPER_PRODUCT_ID ) {
                                if (devicenum>0) {
                                  devicenum--;
                                  continue;
                                }
                                usb_dev_handle *handle;
                                if(debug) {
                                  printf("lvr_winusb with Vendor Id: %x and Product Id: %x found.\n", USB_TEMPER_VENDOR_ID, USB_TEMPER_PRODUCT_ID);
                                }
 
                                if (!(handle = usb_open(dev))) {
                                        printf("Could not open USB device\n");
                                        return NULL;
                                }
                                return handle;
                        }
                }
        }
        return NULL;
}
 
 
void ini_control_transfer(usb_dev_handle *dev) {
    int r,i;

    char question[] = { 0x01,0x01 };

    r = usb_control_msg(dev, 0x21, 0x09, 0x0201, 0x00, (char *) question, 2, timeout);
    if( r < 0 )
    {
          perror("USB control write"); bad("USB write failed"); 
    }


    if(debug) {
      for (i=0;i<reqIntLen; i++) printf("%02x ",question[i] & 0xFF);
      printf("\n");
    }
}
 
void control_transfer(usb_dev_handle *dev, const char *pquestion) {
    int r,i;

    char question[reqIntLen];
    
    memcpy(question, pquestion, sizeof question);

    r = usb_control_msg(dev, 0x21, 0x09, 0x0200, 0x01, (char *) question, reqIntLen, timeout);
    if( r < 0 )
    {
          perror("USB control write"); bad("USB write failed"); 
    }

    if(debug) {
        for (i=0;i<reqIntLen; i++) printf("%02x ",question[i]  & 0xFF);
        printf("\n");
    }
}

void interrupt_transfer(usb_dev_handle *dev) {
 
    int r,i;
    char answer[reqIntLen];
    char question[reqIntLen];
    for (i=0;i<reqIntLen; i++) question[i]=i;
    r = usb_interrupt_write(dev, endpoint_Int_out, question, reqIntLen, timeout);
    if( r < 0 )
    {
          perror("USB interrupt write"); bad("USB write failed"); 
    }
    r = usb_interrupt_read(dev, endpoint_Int_in, answer, reqIntLen, timeout);
    if( r != reqIntLen )
    {
          perror("USB interrupt read"); bad("USB read failed"); 
    }

    if(debug) {
       for (i=0;i<reqIntLen; i++) printf("%i, %i, \n",question[i],answer[i]);
    }
 
    usb_release_interface(dev, 0);
}

void interrupt_read(usb_dev_handle *dev) {
 
    int r,i;
    char answer[reqIntLen];
    bzero(answer, reqIntLen);
    
    r = usb_interrupt_read(dev, 0x82, answer, reqIntLen, timeout);
    if( r != reqIntLen )
    {
          perror("USB interrupt read"); bad("USB read failed"); 
    }

    if(debug) {
       for (i=0;i<reqIntLen; i++) printf("%02x ",answer[i]  & 0xFF);
    
       printf("\n");
    }
}

void interrupt_read_temperatura(usb_dev_handle *dev, float *tempC) {
 
    int r,i, temperature;
    char answer[reqIntLen];
    bzero(answer, reqIntLen);
    
    r = usb_interrupt_read(dev, 0x82, answer, reqIntLen, timeout);
    if( r != reqIntLen )
    {
          perror("USB interrupt read"); bad("USB read failed"); 
    }


    if(debug) {
      for (i=0;i<reqIntLen; i++) printf("%02x ",answer[i]  & 0xFF);
    
      printf("\n");
    }
    
    temperature = (answer[3] & 0xFF) + (answer[2] << 8);
    temperature += calibration;
    *tempC = temperature * (125.0 / 32000.0);

}

void bulk_transfer(usb_dev_handle *dev) {
 
    int r,i;
    char answer[reqBulkLen];

    r = usb_bulk_write(dev, endpoint_Bulk_out, NULL, 0, timeout);
    if( r < 0 )
    {
          perror("USB bulk write"); bad("USB write failed"); 
    }
    r = usb_bulk_read(dev, endpoint_Bulk_in, answer, reqBulkLen, timeout);
    if( r != reqBulkLen )
    {
          perror("USB bulk read"); bad("USB read failed"); 
    }


    if(debug) {
      for (i=0;i<reqBulkLen; i++) printf("%02x ",answer[i]  & 0xFF);
    }
 
    usb_release_interface(dev, 0);
}
 
void* malloc_wrapper(size_t n, const char* file, int line)
{
    void* ptr = malloc(n);
    if (NULL == ptr)
    {
        fprintf(stderr, "ERROR: malloc() failure %s:%d, abort\n", file, line);
        exit(-1);
    }

    return ptr;
}

usb_temper_t usb_temper_init(int devicenum)
{
    usb_dev_handle *usb_handle = setup_libusb_access(devicenum);
    if (NULL == usb_handle)
    {
        return NULL;
    }

    ini_control_transfer(usb_handle);
     
    control_transfer(usb_handle, uTemperatura );
    interrupt_read(usb_handle);
 
    control_transfer(usb_handle, uIni1 );
    interrupt_read(usb_handle);
 
    control_transfer(usb_handle, uIni2 );
    interrupt_read(usb_handle);
    interrupt_read(usb_handle);

    usb_temper_t usb_temper = MALLOC(sizeof(*usb_temper));
    usb_temper->usb_handle = usb_handle;
    usb_temper->devicenum  = devicenum;

    return usb_temper;
}

float usb_temper_get_tempc(usb_temper_t usb_temper)
{
    // return an impossible temperature (absolute zero = -273.15) if bad
    // inputs or some other failure
    float tempc = -9999.99;

    if (usb_temper && usb_temper->usb_handle)
    {
        control_transfer(usb_temper->usb_handle, uTemperatura );
        interrupt_read_temperatura(usb_temper->usb_handle, &tempc);
    }

    return tempc;
}

int usb_temper_finish(usb_temper_t* usb_temper)
{
    if (usb_temper && (*usb_temper) && (*usb_temper)->usb_handle)
    {
        usb_release_interface((*usb_temper)->usb_handle, USB_TEMPER_INTERFACE1);
        usb_release_interface((*usb_temper)->usb_handle, USB_TEMPER_INTERFACE2);
        usb_close((*usb_temper)->usb_handle); 
        free(*usb_temper);
        *usb_temper = NULL;
    }

    return 0;
}

