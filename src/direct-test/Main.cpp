#include "RenderSystem.h"

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
	Win32Application m_Window = {};
	m_Window.Initialize(hInstance, nShowCmd);

	RenderSystem particlesRender(m_Window);
	particlesRender.MainLoop();

	return 0;
}
