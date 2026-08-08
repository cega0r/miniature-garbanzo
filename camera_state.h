#pragma once
#include <mutex>
#include <atomic>

struct CameraState {
    std::mutex mtx;
    double x     = 0.0;
    double y     = 64.0;
    double z     = 0.0;
    float  yaw   = 0.f;
    float  pitch = 0.f;
    float  fov   = 70.f;

    std::atomic<bool> hasData{ false };

    void Write(double px, double py, double pz, float pyaw, float ppitch) {
        std::lock_guard<std::mutex> lk(mtx);
        x = px; y = py; z = pz;
        yaw = pyaw; pitch = ppitch;
        hasData = true;
    }

    void Read(float& ox, float& oy, float& oz, float& oyaw, float& opitch) {
        std::lock_guard<std::mutex> lk(mtx);
        ox = (float)x; oy = (float)y; oz = (float)z;
        oyaw = yaw; opitch = pitch;
    }
};

extern CameraState g_cam;
