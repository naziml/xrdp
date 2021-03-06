[![Build Status](https://travis-ci.org/neutrinolabs/xrdp.svg?branch=devel)](https://travis-ci.org/neutrinolabs/xrdp)
[![Gitter](https://badges.gitter.im/Join%20Chat.svg)](https://gitter.im/neutrinolabs/xrdp)
![Apache-License](https://img.shields.io/badge/License-Apache%202.0-blue.svg)

*Current Version:* 0.9.2

# xrdp - an open source RDP server

## Overview

**xrdp** provides a graphical login to remote machines using Microsoft
Remote Desktop Protocol (RDP). xrdp accepts connections from a variety of
RDP clients: FreeRDP, rdesktop, NeutrinoRDP and Microsoft Remote Desktop
Client (for Windows, Mac OS, iOS and Android).

RDP transport is encrypted using TLS by default.

![demo](https://github.com/neutrinolabs/xrdp/raw/gh-pages/xrdp_demo.gif)

## Features

### Remote Desktop Access

 * Connect to a Linux desktop using RDP from anywhere (requires
   [xorgxrdp](https://github.com/neutrinolabs/xorgxrdp) Xorg module)
 * Reconnect to an existing session
 * Session resizing
 * RDP/VNC proxy (connect to another RDP/VNC server via xrdp)

### Access to Remote Resources
 * two-way clipboard transfer (text, bitmap, file)
 * audio redirection
 * drive redirection (mount local client drives on remote machine)

## Quick Start

Most Linux distributions should distribute the latest release of xrdp in their
repository. You would need xrdp and xorgxrdp packages for the best
experience. It is recommended that xrdp depends on xorgxrdp, so it should
be sufficient to install xrdp. If xorgxrdp is not provided, use Xvnc
server.

### Ubuntu / Debian
```bash
apt-get install xrdp
```

### RedHat / CentOS / Fedora

On RedHat and CentOS, make sure to enable EPEL packages first.

```bash
yum install epel-release
```

Install xrdp package.

```bash
yum install xrdp
```

`yum` is being replaced with `dnf`, so you may need to use `dnf` instead
of `yum` in the above commands.

To allow outside access to the service, open port 3389 on the firewall.

```
firewall-cmd --permanent --zone=public --add-port=3389/tcp
firewall-cmd --reload
```

## Environment

**xrdp** primarily targets to GNU/Linux. Tested on x86, x86_64, SPARC and
PowerPC.

xorgxrdp and RemoteFX Codec have special optimization for x86 and x86_64 using
SIMD instructions.

FreeBSD is not a primary target of xrdp. It is working on FreeBSD except
for the drive redirection feature.

Other operating systems such as Mac OS are not supported so far, but we
welcome your contributions.

## Compiling

See also https://github.com/neutrinolabs/xrdp/wiki#building-from-sources

### Prerequisites

To compile xrdp from the packaged sources, you need basic build tools - a
compiler (**gcc** or **clang**) and the **make** program.  Additionally,
you would need **openssl-devel**, **pam-devel**, **libX11-devel**,
**libXfixes-devel**, **libXrandr-devel**. More additional software would
be needed depending on your configuration.

To compile xrdp from a checked out git repository, you would additionally
need **autoconf**, **automake**, **libtool** and **pkgconfig**.

### Get the source and build it

If compiling from the packaged source, unpack the tarball and change to the
resulting directory.

If compiling from a checked out repository, please make sure you've got the submodules
cloned too (use `git clone --recursive https://github.com/neutrinolabs/xrdp`)

Then run following commands to compile and install xrdp:
```bash
./bootstrap
./configure
make
sudo make install
```

## Directory Structure

```
xrdp
├── common ······ common code
├── docs ········ documentation
├── fontdump ···· font dump for Windows
├── genkeymap ··· keymap generator
├── instfiles ··· installable data file
├── keygen ······ xrdp RSA key pair generator
├── libpainter ·· painter library
├── librfxcodec · RFX codec library
├── libxrdp ····· core RDP protocol implementation
├── m4 ·········· Autoconf macros
├── mc ·········· media center module
├── neutrinordp · RDP client module for proxying RDP connections using NeutrinoRDP
├── pkgconfig ··· pkg-config configuration
├── sesman ······ session manager for xrdp
├── tcutils ····· QT based utility program for thin clients
├── tests ······· tests for the code
├── vnc ········· VNC client module for xrdp
├── vrplayer ···· QT player redirecting video/audio to clients over xrdpvr channel
├── xorg ········ X11rdp, an Xorg backend for xrdp
├── xrdp ········ main server code
├── xrdpapi ····· virtual channel API
├── xrdpvr ······ API for playing media over RDP
└── xup ········· X11rdp and xorgxrdp client module
```
