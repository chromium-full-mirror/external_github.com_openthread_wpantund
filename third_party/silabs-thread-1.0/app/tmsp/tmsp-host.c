// File: tmsp-host.c
//
// *** Generated file. Do not edit! ***
//
// run tmsp-update.sh to regenerate
//
// Description: Host functions for sending Thread management commands
// to the NCP.
//
// TMSP Version: 13.0
//
// Copyright 2016 Silicon Laboratories, Inc.                                *80*

#define EMBER_APPLICATION_HAS_BINARY_COMMAND_INTERPRETER

#include PLATFORM_HEADER
#ifdef CONFIGURATION_HEADER
  #include CONFIGURATION_HEADER
#endif
#include "include/ember.h"
#include "include/thread-debug.h"
#include "app/ip-ncp/binary-management.h"
#include "app/util/serial/command-interpreter2.h"
#include "app/tmsp/tmsp-enum.h"
#include "app/tmsp/tmsp-frame-utilities.h"
#include "app/tmsp/tmsp-host-utilities.h"
#include "stack/ip/host/host-mfglib.h"


//------------------------------------------------------------------------------
// Core
//------------------------------------------------------------------------------

void emberResetMicro(void)
{
  emSendBinaryManagementCommand(EMBER_RESET_MICRO_COMMAND_IDENTIFIER,
                                "");
}

void emberResetNetworkState(void)
{
  bool preHookResult = tmspHostResetNetworkStatePreHook();
  if (preHookResult) {
    emSendBinaryManagementCommand(EMBER_RESET_NETWORK_STATE_COMMAND_IDENTIFIER,
                                  "");
  }
}

void emberInitHost(void)
{
  emSendBinaryManagementCommand(EMBER_INIT_HOST_COMMAND_IDENTIFIER,
                                "");
}

void emberState(void)
{
  emSendBinaryManagementCommand(EMBER_STATE_COMMAND_IDENTIFIER,
                                "");
}

void emberGetVersions(void)
{
  emSendBinaryManagementCommand(EMBER_GET_VERSIONS_COMMAND_IDENTIFIER,
                                "");
}

void emberFormNetwork(const EmberNetworkParameters *parameters,
                      uint16_t options,
                      uint32_t channelMask)
{
  bool preHookResult = tmspHostFormNetworkPreHook(&options);
  if (preHookResult) {
    emSendBinaryManagementCommand(EMBER_FORM_NETWORK_COMMAND_IDENTIFIER,
                                  "bbusvwb",
                                  parameters->networkId,
                                  sizeof(parameters->networkId),
                                  parameters->ulaPrefix.bytes,
                                  sizeof(parameters->ulaPrefix.bytes),
                                  parameters->nodeType,
                                  parameters->radioTxPower,
                                  options,
                                  channelMask,
                                  parameters->legacyUla.bytes,
                                  sizeof(parameters->legacyUla.bytes));
  }
}

void emberJoinNetwork(const EmberNetworkParameters *parameters,
                      uint16_t options,
                      uint32_t channelMask)
{
  emSendBinaryManagementCommand(EMBER_JOIN_NETWORK_COMMAND_IDENTIFIER,
                                "bbvusbuvw",
                                parameters->networkId,
                                sizeof(parameters->networkId),
                                parameters->extendedPanId,
                                sizeof(parameters->extendedPanId),
                                parameters->panId,
                                parameters->nodeType,
                                parameters->radioTxPower,
                                parameters->joinKey,
                                sizeof(parameters->joinKey),
                                parameters->joinKeyLength,
                                options,
                                channelMask);
}

void emberResumeNetwork(void)
{
  emSendBinaryManagementCommand(EMBER_RESUME_NETWORK_COMMAND_IDENTIFIER,
                                "");
}

void emberAttachToNetwork(void)
{
  emSendBinaryManagementCommand(EMBER_ATTACH_TO_NETWORK_COMMAND_IDENTIFIER,
                                "");
}

void emberHostJoinClientComplete(uint32_t keySequence,
                                 const uint8_t *key,
                                 const uint8_t *ulaPrefix)
{
  emSendBinaryManagementCommand(EMBER_HOST_JOIN_CLIENT_COMPLETE_COMMAND_IDENTIFIER,
                                "wbb",
                                keySequence,
                                key, EMBER_ENCRYPTION_KEY_SIZE,
                                ulaPrefix, 8);
}

void emberSetSecurityParameters(const EmberSecurityParameters *parameters,
                                uint16_t options)
{
  emSendBinaryManagementCommand(EMBER_SET_SECURITY_PARAMETERS_COMMAND_IDENTIFIER,
                                "bbv",
                                parameters->networkKey,
                                EMBER_ENCRYPTION_KEY_SIZE,
                                parameters->presharedKey,
                                parameters->presharedKeyLength,
                                options);
}

void emberSwitchToNextNetworkKey(void)
{
  emSendBinaryManagementCommand(EMBER_SWITCH_TO_NEXT_NETWORK_KEY_COMMAND_IDENTIFIER,
                                "");
}

void emberStartScan(EmberNetworkScanType scanType,
                    uint32_t channelMask,
                    uint8_t duration)
{
  emSendBinaryManagementCommand(EMBER_START_SCAN_COMMAND_IDENTIFIER,
                                "uwu",
                                scanType,
                                channelMask,
                                duration);
}

void emberStopScan(void)
{
  emSendBinaryManagementCommand(EMBER_STOP_SCAN_COMMAND_IDENTIFIER,
                                "");
}

void emberGetRipEntry(uint8_t index)
{
  emSendBinaryManagementCommand(EMBER_GET_RIP_ENTRY_COMMAND_IDENTIFIER,
                                "u",
                                index);
}

void emberGetMulticastTable(void)
{
  emSendBinaryManagementCommand(EMBER_GET_MULTICAST_TABLE_COMMAND_IDENTIFIER,
                                "");
}

void emberGetCounter(EmberCounterType type)
{
  emSendBinaryManagementCommand(EMBER_GET_COUNTER_COMMAND_IDENTIFIER,
                                "u",
                                type);
}

void emberClearCounters(void)
{
  emSendBinaryManagementCommand(EMBER_CLEAR_COUNTERS_COMMAND_IDENTIFIER,
                                "");
}

EmberStatus emberSetTxPowerMode(uint16_t txPowerMode)
{
  emSendBinaryManagementCommand(EMBER_SET_TX_POWER_MODE_COMMAND_IDENTIFIER,
                                "v",
                                txPowerMode);
  return EMBER_SUCCESS;
}

void emberGetTxPowerMode(void)
{
  emSendBinaryManagementCommand(EMBER_GET_TX_POWER_MODE_COMMAND_IDENTIFIER,
                                "");
}

void emberFfWakeup(void)
{
  emSendBinaryManagementCommand(EMBER_FF_WAKEUP_COMMAND_IDENTIFIER,
                                "");
}

void emberGetCcaThreshold(void)
{
  emSendBinaryManagementCommand(EMBER_GET_CCA_THRESHOLD_COMMAND_IDENTIFIER,
                                "");
}

void emberSetCcaThreshold(int8_t threshold)
{
  emSendBinaryManagementCommand(EMBER_SET_CCA_THRESHOLD_COMMAND_IDENTIFIER,
                                "s",
                                threshold);
}

void emberGetRadioPower(void)
{
  emSendBinaryManagementCommand(EMBER_GET_RADIO_POWER_COMMAND_IDENTIFIER,
                                "");
}

void emberSetRadioPower(int8_t power)
{
  emSendBinaryManagementCommand(EMBER_SET_RADIO_POWER_COMMAND_IDENTIFIER,
                                "s",
                                power);
}

void emberEcho(const uint8_t *data,
               uint8_t length)
{
  emSendBinaryManagementCommand(EMBER_ECHO_COMMAND_IDENTIFIER,
                                "b",
                                data, length);
}

void emberConfigureGateway(uint8_t borderRouterFlags,
                           bool isStable,
                           const uint8_t *prefix,
                           uint8_t prefixLength,
                           uint8_t domainId,
                           uint32_t preferredLifetime,
                           uint32_t validLifetime)
{
  emSendBinaryManagementCommand(EMBER_CONFIGURE_GATEWAY_COMMAND_IDENTIFIER,
                                "uubuww",
                                borderRouterFlags,
                                isStable,
                                prefix, prefixLength,
                                domainId,
                                preferredLifetime,
                                validLifetime);
}

void emberGetIndexedToken(EmberTokenId tokenId,
                          uint8_t index)
{
  emSendBinaryManagementCommand(EMBER_GET_INDEXED_TOKEN_COMMAND_IDENTIFIER,
                                "uu",
                                tokenId,
                                index);
}

void emberPollForData(void)
{
  emSendBinaryManagementCommand(EMBER_POLL_FOR_DATA_COMMAND_IDENTIFIER,
                                "");
}

void emberDeepSleep(bool sleep)
{
  emSendBinaryManagementCommand(EMBER_DEEP_SLEEP_COMMAND_IDENTIFIER,
                                "u",
                                sleep);
}

void emberStackPollForData(uint32_t pollMs)
{
  emSendBinaryManagementCommand(EMBER_STACK_POLL_FOR_DATA_COMMAND_IDENTIFIER,
                                "w",
                                pollMs);
}

bool emberOkToNap(void)
{
  emSendBinaryManagementCommand(EMBER_OK_TO_NAP_COMMAND_IDENTIFIER,
                                "");
  return false;
}

bool emberPing(const uint8_t *destination,
               uint16_t id,
               uint16_t sequence,
               uint16_t length,
               uint8_t hopLimit)
{
  emSendBinaryManagementCommand(EMBER_PING_COMMAND_IDENTIFIER,
                                "bvvvu",
                                destination, 16,
                                id,
                                sequence,
                                length,
                                hopLimit);
  return true;
}

void emberJoinCommissioned(int8_t radioTxPower,
                           EmberNodeType nodeType,
                           bool requireConnectivity)
{
  emSendBinaryManagementCommand(EMBER_JOIN_COMMISSIONED_COMMAND_IDENTIFIER,
                                "suu",
                                radioTxPower,
                                nodeType,
                                requireConnectivity);
}

void emberCommissionNetwork(uint8_t preferredChannel,
                            uint32_t fallbackChannelMask,
                            const uint8_t *networkId,
                            uint8_t networkIdLength,
                            uint16_t panId,
                            const uint8_t *ulaPrefix,
                            const uint8_t *extendedPanId,
                            const EmberKeyData *key,
                            uint32_t keySequence)
{
  emSendBinaryManagementCommand(EMBER_COMMISSION_NETWORK_COMMAND_IDENTIFIER,
                                "uwbvbbbw",
                                preferredChannel,
                                fallbackChannelMask,
                                networkId, networkIdLength,
                                panId,
                                ulaPrefix, 8,
                                extendedPanId, EXTENDED_PAN_ID_SIZE,
                                key, sizeof(EmberKeyData),
                                keySequence);
}

void emberRequestDhcpAddress(const uint8_t *prefix,
                             uint8_t prefixLength)
{
  emSendBinaryManagementCommand(EMBER_REQUEST_DHCP_ADDRESS_COMMAND_IDENTIFIER,
                                "b",
                                prefix, prefixLength);
}

void emberHostToNcpNoOp(const uint8_t *bytes,
                        uint8_t bytesLength)
{
  emSendBinaryManagementCommand(EMBER_HOST_TO_NCP_NO_OP_COMMAND_IDENTIFIER,
                                "b",
                                bytes, bytesLength);
}

void emberGetGlobalAddresses(const uint8_t *prefix,
                             uint8_t prefixLength)
{
  emSendBinaryManagementCommand(EMBER_GET_GLOBAL_ADDRESSES_COMMAND_IDENTIFIER,
                                "b",
                                prefix, prefixLength);
}

void emberGetDhcpClients(void)
{
  emSendBinaryManagementCommand(EMBER_GET_DHCP_CLIENTS_COMMAND_IDENTIFIER,
                                "");
}

void emberSetCommissioningMacKey(bool isValid,
                                 const uint8_t *key)
{
  emSendBinaryManagementCommand(EMBER_SET_COMMISSIONING_MAC_KEY_COMMAND_IDENTIFIER,
                                "ub",
                                isValid,
                                key, 16);
}

void emberAddSteeringEui64(const EmberEui64 *eui64)
{
  emSendBinaryManagementCommand(EMBER_ADD_STEERING_EUI64_COMMAND_IDENTIFIER,
                                "b",
                                eui64->bytes,
                                EUI64_SIZE);
}

void emberBecomeCommissioner(const uint8_t *deviceName,
                             uint8_t deviceNameLength)
{
  emSendBinaryManagementCommand(EMBER_BECOME_COMMISSIONER_COMMAND_IDENTIFIER,
                                "b",
                                deviceName, deviceNameLength);
}

void emberGetCommissioner(void)
{
  emSendBinaryManagementCommand(EMBER_GET_COMMISSIONER_COMMAND_IDENTIFIER,
                                "");
}

void emberSendSteeringData(void)
{
  emSendBinaryManagementCommand(EMBER_SEND_STEERING_DATA_COMMAND_IDENTIFIER,
                                "");
}

void emberStopCommissioning(void)
{
  emSendBinaryManagementCommand(EMBER_STOP_COMMISSIONING_COMMAND_IDENTIFIER,
                                "");
}

void emberSetJoinKey(const EmberEui64 *eui64,
                     const uint8_t *key,
                     uint8_t keyLength)
{
  emSendBinaryManagementCommand(EMBER_SET_JOIN_KEY_COMMAND_IDENTIFIER,
                                "bb",
                                eui64->bytes,
                                EUI64_SIZE,
                                key,
                                keyLength);
}

void emberSetJoiningMode(EmberJoiningMode mode,
                         uint8_t length)
{
  emSendBinaryManagementCommand(EMBER_SET_JOINING_MODE_COMMAND_IDENTIFIER,
                                "uu",
                                mode,
                                length);
}

void emberChangeNodeType(EmberNodeType newType)
{
  emSendBinaryManagementCommand(EMBER_CHANGE_NODE_TYPE_COMMAND_IDENTIFIER,
                                "u",
                                newType);
}

void emberGetGlobalPrefixes(void)
{
  emSendBinaryManagementCommand(EMBER_GET_GLOBAL_PREFIXES_COMMAND_IDENTIFIER,
                                "");
}

void emberResignGlobalAddress(const EmberIpv6Address *address)
{
  bool preHookResult = tmspHostResignGlobalAddressPreHook(address);
  if (preHookResult) {
    emSendBinaryManagementCommand(EMBER_RESIGN_GLOBAL_ADDRESS_COMMAND_IDENTIFIER,
                                  "b",
                                  address, sizeof(EmberIpv6Address));
  }
}

void emberSetEui64(const EmberEui64 *eui64)
{
  emSendBinaryManagementCommand(EMBER_SET_EUI64_COMMAND_IDENTIFIER,
                                "b",
                                eui64->bytes,
                                EUI64_SIZE);
  tmspHostSetEui64PostHook(eui64);
}

void emberRequestSlaacAddress(const uint8_t *prefix,
                              uint8_t prefixLength)
{
  emSendBinaryManagementCommand(EMBER_REQUEST_SLAAC_ADDRESS_COMMAND_IDENTIFIER,
                                "b",
                                prefix, prefixLength);
}

void emberCustomHostToNcpMessage(const uint8_t *message,
                                 uint8_t messageLength)
{
  emSendBinaryManagementCommand(EMBER_CUSTOM_HOST_TO_NCP_MESSAGE_COMMAND_IDENTIFIER,
                                "b",
                                message, messageLength);
}

void emberGetNetworkDataTlv(uint8_t type,
                            uint8_t index)
{
  emSendBinaryManagementCommand(EMBER_GET_NETWORK_DATA_TLV_COMMAND_IDENTIFIER,
                                "uu",
                                type,
                                index);
}

void emberGetRoutingLocator(void)
{
  emSendBinaryManagementCommand(EMBER_GET_ROUTING_LOCATOR_COMMAND_IDENTIFIER,
                                "");
}

void emberSetRandomizeMacExtendedId(bool value)
{
  emSendBinaryManagementCommand(EMBER_SET_RANDOMIZE_MAC_EXTENDED_ID_COMMAND_IDENTIFIER,
                                "u",
                                value);
}

void emberConfigureExternalRoute(uint8_t externalRouteFlags,
                                 const uint8_t *prefix,
                                 uint8_t prefixLength,
                                 uint8_t externalRouteDomainId)
{
  emSendBinaryManagementCommand(EMBER_CONFIGURE_EXTERNAL_ROUTE_COMMAND_IDENTIFIER,
                                "ubu",
                                externalRouteFlags,
                                prefix, prefixLength,
                                externalRouteDomainId);
}

void emberAllowNativeCommissioner(bool on)
{
  emSendBinaryManagementCommand(EMBER_ALLOW_NATIVE_COMMISSIONER_COMMAND_IDENTIFIER,
                                "u",
                                on);
}

void emberSetCommissionerKey(const uint8_t *commissionerKey,
                             uint8_t commissionerKeyLength)
{
  emSendBinaryManagementCommand(EMBER_SET_COMMISSIONER_KEY_COMMAND_IDENTIFIER,
                                "b",
                                commissionerKey, commissionerKeyLength);
}

void emberGetStandaloneBootloaderInfo(void)
{
  emSendBinaryManagementCommand(EMBER_GET_STANDALONE_BOOTLOADER_INFO_COMMAND_IDENTIFIER,
                                "");
}

void emberLaunchStandaloneBootloader(uint8_t mode)
{
  emSendBinaryManagementCommand(EMBER_LAUNCH_STANDALONE_BOOTLOADER_COMMAND_IDENTIFIER,
                                "u",
                                mode);
}

void emberGetMfgToken(EmberMfgTokenId tokenId)
{
  emSendBinaryManagementCommand(EMBER_GET_MFG_TOKEN_COMMAND_IDENTIFIER,
                                "u",
                                tokenId);
}

void emberSetMfgToken(EmberMfgTokenId tokenId,
                      const uint8_t *tokenData,
                      uint8_t tokenDataLength)
{
  emSendBinaryManagementCommand(EMBER_SET_MFG_TOKEN_COMMAND_IDENTIFIER,
                                "ub",
                                tokenId,
                                tokenData, tokenDataLength);
}

static void resetMicroHandlerCallback(void)
{
  EmberResetCause cause;
  cause = (EmberResetCause) emberUnsignedCommandArgument(0);
  bool preHookResult = tmspHostResetMicroHandlerPreHook();
  if (preHookResult) {
    emberResetMicroHandler(cause);
  }
}

static void stateReturnCallback(void)
{
  EmberNetworkParameters parameters;
  EmberEui64 localEui64;
  EmberEui64 macExtendedId;
  EmberNetworkStatus networkStatus;
  MEMSET(&parameters, 0, sizeof(EmberNetworkParameters));
  MEMSET(&localEui64, 0, sizeof(EmberEui64));
  MEMSET(&macExtendedId, 0, sizeof(EmberEui64));
  emberGetStringArgument(0, parameters.networkId, sizeof(parameters.networkId), false);
  emberGetStringArgument(1, parameters.ulaPrefix.bytes, sizeof(parameters.ulaPrefix.bytes), false);
  emberGetStringArgument(2, parameters.extendedPanId, sizeof(parameters.extendedPanId), false);
  parameters.panId = emberUnsignedCommandArgument(3);
  parameters.channel = emberUnsignedCommandArgument(4);
  parameters.nodeType = emberUnsignedCommandArgument(5);
  parameters.radioTxPower = emberSignedCommandArgument(6);
  networkStatus = emberUnsignedCommandArgument(7);
  emberGetStringArgument(8, localEui64.bytes, sizeof(localEui64.bytes), false);
  emberGetStringArgument(9, macExtendedId.bytes, sizeof(macExtendedId.bytes), false);
  emberGetStringArgument(10, parameters.legacyUla.bytes, sizeof(parameters.legacyUla.bytes), false);
  bool preHookResult = tmspStateReturnPreHook(&parameters,
                                              &localEui64,
                                              &macExtendedId,
                                              networkStatus);
  if (preHookResult) {
    emberStateReturn(&parameters,
                     &localEui64,
                     &macExtendedId,
                     networkStatus);
  }
}

static void getVersionsReturnCallback(void)
{
  uint8_t *versionName;
  uint16_t managementVersionNumber;
  uint16_t stackVersionNumber;
  uint16_t stackBuildNumber;
  EmberVersionType versionType;
  uint8_t *buildTimestamp;
  versionName = emberStringCommandArgument(0, NULL);
  managementVersionNumber = (uint16_t) emberUnsignedCommandArgument(1);
  stackVersionNumber = (uint16_t) emberUnsignedCommandArgument(2);
  stackBuildNumber = (uint16_t) emberUnsignedCommandArgument(3);
  versionType = (EmberVersionType) emberUnsignedCommandArgument(4);
  buildTimestamp = emberStringCommandArgument(5, NULL);
  emberGetVersionsReturn(versionName,
                         managementVersionNumber,
                         stackVersionNumber,
                         stackBuildNumber,
                         versionType,
                         buildTimestamp);
}

static void getRipEntryReturnCallback(void)
{
  uint8_t index;
  EmberRipEntry entry;
  MEMSET(&entry, 0, sizeof(EmberRipEntry));
  index = emberUnsignedCommandArgument(0);
  emberGetStringArgument(1, entry.longId, 8, false);
  entry.type = emberUnsignedCommandArgument(2);
  entry.rollingRssi = emberSignedCommandArgument(3);
  entry.nextHopIndex = emberUnsignedCommandArgument(4);
  entry.ripMetric = emberUnsignedCommandArgument(5);
  entry.incomingLinkQuality = emberUnsignedCommandArgument(6);
  entry.outgoingLinkQuality = emberUnsignedCommandArgument(7);
  entry.mleSync = emberUnsignedCommandArgument(8);
  entry.age = emberUnsignedCommandArgument(9);
  emberGetRipEntryReturn(index,
                         &entry);
}

static void initReturnCallback(void)
{
  EmberStatus status;
  status = (EmberStatus) emberUnsignedCommandArgument(0);
  emberInitReturn(status);
}

static void getCounterReturnCallback(void)
{
  EmberCounterType type;
  uint16_t value;
  type = (EmberCounterType) emberUnsignedCommandArgument(0);
  value = (uint16_t) emberUnsignedCommandArgument(1);
  emberGetCounterReturn(type,
                        value);
}

static void setSecurityParametersReturnCallback(void)
{
  EmberStatus status;
  status = (EmberStatus) emberUnsignedCommandArgument(0);
  emberSetSecurityParametersReturn(status);
}

static void switchToNextNetworkKeyReturnCallback(void)
{
  EmberStatus status;
  status = (EmberStatus) emberUnsignedCommandArgument(0);
  emberSwitchToNextNetworkKeyReturn(status);
}

static void formNetworkReturnCallback(void)
{
  EmberStatus status;
  status = (EmberStatus) emberUnsignedCommandArgument(0);
  emberFormNetworkReturn(status);
}

static void joinNetworkReturnCallback(void)
{
  EmberStatus status;
  status = (EmberStatus) emberUnsignedCommandArgument(0);
  emberJoinNetworkReturn(status);
}

static void resumeNetworkReturnCallback(void)
{
  EmberStatus status;
  status = (EmberStatus) emberUnsignedCommandArgument(0);
  emberResumeNetworkReturn(status);
}

static void attachToNetworkReturnCallback(void)
{
  EmberStatus status;
  status = (EmberStatus) emberUnsignedCommandArgument(0);
  emberAttachToNetworkReturn(status);
}

static void energyScanHandlerCallback(void)
{
  uint8_t channel;
  int8_t maxRssiValue;
  channel = (uint8_t) emberUnsignedCommandArgument(0);
  maxRssiValue = (int8_t) emberSignedCommandArgument(1);
  emberEnergyScanHandler(channel,
                         maxRssiValue);
}

static void activeScanHandlerCallback(void)
{
  EmberMacBeaconData beaconData;
  MEMSET(&beaconData, 0, sizeof(EmberMacBeaconData));
  beaconData.allowingJoin = emberUnsignedCommandArgument(0);
  beaconData.channel = emberUnsignedCommandArgument(1);
  beaconData.lqi = emberUnsignedCommandArgument(2);
  beaconData.rssi = emberUnsignedCommandArgument(3);
  emberGetStringArgument(4, beaconData.networkId, sizeof(beaconData.networkId), false);
  beaconData.panId = emberUnsignedCommandArgument(5);
  emberGetStringArgument(6, beaconData.extendedPanId, sizeof(beaconData.extendedPanId), false);
  emberGetStringArgument(7, beaconData.longId, sizeof(beaconData.longId), false);
  beaconData.protocolId = emberUnsignedCommandArgument(8);
  beaconData.version = emberUnsignedCommandArgument(9);
  emberGetStringArgument(10, beaconData.steeringData, sizeof(beaconData.steeringData), false);
  beaconData.steeringDataLength = emberUnsignedCommandArgument(11);
  emberActiveScanHandler(&beaconData);
}

static void scanReturnCallback(void)
{
  EmberStatus status;
  status = (EmberStatus) emberUnsignedCommandArgument(0);
  emberScanReturn(status);
}

static void setAddressHandlerCallback(void)
{
  uint8_t address[16];
  emberGetStringArgument(0, address, 16, false);
  emberSetAddressHandler(address);
}

static void startHostJoinClientHandlerCallback(void)
{
  uint8_t parentAddress[16];
  emberGetStringArgument(0, parentAddress, 16, false);
  emberStartHostJoinClientHandler(parentAddress);
}

static void setTxPowerModeReturnCallback(void)
{
  EmberStatus status;
  status = (EmberStatus) emberUnsignedCommandArgument(0);
  emberSetTxPowerModeReturn(status);
}

static void getTxPowerModeReturnCallback(void)
{
  uint16_t txPowerMode;
  txPowerMode = (uint16_t) emberUnsignedCommandArgument(0);
  emberGetTxPowerModeReturn(txPowerMode);
}

static void getMulticastEntryReturnCallback(void)
{
  uint8_t lastSequence;
  uint8_t windowBitmask;
  uint8_t dwellQs;
  uint8_t seed[8];
  lastSequence = (uint8_t) emberUnsignedCommandArgument(0);
  windowBitmask = (uint8_t) emberUnsignedCommandArgument(1);
  dwellQs = (uint8_t) emberUnsignedCommandArgument(2);
  emberGetStringArgument(3, seed, 8, false);
  emberGetMulticastEntryReturn(lastSequence,
                               windowBitmask,
                               dwellQs,
                               seed);
}

static void getCcaThresholdReturnCallback(void)
{
  int8_t threshold;
  threshold = (int8_t) emberSignedCommandArgument(0);
  emberGetCcaThresholdReturn(threshold);
}

static void setCcaThresholdReturnCallback(void)
{
  EmberStatus status;
  status = (EmberStatus) emberUnsignedCommandArgument(0);
  emberSetCcaThresholdReturn(status);
}

static void getRadioPowerReturnCallback(void)
{
  int8_t power;
  power = (int8_t) emberSignedCommandArgument(0);
  emberGetRadioPowerReturn(power);
}

static void setRadioPowerReturnCallback(void)
{
  EmberStatus status;
  status = (EmberStatus) emberUnsignedCommandArgument(0);
  emberSetRadioPowerReturn(status);
}

static void echoReturnCallback(void)
{
  uint8_t *data;
  uint8_t length;
  data = emberStringCommandArgument(0, &length);
  emberEchoReturn(data,
                  length);
}

static void assertInfoReturnCallback(void)
{
  uint8_t *fileName;
  uint32_t line;
  fileName = emberStringCommandArgument(0, NULL);
  line = (uint32_t) emberUnsignedCommandArgument(1);
  emberAssertInfoReturn(fileName,
                        line);
}

static void setCommAppSecurityHandlerCallback(void)
{
  const uint8_t *masterKey;
  uint8_t masterKeyLen;
  uint32_t sequenceNumber;

  masterKey = emberStringCommandArgument(0, &masterKeyLen);
  sequenceNumber = emberUnsignedCommandArgument(1);

  if (masterKeyLen == EMBER_ENCRYPTION_KEY_SIZE) {
    emberSetCommAppSecurityHandler(masterKey, sequenceNumber);
  }
}

static void configureGatewayReturnCallback(void)
{
  EmberStatus status;
  status = (EmberStatus) emberUnsignedCommandArgument(0);
  emberConfigureGatewayReturn(status);
}

static void getChannelCalDataTokenReturnCallback(void)
{
  uint8_t lna;
  int8_t tempAtLna;
  uint8_t modDac;
  int8_t tempAtModDac;
  lna = (uint8_t) emberUnsignedCommandArgument(0);
  tempAtLna = (int8_t) emberSignedCommandArgument(1);
  modDac = (uint8_t) emberUnsignedCommandArgument(2);
  tempAtModDac = (int8_t) emberSignedCommandArgument(3);
  emberGetChannelCalDataTokenReturn(lna,
                                    tempAtLna,
                                    modDac,
                                    tempAtModDac);
}

static void pollForDataReturnCallback(void)
{
  EmberStatus status;
  bool dataPending;
  status = (EmberStatus) emberUnsignedCommandArgument(0);
  dataPending = (bool) emberUnsignedCommandArgument(1);
  emberPollForDataReturn(status,
                         dataPending);
}

static void deepSleepReturnCallback(void)
{
  EmberStatus status;
  status = (EmberStatus) emberUnsignedCommandArgument(0);
  emberDeepSleepReturn(status);
}

static void stackPollForDataReturnCallback(void)
{
  EmberStatus status;
  status = (EmberStatus) emberUnsignedCommandArgument(0);
  emberStackPollForDataReturn(status);
}

static void okToNapReturnCallback(void)
{
  uint8_t stateMask;
  stateMask = (uint8_t) emberUnsignedCommandArgument(0);
  emberOkToNapReturn(stateMask);
}

static void deepSleepCallbackCallback(void)
{
  uint16_t sleepDuration;
  sleepDuration = (uint16_t) emberUnsignedCommandArgument(0);
  emberDeepSleepCallback(sleepDuration);
}

static void resetNetworkStateReturnCallback(void)
{
  EmberStatus status;
  status = (EmberStatus) emberUnsignedCommandArgument(0);
  emberResetNetworkStateReturn(status);
}

static void externalRouteChangeCallback(void)
{
  uint8_t *prefix;
  uint8_t prefixLength;
  bool available;
  prefix = emberStringCommandArgument(0, &prefixLength);
  available = (bool) emberUnsignedCommandArgument(1);
  emberExternalRouteChange(prefix,
                           prefixLength,
                           available);
}

static void dhcpServerChangeCallback(void)
{
  uint8_t *prefix;
  uint8_t prefixLength;
  bool available;
  prefix = emberStringCommandArgument(0, &prefixLength);
  available = (bool) emberUnsignedCommandArgument(1);
  emberDhcpServerChange(prefix,
                        prefixLength,
                        available);
}

static void addressConfigurationChangeCallback(void)
{
  EmberIpv6Address address;
  uint32_t preferredLifetime;
  uint32_t validLifetime;
  uint8_t addressFlags;
  MEMSET(&address, 0, sizeof(EmberIpv6Address));
  emberIpv6AddressCommandArgument(0, &address);
  preferredLifetime = (uint32_t) emberUnsignedCommandArgument(1);
  validLifetime = (uint32_t) emberUnsignedCommandArgument(2);
  addressFlags = (uint8_t) emberUnsignedCommandArgument(3);
  bool preHookResult = tmspHostAddressConfigurationChangePreHook(&address,
                                                                 preferredLifetime,
                                                                 validLifetime,
                                                                 addressFlags);
  if (preHookResult) {
    emberAddressConfigurationChange(&address,
                                    preferredLifetime,
                                    validLifetime,
                                    addressFlags);
  }
}

static void requestDhcpAddressReturnCallback(void)
{
  EmberStatus status;
  uint8_t *prefix;
  uint8_t prefixLength;
  status = (EmberStatus) emberUnsignedCommandArgument(0);
  prefix = emberStringCommandArgument(1, &prefixLength);
  emberRequestDhcpAddressReturn(status,
                                prefix,
                                prefixLength);
}

static void commissionNetworkReturnCallback(void)
{
  EmberStatus status;
  status = (EmberStatus) emberUnsignedCommandArgument(0);
  emberCommissionNetworkReturn(status);
}

static void getGlobalAddressReturnCallback(void)
{
  EmberIpv6Address address;
  uint32_t preferredLifetime;
  uint32_t validLifetime;
  uint8_t addressFlags;
  MEMSET(&address, 0, sizeof(EmberIpv6Address));
  emberGetStringArgument(0, address.bytes, EMBER_NETWORK_ID_SIZE, false);
  preferredLifetime = emberUnsignedCommandArgument(1);
  validLifetime = emberUnsignedCommandArgument(2);
  addressFlags = emberUnsignedCommandArgument(3);
  emberGetGlobalAddressReturn(&address,
                              preferredLifetime,
                              validLifetime,
                              addressFlags);
}

static void getDhcpClientReturnCallback(void)
{
  EmberIpv6Address address;
  MEMSET(&address, 0, sizeof(EmberIpv6Address));
  emberGetStringArgument(0, address.bytes, sizeof(address.bytes), false);
  emberGetDhcpClientReturn(&address);
}

static void becomeCommissionerReturnCallback(void)
{
  EmberStatus status;
  status = (EmberStatus) emberUnsignedCommandArgument(0);
  emberBecomeCommissionerReturn(status);
}

static void sendSteeringDataReturnCallback(void)
{
  EmberStatus status;
  status = (EmberStatus) emberUnsignedCommandArgument(0);
  emberSendSteeringDataReturn(status);
}

static void changeNodeTypeReturnCallback(void)
{
  EmberStatus status;
  status = (EmberStatus) emberUnsignedCommandArgument(0);
  emberChangeNodeTypeReturn(status);
}

static void getGlobalPrefixReturnCallback(void)
{
  uint8_t borderRouterFlags;
  bool isStable;
  uint8_t prefix[8];
  uint8_t domainId;
  uint32_t preferredLifetime;
  uint32_t validLifetime;
  borderRouterFlags = (uint8_t) emberUnsignedCommandArgument(0);
  isStable = (bool) emberUnsignedCommandArgument(1);
  emberGetStringArgument(2, prefix, 8, false);
  domainId = (uint8_t) emberUnsignedCommandArgument(3);
  preferredLifetime = (uint32_t) emberUnsignedCommandArgument(4);
  validLifetime = (uint32_t) emberUnsignedCommandArgument(5);
  emberGetGlobalPrefixReturn(borderRouterFlags,
                             isStable,
                             prefix,
                             domainId,
                             preferredLifetime,
                             validLifetime);
}

static void resignGlobalAddressReturnCallback(void)
{
  EmberStatus status;
  status = (EmberStatus) emberUnsignedCommandArgument(0);
  emberResignGlobalAddressReturn(status);
}

static void slaacServerChangeCallback(void)
{
  uint8_t *prefix;
  uint8_t prefixLength;
  bool available;
  prefix = emberStringCommandArgument(0, &prefixLength);
  available = (bool) emberUnsignedCommandArgument(1);
  emberSlaacServerChange(prefix,
                         prefixLength,
                         available);
}

static void requestSlaacAddressReturnCallback(void)
{
  EmberStatus status;
  uint8_t *prefix;
  uint8_t prefixLength;
  status = (EmberStatus) emberUnsignedCommandArgument(0);
  prefix = emberStringCommandArgument(1, &prefixLength);
  emberRequestSlaacAddressReturn(status,
                                 prefix,
                                 prefixLength);
}

static void customNcpToHostMessageCallback(void)
{
  uint8_t *message;
  uint8_t messageLength;
  message = emberStringCommandArgument(0, &messageLength);
  emberCustomNcpToHostMessageHandler(message,
                                     messageLength);
}

static void ncpToHostNoOpCallback(void)
{
  uint8_t *bytes;
  uint8_t bytesLength;
  bytes = emberStringCommandArgument(0, &bytesLength);
  emberNcpToHostNoOp(bytes,
                     bytesLength);
}

static void commissionerStatusHandlerCallback(void)
{
  uint16_t flags;
  uint8_t *commissionerName;
  uint8_t commissionerNameLength;
  flags = (uint16_t) emberUnsignedCommandArgument(0);
  commissionerName = emberStringCommandArgument(1, &commissionerNameLength);
  emberCommissionerStatusHandler(flags,
                                 commissionerName,
                                 commissionerNameLength);
}

static void leaderDataHandlerCallback(void)
{
  uint8_t leaderData[EMBER_NETWORK_DATA_LEADER_SIZE];
  emberGetStringArgument(0, leaderData, EMBER_NETWORK_DATA_LEADER_SIZE, false);
  emberLeaderDataHandler(leaderData);
}

static void getNetworkDataTlvReturnCallback(void)
{
  uint8_t type;
  uint8_t index;
  uint8_t versionNumber;
  uint8_t *tlv;
  uint8_t tlvLength;
  type = (uint8_t) emberUnsignedCommandArgument(0);
  index = (uint8_t) emberUnsignedCommandArgument(1);
  versionNumber = (uint8_t) emberUnsignedCommandArgument(2);
  tlv = emberStringCommandArgument(3, &tlvLength);
  emberGetNetworkDataTlvReturn(type,
                               index,
                               versionNumber,
                               tlv,
                               tlvLength);
}

static void getRoutingLocatorReturnCallback(void)
{
  EmberIpv6Address rloc;
  MEMSET(&rloc, 0, sizeof(EmberIpv6Address));
  emberIpv6AddressCommandArgument(0, &rloc);
  emberGetRoutingLocatorReturn(&rloc);
}

static void setRandomizeMacExtendedIdReturnCallback(void)
{
  emberSetRandomizeMacExtendedIdReturn();
}

static void configureExternalRouteReturnCallback(void)
{
  EmberStatus status;
  status = (EmberStatus) emberUnsignedCommandArgument(0);
  emberConfigureExternalRouteReturn(status);
}

static void allowNativeCommissionerReturnCallback(void)
{
  EmberStatus status;
  status = (EmberStatus) emberUnsignedCommandArgument(0);
  emberAllowNativeCommissionerReturn(status);
}

static void setCommissionerKeyReturnCallback(void)
{
  EmberStatus status;
  status = (EmberStatus) emberUnsignedCommandArgument(0);
  emberSetCommissionerKeyReturn(status);
}

static void networkStatusHandlerCallback(void)
{
  EmberNetworkStatus newNetworkStatus;
  EmberNetworkStatus oldNetworkStatus;
  EmberJoinFailureReason reason;
  newNetworkStatus = (EmberNetworkStatus) emberUnsignedCommandArgument(0);
  oldNetworkStatus = (EmberNetworkStatus) emberUnsignedCommandArgument(1);
  reason = (EmberJoinFailureReason) emberUnsignedCommandArgument(2);
  emberNetworkStatusHandler(newNetworkStatus,
                            oldNetworkStatus,
                            reason);
}

static void getStandaloneBootloaderInfoReturnCallback(void)
{
  uint16_t version;
  uint8_t platformId;
  uint8_t microId;
  uint8_t phyId;
  version = (uint16_t) emberUnsignedCommandArgument(0);
  platformId = (uint8_t) emberUnsignedCommandArgument(1);
  microId = (uint8_t) emberUnsignedCommandArgument(2);
  phyId = (uint8_t) emberUnsignedCommandArgument(3);
  emberGetStandaloneBootloaderInfoReturn(version,
                                         platformId,
                                         microId,
                                         phyId);
}

static void launchStandaloneBootloaderReturnCallback(void)
{
  EmberStatus status;
  status = (EmberStatus) emberUnsignedCommandArgument(0);
  emberLaunchStandaloneBootloaderReturn(status);
}

static void getMfgTokenReturnCallback(void)
{
  EmberMfgTokenId tokenId;
  EmberStatus status;
  uint8_t *tokenData;
  uint8_t tokenDataLength;
  tokenId = (EmberMfgTokenId) emberUnsignedCommandArgument(0);
  status = (EmberStatus) emberUnsignedCommandArgument(1);
  tokenData = emberStringCommandArgument(2, &tokenDataLength);
  emberGetMfgTokenReturn(tokenId,
                         status,
                         tokenData,
                         tokenDataLength);
}

static void setMfgTokenReturnCallback(void)
{
  EmberMfgTokenId tokenId;
  EmberStatus status;
  tokenId = (EmberMfgTokenId) emberUnsignedCommandArgument(0);
  status = (EmberStatus) emberUnsignedCommandArgument(1);
  emberSetMfgTokenReturn(tokenId,
                         status);
}

static void hostStateHandlerCallback(void)
{
  EmberNetworkParameters parameters;
  EmberEui64 localEui64;
  EmberEui64 macExtendedId;
  EmberNetworkStatus networkStatus;
  MEMSET(&parameters, 0, sizeof(EmberNetworkParameters));
  MEMSET(&localEui64, 0, sizeof(EmberEui64));
  MEMSET(&macExtendedId, 0, sizeof(EmberEui64));
  emberGetStringArgument(0, parameters.networkId, sizeof(parameters.networkId), false);
  emberGetStringArgument(1, parameters.ulaPrefix.bytes, sizeof(parameters.ulaPrefix.bytes), false);
  emberGetStringArgument(2, parameters.extendedPanId, sizeof(parameters.extendedPanId), false);
  parameters.panId = emberUnsignedCommandArgument(3);
  parameters.channel = emberUnsignedCommandArgument(4);
  parameters.nodeType = emberUnsignedCommandArgument(5);
  parameters.radioTxPower = emberSignedCommandArgument(6);
  networkStatus = emberUnsignedCommandArgument(7);
  emberGetStringArgument(8, localEui64.bytes, sizeof(localEui64.bytes), false);
  emberGetStringArgument(9, macExtendedId.bytes, sizeof(macExtendedId.bytes), false);
  emberGetStringArgument(10, parameters.legacyUla.bytes, sizeof(parameters.legacyUla.bytes), false);
  emberHostStateHandler(&parameters,
                        &localEui64,
                        &macExtendedId,
                        networkStatus);
}

//------------------------------------------------------------------------------
// MFGLIB
//------------------------------------------------------------------------------
#ifdef MFGLIB

void tmspHostMfglibStart(bool requestRxCallback)
{
  emSendBinaryManagementCommand(EMBER_MFGLIB_START_COMMAND_IDENTIFIER,
                                "u",
                                requestRxCallback);
}

void tmspHostMfglibEnd(void)
{
  emSendBinaryManagementCommand(EMBER_MFGLIB_END_COMMAND_IDENTIFIER,
                                "");
}

void tmspHostMfglibStartActivity(MfglibActivities type)
{
  emSendBinaryManagementCommand(EMBER_MFGLIB_START_ACTIVITY_COMMAND_IDENTIFIER,
                                "u",
                                type);
}

void tmspHostMfglibStopActivity(MfglibActivities type)
{
  emSendBinaryManagementCommand(EMBER_MFGLIB_STOP_ACTIVITY_COMMAND_IDENTIFIER,
                                "u",
                                type);
}

void tmspHostMfglibSendPacket(const uint8_t *packet,
                              uint8_t packetLength,
                              uint16_t repeat)
{
  emSendBinaryManagementCommand(EMBER_MFGLIB_SEND_PACKET_COMMAND_IDENTIFIER,
                                "bv",
                                packet, packetLength,
                                repeat);
}

void tmspHostMfglibSet(MfglibValues type,
                       uint16_t arg1,
                       int8_t arg2)
{
  emSendBinaryManagementCommand(EMBER_MFGLIB_SET_COMMAND_IDENTIFIER,
                                "uvs",
                                type,
                                arg1,
                                arg2);
}

void tmspHostMfglibGet(MfglibValues type)
{
  emSendBinaryManagementCommand(EMBER_MFGLIB_GET_COMMAND_IDENTIFIER,
                                "u",
                                type);
}

void tmspHostMfglibTestContModCal(uint8_t channel,
                                  uint32_t duration)
{
  emSendBinaryManagementCommand(EMBER_MFGLIB_TEST_CONT_MOD_CAL_COMMAND_IDENTIFIER,
                                "uw",
                                channel,
                                duration);
}

static void mfglibStartTestReturnCallback(void)
{
  EmberStatus status;
  status = (EmberStatus) emberUnsignedCommandArgument(0);
  tmspHostMfglibStartTestReturn(status);
}

static void mfglibRxReturnCallback(void)
{
  uint8_t *payload;
  uint8_t payloadLength;
  uint8_t lqi;
  int8_t rssi;
  payload = emberStringCommandArgument(0, &payloadLength);
  lqi = (uint8_t) emberUnsignedCommandArgument(1);
  rssi = (int8_t) emberSignedCommandArgument(2);
  tmspHostMfglibRxReturn(payload,
                         payloadLength,
                         lqi,
                         rssi);
}

static void mfglibEndTestReturnCallback(void)
{
  EmberStatus status;
  uint32_t mfgReceiveCount;
  status = (EmberStatus) emberUnsignedCommandArgument(0);
  mfgReceiveCount = (uint32_t) emberUnsignedCommandArgument(1);
  tmspHostMfglibEndTestReturn(status,
                              mfgReceiveCount);
}

static void mfglibStartReturnCallback(void)
{
  uint8_t type;
  EmberStatus status;
  type = (uint8_t) emberUnsignedCommandArgument(0);
  status = (EmberStatus) emberUnsignedCommandArgument(1);
  tmspHostMfglibStartReturn(type,
                            status);
}

static void mfglibStopReturnCallback(void)
{
  uint8_t type;
  EmberStatus status;
  type = (uint8_t) emberUnsignedCommandArgument(0);
  status = (EmberStatus) emberUnsignedCommandArgument(1);
  tmspHostMfglibStopReturn(type,
                           status);
}

static void mfglibSendPacketEventHandlerCallback(void)
{
  EmberStatus status;
  status = (EmberStatus) emberUnsignedCommandArgument(0);
  tmspHostMfglibSendPacketEventHandler(status);
}

static void mfglibSetReturnCallback(void)
{
  uint8_t type;
  EmberStatus status;
  type = (uint8_t) emberUnsignedCommandArgument(0);
  status = (EmberStatus) emberUnsignedCommandArgument(1);
  tmspHostMfglibSetReturn(type,
                          status);
}

static void mfglibGetChannelReturnCallback(void)
{
  uint8_t channel;
  channel = (uint8_t) emberUnsignedCommandArgument(0);
  tmspHostMfglibGetChannelReturn(channel);
}

static void mfglibGetPowerReturnCallback(void)
{
  int8_t power;
  power = (int8_t) emberSignedCommandArgument(0);
  tmspHostMfglibGetPowerReturn(power);
}

static void mfglibGetPowerModeReturnCallback(void)
{
  uint16_t txPowerMode;
  txPowerMode = (uint16_t) emberUnsignedCommandArgument(0);
  tmspHostMfglibGetPowerModeReturn(txPowerMode);
}

static void mfglibGetSynOffsetReturnCallback(void)
{
  int8_t synOffset;
  synOffset = (int8_t) emberSignedCommandArgument(0);
  tmspHostMfglibGetSynOffsetReturn(synOffset);
}
#endif /* MFGLIB */

//------------------------------------------------------------------------------
// EMBER_TEST
//------------------------------------------------------------------------------
#ifdef EMBER_TEST

void emberConfigUart(uint8_t dropPercentage,
                     uint8_t corruptPercentage)
{
  emSendBinaryManagementCommand(EMBER_CONFIG_UART_COMMAND_IDENTIFIER,
                                "uu",
                                dropPercentage,
                                corruptPercentage);
}

void emberResetNcpAsh(void)
{
  emSendBinaryManagementCommand(EMBER_RESET_NCP_ASH_COMMAND_IDENTIFIER,
                                "");
}

void emberResetIpDriverAsh(void)
{
  emSendBinaryManagementCommand(EMBER_RESET_IP_DRIVER_ASH_COMMAND_IDENTIFIER,
                                "");
}

void emberStartUartStorm(uint16_t rate)
{
  emSendBinaryManagementCommand(EMBER_START_UART_STORM_COMMAND_IDENTIFIER,
                                "v",
                                rate);
}

void emberStopUartStorm(void)
{
  emSendBinaryManagementCommand(EMBER_STOP_UART_STORM_COMMAND_IDENTIFIER,
                                "");
}

void emberSendDone(uint32_t timeoutMs)
{
  emSendBinaryManagementCommand(EMBER_SEND_DONE_COMMAND_IDENTIFIER,
                                "w",
                                timeoutMs);
}

static void configUartReturnCallback(void)
{
  emberConfigUartReturn();
}

static void resetNcpAshReturnCallback(void)
{
  emberResetNcpAshReturn();
}

static void startUartStormReturnCallback(void)
{
  emberStartUartStormReturn();
}

static void stopUartStormReturnCallback(void)
{
  emberStopUartStormReturn();
}

static void sendDoneReturnCallback(void)
{
  emberSendDoneReturn();
}
#endif /* EMBER_TEST */

//------------------------------------------------------------------------------
// QA_THREAD_TEST
//------------------------------------------------------------------------------
#if (defined (EMBER_TEST) || defined(QA_THREAD_TEST) || defined(RTOS))

void emberGetNetworkKeyInfo(bool keyInUse)
{
  emSendBinaryManagementCommand(EMBER_GET_NETWORK_KEY_INFO_COMMAND_IDENTIFIER,
                                "u",
                                keyInUse);
}

void emberResetNcpGpio(void)
{
  emSendBinaryManagementCommand(EMBER_RESET_NCP_GPIO_COMMAND_IDENTIFIER,
                                "");
}

void emberEnableResetNcpGpio(uint8_t enable)
{
  emSendBinaryManagementCommand(EMBER_ENABLE_RESET_NCP_GPIO_COMMAND_IDENTIFIER,
                                "u",
                                enable);
}

void emberForceAssert(void)
{
  emSendBinaryManagementCommand(EMBER_FORCE_ASSERT_COMMAND_IDENTIFIER,
                                "");
}

void emberGetNodeStatus(void)
{
  emSendBinaryManagementCommand(EMBER_GET_NODE_STATUS_COMMAND_IDENTIFIER,
                                "");
}

void emberAddAddressData(const uint8_t *longId,
                         uint16_t shortId)
{
  emSendBinaryManagementCommand(EMBER_ADD_ADDRESS_DATA_COMMAND_IDENTIFIER,
                                "bv",
                                longId, 8,
                                shortId);
}

void emberClearAddressCache(void)
{
  emSendBinaryManagementCommand(EMBER_CLEAR_ADDRESS_CACHE_COMMAND_IDENTIFIER,
                                "");
}

void emberLookupAddressData(const uint8_t *longId)
{
  emSendBinaryManagementCommand(EMBER_LOOKUP_ADDRESS_DATA_COMMAND_IDENTIFIER,
                                "b",
                                longId, 8);
}

void emberStartUartSpeedTest(uint8_t payloadLength,
                             uint32_t timeoutMs,
                             uint32_t intervalMs)
{
  emSendBinaryManagementCommand(EMBER_START_UART_SPEED_TEST_COMMAND_IDENTIFIER,
                                "uww",
                                payloadLength,
                                timeoutMs,
                                intervalMs);
}

void emberNcpUdpStorm(uint8_t totalPackets,
                      const uint8_t *dest,
                      uint16_t payloadLength,
                      uint16_t txDelayMs)
{
  emSendBinaryManagementCommand(EMBER_NCP_UDP_STORM_COMMAND_IDENTIFIER,
                                "ubvv",
                                totalPackets,
                                dest, 16,
                                payloadLength,
                                txDelayMs);
}

static void getNetworkKeyInfoReturnCallback(void)
{
  EmberStatus status;
  uint32_t sequence;
  uint8_t state;
  status = (EmberStatus) emberUnsignedCommandArgument(0);
  sequence = (uint32_t) emberUnsignedCommandArgument(1);
  state = (uint8_t) emberUnsignedCommandArgument(2);
  emberGetNetworkKeyInfoReturn(status,
                               sequence,
                               state);
}

static void getNodeStatusReturnCallback(void)
{
  EmberStatus status;
  uint8_t ripId;
  EmberNodeId nodeId;
  uint8_t parentRipId;
  EmberNodeId parentId;
  uint8_t networkFragmentIdentifier[ISLAND_ID_SIZE];
  uint32_t networkFrameCounter;
  status = (EmberStatus) emberUnsignedCommandArgument(0);
  ripId = (uint8_t) emberUnsignedCommandArgument(1);
  nodeId = (EmberNodeId) emberUnsignedCommandArgument(2);
  parentRipId = (uint8_t) emberUnsignedCommandArgument(3);
  parentId = (EmberNodeId) emberUnsignedCommandArgument(4);
  emberGetStringArgument(5, networkFragmentIdentifier, ISLAND_ID_SIZE, false);
  networkFrameCounter = (uint32_t) emberUnsignedCommandArgument(6);
  emberGetNodeStatusReturn(status,
                           ripId,
                           nodeId,
                           parentRipId,
                           parentId,
                           networkFragmentIdentifier,
                           networkFrameCounter);
}

static void addAddressDataReturnCallback(void)
{
  uint16_t shortId;
  shortId = (uint16_t) emberUnsignedCommandArgument(0);
  emberAddAddressDataReturn(shortId);
}

static void clearAddressCacheReturnCallback(void)
{
  emberClearAddressCacheReturn();
}

static void lookupAddressDataReturnCallback(void)
{
  uint16_t shortId;
  shortId = (uint16_t) emberUnsignedCommandArgument(0);
  emberLookupAddressDataReturn(shortId);
}

static void uartSpeedTestReturnCallback(void)
{
  uint32_t totalBytesSent;
  uint32_t payloadBytesSent;
  uint32_t uartStormTimeoutMs;
  totalBytesSent = (uint32_t) emberUnsignedCommandArgument(0);
  payloadBytesSent = (uint32_t) emberUnsignedCommandArgument(1);
  uartStormTimeoutMs = (uint32_t) emberUnsignedCommandArgument(2);
  emberUartSpeedTestReturn(totalBytesSent,
                           payloadBytesSent,
                           uartStormTimeoutMs);
}

static void ncpUdpStormReturnCallback(void)
{
  EmberStatus status;
  status = (EmberStatus) emberUnsignedCommandArgument(0);
  emberNcpUdpStormReturn(status);
}

static void ncpUdpStormCompleteHandlerCallback(void)
{
  emberNcpUdpStormCompleteHandler();
}
#endif /* (defined (EMBER_TEST) || defined(QA_THREAD_TEST) || defined(RTOS)) */

static void setNetworkKeysCommand(void)
{
  uint8_t keySequence = emberUnsignedCommandArgument(0);
  uint8_t keyLength;
  EmberKeyData *key = ((EmberKeyData *)
                       emberStringCommandArgument(1, &keyLength));
  if (keyLength == EMBER_ENCRYPTION_KEY_SIZE) {
    emberSetNetworkKeysHandler(keySequence, key->contents, 0, NULL);
  }
}

//------------------------------------------------------------------------------
// APP_USES_SOFTWARE_FLOW_CONTROL
//------------------------------------------------------------------------------
#ifdef EMBER_APPLICATION_USES_SOFTWARE_FLOW_CONTROL

void emberStartXOnXOffTest(void)
{
  emSendBinaryManagementCommand(EMBER_START_XON_XOFF_TEST_COMMAND_IDENTIFIER,
                                "");
}
#endif /* EMBER_APPLICATION_USES_SOFTWARE_FLOW_CONTROL */

//------------------------------------------------------------------------------
// Command Handler Table

const EmberCommandEntry managementCallbackCommandTable[] = {
  emberBinaryCommandEntryAction(CB_RESET_MICRO_COMMAND_IDENTIFIER,
                                resetMicroHandlerCallback,
                                "u",
                                NULL),
  emberBinaryCommandEntryAction(CB_STATE_COMMAND_IDENTIFIER,
                                stateReturnCallback,
                                "bbbvuusubbb",
                                NULL),
  emberBinaryCommandEntryAction(CB_GET_VERSIONS_COMMAND_IDENTIFIER,
                                getVersionsReturnCallback,
                                "bvvvub",
                                NULL),
  emberBinaryCommandEntryAction(CB_GET_RIP_ENTRY_COMMAND_IDENTIFIER,
                                getRipEntryReturnCallback,
                                "ubusuuuuuu",
                                NULL),
  emberBinaryCommandEntryAction(CB_INIT_COMMAND_IDENTIFIER,
                                initReturnCallback,
                                "u",
                                NULL),
  emberBinaryCommandEntryAction(CB_GET_COUNTER_COMMAND_IDENTIFIER,
                                getCounterReturnCallback,
                                "uv",
                                NULL),
  emberBinaryCommandEntryAction(CB_SET_SECURITY_PARAMETERS_COMMAND_IDENTIFIER,
                                setSecurityParametersReturnCallback,
                                "u",
                                NULL),
  emberBinaryCommandEntryAction(CB_SWITCH_TO_NEXT_NETWORK_KEY_COMMAND_IDENTIFIER,
                                switchToNextNetworkKeyReturnCallback,
                                "u",
                                NULL),
  emberBinaryCommandEntryAction(CB_FORM_NETWORK_COMMAND_IDENTIFIER,
                                formNetworkReturnCallback,
                                "u",
                                NULL),
  emberBinaryCommandEntryAction(CB_JOIN_NETWORK_COMMAND_IDENTIFIER,
                                joinNetworkReturnCallback,
                                "u",
                                NULL),
  emberBinaryCommandEntryAction(CB_RESUME_NETWORK_COMMAND_IDENTIFIER,
                                resumeNetworkReturnCallback,
                                "u",
                                NULL),
  emberBinaryCommandEntryAction(CB_ATTACH_TO_NETWORK_COMMAND_IDENTIFIER,
                                attachToNetworkReturnCallback,
                                "u",
                                NULL),
  emberBinaryCommandEntryAction(CB_ENERGY_SCAN_COMMAND_IDENTIFIER,
                                energyScanHandlerCallback,
                                "us",
                                NULL),
  emberBinaryCommandEntryAction(CB_ACTIVE_SCAN_COMMAND_IDENTIFIER,
                                activeScanHandlerCallback,
                                "uuuubvbbuubu",
                                NULL),
  emberBinaryCommandEntryAction(CB_SCAN_COMMAND_IDENTIFIER,
                                scanReturnCallback,
                                "u",
                                NULL),
  emberBinaryCommandEntryAction(CB_SET_ADDRESS_COMMAND_IDENTIFIER,
                                setAddressHandlerCallback,
                                "b",
                                NULL),
  emberBinaryCommandEntryAction(CB_START_HOST_JOIN_CLIENT_COMMAND_IDENTIFIER,
                                startHostJoinClientHandlerCallback,
                                "b",
                                NULL),
  emberBinaryCommandEntryAction(CB_SET_TX_POWER_MODE_COMMAND_IDENTIFIER,
                                setTxPowerModeReturnCallback,
                                "u",
                                NULL),
  emberBinaryCommandEntryAction(CB_GET_TX_POWER_MODE_COMMAND_IDENTIFIER,
                                getTxPowerModeReturnCallback,
                                "v",
                                NULL),
  emberBinaryCommandEntryAction(CB_GET_MULTICAST_ENTRY_COMMAND_IDENTIFIER,
                                getMulticastEntryReturnCallback,
                                "uuub",
                                NULL),
  emberBinaryCommandEntryAction(CB_GET_CCA_THRESHOLD_COMMAND_IDENTIFIER,
                                getCcaThresholdReturnCallback,
                                "s",
                                NULL),
  emberBinaryCommandEntryAction(CB_SET_CCA_THRESHOLD_COMMAND_IDENTIFIER,
                                setCcaThresholdReturnCallback,
                                "u",
                                NULL),
  emberBinaryCommandEntryAction(CB_GET_RADIO_POWER_COMMAND_IDENTIFIER,
                                getRadioPowerReturnCallback,
                                "s",
                                NULL),
  emberBinaryCommandEntryAction(CB_SET_RADIO_POWER_COMMAND_IDENTIFIER,
                                setRadioPowerReturnCallback,
                                "u",
                                NULL),
  emberBinaryCommandEntryAction(CB_ECHO_COMMAND_IDENTIFIER,
                                echoReturnCallback,
                                "b",
                                NULL),
  emberBinaryCommandEntryAction(CB_ASSERT_INFO_COMMAND_IDENTIFIER,
                                assertInfoReturnCallback,
                                "bw",
                                NULL),
  emberBinaryCommandEntryAction(CB_CONFIGURE_GATEWAY_COMMAND_IDENTIFIER,
                                configureGatewayReturnCallback,
                                "u",
                                NULL),
  emberBinaryCommandEntryAction(CB_GET_CHANNEL_CAL_DATA_TOKEN_COMMAND_IDENTIFIER,
                                getChannelCalDataTokenReturnCallback,
                                "usus",
                                NULL),
  emberBinaryCommandEntryAction(CB_POLL_FOR_DATA_COMMAND_IDENTIFIER,
                                pollForDataReturnCallback,
                                "uu",
                                NULL),
  emberBinaryCommandEntryAction(CB_DEEP_SLEEP_COMMAND_IDENTIFIER,
                                deepSleepReturnCallback,
                                "u",
                                NULL),
  emberBinaryCommandEntryAction(CB_STACK_POLL_FOR_DATA_COMMAND_IDENTIFIER,
                                stackPollForDataReturnCallback,
                                "u",
                                NULL),
  emberBinaryCommandEntryAction(CB_OK_TO_NAP_COMMAND_IDENTIFIER,
                                okToNapReturnCallback,
                                "u",
                                NULL),
  emberBinaryCommandEntryAction(CB_DEEP_SLEEP_CALLBACK_COMMAND_IDENTIFIER,
                                deepSleepCallbackCallback,
                                "v",
                                NULL),
  emberBinaryCommandEntryAction(CB_RESET_NETWORK_STATE_COMMAND_IDENTIFIER,
                                resetNetworkStateReturnCallback,
                                "u",
                                NULL),
  emberBinaryCommandEntryAction(CB_EXTERNAL_ROUTE_CHANGE_COMMAND_IDENTIFIER,
                                externalRouteChangeCallback,
                                "bu",
                                NULL),
  emberBinaryCommandEntryAction(CB_DHCP_SERVER_CHANGE_COMMAND_IDENTIFIER,
                                dhcpServerChangeCallback,
                                "bu",
                                NULL),
  emberBinaryCommandEntryAction(CB_ADDRESS_CONFIGURATION_CHANGE_COMMAND_IDENTIFIER,
                                addressConfigurationChangeCallback,
                                "bwwu",
                                NULL),
  emberBinaryCommandEntryAction(CB_REQUEST_DHCP_ADDRESS_COMMAND_IDENTIFIER,
                                requestDhcpAddressReturnCallback,
                                "ub",
                                NULL),
  emberBinaryCommandEntryAction(CB_COMMISSION_NETWORK_COMMAND_IDENTIFIER,
                                commissionNetworkReturnCallback,
                                "u",
                                NULL),
  emberBinaryCommandEntryAction(CB_GET_GLOBAL_ADDRESS_COMMAND_IDENTIFIER,
                                getGlobalAddressReturnCallback,
                                "bwwu",
                                NULL),
  emberBinaryCommandEntryAction(CB_GET_DHCP_CLIENT_COMMAND_IDENTIFIER,
                                getDhcpClientReturnCallback,
                                "b",
                                NULL),
  emberBinaryCommandEntryAction(CB_BECOME_COMMISSIONER_COMMAND_IDENTIFIER,
                                becomeCommissionerReturnCallback,
                                "u",
                                NULL),
  emberBinaryCommandEntryAction(CB_SEND_STEERING_DATA_COMMAND_IDENTIFIER,
                                sendSteeringDataReturnCallback,
                                "u",
                                NULL),
  emberBinaryCommandEntryAction(CB_CHANGE_NODE_TYPE_COMMAND_IDENTIFIER,
                                changeNodeTypeReturnCallback,
                                "u",
                                NULL),
  emberBinaryCommandEntryAction(CB_GET_GLOBAL_PREFIX_COMMAND_IDENTIFIER,
                                getGlobalPrefixReturnCallback,
                                "ubuww",
                                NULL),
  emberBinaryCommandEntryAction(CB_RESIGN_GLOBAL_ADDRESS_COMMAND_IDENTIFIER,
                                resignGlobalAddressReturnCallback,
                                "u",
                                NULL),
  emberBinaryCommandEntryAction(CB_SLAAC_SERVER_CHANGE_COMMAND_IDENTIFIER,
                                slaacServerChangeCallback,
                                "bu",
                                NULL),
  emberBinaryCommandEntryAction(CB_REQUEST_SLAAC_ADDRESS_COMMAND_IDENTIFIER,
                                requestSlaacAddressReturnCallback,
                                "ub",
                                NULL),
  emberBinaryCommandEntryAction(CB_CUSTOM_NCP_TO_HOST_MESSAGE_COMMAND_IDENTIFIER,
                                customNcpToHostMessageCallback,
                                "b",
                                NULL),
  emberBinaryCommandEntryAction(CB_NCP_TO_HOST_NO_OP_COMMAND_IDENTIFIER,
                                ncpToHostNoOpCallback,
                                "b",
                                NULL),
  emberBinaryCommandEntryAction(CB_COMMISSIONER_STATUS_COMMAND_IDENTIFIER,
                                commissionerStatusHandlerCallback,
                                "vb",
                                NULL),
  emberBinaryCommandEntryAction(CB_LEADER_DATA_COMMAND_IDENTIFIER,
                                leaderDataHandlerCallback,
                                "b",
                                NULL),
  emberBinaryCommandEntryAction(CB_GET_NETWORK_DATA_TLV_COMMAND_IDENTIFIER,
                                getNetworkDataTlvReturnCallback,
                                "uuub",
                                NULL),
  emberBinaryCommandEntryAction(CB_GET_ROUTING_LOCATOR_COMMAND_IDENTIFIER,
                                getRoutingLocatorReturnCallback,
                                "b",
                                NULL),
  emberBinaryCommandEntryAction(CB_SET_RANDOMIZE_MAC_EXTENDED_ID_COMMAND_IDENTIFIER,
                                setRandomizeMacExtendedIdReturnCallback,
                                "",
                                NULL),
  emberBinaryCommandEntryAction(CB_CONFIGURE_EXTERNAL_ROUTE_COMMAND_IDENTIFIER,
                                configureExternalRouteReturnCallback,
                                "u",
                                NULL),
  emberBinaryCommandEntryAction(CB_ALLOW_NATIVE_COMMISSIONER_COMMAND_IDENTIFIER,
                                allowNativeCommissionerReturnCallback,
                                "u",
                                NULL),
  emberBinaryCommandEntryAction(CB_SET_COMMISSIONER_KEY_COMMAND_IDENTIFIER,
                                setCommissionerKeyReturnCallback,
                                "u",
                                NULL),
  emberBinaryCommandEntryAction(CB_NETWORK_STATUS_COMMAND_IDENTIFIER,
                                networkStatusHandlerCallback,
                                "uuu",
                                NULL),
  emberBinaryCommandEntryAction(CB_GET_STANDALONE_BOOTLOADER_INFO_COMMAND_IDENTIFIER,
                                getStandaloneBootloaderInfoReturnCallback,
                                "vuuu",
                                NULL),
  emberBinaryCommandEntryAction(CB_LAUNCH_STANDALONE_BOOTLOADER_COMMAND_IDENTIFIER,
                                launchStandaloneBootloaderReturnCallback,
                                "u",
                                NULL),
  emberBinaryCommandEntryAction(CB_GET_MFG_TOKEN_COMMAND_IDENTIFIER,
                                getMfgTokenReturnCallback,
                                "uub",
                                NULL),
  emberBinaryCommandEntryAction(CB_SET_MFG_TOKEN_COMMAND_IDENTIFIER,
                                setMfgTokenReturnCallback,
                                "uu",
                                NULL),
  emberBinaryCommandEntryAction(CB_HOST_STATE_COMMAND_IDENTIFIER,
                                hostStateHandlerCallback,
                                "bbbvuusubbb",
                                NULL),
  emberBinaryCommandEntryAction(CB_SET_NETWORK_KEYS_COMMAND_IDENTIFIER,
                                setNetworkKeysCommand,
                                "wb",
                                NULL),

#ifdef MFGLIB
  emberBinaryCommandEntryAction(CB_MFGLIB_START_TEST_COMMAND_IDENTIFIER,
                                mfglibStartTestReturnCallback,
                                "u",
                                NULL),
  emberBinaryCommandEntryAction(CB_MFGLIB_RX_COMMAND_IDENTIFIER,
                                mfglibRxReturnCallback,
                                "bus",
                                NULL),
  emberBinaryCommandEntryAction(CB_MFGLIB_END_TEST_COMMAND_IDENTIFIER,
                                mfglibEndTestReturnCallback,
                                "uw",
                                NULL),
  emberBinaryCommandEntryAction(CB_MFGLIB_START_COMMAND_IDENTIFIER,
                                mfglibStartReturnCallback,
                                "uu",
                                NULL),
  emberBinaryCommandEntryAction(CB_MFGLIB_STOP_COMMAND_IDENTIFIER,
                                mfglibStopReturnCallback,
                                "uu",
                                NULL),
  emberBinaryCommandEntryAction(CB_MFGLIB_SEND_PACKET_EVENT_COMMAND_IDENTIFIER,
                                mfglibSendPacketEventHandlerCallback,
                                "u",
                                NULL),
  emberBinaryCommandEntryAction(CB_MFGLIB_SET_COMMAND_IDENTIFIER,
                                mfglibSetReturnCallback,
                                "uu",
                                NULL),
  emberBinaryCommandEntryAction(CB_MFGLIB_GET_CHANNEL_COMMAND_IDENTIFIER,
                                mfglibGetChannelReturnCallback,
                                "u",
                                NULL),
  emberBinaryCommandEntryAction(CB_MFGLIB_GET_POWER_COMMAND_IDENTIFIER,
                                mfglibGetPowerReturnCallback,
                                "s",
                                NULL),
  emberBinaryCommandEntryAction(CB_MFGLIB_GET_POWER_MODE_COMMAND_IDENTIFIER,
                                mfglibGetPowerModeReturnCallback,
                                "v",
                                NULL),
  emberBinaryCommandEntryAction(CB_MFGLIB_GET_SYN_OFFSET_COMMAND_IDENTIFIER,
                                mfglibGetSynOffsetReturnCallback,
                                "s",
                                NULL),
#endif /* MFGLIB */
#ifdef EMBER_TEST
  emberBinaryCommandEntryAction(CB_CONFIG_UART_COMMAND_IDENTIFIER,
                                configUartReturnCallback,
                                "",
                                NULL),
  emberBinaryCommandEntryAction(CB_RESET_NCP_ASH_COMMAND_IDENTIFIER,
                                resetNcpAshReturnCallback,
                                "",
                                NULL),
  emberBinaryCommandEntryAction(CB_START_UART_STORM_COMMAND_IDENTIFIER,
                                startUartStormReturnCallback,
                                "",
                                NULL),
  emberBinaryCommandEntryAction(CB_STOP_UART_STORM_COMMAND_IDENTIFIER,
                                stopUartStormReturnCallback,
                                "",
                                NULL),
  emberBinaryCommandEntryAction(CB_SEND_DONE_COMMAND_IDENTIFIER,
                                sendDoneReturnCallback,
                                "",
                                NULL),
#endif /* EMBER_TEST */
#if (defined (EMBER_TEST) || defined(QA_THREAD_TEST) || defined(RTOS))
  emberBinaryCommandEntryAction(CB_GET_NETWORK_KEY_INFO_COMMAND_IDENTIFIER,
                                getNetworkKeyInfoReturnCallback,
                                "uwu",
                                NULL),
  emberBinaryCommandEntryAction(CB_GET_NODE_STATUS_COMMAND_IDENTIFIER,
                                getNodeStatusReturnCallback,
                                "uuvuvbw",
                                NULL),
  emberBinaryCommandEntryAction(CB_ADD_ADDRESS_DATA_COMMAND_IDENTIFIER,
                                addAddressDataReturnCallback,
                                "v",
                                NULL),
  emberBinaryCommandEntryAction(CB_CLEAR_ADDRESS_CACHE_COMMAND_IDENTIFIER,
                                clearAddressCacheReturnCallback,
                                "",
                                NULL),
  emberBinaryCommandEntryAction(CB_LOOKUP_ADDRESS_DATA_COMMAND_IDENTIFIER,
                                lookupAddressDataReturnCallback,
                                "v",
                                NULL),
  emberBinaryCommandEntryAction(CB_UART_SPEED_TEST_COMMAND_IDENTIFIER,
                                uartSpeedTestReturnCallback,
                                "www",
                                NULL),
  emberBinaryCommandEntryAction(CB_NCP_UDP_STORM_COMMAND_IDENTIFIER,
                                ncpUdpStormReturnCallback,
                                "u",
                                NULL),
  emberBinaryCommandEntryAction(CB_NCP_UDP_STORM_COMPLETE_COMMAND_IDENTIFIER,
                                ncpUdpStormCompleteHandlerCallback,
                                "",
                                NULL),
#endif /* (defined (EMBER_TEST) || defined(QA_THREAD_TEST) || defined(RTOS)) */
#ifdef EMBER_APPLICATION_USES_SOFTWARE_FLOW_CONTROL
#endif /* EMBER_APPLICATION_USES_SOFTWARE_FLOW_CONTROL */
  { NULL }    // terminator
};
