/*
 */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <asm/types.h>
#include <unistd.h>
#include <dirent.h>
#include "empia_xuctrl.h"
#include "camera_ctrl.h"
#include <pthread.h>
#include <time.h>

int bCapture;
int frame_count;
int led_on;
int mode;
int bSaveMode;
int wled_timer;
int auto_capture;
struct timeval start, stop;

int xioctl(int fd, int request, void* arg)
{
	for (int i = 0; i < 100; i++) {
		int r = ioctl(fd, request, arg);
		if (r != -1 || errno != EINTR) 
			return r;
	}
	return -1;
}

int check_scanner_device(int fd) {
	struct v4l2_capability cap;
	int ret=0;
	if(ioctl(fd, VIDIOC_QUERYCAP, &cap) < 0){
		perror("VIDIOC_QUERYCAP");
		return -1;
	}else{
		//printf("cap.card %s\n", cap.card);
		ret = strncmp(cap.card, "USB 2.0 Scanner", 15);
		//printf("ret(%d)\n", ret);
	}
	return ret;
}

camera_t* camera_open(int fd, uint32_t width, uint32_t height)
{
	camera_t* camera = malloc(sizeof (camera_t));
	camera->fd = fd;
	camera->width = width;
	camera->height = height;
	camera->buffer_count = 0;
	camera->buffer.start = NULL;
	camera->buffer.length = 0;
	return camera;
}

void camera_init(camera_t* camera) {
	//check capabilities
	struct v4l2_capability cap;
	if(ioctl(camera->fd, VIDIOC_QUERYCAP, &cap) < 0){
		perror("VIDIOC_QUERYCAP");
		exit(1);
	}
	if(!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)){
		fprintf(stderr, "The device does not handle single-planar video capture.\n");
		exit(1);
	}

	//set pixel format as YUYV  
	struct v4l2_format format;
	memset(&format, 0, sizeof format);
	format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	format.fmt.pix.width = camera->width;
	format.fmt.pix.height = camera->height;
	format.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
	format.fmt.pix.field = V4L2_FIELD_NONE;
	if(ioctl(camera->fd, VIDIOC_S_FMT, &format) < 0){
	    perror("VIDIOC_S_FMT");
	    exit(1);
	}

	//request Buffers for memory map
	struct v4l2_requestbuffers bufrequest;
	memset(&bufrequest, 0, sizeof bufrequest);
	bufrequest.count = 1;
	bufrequest.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	bufrequest.memory = V4L2_MEMORY_MMAP;
	if(ioctl(camera->fd, VIDIOC_REQBUFS, &bufrequest) < 0){
	    perror("VIDIOC_REQBUFS");
	    exit(1);
	}
	camera->buffer_count = bufrequest.count;

	//get memory map buffer info via index then store to allocate buffer camera->bufferinfo
	memset(&camera->bufferinfo, 0, sizeof(camera->bufferinfo));
	camera->bufferinfo.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	camera->bufferinfo.memory = V4L2_MEMORY_MMAP;
	camera->bufferinfo.index = 0;	 
	if(ioctl(camera->fd, VIDIOC_QUERYBUF, &camera->bufferinfo) < 0){
	    perror("VIDIOC_QUERYBUF");
	    exit(1);
	}	

	camera->buffer.length = camera->bufferinfo.length;

	//map devices /dev/video0 into memory and get return value as map start address.
	camera->buffer.start = mmap(NULL, camera->bufferinfo.length, PROT_READ | PROT_WRITE, MAP_SHARED, camera->fd, camera->bufferinfo.m.offset);

	if(camera->buffer.start == MAP_FAILED){
	    perror("mmap");
	    exit(1);
	}
	memset(camera->buffer.start, 0, camera->buffer.length);

}

//Image Quality Setting
void camera_image_ctl(camera_t* camera){

	struct v4l2_queryctrl queryctrl;
	struct v4l2_control control;

	//Set Brightness
	memset(&queryctrl, 0, sizeof(queryctrl));
	queryctrl.id = V4L2_CID_BRIGHTNESS;

	if (-1 == ioctl(camera->fd, VIDIOC_QUERYCTRL, &queryctrl)) {
		if (errno != EINVAL) {
		perror("VIDIOC_QUERYCTRL");
		exit(EXIT_FAILURE);
		} else {
		printf("V4L2_CID_BRIGHTNESS is not supportedn");
		}
	} else if (queryctrl.flags & V4L2_CTRL_FLAG_DISABLED) {
		printf("V4L2_CID_BRIGHTNESS is not supportedn");
	} else {
		memset(&control, 0, sizeof (control));
		control.id = V4L2_CID_BRIGHTNESS;
		control.value = IMG_BRIGHTNESS;
		if (-1 == ioctl(camera->fd, VIDIOC_S_CTRL, &control)) {
			perror("VIDIOC_S_CTRL");
			exit(EXIT_FAILURE);
		}
	}
}

void stream_on(camera_t* camera)
{
	enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if (xioctl(camera->fd, VIDIOC_STREAMON, &type) == -1) {
		perror("VIDIOC_STREAMON");
		exit(EXIT_FAILURE);
	}
}

void stream_off(camera_t* camera)
{
	enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if (xioctl(camera->fd, VIDIOC_STREAMOFF, &type) == -1) {
		perror("VIDIOC_STREAMOFF");
		exit(EXIT_FAILURE);
	}
}

//Save buffer data as yuv file
void save_file(camera_t* camera, int i){
	char filename[30];
	int imgfile;

	sprintf(filename, "Still00%d.yuv", i);
 	printf("capture image: %s\n", filename);
	
	if((imgfile = open(filename, O_WRONLY | O_CREAT, 0660))< 0){
		printf("open image fail.\n");
	}
	write (imgfile, camera->buffer.start , camera->buffer.length);
 	close(imgfile);
}

void camera_finish(camera_t* camera)
{
	munmap(camera->buffer.start, camera->buffer.length);
	camera->buffer_count = 0;
	camera->buffer.length = 0;
	camera->buffer.start = NULL;
}

void camera_close(camera_t* camera)
{
	if (close(camera->fd) == -1) {
		perror("close");
		exit(EXIT_FAILURE);
	}
	free(camera);
}

int iCount=0;
pthread_t	 hThread_SaveMode;

void * Thread_SaveMode (void *arg)
{
	int *saveTime;
	saveTime = (int*)arg;
	//printf("Thread_SaveMode saveTime(%d)\n", *saveTime);
	if(*saveTime == 0){
		//printf("saveTime = 0 exit thread\n");
		pthread_exit(NULL);
	}
	
	while(1){
		//printf("SaveMode iCount %d\n", iCount);
		usleep(1000*1000);
		iCount++;
		if((iCount >= *saveTime) && (!bCapture)){
			//printf("iCount(%d) It's time to enter save mode\n", iCount);
			bSaveMode = 1;
			iCount=0;
			hThread_SaveMode = 0;
			pthread_exit (NULL);
		}
	}
}

int capture(camera_t* camera)
// modified by Guanqun Wang
{
	int retrive;
	int ret = 0;
	unsigned char data;
	int get_frame_number=0;

	memset(&camera->bufferinfo, 0, sizeof(camera->bufferinfo));
	camera->bufferinfo.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	camera->bufferinfo.memory = V4L2_MEMORY_MMAP;
	camera->bufferinfo.index = 0; /* Queueing buffer index 0. */
	
	// Put the buffer in the incoming queue.
	if(ioctl(camera->fd, VIDIOC_QBUF, &camera->bufferinfo) < 0){
	    perror("VIDIOC_QBUF");
	    exit(1);
	}

	frame_count++;
	stream_on(camera);

	// The buffer's waiting in the outgoing queue.
	if(ioctl(camera->fd, VIDIOC_DQBUF, &camera->bufferinfo) < 0){
	    perror("VIDIOC_DQBUF");
	    exit(1);
	}		

	while(!bCapture){
		if(bSaveMode){
			//printf("Enter save mode turn off led\n");
			stream_off(camera);
			white_led_off(camera->fd);
			wait_led_off(camera->fd);
			break;
		}
		//Put the buffer in the incoming queue.
		if(ioctl(camera->fd, VIDIOC_QBUF, &camera->bufferinfo) < 0){
		    perror("VIDIOC_QBUF");
		    exit(1);
		}
		if(frame_count<AE_STABLE_FRAME){
				frame_count++;
		}
		// The buffer's waiting in the outgoing queue.
		if(ioctl(camera->fd, VIDIOC_DQBUF, &camera->bufferinfo) < 0){
		    perror("VIDIOC_DQBUF");
		    exit(1);
		}		
	}

	if(bCapture){
		//printf("bCapture\n");
		iCount=0;			
		bSaveMode = 0;

		get_frame_number = AE_STABLE_FRAME-frame_count;
		for(retrive=0; retrive<get_frame_number; retrive++){
			// Put the buffer in the incoming queue.
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
		stream_off(camera);
		gettimeofday(&stop, NULL);
		beep(camera->fd, BEEP_DURATION, 1); //capture finish beep one time
		//printf("capture time: %d ms\n", ((stop.tv_sec - start.tv_sec)*1000000 + stop.tv_usec - start.tv_usec)/1000);
		bCapture=0;
		frame_count=0;
		//start save mode counter thread
		if(wled_timer){
			if(hThread_SaveMode == 0){
				//printf("hThread_SaveMode == NULL create Thread_SaveMode\n");
				if (0 != pthread_create(&hThread_SaveMode, NULL, Thread_SaveMode, &wled_timer))
				{
					printf ("Create Save Mode Thread Failed!!!\n");
					return -1;
				}else{
					//printf("pthread_create success\n");
				}
			}
			else{
				//printf("hThread_SaveMode != NULL Do not create thread again but reset iCount\n");
				iCount = 0;
			}
		}else{ //timer = 0
			white_led_off(camera->fd);
			wait_led_off(camera->fd);
		}			
		
		return 0;
	}


		get_frame_number=0;

	return 0;
}

int beep(int fd, int duration, int times){
	unsigned char data;
	int i, ret;
	for(i=0; i<times; i++){
		ret = empia_regRead(fd, P0_R, &data); 
		data = data | (1<<BIT_BUZZER);
		ret = empia_regWrite(fd, P0_W, data);
		ret = empia_regRead(fd, P0_R, &data); 
		usleep(duration*1000);	
		data &= (~(1<<BIT_BUZZER));
		ret = empia_regWrite(fd, P0_W, data);
		usleep(100*1000);	
	}
	return ret;
}

void wait_led_on(int fd){
	unsigned char data;
	int count = 0;
	int ret;
	ret = empia_regRead(fd, P3_R, &data);

	while ((data & (1<<BIT_WLED)) == 0){
		ret = empia_regRead(fd, P3_R, &data);
		usleep(30*1000);
		count++;
		if(count > 20)
			break;
	}
}

void wait_led_off(int fd){
	unsigned char data;
	int count= 0;
	int ret;
	ret = empia_regRead(fd, P3_R, &data);
	while ((data & (1<<BIT_WLED)) != 0){
		ret = empia_regRead(fd, P3_R, &data);
		usleep(30*1000);
		count++;
		if(count > 20)
			break;
	}
}

void white_led_on(int fd){
	unsigned char data;
	int ret;
	ret = empia_regRead(fd, P3_R, &data);;
	//data=data|0x02;
	data = data | (1<<BIT_WLED);
	ret = empia_regWrite(fd, P3_W, data);	
	led_on=1;
}

void white_led_off(int fd){
	unsigned char data;
	int ret;
	ret = empia_regRead(fd, P3_R, &data);
	data &= (~(1<<BIT_WLED));	
	ret = empia_regWrite(fd, P3_W, data);
	frame_count=0;
	led_on=0;
}

void sensor1_enable(int fd){
	unsigned char data;
	int ret;
	ret = empia_regRead(fd, P0_R, &data);
	data = data | (1<<BIT_SENSOR_EN); //sensor enable pull high
	data= data&(~(1<<BIT_WP)); //wp pull low
	ret = empia_regWrite(fd, P0_W, data);	
}

