// engine/ipx.c — LAN networking over the Novell IPX API.
//
// Period-authentic DOS multiplayer transport, the way id's DOOM did it
// (ipx/IPXNET.C). Runs under DOSBox-X's built-in IPX-over-UDP (ipx=true + IPXNET
// STARTSERVER/CONNECT) and over a real IPX LAN.
//
// Model: zero-config peer pairing by BROADCAST discovery. One side hosts
// (netHost -> MASTER), the other joins (netJoin -> SLAVE); both beacon on the
// game socket until they hear the complementary role, then exchange unicast game
// datagrams. The receive path is POLLED (ESRAddress = 0, no async upcall):
// netPoll() scans the listen ECBs' InUseFlag.
//
// Builds for BOTH 16-bit real mode and 32-bit DOS/4GW. The protocol/state code
// below is shared; only the low-level driver access forks on __386__:
//   - 16-bit: the IPX entry (INT 2Fh/AX=7A00h) is far-called directly via
//     #pragma aux; ECBs/buffers live in DGROUP.
//   - 32-bit: the real-mode IPX entry is reached through the DPMI bridge —
//     INT 31h 0300h to fetch the entry, 0301h to far-call it — and ECBs/buffers
//     live in DOS conventional memory (dpmiAllocDos), polled from the flat
//     selector. Because receive is polled, no DPMI 0303h callback is needed.

#include "ipx.h"
#include <string.h>     // memcpy, memset, memcmp
#include "black8.h"     // timerQuery (18.2 Hz tick counter)

// ---- IPX wire structures (byte-exact; verified against DOOM ipx/IPXNET.H) ----
#pragma pack(push, 1)

typedef struct {                    // Event Control Block (42 bytes, 1 fragment)
    unsigned short Link[2];         //  0  far ptr (off,seg)
    unsigned short ESRAddress[2];   //  4  far ptr; 0 = polled (no upcall)
    unsigned char  InUseFlag;       //  8  nonzero while the driver owns the ECB
    unsigned char  CompletionCode;  //  9  0 = success
    unsigned short Socket;          // 10  BIG-ENDIAN
    unsigned char  IPXWorkspace[4]; // 12
    unsigned char  DriverWorkspace[12]; // 16
    unsigned char  ImmediateAddress[6]; // 28  next-hop node (= dest node on 1 segment)
    unsigned short FragmentCount;   // 34
    unsigned short fAddress[2];     // 36  fragment 0 far ptr (off,seg)
    unsigned short fSize;           // 40  fragment 0 size
} ECB;

typedef struct {                    // IPX header (30 bytes, all numerics BIG-ENDIAN)
    unsigned short Checksum;        //  0  0xFFFF = none (driver fills)
    unsigned short Length;          //  2  header+data (driver fills on send)
    unsigned char  TransportControl;//  4  hop count (driver)
    unsigned char  PacketType;      //  5  4 = PEP datagram
    unsigned char  DestNetwork[4];  //  6
    unsigned char  DestNode[6];     // 10
    unsigned short DestSocket;      // 16  BIG-ENDIAN
    unsigned char  SourceNetwork[4];// 18
    unsigned char  SourceNode[6];   // 22
    unsigned short SourceSocket;    // 28  BIG-ENDIAN
} IPXHeader;

#define IPX_DATA_MAX 512
typedef struct {                    // contiguous header+data = one fragment
    IPXHeader     ipx;
    unsigned char data[IPX_DATA_MAX];
} IPXPacket;

#pragma pack(pop)

// ---- App-level payload tags (first data byte) ----
#define TAG_HELLO       0xB1        // discovery beacon: [B1][role]
#define TAG_DATA        0xB2        // game datagram:    [B2][len][bytes...]
#define HELLO_ROLE_HOST 1
#define HELLO_ROLE_JOIN 2

#define RX_ECBS 6                   // posted listen ECBs
#define RXQ     8                   // decoded game-datagram ring slots

// ---- buffers: pointers + their real-mode seg:off (filled by ipxSetupBuffers) ----
// 16-bit: pointers reference DGROUP statics. 32-bit: they reference DOS memory
// via the flat selector. Either way the protocol code below is identical.
static ECB*           TxEcb;
static IPXPacket*     TxPkt;
static ECB*           RxEcb[RX_ECBS];
static IPXPacket*     RxPkt[RX_ECBS];
static unsigned char* AddrBuf;                      // 10 bytes: net(4)+node(6) scratch

static unsigned short TxEcbSeg, TxEcbOff;
static unsigned short TxPktSeg, TxPktOff;
static unsigned short RxEcbSeg[RX_ECBS], RxEcbOff[RX_ECBS];
static unsigned short RxPktSeg[RX_ECBS], RxPktOff[RX_ECBS];
static unsigned short AddrBufSeg, AddrBufOff;

// ---- module state ----
static int            Inited     = 0;
static unsigned short MySocketBE = 0;
static unsigned char  LocalNet[4];
static unsigned char  LocalNode[6];

static unsigned char  RxQData[RXQ][NET_MAX_PACKET_BYTES];
static int            RxQLen[RXQ];
static int            RxQHead, RxQTail;

static int            DiscStatus     = NET_DISC_IDLE;
static int            DiscRoleIntent = NET_ROLE_NONE;   // MASTER (host) / SLAVE (join)
static int            Role           = NET_ROLE_NONE;
static unsigned char  PeerNet[4];
static unsigned char  PeerNode[6];
static unsigned long  LastBeacon     = 0;

static unsigned short swap16(unsigned short v) {
    return (unsigned short)((v << 8) | (v >> 8));
}

// Forked low-level driver access. Each side provides:
//   int  ipxGetEntry(void);                              detect IPX + cache entry
//   unsigned char ipxCall(unsigned func, seg, off);      BX=func, ES:SI=seg:off -> AL
//   unsigned char ipxOpen(unsigned short socketBE);      func 0 (AL=0 short-lived)
//   void ipxClose(unsigned short socketBE);              func 1
//   int  ipxSetupBuffers(void);                          alloc + fill the pointers/seg/off
//   void ipxReleaseBuffers(void);
// Function 9 (Get Internetwork Address) and 0Ah (Relinquish Control) ride on
// ipxCall directly.

#if defined(__386__)
// =========================================================================
// 32-bit DOS/4GW: reach the real-mode IPX entry through the DPMI bridge.
// =========================================================================
#include "dpmi.h"

static unsigned short EntrySeg, EntryOff;
static unsigned short DosSeg, DosSel;               // DOS conventional buffer block

static int ipxGetEntry(void) {
    DpmiRealModeRegs r;
    memset(&r, 0, sizeof(r));
    r.eax = 0x7A00;
    dpmiRealModeInt(0x2F, &r);                      // INT 2Fh in real mode
    if ((r.eax & 0xFF) != 0xFF) {
        return 0;                                   // AL != FF -> IPX not present
    }
    EntrySeg = r.es;
    EntryOff = (unsigned short)r.edi;
    return 1;
}

static unsigned char ipxCall(unsigned func, unsigned short seg, unsigned short off) {
    DpmiRealModeRegs r;
    memset(&r, 0, sizeof(r));
    r.ebx = func;
    r.es  = seg;
    r.esi = off;
    r.cs  = EntrySeg;                               // far-call target = IPX entry
    r.ip  = EntryOff;
    dpmiCallRealFar(&r);
    return (unsigned char)(r.eax & 0xFF);
}

static unsigned char ipxOpen(unsigned short socketBE) {
    DpmiRealModeRegs r;
    memset(&r, 0, sizeof(r));
    r.eax = 0;                                      // AL = 0 (short-lived), func 0 in BX
    r.ebx = 0;
    r.edx = socketBE;
    r.cs  = EntrySeg;
    r.ip  = EntryOff;
    dpmiCallRealFar(&r);
    return (unsigned char)(r.eax & 0xFF);
}

static void ipxClose(unsigned short socketBE) {
    DpmiRealModeRegs r;
    memset(&r, 0, sizeof(r));
    r.ebx = 1;
    r.edx = socketBE;
    r.cs  = EntrySeg;
    r.ip  = EntryOff;
    dpmiCallRealFar(&r);
}

static int ipxSetupBuffers(void) {
    unsigned long base, off;
    unsigned int  needed, para;
    int           i;

    needed = sizeof(ECB) + sizeof(IPXPacket)
           + RX_ECBS * (sizeof(ECB) + sizeof(IPXPacket)) + 16;
    para = (needed + 15) >> 4;
    if (!dpmiAllocDos(para, &DosSeg, &DosSel)) {
        return 0;
    }
    base = (unsigned long)DosSeg << 4;              // flat linear address of the block
    off  = 0;

    TxEcb = (ECB*)(base + off);       TxEcbSeg = DosSeg; TxEcbOff = (unsigned short)off; off += sizeof(ECB);
    TxPkt = (IPXPacket*)(base + off); TxPktSeg = DosSeg; TxPktOff = (unsigned short)off; off += sizeof(IPXPacket);
    for (i = 0; i < RX_ECBS; i++) {
        RxEcb[i] = (ECB*)(base + off);       RxEcbSeg[i] = DosSeg; RxEcbOff[i] = (unsigned short)off; off += sizeof(ECB);
        RxPkt[i] = (IPXPacket*)(base + off); RxPktSeg[i] = DosSeg; RxPktOff[i] = (unsigned short)off; off += sizeof(IPXPacket);
    }
    AddrBuf = (unsigned char*)(base + off); AddrBufSeg = DosSeg; AddrBufOff = (unsigned short)off;
    return 1;
}

static void ipxReleaseBuffers(void) {
    dpmiFreeDos(DosSel);
}

#else
// =========================================================================
// 16-bit real mode: far-call the IPX entry directly; buffers in DGROUP.
// =========================================================================
#include <i86.h>        // FP_OFF, FP_SEG, MK_FP, __far

// IPXEntry holds the real-mode far entry (off:seg); the pragmas below issue
// "call dword ptr [IPXEntry]" — the Watcom equivalent of DOOM's Borland thunk.
static void (__far *IPXEntry)(void) = 0;

extern unsigned char ipxInstallCheck(void);
#pragma aux ipxInstallCheck =   \
    "mov ax,7A00h"              \
    "int 2Fh"                   \
    value [al]                  \
    modify [ax bx es di];

extern void __far *ipxGetEntryFar(void);
#pragma aux ipxGetEntryFar =    \
    "mov ax,7A00h"              \
    "int 2Fh"                   \
    "mov dx,es"                 \
    "mov ax,di"                 \
    value [dx ax]               \
    modify [ax bx es di];

// BX = function, ES:SI -> ECB (far ptr). Returns AL (completion/status).
extern unsigned char ipxCallFar(unsigned func, void __far *ecb);
#pragma aux ipxCallFar =             \
    "call dword ptr [IPXEntry]"     \
    parm [bx] [es si]               \
    value [al]                      \
    modify [ax bx cx dx si di es];

extern unsigned char ipxOpen(unsigned short socketBE);   // func 0, AL=0 short-lived
#pragma aux ipxOpen =                \
    "xor al,al"                     \
    "mov bx,0"                      \
    "call dword ptr [IPXEntry]"     \
    parm [dx]                       \
    value [al]                      \
    modify [ax bx cx dx si di es];

extern void ipxClose(unsigned short socketBE);           // func 1
#pragma aux ipxClose =               \
    "mov bx,1"                      \
    "call dword ptr [IPXEntry]"     \
    parm [dx]                       \
    modify [ax bx cx dx si di es];

static unsigned char ipxCall(unsigned func, unsigned short seg, unsigned short off) {
    return ipxCallFar(func, MK_FP(seg, off));
}

static int ipxGetEntry(void) {
    void __far* e;
    if (ipxInstallCheck() != 0xFF) {
        return 0;
    }
    e = ipxGetEntryFar();
    IPXEntry = e;
    return 1;
}

// DGROUP-resident buffers (stable addresses the driver can dereference).
static ECB          TxEcbS;
static IPXPacket    TxPktS;
static ECB          RxEcbS[RX_ECBS];
static IPXPacket    RxPktS[RX_ECBS];
static unsigned char AddrBufS[10];

static int ipxSetupBuffers(void) {
    void __far* f;
    int         i;

    f = (void __far*)&TxEcbS; TxEcb = &TxEcbS; TxEcbSeg = FP_SEG(f); TxEcbOff = FP_OFF(f);
    f = (void __far*)&TxPktS; TxPkt = &TxPktS; TxPktSeg = FP_SEG(f); TxPktOff = FP_OFF(f);
    for (i = 0; i < RX_ECBS; i++) {
        f = (void __far*)&RxEcbS[i]; RxEcb[i] = &RxEcbS[i]; RxEcbSeg[i] = FP_SEG(f); RxEcbOff[i] = FP_OFF(f);
        f = (void __far*)&RxPktS[i]; RxPkt[i] = &RxPktS[i]; RxPktSeg[i] = FP_SEG(f); RxPktOff[i] = FP_OFF(f);
    }
    f = (void __far*)AddrBufS; AddrBuf = AddrBufS; AddrBufSeg = FP_SEG(f); AddrBufOff = FP_OFF(f);
    return 1;
}

static void ipxReleaseBuffers(void) {
    // nothing to free — buffers are DGROUP statics
}

#endif // __386__

// ---- shared protocol layer (build-agnostic) ----

// Send a payload to an explicit dest (node/immediate). Blocks to completion —
// sends are tiny and infrequent (one keystate datagram per frame).
static int ipxSendBytes(const unsigned char* dnet, const unsigned char* dnode,
                        const unsigned char* imm, const unsigned char* payload, int plen) {
    unsigned long start;

    if (plen < 0 || plen > IPX_DATA_MAX) {
        return 0;
    }

    memset(&TxPkt->ipx, 0, sizeof(IPXHeader));
    TxPkt->ipx.PacketType = 4;                     // PEP; driver fills checksum/length/source
    memcpy(TxPkt->ipx.DestNetwork, dnet,  4);
    memcpy(TxPkt->ipx.DestNode,    dnode, 6);
    TxPkt->ipx.DestSocket = MySocketBE;
    memcpy(TxPkt->data, payload, plen);

    memset(TxEcb, 0, sizeof(ECB));
    TxEcb->Socket = MySocketBE;
    memcpy(TxEcb->ImmediateAddress, imm, 6);
    TxEcb->FragmentCount = 1;
    TxEcb->fAddress[0] = TxPktOff;
    TxEcb->fAddress[1] = TxPktSeg;
    TxEcb->fSize = (unsigned short)(sizeof(IPXHeader) + plen);

    ipxCall(3, TxEcbSeg, TxEcbOff);               // SendPacket (async)

    start = timerQuery();
    while (TxEcb->InUseFlag != 0) {               // poll to completion
        ipxCall(0x0A, 0, 0);                      // RelinquishControl
        if ((timerQuery() - start) > 18UL) {      // ~1s safety net
            break;
        }
    }
    return (TxEcb->CompletionCode == 0);
}

// (Re)post listen ECB i so the driver delivers the next packet into RxPkt[i].
static void postRx(int i) {
    memset(RxEcb[i], 0, sizeof(ECB));
    RxEcb[i]->Socket = MySocketBE;
    RxEcb[i]->FragmentCount = 1;
    RxEcb[i]->fAddress[0] = RxPktOff[i];
    RxEcb[i]->fAddress[1] = RxPktSeg[i];
    RxEcb[i]->fSize = (unsigned short)sizeof(IPXPacket);
    ipxCall(4, RxEcbSeg[i], RxEcbOff[i]);         // ListenForPacket
}

static void sendBeacon(void) {
    unsigned char buf[2];
    unsigned char bnode[6];

    memset(bnode, 0xFF, 6);                        // broadcast node + immediate
    buf[0] = TAG_HELLO;
    buf[1] = (unsigned char)((DiscRoleIntent == NET_ROLE_MASTER) ? HELLO_ROLE_HOST
                                                                 : HELLO_ROLE_JOIN);
    ipxSendBytes(LocalNet, bnode, bnode, buf, 2);
}

static void handleHello(IPXPacket* p, int dlen) {
    unsigned char role;

    if (dlen < 2) {
        return;
    }
    role = p->data[1];

    if (DiscStatus != NET_DISC_SEARCHING) {
        return;
    }
    // Pair host<->join only; role is fixed by which menu item we chose.
    if ((DiscRoleIntent == NET_ROLE_MASTER && role == HELLO_ROLE_JOIN) ||
        (DiscRoleIntent == NET_ROLE_SLAVE  && role == HELLO_ROLE_HOST)) {
        memcpy(PeerNet,  p->ipx.SourceNetwork, 4);
        memcpy(PeerNode, p->ipx.SourceNode, 6);
        Role       = DiscRoleIntent;
        DiscStatus = NET_DISC_PAIRED;
        sendBeacon();                             // help the peer pair promptly
    }
}

static void handleRx(int i) {
    IPXPacket*    p = RxPkt[i];
    int           dlen;
    unsigned char tag;

    // DOSBox loops our own broadcast back to us — ignore it.
    if (memcmp(p->ipx.SourceNode, LocalNode, 6) == 0) {
        return;
    }

    dlen = (int)swap16(p->ipx.Length) - (int)sizeof(IPXHeader);
    if (dlen < 1) {
        return;
    }
    tag = p->data[0];

    if (tag == TAG_HELLO) {
        handleHello(p, dlen);
    } else if (tag == TAG_DATA) {
        int n;
        int nxt;
        if (dlen < 2) {
            return;
        }
        n = p->data[1];                           // explicit length (exact, no padding)
        if (n > dlen - 2)            n = dlen - 2;
        if (n > NET_MAX_PACKET_BYTES) n = NET_MAX_PACKET_BYTES;
        // Only accept game data from our paired peer.
        if (DiscStatus != NET_DISC_PAIRED ||
            memcmp(p->ipx.SourceNode, PeerNode, 6) != 0) {
            return;
        }
        nxt = (RxQHead + 1) % RXQ;
        if (nxt != RxQTail) {                     // drop if ring full
            memcpy(RxQData[RxQHead], p->data + 2, n);
            RxQLen[RxQHead] = n;
            RxQHead = nxt;
        }
    }
}

// ---- public API ----

int netInit(unsigned short socket) {
    int i;

    Inited     = 0;
    DiscStatus = NET_DISC_IDLE;
    Role       = NET_ROLE_NONE;
    RxQHead    = RxQTail = 0;

    if (!ipxGetEntry()) {                          // no IPX driver present
        return 0;
    }
    MySocketBE = swap16(socket);
    if (ipxOpen(MySocketBE) != 0) {                // 0 = opened OK
        return 0;
    }
    if (!ipxSetupBuffers()) {                       // alloc + wire up the buffers
        return 0;
    }

    memset(AddrBuf, 0, 10);
    ipxCall(9, AddrBufSeg, AddrBufOff);            // Get Internetwork Address
    memcpy(LocalNet,  AddrBuf,     4);
    memcpy(LocalNode, AddrBuf + 4, 6);

    for (i = 0; i < RX_ECBS; i++) {
        postRx(i);
    }

    Inited = 1;
    return 1;
}

void netShutdown(void) {
    if (!Inited) {
        return;
    }
    ipxClose(MySocketBE);                          // cancels all ECBs on the socket
    ipxReleaseBuffers();
    Inited     = 0;
    DiscStatus = NET_DISC_IDLE;
}

void netPoll(void) {
    int i;

    if (!Inited) {
        return;
    }
    for (i = 0; i < RX_ECBS; i++) {
        if (RxEcb[i]->InUseFlag == 0) {           // driver finished with this ECB
            if (RxEcb[i]->CompletionCode == 0) {
                handleRx(i);
            }
            postRx(i);                            // re-arm
        }
    }
    if (DiscStatus == NET_DISC_SEARCHING) {
        unsigned long now = timerQuery();
        if ((now - LastBeacon) >= 9) {            // ~2 beacons/sec
            sendBeacon();
            LastBeacon = now;
        }
    }
}

int netHost(void) {
    if (!Inited) {
        return 0;
    }
    DiscRoleIntent = NET_ROLE_MASTER;
    Role           = NET_ROLE_NONE;
    DiscStatus     = NET_DISC_SEARCHING;
    LastBeacon     = 0;
    sendBeacon();
    return 1;
}

int netJoin(void) {
    if (!Inited) {
        return 0;
    }
    DiscRoleIntent = NET_ROLE_SLAVE;
    Role           = NET_ROLE_NONE;
    DiscStatus     = NET_DISC_SEARCHING;
    LastBeacon     = 0;
    sendBeacon();
    return 1;
}

int netDiscoveryStatus(void) { return DiscStatus; }
int netRole(void)            { return Role; }

int netSendToPeer(const void* data, unsigned int len) {
    unsigned char buf[2 + NET_MAX_PACKET_BYTES];

    if (DiscStatus != NET_DISC_PAIRED) {
        return 0;
    }
    if (len > NET_MAX_PACKET_BYTES) {
        return 0;
    }
    buf[0] = TAG_DATA;
    buf[1] = (unsigned char)len;
    memcpy(buf + 2, data, len);
    return ipxSendBytes(PeerNet, PeerNode, PeerNode, buf, (int)(2 + len));
}

int netRecv(void* buf, unsigned int maxLen, void* fromAddr) {
    int n;

    (void)fromAddr;
    if (!Inited || RxQTail == RxQHead) {
        return 0;
    }
    n = RxQLen[RxQTail];
    if ((unsigned int)n > maxLen) {
        n = (int)maxLen;
    }
    memcpy(buf, RxQData[RxQTail], n);
    RxQTail = (RxQTail + 1) % RXQ;
    return n;
}
