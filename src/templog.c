#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sqlite3.h>
#include <sys/stat.h>

#define BUFLEN (128)
#define DEFAULT_N_RECORDS (10)
#define DEG_C_TO_DEG_F(deg_c) ((9.0 / 5.0 * (deg_c) + 32.0))

typedef struct temp_db_data
{
    time_t ts;
    double tempc;
}
temp_db_data_t;

typedef struct stats
{
    size_t n;
    double min;
    time_t min_time;
    double max;
    time_t max_time;
    double med;
    double avg;
    double std;
}
stats_t;

int double_compare(const void* d1, const void* d2)
{
    return ( *((double*)d1) < *((double*)d2) );
}

int double_is_zero(double d)
{
    const static double double_zero = 0.0;
    return (0 == memcmp(&d, &double_zero, sizeof(double_zero)));
}

int calc_temp_stats(const temp_db_data_t *db_data, stats_t *stats)
{
    if (NULL == db_data || NULL == stats) { return 0; }

    stats->n        = 0;
    stats->min      = 999999.9;
    stats->min_time = 0;
    stats->max      = 0.0;
    stats->max_time = 0;
    stats->med      = 0;
    stats->avg      = 0;
    stats->std      = 0;

    for (   stats->n=0; 
            ( 0!=db_data[stats->n].ts && 
              !double_is_zero(db_data[stats->n].tempc) );
            ++stats->n)
    {
        double tempc = db_data[stats->n].tempc; // convenience variable
        time_t ts = db_data[stats->n].ts;       // convenience variable
        stats->avg += tempc;

        if (tempc > stats->max) { stats->max=tempc; stats->max_time=ts; }
        if (tempc < stats->min) { stats->min=tempc; stats->min_time=ts; }
    }

    stats->avg /= stats->n;

    double *sorted = malloc(stats->n * sizeof(double));

    size_t i;
    for (i=0; i<stats->n; ++i)
    {
        double tempc = db_data[i].tempc; // convenience variable
        sorted[i] = tempc;
        stats->std += ((tempc - stats->avg) * (tempc - stats->avg));
    }

    stats->std /= stats->n;
    stats->std = sqrt(stats->std);

    qsort(sorted, stats->n, sizeof(double), double_compare);

    //printf("DEBUG: sorted: ");
    //for (i=0; i<stats->n; ++i)
    //{
    //    printf("%.1lf, ", sorted[i]);
    //}
    //printf("\n");

    stats->med = sorted[stats->n/2];

    free(sorted);

    return 1;
}

int print_stats(FILE* fp, const stats_t *stats)
{
    if (NULL == stats) { return 0; }
    if (NULL == fp) { fp = stdout; }

    char min_time_str[BUFLEN];
    char max_time_str[BUFLEN];

    struct tm *min_tm = localtime(&stats->min_time);
    strftime(min_time_str, BUFLEN, "%c", min_tm);

    struct tm *max_tm = localtime(&stats->max_time);
    strftime(max_time_str, BUFLEN, "%c", max_tm);

    fprintf(fp,
            "STATS:\n"
            "  n ..... %zu\n"
            "  min ... %5.1lf C, %5.1lf F @ %s\n"
            "  max ... %5.1lf C, %5.1lf F @ %s\n"
            "  med ... %5.1lf C, %5.1lf F\n"
            "  avg ... %5.1lf C, %5.1lf F\n"
            "  std ... %5.1lf C, %5.1lf F\n"
            , stats->n
            , stats->min, DEG_C_TO_DEG_F(stats->min), min_time_str
            , stats->max, DEG_C_TO_DEG_F(stats->max), max_time_str
            , stats->med, DEG_C_TO_DEG_F(stats->med)
            , stats->avg, DEG_C_TO_DEG_F(stats->avg)
            , stats->std, DEG_C_TO_DEG_F(stats->std)
           );

    return 1;
}

int print_sqlite_result(FILE* fp, const temp_db_data_t *db_data)
{
    if (NULL == fp) { fp = stdout; }

    if (NULL == db_data) { return 0; }

    size_t i = 0;

    for (i=0; (0!=db_data[i].ts && !double_is_zero(db_data[i].tempc)); ++i)
    {
        char buf[BUFLEN];
        struct tm *tm = localtime(&db_data[i].ts);
        double tempf = (9.0 / 5.0 * db_data[i].tempc + 32.0);
        strftime(buf, BUFLEN, "%c", tm);

        fprintf(fp, "%s [%ld]: %.1lf deg F (%.1lf deg C)\n"
                , buf
                , (long int)db_data[i].ts
                , tempf
                , db_data[i].tempc);
    }

    return 1;
}

temp_db_data_t* fetch_from_sqlite(const char *fn, int n, int secs)
{
    const char *sql = 
        "SELECT timestamp,tempc FROM temps ORDER BY timestamp;" ;
    int rc = 0;
    sqlite3 *db = NULL;
    sqlite3_stmt *stmt = NULL;
    time_t now;

    if (n>0)
    {
        sql = "SELECT timestamp,tempc FROM temps ORDER BY timestamp DESC LIMIT ?;" ;
    }
    else if (secs>0)
    {
        sql = "SELECT timestamp,tempc FROM temps WHERE timestamp > ? ORDER BY timestamp;" ;
    }

    rc = sqlite3_open(fn, &db);
    if (SQLITE_OK != rc)
    {
        fprintf(stderr, "ERROR: sqlite3_open() failure, rc=%d\n", rc);
        return NULL;
    }

    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, 0);
    if (rc == SQLITE_OK)
    {
        if (n>0)
        {
            sqlite3_bind_int(stmt, 1, n);
        }
        else if (secs>0)
        {
            time(&now);
            now -= secs;
            sqlite3_bind_int(stmt, 1, secs);
        }
    } 
    else
    {
        fprintf(stderr, "ERROR: sqlite3_prepare_v2(): %s\n",
                sqlite3_errmsg(db));
        return NULL;
    }

    size_t db_data_size = 256;
    temp_db_data_t *db_data = NULL;
    db_data = malloc(db_data_size*sizeof(temp_db_data_t));

    size_t n_rows = 0;
    int step;
    while ( SQLITE_ROW == (step = sqlite3_step(stmt)) )
    {
        if (n_rows == (db_data_size-2))
        {
            db_data_size *= 2;
            db_data = realloc(db_data, (db_data_size*sizeof(temp_db_data_t)));
            if (NULL == db_data)
            {
                fprintf(stderr, "ERROR: realloc() failure\n");
                exit(-1);
            }
        }
        time_t temp_i = sqlite3_column_int(stmt, 0);
        double temp_d = sqlite3_column_double(stmt, 1);
        db_data[n_rows].ts = temp_i;
        db_data[n_rows].tempc = temp_d;
        ++n_rows;
    } 
    db_data[n_rows].ts = db_data[n_rows].tempc = 0;

    sqlite3_finalize(stmt);
    sqlite3_close(db);

    return db_data;
}

void usage(const char* progname)
{
    fprintf(stderr,
            "Usage: %s [options] <arguments>\n"
            "  ARGUMENTS:\n"
            "    -f SQLITE_FILE ... query SQLITE_FILE for temp logs\n"
            "  OPTIONS:\n"
            "    -n N_RECORDS ..... query N_RECORDS records from DB, default=%d\n"
            "    -H HOURS ......... query last HOURS worth of records\n"
            "    -d DAYS .......... query last DAYS worth of records\n"
            "                       days takes precedence over hours\n"
            "                       set -n 0 to use days or hours\n"
            "    -s STYLE ......... print style, bit mask:\n"
            "                       1 = individual records\n"
            "                       2 = summary stats\n"
            "                       3 = both records and stats\n"
            "    -h ............... show this help\n"
            , progname
            , DEFAULT_N_RECORDS
           );
}
 
int main(int argc, char **argv)
{
    enum 
    {
        STYLE_RECORDS=1,
        STYLE_STATS=2,
    };

    int c;
    char *sqlite_file = NULL;
    struct stat statbuf;
    int n_records = DEFAULT_N_RECORDS;
    int hours = 0;
    int days = 0;
    int secs = 0;
    int style = 0;

    while ( -1 != (c = getopt(argc, argv, "?hf:n:H:d:s:")) )
    {
        switch (c)
        {
            case 'f': sqlite_file = optarg;     break;
            case 'n': n_records = atoi(optarg); break;
            case 'H': hours = atoi(optarg);     break;
            case 'd': days = atoi(optarg);      break;
            case 's': style = atoi(optarg);     break;
            case '?':
            case 'h': 
            default:  usage(argv[0]); exit(0);  break;
        }
    }

    if (NULL == sqlite_file)
    {
        fprintf(stderr, "ERROR: sqlite_file not provided\n");
        exit(-1);
    }
    else if (0 != stat(sqlite_file, &statbuf))
    {
        fprintf(stderr, "ERROR: stat() failure for \"%s\"\n", sqlite_file);
        exit(-1);
    }

    if (hours > 0) { secs = 60*60; }
    if (days > 0)  { secs = 60*60*24; }

    temp_db_data_t* db_data = fetch_from_sqlite(sqlite_file, n_records, secs);
    if (style & STYLE_STATS)
    {
        stats_t stats;
        calc_temp_stats(db_data, &stats);
        print_stats(stdout, &stats);
    }

    if (style & STYLE_RECORDS)
    {
        print_sqlite_result(stdout, db_data);
    }

    free(db_data);

    return 0; 
}

