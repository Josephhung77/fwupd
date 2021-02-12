/*
 * Copyright (C) 2021 Ricardo Cañuelo <ricardo.canuelo@collabora.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#pragma once

#include <glib-object.h>

#include "fu-device.h"

#define FU_TYPE_BLE_DEVICE (fu_ble_device_get_type ())
G_DECLARE_DERIVABLE_TYPE (FuBleDevice, fu_ble_device, FU, BLE_DEVICE, FuDevice)

struct _FuBleDeviceClass
{
	FuDeviceClass	parent_class;
	GByteArray	*(*read)			(FuBleDevice	*self,
							 const gchar	*uuid,
							 GError		**error);
	gboolean	 (*write)			(FuBleDevice	*self,
							 const gchar	*uuid,
							 GByteArray	*buf,
							 GError		**error);
	gpointer	__reserved[28];
};

FuBleDevice		*fu_ble_device_new		(void);
GByteArray		*fu_ble_device_read		(FuBleDevice	*self,
							 const gchar	*uuid,
							 GError		**error);
gchar			*fu_ble_device_read_string	(FuBleDevice	*self,
							 const gchar	*uuid,
							 GError		**error);
gboolean		 fu_ble_device_write		(FuBleDevice	*self,
							 const gchar	*uuid,
							 GByteArray	*buf,
							 GError		**error);
