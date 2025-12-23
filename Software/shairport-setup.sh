#!/usr/bin/env bash
apt update
apt upgrade

set -euo pipefail

CONFIG="/boot/firmware/config.txt"
BACKUP="/boot/firmware/config.txt.bak.$(date +%Y%m%d-%H%M%S)"

echo "Backing up ${CONFIG} to ${BACKUP}..."
cp -a "$CONFIG" "$BACKUP"

# Ensure file ends with a newline to keep sed/appends clean
printf "\n" >> "$CONFIG"

# Comment out 'dtparam=audio=on' if it exists (handles leading spaces)
if grep -Eq '^\s*dtparam=audio=on\s*$' "$CONFIG"; then
  echo "Commenting out 'dtparam=audio=on'..."
  # Replace whole line with commented version; keep only one commented line
  sed -i 's/^\s*dtparam=audio=on\s*$/# dtparam=audio=on/' "$CONFIG"
else
  echo "'dtparam=audio=on' not found; nothing to comment."
fi

# Add 'dtoverlay=hifiberry-dac' if not present
if grep -Eq '^\s*dtoverlay=hifiberry-dac\s*$' "$CONFIG"; then
  echo "'dtoverlay=hifiberry-dac' already present; skipping."
else
  echo "Adding 'dtoverlay=hifiberry-dac'..."
  # Put near the end for clarity
  printf "dtoverlay=hifiberry-dac\n" >> "$CONFIG"
fi

# Enable SPI via config.txt (safe and idempotent)
if grep -Eq '^\s*dtparam=spi=on\s*$' "$CONFIG"; then
  echo "SPI already enabled in config.txt; skipping."
else
  # Remove any explicit spi=off first, then add spi=on
  sed -i '/^\s*dtparam=spi=off\s*$/d' "$CONFIG"
  echo "Enabling SPI via 'dtparam=spi=on' in config.txt..."
  printf "dtparam=spi=on\n" >> "$CONFIG"
fi

# Install shairport-sync incl. dependencies and metadata-reader
cd ~
apt install --no-install-recommends build-essential git autoconf automake libtool \
    libpopt-dev libconfig-dev libasound2-dev avahi-daemon libavahi-client-dev libssl-dev libsoxr-dev \
    libplist-dev libsodium-dev libavutil-dev libavcodec-dev libavformat-dev uuid-dev libgcrypt-dev xxd
git clone https://github.com/mikebrady/nqptp.git
cd nqptp
autoreconf -fi
./configure --with-systemd-startup
make
make install
systemctl enable nqptp
systemctl start nqptp
cd  ~
git clone https://github.com/mikebrady/shairport-sync.git
cd shairport-sync
autoreconf -fi
./configure --sysconfdir=/etc --with-alsa \
    --with-soxr --with-avahi --with-ssl=openssl --with-airplay-2 --with-metadata
make
make install
cd ~
git clone https://github.com/mikebrady/shairport-sync-metadata-reader.git
cd shairport-sync-metadata-reader
autoreconf -i -f
./configure
make
make install
cd ~

# Install metadata-printer
mkdir metadata-printer
cd metadata-printer
cat > metadata-printer.c <<'EOF'
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/types.h>
#include <linux/spi/spidev.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <time.h>
#include <poll.h>
#include <errno.h>

#define MAX_N 512

const uint16_t character_patterns[] = {
  0b0000000000000000, /* (space) */
  0b0000100100000001, /* ! */
  0b0001100000000000, /* " */
  0b1001100101010100, /* # */
  0b1011001101010100, /* $ */
  0b1101011100011110, /* % */
  0b0011010001111000, /* & */
  0b0001000000000000, /* ' */
  0b0100000000001000, /* ( */
  0b0000010000000010, /* ) */
  0b1101010000011110, /* * */
  0b1001000000010100, /* + */
  0b0000000000000010, /* , */
  0b1000000000010000, /* - */
  0b0000000000000001, /* . */
  0b0100000000000010, /* / */
  0b0110101101100010, /* 0 */
  0b0100100100000000, /* 1 */
  0b1010100001110000, /* 2 */
  0b1010100101000000, /* 3 */
  0b1000101100010000, /* 4 */
  0b0010001001011000, /* 5 */
  0b1010001101110000, /* 6 */
  0b0010100100000000, /* 7 */
  0b1010101101110000, /* 8 */
  0b1010101101010000, /* 9 */
  0b0001000000000100, /* : */
  0b0001000000000010, /* ; */
  0b0100000000011000, /* < */
  0b1000000001010000, /* = */
  0b1000010000000010, /* > */
  0b1010100000000101, /* ? */
  0b1011101001100000, /* @ */
  0b1010101100110000, /* A */
  0b1011100101000100, /* B */
  0b0010001001100000, /* C */
  0b0011100101000100, /* D */
  0b0010001001110000, /* E */
  0b0010001000110000, /* F */
  0b1010001101100000, /* G */
  0b1000101100110000, /* H */
  0b0011000001000100, /* I */
  0b0000100101100000, /* J */
  0b0100001000111000, /* K */
  0b0000001001100000, /* L */
  0b0100111100100000, /* M */
  0b0000111100101000, /* N */
  0b0010101101100000, /* O */
  0b1010101000110000, /* P */
  0b0010101101101000, /* Q */
  0b1010101000111000, /* R */
  0b1010001101010000, /* S */
  0b0011000000000100, /* T */
  0b0000101101100000, /* U */
  0b0100001000100010, /* V */
  0b0000101100101010, /* W */
  0b0100010000001010, /* X */
  0b1000101101010000, /* Y */
  0b0110000001000010, /* Z */
  0b0010001001100000, /* [ */
  0b0000010000001000, /* \ */
  0b0010100101000000, /* ] */
  0b0000000000001010, /* ^ */
  0b0000000001000000, /* _ */
  0b0000010000000000, /* ` */
  0b0000000001110100, /* a */
  0b0000001001111000, /* b */
  0b1000000001110000, /* c */
  0b1000100101000010, /* d */
  0b0000000001110010, /* e */
  0b1100000000010100, /* f */
  0b1100100101000000, /* g */
  0b0000001000110100, /* h */
  0b0000000000000100, /* i */
  0b0001000000100010, /* j */
  0b0101000000001100, /* k */
  0b0000001000100000, /* l */
  0b1000000100110100, /* m */
  0b0000000000110100, /* n */
  0b1000000101110000, /* o */
  0b0000011000110000, /* p */
  0b1100100100000000, /* q */
  0b0000000000110000, /* r */
  0b1000000001001000, /* s */
  0b0000001001110000, /* t */
  0b0000000101100000, /* u */
  0b0000000000100010, /* v */
  0b0000000100101010, /* w */
  0b0100010000001010, /* x */
  0b1001100101000000, /* y */
  0b0000000001010010, /* z */
  0b0010010001010010, /* { */
  0b0001000000000100, /* | */
  0b1110000001001000, /* } */
  0b1100000000010010, /* ~ */
  0b0000000000000000, /* (del) */
};

static uint16_t pattern(char c) {
    unsigned char uc = (unsigned char)c;
    if (uc < 32 || uc > 127) return 0;
    return character_patterns[uc - 32];
}

static ssize_t display_text(int fd, const char *str, size_t n) {
    if (n < 1) return 0;

    if (n > MAX_N) n = MAX_N;

    uint16_t chars[MAX_N];

    // Reverse order is correct for your shift-register wiring.
    for (ssize_t i = (ssize_t)n - 1; i >= 0; i--) {
        chars[n - 1 - i] = pattern(str[i]);
    }

    if (write(fd, chars, n*2) < 0) {
        perror("write(spi)");
        return -1;
    }
    return (ssize_t)n;
}

/**
 * Extracts title from a string in the format: Title: "xxx"
 *
 * @param title Pointer to buffer where title will be stored (output)
 * @param n Maximum size of the title buffer
 * @param input Input string to parse
 * @return Number of characters written to title (excluding null terminator),
 *         or 0 if no valid title was found
 */
static size_t extract_title_if_present(char** title, const size_t n, const char* input) {
    if (!input || !title || !*title || n == 0) return 0;

    const char* prefix = "Title: ";
    const char* pos = strstr(input, prefix);
    if (!pos) return 0;

    pos += strlen(prefix);
    while (*pos && isspace((unsigned char)*pos)) pos++;

    if (*pos != '"') return 0;
    pos++;

    const char* end = strchr(pos, '"');
    if (!end) return 0;

    size_t title_len = (size_t)(end - pos);
    size_t copy_len = (title_len < n - 1) ? title_len : (n - 1);

    strncpy(*title, pos, copy_len);
    (*title)[copy_len] = '\0';
    return copy_len;
}

/**
 * Extracts artist from a string in the format: artist: "xxx"
 *
 * @param artist Pointer to buffer where artist will be stored (output)
 * @param n Maximum size of the artist buffer
 * @param input Input string to parse
 * @return Number of characters written to artist (excluding null terminator),
 *         or 0 if no valid artist was found
 */
static size_t extract_artist_if_present(char** artist, const size_t n, const char* input) {
    if (!input || !artist || !*artist || n == 0) return 0;

    const char* prefix = "Artist: ";
    const char* pos = strstr(input, prefix);
    if (!pos) return 0;

    pos += strlen(prefix);
    while (*pos && isspace((unsigned char)*pos)) pos++;

    if (*pos != '"') return 0;
    pos++;

    const char* end = strchr(pos, '"');
    if (!end) return 0;

    size_t artist_len = (size_t)(end - pos);
    size_t copy_len = (artist_len < n - 1) ? artist_len : (n - 1);

    strncpy(*artist, pos, copy_len);
    (*artist)[copy_len] = '\0';
    return copy_len;
}

typedef struct {
    char characters[14];
} display_line_t;

typedef struct {
    size_t scroll_progress;
    char current_line[MAX_N];
} scroll_state_t;

/**
 * Scrolls a line into a 14-character window.
 * - If len <= 14: pads with spaces
 * - If len > 14: scrolls with three separator space between repeats
 */
static display_line_t auto_scroll_line_state(scroll_state_t *st, const char *line) {
    display_line_t ret;
    size_t len = strlen(line);

    // Reset scroll if content changed
    if (strcmp(st->current_line, line) != 0) {
        st->scroll_progress = 0;
        strncpy(st->current_line, line, MAX_N - 1);
        st->current_line[MAX_N - 1] = '\0';
    }

    // Fits: pad with spaces
    if (len <= 14) {
        memcpy(ret.characters, line, len);
        for (size_t i = len; i < 14; i++) ret.characters[i] = ' ';
        return ret;
    }

    // Scroll period includes three separator space
    const size_t period = len + 3;

    for (size_t i = 0; i < 14; i++) {
        size_t src_pos = (st->scroll_progress + i) % period;
        ret.characters[i] = (src_pos < len) ? line[src_pos] : ' ';
    }

    st->scroll_progress = (st->scroll_progress + 1) % period;
    return ret;
}

static ssize_t render_28_characters(int fd, const char *s) {
    static char current_characters[28] = {0};
    if (memcmp(current_characters, s, 28) == 0) return 0;
    memcpy(current_characters, s, 28);
    return display_text(fd, current_characters, 28);
}

static uint64_t millis(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)ts.tv_nsec / 1000000ULL;
}

int main(void) {
    int fd = open("/dev/spidev0.0", O_WRONLY);
    if (fd < 0) {
        perror("open(/dev/spidev0.0)");
        return 1;
    }

    unsigned char mode = SPI_MODE_0;
    if (ioctl(fd, SPI_IOC_WR_MODE, &mode) < 0) {
        perror("SPI_IOC_WR_MODE");
        close(fd);
        return 1;
    }

    unsigned long max_speed = 100000; // Hz
    if (ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &max_speed) < 0) {
        perror("SPI_IOC_WR_MAX_SPEED_HZ");
        close(fd);
        return 1;
    }

    char input[MAX_N];

    char title_buffer[MAX_N]  = {0};
    char artist_buffer[MAX_N] = {0};
    char *title_buf_ptr  = title_buffer;
    char *artist_buf_ptr = artist_buffer;

    display_line_t title_line  = {{' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '}};
    display_line_t artist_line = {{' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '}};

    scroll_state_t title_scroll  = {0};
    scroll_state_t artist_scroll = {0};
    title_scroll.current_line[0]  = '\0';
    artist_scroll.current_line[0] = '\0';

    uint64_t last = millis();
    const uint64_t interval_ms = 150;

    // Event-driven input: poll stdin with a timeout so we still tick scrolling.
    struct pollfd pfd;
    pfd.fd = STDIN_FILENO;
    pfd.events = POLLIN;

    int stdin_open = 1;

    for (;;) {
        uint64_t now = millis();
        int timeout = (int)((last + interval_ms > now) ? (last + interval_ms - now) : 0);

        // If stdin is closed (pipe ended), we still want scrolling to continue.
        int pr = poll(stdin_open ? &pfd : NULL, stdin_open ? 1 : 0, timeout);
        if (pr < 0) {
            if (errno == EINTR) continue;
            perror("poll");
            break;
        }

        // Handle stdin events if still open
        if (stdin_open && pr > 0) {
            // If pipe closed/hung up, stop reading but keep scrolling last text.
            if (pfd.revents & (POLLHUP | POLLERR | POLLNVAL)) {
                stdin_open = 0;
            } else if (pfd.revents & POLLIN) {
                if (!fgets(input, MAX_N, stdin)) {
                    // EOF: upstream closed. Keep scrolling last known buffers.
                    stdin_open = 0;
                } else {
                    input[strcspn(input, "\n")] = 0;

                    // Update buffers on metadata lines (event-driven)
                    (void)extract_artist_if_present(&artist_buf_ptr, sizeof(artist_buffer), input);
                    (void)extract_title_if_present(&title_buf_ptr, sizeof(title_buffer), input);
                }
            }
        }

        // Tick: advance scroll and render at interval_ms
        now = millis();
        if (now - last >= interval_ms) {
            artist_line = auto_scroll_line_state(&artist_scroll, artist_buffer);
            title_line  = auto_scroll_line_state(&title_scroll,  title_buffer);

            char render_buffer[28];
            memcpy(render_buffer + 14, artist_line.characters, 14);
            memcpy(render_buffer,      title_line.characters,  14);
            (void)render_28_characters(fd, render_buffer);
            last = now;
        }
    }

    close(fd);
    return 0;
}
EOF
gcc -o metadata-printer metadata-printer.c
mv ./metadata-printer /usr/local/bin/metadata-printer
rm *
cd ..
rmdir metadata-printer

# Autostart shairport-sync, metadata-reader and metadata-printer
cat > /etc/systemd/system/shairport-custom.service << 'EOF'
[Unit]
Description=Shairport Sync with Metadata Reader
After=network.target sound.target

[Service]
Type=forking
ExecStart=/bin/bash -c 'shairport-sync & sleep 2; while [ ! -p /tmp/shairport-sync-metadata ]; do sleep 0.5; done; shairport-sync-metadata-reader < /tmp/shairport-sync-metadata | metadata-printer &'
Restart=on-failure
User=admin

[Install]
WantedBy=multi-user.target
EOF

systemctl daemon-reload
systemctl enable shairport-custom.service
systemctl start shairport-custom.service
echo "A reboot is recommended to apply changes."
echo "Run: sudo reboot now"
