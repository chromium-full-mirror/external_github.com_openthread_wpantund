/**
 * @file coap.h
 * @brief Constrained Application Protocol (CoAP) API.
 *
 * <!--Copyright 2015 Silicon Laboratories, Inc.                         *80*-->
 */

typedef enum {
  EMBER_NO_COAP_OPTIONS      = 0,
  EMBER_COAP_SECURED         = BIT(0),
  EMBER_COAP_NON             = BIT(1)
} EmberCoapOptions_e;

// Use an integer type instead of an enum because some C compilers object to
// bit operations on enum types.
typedef uint16_t EmberCoapMessageOptions;

#define MAKE_COAP_CODE(class, detail) ((class << 5) | detail)
#define GET_COAP_CLASS(code) (((code) & 0xE0) >> 5)
#define GET_COAP_DETAIL(code) ((code) & 0x1F)

typedef enum {
  EMBER_COAP_CLASS_REQUEST               = 0,
  EMBER_COAP_CLASS_SUCCESS_RESPONSE      = 2,
  EMBER_COAP_CLASS_CLIENT_ERROR_RESPONSE = 4,
  EMBER_COAP_CLASS_SERVER_ERROR_RESPONSE = 5,
} EmberCoapClass;

typedef enum {
  // empty messages
  EMBER_COAP_CODE_EMPTY                          = MAKE_COAP_CODE(0,  0),

  // requests
  EMBER_COAP_CODE_GET                            = MAKE_COAP_CODE(0,  1),
  EMBER_COAP_CODE_POST                           = MAKE_COAP_CODE(0,  2),
  EMBER_COAP_CODE_PUT                            = MAKE_COAP_CODE(0,  3),
  EMBER_COAP_CODE_DELETE                         = MAKE_COAP_CODE(0,  4),

  // responses
  EMBER_COAP_CODE_201_CREATED                    = MAKE_COAP_CODE(2,  1),
  EMBER_COAP_CODE_202_DELETED                    = MAKE_COAP_CODE(2,  2),
  EMBER_COAP_CODE_203_VALID                      = MAKE_COAP_CODE(2,  3),
  EMBER_COAP_CODE_204_CHANGED                    = MAKE_COAP_CODE(2,  4),
  EMBER_COAP_CODE_205_CONTENT                    = MAKE_COAP_CODE(2,  5),
  EMBER_COAP_CODE_400_BAD_REQUEST                = MAKE_COAP_CODE(4,  0),
  EMBER_COAP_CODE_401_UNAUTHORIZED               = MAKE_COAP_CODE(4,  1),
  EMBER_COAP_CODE_402_BAD_OPTION                 = MAKE_COAP_CODE(4,  2),
  EMBER_COAP_CODE_403_FORBIDDEN                  = MAKE_COAP_CODE(4,  3),
  EMBER_COAP_CODE_404_NOT_FOUND                  = MAKE_COAP_CODE(4,  4),
  EMBER_COAP_CODE_405_METHOD_NOT_ALLOWED         = MAKE_COAP_CODE(4,  5),
  EMBER_COAP_CODE_406_NOT_ACCEPTABLE             = MAKE_COAP_CODE(4,  6),
  EMBER_COAP_CODE_412_PRECONDITION_FAILED        = MAKE_COAP_CODE(4, 12),
  EMBER_COAP_CODE_413_REQUEST_ENTITY_TOO_LARGE   = MAKE_COAP_CODE(4, 13),
  EMBER_COAP_CODE_415_UNSUPPORTED_CONTENT_FORMAT = MAKE_COAP_CODE(4, 15),
  EMBER_COAP_CODE_500_INTERNAL_SERVER_ERROR      = MAKE_COAP_CODE(5,  0),
  EMBER_COAP_CODE_501_NOT_IMPLEMENTED            = MAKE_COAP_CODE(5,  1),
  EMBER_COAP_CODE_502_BAD_GATEWAY                = MAKE_COAP_CODE(5,  2),
  EMBER_COAP_CODE_503_SERVICE_UNAVAILABLE        = MAKE_COAP_CODE(5,  3),
  EMBER_COAP_CODE_504_GATEWAY_TIMEOUT            = MAKE_COAP_CODE(5,  4),
  EMBER_COAP_CODE_505_PROXYING_NOT_SUPPORTED     = MAKE_COAP_CODE(5,  5),
} EmberCoapCode;

typedef enum {
  EMBER_COAP_INVALID_STATUS    = 0,
  EMBER_COAP_MESSAGE_TIMED_OUT = 1,
  EMBER_COAP_MESSAGE_ACKED     = 2,
  EMBER_COAP_MESSAGE_RESET     = 3,
  EMBER_COAP_MESSAGE_RESPONSE  = 4
} EmberCoapStatus;

typedef enum {
  EMBER_COAP_NO_OPTION             = 0,
  EMBER_COAP_OPTION_CONTENT_TYPE   = 1,
  EMBER_COAP_OPTION_URI_HOST       = 3,
  EMBER_COAP_OPTION_ETAG           = 4,
  EMBER_COAP_OPTION_IF_NONE_MATCH  = 5,
  EMBER_COAP_OPTION_OBSERVE        = 6,
  EMBER_COAP_OPTION_URI_PORT       = 7,
  EMBER_COAP_OPTION_LOCATION_PATH  = 8,
  EMBER_COAP_OPTION_URI_PATH       = 11,
  EMBER_COAP_OPTION_CONTENT_FORMAT = 12,
  EMBER_COAP_OPTION_MAX_AGE        = 14,
  EMBER_COAP_OPTION_URI_QUERY      = 15,
  EMBER_COAP_OPTION_ACCEPT         = 17,
  EMBER_COAP_OPTION_LOCATION_QUERY = 20,
  EMBER_COAP_OPTION_PROXY_URI      = 35,
  EMBER_COAP_OPTION_PROXY_SCHEME   = 39,
  EMBER_COAP_OPTION_SIZE1          = 60
} EmberCoapOptionType;

typedef enum {
  EMBER_COAP_CONTENT_FORMAT_TEXT_PLAIN   =  0,
  EMBER_COAP_CONTENT_FORMAT_LINK_FORMAT  = 40,
  EMBER_COAP_CONTENT_FORMAT_XML          = 41,
  EMBER_COAP_CONTENT_FORMAT_OCTET_STREAM = 42,
  EMBER_COAP_CONTENT_FORMAT_EXI          = 47,
  EMBER_COAP_CONTENT_FORMAT_JSON         = 50,
  EMBER_COAP_CONTENT_FORMAT_CBOR         = 60,
  EMBER_COAP_CONTENT_FORMAT_NONE         = -1   // no content format option
} EmberCoapContentFormatType;

#define EMBER_COAP_PORT        5683
#define EMBER_COAP_SECURE_PORT 5684

#define EMBER_COAP_MAX_TOKEN_LENGTH 8

// From RFC 7252
#define EMBER_COAP_DEFAULT_TIMEOUT_MS 90000

typedef struct {
  // IP addressing
  EmberIpv6Address localAddress;
  EmberIpv6Address remoteAddress;
  uint16_t localPort;
  uint16_t remotePort;

  // CoAP header
  EmberCoapCode code;
  uint8_t token[EMBER_COAP_MAX_TOKEN_LENGTH];
  uint8_t tokenLength;
  const uint8_t *encodedOptions;
  uint16_t encodedOptionsLength;
  const uint8_t *encodedUri;
  uint16_t encodedUriLength;
  EmberCoapContentFormatType contentFormat;
  bool contentFormatExists;

  // Payload
  const uint8_t *payload;
  uint16_t payloadLength;

  // Message options
  EmberCoapMessageOptions options;
} EmberCoapMessage;

// If 'status' is EMBER_COAP_MESSAGE_RESPONSE then 'message' is the response.
// If 'status' is EMBER_COAP_MESSAGE_ACKED then a response may arrive later.
typedef void (*EmberCoapResponseHandler)(EmberCoapStatus status,
                                         EmberCoapMessage *message,
                                         void *applicationData,
                                         uint16_t applicationDataLength);

// If responseHandler is non-NULL, a value of zero for responseTimeoutMs is
// treated as EMBER_COAP_DEFAULT_TIMEOUT_MS.
EmberStatus emberCoapSend(EmberCoapMessage *message,
                          const uint8_t *uri,
                          uint32_t responseTimeoutMs,
                          EmberCoapResponseHandler responseHandler,
                          void *applicationData,
                          uint16_t applicationDataLength);

#define emberCoapSendNonconfirmable(message) \
  (emberCoapSend((message), 0, NULL, NULL, 0))

#define emberCoapSendConfirmable(message, responseHandler) \
  (emberCoapSend((message), 0, (responseHandler), NULL, 0))

// Utility function for the simple case.

EmberStatus emberCoapSendUri(EmberCoapCode code,
                             const EmberIpv6Address *destination,
                             const uint8_t *uri,
                             const uint8_t *body,
                             uint16_t bodyLength,
                             EmberCoapResponseHandler responseHandler);

#define emberCoapGet(destination, uri, handler) \
 (emberCoapSendUri(EMBER_COAP_CODE_GET, (destination), (uri), NULL, 0, \
                   (handler)))

#define emberCoapPost(destination, uri, body, bodyLength, handler) \
 (emberCoapSendUri(EMBER_COAP_CODE_POST, (destination), (uri), (body), \
                   (bodyLength), (handler)))

#define emberCoapPostNonconfirmable(destination, uri, body, bodyLength) \
 (emberCoapSendUri(EMBER_COAP_CODE_POST, (destination), (uri), (body), \
                   (bodyLength), NULL))

#define emberCoapPut(destination, uri, body, bodyLength, handler) \
 (emberCoapSendUri(EMBER_COAP_CODE_PUT, (destination), (uri), (body), \
                   (bodyLength), (handler)))

#define emberCoapDelete(destination, uri, handler) \
 (emberCoapSendUri(EMBER_COAP_CODE_DELETE, (destination), (uri), NULL, 0, \
                   (handler)))

void emberCoapMessageHandler(EmberCoapMessage *message);

/**
 * @brief Call this function to respond a CoAP message.
 * If responseCode == EMBER_COAP_INVALID_CODE, then the default response code
 * is used.
 */
bool emberCoapRespond(EmberCoapCode responseCode,
                      const uint8_t *payload,
                      uint16_t payloadLength);

/**
 * @brief Call this function to send a 204 Changed response.
 */
bool emberCoapRespondChanged(EmberCoapContentFormatType format,
                             const uint8_t *payload,
                             uint16_t payloadLength);

/**
 * @brief Call this function to send a 205 Content response.
 */
bool emberCoapRespondContent(EmberCoapContentFormatType format,
                             const uint8_t *payload,
                             uint16_t payloadLength);

/**
 * @brief Call this function to send a 201 Created response.
 */
bool emberCoapRespondCreated(const uint8_t *locationPath);

/**
 * @brief This function converts a string URI into CoAP options.  The
 * length of the options is returned.
 */
uint16_t emberCoapEncodeUri(const uint8_t *uri,
                            uint8_t *buffer,
                            uint16_t bufferLength,
                            EmberCoapOptionType previousOption);

/**
 * @brief This function converts CoAP options into a string URI.  The buffer
 * for the decoded URI must be large enough to hold the URI and a null
 * terminator.
 */
bool emberCoapDecodeUri(const uint8_t *encodedUri,
                        uint16_t encodedUriLength,
                        uint8_t *uri,
                        uint16_t uriLength);

// Temporary patch for renaming.
#define emberCoapConvertUri emberCoapDecodeUri

/**
 * @brief This function returns the length of the decoded URI, including a null
 * terminator.
 */
uint16_t emberCoapDecodedUriLength(const uint8_t *rawUri,
                                   uint16_t rawUriLength);

// Writes the encoded options into 'buffer', returning the number of
// bytes written.  If non-NULL, uriPath and locationPath are inserted
// put in their correct place in the encoded options.
// 'otherOptions' are options encoded as bytes
//  <type> <size> <data> ...
// with the last <type> being zero.  The types must be in increasing
// order (except for the final zero).
uint16_t emberCoapWriteOptions(uint8_t *buffer,
                               uint16_t bufferLength,
                               const uint8_t *uriPath,
                               const uint8_t *locationPath,
                               const uint8_t *otherOptions);

/** @brief Indicates if the code represents a request. */
bool emberCoapIsRequest(EmberCoapCode code);

/** @brief Indicates if the code represents a response. */
bool emberCoapIsResponse(EmberCoapCode code);

#ifdef DOXYGEN_SHOULD_SKIP_THIS
  /** @brief Indicates if the code represents a success response. */
  bool emberCoapIsSuccessResponse(EmberCoapCode code);
  /** @brief Indicates if the code represents a client error response. */
  bool emberCoapIsClientErrorResponse(EmberCoapCode code);
  /** @brief Indicates if the code represents a server error response. */
  bool emberCoapIsServerErrorResponse(EmberCoapCode code);
#else
  #define emberCoapIsSuccessResponse(code) \
    (GET_COAP_CLASS(code) == EMBER_COAP_CLASS_SUCCESS_RESPONSE)
  #define emberCoapIsClientErrorResponse(code) \
    (GET_COAP_CLASS(code) == EMBER_COAP_CLASS_CLIENT_ERROR_RESPONSE)
  #define emberCoapIsServerErrorResponse(code) \
    (GET_COAP_CLASS(code) == EMBER_COAP_CLASS_SERVER_ERROR_RESPONSE)
#endif

typedef enum {
  COAP_TYPE_CONFIRMABLE     = 0,
  COAP_TYPE_NON_CONFIRMABLE = 1,
  COAP_TYPE_ACK             = 2,
  COAP_TYPE_RESET           = 3
} CoapType;

typedef struct {
  EmberCoapMessage publicMessage;

  CoapType type;
  uint16_t messageId;             // matched against incoming acks
  uint8_t dtlsHandle;
  EmberCoapResponseHandler responseHandler;
  uint32_t responseTimeoutMs;
  const uint8_t *responseAppData;
  uint16_t responseAppDataLength;
  Buffer message;       // incoming message buffer (for potential reuse)
} CoapMessage;

typedef struct {
  EmberCoapOptionType type;
  uint16_t length;
  const uint8_t *bytes;
} CoapOption;

// Returns a pointer to the option data and initializes the message for
// walking through the following options.
uint16_t emberReadCoapOptions(const EmberCoapMessage *message,
                              EmberCoapOptionType * wantedTypes,
                              uint8_t wantedTypesCount,
                              CoapOption *options,
                              uint16_t maxOptions);

typedef struct {
  Event event;
  uint8_t retryCount;           // number of retries so far
  uint16_t messageId;           // matched against incoming acks
  EmberCoapMessageOptions options; // for EMBER_COAP_NOTIFY_ACK and tx options
  Buffer packetHeader;          // for resending
  uint16_t token;               // for matching requests and responses
  uint32_t responseTimeoutMs;
  EmberCoapResponseHandler handler;
  uint16_t responseAppDataLength;
  Buffer retryEncodedUri;         // Only used when (EM_LOG_COAP_ENABLED == true)
  EmberIpv6Address remoteAddress; // Host specific
  uint16_t remotePort;            // Host specific
  uint8_t dtlsHandle;             // Stack specific
} CoapRetryEvent;

EmberStatus emberFinishCoapMessage(CoapMessage *coapMessage,
                                   uint8_t *coapHeader,
                                   uint16_t coapHeaderLength,
                                   Buffer payloadBuffer,
                                   EmberCoapMessageOptions options);

EmberStatus emberRetryCoapMessage(CoapRetryEvent *message);
