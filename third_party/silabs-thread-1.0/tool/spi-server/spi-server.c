// File: spi-server.c
//
// Description: Expose SPI transport over an IPv6 stream socket
//
// Copyright 2012 by Silicon Labs. All rights reserved.                     *80*

#define PROGNAME "spi-server"

#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <syslog.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <getopt.h>
#include <stdio.h>
#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/file.h>
#include <time.h>
#include <netinet/in.h>
#include <net/if.h>
#include <poll.h>
#include <signal.h>
#include <linux/types.h>
#include <linux/spi/spidev.h>

typedef unsigned char  boolean;
typedef unsigned char  int8u;
typedef signed   char  int8s;
typedef unsigned short int16u;
typedef signed   short int16s;
typedef unsigned int   int32u;
typedef signed   int   int32s;

#define VALID_FD_ISSET(fd, set) (((fd) >= 0) && (FD_ISSET((fd), (set))))

#define STRINGIFY(x) #x
#define STRINGIFYX(x) STRINGIFY(x)

#define TRUE  1
#define FALSE 0
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define elapsedTimeInt32u(oldTime, newTime) \
  ((int32u) ((int32u)(newTime) - (int32u)(oldTime)))
#define timeGTorEqualInt32u(t1, t2) \
  (elapsedTimeInt32u(t2, t1) <= (int32u) 0x80000000)
#define MAX_DATE_TIME_STR_LEN 64

typedef struct {
  void (*handler)(void);
  int32u timeToExecute;
  boolean active;
} EventControl;

static int32u getInt32uMillisecondTick(void);
static void setTimeout(EventControl *event, int32u delay);
static void checkEvent(EventControl *event);

#define DEV_STRING_MAX   256
#define GPIO_STRING_MAX  256
static char wakeGpioArg[DEV_STRING_MAX+1];  //FIXME: Currently not used

static int logging = 1;
static boolean logEnabled = TRUE;
static int ncpSpiFd = -1;
static int ncpIntFd = -1;
static int ncpResetFd = -1;
static int clientInFd = -1;
static int clientOutFd = -1;
static int listenFd = -1;
static boolean multiClient = FALSE;
static boolean additionalTimestamp = FALSE;

#define ERROR_DELAY 1000
static void initHandler(void);
static void errorHandler(void);
static EventControl rxErrorEvent = { &errorHandler, 0, 0 };
static EventControl initEvent = { &initHandler, 0, 0 };

#define TIMEOUT_LONG_US 100000
#define TIMEOUT_SHORT_US 10000
#define TIMEOUT_INTERTRANSACTION_US 1000
#define TIMEOUT_RESET_US 1000

#ifndef SPI_SERVER_WATCHDOG_TIMEOUT
#define SPI_SERVER_WATCHDOG_TIMEOUT       5 // In seconds
#endif

static int32u traceMask = 0;
typedef enum {
  LOG_CLIENT_RX,
  LOG_CLIENT_TX,
  LOG_SPI_RX,
  LOG_SPI_TX,
  LOG_SPI_LOGIC,
  LOG_MAX_TYPE  // Must be last
} LogEvent;

typedef enum {
  DIR_INPUT,
  DIR_OUTPUT
} GpioDir;

static const char * const logEventNames[] = {
  "CLI-RX",
  "CLI-TX",
  "SPI-RX",
  "SPI-TX",
  "SPI-LOG",
};

static void getLogTimestamp(char *datetime);
static void logEvent(LogEvent type, char *subType, int8u *data, int16u length);
static void logStatus(char *format, ...);
static void logError(char *format, ...);
static void logWarning(char *format, ...);

static void cleanupAndExit(int exitCode);

//--------------------------------------------------------------------------
/* NCP SPI Protocol
   - NCP and Host flow control supported via in-transaction NAK mechanism
   - Simple one byte command (bitmask):
     +-7-+-6-+-5-+-4-+-3-+-2-+-1-+-0-+
     # 0 | 0 | 1 |pay#rst|nak|mbz|dir#
     +---+---+---+---+---+---+---+---+
     0x80 = 0 \
     0x40 = 0  > These avoid any confusion with PP bytes of 00 or FF
     0x20 = 1 /
     0x10 = payload bit (1=next byte is <len> length of payload in bytes
                         followed by <pay> payload of <len> bytes,
                         which if truncated results in no <pay> data accepted)
     0x08 = reset bit - set in 1st transaction after a reboot
     0x04 = nak (1=NAK - data received from peer is currently ignored
     0x02 = mbz (must be zero -- for future definition)
     0x01 = direction bit (0=host-to-NCP 1=NCP-to-host)
     For simplicity, payload bit can always be set to guarantee <len> byte is
     present in which <len> of 0x00 indicates presence of <pay>.
     SPI packet format therefore is:
     <cmd> [<len> <pay...>]
   - <len> can range from 0 to 254 (0x00 to 0xFE).
     Value of 0xFF is reserved.
*/
#define NCP_SPI_PKT_OVERHEAD      2   // <cmd> <len>
#define NCP_SPI_MAX_PAYLOAD       254 // Max payload length per SPI transaction
#define NCP_SPI_RX_SLOP           5   // Rx tolerance of initial pad bytes
#define NCP_SPI_BUFSIZE           ( NCP_SPI_RX_SLOP \
                                  + NCP_SPI_PKT_OVERHEAD \
                                  + NCP_SPI_MAX_PAYLOAD )

#define NCP_SPI_PKT_CMD_IDX       0
#define NCP_SPI_PKT_LEN_IDX       1
#define NCP_SPI_PKT_PAY_IDX       2

#define NCP_SPI_PKT_CMD_PAT_MASK  0xE0
#define NCP_SPI_PKT_CMD_PAT       0x20  // Pattern for all commands
#define NCP_SPI_PKT_CMD_PAY       0x10  // Payload (length) is present
#define NCP_SPI_PKT_CMD_RST       0x08  // Reset happened
#define NCP_SPI_PKT_CMD_NAK       0x04  // NAK - can't accept data now
#define NCP_SPI_PKT_CMD_RSP       0x01  // Direction is response (NCP-to-Host)

#define NCP_SPI_LEN_ERROR         0xFF  // <len> value that indicates error

#define NCP_SPI_MODE              0
#define NCP_SPI_SPEED_HZ          (1024 * 1024) // 1 MHz
#define NCP_SPI_BITS_PER_WORD     8
#define NCP_SPI_DELAY_BEG_US      20 // Delay from NSEL assert to 1st SCLK
#define NCP_SPI_DELAY_END_US      0  // Delay after last SSLK to NSEL deassert

#define NCP_EM3XX_RX_PAD          0xFF
#define NCP_HOST_TX_PAD           0x00

static int8u ncpRxBuffer[NCP_SPI_BUFSIZE];
static int8u ncpTxBuffer[NCP_SPI_BUFSIZE] = { NCP_SPI_PKT_CMD_PAT
                                            | NCP_SPI_PKT_CMD_PAY
                                            | 0 // Always ACK NCP
                                            | NCP_SPI_PKT_CMD_RST,
                                              0x00, // <cmd>
                                              0x00, // <len>
                                            };
static int16s ncpRxAvail = 0;
static int16s ncpTxAvail = 0;

//------------------------------------------------------------------------------
// Serial connection to NCP.

static void interTransactionDelay(void)
{
  struct timeval timeout;
  timeout.tv_sec = 0;
  timeout.tv_usec = TIMEOUT_INTERTRANSACTION_US;
  select(0, NULL, NULL, NULL, &timeout);
}

static int openGpioDevice(const char *gpioDevice, const char *displayName, GpioDir dir)
{
  int fd = -1;
  if (gpioDevice && *gpioDevice) {
    char gpioDevicePath[GPIO_STRING_MAX+1] = "/sys/class/gpio/";
    if (gpioDevice[0] == '/') { // Full path on command line overrides
      strncpy(gpioDevicePath, gpioDevice, sizeof(gpioDevicePath)-1);
    } else {
      strncat(gpioDevicePath, gpioDevice, sizeof(gpioDevicePath)-strlen(gpioDevicePath)-1);
      strncat(gpioDevicePath, "/value", sizeof(gpioDevicePath)-strlen(gpioDevicePath)-1);
    }
    fd = open(gpioDevicePath, (dir == DIR_INPUT ? O_RDONLY : O_WRONLY));
    if (fd < 0) {
      logStatus("Cannot open %s device %s.", displayName, gpioDevice);
      perror("gpio open failed");
      exit(1);
    }
    logStatus("Opened %s device %s.", displayName, gpioDevice);
  }
  return fd;
}

static void openNcpReset(const char *resetDevice)
{
  ncpResetFd = openGpioDevice(resetDevice, "nRESET", DIR_OUTPUT);
}

static void openNcpSpiSerial(const char *spiDevice, const char *intDevice)
{
  int status;
  ncpSpiFd = open(spiDevice, O_RDWR);
  if (ncpSpiFd < 0) {
    logError("Cannot open SPI device %s.", spiDevice);
    perror("spi open failed");
    exit(1);
  }

  // Lock the file descriptor
  if (flock(ncpSpiFd, LOCK_EX|LOCK_NB) < 0) {
    // The only error we care about is EWOULDBLOCK. EINVAL is fine,
    // it just means this file descriptor doesn't support locking.
    if (EWOULDBLOCK == errno) {
      perror("flock");
      logError("SPI device is locked by another process");
      exit(1);
    }
  }

  logStatus("Opened SPI device %s.", spiDevice);
  ncpIntFd = openGpioDevice(intDevice, "nHOST_INT", DIR_INPUT);

  // SPI mode
  int8u mode = NCP_SPI_MODE;
  status = ioctl(ncpSpiFd, SPI_IOC_WR_MODE, &mode);
  assert(status >= 0);
  status = ioctl(ncpSpiFd, SPI_IOC_RD_MODE, &mode);
  assert(status >= 0);
  logEvent(LOG_SPI_LOGIC, "SPI-Mode", (int8u*)&mode, sizeof(mode));
  int8u bits = NCP_SPI_BITS_PER_WORD;
  status = ioctl(ncpSpiFd, SPI_IOC_WR_BITS_PER_WORD, &bits);
  assert(status >= 0);
  status = ioctl(ncpSpiFd, SPI_IOC_RD_BITS_PER_WORD, &bits);
  assert(status >= 0);
  logEvent(LOG_SPI_LOGIC, "SPI-Bits", (int8u*)&bits, sizeof(bits));
  int32u speed = NCP_SPI_SPEED_HZ;
  status = ioctl(ncpSpiFd, SPI_IOC_WR_MAX_SPEED_HZ, &speed);
  assert(status >= 0);
  status = ioctl(ncpSpiFd, SPI_IOC_RD_MAX_SPEED_HZ, &speed);
  assert(status >= 0);
  logEvent(LOG_SPI_LOGIC, "SPI-Speed", (int8u*)&speed, sizeof(speed));
}

static boolean ncpHostIntAsserted(void)
{
  assert(ncpIntFd >= 0);

  int8u gpioPinState = '1'; // Assume deasserted, is low-true signal
  (void) lseek(ncpIntFd, 0, SEEK_SET);
  (void) read(ncpIntFd, &gpioPinState, 1);
  logEvent(LOG_SPI_LOGIC, "INT=", &gpioPinState, sizeof(gpioPinState));
  return(gpioPinState == '0');
}

static void resetNcp(void)
{
  int attemptsLeft = 2;
  while (attemptsLeft--) {
    int maxFd = -1;
    int ret = 0;
    int8u gpioPinState;
    fd_set except;
    FD_ZERO(&except);

    assert(ncpResetFd >= 0);

    logStatus("Resetting NCP");

    // set nRESET low
    gpioPinState = '0';
    (void) write(ncpResetFd, &gpioPinState, 1);

    // delay for a bit
    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = TIMEOUT_RESET_US;
    ret = select(0, NULL, NULL, NULL, &timeout);

    if (ret < 0) {
      // We were interrupted. Stop.
      logWarning("NCP reset interrupted.");
      break;
    }

    if (ncpIntFd >= 0) {
      ncpHostIntAsserted();
      FD_SET(ncpIntFd, &except);
      maxFd = ncpIntFd;
    }

    // set nRESET high and leave it that way
    gpioPinState = '1';
    (void) write(ncpResetFd, &gpioPinState, 1);

    // delay for a bit more
    timeout.tv_sec = 0;
    timeout.tv_usec = 1000*500; // Half-second timeout
    ret = select(maxFd+1, NULL, NULL, &except, &timeout);

    if (ret < 0) {
      // We were interrupted. Stop.
      logWarning("NCP reset interrupted.");
      break;
    }

    if ((ncpIntFd >= 0) && (ret == 0)) {
      logWarning("NCP reset attempted, but didn't get expected interrupt in time.");
    } else {
      logStatus("NCP was reset successfully.");
      break;
    }
  }
}

/** @brief parses one SPI response from the NCP
 * @param buf    buffer containing response (must not be NULL)
 * @param trLen  Number of bytes in buffer (must be > 0)
 * @param pAcked Set true if Tx bytes sent were consumed by NCP
 * @return       Number of buf bytes parsed (consumed)
 */
static int16s spiParseRsp(int8u *buf, int16s trLen, boolean *pAcked)
{
  assert((buf != NULL) && (trLen > 0));
  int8u *trBuf = buf;
  int8u cmd = *trBuf++;
  trLen--;
  if ( (cmd & (NCP_SPI_PKT_CMD_PAT_MASK | NCP_SPI_PKT_CMD_RSP))
     !=       (NCP_SPI_PKT_CMD_PAT      | NCP_SPI_PKT_CMD_RSP) ) {
    //FIXME: What to do with junk from peer?
    return -1; // for now just ignore this byte; negate signals error to caller
  }

  // Tell caller whether NCP agreed to accept the payload we just sent
  if (pAcked != NULL) {
    *pAcked = !(cmd & NCP_SPI_PKT_CMD_NAK);
  }

  if (cmd & NCP_SPI_PKT_CMD_RST) { // NCP has reset
    //FIXME: take action on NCP reset?
    // Hopefully higher-level protocols have own mechanism for this
    // At SPI protocol level, things are pretty stateless so nothing
    // much we need to do in this situation.
  }

  if ( (cmd & NCP_SPI_PKT_CMD_PAY) // response claimed to have len for payload
     &&(trLen > 0) ) {             // response actually has len for payload
    ncpRxAvail = *trBuf++;         // amount of payload data NCP had to send
    trLen--;
    if (ncpRxAvail == NCP_SPI_LEN_ERROR) {
      //FIXME: Deal with ncpRxAvail == NCP_SPI_LEN_ERROR -- support it?
      ncpRxAvail = 0;
    }
    int8u rxLen = MIN(trLen, ncpRxAvail); // amount actually received

    // Forward newly received NCP data out to client
    if (rxLen > 0) {
      logEvent(LOG_CLIENT_TX, NULL, trBuf, rxLen);
      if (clientOutFd >= 0) {
        int flags = fcntl(clientOutFd, F_GETFL);

        if ((flags >= 0) && (flags & O_NONBLOCK)) {
          // Turn off O_NONBLOCK, because we want writes to the client block
          fcntl(clientOutFd, F_SETFL, flags & ~O_NONBLOCK);
        }

        alarm(0); // Turn off our watchdog.
        int writeLen = write(clientOutFd, trBuf, rxLen);
        assert(writeLen == rxLen);
        alarm(SPI_SERVER_WATCHDOG_TIMEOUT); // Turn our watchdog back on.

        if ((flags >= 0) && (flags & O_NONBLOCK)) {
          // Restore the flags
          fcntl(clientOutFd, F_SETFL, flags);
        }
      }
      ncpRxAvail -= rxLen; // update amount still to be retrieved from NCP
      trBuf += rxLen;
      trLen -= rxLen;
    }
  } else { // No payload
    // No new data available
    ncpRxAvail = 0;
  }

  return (trBuf - buf);
}

/** @brief parses all SPI response data from the NCP
 * @param len  Number of bytes in ncpRxbuffer[] (could be 0)
 * @return     TRUE if Tx bytes sent were consumed by NCP
 */
static boolean spiParseRx(int16s len)
{
  int16s i = 0;
  boolean acked = FALSE;
  int8u errors = 0;
  while ( (i < len) && (errors <= 1) ) { // Only tolerate one error per Rx
    if (ncpRxBuffer[i] == NCP_EM3XX_RX_PAD) {
      i++;
    } else {
      // Looks like we have a command byte, parse the response
      int16s consumed = spiParseRsp(&ncpRxBuffer[i], len - i, &acked);
      if (consumed < 0) { // Parse error signalled, consumed is negated
        errors += 1;
        consumed = -consumed;
        logEvent(LOG_SPI_LOGIC, "BadRx=", &ncpRxBuffer[i], consumed);
        i += consumed;
      } else {
        // don't continue trying to parse because the NCP will include garbage
        // after the packet which must be ignored
        break;
      }
    }
  }
  return acked;
}

static boolean spiTransact(int16s pktLen)
{
  #define SPI_IOC_TRANSFERS 2
  struct spi_ioc_transfer tr[SPI_IOC_TRANSFERS] = {
  { // The first transfer is just to delay a bit after nSEL assertion
    .tx_buf        = (unsigned long)NULL,
    .rx_buf        = (unsigned long)NULL,
    .len           = 0,
    .delay_usecs   = NCP_SPI_DELAY_BEG_US,
    .speed_hz      = 0,
    .bits_per_word = 0,
    .cs_change     = FALSE,
  },{ // The 2nd transfer is the actual data transfer
    .tx_buf        = (unsigned long)ncpTxBuffer,
    .rx_buf        = (unsigned long)ncpRxBuffer,
    .len           = pktLen,
    .delay_usecs   = NCP_SPI_DELAY_END_US,
    .speed_hz      = NCP_SPI_SPEED_HZ,
    .bits_per_word = NCP_SPI_BITS_PER_WORD,
    .cs_change     = FALSE, // Could be TRUE as well
  },
  };

  // Perform the SPI transaction
  logEvent(LOG_SPI_TX, NULL, ncpTxBuffer, pktLen);
  int ret = ioctl(ncpSpiFd, SPI_IOC_MESSAGE(SPI_IOC_TRANSFERS), tr);
  assert(ret >= 0);
  logEvent(LOG_SPI_RX, NULL, ncpRxBuffer, pktLen);

  // Turn off reset indication after 1st transaction
  if (ncpTxBuffer[NCP_SPI_PKT_CMD_IDX] & NCP_SPI_PKT_CMD_RST) {
    ncpTxBuffer[NCP_SPI_PKT_CMD_IDX] &= ~NCP_SPI_PKT_CMD_RST;
  }

  return spiParseRx(pktLen);
}

static void initHandler(void)
{
  //FIXME!!
}

static void errorHandler(void)
{
  //FIXME!!
  rxErrorEvent.active = FALSE;
  setTimeout(&initEvent, ERROR_DELAY);
}

//------------------------------------------------------------------------------
// Client socket interface.

static void openListenSocket(int16u port, boolean allInterfaces)
{
  listenFd = socket(AF_INET6, SOCK_STREAM, 0);
  if (listenFd < 0) {
    perror("socket creation failed");
    exit(1);
  }
  // Set SO_REUSEADDR prior to bind() to avoid re-bind() hassles
  int set = 1;
  (void) setsockopt(listenFd, SOL_SOCKET, SO_REUSEADDR, &set, sizeof(set));
  struct sockaddr_in6 address;
  memset(&address, 0, sizeof(struct sockaddr_in6));
  address.sin6_family = AF_INET6;
  address.sin6_addr = (allInterfaces ? in6addr_any : in6addr_loopback);
  address.sin6_port = htons(port);
  if (bind(listenFd, (struct sockaddr *) &address, sizeof(address)) != 0) {
    perror("bind failed");
    exit(1);
  }
  // Put listen socket into non-block mode for accept()
  int flags = fcntl(listenFd, F_GETFL);
  if (flags >= 0) {
    fcntl(listenFd, F_SETFL, flags | O_NONBLOCK);
  }

  if (listen(listenFd, 2) == -1) {
    perror("listen failed");
    exit(1);
  }
  logStatus("Listening on port %u.", port);
}

static void handleListenInput(void)
{
  // Terminate an existing client if new one comes along
  //FIXME: Do we really want to allow this?
  if (clientInFd >= 0) {
    close(clientInFd);
    if(clientInFd == clientOutFd)
      clientOutFd = -1;
    clientInFd = -1;
  }
  if (clientOutFd >= 0) {
    close(clientOutFd);
    clientOutFd = -1;
  }
  struct sockaddr addr;
  socklen_t addrlen = sizeof(addr);
  clientInFd = accept(listenFd, &addr, &addrlen);
  if (clientInFd < 0) {
    return; // False alarm -- client has already disappeared
  }
  clientOutFd = clientInFd;
  logEvent(LOG_CLIENT_RX, "ACCEPT", NULL, 0);
  logStatus("Client accepted.");
  // Put client socket into non-block mode for read()/write()
  int flags = fcntl(clientInFd, F_GETFL);
  if (flags >= 0) {
    fcntl(clientInFd, F_SETFL, flags | O_NONBLOCK);
  }
}

static boolean handleClientAndNcpInput(boolean clientPending, boolean ncpPending)
{
  if (ncpPending) {
    ncpPending = ncpHostIntAsserted(); // Necessary to 'ack' the interrupt
  }
  while (1) {
    // Check for more data to send to NCP
    if ( (ncpTxAvail < NCP_SPI_MAX_PAYLOAD) && (clientInFd >= 0) ) {
      int got = read(clientInFd, &ncpTxBuffer[NCP_SPI_PKT_PAY_IDX + ncpTxAvail],
                     NCP_SPI_MAX_PAYLOAD - ncpTxAvail);
      if (got > 0) {
        logEvent(LOG_CLIENT_RX,
                 NULL,
                 &ncpTxBuffer[NCP_SPI_PKT_PAY_IDX + ncpTxAvail],
                 got);
        ncpTxAvail += got;
        assert(ncpTxAvail >= 0 && ncpTxAvail <= NCP_SPI_MAX_PAYLOAD);
      } else
      if ( (got == 0) || (errno != EWOULDBLOCK) ) { // EOF/error
        // shutdown(clientInFd, SHUT_RDWR); //FIXME: Need to shutdown() before close()?
        close(clientInFd);
        if(clientInFd == clientOutFd) {
          clientOutFd = -1;
        } else {
          close(clientOutFd);
        }
        clientInFd = -1;
        logEvent(LOG_CLIENT_RX, "EOF", NULL, 0);
        logWarning("Client disconnected.");
        if (! multiClient) {
          logWarning("Terminating.");
          cleanupAndExit(0);
        } else {
          // Don't return here - there still might be data to transact
          // and more clients to handle.
        }
      } else {
        // EWOULDBLOCK -- no data to forward yet
      }
    }
    if ( ncpPending && (ncpRxAvail == 0) ) {
      // NCP is saying it has data for us, but we don't know how much.
      // Rather than doing a tiny probe just to find out, assume it is
      // a bunch and just go grab as much as possible.
      ncpRxAvail = NCP_SPI_MAX_PAYLOAD;
    }
    ncpPending = FALSE;
    if ( (ncpTxAvail == 0) && (ncpRxAvail == 0) ) {
      return FALSE; // Nothing more to do -- caller can use long leash
    }
    // ncpTxAvail > 0 indicates data to send to  NCP
    // ncpRxAvail > 0 indicates data to get from NCP
    // Our SPI transaction payload shall be the greater of the two
    int16s payLen = MAX(ncpTxAvail, ncpRxAvail);
    // ncpTxBuffer[NCP_SPI_PKT_CMD_IDX] is already set up; we rarely change it
    ncpTxBuffer[NCP_SPI_PKT_LEN_IDX] = ncpTxAvail;
    // ncpTxBuffer[] already contains data to be sent, but pad it out
    // to the actual transaction length with NCP_HOST_TX_PAD
    memset(&ncpTxBuffer[NCP_SPI_PKT_PAY_IDX + ncpTxAvail], NCP_HOST_TX_PAD,
           payLen - ncpTxAvail + NCP_SPI_RX_SLOP);
    // ncpRxBuffer[] gets cleared by padding using NCP_EM3XX_RX_PAD
    memset(ncpRxBuffer, NCP_EM3XX_RX_PAD, payLen + NCP_SPI_RX_SLOP);
    if (spiTransact(NCP_SPI_PKT_OVERHEAD + payLen + NCP_SPI_RX_SLOP)) {
      // NCP accepted the data sent!
      ncpTxAvail = 0;
    }
    if ( (ncpTxAvail > 0) && (ncpRxAvail == 0) ) {
      // NCP NAK'd data sent to it, and has no data to return.
      // This is likely a flow-control situation.
      // Return -- caller should use a short leash
      return TRUE;
    }
    // Continue loop to try to (re)send or retrieve pending data
    interTransactionDelay(); // Ensure enough time between NCP transactions
  }
  assert(FALSE); // we shouldn't get here...
}

//------------------------------------------------------------------------------
// Timing.

static int32u getInt32uMillisecondTick(void)
{
  struct timeval tv;
  int32u now;
  gettimeofday(&tv, NULL);
  now = (tv.tv_sec * 1000) + (tv.tv_usec / 1000);
  return now;
}

static void setTimeout(EventControl *event, int32u delay)
{
  event->timeToExecute = getInt32uMillisecondTick() + delay;
  event->active = TRUE;
}

static void checkEvent(EventControl *event)
{
  int32u now = getInt32uMillisecondTick();
  if (event->active
      && timeGTorEqualInt32u(now, event->timeToExecute)) {
    event->active = FALSE;
    event->handler();
  }
}

//------------------------------------------------------------------------------
// Command-line arguments.

static boolean parseArguments(int argc, char *argv[])
{
  if (argc < 3) {
    return FALSE;
  }
  char portArg[DEV_STRING_MAX+1];
  char spiDevArg[DEV_STRING_MAX+1];
  char intGpioArg[GPIO_STRING_MAX+1];
  char resetGpioArg[GPIO_STRING_MAX+1];
  boolean allInterfaces;
  boolean useStdio = FALSE;
  int port = -1;
  memset(portArg, 0, sizeof(portArg));
  memset(spiDevArg, 0, sizeof(spiDevArg));
  memset(intGpioArg, 0, sizeof(intGpioArg));

  while (TRUE) {
    static struct option long_options[] = {
      { "nolog",          no_argument,        &logging,   0  },
      { "port",           required_argument,  0,         'p' },
      { "spiDev",         required_argument,  0,         's' },
      { "intGpio",        required_argument,  0,         'i' },
      { "resetGpio",      required_argument,  0,         'r' },
      { "wakeGpio",       required_argument,  0,         'w' },
      { "traceMask",      required_argument,  0,         'T' },
      { "allInterfaces",  no_argument,        0,         'a' },
      { "multiClient",    no_argument,        0,         'm' },
      { "timestamp",      no_argument,        0,         't' },
      { 0, 0, 0, 0 }
    };

    int option_index = 0;
    int c = getopt_long(argc, argv, "p:s:i:r:w:T:amt", long_options, &option_index);
    if (c == -1) {
      if (option_index != argc && option_index != 0) {
        fprintf(stderr, "Unexpected argument %s\n", argv[option_index]);
        return FALSE;
      }
      break;
    }

    switch (c) {
    case 0:
      break;
    case 'a': // listen on all interfaces (undocumented)
      allInterfaces = TRUE;
      break;
    case 'm': // allow multiple clients (don't exit after first) (undocumented)
      multiClient = TRUE;
      break;
    case 'p': // port
      if (strcmp(optarg, "-") == 0) {
        useStdio = TRUE;
      } else {
        sscanf(optarg, "%" STRINGIFYX(DEV_STRING_MAX) "s", portArg);
        port = atoi(portArg);
        if (port < 0 || port > 65535) {
          fprintf(stderr, "Invalid simulator port number %d\n", port);
          return FALSE;
        }
      }
      break;
    case 's': // spidev
      sscanf(optarg, "%" STRINGIFYX(DEV_STRING_MAX) "s", spiDevArg);
      break;
    case 'i': // nHOST_INT GPIO
      sscanf(optarg, "%" STRINGIFYX(DEV_STRING_MAX) "s", intGpioArg);
      break;
    case 'r': // nRESET GPIO
      sscanf(optarg, "%" STRINGIFYX(DEV_STRING_MAX) "s", resetGpioArg);
      break;
    case 'w': // nWAKE GPIO
      sscanf(optarg, "%" STRINGIFYX(DEV_STRING_MAX) "s", wakeGpioArg);
      break;
    case 'T': // trace mask
      traceMask = (int8u) strtoul(optarg, NULL, 16);
      break;
    case 't': // print additional timestamp
      additionalTimestamp = TRUE;
      break;
    default:
      return FALSE;
    }
  }

  logEnabled = (boolean) logging;
  if (logEnabled) {
    openlog("spi-server", LOG_PERROR | LOG_PID | LOG_CONS, LOG_DAEMON);
  }

  logStatus("traceMask = 0x%02X", traceMask);
  openNcpSpiSerial(spiDevArg, intGpioArg);
  openNcpReset(resetGpioArg);
  if (useStdio) {
    multiClient = FALSE;
    clientInFd = dup(STDIN_FILENO);
    clientOutFd = dup(STDOUT_FILENO);

    // Put client socket into non-block mode for read()/write()
    int flags = fcntl(clientInFd, F_GETFL);
    if (flags >= 0) {
      fcntl(clientInFd, F_SETFL, flags | O_NONBLOCK);
    }
  } else {
    openListenSocket(port, allInterfaces);
  }
  return TRUE;
}

//------------------------------------------------------------------------------
// Cleanup

static int signalCaught = 0;

static void signalHandler(int sig)
{
  // save signal for DSR processing to avoid race condition of processing now
  signalCaught = sig;

  // Immediately unlock the SPI socket.
  flock(ncpSpiFd, LOCK_UN);

  // If we get this signal twice, then just die.
  signal(sig, SIG_DFL);
}

static void cleanupAndExit(int exitCode)
{
  // Unlock the SPI socket.
  flock(ncpSpiFd, LOCK_UN);

  if (signalCaught) {
    // we test this here because some syscalls, notably select(), return an
    // error when interrupted by a signal and we want to still be able to
    // inform the user that the signal was the underlying cause
    logWarning("Caught signal %d, terminating", signalCaught);
  }

  closelog();
  exit(exitCode);
}

//------------------------------------------------------------------------------
// Main.

int main(int argc, char **argv)
{
  boolean ncpFlowControlled = FALSE;

  if (! parseArguments(argc, argv)) {
    fprintf(stderr, "Usage: %s -p port -s spiDev [-i intGpio] [-r resetGpio]"
                    " [-w wakeGpio] [-T traceMask] [-f]\n", PROGNAME);
    exit(1);
  }

  resetNcp();

  logStatus("SPI Server running...");

  // configure signal handlers
  struct sigaction sigHandler;
  sigHandler.sa_handler = signalHandler;
  sigemptyset(&sigHandler.sa_mask);
  sigHandler.sa_flags = 0;
  sigaction(SIGINT, &sigHandler, NULL);
  sigaction(SIGTERM, &sigHandler, NULL);
  sigaction(SIGABRT, &sigHandler, NULL);
  sigaction(SIGALRM, &sigHandler, NULL);

  while (1) {
    if (signalCaught) {
      cleanupAndExit(1);
    }

    checkEvent(&rxErrorEvent);
    checkEvent(&initEvent);
    if (initEvent.active) {
      //FIXME: Anything to do?
    } else {
      fd_set input, except;
      int maxFd = -1;
      FD_ZERO(&input);
      FD_ZERO(&except);
      if (listenFd >= 0) {
        FD_SET(listenFd, &input);
        FD_SET(listenFd, &except);
        maxFd = listenFd;
      }
      if (clientOutFd >= 0) {
        FD_SET(clientOutFd, &except);
        maxFd = clientOutFd;
      }
      if (clientInFd >= 0) {
        if (!ncpFlowControlled) {
          FD_SET(clientInFd, &input);
        }
        FD_SET(clientInFd, &except);
        maxFd = MAX(maxFd, clientInFd);
      }
      if (ncpIntFd >= 0) {
        FD_SET(ncpIntFd, &except);
        maxFd = MAX(maxFd, ncpIntFd);
      }
      struct timeval timeout;
      timeout.tv_sec = 0;
      timeout.tv_usec = ncpFlowControlled ? TIMEOUT_SHORT_US : TIMEOUT_LONG_US;

      alarm(0); // Turn off our watchdog.

      int n = select(maxFd+1, &input, NULL, &except, &timeout);

      // Turn our watchdog back on. If the process doesn't get
      // back around to the above call to `alarm(0)` within
      // five seconds, then the process will receive SIGALRM
      // and terminate.
      alarm(SPI_SERVER_WATCHDOG_TIMEOUT);

#if     VERBOSE_DEBUG
      logEvent(LOG_SPI_LOGIC, "Wake", (int8u*)&n, sizeof(n));
      logEvent(LOG_SPI_LOGIC, "Time", (int8u*)&timeout.tv_usec, sizeof(timeout.tv_usec));
#endif//VERBOSE_DEBUG
      if ((n < 0) && (errno != EINTR)) {
        perror("select failed");
        cleanupAndExit(1);
      } else if (n == 0) {
        // Timeout.
        ncpFlowControlled = handleClientAndNcpInput(FALSE, FALSE);
      } else {
        if (VALID_FD_ISSET(clientInFd, &except)) {
          perror("clientInFd socket error");
          cleanupAndExit(1);
        }
        if (VALID_FD_ISSET(clientOutFd, &except)) {
          perror("clientOutFd socket error");
          cleanupAndExit(1);
        }
        if (VALID_FD_ISSET(listenFd, &except)) {
          perror("listenFd socket error");
          cleanupAndExit(1);
        }
        if (VALID_FD_ISSET(listenFd, &input)) {
          handleListenInput();
        }
        ncpFlowControlled = handleClientAndNcpInput(
                                VALID_FD_ISSET(clientInFd, &input),
                                VALID_FD_ISSET(ncpIntFd, &except));
      }
    }
  }
}

//------------------------------------------------------------------------------
// Log.

static void logEvent(LogEvent type, char *subType, int8u *data, int16u length)
{
  if (logEnabled) {
    if ((1<<type) & traceMask) {
      #define BUF_LEN 100
      char logBuffer[BUF_LEN];
      int8u bufLen=0;

      bufLen = sprintf(logBuffer, "[%s] ", logEventNames[type]);
      if (additionalTimestamp)
      {
        char datetime[MAX_DATE_TIME_STR_LEN];
        getLogTimestamp(datetime);
        bufLen += sprintf(&logBuffer[bufLen], "[%s] [", datetime);
      }

      if (subType != NULL) {
        bufLen += sprintf(&logBuffer[bufLen], "%s", subType);
      }

      int16u i;
      for (i = 0; i < length; i++) {
        bufLen += sprintf(&logBuffer[bufLen], " %02X", data[i]);
        if (bufLen >= BUF_LEN-4)
        {
          logBuffer[bufLen]='\0';
          syslog(LOG_INFO, "%s", logBuffer);
          bufLen = 0;
        }
      }
      bufLen += sprintf(&logBuffer[bufLen], "]");
      logBuffer[bufLen]='\0';

      syslog(LOG_DEBUG, "%s", logBuffer);
    }
  }
}

static void getLogTimestamp(char *datetime)
{
  struct timeval tv;
  struct tm *tm;
  uint8_t len = 0;
  gettimeofday(&tv, NULL);
  tm = localtime(&tv.tv_sec);
  assert(tm != NULL);
  len = strftime(datetime, MAX_DATE_TIME_STR_LEN, "%Y-%m-%d %H:%M:%S", tm);
  sprintf(&datetime[len], ".%03d", (int)(tv.tv_usec / 1000));
}

static void logStatus(char *format, ...)
{
  va_list ap;
  va_start(ap, format);
  vsyslog(LOG_INFO, format, ap);
}

static void logError(char *format, ...)
{
  va_list ap;
  va_start(ap, format);
  vsyslog(LOG_ERR, format, ap);
}

static void logWarning(char *format, ...)
{
  va_list ap;
  va_start(ap, format);
  vsyslog(LOG_WARNING, format, ap);
}
