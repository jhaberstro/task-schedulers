#include <iostream>
#include "mpsc_queue.hpp"
#include "task_distributing_scheduler.hpp"
#include <sys/time.h>
#include <cstring>

enum {block_width=8, block_height=8};
enum {image_width=4096, image_height=4096};
//----
enum {num_horizontal_blocks=image_width/block_width};
enum {num_vertical_blocks=image_height/block_height};
double g_delta_cr=0.0;
double g_delta_ci=0.0;
//----------------------------------------------------------------------------


//============================================================================
// Mandelbrot fractal calculation
//============================================================================
struct mandelbrot_block
{
	double start_cr, start_ci;
	uint8_t *result;
};
//----

void calculate_mandelbrot_block(void *data)
{
	mandelbrot_block* block_ = static_cast< mandelbrot_block* >(data);
	// calculate Mandelbrot fractal image block
	double ci=block_->start_ci;
	for(unsigned y=0; y<block_height; ++y)
	{
		uint8_t *res=block_->result+y*image_width;
		double cr=block_->start_cr;
		for(unsigned x=0; x<block_width; ++x)
		{
			// calculate Mandelbrot fractal pixel
			double zr=cr, zi=ci;
			double zr2, zi2;
			enum {max_iterations=256};
			unsigned i=0;
			do
			{
				zr2=zr*zr;
				zi2=zi*zi;
				zi=2.0*zr*zi+ci;
				zr=zr2-zi2+cr;
			} while(zr2+zi2<4.0 && ++i<max_iterations);

			// move to the next pixel
			cr+=g_delta_cr;
			*res++=uint8_t(i);
		}

		// move to the next line
		ci-=g_delta_ci;
	}
}

double elapsed_time_ms(timeval t1, timeval t2) {
	double elapsedTime = (t2.tv_sec - t1.tv_sec) * 1000.0;	// sec to ms
    elapsedTime += (t2.tv_usec - t1.tv_usec) / 1000.0;   	// us to ms
	return elapsedTime;
}

int main (int argc, char * const argv[]) {
	// Mandelbrot fractal setup
	enum {num_blocks=num_horizontal_blocks*num_vertical_blocks};
	enum {num_fractals=2};
	double mandelbrot_x=-2.0f;
	double mandelbrot_y=-1.0f;
	double mandelbrot_width=3.0f;
	double mandelbrot_height=2.0f;
	mandelbrot_y+=mandelbrot_height;	
	
	timeval t1, t2;
    
	// Single threaded profiling
	{
		uint8_t* image_mt=(uint8_t*)malloc(image_width*image_height);
		mandelbrot_block* blocks=(mandelbrot_block*)malloc(num_blocks*sizeof(mandelbrot_block));
		// setup image blocks and add jobs for multi-threaded test
        double elapsed = 0.0f;
		{
			for(unsigned i=0; i<num_fractals; ++i)
			{
				printf("Calculating fractal %i/%i...\n", i+1, num_fractals);
				gettimeofday(&t1, 0);
				g_delta_cr=mandelbrot_width/image_width;
				g_delta_ci=mandelbrot_height/image_width;
				unsigned bi=0;
				for(unsigned by=0; by<num_vertical_blocks; ++by)
					for(unsigned bx=0; bx<num_horizontal_blocks; ++bx)
				{
					mandelbrot_block &block=blocks[bi++];
					block.start_cr=mandelbrot_x+double(bx)*mandelbrot_width/num_horizontal_blocks;
					block.start_ci=mandelbrot_y-double(by)*mandelbrot_height/num_vertical_blocks;
					block.result=image_mt+bx*block_width+by*block_height*image_width;
					calculate_mandelbrot_block(&block);
				}

				gettimeofday(&t2, 0);
				double e = elapsed_time_ms(t1, t2);
				std::cout << e << std::endl;
				elapsed += e;
				
				mandelbrot_x+=mandelbrot_width*0.05f;
				mandelbrot_y-=mandelbrot_height*0.025f;
				mandelbrot_width*=0.9f;
				mandelbrot_height*=0.9f;
			}
		}
		free(blocks);
		free(image_mt);
		std::cout << "Serial time (ms): " << elapsed << std::endl;
	}
	
	// Multi-threaded profiling
	{
		task_distributing_scheduler jq;
	
		uint8_t* image_mt=(uint8_t*)malloc(image_width*image_height);
		mandelbrot_block* blocks=(mandelbrot_block*)malloc(num_blocks*sizeof(mandelbrot_block));
		// setup image blocks and add jobs for multi-threaded test
        double elapsed = 0.0f;
		{
			for(unsigned i=0; i<num_fractals; ++i)
			{
				printf("Calculating fractal %i/%i...\n", i+1, num_fractals);
				gettimeofday(&t1, 0);
				g_delta_cr=mandelbrot_width/image_width;
				g_delta_ci=mandelbrot_height/image_width;
				unsigned bi=0;
				for(unsigned by=0; by<num_vertical_blocks; ++by)
					for(unsigned bx=0; bx<num_horizontal_blocks; ++bx)
				{
					mandelbrot_block &block=blocks[bi++];
					block.start_cr=mandelbrot_x+double(bx)*mandelbrot_width/num_horizontal_blocks;
					block.start_ci=mandelbrot_y-double(by)*mandelbrot_height/num_vertical_blocks;
					block.result=image_mt+bx*block_width+by*block_height*image_width;
					jq.submit_task(calculate_mandelbrot_block, &block);
				}
			
				jq.wait_for_all_tasks();
				//jq.end_add_job(jtid);
				//jq.wait_all_jobs();
				gettimeofday(&t2, 0);
				double e = elapsed_time_ms(t1, t2);
				std::cout << e << std::endl;
				elapsed += e;
				

				mandelbrot_x+=mandelbrot_width*0.05f;
				mandelbrot_y-=mandelbrot_height*0.025f;
				mandelbrot_width*=0.9f;
				mandelbrot_height*=0.9f;
			}
		}
		
		free(blocks);
		free(image_mt);
		std::cout << "Parallel time (ms): " << elapsed << std::endl;
		
	}

    return 0;
}
