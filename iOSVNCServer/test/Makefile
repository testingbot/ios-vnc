libimobiledevice_screenshot_test: libimobiledevice_screenshot_test.o
	clang -Wall -l imobiledevice -o libimobiledevice_screenshot_test libimobiledevice_screenshot_test.o

test: libimobiledevice_screenshot_test
	./libimobiledevice_screenshot_test

clean:
	rm libimobiledevice_screenshot_test
	rm *.o
	rm *.tiff

libimobiledevice_screenshot_test.o: libimobiledevice_screenshot_test.c
	clang -c -Wall -std=c99 libimobiledevice_screenshot_test.c
