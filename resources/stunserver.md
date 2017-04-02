% STUNSERVER(1) January 22, 2012 | User Manual

# NAME

stunserver \- STUN protocol service (RFCs: 3489, 5389, 5789, 5780)

# SYNOPSIS

**stunserver** [OPTIONS]

#DESCRIPTION

stunserver starts a STUN listening service that responds to STUN binding requests from remote
clients. Options are described below.

# OPTIONS

The following options are supported.

    --mode MODE
    --primaryinterface INTERFACE
    --altinterface INTERFACE
    --primaryport PORTNUMBER
    --altport PORTNUMBER
    --family IPVERSION
    --protocol PROTO
    --maxconn MAXCONN
    --verbosity LOGLEVEL
    --ddp
    --primaryadvertised
    --altadvertised
    --configfile
    --help

Details of each option are as follows.

**--mode** MODE

Where the MODE parameter specified is either "basic" or "full".
In basic mode, the server listens on a single port. Basic mode is sufficient for basic NAT
traversal scenarios in which a client needs to discover its external IP address
and obtain a port mapping for a local port it is listening on. The STUN
CHANGE-REQUEST attribute is not supported in basic mode.

In full mode, the STUN service listens on two different interfaces and two
different ports on each. A client binding request may specify an option
for the server to send the response back from one of the alternate
interfaces and/or ports.  Full mode facilitates clients attempting to
discover NAT behavior and NAT filtering behavior of the network they are on.
Full mode requires two unique IP addresses on the host. When run over TCP,
the service is not able to support a CHANGE-REQUEST attribute from 
the client.
 
If this parameter is not specified, basic mode is the default.

____


**--primaryinterface** INTERFACE

Where INTERFACE specified is either a local IP address (e.g. "192.168.1.2") 
of the host or the name of a network interface (e.g. "eth0").

The interface or address specified will be used by the service as the primary
listening address.

In basic mode, the default is to bind to all available adapters (INADDR_ANY).
In full mode, the default is to bind to the first non-localhost adapter with
a configured IP address.

____


**--altinterface** INTERFACE

Where INTERFACE specified is either a local IP address (e.g. "192.168.1.3") 
of the host or the name of a network interface (e.g. "eth1").

This parameter is nearly identical as the --primaryinterface option except
that it specifies the alternate listening address for full mode.

This option is ignored in basic mode. In full mode, the default is to bind
to the second non-localhost adapter with a configured IP address.

____


**--primaryport** PORTNUM

Where PORTNUM is a value between 1 to 65535.

This is the primary port the server will bind to for listening for incoming 
binding requests. The service will bind both the primary address and the
alternate address to this port.

The default is 3478.

____


**--altport** PORTNUM

Where PORTNUM is a value between 1 to 65535.

This is the alternate port the server will bind to for listening for incoming 
binding requests. The service will bind both the primary address and the
alternate address to this port.

This option is ignored in basic mode. The default is 3479.

____


**--family** IPVERSION

Where IPVERSION is either "4" or "6" to specify the usage of IPV4 or IPV6.

The default family is 4 for IPv4 usage.

____

**--protocol** PROTO

Where PROTO is either IP protocol, "udp" or "tcp".

udp is the default.

____


**--maxconn** MAXCONN

Where MAXCONN is a value between 1 and 100000. 

For TCP mode, this parameter specifies the maximum number of simultaneous
connections that can exist at any given time.

This parameter is ignored when the protocol is UDP. The default value is 1000

____

**--verbosity** LOGLEVEL

Where LOGLEVEL is a value greater than or equal to 0.

This parameter specifies how much is printed to the console with regards to
initialization, errors, and network activity.  A value of 0 specifies a
very minimal amount of output.  A value of 1 shows slightly more. A value of
2 shows even more. Specifying 3 will show a lot more.

The default is 0.

____

**--ddp**

The --ddp switch is for "Distributed Denial (of service) Protection".  Any client IP address that
floods the service with too many packets in a short interval is put into a "penalty box" that
will result in subsequent packets received from this IP to be dropped. The result is that
the client receives no response.

____

**--primaryadvertised** PRIMARY-IP

**--altadvertised** ALT-IP

Where PRIMARY-IP and ALT-IP are valid numeric IP address strings (e.g. "101.23.45.67") that
are the public IP addresses of the --primaryinterface and --altinterface addresses discussed
above.

These two parameters are for advanced usage only. It is intended for support of
running a STUN server in full mode on Amazon EC2 or other hosted environment
where the server is running behind a NAT. Do not set this parameter unless you
know specifically the effect it creates.

Normally, without these parameters being set, the ORIGIN attribute, OTHER-ADDRESS attribute, and
CHANGED-ADDRESS attributes are are determined by querying the local adapters or sockets
for the IP address they are listening on. When running the server in a NAT environment,
binding responses will still contain a correct set of mapping address attributes, such that P2P
connectivity may succeed.  However, the the ORIGIN, OTHER-ADDRESS,
and CHANGED-ADDRESS attributes sent by the server will be incorrect. The impact of sending an incorrect OTHER-ADDRESS or CHANGED-ADDRESS
will result in a client attempting to do NAT Behavior tests or NAT filtering tests to report an incorrect result.

For more details, visit www.stunprotocol.org for details on how to correctly set these parameters for use within Amazon EC2.

____

**--configfile** FILENAME

The --configfile switch allows the server to be configured with a JSON configuration file rather
that through command line parameters. If this switch is specified, most other command line parameters will be ignored.
(--verbosity is the only one honored).  Instead of configuring the server with command line parameters, the configuration
will be read from file. Since multiple configurations can be specified, this has the added advantage of allowing multiple
protocols and IP families to run within the same process (each in a separate thread). The fields of each configuration node
are named identical to the corresponding command line parameters (with the leading dashes removed). An example stun.conf
configuration file is shipped in the "testcode" folder of the source package

____

**--reuseaddr**

The --reuseaddr switch allows the STUN server port to be shared with other processes. This is useful for scenarios where another process needs to send from the STUN server port.

____

**--help**

Prints this help page


# EXAMPLES

stunserver
:   With no options, starts a basic STUN binding service on UDP port 3478.

stunserver --mode full --primaryinterface 128.34.56.78 --altinterface 128.34.56.79
:   Above example starts a dual-host STUN service on the the interfaces
    identified by the IP address "128.34.56.78" and "128.34.56.79". There are
    four UDP socket listeners


    128.34.56.78:3478 (Primary IP, Primary Port)
    128.34.56.78:3479 (Primary IP, Alternate Port)
    128.34.56.79:3478 (Primary IP, Primary Port)
    128.34.56.79:3479 (Alternate IP, Alternate Port)

An error occurs if the addresses specified do not exist on the local host
running the service.

stunserver --mode full --primaryinterface eth0 --altinterface eth1
:   Same as above, except the interfaces are specified by their names as
    enumerated by the system. The "ifconfig" or "ipconfig" command will
    enumerate available interface names.


# AUTHOR
john selbie (john@selbie.com)


