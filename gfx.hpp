#ifndef GFX_HPP
#define GFX_HPP

#define _USE_MATH_DEFINES
#include <vector>
#include <random>
#include <SDL2/SDL.h>
#include <GL/glew.h>
#include <Eigen/Core>
#include <Eigen/Geometry>

namespace fox
{
	class counter;
}
class physics;

class gfx
{
public:

	gfx();
	
	void init();
	void deinit();
	void render();
	void resize(int w, int h);
	int main_loop();
	
private:
	void print_info();
	void load_shaders();

	fox::counter *fps_counter;
	fox::counter *update_counter;
	fox::counter *perf_counter;
	SDL_Window *window;
	SDL_GLContext context;
	int done;
	int win_w;
	int win_h;

	/**
	 * @brief A random generator that is initialized in the constructor
	 */
	std::mt19937_64 generator;

	double G = 6.67408e-11;
	uint16_t obj_count;

	// an empty vertex array object to bind to
	uint32_t default_vao;
	Eigen::Vector3f eye, target, up;
	Eigen::Affine3f V;
	Eigen::Affine3f M;
	Eigen::Projective3f P, MVP;

	GLuint point_render_program, shader_vert_id, shader_frag_id;
	GLuint x_vbo;
	std::vector<float> x_gfx;

	const static uint8_t perf_array_size = 8;
	double phys_times[perf_array_size];
	double render_times[perf_array_size];
	double phys_time;
	double render_time;
	uint8_t perf_index;
	double total_time;

	physics *p;
};

#endif
