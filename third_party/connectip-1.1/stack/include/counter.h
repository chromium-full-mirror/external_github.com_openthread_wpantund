#ifndef __EMBER_COUNTER_H__
#define __EMBER_COUNTER_H__

/**
 * @brief Defines the events reported to the application
 * by the ::emberCounterHandler().
 */
#ifdef DOXYGEN_SHOULD_SKIP_THIS
enum EmberCounterType
#else
typedef uint8_t EmberCounterType;
enum
#endif
{
  /** Every packet that comes in over the radio (except mac acks). */
  EMBER_COUNTER_PHY_IN_PACKETS,

  /** Every packet that goes out over the radio (except mac acks). */
  EMBER_COUNTER_PHY_OUT_PACKETS,

  /** Every incoming byte, including the 802.15.4 length byte.
   * Note mac acks are not counted. */
  EMBER_COUNTER_PHY_IN_OCTETS,

  /** Every outgoing byte, including the 802.15.4 length byte.
   * Mac retries contribute to the count, but not mac acks. */
  EMBER_COUNTER_PHY_OUT_OCTETS,

  /** Incoming mac unicasts, post duplicate detection. */
  EMBER_COUNTER_MAC_IN_UNICAST,

  EMBER_COUNTER_MAC_IN_BROADCAST,

  /** Outgoing mac unicasts for which we received an ack, possibly
   * after retrying. */
  EMBER_COUNTER_MAC_OUT_UNICAST_SUCCESS,

  /** Outgoing unicasts for which we never received an ack even
   * after retrying. */
  EMBER_COUNTER_MAC_OUT_UNICAST_ACK_FAIL,

  /** Outgoing mac packets which were never transmitted because
   * clear channel assessment always returned busy. */
  EMBER_COUNTER_MAC_OUT_UNICAST_CCA_FAIL,

  /** Outgoing unicasts that failed even after extended mac retries. */
  EMBER_COUNTER_MAC_OUT_UNICAST_EXT_FAIL,

  /** Outgoing unicast retries.  This does not count the initial
   * transmission.  Note a single mac transmission can result in
   * multiple retries. */
  EMBER_COUNTER_MAC_OUT_UNICAST_RETRY,

  EMBER_COUNTER_MAC_OUT_BROADCAST,

  EMBER_COUNTER_MAC_OUT_BROADCAST_CCA_FAIL,

  /** Dropped incoming MAC packets (out of memory) */
  EMBER_COUNTER_MAC_DROP_IN_MEMORY,

  /** Dropped incoming MAC packets (no EUI) */
  EMBER_COUNTER_MAC_DROP_IN_NO_EUI,

  /** Dropped incoming MAC packets (invalid frame counter) */
  EMBER_COUNTER_MAC_DROP_IN_FRAME_COUNTER,

  /** Dropped incoming MAC packets (can't decrypt) */
  EMBER_COUNTER_MAC_DROP_IN_DECRYPT,

  /** Dropped incoming MAC packets (duplicate message) */
  EMBER_COUNTER_MAC_DROP_IN_DUPLICATE,

  /** IP packets */
  EMBER_COUNTER_IP_IN_UNICAST,
  EMBER_COUNTER_IP_OUT_UNICAST,
  EMBER_COUNTER_IP_IN_MULTICAST,
  EMBER_COUNTER_IP_OUT_MULTICAST,

  /** RPL-only messages */
  EMBER_COUNTER_RPL_OUT_DIS,
  EMBER_COUNTER_RPL_OUT_DIO,
  EMBER_COUNTER_RPL_OUT_DAO,
  EMBER_COUNTER_RPL_OUT_DAO_ACK,
  EMBER_COUNTER_RPL_RESET_DIO_TIMER,
  EMBER_COUNTER_RPL_START_DAO_TIMER,
  EMBER_COUNTER_RPL_POISON,

  /** MLE and RIP messages */
  EMBER_COUNTER_MLE_OUT_JOIN_REQUEST,
  EMBER_COUNTER_MLE_OUT_JOIN_ACCEPT,
  EMBER_COUNTER_MLE_OUT_REQUEST,
  EMBER_COUNTER_MLE_OUT_ACCEPT,
  EMBER_COUNTER_MLE_OUT_ACCEPT_AND_REQUEST,
  EMBER_COUNTER_MLE_OUT_REJECT,
  EMBER_COUNTER_MLE_OUT_ADVERTISEMENT,

  /** Application UDP messages.  Excludes DNS, PANA, MLE. */
  EMBER_COUNTER_UDP_IN,
  EMBER_COUNTER_UDP_OUT,

  /** UART in and out data */
  EMBER_COUNTER_UART_IN_DATA,
  EMBER_COUNTER_UART_IN_MANAGEMENT,
  EMBER_COUNTER_UART_IN_FAIL,
  EMBER_COUNTER_UART_OUT_DATA,
  EMBER_COUNTER_UART_OUT_MANAGEMENT,
  EMBER_COUNTER_UART_OUT_FAIL,

  // Counters for non-packet events below.

  EMBER_COUNTER_BUFFER_ALLOCATION_FAIL,

  /** ASHv3 */
  EMBER_ASH_V3_ACK_SENT,
  EMBER_ASH_V3_ACK_RECEIVED,
  EMBER_ASH_V3_NACK_SENT,
  EMBER_ASH_V3_NACK_RECEIVED,
  EMBER_ASH_V3_RESEND,
  EMBER_ASH_V3_BYTES_SENT,
  EMBER_ASH_V3_TOTAL_BYTES_RECEIVED,
  EMBER_ASH_V3_VALID_BYTES_RECEIVED,

  /** A placeholder giving the number of Ember counter types. */
  EMBER_COUNTER_TYPE_COUNT
};

/**
 * @ brief Defines the CLI enumerations for the ::EmberCounterType enum.
*/
#define EMBER_COUNTER_STRINGS                   \
  "PHY in",                                     \
    "PHY out",                                  \
    "PHY in bytes",                             \
    "PHY out bytes",                            \
    "MAC in uni",                               \
    "MAC in bro",                               \
    "MAC out uni succ",                         \
    "MAC out uni ack fail",                     \
    "MAC out uni cca fail",                     \
    "MAC out uni ext fail",                     \
    "MAC out uni retry",                        \
    "MAC out bro",                              \
    "MAC out bro cca fail",                     \
    "MAC drop in memory",                       \
    "MAC drop in no eui",                       \
    "MAC drop in frame counter",                \
    "MAC drop in decrypt",                      \
    "MAC drop in dup",                          \
    "IP in uni",                                \
    "IP out uni",                               \
    "IP in multi",                              \
    "IP out multi",                             \
    "RPL out dis",                              \
    "RPL out dio",                              \
    "RPL out dao",                              \
    "RPL out dao-ack",                          \
    "RPL reset dio",                            \
    "RPL reset dao",                            \
    "RPL poison",                               \
    "MLE out join req",                         \
    "MLE out join acc",                         \
    "MLE out req",                              \
    "MLE out acc",                              \
    "MLE out acc and req",                      \
    "MLE out rej",                              \
    "MLE out adv",                              \
    "UDP in",                                   \
    "UDP out",                                  \
    "UART in data",                             \
    "UART in mgmt",                             \
    "UART in fail",                             \
    "UART out data",                            \
    "UART out mgmt",                            \
    "UART out fail",                            \
    "BUFFER alloc fail",                        \
    "ASHv3 ack sent",                           \
    "ASHv3 ack received",                       \
    "ASHv3 nack sent",                          \
    "ASHv3 nack received",                      \
    "ASHv3 resend",                             \
    "ASHv3 bytes sent",                         \
    "ASHv3 total bytes received",               \
    "ASHv3 valid bytes received",               \
    NULL

#endif
