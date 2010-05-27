Odin II - Version 0.1 - Quick Start

In the DOCUMENTATION directory there is more thorough details about
Odin II.  This is a quick start guide.

INSTALL
------------

1. LIBVPR

This library needs to be built for your platform.  The Makefile for
Odin will link the LIBVPR library located in:
../libvpr_5 or/and ../libvpr_6

2. Odin II

Odin is compiled with a standard Makefile.  For the first time
I would suggest a:
make clean && make

Note: 
- that Odin relies on Bison and Flex.  These details are not 
included in this install and are left to the user.
- the default make targets VRP 5.0.  For VPR 6.0 you can either
change the Makefile variable "BUILD=" to VPR6 or you can type:
make clean && make BUILD=VPR6

USAGE
-------------

To run Odin II quickly once installed, go into the QUICK_TEST
directory and type:
./quick_test.bash

This will compile a simple verilog file and generate outputs
from Odin.  The most important file being test.blif, which
is the BLIF formatted version of test.v.

CONTACT
-------------

jamieson dot peter at gmail dot com
ken at unb dot ca
- We will service all requests as timely as possible, but
please explain the problem with enough detail to help. 
