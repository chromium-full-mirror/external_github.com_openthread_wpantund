// File: coap.h
//
// Description: A CoAP server and client
//
// Copyright 2015 Silicon Laboratories, Inc.                                *80*

#ifndef __COAP_H__
#define __COAP_H__

// Continuation of public enum from stack/include/coap.h.
enum {
  EMBER_COAP_JOINER_ENTRUST              = BIT(10), // special commissioning message
  EMBER_COAP_COMMISSION_DATASET_ANNOUNCE = BIT(11), // special dataset message
  EMBER_COAP_USE_GP16_SOURCE             = BIT(12)
};

// This is RFC 7252's ACK_TIMEOUT * ACK_RANDOM_FACTOR = 2000ms * 1.5
#define COAP_ACK_TIMEOUT_MS    3000
#define COAP_MAX_RETRANSMIT    4

#define THREAD_COAP_PORT 61631

void emCoapMarkBuffers(void);
void emCoapInitialize(void);

bool emCoapSendResponse(const uint8_t *destination,
                           const uint8_t *data,
                           uint16_t length);
bool emCoapStartResponse(uint16_t length);

void emSendCoapRequest(const uint8_t *destination,
                       const uint8_t *option,
                       ...);

/**
 * @addtogroup coap
 * @verbatim
 The Coap Header, from RFC 7252
   0                   1                   2                   3
  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 |Ver| T |  TKL  |      Code     |        Message ID             |
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 |   Tokens (if any, TKL bytes) ...
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 |   Options (if any) ...
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 |1 1 1 1 1 1 1 1|   Payload (if any) ...
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

 The fields in the header are defined as follows:

   Version (Ver):  2-bit unsigned integer.  Indicates the CoAP version
      number.  Implementations of this specification MUST set this field
      to 1 (01 binary).  Other values are reserved for future versions.
      Messages with unknown version numbers MUST be silently ignored.

   Type (T):  2-bit unsigned integer.  Indicates if this message is of
      type Confirmable (0), Non-confirmable (1), Acknowledgement (2), or
      Reset (3).  The semantics of these message types are defined in
      Section 4.

   Token Length (TKL):  4-bit unsigned integer.  Indicates the length of
      the variable-length Token field (0-8 bytes).  Lengths 9-15 are
      reserved, MUST NOT be sent, and MUST be processed as a message
      format error.

   Code:  8-bit unsigned integer, split into a 3-bit class (most
      significant bits) and a 5-bit detail (least significant bits),
      documented as "c.dd" where "c" is a digit from 0 to 7 for the
      3-bit subfield and "dd" are two digits from 00 to 31 for the 5-bit
      subfield.  The class can indicate a request (0), a success
      response (2), a client error response (4), or a server error
      response (5).  (All other class values are reserved.)  As a
      special case, Code 0.00 indicates an Empty message.  In case of a
      request, the Code field indicates the Request Method; in case of a
      response, a Response Code.  Possible values are maintained in the
      CoAP Code Registries (Section 12.1).  The semantics of requests
      and responses are defined in Section 5.

   Message ID:  16-bit unsigned integer in network byte order.  Used to
      detect message duplication and to match messages of type
      Acknowledgement/Reset to messages of type Confirmable/Non-
      confirmable.  The rules for generating a Message ID and matching
      messages are defined in Section 4.

   Method codes:

    +------+--------+-----------+
    | Code | Name   | Reference |
    +------+--------+-----------+
    | 0.01 | GET    | [RFC7252] |
    | 0.02 | POST   | [RFC7252] |
    | 0.03 | PUT    | [RFC7252] |
    | 0.04 | DELETE | [RFC7252] |
    +------+--------+-----------+

   Response codes:

    +------+------------------------------+-----------+
    | Code | Description                  | Reference |
    +------+------------------------------+-----------+
    | 2.01 | Created                      | [RFC7252] |
    | 2.02 | Deleted                      | [RFC7252] |
    | 2.03 | Valid                        | [RFC7252] |
    | 2.04 | Changed                      | [RFC7252] |
    | 2.05 | Content                      | [RFC7252] |
    | 4.00 | Bad Request                  | [RFC7252] |
    | 4.01 | Unauthorized                 | [RFC7252] |
    | 4.02 | Bad Option                   | [RFC7252] |
    | 4.03 | Forbidden                    | [RFC7252] |
    | 4.04 | Not Found                    | [RFC7252] |
    | 4.05 | Method Not Allowed           | [RFC7252] |
    | 4.06 | Not Acceptable               | [RFC7252] |
    | 4.12 | Precondition Failed          | [RFC7252] |
    | 4.13 | Request Entity Too Large     | [RFC7252] |
    | 4.15 | Unsupported Content-Format   | [RFC7252] |
    | 5.00 | Internal Server Error        | [RFC7252] |
    | 5.01 | Not Implemented              | [RFC7252] |
    | 5.02 | Bad Gateway                  | [RFC7252] |
    | 5.03 | Service Unavailable          | [RFC7252] |
    | 5.04 | Gateway Timeout              | [RFC7252] |
    | 5.05 | Proxying Not Supported       | [RFC7252] |
    +------+------------------------------+-----------+
 @endverbatim
*/

#define COAP_VERSION      (1 << 6)
#define COAP_VERSION_MASK (3 << 6)
#define COAP_MAX_TOKEN_LENGTH 0x08
#define COAP_TOKEN_LENGTH_MASK 0x0F
#define COAP_TOKEN_INDEX 4

#define COAP_TYPE_MASK (3 << 4)
#define COAP_TYPE_SHIFT_AMOUNT 4

#define COAP_OPTION_COUNT_MASK 0x0F

#define COAP_TRANSACTION_ID_INDEX 2
#define COAP_TRANSACTION_ID_LENGTH 2

#define COAP_BASE_SIZE 4
#define MAX_REQUEST_SIZE COAP_BASE_SIZE + 20
#define MAX_RESPONSE_SIZE MAX_REQUEST_SIZE
#define MAX_URI_PATH_SIZE 14

// Option Deltas
//
//   for 0..14
//
//     0   1   2   3   4   5   6   7
//   +---+---+---+---+---+---+---+---+
//   | option delta  |    length     |
//   +---+---+---+---+---+---+---+---+
//
//   for 15..269:
//   +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
//   | Option Delta  | 1   1   1   1 |          Length - 15          |
//   +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
//   |   Option Value ...
//   +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
//
//   for 270..524:
//   +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
//   | Option Delta  | 1   1   1   1 | 1   1   1   1   1   1   1   1 |
//   +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
//   |         Length - 270          |   Option Value ...
//   +---+---+---+---+---+---+---+---+
//
//   for 525..779:
//   +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
//   | Option Delta  | 1   1   1   1 | 1   1   1   1   1   1   1   1 |
//   +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
//   | 1   1   1   1   1   1   1   1 |        Length - 525           |
//   +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
//   |   Option Value ...
//   +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
//
//   for 780..1034:
//   +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
//   | Option Delta  | 1   1   1   1 | 1   1   1   1   1   1   1   1 |
//   +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
//   | 1   1   1   1   1   1   1   1 | 1   1   1   1   1   1   1   1 |
//   +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
//   |        Length - 780           |       Option Value ...
//   +---+---+---+---+---+---+---+---+

#define COAP_OPTION_LENGTH 1
#define COAP_OPTION_DELTA_MASK 0xF0
#define COAP_OPTION_LENGTH_MASK 0x0F

#define COAP_OPTION_ETAG_LENGTH 2

#ifdef EMBER_TEST
  extern bool emCoapHistoryCheck;
#endif

typedef enum {
  COAP_NO_FAILURE                      = BIT(0),
  COAP_FAILURE_UNKNOWN_CRITICAL_OPTION = BIT(1),
  COAP_FAILURE_BAD_OPTIONS_LENGTH      = BIT(2),
  COAP_FAILURE_BLOCK_ERROR             = BIT(3),
  COAP_FAILURE_OUT_OF_MEMORY_ERROR     = BIT(4),
  COAP_FAILURE_BAD_REQUEST             = BIT(5)
} CoapFailureCode;

// If 'uri' is NULL, then message->uriPath is used as the encoded URI.  If
// that is also NULL, then there is no URI.

EmberStatus emSubmitCoapMessage(CoapMessage *message,
                                const uint8_t *uri,
                                Buffer payloadBuffer,
                                EmberCoapMessageOptions options);

// Stack message handler, returns true if this was a stack message,
// otherwise returns false and the application message handler is
// called.
bool emCoapMessageHandler(CoapMessage *coap);

const uint8_t *emGetCoapCodeName(EmberCoapCode type);
const uint8_t *emGetCoapTypeName(CoapType type);

EmberStatus emReallySubmitCoapMessage(Buffer message,
                                      EmberCoapMessageOptions options);

typedef void (CoapMessageHandler)(CoapMessage *coapMessage);
void emProcessCoapMessage(CoapMessage *coapMessage,
                          CoapMessageHandler *messageHandler);

bool emParseCoapMessage(const uint8_t *data,
                        uint16_t dataLength,
                        CoapMessage *coapMessage);

bool emAttemptMessageIdMatchAndDelete(const CoapMessage *message);

extern Buffer emUnackedCoapMessages;

bool emCoapRespondWithUri(EmberCoapCode responseCode,
                          const uint8_t *payload,
                          uint16_t payloadLength);

bool emCoapReallyRespondWithUri(EmberCoapCode responseCode,
                                const uint8_t *payload,
                                uint16_t payloadLength,
                                const uint8_t *decodedUri);

//
// Unacked Messages
//

extern EventActions emCoapRetryEventActions;
void emAddUnackedCoapMessage(CoapRetryEvent *event, CoapMessage *message);
void emNoteCoapAck(uint16_t messageId);

extern uint16_t emStackCoapPort;

#endif
