#include "x50.h"

std::future<int> f;

X50::X50(){
	fd = -1;
	devNO = -1;
	init_done = false;
	paper_detected = false;
	led_is_on = false;
	stop_update = false;
	frame_count = 0;
	warmup_started = false;
	if (findDev())
		if (openDev())
			init();
}

bool X50::findDev(){
	int maxDevNum = 4;
	char deviceName[20];

	for(int devNum=0; devNum<maxDevNum;devNum++){
		sprintf(deviceName, "/dev/video%d", devNum);
		fd = open(deviceName, O_RDWR);
		if (fd < 0) continue;
		int ret = check_scanner_device(fd);
		close(fd);
		if (ret == 0){
			std::cout << "Found scanner: /dev/video" << devNum << std::endl;
			devNO = devNum;
			return true;
		}
	}
	devNO = -1;
	return false;
}

bool X50::openDev(){
	char deviceName[20];
	sprintf(deviceName, "/dev/video%d", devNO);
	fd = open(deviceName, O_RDWR);
	if (fd < 0) return false;
	else return true;
}

void X50::ledOn(){
	if (!led_is_on){
		white_led_on(camera->fd);
		wait_led_on(camera->fd);
		led_is_on = true;
	}
}

void X50::ledOff(){
	if (led_is_on){
		white_led_off(camera->fd);
		wait_led_off(camera->fd);
		led_is_on = false;
	}
}

void X50::init(){
	initDynCtrls(fd);
	camera = camera_open(fd, IMG_WIDTH, IMG_HEIGHT);
	camera_init(camera);
	camera_image_ctl(camera);
	sensor1_enable(fd);
	init_done = true;
}

void X50::captureSetup(){
	empia_regWrite(fd, 0xC4, 0x00); // problem here?
	ledOn();
	empia_regWrite(fd, 0xC4, 0x01); // still image mode on
	wait_led_on(fd); // neccesary according to plustek?

	memset(&camera->bufferinfo, 0, sizeof(camera->bufferinfo));
	camera->bufferinfo.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	camera->bufferinfo.memory = V4L2_MEMORY_MMAP;
	camera->bufferinfo.index = 0; /* Queueing buffer index 0. */


	// Put the buffer in the incoming queue.
	if(ioctl(camera->fd, VIDIOC_QBUF, &camera->bufferinfo) < 0){
	    perror("VIDIOC_QBUF");
	    exit(1);
	}
	
	stream_on(camera);

	// The buffer's waiting in the outgoing queue.
	if(ioctl(camera->fd, VIDIOC_DQBUF, &camera->bufferinfo) < 0){
	    perror("VIDIOC_DQBUF");
	    exit(1);
	}
}

int X50::detectPaperL(){
	unsigned char data;
	int ret = empia_regRead(fd, P0_R, &data);
	return data&(1<<BIT_SENSOR1);
}

int X50::detectPaperR(){
	unsigned char data;
	int ret = empia_regRead(fd, P0_R, &data);
	return data&(1<<BIT_SENSOR2);
}

void X50::autoDetect(){
	int status_old = 1;
	int status_new = 0;
	while(!paper_detected)
	{
		status_new = detectPaperL();
		if (status_new > status_old)
		{
			std::cout << "paper detected" << std::endl;
			paper_detected = true;
			break;
		}
		status_old = status_new;
		usleep(SENSOR_DETECT_FREQUENCE);
	}
}

int X50::autoStart(){
	autoDetect();
	if (!stop_update)
		captureSetup();
	frame_count = 0;
	while(!stop_update)
	{
		if (frame_count < AE_STABLE_FRAME){
			std::cout << "Grab frame " << frame_count << std::endl;
			frame_count++;
		}

		//Put the buffer in the incoming queue.
		if(ioctl(camera->fd, VIDIOC_QBUF, &camera->bufferinfo) < 0){
		    perror("VIDIOC_QBUF");
		    exit(1);
		}

		// The buffer's waiting in the outgoing queue.
		if(ioctl(camera->fd, VIDIOC_DQBUF, &camera->bufferinfo) < 0){
		    perror("VIDIOC_DQBUF");
		    exit(1);
		}
	}
	return 0;
}

object X50::retrieveBuff(){
	stream_off(camera);
	object memory_view(handle<>(PyMemoryView_FromMemory((char*)camera->buffer.start, camera->buffer.length, PyBUF_READ)));
	beep(camera->fd, BEEP_DURATION, 1);
	ledOff();
	frame_count = 0;
	return memory_view;
}

void X50::warmup(){
	paper_detected = false;
	stop_update = false;
	warmup_started = true;
	f = std::async(&X50::autoStart, this);
}

object X50::scan(){
	if (!warmup_started)
	{
		std::cout << "Scan called before warmup, aborting...";
		return object();
	}
	if (!paper_detected)
		paper_detected = true;
	while(true)
	{
		if (frame_count >= AE_STABLE_FRAME)
		{	
			stop_update = true;
			int ret = f.get();
			break;
		}
	}
	warmup_started = false;
	paper_detected = false;
	return retrieveBuff();
}

void X50::cancel(){
	if (warmup_started){	
		stop_update = true;
		paper_detected = true;
		int ret = f.get();
		stream_off(camera);
		ledOff();
		paper_detected = false;
		frame_count = 0;
		warmup_started = false;
	}
}

void X50::doBeep(int n_ms){
	beep(camera->fd, n_ms, 1);
}

void X50::clean(){
	stream_off(camera);
	ledOff();
	camera_finish(camera);
	camera_close(camera);
	init_done = false;
	fd = -1;
	devNO = -1;
	paper_detected = false;
	stop_update = false;
	frame_count = 0;
	warmup_started = false;
}

BOOST_PYTHON_MODULE(scanpy){
	class_<X50>("X50")
		.def("findDev", &X50::findDev)
		.def("openDev", &X50::openDev)
        .def("init", &X50::init)
        .def("ledOn", &X50::ledOn)
        .def("ledOff", &X50::ledOff)
		.def("captureSetup", &X50::captureSetup)
		.def("detectPaperL", &X50::detectPaperL)
		.def("detectPaperR", &X50::detectPaperR)
		.def("autoDetect", &X50::autoDetect)
		.def("autoStart", &X50::autoStart)
        .def("retrieveBuff", &X50::retrieveBuff)
		.def("warmup", &X50::warmup)
		.def("scan", &X50::scan)
		.def("cancel", &X50::cancel)
		.def("beep", &X50::doBeep)
		.def("clean", &X50::clean);
}