#include <vk_engine.h>

int main(int argc, char* argv[])
{
	VulkanEngine engine;

	engine.init();
	
	engine.Run();
	
	engine.cleanup();
	
	return 0;
}
