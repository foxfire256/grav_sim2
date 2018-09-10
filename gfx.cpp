#include "gfx.hpp"

#include <iostream>
#include <omp.h>
#include <GL/glu.h>

#include "physics.hpp"
#include "fox/counter.hpp"
#include "fox/gfx/eigen_opengl.hpp"

#ifdef _WIN32
std::string data_root = "C:/dev/grav_sim2";
#else // Linux
std::string data_root = "/home/foxfire/dev/grav_sim2";
#endif

#define print_opengl_error() print_opengl_error2((char *)__FILE__, __LINE__)
int print_opengl_error2(char *file, int line);

gfx::gfx()
{
	this->generator = std::mt19937_64(std::random_device{}());
}

void gfx::init()
{
	done = 0;
	int ret;
	std::string window_title = "Gravity Sim 2";
	win_w = 768;
	win_h = 768;
	
	ret = SDL_Init(SDL_INIT_VIDEO);
	if(ret < 0)
	{
		printf("Unable to init SDL: %s\n", SDL_GetError());
		exit(1);
	}
	
	window = SDL_CreateWindow(
		window_title.c_str(),
		SDL_WINDOWPOS_UNDEFINED,
		SDL_WINDOWPOS_UNDEFINED,
		win_w,
		win_h,
		SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE
		);
	
	if(!window)
	{
		printf("Couldn't create window: %s\n", SDL_GetError());
		SDL_Quit();
		exit(-1);
	}
	
	SDL_ShowWindow(window);
	
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_ACCELERATED_VISUAL, 1);
	SDL_GL_SetAttribute(SDL_GL_RETAINED_BACKING, 1);
	
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 6);
	
	//SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,
	//					SDL_GL_CONTEXT_PROFILE_CORE);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,
		SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);

	context = SDL_GL_CreateContext(window);
	
	ret = SDL_GL_MakeCurrent(window, context);
	if(ret)
	{
		printf("ERROR could not make GL context current after init!\n");
		if(window)
			SDL_DestroyWindow(window);
		
		SDL_Quit();
		exit(1);
	}
	
	// 0 = no vsync
	SDL_GL_SetSwapInterval(0);
	
	std::cout << "Running on platform: " << SDL_GetPlatform() << std::endl;
	std::cout << "Number of logical CPU cores: " << SDL_GetCPUCount() << std::endl;
	int ram_mb = SDL_GetSystemRAM();
	char buffer[8];
	snprintf(buffer, 8, "%.1f", ram_mb / 1024.0f);
	std::cout << "System RAM " << ram_mb << "MB (" << buffer << " GB)\n";
	
	// OpenGL init
	// init glew first
	glewExperimental = GL_TRUE; // Needed in core profile
	if(glewInit() != GLEW_OK)
	{
		printf("Failed to initialize GLEW\n");
		exit(-1);
	}
	
	// HACK: to get around initial glew error with core profiles
	GLenum gl_err = glGetError();
	while(gl_err != GL_NO_ERROR)
	{
		//printf("glError in file %s @ line %d: %s (after glew init)\n",
		//	(char *)__FILE__, __LINE__, gluErrorString(gl_err));
		gl_err = glGetError();
	}
	
	print_opengl_error();
	
	print_info();

	print_opengl_error();

	// init basic OpenGL stuff
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

	// TODO: may want depth test off and blending off
	glDisable(GL_DEPTH_TEST);
	glEnable(GL_BLEND); // alpha channel
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glEnable(GL_POLYGON_SMOOTH);
	glHint(GL_POLYGON_SMOOTH_HINT, GL_NICEST);
	// need compatability profile for these
	glEnable(GL_POINT_SMOOTH);
	glHint(GL_POINT_SMOOTH_HINT, GL_NICEST);

	glPointSize(3.0f);

	// OpenGL 3.2 core requires a VAO to be bound to use a VBO
	// WARNING: GLES 2.0 does not support VAOs
	glGenVertexArrays(1, &default_vao);
	glBindVertexArray(default_vao);

	// initialize some defaults
	eye = Eigen::Vector3f(0.0f, 0.0f, 10.0f);
	target = Eigen::Vector3f(0.0f, 0.0f, 0.0f);
	up = Eigen::Vector3f(0.0f, 1.0f, 0.0f);

	fox::gfx::look_at(eye, target, up, V);
	fox::gfx::perspective(65.0f, (float)win_w / (float)win_h, 0.01f, 40.0f, P);
	M = Eigen::Matrix4f::Identity();

	print_opengl_error();

	obj_count = 1024;

	x_gfx.resize(obj_count * 3);
	for(int i = 0; i < obj_count; i++)
	{
		x_gfx[i * 3] = 0.0f;
		x_gfx[i * 3 + 1] = 0.0f;
		x_gfx[i * 3 + 2] = 0.0f;
	}
	glGenBuffers(1, &x_vbo);
	glBindBuffer(GL_ARRAY_BUFFER, x_vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(float) * obj_count * 3,
		x_gfx.data(), GL_STATIC_DRAW);

	print_opengl_error();
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	load_shaders();

	print_opengl_error();

	glUseProgram(point_render_program);
	GLint u;
	MVP = P * (V * M);
	u = glGetUniformLocation(point_render_program, "MVP");
	glUniformMatrix4fv(u, 1, GL_FALSE, MVP.data());

	print_opengl_error();
	fflush(stdout);

	update_counter = new fox::counter();
	fps_counter = new fox::counter();
	perf_counter = new fox::counter();

	perf_index = 0;

	p = new physics();
	p->init(obj_count);
}

void gfx::deinit()
{
	// TODO: can this be freed earlier?
	if(shader_vert_id != 0)
		glDeleteShader(shader_vert_id);
	// TODO: can this be freed earlier?
	if(shader_frag_id != 0)
		glDeleteShader(shader_frag_id);
	if(point_render_program != 0)
		glDeleteProgram(point_render_program);

	glDeleteBuffers(1, &x_vbo);

	SDL_GL_DeleteContext(context);
	SDL_DestroyWindow(window);
	
	SDL_Quit();

	delete p;

	delete update_counter;
	delete fps_counter;
	delete perf_counter;
}

void gfx::render()
{
	render_times[perf_index] = perf_counter->update_double();
	
	int status = SDL_GL_MakeCurrent(window, context);
	if(status)
	{
		printf("SDL_GL_MakeCurrent() failed in render(): %s\n",
			   SDL_GetError());
		SDL_GL_DeleteContext(context);
		SDL_DestroyWindow(window);
		
		SDL_Quit();
		exit(1);
	}

	double delta_t = update_counter->update_double();
	p->step(delta_t);
	auto px = p->get_pos();
	#pragma omp parallel for
	for(int i = 0; i < obj_count; i++)
	{
		x_gfx[i * 3] = (float)px[i][0];
		x_gfx[i * 3 + 1] = (float)px[i][1];
		x_gfx[i * 3 + 2] = (float)px[i][2];
	}

	phys_times[perf_index] = perf_counter->update_double();

	perf_index++;
	if(perf_index >= perf_array_size)
		perf_index = 0;
	total_time += fps_counter->update_double();
	if(total_time >= 1.0)
	{
		phys_time = 0.0;
		render_time = 0.0;
		for(uint8_t i = 0; i < perf_array_size; i++)
		{
			phys_time += phys_times[i];
			render_time += render_times[i];
		}
		printf("Physics time:    %.9f\n", phys_time);
		printf("Render time:     %.9f\n", render_time);
		printf("----------------------------\n");
		//fflush(stdout);
		total_time = 0.0;
	}

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glUseProgram(point_render_program);
	GLint vertex_loc = glGetAttribLocation(point_render_program, "vertex");
	glBindBuffer(GL_ARRAY_BUFFER, x_vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(float) * obj_count * 3,
		x_gfx.data(), GL_STATIC_DRAW);
	glEnableVertexAttribArray(vertex_loc);
	glVertexAttribPointer(vertex_loc, 3, GL_FLOAT, GL_FALSE, 0, 0);

	glDrawArrays(GL_POINTS, 0, obj_count);

	glBindBuffer(GL_ARRAY_BUFFER, 0);

	SDL_GL_SwapWindow(window);

	if(print_opengl_error())
	{
		fflush(stdout);
		exit(-1);
	}
}

void gfx::resize(int w, int h)
{
	win_w = w;
	win_h = h;
	
	glViewport(0, 0, win_w, win_h);
	fox::gfx::perspective(65.0f, (float)win_w / (float)win_h, 0.01f, 40.0f, P);
	MVP = P * (V * M);
	if(point_render_program != 0)
	{
		GLuint u;
		glUseProgram(point_render_program);
		u = glGetUniformLocation(point_render_program, "MVP");
		glUniformMatrix4fv(u, 1, GL_FALSE, MVP.data());
	}
}

void gfx::load_shaders()
{
	print_opengl_error();

	std::string fname = data_root + "/point_render_v330.vert";
	FILE *f;
	uint8_t *vert_data, *frag_data, *phys_data;
	long fsize, vsize, phsize;
	long ret, result;
	f = fopen(fname.c_str(), "rt");
	if(f == NULL)
	{
		printf("ERROR couldn't open shader file %s\n", fname.c_str());
		exit(-1);
	}
	// find the file size
	fseek(f, 0, SEEK_END);
	vsize = ftell(f);
	rewind(f);

	vert_data = (uint8_t *)malloc(sizeof(uint8_t) * vsize);
	if(vert_data == nullptr)
	{
		printf("Failed to allocate vertex shader memory\n");
		exit(-1);
	}
	result = fread(vert_data, sizeof(uint8_t), vsize, f);
	if(result != vsize)
	{
		printf("ERROR: loading shader: %s\n", fname.c_str());
		printf("Expected %d bytes but only read %d\n", vsize, result);

		fclose(f);
		free(vert_data);
		exit(-1);
	}
	// TODO: is this really a good idea?
	vert_data[vsize - 1] = '\0';
	fclose(f);

	fname = data_root + "/point_render_v330.frag";
	f = fopen(fname.c_str(), "rt");
	if(f == NULL)
	{
		printf("ERROR couldn't open shader file %s\n", fname.c_str());
		exit(-1);
	}
	// find the file size
	fseek(f, 0, SEEK_END);
	fsize = ftell(f);
	rewind(f);

	frag_data = (uint8_t *)malloc(sizeof(uint8_t) * fsize);
	if(frag_data == nullptr)
	{
		printf("Failed to allocate frag shader memory\n");
		exit(-1);
	}
	result = fread(frag_data, sizeof(uint8_t), fsize, f);
	if(result != fsize)
	{
		printf("ERROR: loading shader: %s\n", fname.c_str());
		printf("Expected %d bytes but only read %d\n", fsize, result);

		fclose(f);
		free(frag_data);
		exit(-1);
	}
	// TODO: is this really a good idea?
	frag_data[fsize - 1] = '\0';
	fclose(f);

	// actually create the shader
		shader_vert_id = glCreateShader(GL_VERTEX_SHADER);
	if(shader_vert_id == 0)
	{
		printf("Failed to create GL_VERTEX_SHADER!\n");
		exit(-1);
	}
	glShaderSource(shader_vert_id, 1, (GLchar **)&vert_data, NULL);
	glCompileShader(shader_vert_id);
	free(vert_data);

	// print shader info log
	int length = 0, chars_written = 0;
	char *info_log;
	glGetShaderiv(shader_vert_id, GL_INFO_LOG_LENGTH, &length);
	// WTF? so why was 4 used?
	// use 2 for the length because NVidia cards return a line feed always
	if(length > 4)
	{
		info_log = (char *)malloc(sizeof(char) * length);
		if(info_log == NULL)
		{
			printf("ERROR couldn't allocate %d bytes for shader info log!\n",
				length);
			exit(-1);
		}

		glGetShaderInfoLog(shader_vert_id, length, &chars_written, info_log);

		printf("Shader info log: %s\n", info_log);

		free(info_log);
	}

	// actually create the shader
	shader_frag_id = glCreateShader(GL_FRAGMENT_SHADER);
	if(shader_frag_id == 0)
	{
		printf("Failed to create GL_VERTEX_SHADER!\n");
		exit(-1);
	}
	glShaderSource(shader_frag_id, 1, (GLchar **)&frag_data, NULL);
	glCompileShader(shader_frag_id);
	free(frag_data);

	// print shader info log
	length = 0;
	chars_written = 0;
	glGetShaderiv(shader_frag_id, GL_INFO_LOG_LENGTH, &length);
	// WTF? so why was 4 used?
	// use 2 for the length because NVidia cards return a line feed always
	if(length > 4)
	{
		info_log = (char *)malloc(sizeof(char) * length);
		if(info_log == NULL)
		{
			printf("ERROR couldn't allocate %d bytes for shader info log!\n",
				length);
			exit(-1);
		}

		glGetShaderInfoLog(shader_frag_id, length, &chars_written, info_log);

		printf("Shader info log: %s\n", info_log);

		free(info_log);
	}

	// create the shader program
	point_render_program = glCreateProgram();
	if(point_render_program == 0)
	{
		printf("Failed at glCreateProgram()!\n");
		exit(-1);
	}

	glAttachShader(point_render_program, shader_vert_id);
	glAttachShader(point_render_program, shader_frag_id);

	glLinkProgram(point_render_program);

	glGetProgramiv(point_render_program, GL_INFO_LOG_LENGTH, &length);

	// use 2 for the length because NVidia cards return a line feed always
	if(length > 4)
	{
		info_log = (char *)malloc(sizeof(char) * length);
		if(info_log == NULL)
		{
			printf("ERROR couldn't allocate %d bytes for shader program info log!\n",
				length);
			exit(-1);
		}
		else
		{
			printf("Shader program info log:\n");
		}

		glGetProgramInfoLog(point_render_program, length, &chars_written, info_log);

		printf("%s\n", info_log);

		free(info_log);
	}

	print_opengl_error();
}

int gfx::main_loop()
{
	SDL_Event event;
	while(SDL_PollEvent(&event))
	{
		switch(event.type)
		{
			case SDL_KEYDOWN:
				if(event.key.keysym.sym == SDLK_ESCAPE)
				{
					done = 1;
				}
				break;
			case SDL_KEYUP:
				break;
			case SDL_QUIT:
				done = 1;
				break;
			case SDL_WINDOWEVENT:
				switch (event.window.event)
				{
					case SDL_WINDOWEVENT_CLOSE:
						done = 1;
						break;
					case SDL_WINDOWEVENT_RESIZED:
						resize(event.window.data1, event.window.data2);
						break;
					default:
						break;
				}
			default:
				break;
		}
	}
	return done;
}

void gfx::print_info()
{
	printf("GLEW library version %s\n", glewGetString(GLEW_VERSION));
	
	if(glewIsSupported("GL_VERSION_5_1"))
	{
		printf("GLEW supported GL_VERSION_5_1\n");
	}
	else if(glewIsSupported("GL_VERSION_5_0"))
	{
		printf("GLEW supported GL_VERSION_5_0\n");
	}
	else if(glewIsSupported("GL_VERSION_4_8"))
	{
		printf("GLEW supported GL_VERSION_4_8\n");
	}
	else if(glewIsSupported("GL_VERSION_4_7"))
	{
		printf("GLEW supported GL_VERSION_4_7\n");
	}
	else if(glewIsSupported("GL_VERSION_4_6"))
	{
		printf("GLEW supported GL_VERSION_4_6\n");
	}
	else if(glewIsSupported("GL_VERSION_4_5"))
	{
		printf("GLEW supported GL_VERSION_4_5\n");
	}
	else if(glewIsSupported("GL_VERSION_4_4"))
	{
		printf("GLEW supported GL_VERSION_4_4\n");
	}
	else if(glewIsSupported("GL_VERSION_4_3"))
	{
		printf("GLEW supported GL_VERSION_4_3\n");
	}
	else if(glewIsSupported("GL_VERSION_4_2"))
	{
		printf("GLEW supported GL_VERSION_4_2\n");
	}
	else if(glewIsSupported("GL_VERSION_4_1"))
	{
		printf("GLEW supported GL_VERSION_4_1\n");
	}
	else if(glewIsSupported("GL_VERSION_4_0"))
	{
		printf("GLEW supported GL_VERSION_4_0\n");
	}
	else if(glewIsSupported("GL_VERSION_3_2"))
	{
		printf("GLEW supported GL_VERSION_3_2\n");
	}
	else if(glewIsSupported("GL_VERSION_3_1"))
	{
		printf("GLEW supported GL_VERSION_3_1\n");
	}
	else if(glewIsSupported("GL_VERSION_3_0"))
	{
		printf("GLEW supported GL_VERSION_3_0\n");
	}
	else if(glewIsSupported("GL_VERSION_2_1"))
	{
		printf("GLEW supported GL_VERSION_2_1\n");
	}
	else if(glewIsSupported("GL_VERSION_2_0"))
	{
		printf("GLEW supported GL_VERSION_2_0\n");
	}
	else
	{
		printf("NO GLEW GL_VERSION seems to be supported!\n");
	}
	
	printf("GL_VENDOR: %s\n", glGetString(GL_VENDOR));
	printf("GL_RENDERER: %s\n", glGetString(GL_RENDERER));
	printf("GL_VERSION: %s\n", glGetString(GL_VERSION));
	printf("GL_SHADING_LANGUAGE_VERSION: %s\n",
		   glGetString(GL_SHADING_LANGUAGE_VERSION));
	
	printf("Eigen version: %d.%d.%d\n", EIGEN_WORLD_VERSION,
		   EIGEN_MAJOR_VERSION,  EIGEN_MINOR_VERSION);
}

//------------------------------------------------------------------------------
// Returns 1 if an OpenGL error occurred, 0 otherwise.
int print_opengl_error2(char *file, int line)
{
	GLenum gl_err;
	int	ret_code = 0;
	
	gl_err = glGetError();
	while(gl_err != GL_NO_ERROR)
	{
		printf("glError in file %s @ line %d: %s\n", file, line,
			gluErrorString(gl_err));
		
		ret_code = 1;
		gl_err = glGetError();
	}
	return ret_code;
}
