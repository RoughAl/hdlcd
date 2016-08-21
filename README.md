# The HDLC Daemon (HDLCd)
The HDLCd implement the HDLC protocol to easily talk to devices connected via serial communications

This package offers the system deamon "HDLCd" that implements the "High-level Data Link Control" protocol (HDLC).
The purpose of this deamon is to easily use serial devices that make use of the HDLC protocol for communication.
This software is intended to be portable and makes use of the boost libraries. It was tested on GNU/Linux (GCC toolchain) and Microsoft Windows (nuwen MinGW).

The HDLCd itself offers simple TCP-based connectivity using a simple command- and framing structure.

Multiple client-like tools to access the HDLCd for multiple use-cases are in a seperate repository
- https://github.com/Strunzdesign/hdlcd-tools
- Install the HDLCd and its bundeled header files before compiling the software from the hdlcd-tools repository!

Releases:
- v1.1 from 16.08.2016
  - Fixed compiler error triggered with GCC 6.1 / nuwen MinGW 14.0

- v1.0 from 10.08.2016
  - First tested version without any open issues

Current state:
- Since 25.07.2016 the master branch is considered stable to be used for the s-net(r) devices of the Fraunhofer-Institute for Integrated Circuits IIS
- HDLCd v1.1 works well with s-net(r) BASE release 3.6
- HDLCd v1.1: Still no flow control and error correction for incoming I-frames (TAG to HDLCd via UART)
  - Due to limitations of broken HDLC in s-net(r) BASE release 3.6
  - Requires changes in BASE release 3.6
  - With these changes it won't be possible to use the old JAVA-based Gateway solution anymore!
  - Thus, the migration to the HDLCd must be completed before flow control and error correction can be added!
  - However, dropping the proprietry HDLC implementation in favor of vanilla HDLC would be the best solution.
- The hdlc-vanilla branch containing an early development stage regarding an implementation of HDLC that conforms to the standard is not ready yet.

Required libraries and tools:
- GCC, the only tested compiler collection thus far (tested: GCC 4.9.3, GCC 6.1)
- Boost, a platform-independent toolkit for development of C++ applications
- CMake, the build system
- Doxygen, for development
- nuwen MinGW, to compile the software on Microsoft Windows (tested: 13.4, 14.0)

See online doxygen documentation at http://strunzdesign.github.io/hdlcd/
