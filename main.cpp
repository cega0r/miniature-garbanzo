#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <thread>
#include <cmath>
#include "proxy.h"
#include "chunk_parser.h"
#include "overlay.h"
#include "world_to_screen.h"
#include "camera_state.h"
#include "config.h"
#include "overlay_sync.h"

// ---------------------------------------------------------------------------
// Ore storage — keyed by chunk coordinate so stale chunks evict cleanly.
// Key = (chunkX << 32) | (uint32_t)chunkZ
// ---------------------------------------------------------------------------
static std::mutex                                        oreMutex;
static std::unordered_map<uint64_t, std::vector<OreBlock>> oreMap;

static uint64_t ChunkKey(int cx, int cz) {
    return (static_cast<uint64_t>(cx) << 32) |
            static_cast<uint64_t>(static_cast<uint32_t>(cz));
}

// ---------------------------------------------------------------------------
// Color and filter helpers
// ---------------------------------------------------------------------------
D2D1_COLOR_F OreColor(const std::string& n) {
    if (n.find("Diamond") != std::string::npos) return D2D1::ColorF(0.2f, 0.9f, 1.f);
    if (n.find("Emerald") != std::string::npos) return D2D1::ColorF(0.f,  1.f,  0.3f);
    if (n.find("Gold")    != std::string::npos) return D2D1::ColorF(1.f,  0.85f,0.f);
    if (n.find("Iron")    != std::string::npos) return D2D1::ColorF(0.8f, 0.6f, 0.4f);
    if (n.find("Redstone")!= std::string::npos) return D2D1::ColorF(1.f,  0.1f, 0.1f);
    if (n.find("Lapis")   != std::string::npos) return D2D1::ColorF(0.2f, 0.4f, 1.f);
    if (n.find("Ancient") != std::string::npos) return D2D1::ColorF(1.f,  0.5f, 0.f);
    if (n.find("Copper")  != std::string::npos) return D2D1::ColorF(0.8f, 0.5f, 0.3f);
    return D2D1::ColorF(0.5f, 0.5f, 0.5f);
}

bool OreEnabled(const std::string& n) {
    if (n.find("Coal")    != std::string::npos) return g_cfg.showCoal;
    if (n.find("Iron")    != std::string::npos) return g_cfg.showIron;
    if (n.find("Copper")  != std::string::npos) return g_cfg.showCopper;
    if (n.find("Gold")    != std::string::npos) return g_cfg.showGold;
    if (n.find("Lapis")   != std::string::npos) return g_cfg.showLapis;
    if (n.find("Redstone")!= std::string::npos) return g_cfg.showRedstone;
    if (n.find("Emerald") != std::string::npos) return g_cfg.showEmerald;
    if (n.find("Diamond") != std::string::npos) return g_cfg.showDiamond;
    if (n.find("Ancient") != std::string::npos) return g_cfg.showAncientDebris;
    return true;
}

// ---------------------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------------------
int main() {
    ShowWindow(GetConsoleWindow(), SW_HIDE);
    g_cfg.Load("xray_config.ini");

    // Wait for Minecraft to open
    HWND mcWin = nullptr;
    while (!mcWin) { mcWin = FindMinecraftWindow(); if (!mcWin) Sleep(1000); }

    RECT mcRect; GetWindowRect(mcWin, &mcRect);
    float sw = static_cast<float>(mcRect.right  - mcRect.left);
    float sh = static_cast<float>(mcRect.bottom - mcRect.top);
    float prevW = sw, prevH = sh;

    Overlay ov;
    ov.Init(mcWin);

    MinecraftProxy proxy;
    ChunkParser    parser;

    // ── Chunk data callback ─────────────────────────────────────────────────
    // Replaces all ores for that chunk so stale data never accumulates.
    proxy.onChunkData = [&](std::vector<uint8_t>& pkt) {
        // Collect new ores for this chunk separately first
        std::vector<OreBlock> fresh;
        int receivedCX = 0, receivedCZ = 0;
        bool firstOre = true;

        parser.Parse(pkt, [&](const OreBlock& ore) {
            if (!OreEnabled(ore.name)) return;
            if (firstOre) {
                receivedCX = ore.chunkX;
                receivedCZ = ore.chunkZ;
                firstOre   = false;
            }
            fresh.push_back(ore);
            if (g_cfg.soundOnFind &&
                (ore.name.find("Diamond") != std::string::npos ||
                 ore.name.find("Ancient") != std::string::npos))
                std::thread([] { Beep(1000, 80); Sleep(60); Beep(1200, 80); }).detach();
        });

        // Atomically replace the chunk's ore list
        std::lock_guard<std::mutex> lk(oreMutex);
        uint64_t key = ChunkKey(receivedCX, receivedCZ);
        if (fresh.empty())
            oreMap.erase(key);          // chunk unloaded or no ores — clean it out
        else
            oreMap[key] = std::move(fresh);
    };

    // ── Player position callback ────────────────────────────────────────────
    proxy.onPlayerPos = [](double x, double y, double z, float yaw, float pitch) {
        g_cam.Write(x, y + 1.62, z, yaw, pitch);
    };

    // ── Start proxy on background thread ───────────────────────────────────
    std::thread([&] {
        proxy.Start(g_cfg.serverHost, g_cfg.serverPort, g_cfg.localPort);
        proxy.RunLoop();
    }).detach();

    bool overlayVisible = true, toggleWasDown = false, minimized = false;
    MSG  msg{};

    while (true) {
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
            if (msg.message == WM_QUIT) goto cleanup;
        }

        if (!IsWindow(mcWin)) {
            mcWin = FindMinecraftWindow();
            if (!mcWin) { Sleep(500); continue; }
        }

        SyncOverlayToGame(ov.hwnd, mcWin, sw, sh, minimized);

        if ((sw != prevW || sh != prevH) && sw > 0 && sh > 0) {
            if (ov.target)
                ov.target->Resize(D2D1::SizeU(static_cast<UINT32>(sw),
                                               static_cast<UINT32>(sh)));
            prevW = sw; prevH = sh;
        }

        bool td = (GetAsyncKeyState(g_cfg.toggleKey) & 0x8000) != 0;
        if (td && !toggleWasDown) overlayVisible = !overlayVisible;
        toggleWasDown = td;

        if (!overlayVisible || minimized || !g_cam.hasData) { Sleep(16); continue; }

        float cx, cy, cz, cyaw, cpitch;
        g_cam.Read(cx, cy, cz, cyaw, cpitch);

        ov.BeginDraw();
        int dc = 0, ac = 0, ec = 0;

        {
            std::lock_guard<std::mutex> lk(oreMutex);
            for (auto& [key, ores] : oreMap) {
                for (auto& ore : ores) {
                    float dx   = static_cast<float>(ore.worldX()) - cx;
                    float dy   = static_cast<float>(ore.localY)   - cy;
                    float dz   = static_cast<float>(ore.worldZ()) - cz;
                    float dist = sqrtf(dx*dx + dy*dy + dz*dz);
                    if (dist > g_cfg.drawDistance) continue;

                    if (dist < 32.f) {
                        if (ore.name.find("Diamond") != std::string::npos) dc++;
                        if (ore.name.find("Ancient") != std::string::npos) ac++;
                        if (ore.name.find("Emerald") != std::string::npos) ec++;
                    }

                    float sx, sy;
                    if (!WorldToScreen(
                            static_cast<float>(ore.worldX()),
                            static_cast<float>(ore.localY),
                            static_cast<float>(ore.worldZ()),
                            cx, cy, cz,
                            cyaw, cpitch,
                            g_cfg.fovOverride,
                            sw, sh,
                            sx, sy)) continue;

                    float scale = 1.f - (dist / g_cfg.drawDistance) * 0.6f;
                    float bs    = 6.f * scale;
                    ov.DrawBox(sx - bs, sy - bs, bs * 2.f, bs * 2.f, OreColor(ore.name));

                    // NOTE: TextOutA inside a D2D1 BeginDraw/EndDraw block causes
                    // GDI / D2D render target conflicts and label flicker.
                    // To fix properly, initialize a IDWriteFactory + IDWriteTextFormat
                    // in Overlay::Init and call target->DrawText() here instead.
                    // Leaving the GDI path disabled until that refactor:
                    if (g_cfg.showLabels) {
                        // TODO: replace with DirectWrite — see overlay.h
                    }
                }
            }
        }

        // HUD — drawn after EndDraw via separate GDI pass to avoid D2D conflict
        ov.EndDraw();

        if (g_cfg.showHUD) {
            std::string hud = "Diamonds:" + std::to_string(dc) +
                              "  Debris:"  + std::to_string(ac) +
                              "  Emeralds:"+ std::to_string(ec);
            HDC hdc = GetDC(ov.hwnd);
            if (hdc) {
                SetTextColor(hdc, RGB(255, 255, 255));
                SetBkMode(hdc, TRANSPARENT);
                TextOutA(hdc, 10, 10, hud.c_str(), static_cast<int>(hud.size()));
                ReleaseDC(ov.hwnd, hdc);
            }
        }

        Sleep(16);
    }

cleanup:
    proxy.Stop();
    ov.Shutdown();
    return 0;
}
