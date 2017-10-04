#include <sys/ioctl.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

#include "dell-wmi-smbios.h"

#define DELL_CAPSULE_FIRMWARE_UPDATES_ENABLED 0x0461
#define DELL_CAPSULE_FIRMWARE_UPDATES_DISABLED 0x0462
#define DELL_CLASS_READ_TOKEN	0
#define DELL_SELECT_READ_TOKEN	0
#define DELL_CLASS_WRITE_TOKEN	1
#define DELL_SELECT_WRITE_TOKEN	0

#define DELL_CLASS_ADMIN_PASS_SET	10
#define DELL_SELECCT_ADMIN_PASS_SET	3

static const char *devfs = "/dev/wmi/dell-smbios";
#define TOKENS_SYSFS "/sys/bus/platform/devices/dell-smbios.0/tokens"
#define BUFFER_SYSFS "/sys/bus/wmi/drivers/dell-smbios/A80593CE-A997-11DA-B012-B622A1EF5492/buffer_size"

void debug_buffer(struct wmi_smbios_ioctl *buffer)
{
	g_print("%x/%x [%x,%x,%x,%x] [%x,%x,%x,%x] \n",
	buffer->buf->class, buffer->buf->select, buffer->buf->input[0],
	buffer->buf->input[1], buffer->buf->input[2], buffer->buf->input[3],
	buffer->buf->output[0],
	buffer->buf->output[1], buffer->buf->output[2], buffer->buf->output[3]);
}

int run_wmi_smbios_cmd(struct wmi_smbios_ioctl *buffer)
{
	gint fd;
	gint ret;
	fd = g_open (devfs, O_NONBLOCK);
	ret = ioctl (fd, WMI_IOWR(0), buffer);
	if (ret != 0)
		g_error("run smbios command failed: %d", ret);
	close(fd);
	return ret;
}

int find_token(guint16 token, guint16 *location, guint16 *value)
{
	gchar *out = NULL;
	gchar **top_split;
	gchar **inner_split;
	gchar **ptr;
	guint16 cmptoken;
	gsize len = 0;
	if (!g_file_get_contents(TOKENS_SYSFS, &out, &len, NULL)) {
		g_error("failed to read tokens");
		return 1;
	}
	top_split = g_strsplit(out, "\n", 0);
	for (ptr = top_split; *ptr; ptr++) {
		inner_split =  g_strsplit(*ptr, "\t", 3);
		if (!*inner_split)
			continue;
		cmptoken = (guint16) g_ascii_strtoll(inner_split[0], NULL, 16);
		if (token != cmptoken)
			continue;
		*location = (guint16) g_ascii_strtoll(inner_split[1], NULL, 16);
		*value = (guint16) g_ascii_strtoll(inner_split[2], NULL, 16);
		g_strfreev(inner_split);
		break;
	}
	g_strfreev(top_split);
	g_free(out);
	if (*location)
		return 0;
	return 2;
}

int
token_is_active(guint16 *location, guint16 *cmpvalue, struct wmi_smbios_ioctl *buffer)
{
	int ret;
	buffer->buf->class = DELL_CLASS_READ_TOKEN;
	buffer->buf->select = DELL_SELECT_READ_TOKEN;
	buffer->buf->input[0] = *location;
	ret = run_wmi_smbios_cmd(buffer);
	if (ret != 0|| buffer->buf->output[0] != 0)
		return ret;
	ret = (buffer->buf->output[1] == *cmpvalue);
	return ret;
}

int query_token(guint16 token, struct wmi_smbios_ioctl *buffer)
{
	guint16 location;
	guint16 value;
	gint ret;
	ret = find_token(token, &location, &value);
	if (ret != 0) {
		g_error("unable to find token %04x", token);
		return 1;
	}
	return token_is_active(&location, &value, buffer);
}

int activate_token(struct wmi_smbios_ioctl *buffer,
		   guint16 token)
{
	guint16 location;
	guint16 value;
	gint ret;
	ret = find_token(token, &location, &value);
	if (ret != 0) {
		g_error("unable to find token %04x", token);
		return 1;
	}
	buffer->buf->class = DELL_CLASS_WRITE_TOKEN;
	buffer->buf->select = DELL_SELECT_WRITE_TOKEN;
	buffer->buf->input[0] = location;
	buffer->buf->input[1] = 1;
	ret = run_wmi_smbios_cmd(buffer);
	return ret;
}

int query_buffer_size(gint *value)
{
	gchar *out;
	gsize len;
	if (!g_file_get_contents(BUFFER_SYSFS, &out, &len, NULL)) {
		g_error("failed to read buffer size");
		return 1;
	}
	*value = g_ascii_strtoll(out, NULL, 10);
	return 0;
}

int main()
{
	struct wmi_smbios_ioctl *buffer;
	gint ret = 0;
	gint value;

	ret = query_buffer_size(&value);
	if (ret != 0) {
		g_error("couldn't read buffer size");
		ret = -1;
		goto out;
	}
	g_print("Working with buffer size: %d\n", value);

	buffer = g_new (struct wmi_smbios_ioctl, sizeof(struct wmi_smbios_ioctl));
	if (buffer == NULL) {
		g_error("failed to alloc memory for ioctl");
		ret = -1;
		goto out;
	}
	buffer->length = value;
	buffer->buf = g_new (struct wmi_calling_interface_buffer, value);
	if (buffer->buf == NULL) {
		g_error("failed to alloc memory");
		ret = -1;
		goto out;
	}

	/* run TPM lookup */
	buffer->buf->class = 7;
	buffer->buf->select = 3;
	buffer->buf->input[0] = 2;
	if (run_wmi_smbios_cmd(buffer)){
		g_error("first rw ioctl failed");
		ret = -1;
		goto out;
	}
	debug_buffer(buffer);

	ret = query_token(DELL_CAPSULE_FIRMWARE_UPDATES_ENABLED, buffer);
	g_print("UEFI Capsule enabled token is: %d\n", ret);

	ret = query_token(DELL_CAPSULE_FIRMWARE_UPDATES_DISABLED, buffer);
	g_print("UEFI Capsule disabled token is: %d\n", ret);

	if (ret) {
		g_print("Enabling UEFI capsule token");
		/* activate UEFI capsule updates */
		if(activate_token(buffer, DELL_CAPSULE_FIRMWARE_UPDATES_ENABLED)) {
			g_error("activate failed");
			ret = -1;
			goto out;
	}

	}
out:
	if(buffer)
		g_free (buffer->buf);
	g_free (buffer);
	return ret;
}
