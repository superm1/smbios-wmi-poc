#include <sys/ioctl.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <linux/types.h>
#include <linux/dell-smbios.h>

#define DELL_CAPSULE_FIRMWARE_UPDATES_ENABLED 0x0461
#define DELL_CAPSULE_FIRMWARE_UPDATES_DISABLED 0x0462
#define DELL_CLASS_READ_TOKEN	0
#define DELL_SELECT_READ_TOKEN	0
#define DELL_CLASS_WRITE_TOKEN	1
#define DELL_SELECT_WRITE_TOKEN	0

static const char *devfs = "/dev/wmi/dell-smbios";
#define TOKENS_SYSFS "/sys/bus/platform/devices/dell-smbios.0/tokens"
#define BUFFER_SYSFS "/sys/bus/wmi/drivers/dell-smbios/A80593CE-A997-11DA-B012-B622A1EF5492/buffer_size"

void debug_buffer(struct wmi_smbios_buffer *buffer)
{
	g_print("%x/%x [%x,%x,%x,%x] [%8x,%8x,%8x,%8x] \n",
	buffer->std.class, buffer->std.select,
	buffer->std.input[0], buffer->std.input[1],
	buffer->std.input[2], buffer->std.input[3],
	buffer->std.output[0], buffer->std.output[1],
	buffer->std.output[2], buffer->std.output[3]);
}

int run_wmi_smbios_cmd(struct wmi_smbios_buffer *buffer)
{
	gint fd;
	gint ret;
	fd = g_open (devfs, O_NONBLOCK);
	ret = ioctl (fd, DELL_WMI_SMBIOS_CMD, buffer);
	if (ret != 0)
		g_error("run smbios command failed: %d", ret);
	close(fd);
	return ret;
}

int find_token(guint16 token, guint16 *location, guint16 *value)
{
	gchar *location_sysfs = NULL;
	gchar *value_sysfs = NULL;
	gchar *out = NULL;
	gsize len = 0;

	location_sysfs = g_strdup_printf("%s/%04x_location", TOKENS_SYSFS, token);
	if (!g_file_get_contents(location_sysfs, &out, &len, NULL)) {
		g_error("failed to read %s", location_sysfs);
		g_free(location_sysfs);
		return 1;
	}
	*location = (guint16) g_ascii_strtoll(out, NULL, 16);
	g_free(out);
	g_free(location_sysfs);

	value_sysfs = g_strdup_printf("%s/%04x_value", TOKENS_SYSFS, token);
	if (!g_file_get_contents(value_sysfs, &out, &len, NULL)) {
		g_error("failed to read %s", value_sysfs);
		g_free(value_sysfs);
		return 1;
	}
	*value = (guint16) g_ascii_strtoll(out, NULL, 16);
	g_free(out);
	g_free(value_sysfs);

	if (*location)
		return 0;
	return 2;
}

int
token_is_active(guint16 *location, guint16 *cmpvalue, struct wmi_smbios_buffer *buffer)
{
	int ret;
	buffer->std.class = DELL_CLASS_READ_TOKEN;
	buffer->std.select = DELL_SELECT_READ_TOKEN;
	buffer->std.input[0] = *location;
	ret = run_wmi_smbios_cmd(buffer);
	if (ret != 0|| buffer->std.output[0] != 0)
		return ret;
	ret = (buffer->std.output[1] == *cmpvalue);
	return ret;
}

int query_token(guint16 token, struct wmi_smbios_buffer *buffer)
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

int activate_token(struct wmi_smbios_buffer *buffer,
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
	buffer->std.class = DELL_CLASS_WRITE_TOKEN;
	buffer->std.select = DELL_SELECT_WRITE_TOKEN;
	buffer->std.input[0] = location;
	buffer->std.input[1] = 1;
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
	struct wmi_smbios_buffer *buffer;
	gint ret = 0;
	gint value;
	gint size;

	ret = query_buffer_size(&value);
	if (ret != 0) {
		g_error("couldn't read buffer size");
		ret = -1;
		goto out;
	}
	g_print("Working with buffer size: %d\n", value);

	buffer = g_new (struct wmi_smbios_buffer, value);
	if (buffer == NULL) {
		g_error("failed to alloc memory for ioctl");
		ret = -1;
		goto out;
	}
	buffer->length = value;

	/* run TPM lookup */
	buffer->std.class = 7;
	buffer->std.select = 3;
	buffer->std.input[0] = 2;
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
	g_free (buffer);
	return ret;
}
