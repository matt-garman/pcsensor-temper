/*
 * pcsensor_sqlite.c by Matt Garman (c) 2015 (matthew.garman@gmail.com)
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
 * THIS SOFTWARE IS PROVIDED BY Philipp Adelt (and other contributors) ''AS
 * IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL Philipp Adelt (or other
 * contributors) BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */



#include <stdio.h>
#include <time.h>
#include <ctype.h>
#include <string.h>
#include <signal.h> 
#include <sqlite3.h>

#include "sensorapi.h"


#define BUFLEN (128)
#define DEFAULT_SLEEP_SECS (300)

#define SQLITE_DB_SCHEMA                                                    \
"CREATE TABLE IF NOT EXISTS temps(\n"                                       \
"    id          INTEGER PRIMARY KEY ASC,\n"                                \
"    timestamp   INTEGER NOT NULL, /* unix timestamp from time() call */\n" \
"    tempc       REAL    NOT NULL  /* temperature in celsius */\n"          \
"    );\n"
 

char* timestamp()
{
    static char ts_buf[32];

    time_t t;
    time(&t);
    struct tm tm;
    localtime_r(&t, &tm);
    strftime(ts_buf, 32, "%Y%m%d-%H:%M:%S", &tm);

    return ts_buf;
}

int log_to_sqlite(const char *fn, time_t ts, float tempc)
{
    char      sql[BUFLEN];
    int       rc       = 0;
    sqlite3  *db       = NULL;
    char     *err_msg  = NULL;
    int       try      = 0;
    const int maxtries = 10;

    rc = sqlite3_open(fn, &db);
    if (SQLITE_OK != rc)
    {
        fprintf(stderr,
                "[%s] ERROR: sqlite3_open() failure, rc=%d\n",
                timestamp(), rc);
        return 0;
    }

    rc = snprintf(sql, BUFLEN, 
            "INSERT INTO temps (timestamp,tempc) VALUES (%ld, %lf);",
            (long int)ts, tempc);
    if (rc >= BUFLEN || rc < 1)
    {
        fprintf(stderr,
                "[%s] ERROR: snprintf() failure, rc=%d\n",
                timestamp(), rc);
        rc = 0;
        goto SQLITE3_CLOSE_LABEL;
    }

    do
    {
        if ( SQLITE_OK == (rc = sqlite3_exec(db, sql, 0, 0, &err_msg)) )
        {
            rc = 1;
            goto SQLITE3_CLOSE_LABEL;
        }
        else
        {
            ++try;
            fprintf(stderr,
                    "[%s] ERROR: sqlite3_exec() failure, "
                    "try=%i/%i, rc=%d, err_msg=%s\n",
                    timestamp(), try, maxtries, rc, err_msg);
            sqlite3_free(err_msg);
            rc = 0;
            sleep(1);
        }
    }
    while (try<maxtries);

SQLITE3_CLOSE_LABEL:
    sqlite3_close(db);
    return rc;
}


static int LOOP = 1;
 
void signal_handler(int sig)
{
    LOOP=1;
    (void) signal(SIGINT, SIG_DFL);
}

void usage(const char* progname)
{
    printf(
            "USAGE: %s [-h] [-v] [-S] [-l sleep_secs] [-a calibration] <-f sqlite_file>\n"
            "  ARGUMENTS:\n"
            "    -f SQLITE_FILE ... path to sqlite3 database file\n"
            "  OPTIONS:\n"
            "    -h ............... show this help\n"
            "    -v ............... verbose/debug mode\n"
            "    -l SLEEP_SECS .... loop every SLEEP_SECS seconds, default\n"
            "                       is %d\n"
            "    -a CALIBRATION ... increase or decrease temperature by\n"
            "                       CALIBRATION degrees for device\n"
            "                       calibration\n"
            "    -S ............... print sqlite3 DB schema and exit\n"
            "                       init DB with:\n"
            "                       %s -S | sqlite3 SQLITE_FILE\n"
            , progname
            , DEFAULT_SLEEP_SECS
            , progname
          );
}

int main( int argc, char **argv)
{
    usb_temper_t usb_temper;
    float tempc;
    int c;
    struct tm *local;
    time_t t;
    int devicenum = 0;
    int debug=0;
    int seconds = DEFAULT_SLEEP_SECS;
    int calibration = 0;
    char *sqlite_file = NULL;

    while ((c = getopt (argc, argv, "hvSf:l:a:")) != -1)
    {
        switch (c)
        {
            case 'v': debug = 1; break;
            case 'l': seconds = atoi(optarg); LOOP = 0; break;
            case 'a': calibration = atoi(optarg); break;
            case 'f': sqlite_file = optarg; break;
            case 'S': printf(SQLITE_DB_SCHEMA "\n"); exit(0); break;
            case '?':
            case 'h': usage(argv[0]); exit(0);
            default:
                fprintf(stderr, 
                        "ERROR: unknown option %c, use -h for help\n", c);
                exit(EXIT_FAILURE);
        }
    }

    if (optind < argc)
    {
        fprintf(stderr, "Non-option ARGV-elements, try -h for help.\n");
        exit(EXIT_FAILURE);
    }

    usb_temper = usb_temper_init(devicenum, debug, calibration);

    (void) signal(SIGINT, signal_handler);


    do {
        tempc = usb_temper_get_tempc(usb_temper);

        t = time(NULL);
        local = localtime(&t);

        if (sqlite_file)
        {
            if (!log_to_sqlite(sqlite_file, t, tempc))
            {
                LOOP = 1;
            }
        }
        else
        {
            printf("%04d/%02d/%02d %02d:%02d:%02d ", 
                    local->tm_year +1900, 
                    local->tm_mon + 1, 
                    local->tm_mday,
                    local->tm_hour,
                    local->tm_min,
                    local->tm_sec);

            printf("Temperature %.2fF %.2fC\n",
                    DEG_C_TO_DEG_F(tempc), tempc);
        }

        if (!LOOP) { sleep(seconds); }
    }
    while (!LOOP);

    usb_temper_finish(&usb_temper);

    return 0; 
}

