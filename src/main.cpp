#include "task_distributing_scheduler.hpp"
#include <iostream>
#include <sys/time.h>
#include <cstring>

enum { kBlockWidth = 8, kBlockHeight = 8 };
enum { kImageWidth = 4096, kImageHeight = 4096 };
//----
enum { kNumHorizontalBlocks = kImageWidth / kBlockWidth };
enum { kNumVerticalBlocks = kImageHeight / kBlockHeight };
double g_delta_cr = 0.0;
double g_delta_ci = 0.0;
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
	double ci = block_->start_ci;
	for(unsigned y = 0; y < kBlockHeight; ++y)
	{
		uint8_t *res=block_->result+y*kImageWidth;
		double cr=block_->start_cr;
		for(unsigned x = 0; x < kBlockWidth; ++x)
		{
			// calculate Mandelbrot fractal pixel
			double zr = cr, zi = ci;
			double zr2, zi2;
			enum { kMaxIterations = 256 };
			unsigned i = 0;
			do
			{
				zr2 = zr * zr;
				zi2 = zi * zi;
				zi = 2.0 * zr * zi + ci;
				zr = zr2 - zi2 + cr;
			} while(zr2 + zi2 < 4.0 && ++i < kMaxIterations);

			// move to the next pixel
			cr += g_delta_cr;
			*res++ = uint8_t(i);
		}

		// move to the next line
		ci -= g_delta_ci;
	}
}

double elapsed_time_ms(timeval t1, timeval t2) {
	double elapsedTime = (t2.tv_sec - t1.tv_sec) * 1000.0;	// sec to ms
    elapsedTime += (t2.tv_usec - t1.tv_usec) / 1000.0;   	// us to ms
	return elapsedTime;
}

uint32_t next_power_of_two(uint32_t v) {
	v--;
	v |= v >> 1;
	v |= v >> 2;
	v |= v >> 4;
	v |= v >> 8;
	v |= v >> 16;
	v++;
	return v;
}

int main (int argc, char * const argv[]) {
	// Mandelbrot fractal setup
	enum { kNumBlocks = kNumHorizontalBlocks * kNumVerticalBlocks };
	enum { kNumFractals = 2 };
    
	// Single threaded profiling
	{
		timeval t1, t2;
		double mandelbrot_x = -2.0f;
		double mandelbrot_y = -1.0f;
		double mandelbrot_width = 3.0f;
		double mandelbrot_height = 2.0f;
		mandelbrot_y += mandelbrot_height;
		
		uint8_t* image_mt = (uint8_t*)malloc(kImageWidth*kImageHeight);
		mandelbrot_block* blocks = (mandelbrot_block*)malloc(kNumBlocks*sizeof(mandelbrot_block));
		// setup image blocks and add jobs for multi-threaded test
        double elapsed = 0.0f;
		{
			for(unsigned i = 0; i < kNumFractals; ++i)
			{
				printf("Calculating fractal %i/%i...\n", i+1, kNumFractals);
				gettimeofday(&t1, 0);
				g_delta_cr = mandelbrot_width/kImageWidth;
				g_delta_ci = mandelbrot_height/kImageWidth;
				unsigned bi = 0;
				for(unsigned by = 0; by < kNumVerticalBlocks; ++by)
					for(unsigned bx = 0; bx < kNumHorizontalBlocks; ++bx)
				{
					mandelbrot_block &block = blocks[bi++];
					block.start_cr = mandelbrot_x + double(bx) * mandelbrot_width / kNumHorizontalBlocks;
					block.start_ci = mandelbrot_y - double(by) * mandelbrot_height / kNumVerticalBlocks;
					block.result = image_mt + bx * kBlockWidth + by * kBlockHeight * kImageWidth;
					calculate_mandelbrot_block(&block);
				}

				gettimeofday(&t2, 0);
				double e = elapsed_time_ms(t1, t2);
				std::cout << e << std::endl;
				elapsed += e;
				
				mandelbrot_x += mandelbrot_width * 0.05f;
				mandelbrot_y -= mandelbrot_height * 0.025f;
				mandelbrot_width *= 0.9f;
				mandelbrot_height *= 0.9f;
			}
		}
		free(blocks);
		free(image_mt);
		std::cout << "Serial time (ms): " << elapsed << std::endl;
	}
	
	// Multi-threaded profiling
	{
		task_distributing_scheduler jq(next_power_of_two(kNumBlocks));
	
		timeval t1, t2;
		double mandelbrot_x = -2.0f;
		double mandelbrot_y = -1.0f;
		double mandelbrot_width = 3.0f;
		double mandelbrot_height = 2.0f;
		mandelbrot_y += mandelbrot_height;
	
		uint8_t* image_mt = (uint8_t*)malloc(kImageWidth*kImageHeight);
		mandelbrot_block* blocks = (mandelbrot_block*)malloc(kNumBlocks*sizeof(mandelbrot_block));
		// setup image blocks and add jobs for multi-threaded test
        double elapsed = 0.0f;
		{
			for(unsigned i = 0; i < kNumFractals; ++i)
			{
				printf("Calculating fractal %i/%i...\n", i+1, kNumFractals);
				gettimeofday(&t1, 0);
				g_delta_cr = mandelbrot_width/kImageWidth;
				g_delta_ci = mandelbrot_height/kImageWidth;
				unsigned bi = 0;
				for(unsigned by = 0; by < kNumVerticalBlocks; ++by)
					for(unsigned bx = 0; bx < kNumHorizontalBlocks; ++bx)
				{
					mandelbrot_block &block = blocks[bi++];
					block.start_cr = mandelbrot_x + double(bx) * mandelbrot_width / kNumHorizontalBlocks;
					block.start_ci = mandelbrot_y - double(by) * mandelbrot_height / kNumVerticalBlocks;
					block.result = image_mt + bx * kBlockWidth + by * kBlockHeight * kImageWidth;
					jq.submit_task(calculate_mandelbrot_block, &block);
				}

				jq.wait_for_all_tasks();
				gettimeofday(&t2, 0);
				double e = elapsed_time_ms(t1, t2);
				std::cout << e << std::endl;
				elapsed += e;
				
				mandelbrot_x += mandelbrot_width * 0.05f;
				mandelbrot_y -= mandelbrot_height * 0.025f;
				mandelbrot_width *= 0.9f;
				mandelbrot_height *= 0.9f;
			}
		}

		free(blocks);
		free(image_mt);
		std::cout << "Parallel time (ms): " << elapsed << std::endl;
		
	}

    return 0;
}
