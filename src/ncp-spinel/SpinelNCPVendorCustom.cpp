/*
 *
 * Copyright (c) 2018 Nest Labs, Inc.
 * All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include "SpinelNCPVendorCustom.h"

#include <syslog.h>
#include <errno.h>

#include "assert-macros.h"
#include "time-utils.h"
#include "any-to.h"
#include "spinel-extra.h"
#include "IPv6Helpers.h"
#include "SpinelNCPInstance.h"
#include "SpinelNCPTask.h"
#include "SpinelNCPTaskSendCommand.h"

#include "spinel-vendor.h"

using namespace nl;
using namespace wpantund;

// Nest specific wpantund properties (name MUST start with "nest:" prefix).

#define kWPANTUNDProperty_NestPAWDestAddress    "nest:PAW:DestAddress"    // Proximity Advance Wakeup (PAW)

SpinelNCPVendorCustom::SpinelNCPVendorCustom(SpinelNCPInstance* instance):
	mInstance(instance)
{
	// Warning: `instance` hasn't yet been fully constructed at this point.

	mSupportedProperties.insert(kWPANTUNDProperty_NestPAWDestAddress);

}

SpinelNCPVendorCustom::~SpinelNCPVendorCustom()
{
	// Warning: `instance` has been partially destructed at this point.
}

bool
SpinelNCPVendorCustom::setup_property_supported_by_class(const std::string& prop_name)
{
	return false;
}

const std::set<std::string>&
SpinelNCPVendorCustom::get_supported_property_keys()const
{
	return mSupportedProperties;
}

bool
SpinelNCPVendorCustom::is_property_key_supported(const std::string& key)const
{
	return get_supported_property_keys().count(key) != 0;
}

void
SpinelNCPVendorCustom::property_get_value(const std::string& key, CallbackWithStatusArg1 cb)
{
	if (strcaseequal(key.c_str(), kWPANTUNDProperty_NestPAWDestAddress)) {
		mInstance->start_new_task(SpinelNCPTaskSendCommand::Factory(mInstance)
			.set_callback(cb)
			.add_command(SpinelPackData(SPINEL_FRAME_PACK_CMD_PROP_VALUE_GET, SPINEL_PROP_VENDOR_NEST_PAW_DEST_ADDR))
			.set_reply_format(SPINEL_DATATYPE_IPv6ADDR_S)
			.finish()
		);

	} else {
		cb(kWPANTUNDStatus_FeatureNotSupported,
			boost::any(std::string("Cannot get property value for ") + key));
	}
}

void
SpinelNCPVendorCustom::property_set_value(const std::string& key, const boost::any& value, CallbackWithStatus cb)
{
	if (strcaseequal(key.c_str(), kWPANTUNDProperty_NestPAWDestAddress)) {
		struct in6_addr addr = any_to_ipv6(value);
		Data command =
			SpinelPackData(
				SPINEL_FRAME_PACK_CMD_PROP_VALUE_SET(SPINEL_DATATYPE_IPv6ADDR_S),
				SPINEL_PROP_VENDOR_NEST_PAW_DEST_ADDR,
				&addr
			);

		mInstance->mSettings[kWPANTUNDProperty_NestPAWDestAddress] = SpinelNCPInstance::SettingsEntry(command);

		mInstance->start_new_task(SpinelNCPTaskSendCommand::Factory(mInstance)
			.set_callback(cb)
			.add_command(command)
			.finish()
		);

	} else {
		cb(kWPANTUNDStatus_FeatureNotSupported);
	}
}

void
SpinelNCPVendorCustom::property_insert_value(const std::string& key, const boost::any& value, CallbackWithStatus cb)
{
	if (strcaseequal(key.c_str(), "__CustomKeyHere__")) {
		cb(kWPANTUNDStatus_Ok);

	} else {
		cb(kWPANTUNDStatus_FeatureNotSupported);
	}
}

void
SpinelNCPVendorCustom::property_remove_value(const std::string& key, const boost::any& value, CallbackWithStatus cb)
{
	if (strcaseequal(key.c_str(), "__CustomKeyHere__")) {
		cb(kWPANTUNDStatus_Ok);

	} else {
		cb(kWPANTUNDStatus_FeatureNotSupported);
	}
}

cms_t
SpinelNCPVendorCustom::get_ms_to_next_event(void)
{
	return CMS_DISTANT_FUTURE;
}

void
SpinelNCPVendorCustom::process(void)
{
	// Do nothing for now.
}
