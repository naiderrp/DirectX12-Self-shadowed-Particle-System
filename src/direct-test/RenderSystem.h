#pragma once
#include "DeviceContext.h"

class RenderSystem
{
public:
	RenderSystem(Win32Application& window);

	RenderSystem(RenderSystem&) = delete;
	RenderSystem(RenderSystem&&) = default;

	RenderSystem& operator=(RenderSystem&) = delete;
	RenderSystem& operator=(RenderSystem&&) = default;

	~RenderSystem() = default;

public:
	void RecordDrawingCommands();

	void Render();

	void ReadDataFromComputePipeline();

	void RunSimulation();

	void MainLoop();

private:
	int particlesCount = 1025; // 1024 is a light source
	std::vector<Particle> m_Particles = Particle::LoadParticles(XMFLOAT3(330.0f * 0.50f, 0, 0), XMFLOAT4(0, 0, -20, 1 / 100000000.0f), 330.0f, particlesCount);

	DeviceContext m_GPU;
};
