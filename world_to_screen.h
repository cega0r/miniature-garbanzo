#pragma once
#include <cmath>

inline bool WorldToScreen(
    float wx, float wy, float wz,
    float camX, float camY, float camZ,
    float yawDeg, float pitchDeg,
    float fovDeg,
    float screenW, float screenH,
    float& outX, float& outY)
{
    float dx=wx-camX, dy=wy-camY, dz=wz-camZ;
    float yaw  =yawDeg  *3.14159265f/180.f;
    float pitch=pitchDeg*3.14159265f/180.f;
    float sinY=sinf(yaw),  cosY=cosf(yaw);
    float sinP=sinf(pitch),cosP=cosf(pitch);
    float rx= cosY*dx - sinY*dz;
    float ry=-sinP*(sinY*dx+cosY*dz)+cosP*dy;
    float rz= cosP*(sinY*dx+cosY*dz)+sinP*dy;
    if(rz<=0.1f)return false;
    float fov   =fovDeg*3.14159265f/180.f;
    float aspect=screenW/screenH;
    float tanH  =tanf(fov*0.5f);
    outX=(screenW*0.5f)+(rx/(rz*tanH*aspect))*(screenW*0.5f);
    outY=(screenH*0.5f)-(ry/(rz*tanH))        *(screenH*0.5f);
    return true;
}
