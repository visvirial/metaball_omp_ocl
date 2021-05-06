#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include <math.h>
#include <omp.h>

#include <SDL/SDL.h>
#include <CL/cl.h>

#include "metaball_common.h"

// TODO: SDL padding is not ovious

// For OpenCL
static cl_context context;
static cl_kernel kernel;
static cl_command_queue command_queue;
static cl_mem buf_data;
static cl_mem buf_br;

// 0=OpenCL(GPU), 1=CPU
static int use_device = 0;

static float br[2*N_BALLS];
static float bv[2*N_BALLS];

static float actual_fps = 0;

SDL_Surface *surface;

static double gettimeofday_sec(){
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return (double)(tv.tv_sec + tv.tv_usec * 0.000001);
}

static void draw_balls_ocl(){
	clEnqueueWriteBuffer(command_queue, buf_br, CL_FALSE, 0, 2*N_BALLS*sizeof(float), br, 0, NULL, NULL);
	size_t global_work_size = WIDTH * HEIGHT;
	size_t local_work_size = 32;
	cl_int errno = clEnqueueNDRangeKernel(command_queue, kernel, 1, NULL, &global_work_size, &local_work_size, 0, NULL, NULL);
	if(errno != CL_SUCCESS)
		fprintf(stderr, "E: failed to execute kernel: errno=%d.\n", errno);
	clEnqueueReadBuffer(command_queue, buf_data, CL_TRUE, 0, 3*WIDTH*HEIGHT*sizeof(Uint8), surface->pixels, 0, NULL, NULL);
}

static void draw_balls_cpu(){
	Uint8 *data = surface->pixels;
	#ifdef _OPENMP
	#pragma omp parallel for
	#endif
	for(int y=0; y<HEIGHT; y++) for(int x=0; x<WIDTH; x++){
		float density = 0.0;
		for(int i=0; i<N_BALLS; i++)
			density += 1.f / ((br[2*i+0] - x) * (br[2*i+0] - x) + (br[2*i+1] - y) * (br[2*i+1] - y));
		int col_r, col_g, col_b;
		if(density > THRESHOLD){
			col_r = INNER_COLOR_R;
			col_g = INNER_COLOR_G;
			col_b = INNER_COLOR_B;
		}else{
			col_r = FACTOR * density * COLOR_R;
			if(col_r > 255) col_r = COLOR_R;
			col_g = FACTOR * density * COLOR_G;
			if(col_g > 255) col_g = COLOR_G;
			col_b = FACTOR * density * COLOR_B;
			if(col_b > 255) col_b = COLOR_B;
		}
		data[3*(x+y*WIDTH)+2] = (Uint8)(col_r);
		data[3*(x+y*WIDTH)+1] = (Uint8)(col_g);
		data[3*(x+y*WIDTH)+0] = (Uint8)(col_b);
	}
}

static void reset_window_title(){
	char buf[1024];
	snprintf(buf, sizeof(buf), "Metaball / %s / %.2fFPS", use_device==0?"OpenCL(GPU)":"CPU", actual_fps);
	SDL_WM_SetCaption(buf, NULL);
}

static inline void main_loop(){
	double begin = gettimeofday_sec();
	double begin_fps = gettimeofday_sec();
	double dt = 0.0;
	for(int frames=0; ; frames++){
		// Process all events
		for(SDL_Event event; SDL_PollEvent(&event); ){
			switch(event.type){
				case SDL_KEYUP:
					if(event.key.keysym.sym == SDLK_SPACE){
						use_device = 1 - (use_device % 2);
						reset_window_title();
					}
					break;
				case SDL_QUIT:
					return;
			}
		}
		// update ball places
		for(int i=0; i<N_BALLS; i++){
			for(int j=0; j<2; j++){
				br[2*i+j] += dt * bv[2*i+j];
				if(br[2*i+j] < 0 || br[2*i+j] > (j==0?WIDTH:HEIGHT)){
					bv[2*i+j] *= -1;
					br[2*i+j] += 2 * dt * bv[2*i+j];
				}
			}
		}
		// update fps if needed
		{
			float elapsed = gettimeofday_sec() - begin_fps;
			if(elapsed > 1.0){
				actual_fps = frames / elapsed;
				reset_window_title();
				frames = 0;
				begin_fps = gettimeofday_sec();
			}
		}
		// draw
		if(use_device == 0)
			draw_balls_ocl();
		else
			draw_balls_cpu();
		// Swap bufferes
		SDL_Flip(surface);
		// Update dt
		dt = gettimeofday_sec() - begin;
		begin = gettimeofday_sec();
	}
}

static void ocl_pfn_notify(const char *errinfo, const void *private_info, size_t cb, void *user_data){
	fprintf(stderr, "W: caught an error in ocl_pfn_notify:\nW:   %s", errinfo);
}

int main(int argc, char *argv[]){
	srand(time(NULL));
	// Initialize SDL
	if(SDL_Init(SDL_INIT_VIDEO)){
		fprintf(stderr, "E: failed to initialize SDL library.\n");
		exit(1);
	}
	// Initialize surface
	surface = SDL_SetVideoMode(WIDTH, HEIGHT, 24, SDL_HWSURFACE|SDL_DOUBLEBUF);
	if(!surface){
		fprintf(stderr, "E: failed to initialize SDL surface.\n");
		exit(1);
	}
	reset_window_title();
	
	// Initialize OpenCL
	// Load kernel source
	FILE *fp;
	fp = fopen(KERNEL_SOURCE_NAME, "r");
	if(!fp){
		fprintf(stderr, "E: failed to load kernel source (%s).\n", KERNEL_SOURCE_NAME);
		exit(1);
	}
	char *kernel_src = malloc(KERNEL_SOURCE_BUF_SIZE * sizeof(char));
	size_t kernel_size = fread(kernel_src, 1, KERNEL_SOURCE_BUF_SIZE, fp);
	fclose(fp);
	// Get platform/device information
	cl_platform_id platforms;
	clGetPlatformIDs(1, &platforms, NULL);
	cl_device_id devices;
	clGetDeviceIDs(platforms, CL_DEVICE_TYPE_GPU, 1, &devices, NULL);
	// Create OpenCL context
	context = clCreateContext(NULL, 1, &devices, ocl_pfn_notify, NULL, NULL);
	// Create command queue
	command_queue = clCreateCommandQueue(context, devices, 0, NULL);
	// Create kernel program from source
	cl_program program = clCreateProgramWithSource(context, 1, (const char **)&kernel_src, (const size_t *)&kernel_size, NULL);
	// Build kernel program
	{
		cl_int errno = clBuildProgram(program, 0, NULL, "-Werror", NULL, NULL);
		char *build_log = malloc(0xFFFF*sizeof(char));;
		size_t log_size;
		clGetProgramBuildInfo(program, devices, CL_PROGRAM_BUILD_LOG, 0xFFFF*sizeof(char), build_log, &log_size);
		printf("I: kernel build log:\n%s\n", build_log);
		free(build_log);
		if(errno != CL_SUCCESS){
			fprintf(stderr, "E: failed to build kernel program.\n");
			exit(1);
		}
	}
	// Create kernel
	kernel = clCreateKernel(program, KERNEL_NAME, NULL);
	// Create buffer
	buf_data = clCreateBuffer(context, CL_MEM_WRITE_ONLY, 4*WIDTH*HEIGHT*sizeof(char), NULL, NULL);
	buf_br = clCreateBuffer(context, CL_MEM_READ_ONLY, 2*N_BALLS*sizeof(float), NULL, NULL);
	clSetKernelArg(kernel, 0, sizeof(cl_mem), &buf_data);
	clSetKernelArg(kernel, 1, sizeof(cl_mem), &buf_br);
	
	// Initialize balls
	for(int i=0; i<N_BALLS; i++){
		br[2*i+0] = 1.0 * WIDTH * rand() / RAND_MAX;
		br[2*i+1] = 1.0 * HEIGHT * rand() / RAND_MAX;
		double theta = 2.0 * M_PI * rand() / RAND_MAX;
		bv[2*i+0] = SPEED * cos(theta);
		bv[2*i+1] = SPEED * sin(theta);
	}
	
	// Enter the main loop
	main_loop();
	
	// Clean up
	clFlush(command_queue);
	clFinish(command_queue);
	clReleaseMemObject(buf_data);
	clReleaseMemObject(buf_br);
	clReleaseKernel(kernel);
	clReleaseProgram(program);
	clReleaseCommandQueue(command_queue);
	clReleaseContext(context);
	SDL_Quit();
	
	return 0;
}

