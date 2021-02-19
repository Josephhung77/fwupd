/*
 * Copyright (C) 2021 Ricardo Cañuelo <ricardo.canuelo@collabora.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#pragma once

#include "fu-bluez-device.h"

FuBluezDevice		*fu_bluez_device_new		(GDBusObjectManager	*object_manager,
							 GDBusProxy		*proxy);
