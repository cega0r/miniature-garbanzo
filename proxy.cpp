#include "proxy.h"
#include "camera_state.h"
#include <zlib.h>
#include <cstring>
#pragma comment(lib, "zlib.lib")

// ---------------------------------------------------------------------------
// VarInt helpers
// ---------------------------------------------------------------------------

// Read a VarInt directly from a socket, one byte at a time.
// Returns number of bytes consumed, or -1 on socket error / malformed data.
static int ReadVI_Sock(SOCKET s, int& out) {
    out = 0;
    int shift = 0;
    for (int i = 0; i < 5; ++i) {
        uint8_t b = 0;
        if (recv(s, reinterpret_cast<char*>(&b), 1, 0) != 1) return -1;
        out |= (b & 0x7F) << shift;
        if (!(b & 0x80)) return i + 1;
        shift += 7;
    }
    return -1; // VarInt longer than 5 bytes — protocol error
}

// Encode a VarInt and write it to a socket.
static bool WriteVI_Sock(SOCKET s, int val) {
    uint8_t buf[5];
    int n = 0;
    do {
        buf[n] = static_cast<uint8_t>(val & 0x7F);
        val >>= 7;
        if (val) buf[n] |= 0x80;
        n++;
    } while (val);
    return send(s, reinterpret_cast<char*>(buf), n, 0) == n;
}

// ---------------------------------------------------------------------------
// MinecraftProxy — public API
// ---------------------------------------------------------------------------

bool MinecraftProxy::Start(const std::string& host, int srvPort, int localPort) {
    serverHost = host;
    serverPort = srvPort;
    listenPort = localPort;

    WSADATA wsa{};
    WSAStartup(MAKEWORD(2, 2), &wsa);

    listenSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenSock == INVALID_SOCKET) return false;

    BOOL reuse = TRUE;
    setsockopt(listenSock, SOL_SOCKET, SO_REUSEADDR,
               reinterpret_cast<char*>(&reuse), sizeof(reuse));

    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons(static_cast<u_short>(localPort));

    if (bind(listenSock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR) return false;
    if (listen(listenSock, 1) == SOCKET_ERROR) return false;

    running = true;
    return true;
}

void MinecraftProxy::RunLoop() {
    sockaddr_in cliAddr{};
    int cliLen = sizeof(cliAddr);
    clientSock = accept(listenSock, reinterpret_cast<sockaddr*>(&cliAddr), &cliLen);
    if (clientSock == INVALID_SOCKET) return;

    // Resolve the real server and connect
    addrinfo hints{};
    hints.ai_family   = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    addrinfo* result  = nullptr;
    if (getaddrinfo(serverHost.c_str(),
                    std::to_string(serverPort).c_str(),
                    &hints, &result) != 0 || !result) {
        closesocket(clientSock);
        return;
    }

    serverSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (connect(serverSock, result->ai_addr, static_cast<int>(result->ai_addrlen)) == SOCKET_ERROR) {
        freeaddrinfo(result);
        closesocket(clientSock);
        return;
    }
    freeaddrinfo(result);

    // Both pipe directions share login/compression state
    PipeState shared;

    // Client → server: we only need to track the handshake next-state field
    // so the server→client thread knows when we've entered PLAY.
    std::thread clientThread([this, &shared] {
        PipeWithIntercept(clientSock, serverSock, false, shared);
    });

    // Server → client: where all the interesting packet interception happens
    PipeWithIntercept(serverSock, clientSock, true, shared);

    clientThread.join();
}

void MinecraftProxy::Stop() {
    running = false;
    if (listenSock != INVALID_SOCKET) { closesocket(listenSock); listenSock = INVALID_SOCKET; }
    if (clientSock != INVALID_SOCKET) { closesocket(clientSock); clientSock = INVALID_SOCKET; }
    if (serverSock != INVALID_SOCKET) { closesocket(serverSock); serverSock = INVALID_SOCKET; }
    WSACleanup();
}

// ---------------------------------------------------------------------------
// Packet framing
// ---------------------------------------------------------------------------

// Read one length-prefixed packet from the socket.
// Returns an empty vector on disconnect or error.
std::vector<uint8_t> MinecraftProxy::ReadPacket(SOCKET s) {
    int len = 0;
    if (ReadVI_Sock(s, len) < 0 || len <= 0 || len > 0x200000) return {};

    std::vector<uint8_t> buf(static_cast<size_t>(len));
    int total = 0;
    while (total < len) {
        int r = recv(s, reinterpret_cast<char*>(buf.data()) + total, len - total, 0);
        if (r <= 0) return {};
        total += r;
    }
    return buf;
}

// Write a length-prefixed packet to the socket.
bool MinecraftProxy::SendPacket(SOCKET s, const std::vector<uint8_t>& data) {
    if (!WriteVI_Sock(s, static_cast<int>(data.size()))) return false;
    int total = 0;
    while (total < static_cast<int>(data.size())) {
        int r = send(s,
                     reinterpret_cast<const char*>(data.data()) + total,
                     static_cast<int>(data.size()) - total,
                     0);
        if (r <= 0) return false;
        total += r;
    }
    return true;
}

// ---------------------------------------------------------------------------
// Buffer parsing helpers
// ---------------------------------------------------------------------------

int MinecraftProxy::ReadVarInt(const std::vector<uint8_t>& b, size_t& p) {
    int r = 0, s = 0;
    while (p < b.size()) {
        uint8_t byte = b[p++];
        r |= (byte & 0x7F) << s;
        if (!(byte & 0x80)) break;
        s += 7;
    }
    return r;
}

double MinecraftProxy::ReadDouble(const std::vector<uint8_t>& b, size_t& p) {
    uint64_t v = 0;
    for (int i = 0; i < 8; ++i) v = (v << 8) | b[p++];
    double d;
    memcpy(&d, &v, 8);
    return d;
}

float MinecraftProxy::ReadFloat(const std::vector<uint8_t>& b, size_t& p) {
    uint32_t v = 0;
    for (int i = 0; i < 4; ++i) v = (v << 8) | b[p++];
    float f;
    memcpy(&f, &v, 4);
    return f;
}

// ---------------------------------------------------------------------------
// Zlib decompression (used when the server enables packet compression)
// ---------------------------------------------------------------------------

std::vector<uint8_t> MinecraftProxy::Decompress(const std::vector<uint8_t>& raw,
                                                  int expectedSize) {
    std::vector<uint8_t> out(static_cast<size_t>(expectedSize));
    uLongf outLen = static_cast<uLongf>(expectedSize);
    if (uncompress(out.data(), &outLen, raw.data(), static_cast<uLong>(raw.size())) != Z_OK)
        return {};
    out.resize(outLen);
    return out;
}

// ---------------------------------------------------------------------------
// Core bidirectional pipe — intercepts chunk data and player position
// ---------------------------------------------------------------------------
//
// Minecraft 1.21.4 (protocol 769) play-state clientbound packet IDs:
//   0x27  Chunk Data and Update Light
//   0x40  Synchronize Player Position
//
// NOTE: Online-mode servers encrypt the stream after the Encryption Response
//       handshake (AES/CFB8). This implementation handles offline-mode only.
//       To support online servers you'd need to intercept the Encryption
//       Request (login 0x01), perform the key exchange, and wrap both sockets
//       in AES decrypt/encrypt streams before re-entering this pipe.
//
void MinecraftProxy::PipeWithIntercept(SOCKET src, SOCKET dst,
                                        bool fromServer, PipeState& shared) {
    while (running) {
        // ── 1. Read raw length-prefixed packet from source ─────────────────
        auto raw = ReadPacket(src);
        if (raw.empty()) break;

        // ── 2. Immediately forward the ORIGINAL bytes — no modification ────
        if (!SendPacket(dst, raw)) break;

        // ── 3. Decompress and parse a working copy for interception ────────
        std::vector<uint8_t> payload;

        {
            std::lock_guard<std::mutex> lk(shared.mtx);

            if (shared.compression) {
                // Compressed framing: VarInt(dataLength) then either
                // zlib-compressed bytes (if dataLength > 0) or raw bytes.
                size_t pp = 0;
                int dataLen = ReadVarInt(raw, pp);
                if (dataLen > 0) {
                    // Compressed — inflate it
                    std::vector<uint8_t> compressed(raw.begin() + static_cast<int>(pp), raw.end());
                    payload = Decompress(compressed, dataLen);
                    if (payload.empty()) continue; // inflate failed, skip
                } else {
                    // Below threshold — uncompressed, strip the leading 0x00 byte
                    payload.assign(raw.begin() + static_cast<int>(pp), raw.end());
                }
            } else {
                payload = raw;
            }
        }

        if (payload.empty()) continue;

        size_t p   = 0;
        int  pid   = ReadVarInt(payload, p);

        // ── 4. Client → Server: track handshake next-state only ────────────
        if (!fromServer) {
            std::lock_guard<std::mutex> lk(shared.mtx);
            if (shared.phase == ProxyPhase::HANDSHAKE && pid == 0x00) {
                // Handshake packet: protocol(VarInt), host(String), port(u16), nextState(VarInt)
                ReadVarInt(payload, p);                          // protocol version
                int slen = ReadVarInt(payload, p); p += slen;   // server address string
                p += 2;                                          // server port (uint16)
                int nextState = ReadVarInt(payload, p);
                shared.phase = (nextState == 2) ? ProxyPhase::LOGIN : ProxyPhase::HANDSHAKE;
            }
            continue;
        }

        // ── 5. Server → Client: update shared state + intercept ───────────
        {
            std::lock_guard<std::mutex> lk(shared.mtx);

            if (shared.phase == ProxyPhase::LOGIN) {
                if (pid == 0x03) {
                    // Set Compression — threshold of -1 means disabled
                    shared.threshold   = ReadVarInt(payload, p);
                    shared.compression = (shared.threshold >= 0);
                }
                else if (pid == 0x02) {
                    // Login Success — transition to PLAY state
                    shared.phase = ProxyPhase::PLAY;
                }
                // 0x01 = Encryption Request: server is online-mode.
                // We can't intercept further without AES. Future work.
            }
            else if (shared.phase == ProxyPhase::PLAY) {

                // ── Chunk Data and Update Light (0x27 in 1.21.4) ──────────
                if (pid == 0x27 && onChunkData) {
                    // Pass the full decompressed payload (parser reads pkt ID first)
                    onChunkData(payload);
                }

                // ── Synchronize Player Position (0x40 in 1.21.4) ──────────
                // 1.21.4 format:
                //   teleport_id  VarInt
                //   x, y, z      Double × 3
                //   vx, vy, vz   Double × 3   ← added in 1.20.6
                //   yaw          Float
                //   pitch        Float
                //   flags        VarInt  (bitmask: which fields are relative)
                else if (pid == 0x40 && onPlayerPos) {
                    if (p + 52 <= payload.size()) {  // 4+8+8+8+8+8+8+4+4 = 60 minus VarInt slop
                        ReadVarInt(payload, p);                // teleport ID
                        double x     = ReadDouble(payload, p);
                        double y     = ReadDouble(payload, p);
                        double z     = ReadDouble(payload, p);
                        ReadDouble(payload, p);                // vx
                        ReadDouble(payload, p);                // vy
                        ReadDouble(payload, p);                // vz
                        float  yaw   = ReadFloat (payload, p);
                        float  pitch = ReadFloat (payload, p);
                        // flags VarInt — bits 0-4 indicate relative coords, we ignore here
                        onPlayerPos(x, y, z, yaw, pitch);
                    }
                }
            }
        }
    }
}
