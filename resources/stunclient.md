% STUNCLIENT(1) January 22, 2012 | User Manual

# NAME

stunclient \- command line app for the STUN protocol

# SYNOPSIS

**stunclient** [OPTIONS] server [port]

#DESCRIPTION

stunclient attempts to discover the local host's own external IP address, obtain a
port mapping, and optionally discover properties of the Network Address Translator (NAT)
between the host and the the server.

# OPTIONS

The following options are supported.

    --mode MODE
    --localaddr INTERFACE
    --localport PORTNUMBER
    --family IPVERSION
    --protocol PROTO
    --verbosity LOGLEVEL
    --help

Details of each option and parameters are as follows.

**server**

The **server** parameter is the IP address or FQDN of the remote server to perform the binding tests with. It is the only required parameter.

_____

**port**

The **port** parameter is an optional parameter that can follow the server parameter. The default is 3478 for UDP and TCP.

_____


**--mode** MODE

Where MODE is either "basic", "behavior", or "filtering".

"basic" mode is the default and indicates that the
client should perform a STUN binding test only.

"behavior" mode indicates that the client should
attempt to diagnose NAT behavior and port mapping methodologies if the server supports this mode.

"filtering" mode indicates that the client should
attempt to diagnose NAT filtering methodologies if the server supports this mode.  The NAT filtering test is only supported for UDP.

"full" mode is a deprecated mode. It performs both a filtering and a behavior test together. Users
are encouraged to run these tests separately and to avoid using the same local port.

____


**--localaddr** INTERFACE or IPADDRESS

The value for this option may the name of an interface (such as "eth0" or "lo"). Or it may be
one of the available IP addresses assigned to a network interface present on the host (such as
"128.23.45.67"). The interface chosen will be the preferred address for sending and receiving
responses with the remote server. The default is to let the system decide which address to send
on and to listen for responses on all addresses (INADDR_ANY).

____


**--localport** PORTNUM

PORTNUM is a value between 1 to 65535. This is the UDP or TCP port that the primary and
alternate interfaces listen on as the primary port for binding requests. If not specified, a
randomly available port chosen by the system is used.

____

**--family** IPVERSION

IPVERSION is either "4" or "6" to specify the usage of IPV4 or IPV6. If not specified, the default value is "4".

____

**--protocol** PROTO

PROTO is either "udp" or "tcp". "udp" is the default if this parameter is not specified

____

**--verbosity** LOGLEVEL

Sets the verbosity of the logging level. 0 is the default (minimal output and logging). 1 shows slightly more. 2 and higher shows even more.

____

**--help**
Prints this help page

# EXAMPLES

stunclient stunserver.org 3478
:    Performs a simple binding test request with the server listening at "stunserver.org"

stunclient --mode filtering --localport 9999 12.34.56.78
:    Performs the NAT filtering tests from local port 9999 to the server listening at IP
     Address 12.34.56.78 (port 3478)

stunclient --mode behavior 12.34.56.78
:    Performs the NAT behavior and port mapping tests from a random local port to the server listening at IP
     Address 12.34.56.78 (port 3478)

stunclient --protocol tcp stun.selbie.com
:    Performs a simple binding test using TCP to server listening on the default port of 3478 at
     stun.selbie.com

# AUTHOR
john selbie (jselbie@gmail.com)

