CFLAGS = -Wall -std=c99 $(includes)
LDFLAGS = -L /usr/local/opt/ -L ./vendor/jsmn/
LDLIBS = -l imobiledevice -l png -l vncserver -l curl -l jsmn
includes = -I /usr/local/include

iosvncserver: main.o iosscreenshot.o
	$(CC) $(LDFLAGS) $(LDLIBS) -o iosvncserver $^ 

dependencies: 
	cd ./vendor/jsmn/; make

build: dependencies iosvncserver

clean:
	rm iosvncserver
	rm *.o
	cd ./vendor/jsmn/; make clean
