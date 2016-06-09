// File: tmsp-host-utilities.c
//
// Description: Functions to assist processing of TMSP commands and returns.
//
// Copyright 2015 Silicon Laboratories, Inc.                                *80*

#include PLATFORM_HEADER
#include "stack/core/ember-stack.h"
#include "stack/ip/ip-address.h"
#include "stack/ip/host/host-address-table.h"
#include "stack/ip/host/host-listener-table.h"

static EmberNetworkParameters networkCache; // Contains our ULA prefix.
static EmberNetworkStatus statusCache;

EmberIpv6Address hostIpAddress; // Mesh-local 64 address
EmberEui64 localEui64 = {{ 0 }};  // Useful to construct Link-local 64 address
#ifndef RTOS
uint8_t emMacExtendedId[8] = { 0 };
#endif

//------------------------------------------------------------------------------
// Cached Stack info

const EmberEui64 *emberEui64(void)
{
  return &localEui64;
}

EmberPanId emberGetPanId(void)
{
  return networkCache.panId;
}

void emberGetNetworkParameters(EmberNetworkParameters *networkParams)
{
  MEMCOPY(networkParams, &networkCache, sizeof(EmberNetworkParameters));
}

EmberNetworkStatus emberNetworkStatus(void)
{
  return statusCache;
}

bool emberGetLocalIpAddress(uint8_t index, EmberIpv6Address *address)
{
  if (index == 0 && ! emIsMemoryZero(hostIpAddress.bytes, 16)) {
    // ML64
    MEMCOPY(address->bytes, hostIpAddress.bytes, 16);
    return true;
  } else if (index == 1) {
    // LL64
    emStoreLongFe8Address(emMacExtendedId, address->bytes);
    return true;
  } else if (index > 1) {
    GlobalAddressEntry *entry = emberGetHostGlobalAddress(index - 2);
    if (entry != NULL) {
      MEMCOPY(address->bytes, entry->address, 16);
      return true;
    }
  }
  return false;
}

void emberSetAddressHandler(const uint8_t *addr)
{
  EmberIpv6Address address;
  MEMCOPY(address.bytes, addr, 16);
  if (emIsDefaultGlobalPrefix(address.bytes)) {
    // Store the ULA.
    MEMCOPY(hostIpAddress.bytes, address.bytes, 16);

    uint8_t meshLocalIdentifier[8];
    emberReverseMemCopy(meshLocalIdentifier,
                        address.bytes + 8,
                        8);
    meshLocalIdentifier[7] ^= 0x02;

    // Configure these for the first time (only once).
    emSetMeshLocalIdentifier(meshLocalIdentifier);
#ifndef UNIX_HOST_SIM
    emberConfigureDefaultHostAddress(&address);
#endif
  } else {
#ifndef UNIX_HOST_SIM
    emberConfigureGlobalHostAddress(&address, MAX_INT32U_VALUE, MAX_INT32U_VALUE, 0);
#endif
  }
}

void emberHostStateHandler(const EmberNetworkParameters *parameters,
                           const EmberEui64 *eui64,
                           const EmberEui64 *extendedMacId,
                           EmberNetworkStatus networkStatus)
{
  MEMCOPY(&networkCache, parameters, sizeof(EmberNetworkParameters));
  MEMCOPY(localEui64.bytes, eui64->bytes, EUI64_SIZE);
  MEMCOPY(emMacExtendedId, extendedMacId, sizeof(EmberEui64));
  statusCache = networkStatus;

#if (defined(UNIX_HOST) || defined(UNIX_HOST_SIM))
  // Set the legacy ULA, and also glean whether it's on the lurker network.
  // See host-address-table.c.
  emSetLegacyUla(networkCache.legacyUla.bytes);
#endif
}

bool tmspStateReturnPreHook(const EmberNetworkParameters *parameters,
                            const EmberEui64 *eui64,
                            const EmberEui64 *extendedMacId,
                            EmberNetworkStatus networkStatus)
{
  emberHostStateHandler(parameters, eui64, extendedMacId, networkStatus);
  return true;
}

// no-op not currently used NCP-to-Host.
void emberNcpToHostNoOp(const uint8_t *bytes, uint8_t bytesLength) {}

//------------------------------------------------------------------------------
// TMSP Hooks
bool tmspHostResetNetworkStatePreHook(void)
{
  // Reset host buffers and tables.
#ifndef RTOS
  emInitializeBuffers();
#endif
  emberInitializeHostAddressTable();
  emberCloseListeners();
  MEMSET(hostIpAddress.bytes, 0, 16);
  return true;
}

bool tmspHostResignGlobalAddressPreHook(const EmberIpv6Address *address)
{
  emberRemoveHostAddress(address);
  return true;
}

bool tmspHostResetMicroHandlerPreHook(void)
{
  emberCloseListeners();
  return true;
}

bool tmspHostAddressConfigurationChangePreHook(const EmberIpv6Address *address,
                                               uint32_t preferredLifetime,
                                               uint32_t validLifetime,
                                               uint8_t addressFlags)
{
  // Add it to our global address table.
  EmberStatus status = emberAddHostGlobalAddress(address,
                                                 preferredLifetime,
                                                 validLifetime,
                                                 addressFlags);

#ifdef UNIX_HOST_SIM
  return true;
#else
  if (status == EMBER_SUCCESS) {
    emberConfigureGlobalHostAddress(address,
                                    preferredLifetime,
                                    validLifetime,
                                    addressFlags);
  }
  return false;
#endif
}

void tmspHostSetEui64PostHook(const EmberEui64 *eui64)
{
  MEMCOPY(localEui64.bytes, eui64->bytes, 8);
}

bool tmspHostFormNetworkPreHook(uint16_t *options)
{
  // Host->NCP does not convey extended PAN ID.
  // Ensure the corresponding options bit is cleared.
  *options = *options & ~(EMBER_EXTENDED_PAN_ID_OPTION);
  return true;
}
