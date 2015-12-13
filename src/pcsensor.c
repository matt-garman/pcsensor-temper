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



#include <stdio.h>
#include <time.h>
#include <ctype.h>
#include <string.h>
#include <signal.h> 

#include "sensorapi.h"

#define VERSION "1.0.0"
 
static int bsalir=1;
static int debug=0;
static int seconds=5;
static int formato=0;
static int mrtg=0;
static int calibration=0;

 
void ex_program(int sig) {
      bsalir=1;
 
      (void) signal(SIGINT, SIG_DFL);
}
 
int main( int argc, char **argv) {
 
     //usb_dev_handle *lvr_winusb = NULL;
     usb_temper_t usb_temper;
     float tempc;
     int c;
     struct tm *local;
     time_t t;
     int devicenum = 0;

     while ((c = getopt (argc, argv, "mfcvhn:l::a:")) != -1)
     switch (c)
       {
       case 'v':
         debug = 1;
         break;
       case 'n':
         if (optarg != NULL) {
           if (!sscanf(optarg,"%i",&devicenum)==1) {
             fprintf (stderr, "Error: '%s' is not numeric.\n", optarg);
             exit(EXIT_FAILURE);
           }
         }
         break;
       case 'c':
         formato=1; //Celsius
         break;
       case 'f':
         formato=2; //Fahrenheit
         break;
       case 'm':
         mrtg=1;
         break;
       case 'l':
         if (optarg!=NULL){
           if (!sscanf(optarg,"%i",&seconds)==1) {
             fprintf (stderr, "Error: '%s' is not numeric.\n", optarg);
             exit(EXIT_FAILURE);
           } else {           
              bsalir = 0;
              break;
           }
         } else {
           bsalir = 0;
           seconds = 5;
           break;
         }
       case 'a':
         if (!sscanf(optarg,"%i",&calibration)==1) {
             fprintf (stderr, "Error: '%s' is not numeric.\n", optarg);
             exit(EXIT_FAILURE);
         } else {           
              break;
         }
       case '?':
       case 'h':
         printf("pcsensor version %s\n",VERSION);
	 printf("      Aviable options:\n");
	 printf("          -h help\n");
	 printf("          -v verbose\n");
	 printf("          -n[i] use device number i (0 is the first one found on the bus)\n");
	 printf("          -l[n] loop every 'n' seconds, default value is 5s\n");
	 printf("          -c output only in Celsius\n");
	 printf("          -f output only in Fahrenheit\n");
	 printf("          -a[n] increase or decrease temperature in 'n' degrees for device calibration\n");
	 printf("          -m output for mrtg integration\n");
  
	 exit(EXIT_FAILURE);
       default:
         if (isprint (optopt))
           fprintf (stderr, "Unknown option `-%c'.\n", optopt);
         else
           fprintf (stderr,
                    "Unknown option character `\\x%x'.\n",
                    optopt);
         exit(EXIT_FAILURE);
       }

     if (optind < argc) {
        fprintf(stderr, "Non-option ARGV-elements, try -h for help.\n");
        exit(EXIT_FAILURE);
     }

 
     //if ((lvr_winusb = setup_libusb_access(devicenum)) == NULL) {
     //    exit(EXIT_FAILURE);
     //} 

     usb_temper = usb_temper_init(devicenum);

     (void) signal(SIGINT, ex_program);

     //ini_control_transfer(lvr_winusb);
     // 
     //control_transfer(lvr_winusb, uTemperatura );
     //interrupt_read(lvr_winusb);
 
     //control_transfer(lvr_winusb, uIni1 );
     //interrupt_read(lvr_winusb);
 
     //control_transfer(lvr_winusb, uIni2 );
     //interrupt_read(lvr_winusb);
     //interrupt_read(lvr_winusb);


 
     do {
           //control_transfer(lvr_winusb, uTemperatura );
           //interrupt_read_temperatura(lvr_winusb, &tempc);
           tempc = usb_temper_get_tempc(usb_temper);

           t = time(NULL);
           local = localtime(&t);

           if (mrtg) {
              if (formato==2) {
                  printf("%.2f\n", (9.0 / 5.0 * tempc + 32.0));
                  printf("%.2f\n", (9.0 / 5.0 * tempc + 32.0));
              } else {
                  printf("%.2f\n", tempc);
                  printf("%.2f\n", tempc);
              }
              
              printf("%02d:%02d\n", 
                          local->tm_hour,
                          local->tm_min);

              printf("pcsensor\n");
           } else {
              printf("%04d/%02d/%02d %02d:%02d:%02d ", 
                          local->tm_year +1900, 
                          local->tm_mon + 1, 
                          local->tm_mday,
                          local->tm_hour,
                          local->tm_min,
                          local->tm_sec);

              if (formato==2) {
                  printf("Temperature %.2fF\n", (9.0 / 5.0 * tempc + 32.0));
              } else if (formato==1) {
                  printf("Temperature %.2fC\n", tempc);
              } else {
                  printf("Temperature %.2fF %.2fC\n", (9.0 / 5.0 * tempc + 32.0), tempc);
              }
           }
           
           if (!bsalir)
              sleep(seconds);
     } while (!bsalir);
                                       
     //usb_release_interface(lvr_winusb, USB_TEMPER_INTERFACE1);
     //usb_release_interface(lvr_winusb, USB_TEMPER_INTERFACE2);
     
     //usb_close(lvr_winusb); 

     usb_temper_finish(&usb_temper);
      
     return 0; 
}
