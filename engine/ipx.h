#ifndef IPX_H
#define IPX_H

// engine/ipx.h — small LAN networking layer over the Novell IPX API.
//
// This is the period-authentic transport for DOS multiplayer: real DOS games
// (DOOM, Duke3D, Warcraft, Descent, ...) used IPX, not a hand-rolled TCP/IP
// packet-driver stack. It runs under DOSBox-X's built-in IPX-over-UDP emulation
// (set ipx=true, then "IPXNET STARTSERVER" on one box and "IPXNET CONNECT <ip>"
// on the other — even both on one machine via 127.0.0.1, which the NE2000/UDP
// path could never do), and over a real IPX LAN. For internet play, relay with
// ipxbox (github.com/fragglet/ipxbox).
//
// The API below is a transport pair (netSendToPeer/netRecv/netPoll) plus
// zero-config BROADCAST DISCOVERY for pairing — netHost() vs netJoin(), no IP
// to type.
//
// 16-bit real mode and 32-bit DOS/4GW. The IPX entry point is reached by a
// real-mode FAR CALL (obtained via INT 2Fh/AX=7A00h); 32-bit builds reach the
// same entry through the DPMI bridge (INT 31h 0300h to fetch it, 0301h to
// far-call it), with ECBs/buffers in DOS conventional memory.

// ---- Tunables (compile-time) ----
#define NET_MAX_PACKET_BYTES    256     // payload limit per game packet

// ---- Discovery state (netDiscoveryStatus) ----
#define NET_DISC_IDLE       0   // not currently discovering
#define NET_DISC_SEARCHING  1   // beaconing, listening for peer
#define NET_DISC_PAIRED     2   // peer found; ready to play

// ---- Role after pairing (netRole) ----
#define NET_ROLE_NONE       0
#define NET_ROLE_MASTER     1   // we are the host (WAIT FOR CONNECTION)
#define NET_ROLE_SLAVE      2   // we are the joiner (MAKE CONNECTION)

// ---- Lifecycle ----

// Initialize networking. `socket` is the IPX socket number both peers use for
// game traffic (host byte order; e.g. 7777). Detects the IPX driver, opens the
// socket, posts the receive ECBs and reads our local node address.
// Returns 1 on success, 0 on failure (no IPX driver, socket busy, 32-bit, ...).
int  netInit(unsigned short socket);

// Release the socket and free resources. Safe even if netInit() returned 0.
void netShutdown(void);

// ---- Per-frame service ----

// Drive timed work: service received packets (re-post receive ECBs), and while
// discovering, periodically broadcast our beacon. Cheap when idle.
void netPoll(void);

// ---- Connection (broadcast discovery — zero config) ----

// Host: start beaconing as the host and listen for a joiner. Becomes MASTER
// once a joiner is found. Returns 1 if armed (network up), 0 otherwise.
int  netHost(void);

// Join: start beaconing as a joiner and listen for a host. Becomes SLAVE once a
// host is found. Returns 1 if armed (network up), 0 otherwise.
int  netJoin(void);

// Current discovery state: NET_DISC_IDLE / SEARCHING / PAIRED.
int  netDiscoveryStatus(void);

// Role decided at pairing time (host = MASTER, joiner = SLAVE). Valid once paired.
int  netRole(void);

// ---- Gameplay transport ----

// Send `len` bytes to the paired peer as a single IPX datagram.
// Returns 1 if handed to the driver, 0 otherwise (not paired, len too big, ...).
int  netSendToPeer(const void* data, unsigned int len);

// Pop one datagram off the receive queue. Returns bytes copied into `buf`, or 0
// if empty. `fromAddr` is accepted for source-call compatibility and may be
// NULL (the paired peer is the only valid sender, so it is otherwise unused).
// Non-blocking — never spins or waits.
int  netRecv(void* buf, unsigned int maxLen, void* fromAddr);

#endif
