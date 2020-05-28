## iOS VNC Server

We've been using this in production for the last 3 years at TestingBot.com
TestingBot Users can take control of a real iOS device through our website.
They can view the device's screen and send taps/keyboard events to the device.

This project provides a VNC endpoint which streams the device's screen to your VNC client.
Any keyboard and mouse events your VNC client sends will be sent to the iOS device.
This does not require the device to be jailbroken.

### Setup

* Clone this repository
* Install dependencies:
	* brew install libimobiledevice
	* brew install libpng
	* brew install libvncserver
	* brew install carthage

* Now you can build the project:
	* `cd iOSVNCServer && make build`

## Dependencies
To control the device, you will need to set up [WebdriverAgent](https://github.com/appium/WebDriverAgent)
This project exposes a couple of endpoints to take control of the device.
Once you've set this up correctly (signing and `./Scripts/bootstrap.sh`), you can run the WebDriverAgent project to expose the endpoint.

## Getting Started
The WebDriverAgent project will output a response `ServerURLHere->http://[SOME_IP]:8100<-ServerURLHere`
These values are required and need to be passed to our `iosvncserver`, together with the `udid` of the device:

* To get the udid: `idevice_id -l`
* Start a WebDriverAgent session: `curl -X POST -H "Content-Type: application/json" \
-d "{\"desiredCapabilities\":{\"bundleId\":\"com.apple.preferences\"}}" \
[SOME_IP]:8100/session
`
* Copy the sessionID from this response and pass it to our VNC server:

You can now run the VNC server:
`./iosvncserver -u [udid] -H [SOME-IP] -P 8100 -S [session-id]`

Point your favorite VNC client to `localhost:5901` and you will be able to take control of the real iOS device.

### Resources
##### [TestingBot](https://testingbot.com/)
