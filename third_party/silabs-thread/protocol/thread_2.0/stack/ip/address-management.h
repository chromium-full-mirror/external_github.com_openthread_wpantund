/*
 * File: address-management.h
 * Description: Address Management Functionality
 *
 * Copyright 2015 by Silicon Laboratories. All rights reserved.             *80*
 */

// Address Management TLVs
enum AddressManagementTlv_e {
  TARGET_EID_TLV            = 0,
  EXTENDED_MAC_ADDRESS_TLV  = 1,
  LOCATOR_TLV               = 2,
  ML_EID_TLV                = 3,
  STATUS_TLV                = 4,
  // 5 was ATTACHED_TIME_TLV, which was dropped from the Thread spec
  LAST_TRANSACTION_TIME_TLV = 6,
  ROUTER_MASK_TLV           = 7,
  ND_OPTION_TLV             = 8,
  ND_DATA_TLV               = 9,
  NETWORK_DATA_TLV          = 10
};

typedef enum {
  STATUS_SUCCESS               = 0,
  STATUS_NO_ADDRESS_AVAILABLE  = 1,
  STATUS_TOO_FEW_ROUTERS       = 2,
  STATUS_HAVE_CHILD_ID_REQUEST = 3,
  STATUS_UNUSED                = 0xFF
} AddressManagementStatus;

bool emHandleAddressManagementPost(CoapMessage *coap,
                                   const uint8_t *convertedUri);
EmberStatus emSendAddressQuery(const EmberIpv6Address *address);

EmberStatus emSendAddressSolicitOrRelease(bool isSolicit,
                                          AddressManagementStatus status);
#define emSendAddressSolicit(status) emSendAddressSolicitOrRelease(true, status)
#define emSendAddressRelease()                        \
  emSendAddressSolicitOrRelease(false, STATUS_UNUSED)

EmberStatus emSendAddressErrorNotification(const EmberIpv6Address *destination,
                                           const EmberIpv6Address *targetEid,
                                           const EmberEui64 *mlEid);
