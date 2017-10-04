#ifndef _UAPI_DELL_WMI_SMBIOS_H_
#define _UAPI_DELL_WMI_SMBIOS_H_

#include <linux/ioctl.h>

struct wmi_calling_interface_buffer {
	guint16 class;
	guint16 select;
	guint32 input[4];
	guint32 output[4];
	guint32 argattrib;
	guint32 blength;
	guint8 *data;
} __packed;

struct wmi_smbios_ioctl {
	guint32 length;
	struct wmi_calling_interface_buffer *buf;
};

#define DELL_WMI_SMBIOS_IOC			'D'
/* run SMBIOS calling interface command
 * note - 32k is too big for size, so this can not be encoded in macro properly
 */
#define DELL_WMI_SMBIOS_CALL_CMD	_IOWR(DELL_WMI_SMBIOS_IOC, 0, struct wmi_smbios_ioctl)

#define WMI_IOC 'W'
#define WMI_IO(instance)       _IO(WMI_IOC, instance)
#define WMI_IOR(instance)      _IOR(WMI_IOC, instance, void*)
#define WMI_IOW(instance)      _IOW(WMI_IOC, instance, void*)
#define WMI_IOWR(instance)     _IOWR(WMI_IOC, instance, void*)

#endif /* _UAPI_DELL_WMI_SMBIOS_H_ */
