/* Copyright (c) 2024, Mikhail Kovalev
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _TERMINOS_H
#define _TERMINOS_H 1

#define __DECLARE_PID_T
#include "__posix_types.h"

/// Used for terminal special characters.
typedef unsigned char cc_t;

/// Used for terminal baud rates.
typedef unsigned int speed_t;

/// Used for terminal modes.
typedef unsigned int tcflag_t;

#define NCCS 32

struct terminos {
    tcflag_t c_iflag;  ///< Input modes.
    tcflag_t c_oflag;  ///< Output modes.
    tcflag_t c_cflag;  ///< Control modes.
    tcflag_t c_lflag;  ///< Local modes.
    cc_t c_cc[NCCS];     ///< Control characters.
};

/// EOF character (Ctrl-D).
#define CEOF 0x04

/// EOL character (Ctrl-J).
#define CEOL 0x0A

/// Erase character (Backspace).
#define CERASE 0x7F

/// EINTR (interrupt) character (Ctrl-C).
#define CEINTR 0x03

/// Kill character (Ctrl-U).
#define CEKILL 0x15

/// MIN value.
#define VMIN 16

/// QUIT character (Ctrl-\).
#define CEQUIT 0x1C

/// START character (Ctrl-Q).
#define VSTART 0x11

/// STOP character (Ctrl-S).
#define VSTOP 0x13

/// SUSP character (Ctrl-Z).
#define VSUSP 0x1A

/// TIME value.
#define VTIME 17


/// Input modes.
enum {
    BRKINT = 0x0001, ///< Signal interrupt on break.
    ICRNL = 0x0002, ///< Map CR to NL on input.
    IGNBRK = 0x0004, ///< Ignore break condition.
    IGNCR = 0x0008, ///< Ignore CR.
    IGNPAR = 0x0010, ///< Ignore characters with parity errors.
    INLCR = 0x0020, ///< Map NL to CR on input.
    INPCK = 0x0040, ///< Enable input parity check.
    ISTRIP = 0x0080, ///< Strip character.
    IXANY = 0x0100, ///< Enable any character to restart output.
    IXOFF = 0x0200, ///< Enable start/stop input control.
    IXON = 0x0400, ///< Enable start/stop output control.
    PARMRK = 0x0800, ///< Mark parity errors.
};

/// Output modes.
enum {
    OPOST = 0x0001, ///< Post-process output.
    ONLCR = 0x0002, ///< Map NL to CR-NL on output.
    OCRNL = 0x0004, ///< Map CR to NL on output.
    ONOCR = 0x0008, ///< No CR output at column 0.
    ONLRET = 0x0010, ///< NL performs CR function.
    OFDEL = 0x0020, ///< Fill is DEL.
    OFILL = 0x0040, ///< Use fill characters for delay.
    NLDLY = 0x0080, ///< Select newline delays: NL0/NL1.
    CRDLY = 0x0100, ///< Select carriage-return delays: CR0/CR1/CR2/CR3.
    TABDLY = 0x0200, ///< Select horizontal-tab delays: TAB0/TAB1/TAB2/TAB3.
    BSDLY = 0x0400, ///< Select backspace delays: BS0/BS1.
    VTDLY = 0x0800, ///< Select vertical-tab delays: VT0/VT1.
    FFDLY = 0x1000, ///< Select form-feed delays: FF0/FF1.
};

/// Baud rates.
enum {
    B0 = 0, ///< Hang up.
    B50 = 50, ///< 50 baud.
    B75 = 75, ///< 75 baud.
    B110 = 110, ///< 110 baud.
    B134 = 134, ///< 134.5 baud.
    B150 = 150, ///< 150 baud.
    B200 = 200, ///< 200 baud.
    B300 = 300, ///< 300 baud.
    B600 = 600, ///< 600 baud.
    B1200 = 1200, ///< 1200 baud.
    B1800 = 1800, ///< 1800 baud.
    B2400 = 2400, ///< 2400 baud.
    B4800 = 4800, ///< 4800 baud.
    B9600 = 9600, ///< 9600 baud.
    B19200 = 19200, ///< 19200 baud.
    B38400 = 38400, ///< 38400 baud.
    B57600 = 57600, ///< 57600 baud.
    B115200 = 115200, ///< 115200 baud.
    B230400 = 230400, ///< 230400 baud.
    B460800 = 460800, ///< 460800 baud.
    B500000 = 500000, ///< 500000 baud.
    B576000 = 576000, ///< 576000 baud.
    B921600 = 921600, ///< 921600 baud.
    B1000000 = 1000000, ///< 1000000 baud.
    B1152000 = 1152000, ///< 1152000 baud.
    B1500000 = 1500000, ///< 1500000 baud.
    B2000000 = 2000000, ///< 2000000 baud.
    B2500000 = 2500000, ///< 2500000 baud.
    B3000000 = 3000000, ///< 3000000 baud.
    B3500000 = 3500000, ///< 3500000 baud.
    B4000000 = 4000000, ///< 4000000 baud.
};

/// Control modes.
enum {
    CIGNORE = 0x0001, ///< Ignore control flags.
    CSIZE = 0x0006, ///< Character size mask.
    CS5 = 0x0000, ///< 5 bits (pseudo).
    CS6 = 0x0002, ///< 6 bits.
    CS7 = 0x0004, ///< 7 bits.
    CS8 = 0x0006, ///< 8 bits.
    CSTOPB = 0x0008, ///< Send 2 stop bits if set, otherwise 1.
    CREAD = 0x0010, ///< Enable receiver if set.
    PARENB = 0x0020, ///< Parity enable.
    PARODD = 0x0040, ///< Odd parity if set, even if clear.
    HUPCL = 0x0080, ///< Hang up on last close.
    CLOCAL = 0x0100, ///< Ignore modem status lines.
};

/// Local modes.
enum {
    ECHO = 0x0001, ///< Enable echo.
    ECHOE = 0x0002, ///< Echo erase character as error-correcting backspace.
    ECHOK = 0x0004, ///< Echo KILL.
    ECHONL = 0x0008, ///< Echo NL.
    ICANON = 0x0010, ///< Canonical input (erase and kill processing).
    IEXTEN = 0x0020, ///< Enable extended input character processing.
    ISIG = 0x0040, ///< Enable signals.
    NOFLSH = 0x0080, ///< Disable flush after interrupt or quit.
    TOSTOP = 0x0100, ///< Send SIGTTOU for background output.
};

/// Attribute selection.
enum {
    TCSANOW = 0, ///< Make change immediate.
    TCSADRAIN = 1, ///< Drain output, then change.
    TCSAFLUSH = 2, ///< Drain output, flush input.
};

/// Line control.
enum {
    TCIFLUSH = 0, ///< Flush pending input.
    TCOFLUSH = 1, ///< Flush output.
    TCIOFLUSH = 2, ///< Flush both.
};

enum {
    TCIOFF = 0, ///< Transmit a STOP character.
    TCION = 1, ///< Transmit a START character.
    TCOOFF = 2, ///< Suspend output.
    TCOON = 3, ///< Restart output.
};

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __STDC_HOSTED__

speed_t cfgetispeed(const struct termios *);
speed_t cfgetospeed(const struct termios *);
int     cfsetispeed(struct termios *, speed_t);
int     cfsetospeed(struct termios *, speed_t);
int     tcdrain(int);
int     tcflow(int, int);
int     tcflush(int, int);
pid_t   tcgetsid(int);
int     tcsendbreak(int, int);
int     tcsetattr(int, int, const struct termios *);

/**
 * @brief Get the terminal attributes.
 *
 * The `tcgetattr` function gets the parameters associated with the terminal
 * referred to by `fd` and stores them in the termios structure referenced by
 * `termios_p`.
 *
 * @param fd          The file descriptor associated with the terminal.
 * @param termios_p   A pointer to a termios structure where the terminal
 *                    attributes will be stored.
 * @return            Upon successful completion, `tcgetattr` returns 0. Otherwise,
 *                    it returns -1 and sets `errno` to indicate the error.
 */
int tcgetattr(int fd, struct terminos *termios_p);

#endif

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif // _TERMINOS_H 