
#include "gfx.hpp"

int main(int argc, char **argv)
{
	gfx *g = new gfx();
	
	g->init();
	
	while(!g->main_loop())
		g->render();
	
	g->deinit();
	
	delete g;
	
	return 0;
}
