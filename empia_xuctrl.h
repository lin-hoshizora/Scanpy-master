#ifndef _EMPIA_XUCTRL_H_
#define _EMPIA_XUCTRL_H_

#ifdef __cplusplus
extern "C" {
#endif 

#include <sys/time.h>
#include <linux/videodev2.h>

/* Video Class-Specific Request Codes */
#define UVC_SET_CUR                                     0x01
#define UVC_GET_CUR                                     0x81

/*
 * Dynamic controls
 */
#define UVCIOC_CTRL_ADD		_IOW('U', 1, struct uvc_xu_control_info)
#define UVCIOC_CTRL_GET		_IOWR('U', 3, struct uvc_xu_control)
#define UVCIOC_CTRL_SET		_IOW('U', 4, struct uvc_xu_control)

#define UVCIOC_CTRL_MAP     _IOWR('u', 0x20, struct uvc_xu_control_mapping)
#define UVCIOC_CTRL_QUERY   _IOWR('u', 0x21, struct uvc_xu_control_query)

/* Data types for UVC control data */
#define UVC_CTRL_DATA_TYPE_RAW		0
#define UVC_CTRL_DATA_TYPE_SIGNED	1
#define UVC_CTRL_DATA_TYPE_UNSIGNED	2
#define UVC_CTRL_DATA_TYPE_BOOLEAN	3
#define UVC_CTRL_DATA_TYPE_ENUM		4
#define UVC_CTRL_DATA_TYPE_BITMASK	5

/* Control flags */
#define UVC_CONTROL_SET_CUR	(1 << 0)
#define UVC_CONTROL_GET_CUR	(1 << 1)
#define UVC_CONTROL_GET_MIN	(1 << 2)
#define UVC_CONTROL_GET_MAX	(1 << 3)
#define UVC_CONTROL_GET_RES	(1 << 4)
#define UVC_CONTROL_GET_DEF	(1 << 5)
/* Control should be saved at suspend and restored at resume. */
#define UVC_CONTROL_RESTORE	(1 << 6)
/* Control can be updated by the camera. */
#define UVC_CONTROL_AUTO_UPDATE	(1 << 7)

#define UVC_CONTROL_GET_RANGE	(UVC_CONTROL_GET_CUR | UVC_CONTROL_GET_MIN | \
								UVC_CONTROL_GET_MAX | UVC_CONTROL_GET_RES | \
								UVC_CONTROL_GET_DEF)

struct uvc_xu_control_info {
	__u8 entity[16];
	__u8 index;
	__u8 selector;
	__u16 size;
	__u32 flags;
};

struct uvc_menu_info {
        __u32 value;
        __u8 name[32];
};

struct uvc_xu_control_mapping {
	__u32 id;
    __u8 name[32];
    __u8 entity[16];
    __u8 selector;

    __u8 size;
    __u8 offset;
    __u32 v4l2_type;
    __u32 data_type;

    struct uvc_menu_info *menu_info;
    __u32 menu_count;

    __u32 reserved[4];
};

struct uvc_xu_control_query {
        __u8 unit;
        __u8 selector;
        __u8 query;             /* Video Class-Specific Request Code, */
                                /* defined in linux/usb/video.h A.8.  */
        __u16 size;
        __u8  *data;
};

struct uvc_xu_control {
	__u8 unit;
	__u8 selector;
	__u16 size;
	__u8  *data;
};

#define LENGTH_OF(a)		(sizeof(a) / sizeof((a)[0]))
#define UVC_EMPIA_XU_ID			4

#define XU_CONTROL_SET_LENGTH	1
#define XU_CONTROL_REG_READ		2
#define XU_CONTROL_REG_WRITE	3
#define XU_CONTROL_I2C_READ		4
#define XU_CONTROL_I2C_WRITE	5

#define XU_CONTROL_REG_READ_EX			0x10
#define XU_CONTROL_REG_WRITE_EX			0x11
#define XU_CONTROL_I2C_READ_EX			0x12
#define XU_CONTROL_I2C_WRITE_EX			0x13
#define XU_CONTROL_GAZO_CUSTOM_READ_ID	0x1C
#define XU_CONTROL_GAZO_CUSTOM_WRITE_ID	0x1D

#define XU_CONTROL_EX_LEN		32

/* I2C access flags */
#define I2C_FLAG_INDEXED        0x01
#define I2C_FLAG_HW             0x02
#define I2C_FLAG_25KHZ          0x04
#define I2C_FLAG_ACK_LAST_READ  0x08
#define I2C_FLAG_READ_STOP      0x10
#define I2C_FLAG_400KHZ         0x20

#define Primary_HW_I2C_Bus		0x01
#define Senondary_HW_I2C_Bus	0x02

void initDynCtrls(int fd);
int empia_regRead(int fd, int idx, unsigned char* data);
int empia_regWrite(int fd, int idx, unsigned char data);
int select_i2cbus(int fd, unsigned char bus);
int empia_i2cRead(int fd, unsigned char bus, unsigned char addr, int idx, unsigned char lenIdx, unsigned char* data, unsigned char lenData );
int empia_i2cWrite(int fd, unsigned char bus, unsigned char addr, int idx, unsigned char lenIdx, unsigned char* data, unsigned char lenData);
int empia_gpioRead(int fd, unsigned char port, unsigned char* data);
int empia_gpioWrite(int fd, unsigned char port, unsigned char mask, unsigned char data);
int empia_extension_writeEx(int fd, unsigned char* buf);
int empia_extension_readEx(int fd, unsigned char* buf, unsigned char* data);

#define CTRL_ON			1
#define CTRL_OFF		0

#ifdef __cplusplus
}
#endif 


#endif
