#pragma once
#include "config.h"

struct Particle
{
    DirectX::XMFLOAT3   pos;
    float               radius;
    float               opacity;

    static float RandomPercent()
    {
        float ret = static_cast<float>((rand() % 10000) - 5000);
        return ret / 5000.0f;
    }

    static std::vector<Particle> LoadParticles(const DirectX::XMFLOAT3& center, const DirectX::XMFLOAT4& velocity, float spread, UINT numParticles)
    {
        srand(0);

        std::vector<Particle> particlesData(numParticles);

        for (UINT i = 0; i < numParticles; i++)
        {
            DirectX::XMFLOAT3 delta(spread, spread, spread);

            while (DirectX::XMVectorGetX(DirectX::XMVector3LengthSq(XMLoadFloat3(&delta))) > spread * spread)
            {
                delta.x = RandomPercent() * spread;
                delta.y = RandomPercent() * spread;
                delta.z = RandomPercent() * spread;
            }

            particlesData[i].pos.x = center.x + delta.x;
            particlesData[i].pos.y = center.y + delta.y;
            particlesData[i].pos.z = center.z + delta.z;

            particlesData[i].radius = 20.0f;
            particlesData[i].opacity = 1.0f;
        }

        return particlesData;
    }
};
