

CC     := gcc
CFLAGS := -g -O2 -Wall -Wfloat-equal -Wwrite-strings -Wendif-labels
LIBS   := -lusb
OBJS   := sensorapi.o
PROGS  := pcsensor pcsensor_sqlite templog

all: $(PROGS)

pcsensor: pcsensor.c $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

pcsensor_sqlite: pcsensor_sqlite.c $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS) -lsqlite3

templog: templog.c
	$(CC) $(CFLAGS) -o $@ $^ -lsqlite3 -lm

%.o: %.c %.h
	$(CC) -c $(CFLAGS) -o $@ $<

clean:		
	rm -f $(PROGS) $(OBJS)


rules-install:			# must be superuser to do this
	cp ../udev/99-tempsensor.rules /etc/udev/rules.d

