/*
 * Copyright (C) 2021 Ricardo Cañuelo <ricardo.canuelo@collabora.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#define G_LOG_DOMAIN				"FuBleDevice"

#include "config.h"

#include "fu-ble-device.h"

G_DEFINE_TYPE (FuBleDevice, fu_ble_device, FU_TYPE_DEVICE)

/**
 * fu_ble_device_read:
 * @self: A #FuBleDevice
 * @uuid: The UUID, e.g. `00cde35c-7062-11eb-9439-0242ac130002`
 * @error: A #GError, or %NULL
 *
 * Reads from a UUID on the device.
 *
 * Returns: (transfer full): data, or %NULL for error
 *
 * Since: 1.5.7
 **/
GByteArray *
fu_ble_device_read (FuBleDevice *self, const gchar *uuid, GError **error)
{
	FuBleDeviceClass *klass = FU_BLE_DEVICE_GET_CLASS (self);
	if (klass->read == NULL) {
		g_set_error_literal (error,
				     G_IO_ERROR,
				     G_IO_ERROR_NOT_SUPPORTED,
				     "not supported");
		return NULL;
	}
	return klass->read (self, uuid, error);
}

/**
 * fu_ble_device_read_string:
 * @self: A #FuBleDevice
 * @uuid: The UUID, e.g. `00cde35c-7062-11eb-9439-0242ac130002`
 * @error: A #GError, or %NULL
 *
 * Reads a string from a UUID on the device.
 *
 * Returns: (transfer full): data, or %NULL for error
 *
 * Since: 1.5.7
 **/
gchar *
fu_ble_device_read_string (FuBleDevice *self, const gchar *uuid, GError **error)
{
	GByteArray *buf = fu_ble_device_read (self, uuid, error);
	if (buf == NULL)
		return NULL;
	return (gchar *) g_byte_array_free (buf, FALSE);
}

/**
 * fu_ble_device_write:
 * @self: A #FuBleDevice
 * @uuid: The UUID, e.g. `00cde35c-7062-11eb-9439-0242ac130002`
 * @error: A #GError, or %NULL
 *
 * Writes to a UUID on the device.
 *
 * Returns: (transfer full): data, or %NULL for error
 *
 * Since: 1.5.7
 **/
gboolean
fu_ble_device_write (FuBleDevice *self,
		     const gchar *uuid,
		     GByteArray *buf,
		     GError **error)
{
	FuBleDeviceClass *klass = FU_BLE_DEVICE_GET_CLASS (self);
	if (klass->write == NULL) {
		g_set_error_literal (error,
				     G_IO_ERROR,
				     G_IO_ERROR_NOT_SUPPORTED,
				     "not supported");
		return FALSE;
	}
	return klass->write (self, uuid, buf, error);
}

static void
fu_ble_device_incorporate (FuDevice *self, FuDevice *donor)
{
	FuBleDeviceClass *kself = FU_BLE_DEVICE_GET_CLASS (self);
	FuBleDeviceClass *kdonor = FU_BLE_DEVICE_GET_CLASS (donor);
	if (kself->read == NULL)
		kself->read = kdonor->read;
	if (kself->write == NULL)
		kself->write = kdonor->write;
}

static void
fu_ble_device_to_string (FuDevice *device, guint idt, GString *str)
{
}

static void
fu_ble_device_init (FuBleDevice *self)
{
}

static void
fu_ble_device_class_init (FuBleDeviceClass *klass)
{
	FuDeviceClass *device_class = FU_DEVICE_CLASS (klass);
	device_class->to_string = fu_ble_device_to_string;
	device_class->incorporate = fu_ble_device_incorporate;
}

/**
 * fu_ble_device_new:
 *
 * Creates a new #FuBleDevice.
 *
 * Returns: (transfer full): a #FuBleDevice
 *
 * Since: 1.5.7
 **/
FuBleDevice *
fu_ble_device_new (void)
{
	FuBleDevice *self = g_object_new (FU_TYPE_BLE_DEVICE, NULL);
	return FU_BLE_DEVICE (self);
}
