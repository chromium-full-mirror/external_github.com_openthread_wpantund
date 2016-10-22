/** @file tmsp-host-utilities.h
 *
 *  Utilities to support Host-side processing of TMSP commands and returns.
 *
 * <!--Copyright 2015 by Silicon Labs. All rights reserved.              *80*-->
 */

#ifndef __TMSP_HOST_UTILITIES_H__
#define __TMSP_HOST_UTILITIES_H__

bool tmspHostResetNetworkStatePreHook(void);
bool tmspHostResignGlobalAddressPreHook(const EmberIpv6Address *address);
bool tmspHostResetMicroHandlerPreHook(void);
bool tmspHostAddressConfigurationChangePreHook(const EmberIpv6Address *address,
                                               uint32_t preferredLifetime,
                                               uint32_t validLifetime,
                                               uint8_t addressFlags);
void tmspHostSetEui64PostHook(const EmberEui64 *eui64);
bool tmspHostFormNetworkPreHook(uint16_t *options);
bool tmspStateReturnPreHook(const EmberNetworkParameters *parameters,
                            const EmberEui64 *eui64,
                            const EmberEui64 *extendedMacId,
                            EmberNetworkStatus networkStatus);

#endif //__TMSP_HOST_UTILITIES_H__
