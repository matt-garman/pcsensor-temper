all:	pcsensor


CFLAGS := -g -O2 -Wall -Wfloat-equal -Wwrite-strings -Wendif-labels
OBJS   := sensorapi.o


%.o: %.c %.h
	$(CC) -c $(CFLAGS) -o $@ $<


pcsensor: pcsensor.c $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ -lusb

clean:		
	rm -f pcsensor $(OBJS)


rules-install:			# must be superuser to do this
	cp ../udev/99-tempsensor.rules /etc/udev/rules.d

