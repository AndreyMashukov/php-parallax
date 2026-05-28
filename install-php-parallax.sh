#!/bin/sh
# install-php-parallax.sh — one-line installer for the php-parallax extension.
#
#   curl -sSL https://github.com/AndreyMashukov/php-parallax/raw/main/install-php-parallax.sh | sh
#
# The script:
#   1. Verifies the active PHP build is ZTS — fails fast otherwise.
#   2. Installs build dependencies via apt / apk / yum where available.
#   3. Clones (or refreshes) the project sources under /tmp/php-parallax-src.
#   4. Runs phpize → configure → make → make install.
#   5. Drops an extension=parallax.so directive into the active PHP scan dir.
#
# The script is idempotent: running it twice on the same host is safe.

set -eu

REPO_URL="${PHP_PARALLAX_REPO:-https://github.com/AndreyMashukov/php-parallax.git}"
REF="${PHP_PARALLAX_REF:-main}"
SRC_DIR="${PHP_PARALLAX_SRC:-/tmp/php-parallax-src}"

log() { printf '[php-parallax] %s\n' "$*" >&2; }
die() { log "FATAL: $*"; exit 1; }

require_bin() {
    command -v "$1" >/dev/null 2>&1 || die "missing required tool: $1"
}

# 1. ZTS check
require_bin php
if [ "$(php -r 'echo ZEND_THREAD_SAFE ? 1 : 0;')" != "1" ]; then
    die "the active PHP build is NOT ZTS. php-parallax requires a thread-safe PHP (--enable-zts). Use the php:*-zts-* Docker images or rebuild PHP with ZTS."
fi
log "PHP ZTS: ok ($(php -r 'echo PHP_VERSION;'))"

# 2. Build deps
if   command -v apk      >/dev/null 2>&1; then
    log "installing build deps via apk"
    apk add --no-cache --quiet ${PHPIZE_DEPS:-autoconf dpkg-dev file g++ gcc libc-dev make pkg-config re2c} git make >/dev/null
elif command -v apt-get  >/dev/null 2>&1; then
    log "installing build deps via apt"
    DEBIAN_FRONTEND=noninteractive apt-get update -qq >/dev/null
    DEBIAN_FRONTEND=noninteractive apt-get install -y -qq --no-install-recommends autoconf gcc make pkg-config git >/dev/null
elif command -v yum      >/dev/null 2>&1; then
    log "installing build deps via yum"
    yum install -y -q autoconf gcc make pkg-config git >/dev/null
else
    log "no package manager detected; assuming build deps are present"
fi

require_bin phpize
require_bin make

# 3. Sources
if [ -d "$SRC_DIR/.git" ]; then
    log "refreshing existing checkout in $SRC_DIR"
    (cd "$SRC_DIR" && git fetch --quiet origin && git checkout --quiet "$REF" && git reset --quiet --hard "origin/$REF" 2>/dev/null || true)
else
    log "cloning $REPO_URL@$REF into $SRC_DIR"
    rm -rf "$SRC_DIR"
    git clone --quiet --depth 1 --branch "$REF" "$REPO_URL" "$SRC_DIR"
fi

cd "$SRC_DIR"

# 4. Build
log "phpize"
phpize >/dev/null
log "configure --enable-parallax"
./configure --enable-parallax >/dev/null
log "make -j$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 1)"
make -j"$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 1)" >/dev/null
log "make install"
make install >/dev/null

# 5. Enable
SCAN_DIR=$(php -i | awk -F'=> ' '/Scan this dir/ { print $2; exit }')
if [ -z "$SCAN_DIR" ] || [ "$SCAN_DIR" = "(none)" ]; then
    log "PHP has no additional .ini scan dir; loading parallax via -d at runtime is your only option"
    log "to enable persistently: add 'extension=parallax.so' to your main php.ini"
else
    INI_FILE="$SCAN_DIR/parallax.ini"
    if [ ! -f "$INI_FILE" ] || ! grep -q '^extension=parallax\.so' "$INI_FILE" 2>/dev/null; then
        log "writing $INI_FILE"
        echo "extension=parallax.so" > "$INI_FILE"
    fi
fi

# 6. Verify
if php -m | grep -q '^parallax$'; then
    log "installed; php -m reports parallax loaded"
else
    die "build succeeded but extension did not load; check '$INI_FILE'"
fi
