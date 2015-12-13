#ifndef SENSOR_API_H__
#define SENSOR_API_H__

#include <usb.h>

#define MALLOC(n) malloc_wrapper((n), __FILE__, __LINE__)
 
struct usb_temper
{
    usb_dev_handle* usb_handle;
    int             devicenum;
};

typedef struct usb_temper* usb_temper_t;

usb_temper_t usb_temper_init(int devicenum);
float        usb_temper_get_tempc(usb_temper_t usb_temper);
int          usb_temper_finish(usb_temper_t* usb_temper);

void* malloc_wrapper(size_t nbytes, const char* file, int line);

#endif // SENSOR_API_H__

