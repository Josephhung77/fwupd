/*
 * Copyright (C) 2021 Ricardo Cañuelo <ricardo.canuelo@collabora.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#define G_LOG_DOMAIN				"FuBluezDevice"

#include "config.h"

#include <string.h>

#include "fu-bluez-device.h"
#include "fu-common.h"
#include "fu-device-private.h"
#include "fu-firmware-common.h"

#define DEFAULT_PROXY_TIMEOUT	5000

/**
 * SECTION:fu-bluez-device
 * @short_description: a Bluez Bluetooth LE device
 *
 * An object that represents a Bluez Bluetooth LE device.
 *
 * See also: #FuBleDevice
 */

struct _FuBluezDevice {
	FuBleDevice		 parent_instance;
	GDBusObjectManager	*object_manager;
	GDBusProxy		*proxy;
	GHashTable		*uuid_paths;	/* utf8 : utf8 */
};

G_DEFINE_TYPE (FuBluezDevice, fu_bluez_device, FU_TYPE_BLE_DEVICE)

#define GET_PRIVATE(o) (fu_bluez_device_get_instance_private (o))

static void
fu_bluez_device_add_uuid_path (FuBluezDevice *self, const gchar *uuid, const gchar *path)
{
	g_return_if_fail (FU_IS_BLUEZ_DEVICE (self));
	g_return_if_fail (uuid != NULL);
	g_return_if_fail (path != NULL);
	g_hash_table_insert (self->uuid_paths,
			     g_strdup (uuid),
			     g_strdup (path));
}

static void
fu_bluez_device_set_modalias (FuBluezDevice *self, const gchar *modalias)
{
	const gchar *subsys = NULL;
	gsize modaliaslen = strlen (modalias);
	guint16 vid = 0x0;
	guint16 pid = 0x0;
	guint16 rev = 0x0;

	g_return_if_fail (modalias != NULL);

	/* usb:v0461p4EEFd0001 */
	if (g_str_has_prefix (modalias, "usb:")) {
		subsys = "USB";
		fu_firmware_strparse_uint16_safe (modalias, modaliaslen, 5, &vid, NULL);
		fu_firmware_strparse_uint16_safe (modalias, modaliaslen, 10, &pid, NULL);
		fu_firmware_strparse_uint16_safe (modalias, modaliaslen, 15, &rev, NULL);

	/* bluetooth:v000ApFFFFdFFFF */
	} else if (g_str_has_prefix (modalias, "bluetooth:")) {
		subsys = "BLE";
		fu_firmware_strparse_uint16_safe (modalias, modaliaslen, 11, &vid, NULL);
		fu_firmware_strparse_uint16_safe (modalias, modaliaslen, 16, &pid, NULL);
		fu_firmware_strparse_uint16_safe (modalias, modaliaslen, 21, &rev, NULL);
	}

	if (subsys != NULL && vid != 0x0 && pid != 0x0 && rev != 0x0) {
		g_autofree gchar *devid = NULL;
		devid = g_strdup_printf ("%s\\VID_%04X&PID_%04X&REV_%04X",
					 subsys, vid, pid, rev);
		fu_device_add_instance_id (FU_DEVICE (self), devid);
	}
	if (subsys != NULL && vid != 0x0 && pid != 0x0) {
		g_autofree gchar *devid = NULL;
		devid = g_strdup_printf ("%s\\VID_%04X&PID_%04X",
					 subsys, vid, pid);
		fu_device_add_instance_id (FU_DEVICE (self), devid);
	}
	if (subsys != NULL && vid != 0x0) {
		g_autofree gchar *devid = NULL;
		g_autofree gchar *vendor_id = NULL;
		devid = g_strdup_printf ("%s\\VID_%04X",
					 subsys, vid);
		fu_device_add_instance_id_full (FU_DEVICE (self), devid,
						FU_DEVICE_INSTANCE_FLAG_ONLY_QUIRKS);
		vendor_id = g_strdup_printf ("%s:%04X", subsys, vid);
		fu_device_add_vendor_id (FU_DEVICE (self), vendor_id);
	}
}

static void
fu_bluez_device_to_string (FuDevice *device, guint idt, GString *str)
{
	FuBluezDevice *self = FU_BLUEZ_DEVICE (device);

	/* FuBleDevice->to_string */
	FU_DEVICE_CLASS (fu_bluez_device_parent_class)->to_string (device, idt, str);

	if (self->uuid_paths != NULL) {
		GHashTableIter iter;
		gpointer key, value;
		g_hash_table_iter_init (&iter, self->uuid_paths);
		while (g_hash_table_iter_next (&iter, &key, &value)) {
			fu_common_string_append_kv (str, idt + 1,
						    (const gchar *) key,
						    (const gchar *) value);
		}
	}
}

/*
 * Returns the value of a property of an object specified by its path as
 * a GVariant, or NULL if the property wasn't found.
 */
static GVariant *
fu_bluez_backend_get_property (const gchar *obj_path,
			       const gchar *iface,
			       const gchar *prop_name,
			       GError **error)
{
	g_autoptr(GDBusProxy) proxy = NULL;
	g_autoptr(GVariant) val = NULL;

	proxy = g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SYSTEM,
					       G_DBUS_PROXY_FLAGS_NONE,
					       NULL,
					       "org.bluez",
					       obj_path,
					       iface,
					       NULL,
					       error);
	if (proxy == NULL) {
		g_prefix_error (error, "failed to connect to %s: ", iface);
		return NULL;
	}
	val = g_dbus_proxy_get_cached_property (proxy, prop_name);
	if (val == NULL) {
		g_prefix_error (error, "property %s not found in %s: ",
				prop_name, obj_path);
		return NULL;
	}

	return g_steal_pointer (&val);
}

/*
 * Returns the value of the string property of an object specified by
 * its path, or NULL if the property wasn't found.
 *
 * The returned string must be freed using g_free().
 */
static gchar *
fu_bluez_backend_get_string_property (const gchar *obj_path,
				      const gchar *iface,
				      const gchar *prop_name,
				      GError **error)
{
	g_autoptr(GVariant) val = NULL;
	val = fu_bluez_backend_get_property (obj_path, iface, prop_name, error);
	if (val == NULL)
		return NULL;
	return g_variant_dup_string (val, NULL);
}

/*
 * Populates the {uuid : object_path} entries of a device for all its
 * characteristics.
 *
 * TODO: Extend to services and descriptors too?
 */
static void
fu_bluez_backend_add_device_uuids (FuBluezDevice *self, GVariant *uuids)
{
	g_autolist(GDBusObject) obj_list = NULL;

	obj_list = g_dbus_object_manager_get_objects (self->object_manager);
	for (GList *l = obj_list; l != NULL; l = l->next) {
		GDBusObject *obj = G_DBUS_OBJECT (l->data);
		g_autoptr(GDBusInterface) iface = NULL;
		g_autoptr(GError) error_local = NULL;
		g_autofree gchar *obj_uuid = NULL;
		const gchar *obj_path = g_dbus_object_get_object_path (obj);

		/* not us */
		if (!g_str_has_prefix (g_dbus_object_get_object_path (obj),
				       g_dbus_proxy_get_object_path (self->proxy)))
			continue;

		/*
		 * TODO: Currently restricted to getting UUIDs for
		 * characteristics only, as the only use case we're
		 * going to need for now is reading/writing
		 * characteristics.
		 */
		iface = g_dbus_object_get_interface (obj, "org.bluez.GattCharacteristic1");
		if (iface == NULL)
			continue;
		obj_uuid = fu_bluez_backend_get_string_property (obj_path,
								 "org.bluez.GattCharacteristic1",
								 "UUID",
								 &error_local);
		if (obj_uuid == NULL) {
			g_debug ("failed to get property: %s", error_local->message);
			continue;
		}
		fu_bluez_device_add_uuid_path (self, obj_uuid, obj_path);
	}
}

static gboolean
fu_bluez_device_setup (FuDevice *device, GError **error)
{
	FuBluezDevice *self = FU_BLUEZ_DEVICE (device);
	g_autoptr(GVariant) val_uuids = NULL;

	val_uuids = g_dbus_proxy_get_cached_property (self->proxy, "UUIDs");
	if (val_uuids != NULL)
		fu_bluez_backend_add_device_uuids (self, val_uuids);

	return TRUE;
}

static gboolean
fu_bluez_device_probe (FuDevice *device, GError **error)
{
	FuBluezDevice *self = FU_BLUEZ_DEVICE (device);
	g_autoptr(GVariant) val_adapter = NULL;
	g_autoptr(GVariant) val_address = NULL;
	g_autoptr(GVariant) val_icon = NULL;
	g_autoptr(GVariant) val_modalias = NULL;
	g_autoptr(GVariant) val_name = NULL;

	val_address = g_dbus_proxy_get_cached_property (self->proxy, "Address");
	if (val_address == NULL) {
		g_set_error_literal (error,
				     G_IO_ERROR,
				     G_IO_ERROR_NOT_SUPPORTED,
				     "No required BLE address");
		return FALSE;
	}
	fu_device_set_logical_id (device, g_variant_get_string (val_address, NULL));
	val_adapter = g_dbus_proxy_get_cached_property (self->proxy, "Adapter");
	if (val_adapter != NULL)
		fu_device_set_physical_id (device, g_variant_get_string (val_adapter, NULL));
	val_name = g_dbus_proxy_get_cached_property (self->proxy, "Name");
	if (val_name != NULL)
		fu_device_set_name (device, g_variant_get_string (val_name, NULL));
	val_icon = g_dbus_proxy_get_cached_property (self->proxy, "Icon");
	if (val_icon != NULL)
		fu_device_add_icon (device, g_variant_get_string (val_name, NULL));
	val_modalias = g_dbus_proxy_get_cached_property (self->proxy, "Modalias");
	if (val_modalias != NULL) {
		fu_bluez_device_set_modalias (self, g_variant_get_string (val_modalias, NULL));
	}

	return TRUE;
}

static GByteArray *
fu_bluez_device_read (FuBleDevice *device, const gchar *uuid, GError **error)
{
	FuBluezDevice *self = FU_BLUEZ_DEVICE (device);
	guint8 byte;
	const gchar *path;
	g_autoptr(GByteArray) buf = g_byte_array_new ();
	g_autoptr(GDBusProxy) proxy = NULL;
	g_autoptr(GVariantBuilder) builder = NULL;
	g_autoptr(GVariantIter) iter = NULL;
	g_autoptr(GVariant) val = NULL;

	path = g_hash_table_lookup (self->uuid_paths, uuid);
	if (path == NULL) {
		g_set_error (error,
			     G_IO_ERROR,
			     G_IO_ERROR_NOT_SUPPORTED,
			     "UUID %s not supported", uuid);
		return NULL;
	}
	proxy = g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SYSTEM,
					       G_DBUS_PROXY_FLAGS_NONE,
					       NULL,
					       "org.bluez",
					       path,
					       "org.bluez.GattCharacteristic1",
					       NULL,
					       error);
	if (proxy == NULL) {
		g_prefix_error (error, "Failed to connect GattCharacteristic1: ");
		return NULL;
	}
	g_dbus_proxy_set_default_timeout (proxy, DEFAULT_PROXY_TIMEOUT);

	/*
	 * Call the "ReadValue" method through the proxy synchronously.
	 *
	 * The method takes one argument: an array of dicts of
	 * {string:value} specifing the options (here the option is
	 * "offset":0.
	 * The result is a byte array.
	 */
	builder = g_variant_builder_new (G_VARIANT_TYPE ("a{sv}"));
	g_variant_builder_add (builder, "{sv}", "offset",
			       g_variant_new("q", 0));

	val = g_dbus_proxy_call_sync (proxy,
				      "ReadValue",
				      g_variant_new ("(a{sv})", builder),
				      G_DBUS_CALL_FLAGS_NONE,
				      -1,
				      NULL,
				      error);
	if (val == NULL) {
		g_prefix_error (error, "Failed to read GattCharacteristic1: ");
		return NULL;
	}
	g_variant_get(val, "(ay)", &iter);
	while (g_variant_iter_loop (iter, "y", &byte))
		g_byte_array_append (buf, &byte, 1);

	/* success */
	return g_steal_pointer (&buf);
}

static gboolean
fu_bluez_device_write (FuBleDevice *device,
		       const gchar *uuid,
		       GByteArray *buf,
		       GError **error)
{
	FuBluezDevice *self = FU_BLUEZ_DEVICE (device);
	const gchar *path;
	g_autoptr(GDBusProxy) proxy = NULL;
	g_autoptr(GVariantBuilder) opt_builder = NULL;
	g_autoptr(GVariantBuilder) val_builder = NULL;
	g_autoptr(GVariant) opt_variant = NULL;
	g_autoptr(GVariant) ret = NULL;
	g_autoptr(GVariant) val_variant = NULL;

	path = g_hash_table_lookup (self->uuid_paths, uuid);
	if (path == NULL) {
		g_set_error (error,
			     G_IO_ERROR,
			     G_IO_ERROR_NOT_SUPPORTED,
			     "UUID %s not supported", uuid);
		return FALSE;
	}
	proxy = g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SYSTEM,
					       G_DBUS_PROXY_FLAGS_NONE,
					       NULL,
					       "org.bluez",
					       path,
					       "org.bluez.GattCharacteristic1",
					       NULL,
					       error);
	if (proxy == NULL) {
		g_prefix_error (error, "Failed to connect GattCharacteristic1: ");
		return FALSE;
	}

	/* build the value variant */
	val_builder = g_variant_builder_new (G_VARIANT_TYPE ("ay"));
	for (gsize i = 0; i < buf->len; i++)
		g_variant_builder_add (val_builder, "y", buf->data[i]);
	val_variant = g_variant_new ("ay", val_builder);

	/* build the options variant (offset = 0) */
	opt_builder = g_variant_builder_new(G_VARIANT_TYPE("a{sv}"));
	g_variant_builder_add (opt_builder, "{sv}", "offset",
			       g_variant_new_uint16 (0));
	opt_variant = g_variant_new("a{sv}", opt_builder);

	ret = g_dbus_proxy_call_sync (proxy,
				      "WriteValue",
				      g_variant_new ("(@ay@a{sv})",
						     val_variant,
						     opt_variant),
				      G_DBUS_CALL_FLAGS_NONE,
				      -1,
				      NULL,
				      error);
	if (ret == NULL) {
		g_prefix_error (error, "Failed to write GattCharacteristic1: ");
		return FALSE;
	}

	/* success */
	return TRUE;
}

static void
fu_bluez_device_finalize (GObject *object)
{
	FuBluezDevice *self = FU_BLUEZ_DEVICE (object);

	g_hash_table_unref (self->uuid_paths);
	g_object_unref (self->proxy);
	g_object_unref (self->object_manager);
	G_OBJECT_CLASS (fu_bluez_device_parent_class)->finalize (object);
}

static void
fu_bluez_device_init (FuBluezDevice *self)
{
	fu_device_add_flag (FU_DEVICE (self), FWUPD_DEVICE_FLAG_NO_GUID_MATCHING);
	self->uuid_paths = g_hash_table_new_full (g_str_hash, g_str_equal,
						  g_free, g_free);
}

static void
fu_bluez_device_class_init (FuBluezDeviceClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	FuDeviceClass *device_class = FU_DEVICE_CLASS (klass);
	FuBleDeviceClass *ble_class = FU_BLE_DEVICE_CLASS (klass);

	object_class->finalize = fu_bluez_device_finalize;
	device_class->probe = fu_bluez_device_probe;
	device_class->setup = fu_bluez_device_setup;
	device_class->to_string = fu_bluez_device_to_string;
	ble_class->read = fu_bluez_device_read;
	ble_class->write = fu_bluez_device_write;
}

/**
 * fu_bluez_device_new:
 * @proxy: a #GDBusProxy
 *
 * Creates a new #FuBluezDevice.
 *
 * Returns: (transfer full): a #FuBluezDevice
 *
 * Since: 1.5.7
 **/
FuBluezDevice *
fu_bluez_device_new (GDBusObjectManager *object_manager, GDBusProxy *proxy)
{
	FuBluezDevice *self = g_object_new (FU_TYPE_BLUEZ_DEVICE, NULL);
	self->object_manager = g_object_ref (object_manager);
	self->proxy = g_object_ref (proxy);
	return FU_BLUEZ_DEVICE (self);
}
