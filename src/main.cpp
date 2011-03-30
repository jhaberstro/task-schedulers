#include "task_distributing_scheduler.hpp"
//#include "work_stealing_lock_scheduler.hpp"
#include "task_manager.hpp"
#include <iostream>
#include <sys/time.h>
#include <cstring>


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

//============================================================================
// Dependency test 1
//============================================================================
struct child1_context
{
    int num_children;
    int volatile counter;
};

void child1(void* data) {
    //std::cout << "child1\n";
    atomic_increment(static_cast< child1_context* >(data)->counter);
}

void dependent1(void* data) {
    child1_context* context = static_cast< child1_context* >(data);
    assert(context->counter = context->num_children);
}

void dependency_test1() {
    std::cout << "Starting dependency test 1" << std::endl;
    
    enum { kTestRuns = 1 };
    for (int test = 0; test < kTestRuns; ++test) {
        enum { kChildren = 1000 };
        child1_context ctx = { kChildren + 1, 0 };
        task_manager jq(next_power_of_two(kChildren + 2), 1);
        task_id parentid = jq.begin_add(child1, &ctx);
        
        for (int i = 0; i < kChildren; ++i) {
            task_id childid = jq.begin_add(child1, &ctx);
            jq.add_child(parentid, childid);
            jq.end_add(childid);
        }
        
        task_id dependentid = jq.begin_add(dependent1, &ctx);
        jq.add_dependency(parentid, dependentid);
        jq.end_add(dependentid);
        
        jq.end_add(parentid);
        jq.wait(dependentid);
    }
    
    std::cout << "Dependency test 1 succedded!\nEnding dependency test 1\n\n";
}

//============================================================================
// Dependency test 2
//============================================================================
struct dependency_test2_global_context
{
    mpsc_queue< int > executionOrder;
};

struct dependency_test2_local_context
{
    dependency_test2_global_context* global_context;
    int id;
};

void dependency_test2_func(void* data) {
    dependency_test2_local_context* context = static_cast< dependency_test2_local_context* >(data);
    context->global_context->executionOrder.push(context->id);
}

void dependency_test2() {
#if 0
    std::cout << "Starting dependency test 2" << std::endl;
    
    enum { kTestRuns = 5 };
    for (int test = 0; test < kTestRuns; ++test) {

        task_manager jq(8);
        
        // Create the parent
        dependency_test2_global_context global_ctx;
        dependency_test2_local_context parent_ctx = { &global_ctx, 0 };
        task_id parentid = jq.create_task(dependency_test2_func, &parent_ctx);
        
        // Create first child
        dependency_test2_local_context child1_ctx = { &global_ctx, 1 };
        task_id child1id = jq.create_task(dependency_test2_func, &child1_ctx);
        jq.add_child(parentid, child1id);
        
        // Create the second child
        dependency_test2_local_context child2_ctx = { &global_ctx, 2 };
        task_id child2id = jq.create_task(dependency_test2_func, &child2_ctx);
        jq.add_child(parentid, child2id);
        
        // Create children's dependent task
        dependency_test2_local_context child_dependent_ctx = { &global_ctx, 3 };
        task_id child_dependentid = jq.create_task(dependency_test2_func, &child_dependent_ctx);
        jq.add_dependency(child2id, child_dependentid);
        jq.add_dependency(child1id, child_dependentid);
        jq.add_child(parentid, child_dependentid);
        
        // Add parent's dependent task
        dependency_test2_local_context parent_dependent_ctx = { &global_ctx, 4 };
        task_id parent_dependentid = jq.create_task(dependency_test2_func, &parent_dependent_ctx);
        jq.add_dependency(parentid, parent_dependentid);
        
        jq.submit_current_transaction();
        jq.wait(parent_dependentid);
        
        int i = 1;
        mpsc_queue< int >::node* n = 0;
        while ((n = global_ctx.executionOrder.pop()) != 0) {
            std::cout << i << ".) " << n->value << std::endl;
            ++i;
        }
        
        std::cout << std::endl;
    }
    
    std::cout << "Ending dependency test 2\n\n";
#endif
}

//============================================================================
// Dependency test 3
//============================================================================
struct dependency_test3_global_context
{
    mpsc_queue< int > executionOrder;
};

struct dependency_test3_local_context
{
    dependency_test3_global_context* global_context;
    int id;
};

void dependency_test3() {
    std::cout << "Starting dependency test 3" << std::endl;
    
    enum { kTestRuns = 5 };
    for (int test = 0; test < kTestRuns; ++test) {
        
        task_manager jq(8);
        
        // Create the parent
        dependency_test2_global_context global_ctx;
        dependency_test2_local_context parent_ctx = { &global_ctx, 0 };
        task_id parentid = jq.begin_add(dependency_test2_func, &parent_ctx);
            // Create sub-parent
            dependency_test2_local_context subparent_ctx = { &global_ctx, 1 };
            task_id subparentid = jq.begin_add(dependency_test2_func, &subparent_ctx);
                jq.add_child(parentid, subparentid);
            
                // Create first child
                dependency_test2_local_context child1_ctx = { &global_ctx, 2 };
                task_id child1id = jq.begin_add(dependency_test2_func, &child1_ctx);
                jq.add_child(subparentid, child1id);
                jq.end_add(child1id);
                
                // Create the second child
                dependency_test2_local_context child2_ctx = { &global_ctx, 3 };
                task_id child2id = jq.begin_add(dependency_test2_func, &child2_ctx);
                jq.add_child(subparentid, child2id);
                jq.end_add(child2id);

                // Create children's dependent task
                dependency_test2_local_context child_dependent_ctx = { &global_ctx, 4 };
                task_id child_dependentid = jq.begin_add(dependency_test2_func, &child_dependent_ctx);
                jq.add_dependency(subparentid, child_dependentid);
                jq.add_child(parentid, child_dependentid);
                jq.end_add(child_dependentid);
            jq.end_add(subparentid);
            
            // Add parent's dependent task
            dependency_test2_local_context parent_dependent_ctx = { &global_ctx, 5 };
            task_id parent_dependentid = jq.begin_add(dependency_test2_func, &parent_dependent_ctx);
            jq.add_dependency(parentid, parent_dependentid);
            jq.end_add(parent_dependentid);
        jq.end_add(parentid);
        jq.wait(parent_dependentid);
        
        int i = 1;
        mpsc_queue< int >::node* n = 0;
        while ((n = global_ctx.executionOrder.pop()) != 0) {
            std::cout << i << ".) " << n->value << std::endl;
            ++i;
        }
        
        std::cout << std::endl;
    }
    
    std::cout << "Ending dependency test 3\n\n";
}



//============================================================================
// Mandelbrot test
//============================================================================
enum { kBlockWidth = 16, kBlockHeight = 16 };
enum { kImageWidth = 4096, kImageHeight = 4096 };
//----
enum { kNumHorizontalBlocks = kImageWidth / kBlockWidth };
enum { kNumVerticalBlocks = kImageHeight / kBlockHeight };
double g_delta_cr = 0.0;
double g_delta_ci = 0.0;

struct mandelbrot_block
{
	double start_cr, start_ci;
	uint8_t *result;
};

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

void mandelbrot_test() {
    std::cout << "Starting mandelbrot test." << std::endl;
    
    // Mandelbrot fractal setup
	enum { kNumBlocks = kNumHorizontalBlocks * kNumVerticalBlocks };
	enum { kNumFractals = 4 };
    
    #if 0
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
    #endif
    
	// Multi-threaded profiling
	{
        task_manager jq(next_power_of_two(kNumBlocks));
        
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
                task_id parent = jq.begin_add(0, 0);
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
                        task_id id = jq.begin_add(calculate_mandelbrot_block, &block);
                        jq.add_child(parent, id);
                        jq.end_add(id);
                    }
                
                jq.end_add(parent);
				jq.wait(parent);
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
    
    std::cout << "Ending mandelbrot test.\n\n";
}

int main (int argc, char * const argv[]) {    
    dependency_test1();
    //dependency_test2();
    dependency_test3();
    mandelbrot_test();
    return 0;
}
