# Alpine `apk add php-parallax`

This directory holds the Alpine package recipe (`APKBUILD`) for the `php84-parallax` package and the GitHub-Pages-hosted apk repository configuration.

## Install path users actually use

Until the package is accepted into Alpine's [aports `community/`](https://gitlab.alphpinelinux.org/alpine/aports), users wire up a third-party apk repository hosted by this project. The release workflow publishes signed `.apk` files for every tagged version to the `gh-pages` branch.

```sh
# Trust the project's apk signing key (one-time, per host)
curl -fsSL https://andreymashukov.github.io/php-parallax/alpine/keys/andreymashukov.rsa.pub \
    > /etc/apk/keys/andreymashukov.rsa.pub

# Register the third-party repository
echo "https://andreymashukov.github.io/php-parallax/alpine/v3.20/community" \
    >> /etc/apk/repositories

# Install
apk update
apk add php-parallax
```

`php-parallax` is an alias provided by `php84-parallax`; pinning to a specific PHP version is optional (`apk add php84-parallax`).

## Building the .apk locally

```sh
docker run --rm -it -v "$PWD:/src" -w /src/dist/alpine alpine:3.20 sh -c '
  apk add --no-cache alpine-sdk php84-dev php84-zts
  adduser -D builder && addgroup builder abuild
  su builder -c "abuild-keygen -ain && abuild -F"
'
```

The produced `.apk` lands under `~/packages/` and can be installed directly via `apk add --allow-untrusted ./php84-parallax-VER.apk` for ad-hoc testing.

## Submitting to aports `community/`

The APKBUILD above is written so it can be dropped into aports without modification once a community maintainer agrees to sponsor it.

1. Open an issue at [gitlab.alpinelinux.org/alpine/aports/-/issues](https://gitlab.alpinelinux.org/alpine/aports/-/issues) titled "[community] new package: php84-parallax".
2. Reference the upstream URL, the licence (MIT), the active maintainer (Andrei Mashukov).
3. Submit a merge request copying `dist/alpine/APKBUILD` into `community/php84-parallax/APKBUILD` once a sponsor responds.
