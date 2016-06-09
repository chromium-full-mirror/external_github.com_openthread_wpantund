Using `wpantund` with `spi-server`
==================================

## Background ##

A low-power wireless network co-processor (NCP) can be connected to a
host in one of two ways:

 *  Asynchronous Serial w/ Flow Control (UART)
 *  Serial Peripheral Interface (SPI)

`wpantund` can directly communicate with the NCP using just about any
method that involves (or behaves like) a two-way serial pipe:

1.  Direct communication with serial device (like `/dev/ttyUSB0`)
2.  A TCP connection to a specific IPv4 host and port
3.  `stdin`/`stdout` of a child process

However, SPI is not a two-way serial pipe. Thus, in order for
`wpantund` to be able to use an NCP that is connected to the host via
SPI, there must be a "middle-man" process which allows us to use the
SPI interface like we would a serial port. That middle-man process is
`spi-server`.

## SPI Server ##

The `spi-server` process abstracts away the details of communicating
directly with the NCP via SPI and presents `wpantund` with an
interface that behaves much like a serial port. That interface can be
either a network port or `stdin`/`stdout` (preferred).

### Hardware Requirements ###

In order to communicate with an NCP using SPI, the following are the
*minimum required* connections that need to be made between the NCP
and the host processor:

Name    | Description               | Host   | NCP    | Trigger
--------|---------------------------|--------|--------|----------------
*SCLK*  | SPI Clock                 | Output | Input  |
*MISO*  | Master-Input/Slave-Output | Input  | Output |
*MOSI*  | Master-Output/Slave-Input | Output | Input  |
*CS*    | Chip/Slave Select         | Output | Input  |
*INT*   | Host Interrupt            | Input  | Output | Falling Edge
*RESET* | NCP Hardware Reset        | Output | Input  |

Note that all of these connections are open-drain active-low.

### Software Requirements ###

`spi-server` needs the above signals to be exposed as devices in the
filesystem:

 *  *SCLK*, *MISO*, *MOSI*, and *CS*: Exposed as a standard linux SPI
    device (e.g.: `/dev/spidev2.0`)
 *  *INT* and *RESET*: Exposed as standard linux GPIO devices: (e.g.:
    `/sys/class/gpio/gpio11/value`)

Additionally, if the host processor is designed to go to sleep, *INT*
should additionally be configured to trigger a wakeup when it is
asserted (low).

### How `spi-server` is invoked ###

In general, `spi-server` is invoked by `wpantund` using the following
command arguments:

    spi-server -p <port> -s <spi-device-path> -i <int-gpio-value-path> -r <reset-gpio-value-path>

Where

 *  `<port>`: Either a numerical TCP port number to listen on, or `-`
    if `stdin`/`stdout` should be used instead.
 *  `<spi-device-path>`: The path to the SPI device (e.g.
    `/dev/spidev2.0`)
 *  `<int-gpio-value-path>`: The path to the value file for the *INT*
    GPIO (e.g. `/sys/class/gpio/gpio11/value`)
 *  `<reset-gpio-value-path>`: The path to the value file for the
    *RESET* GPIO (e.g. `/sys/class/gpio/gpio49/value`)

For example:

    spi-server -p - -s /dev/spidev2.0 -i /sys/class/gpio/gpio11/value -r /sys/class/gpio/gpio49/value

## Using `spi-server` with `wpantund` ##

The easiest way to get `wpantund` to use `spi-server` is to have
`wpantund` invoke `spi-server` directly. We can do this by specifying
the command line to invoke `spi-server` using a specially-formatted
value for the `NCPSocketName` property in `wpantund.conf`:

    NCPSocketName "system:/usr/sbin/spi-server -p - -s /dev/spidev2.0 -i /sys/class/gpio/gpio11/value -r /sys/class/gpio/gpio49/value"

By setting this property, we are telling `wpantund` that whenever it
wants to open a connection to the NCP that it should launch the given
program and communicate with it (via `stdin`/`stdout`) as if it were a
UART-based NCP.

## Troubleshooting ##

The number one cause of problems when using `wpantund` with
`spi-server` is the misconfiguration of one or more GPIO pins on the
host processor (specifically the *INT* pin). If you run into problems,
be sure to first verify that the direction and edge are set properly
on all of the pins shown in the table above. For example, symptoms of
the INT pin being misconfigured include inconsistent NCP behavior and
firmware update failures.

For example, if you are having problems and your `NCPSocketName` line
in `wpantund.conf` looks like this:

    NCPSocketName "system:/usr/sbin/spi-server -p - -s /dev/spidev2.0 -i /sys/class/gpio/gpio11/value -r /sys/class/gpio/gpio49/value"

Then you would do the following checks:

1.  Execute `cat /sys/class/gpio/gpio11/direction`
2.  Verify that the output is `in`
     *  If not, type `echo in > /sys/class/gpio/gpio11/direction` and
        continue
     *  If the file was not found, type `echo 11 >
        /sys/class/gpio/export` and try again
3.  Execute `cat /sys/class/gpio/gpio11/edge`
4.  Verify that it is set to `falling`
     *  If not, type `echo falling > /sys/class/gpio/gpio11/edge` and
        continue
5.  Execute `cat /sys/class/gpio/gpio49/direction`
6.  Verify that it is set to `out`
     *  If not, type `echo out > /sys/class/gpio/gpio49/direction` and
        continue
     *  If the file was not found, type `echo 49 >
        /sys/class/gpio/export` and try again
7.  Execute `cat /sys/class/gpio/gpio49/value`
8.  Verify that it is set to `1`.
     *  If not, type `echo 1 > /sys/class/gpio/gpio49/value` and
        continue

If everything seems configured properly, then there would likely be
something configured incorrectly with `/dev/spidev2.0`.
