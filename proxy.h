#pragma once
#include <winsock2.h>
#include <windows.h>
#include <ws2tcpip.h>
#include <string>
#include <vector>
#include <functional>
#include <thread>
#include <mutex>
#include <atomic>
#pragma comment(lib, "ws2_32.lib")

using PacketCallback    = std::function<void(std::vector<uint8_t>&)>;
using PlayerPosCallback = std::function<void(double,double,double,float,float)>;

// Connection phase shared between the two pipe threads
enum class ProxyPhase { HANDSHAKE, LOGIN, PLAY };

struct PipeState {
    std::mutex      mtx;
    ProxyPhase      phase            = ProxyPhase::HANDSHAKE;
    bool            compression      = false;
    int             threshold        = 0;
};

class MinecraftProxy {
public:
    SOCKET      listenSock = INVALID_SOCKET;
    SOCKET      clientSock = INVALID_SOCKET;
    SOCKET      serverSock = INVALID_SOCKET;
    std::string serverHost;
    int         serverPort = 25565;
    int         listenPort = 25566;

    PacketCallback    onChunkData;
    PlayerPosCallback onPlayerPos;

    bool Start(const std::string& host, int srvPort, int localPort = 25566);
    void RunLoop();
    void Stop();

private:
    std::atomic<bool> running{ false };

    void   PipeWithIntercept(SOCKET src, SOCKET dst, bool fromServer, PipeState& shared);
    std::vector<uint8_t> ReadPacket(SOCKET s);
    bool                 SendPacket(SOCKET s, const std::vector<uint8_t>& data);
    int                  ReadVarInt (const std::vector<uint8_t>& b, size_t& p);
    double               ReadDouble (const std::vector<uint8_t>& b, size_t& p);
    float                ReadFloat  (const std::vector<uint8_t>& b, size_t& p);
    std::vector<uint8_t> Decompress (const std::vector<uint8_t>& raw, int expectedSize);
};
