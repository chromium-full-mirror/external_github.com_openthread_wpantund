/*
 *
 *    Copyright (c) 2016 Nest Labs, Inc.
 *    All rights reserved.
 *
 *    This document is the property of Nest. It is considered
 *    confidential and proprietary information.
 *
 *    This document may not be reproduced or transmitted in any form,
 *    in whole or in part, without the express written permission of
 *    Nest.
 *
 *    Description:
 *      This file contains the virtual base class for the FirmwareInterface
 *      class.
 *
 */

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include "assert-macros.h"
#include "FirmwareInterface.h"
#include <syslog.h>
#include <errno.h>
#include "FirmwareInterface-Base.h"
#include "FirmwareInterface-1.1.3.h"
#include "FirmwareInterface-2.0.0.h"
#include "FirmwareInterface-Thread-1.0.0.h"
#include <boost/algorithm/string/predicate.hpp>

using namespace nl;
using namespace wpantund;
using namespace EmberCommands;

const uint32_t FirmwareInterface::kAllChannelsMask = 0x07FFF800UL;

Data
EmberCommands::StripNCPHeader(const Data& data_in)
{
	Data data(data_in);

	if (data.size() >= sizeof(ncp_header_t)) {
		data.erase(data.begin(), data.begin() + sizeof(ncp_header_t));
	}

	return data;
}

const char*
FirmwareInterface::reset_cause_to_cstr(FirmwareInterface::resetCause reason) {
	const char* reason_cstr = NULL;
	switch(reason) {
	case EmberCommands::FirmwareInterface::RESET_FIB:
		reason_cstr = "FIB";
		break;
	case EmberCommands::FirmwareInterface::RESET_BOOTLOADER:
		reason_cstr = "BOOTLOADER";
		break;
	case EmberCommands::FirmwareInterface::RESET_EXTERNAL:
		reason_cstr = "EXTERNAL";
		break;
	case EmberCommands::FirmwareInterface::RESET_POWERON:
		reason_cstr = "POWERON";
		break;
	case EmberCommands::FirmwareInterface::RESET_WATCHDOG:
		reason_cstr = "WATCHDOG";
		break;
	case EmberCommands::FirmwareInterface::RESET_SOFTWARE:
		reason_cstr = "SOFTWARE";
		break;
	case EmberCommands::FirmwareInterface::RESET_CRASH:
		reason_cstr = "CRASH";
		break;
	case EmberCommands::FirmwareInterface::RESET_FLASH:
		reason_cstr = "FLASH";
		break;
	case EmberCommands::FirmwareInterface::RESET_FATAL:
		reason_cstr = "FATAL";
		break;
	case EmberCommands::FirmwareInterface::RESET_FAULT:
		reason_cstr = "FAULT";
		break;

	default:
	case EmberCommands::FirmwareInterface::RESET_UNKNOWN:
		reason_cstr = "UNKNOWN";
		break;
	}
	return reason_cstr;
}

NCPState
FirmwareInterface::NCPStateConvertFromEmber(uint8_t association_state)
{
	NCPState ret;

	switch (association_state) {
	case 0: // NO_NETWORK
		ret = OFFLINE;
		break;

	case 1: // SAVED_NETWORK
		ret = COMMISSIONED;
		break;

	case 2: // JOINING_NETWORK
		ret = ASSOCIATING;
		break;

	case 3: // JOINED_NETWORK
		ret = ASSOCIATED;
		break;

	case 4: // JOINED_NETWORK_NO_PARENT
		ret = ISOLATED;
		break;

	default:
		ret = FAULT;
		break;
	}

	return ret;
}

std::string
FirmwareInterface::configure_gateway_flags_to_string(uint8_t flags)
{
	std::string ret;
	int i;
	for (i = 7; i >=0 ; --i) {
		uint8_t mask = (1<<i);
		if (i == 3) {
			ret += ' ';
		}
		if (!(mask & flags)) {
			ret += '-';
			continue;
		}
		switch (mask) {
		case DEFAULT_ROUTE_FLAG:
			ret += 'd';
			break;
		case CONFIGURE_FLAG:
			ret += 'C';
			break;
		case DHCP_FLAG:
			ret += 'D';
			break;
		case SLAAC_VALID_FLAG:
			ret += 'V';
			break;
		case SLAAC_PREFERRED_FLAG:
			ret += 'P';
			break;
		case HIGH_PREFERENCE:
			ret += 'H';
			break;
		case TEMPORARY_FLAG:
			ret += 'T';
			break;
		default:
			ret += '0' + i;
			break;
		}
	}
	return ret;
}


boost::shared_ptr<FirmwareInterface>
FirmwareInterface::GetFirmwareInterfaceForVersion(int management_version, const std::string& version_string)
{
	boost::shared_ptr<FirmwareInterface> ret;
	int i;
	struct {
		const char *prefix;
		int minimum_next_field_value;
		int minimum_management_version;
		int maximum_management_version;
		boost::shared_ptr<FirmwareInterface> (*alloc)(int management_version, const std::string& version_string);
	} table[] = {
		{"ConnectIP/1.1.3b",  79, 3, 5, &FirmwareInterface_1_1_3::alloc},
		{"ConnectIP/2.0.0b", 300, 7, 9, &FirmwareInterface_2_0_0::alloc},
		{"ConnectIP/2.0.",     1, 7, 9, &FirmwareInterface_2_0_0::alloc},
		{"Thread/1.0.3b", 392, 0x0D00, 0x0D00, &FirmwareInterface_Thread_1_0_0::alloc},
	};

	if (version_string.empty()) {
		ret = FirmwareInterface_Base::alloc(management_version, version_string);
	} else {
		for (i = 0; i < sizeof(table) / sizeof(*table); i++) {
			if (boost::starts_with(version_string, table[i].prefix)
				&& (management_version >= table[i].minimum_management_version)
				&& (management_version <= table[i].maximum_management_version)
				&& (strtol(version_string.c_str() + strlen(table[i].prefix), NULL, 10) >= table[i].minimum_next_field_value)
			) {
				ret = table[i].alloc(management_version, version_string);
				break;
			}
		}
	}

	if (!ret) {
		throw unsupported_firmware_version();
	}

	return ret;
}

FirmwareInterface::~FirmwareInterface()
{
}
