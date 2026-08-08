#pragma once
#include <vector>
#include <string>
#include <unordered_map>
#include <functional>

static const std::unordered_map<int,std::string> ORE_IDS = {
    {68,"Coal Ore"},{69,"Deepslate Coal"},
    {72,"Iron Ore"},{73,"Deepslate Iron"},
    {80,"Copper Ore"},{81,"Deepslate Copper"},
    {84,"Gold Ore"},{85,"Deepslate Gold"},
    {476,"Redstone Ore"},{477,"Deepslate Redstone"},
    {537,"Emerald Ore"},{538,"Deepslate Emerald"},
    {549,"Lapis Ore"},{550,"Deepslate Lapis"},
    {561,"Diamond Ore"},{562,"Deepslate Diamond"},
    {669,"Ancient Debris"},
};

struct OreBlock {
    int chunkX,chunkZ,localX,localY,localZ;
    std::string name;
    int worldX() const { return chunkX*16+localX; }
    int worldZ() const { return chunkZ*16+localZ; }
};

using OreFoundCallback = std::function<void(const OreBlock&)>;

class ChunkParser {
public:
    void Parse(const std::vector<uint8_t>& pkt, OreFoundCallback cb, int minY=-64);
private:
    int  ReadVarInt(const std::vector<uint8_t>& b, size_t& p);
    void DecodeSection(const std::vector<uint8_t>& b, size_t& p,
                       int cx, int cz, int sy, OreFoundCallback& cb);
};
