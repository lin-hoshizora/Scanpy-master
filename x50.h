#include <boost/python.hpp>

#include <stdio.h>
#include <fcntl.h>
#include <iostream>
#include <cstring>
#include <thread>
#include <future>

extern "C" {
#include "empia_xuctrl.h"
#include "camera_ctrl.h"
}

using namespace boost::python;

class X50
{
public:
	X50();
	bool findDev();
	bool openDev();
	void init();
	void ledOn();
	void ledOff();
	void captureSetup();
	int detectPaperL();
	int detectPaperR();
	void autoDetect();
	int autoStart();
	object retrieveBuff();
	void doBeep(int n_ms);
	void warmup();
	object scan();
	void cancel();
	void clean();
private:
	int fd, devNO;
	camera_t* camera;
	std::string img_str;
	bool init_done;
	bool led_is_on;
	bool paper_detected;
	bool stop_update;
	int frame_count;
	bool warmup_started;
};
