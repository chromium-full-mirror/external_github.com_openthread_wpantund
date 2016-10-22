#ifndef __PHY_H__
#define __PHY_H__

// Redefinition mechanism to build dual phy Connect in a single image.
#if defined (EMBER_STACK_CONNECT) && defined(CONNECT_DUAL_PHY)
#if defined (PHY_EM3XX)
  #include "mac/dual-phy-renaming-em3xx.h"
#elif defined (PHY_PRO2PLUS)
  #include "mac/dual-phy-renaming-pro2plus.h"
#endif
#endif // EMBER_STACK_CONNECT && CONNECT_DUAL_PHY

#define IS_VALID_CHANNEL(newValue)                           \
  ( ((newValue) <= EMBER_MAX_802_15_4_CHANNEL_NUMBER) &&     \
    ( (EMBER_MIN_802_15_4_CHANNEL_NUMBER == 0) ||            \
      ((newValue) >= EMBER_MIN_802_15_4_CHANNEL_NUMBER) ) )

#define IS_VALID_CHANNEL_INDEX(newValue)                     \
  ( (newValue) < EMBER_NUM_802_15_4_CHANNELS )

// 802.15.4 PHY payload length.
#define PHY_MAX_SIZE          127
// 802.15.4 PHY header length.
#define PHY_LENGTH_BYTE_SIZE  1
// 802.15.4 MAC footer length
#define PHY_APPENDED_CRC_SIZE 2

// IEEE 802.15.4-2003 6.1.6:
#define IEEE_154_PER_TEST_PHY_PAYLOAD_LENGTH 20

// where do error codes go?
#define ERR_RADIO_TX_BUSY  1
#define CCA_BUSY 0
#define CCA_CLEAR 1
#define LQI_DONT_USE 0

// An int8_t value that represents an invalid RSSI energy level.
// Maybe someday we'll need to make it public in ember-types.h.
#define EMBER_PHY_INVALID_RSSI -128

//------------------------------------------------------------------
// 15.4 MAC Frame Control and Information Element fields move to
// separate file for clients that need only those macros.
//------------------------------------------------------------------
#include "ieee802154mac.h"

//------------------------------------------------------------------------
// 15.4 Commands used by phy code
#define DATA_REQUEST_COMMAND 0x04

//------------------------------------------------------------------
// 15.4g 16-bit PHY Header (PHR) definitions -- in host order
#define PHR_154G_MODE_SWITCH_MASK        0x8000u
#define PHR_154G_MS_PARAMETER_ENTRY_MASK 0x6000u
#define PHR_154G_MS_FEC_MASK             0x1000u
#define PHR_154G_MS_NEW_MODE_MASK        0x0FE0u
#define PHR_154G_MS_CHECKSUM_MASK        0x001Eu
#define PHR_154G_MS_PARITY_MASK          0x0001u
#define PHR_154G_FCS_MASK                0x1000u
#define PHR_154G_DW_MASK                 0x0800u
#define PHR_154G_FRAME_LENGTH_MASK       0x07FFu

//------------------------------------------------------------------
// Miscellaneous PHY functions.

// This function stores the 64-bit EUI.  It relies on the value already having
// been stored in the emLocalEui64 variable.  For some implementations,
// this variable may be memory-mapped over the appropriate registers, in
// which case this function may evaluate to whitespace as the work is already
// done.
void emPhySetEui64(void);

//------------------------------------------------------------------
// PHY Power Management.

// Radio power modes.
enum {
  EMBER_RADIO_POWER_MODE_RX_ON,
  EMBER_RADIO_POWER_MODE_ED_ON = EMBER_RADIO_POWER_MODE_RX_ON,
  EMBER_RADIO_POWER_MODE_OFF
};
typedef uint8_t RadioPowerMode;

void emRadioInit(RadioPowerMode initialRadioPowerMode);

// Turns the radio power completely off immediately.
void emRadioSleep(void);

// Wakes the radio up from sleep and puts it in RX mode.
void emRadioWakeUp(void);

// Returns the current power status of the radio.
RadioPowerMode emRadioGetPowerStatus(void);


//------------------------------------------------------------------
// Transmit APIs.

// the mac will call this function
// -memptr must point to a phy packet.
// -memptr[0] (length byte) must be set to the value that is to be transmitted
//  over the air.
//  This function does not modify the length byte to reflect the two CRC bytes
//  that the phy appends.
EmberStatus emRadioTransmit(uint8_t* memptr);

// the mac will implement this function
extern void emRadioTransmitCompleteCallback(EmberStatus status,
                                            uint32_t sfdSentTime,
                                            uint8_t framePending);

extern void emRadioReceiveCompleteCallback(EmberMessageBuffer header);

// Returns true if radio needs calibrating; false otherwise.
// This function must not be called while a transmit is in progress.
bool emRadioCheckRadio(void);

// Setters and getters for radio node id and pan id.
EmberNodeId emRadioGetNodeId(void);
void emRadioSetNodeId(uint16_t nodeId);
EmberPanId emRadioGetPanId(void);
void emRadioSetPanId(uint16_t panId);

// APIs for dealing with MAC vs. PHY channel numbering
#define INVALID_CHANNEL 0xFF
#ifndef MAC_HAS_CHANNEL_PAGES
#define MAC_HAS_CHANNEL_PAGES ( defined(CORTEXM3_EMBER_MICRO) \
                              &&(defined(PHY_PRO2PLUS) || defined(PHY_PRO2)) \
                              && !defined(EMBER_STACK_CONNECT) )
#endif//MAC_HAS_CHANNEL_PAGES

// This API returns INVALID_CHANNEL if the passed channel is invalid.
uint8_t emPhyGetPhyChannel(uint8_t macPgChan);

#if     MAC_HAS_CHANNEL_PAGES
  #define MAX_CHANNELS_PER_PAGE     27u // channels 0-26 // Must be < 32!
  #define CHANNEL_BITS              5u  // need 5 bits for 27 channels
  // Some macros for messing with single-byte-encoded MAC Page+Channel values
  #define emMacPgChanPg(macPgChan)  ((uint8_t)(macPgChan) >> CHANNEL_BITS)
  #define emMacPgChanCh(macPgChan)  ((uint8_t)(macPgChan) & (BIT(CHANNEL_BITS)-1))
  #define emMacPgChan(page, chan)   ( ((uint8_t)((page) << CHANNEL_BITS)) \
                                    | ((chan) & (BIT(CHANNEL_BITS)-1)) )
  void emPhyEnableChannelMaskEnforcement(bool enable);
  bool emPhyChannelMaskEnforcementEnabled(void);
  EmberStatus emPhySetChannelMask(uint32_t pageAndChannelMask);
  uint32_t emPhyGetChannelMask(uint8_t macPage);
  EmberStatus emPhySetChannelPageToUse(uint8_t macPage);
  uint8_t emPhyGetChannelPageInUse(void);
  uint8_t emPhyGetChannelPageForChannel(uint8_t macPgChan);
#else//!MAC_HAS_CHANNEL_PAGES
  #undef  MAC_HAS_CHANNEL_PAGES // Prevent some lame-o using #ifdef vs. #if
  #define MAX_CHANNELS_PER_PAGE     255u // channels 0-254
  #define CHANNEL_BITS              8u   // need 8 bits for 255 channels
  // Some macros for messing with single-byte-encoded MAC Page+Channel values
  #define emMacPgChanPg(macPgChan)  0
  #define emMacPgChanCh(macPgChan)  (macPgChan)
  #define emMacPgChan(page, chan)   (chan)
  #define emPhyEnableChannelMaskEnforcement(enable) /*no-op*/
  #define emPhyChannelMaskEnforcementEnabled(void)  (false)
  #define emPhySetChannelMask(pageAndChannelMask)   (EMBER_ERR_FATAL)
  #define emPhyGetChannelMask(macPage)              (0ul)
  #define emPhySetChannelPageToUse(macPage) ( (macPage) ? EMBER_ERR_FATAL \
                                                        : EMBER_SUCCESS )
  #define emPhyGetChannelPageInUse()                (0)
  #define emPhyGetChannelPageForChannel(macPgChan)  (0)

#endif//MAC_HAS_CHANNEL_PAGES
#define PHY_INVALID_FREQ_HZ 0xFFFFFFFFul
uint32_t emPhyGetChannelFreqHz(uint8_t macPgChan);

// Setters and getters for radio channel and power.
EmberStatus emSetPhyRadioChannel(uint8_t radioChannel);
uint8_t emGetPhyRadioChannel(void);
EmberStatus emSetPhyRadioPower(int8_t power);
int8_t emGetPhyRadioPower(void);
// Set floating point radio power
EmberStatus emSetPhyRadioPowerFl(int16_t power);
int16_t emGetPhyRadioPowerFl(void);

uint16_t emGetTxPowerMode(void);

uint8_t emRadioGetCca(void);

// Gets a true random number out of radios that support this.
// This will typically take a while, and so should be used to seed a PRNG
// and not as a source of random numbers for regular use.
// Returns true if the call succeeds, and false otherwise.
bool emRadioGetRandomNumbers(uint16_t *rn, uint8_t count);

// Old interface which only gets a single uint16_t.
#define emRadioGetRandomNumber(rn) \
 (emRadioGetRandomNumbers((rn), 1))

// Use the radio to generate a seed for the HAL pseudo-random number generator.
void emRadioSeedRandom(void);

#ifndef  BOOTLOADER_PHY
void emRadioEnableAddressMatching(uint8_t enable);
uint8_t emRadioAddressMatchingEnabled(void);

void emRadioEnableAutoAck(bool enable);
bool emRadioAutoAckEnabled(void);
bool emRadioHoldOffIsActive(void);
#endif// BOOTLOADER_PHY

void emRadioEnablePacketTrace(bool enable);
bool emRadioPacketTraceEnabled(void);

void emRadioEnableReceiveCrc(bool enable);
bool emRadioReceiveCrcEnabled(void);

void emStartTransmitStream(void);

void emStopTransmitStream(void);

void emStartTransmitTone(void);

void emStopTransmitTone(void);

void emMacCheckIncomingQueue(void);

#if ( (defined(PHY_EM3XX) || defined(PHY_EFR32)) \
    &&(!defined(BOOTLOADER_PHY))                 \
    &&(!defined(EMBER_TEST))                     \
    &&( defined(EMBER_STACK_IP))                 \
    )
  void emRadioPowerFem(bool powerUp);
#else
  #define emRadioPowerFem(powerUp) /* no-op */
#endif

#if defined( PHY_EM2420 )
  #error The EM2420 is no longer supported.
#elif defined( PHY_EM2420B )
  #error The EM2420 is no longer supported.
#elif defined( PHY_EM250 )
  #include "em250/phy.h"
#elif defined( PHY_EM250B )
  #include "em250b/phy.h"
#elif defined( PHY_EM3XX )
  #include "em3xx/phy.h"
#elif defined( PHY_SI446X_US ) || defined( PHY_SI446X_EU ) || defined( PHY_SI446X_CN ) || defined( PHY_SI446X_JP )
  #include "si446x/phy.h"
#elif defined( PHY_COBRA )
  #include "cobra/phy.h"
#elif defined( PHY_PRO2PLUS )
  #include "pro2class/phy.h"
#elif defined( PHY_PRO2 ) || defined( PHY_EZR2 )
  #define PHY_PRO2PLUS 1 // So don't have to add PHY_PRO2 to many places
  #include "pro2class/phy.h"
#elif defined( PHY_EFR32 )
  #include "efr32/phy.h"
#elif !defined( PHY_NULL )
  #error No radio defined - include ember-config.h
#endif

// The above per-PHY include file should define the following parameters
// (with example values shown for two representative PHY possibilities):
//                               QPSK       802.15.4            MR-FSK
// Per-PHY Parameter Name        2011       Parameter           2011-g
// ----------------------------  ---------  ------------------  ------
// EMBER_PHY_MAX_PHR_BYTES       1          aMRFSKPHRLength---> 2
// EMBER_PHY_MAX_PAYLOAD_BYTES   127        aMaxPHYPacketSize   2047
// EMBER_PHY_MAX_CRC_BYTES       2          --                  4
// EMBER_PHY_DATA_RATE_BPS       250        kb/s                100
// EMBER_PHY_BIT_TIME_US         4          (1000/data rate)    10
// EMBER_PHY_SYMBOL_BITS         4        8/phySymbolsPerOctet  1
// EMBER_PHY_PREAMBLE_BITS       32 Sym2Bit(phyPreambleSymbolLength)
// EMBER_PHY_SFD_BITS            8  Sym2Bit(phySHRDuration-phyPreambleSymbolLength)
//                          -or-          8*aMRFSKSFDLength---> 16
// EMBER_PHY_BYTE_BITS           8  Sym2Bit(phySymbolsPerOctet) 8
// EMBER_PHY_TURNAROUND_SYMBOLS  12         aTurnaroundTime     1ms / n us/sym
// EMBER_PHY_CCA_SYMBOLS         8          aCCATime            8
//                          -or- 0..1000    phyCCADuration*     0..1000
//                              *For PHYs operating in Japan 920,950MHz band
//
// The per-PHY include file can also optionally override:
// PHY_MAX_SIZE
// EMBER_PHY_SYMBOL_DURATION_US
// EMBER_PHY_ACK_TIMEOUT_SYMBOLS
// EMBER_PHY_BACKOFF_SYMBOLS

#ifndef EMBER_PHY_MAX_PAYLOAD_BYTES
#define EMBER_PHY_MAX_PAYLOAD_BYTES PHY_MAX_SIZE
#endif//EMBER_PHY_MAX_PAYLOAD_BYTES
#if     (EMBER_PHY_MAX_PAYLOAD_BYTES > PHY_MAX_SIZE)
 #warning Limiting EMBER_PHY_MAX_PAYLOAD_BYTES to PHY_MAX_SIZE
 #undef  EMBER_PHY_MAX_PAYLOAD_BYTES
 #define EMBER_PHY_MAX_PAYLOAD_BYTES PHY_MAX_SIZE
#endif//(EMBER_PHY_MAX_PAYLOAD_BYTES > PHY_MAX_SIZE)

// Convenience macros and definitions for converting per-PHY parameters.
// These should work fine for known PHYs yielding values that will fit
// within 16-bit (or larger) signed quantities.
#define EMBER_PHY_BITS_TO_BYTES(bits)     (((bits) + EMBER_PHY_BYTE_BITS - 1) / EMBER_PHY_BYTE_BITS)
#define EMBER_PHY_BITS_TO_SYMBOLS(bits)   (((bits) + EMBER_PHY_SYMBOL_BITS - 1) / EMBER_PHY_SYMBOL_BITS)
#define EMBER_PHY_BYTES_TO_SYMBOLS(bytes) (((bytes) * EMBER_PHY_BYTE_BITS) / EMBER_PHY_SYMBOL_BITS)
#define EMBER_PHY_BITS_TO_US(bits)        ((bits) * EMBER_PHY_BIT_TIME_US)
#define EMBER_PHY_BYTES_TO_US(bytes)      ((bytes) * EMBER_PHY_BITS_TO_US(EMBER_PHY_BYTE_BITS))
#define EMBER_PHY_SYMBOL_TIME_US          EMBER_PHY_BITS_TO_US(EMBER_PHY_SYMBOL_BITS)
#define EMBER_PHY_SYMBOLS_TO_US(symbols)  ((symbols) * EMBER_PHY_SYMBOL_TIME_US)
// Some PHYs separate actual Symbol 'time' from 'duration' where the former
// is actual time on the air, and the latter is a worst-case duration applied
// to CSMA, CCA, and MAC timing/timeouts.  Otherwise treat 'em the same.
#ifndef EMBER_PHY_SYMBOL_DURATION_US
#define EMBER_PHY_SYMBOL_DURATION_US      EMBER_PHY_SYMBOL_TIME_US
#endif//EMBER_PHY_SYMBOL_DURATION_US
#define EMBER_PHY_SYMBOLS_TO_DURATION_US(symbols) \
                                          ((symbols) * EMBER_PHY_SYMBOL_DURATION_US)
#define EMBER_PHY_CCA_DURATION_US         EMBER_PHY_SYMBOLS_TO_DURATION_US(EMBER_PHY_CCA_SYMBOLS)
#define EMBER_PHY_TURNAROUND_DURATION_US  EMBER_PHY_SYMBOLS_TO_DURATION_US(EMBER_PHY_TURNAROUND_SYMBOLS)
#define EMBER_PHY_ACK_TIMEOUT_DURATION_US EMBER_PHY_SYMBOLS_TO_DURATION_US(EMBER_PHY_ACK_TIMEOUT_SYMBOLS)
// 802.15.4-2011g Section 6.4.3:
// macAckWaitDuration = aUnitBackoffPeriod + aTurnaroundTime
//   + phySHRDuration + ceil((PHR+FCF+Seq#+CRC) * phySymbolsPerOctet)
// where:
// aUnitBackoffPeriod = aTurnaroundTime + aCCATime (or phyCCADuration)
// phySHRDuration     = PreambleDuration + SFDDuration
#ifndef EMBER_PHY_BACKOFF_SYMBOLS
#define EMBER_PHY_BACKOFF_SYMBOLS ( 0                          \
          + EMBER_PHY_TURNAROUND_SYMBOLS                            \
          + EMBER_PHY_CCA_SYMBOLS                                   \
          )
#endif//EMBER_PHY_BACKOFF_SYMBOLS
#ifndef EMBER_PHY_ACK_TIMEOUT_SYMBOLS
#define EMBER_PHY_ACK_TIMEOUT_SYMBOLS ( 0                           \
          + EMBER_PHY_BACKOFF_SYMBOLS                          \
          + EMBER_PHY_TURNAROUND_SYMBOLS                            \
          + EMBER_PHY_BITS_TO_SYMBOLS(EMBER_PHY_PREAMBLE_BITS)      \
          + EMBER_PHY_BITS_TO_SYMBOLS(EMBER_PHY_SFD_BITS)           \
          + ((EMBER_PHY_MAX_PHR_BYTES + 3 + EMBER_PHY_MAX_CRC_BYTES)\
             * EMBER_PHY_BITS_TO_SYMBOLS(EMBER_PHY_BYTE_BITS))      \
          )
#endif//EMBER_PHY_ACK_TIMEOUT_SYMBOLS

#ifdef EMBER_TEST
bool _radioReceive(uint8_t *packet, uint32_t rxSynctime, uint8_t linkQuality);
void _radioTransmitComplete(void);
#endif

#ifndef EMBER_STACK_IP
// On the ZIP stack all ember... / mfglib... APIs are renamed to
// emApi... / mfglibApi... due to RTOS namespace conflicts.
// This change doesn't apply to ZNET.
  #define emApiSetTxPowerMode emberSetTxPowerMode
  #define mfglibApiStart mfglibStart
  #define mfglibApiEnd mfglibEnd
  #define mfglibApiStartTone mfglibStartTone
  #define mfglibApiStopTone mfglibStopTone
  #define mfglibApiStartStream mfglibStartStream
  #define mfglibApiStopStream mfglibStopStream
  #define mfglibApiSendPacket mfglibSendPacket
  #define mfglibApiSetChannel mfglibSetChannel
  #define mfglibApiGetChannel mfglibGetChannel
  #define mfglibApiSetPower mfglibSetPower
  #define mfglibApiGetPower mfglibGetPower
  #define mfglibApiGetPowerMode mfglibGetPowerMode
  #define mfglibApiSetSynOffset mfglibSetSynOffset
  #define mfglibApiGetSynOffset mfglibGetSynOffset
  #define mfglibApiTestContModCal mfglibTestContModCal
#endif

#endif //__PHY_H__
