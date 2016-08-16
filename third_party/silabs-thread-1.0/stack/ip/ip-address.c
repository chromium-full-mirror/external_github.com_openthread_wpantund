/*
 * File: ip-address.c
 * Description: IPv6 address utilities
 * Author(s): Richard Kelsey, Matteo Paris
 *
 * Copyright 2013 Silicon Laboratories, Inc.                                *80*
 */

#include "core/ember-stack.h"
#include "rip.h"
#include "ip-address.h"

// See RFC 4291 (IPv6 Addressing Architecture)

//----------------------------------------------------------------
// Mesh local identifier

uint8_t emMeshLocalIdentifier[8];

bool emberIsMeshLocalIdentifier(const uint8_t *identifier)
{
  return (MEMCOMPARE(identifier, emMeshLocalIdentifier, 8) == 0);
}

void emSetMeshLocalIdentifier(const uint8_t *identifier)
{
  MEMCOPY(emMeshLocalIdentifier, identifier, 8);
}

//----------------------------------------------------------------
// Interface Ids

// This six bytes prepended to a short ID to get an inteface ID.
const uint8_t emShortInterfaceIdPrefix[6] = {0x00, 0x00, 0x00, 0xFF, 0xFE, 0x00};

static bool isShortInterfaceId(const uint8_t *interfaceId)
{
  return MEMCOMPARE(emShortInterfaceIdPrefix, interfaceId, 6) == 0;
}

void emStoreShortInterfaceId(uint16_t id, uint8_t *target)
{
  MEMCOPY(target, emShortInterfaceIdPrefix, 6);
  emberStoreHighLowInt16u(target + 6, id);
}

bool emGleanShortIdFromInterfaceId(const uint8_t *interfaceId, uint16_t* shortId)
{
  if (isShortInterfaceId(interfaceId)) {
    if (shortId != NULL) {
      *shortId = emberFetchHighLowInt16u(interfaceId + 6);
    }
    return true;
  }
  return false;
}

// emStoreLongInterfaceId(eui, target) is a macro

bool emIsLocalLongInterfaceId(const uint8_t* interfaceId)
{
  uint8_t unmodified[8];
  emUnmodifyInterfaceId(interfaceId, unmodified);
  return (MEMCOMPARE(unmodified, emMeshLocalIdentifier, 8) == 0
          || MEMCOMPARE(unmodified, emMacExtendedId, 8) == 0);
}

// Bit 7 of a mesh local identifier (longId) is set to 0 for globally-unique
// addresses (those that have been allocated by some infallible organizational
// system) and 1 for locally-administered ones.  The folks that came up with
// IPv6 reversed the meaning of this bit when an EUI64 is used as an interface
// ID.  The notion was that this would allow sysadmins to type in a local
// address as FE80::xxxx instead of having to type FE80::0200:0:0:xxxx.  So we
// have to flip the bit back and forth. On top of this, 802.15.4 and IPv6 store
// the bytes in the opposite order.

void emModifyLongId(const uint8_t *longId, uint8_t *target)
{
  emberReverseMemCopy(target, longId, 8);
  target[0] ^= 0x02;
}

void emUnmodifyInterfaceId(const uint8_t *interfaceId, uint8_t *longId)
{
  emberReverseMemCopy(longId, interfaceId, 8);
  longId[7] ^= 0x02;
}

// ND is set up in such a way that we have to use the short ID in an
// interface before it has been assigned to us.  This means that we
// have to recognize the interface before setting the local node ID.

bool emIsLocalShortInterfaceId(const uint8_t *interfaceId)
{
  EmberNodeId shortId;
  EmberNodeId myShortId = emberGetNodeId();
  return (emGleanShortIdFromInterfaceId(interfaceId, &shortId)
          && myShortId != EM_USE_LONG_ADDRESS
          && myShortId == shortId);
}

//----------------------------------------------------------------
// The loopback address just has the low bit set.

bool emIsLoopbackAddress(const uint8_t *address)
{
  return (emIsMemoryZero(address, 15) && address[15] == 1);
}

//----------------------------------------------------------------
// Addresses using the default global prefix.

// GP16 = global prefix 16, the IPv6 address formed from the default global
// prefix, the short interface ID prefix, and the short MAC ID.

bool emStoreGp16(uint16_t id, uint8_t *target)
{
  emStoreShortInterfaceId(id, target + DEFAULT_PREFIX_BYTES);
  return emStoreDefaultGlobalPrefix(target);
}

bool emIsGp16(const uint8_t *address, uint16_t *returnShort)
{
  return (emIsDefaultGlobalPrefix(address)
          && emGleanShortIdFromInterfaceId(address + DEFAULT_PREFIX_BYTES,
                                           returnShort));
}

// Ditto, for the EUI64-based interface identifier.

bool emStoreGp64(const uint8_t *eui, uint8_t *target)
{
  emStoreLongInterfaceId(eui, target + DEFAULT_PREFIX_BYTES);
  return emStoreDefaultGlobalPrefix(target);
}

bool emIsGp64(const uint8_t *address)
{
  return (emIsDefaultGlobalPrefix(address)
          && ! isShortInterfaceId(address + 8));
}

//----------------------------------------------------------------
// Link local addresses

// The link-local prefix.
const Bytes8 emFe8Prefix = {{ 0xFE, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }};

void emStoreLongFe8Address(uint8_t *eui64, uint8_t *target)
{
  MEMCOPY(target, emFe8Prefix.contents, 8);
  emStoreLongInterfaceId(eui64, target + 8);
}

void emStoreShortFe8Address(uint16_t id, uint8_t *target)
{
  MEMCOPY(target, emFe8Prefix.contents, 8);
  emStoreShortInterfaceId(id, target + 8);
}

bool emIsFe8Address(const uint8_t *address)
{
  return (MEMCOMPARE(address, emFe8Prefix.contents, 8) == 0);
}

bool emIsLl16(const uint8_t *address, uint16_t *returnShort)
{
  return (emIsFe8Address(address)
          && emGleanShortIdFromInterfaceId(address + DEFAULT_PREFIX_BYTES,
                                           returnShort));
}

bool emIsLl64(const uint8_t *address)
{
  return (emIsFe8Address(address)
          && ! isShortInterfaceId(address + 8));
}

bool emIsLocalLl64(const uint8_t *address)
{
  return (emIsFe8Address(address)
          && emIsLocalLongInterfaceId(address + 8));
}

bool emStoreDefaultIpAddress(uint8_t *target)
{
  return (emberGetNodeId() < EM_USE_LONG_ADDRESS
          ? emStoreGp16(emberGetNodeId(), target)
          : emStoreGp64(emMeshLocalIdentifier, target));
}

bool emIsUnicastForMe(const uint8_t *address)
{
  return (emIsLocalIpAddress(address)
          || emIsLoopbackAddress(address));
}

void emStoreDefaultFe8Address(uint8_t *target)
{
  if (emberGetNodeId() < EM_USE_LONG_ADDRESS) {
    emStoreShortFe8Address(emberGetNodeId(), target);
  } else {
    emStoreLongFe8Address(emMacExtendedId, target);
  }
}

//----------------------------------------------------------------
// Multicast

// See http://www.iana.org/assignments/ipv6-multicast-addresses/

// FF01::XX = interface-local scope
// FF02::XX = link-local scope
// FF03::XX = subnet-local scope
// FF05::XX = site-local scope
//
// FF0X::1 = all nodes address
// FF0X::2 = all routers address
// FF0X::1A = all RPL nodes address
// FF0X::FB = mDNSv6 address

// FF32:XX::1 - Link local All Thread Nodes multicast
// FF32:XX::2 - Link local All Thread Routers multicast
// FF33:XX::1 - Realm local All Thread Nodes multicast
// FF33:XX::2 - Realm local All Thread Routers multicast

bool emIsFf32MulticastAddress(const uint8_t *address)
{
  uint8_t prefix[16];

  if (emStoreDefaultGlobalPrefix(prefix)) {
    return (address[0] == 0xFF
            && address[1] == 0x32
            && address[3] == 0x40
            && MEMCOMPARE(address + 4, prefix, 8) == 0);
  }
  return false;
}

bool emIsFf33MulticastAddress(const uint8_t *address)
{
  uint8_t prefix[16];

  if (emStoreDefaultGlobalPrefix(prefix)) {
    return (address[0] == 0xFF
            && address[1] == 0x33
            && address[3] == 0x40
            && MEMCOMPARE(address + 4, prefix, 8) == 0);
  }

  return false;
}

bool emIsFf01MulticastAddress(const uint8_t *address)
{
  return (address[0] == 0xFF && address[1] == 0x01);
}

bool emIsFf02MulticastAddress(const uint8_t *address)
{
  return (address[0] == 0xFF && address[1] == 0x02);
}

bool emIsFf03MulticastAddress(const uint8_t *address)
{
  return (address[0] == 0xFF && address[1] == 0x03);
}

// Once we get enough of these it may become more efficient to create
// them on the fly.

// Link local - all nodes.
const Bytes16 emFf02AllNodesMulticastAddress =
  {{ 0xFF, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01 }};

// Link local - all routers.
const Bytes16 emFf02AllRoutersMulticastAddress =
  {{ 0xFF, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02 }};

// Realm (mesh) local - all nodes.
const Bytes16 emFf03AllNodesMulticastAddress =
  {{ 0xFF, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01 }};

// Realm (mesh) local - all routers.
const Bytes16 emFf03AllRoutersMulticastAddress =
  {{ 0xFF, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02 }};

// Site local - all nodes.
const Bytes16 emFf05AllNodesMulticastAddress =
  {{ 0xFF, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01 }};

// Site local - all routers.
const Bytes16 emFf05AllRoutersMulticastAddress =
  {{ 0xFF, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02 }};

// Link local - all RPL nodes.
const Bytes16 emFf02AllRplNodesMulticastAddress =
  {{ 0xFF, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1A }};

// RFC 3315 has a All_DHCP_Relay_Agents_and_Servers (FF02::1:2).
// We need the same thing but with the scope one tick larger.

const Bytes16 emFf03AllDhcpMulticastAddress =
  {{ 0xFF, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
     0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x03 }};

const Bytes16 emFf33AllThreadNodesMulticastAddress =
  {{ 0xFF, 0x33, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01 }};
