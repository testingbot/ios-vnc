CFLAGS = -Wall -std=c99 $(includes)
LDFLAGS = -L /opt/local/lib
LDLIBS = -l imobiledevice -l png -l vncserver
includes = -I /opt/local/include

iosvncserver: main.o iosscreenshot.o
	$(CC) $(LDFLAGS) $(LDLIBS) -o iosvncserver $^ 

clean:
	rm iosvncserver
	rm *.o
