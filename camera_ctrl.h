#include <stdint.h>
#include <sys/types.h>
#include <linux/videodev2.h>
#include <sys/ioctl.h>

#define IMG_WIDTH 2048
#define IMG_HEIGHT 1536
#define IMG_BRIGHTNESS 78 //Range:0-255
#define P0 0x80
#define P1 0x81
#define P3 0x83
#define P0_W 0x80
#define P1_W 0x81
#define P3_W 0x83
#define P0_R 0x84
#define P1_R 0x85
#define P3_R 0x87
//P0
#define BIT_SENSOR1 0
#define BIT_SENSOR_EN 1
#define BIT_SENSOR2 2
#define BIT_BUZZER 3
#define BIT_WP 7
//P3
#define BIT_WLED 1
#define BIT_INFRARED_LED 2

#define BEEP_DURATION 70 //70ms
#define WLED_ON_MAX_TIME 1800 //30mins
#define SENSOR_DETECT_FREQUENCE 40*1000 //40ms
#define AE_STABLE_FRAME 7

typedef struct {
	uint8_t* start;
	size_t length;
} buffer_t;

typedef struct {
	int fd;
	uint32_t width;
	uint32_t height;
	size_t buffer_count;
	struct v4l2_buffer bufferinfo;
	buffer_t buffer;
} camera_t;

int check_scanner_device(int fd);
camera_t* camera_open(int fd, uint32_t width, uint32_t height);
void camera_init(camera_t* camera);
void camera_image_ctl(camera_t* camera);
void camera_finish(camera_t* camera);
void camera_close(camera_t* camera);
int capture(camera_t* camera); // modifed
int beep(int fd, int duration, int times);
void wait_led_on(int fd);
void wait_led_off(int fd);
void white_led_on(int fd);
void white_led_off(int fd);
void sensor1_enable(int fd);

// added for interface to C++ class
int xioctl(int fd, int request, void* arg);
void stream_on(camera_t* camera);
void stream_off(camera_t* camera);