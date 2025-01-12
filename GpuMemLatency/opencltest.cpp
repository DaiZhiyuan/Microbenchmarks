#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys\timeb.h>

#define CL_USE_DEPRECATED_OPENCL_1_2_APIS
#include <CL/cl.h>
#define MAX_SOURCE_SIZE (0x100000)

int default_test_sizes[27] = { 2, 4, 8, 16, 24, 32, 48, 64, 128, 256, 512, 600, 768, 1024, 1536, 2048, 3072, 4096, 5120, 6144, 8192, 16384, 32768, 65536, 131072, 262144, 1048576 };

cl_device_id selected_device_id;
cl_platform_id selected_platform_id;
cl_context get_context_from_user(int platform_index, int device_index);
float latency_test(cl_context context,
    cl_command_queue command_queue,
    cl_kernel kernel,
    uint32_t list_size,
    uint32_t chase_iterations,
    bool sattolo);
float latency_bw_test(cl_context context,
    cl_command_queue command_queue,
    cl_kernel kernel,
    uint32_t list_size,
    uint32_t thread_count,
    uint32_t local_size,
    uint32_t chase_iterations);
float int_exec_latency_test(cl_context context,
    cl_command_queue command_queue,
    cl_kernel kernel,
    uint32_t iterations);
float int_atomic_latency_test(cl_context context,
    cl_command_queue command_queue,
    cl_kernel kernel,
    uint32_t iterations,
    bool local);
cl_ulong get_max_buffer_size();
cl_ulong get_max_constant_buffer_size();


int main(int argc, char *argv[]) {
    cl_int ret;
    uint32_t stride = 1211;
    uint32_t list_size = 3840*2160*4;
    uint32_t chase_iterations = 1e6 * 7;
    uint32_t thread_count = 1, local_size = 1;
    float latency, bandwidth;
    int platform_index = -1, device_index = -1;

    if (argc > 2)
    {
        platform_index = atoi(argv[1]);
        device_index = atoi(argv[2]);
        fprintf(stderr, "Will use OpenCL platform index %d, device index %d\n", platform_index, device_index);

        if (argc > 3) stride = atoi(argv[3]);
        if (argc > 4) chase_iterations = atoi(argv[4]);
        if (argc > 5) thread_count = atoi(argv[5]);
        if (argc > 6) local_size = atoi(argv[6]);
    }
    else
    {
        fprintf(stderr, "Usage: [opencl platform index] [opencl device index] [stride] [p-chase iterations] [threads] [local work size]\n");
        fprintf(stderr, "Number of threads (OpenCL global work size) must be divisible by local work size\n");
    }

    fprintf(stderr, "Doing %d K p-chase iterations with stride %d over %d KiB region\n", chase_iterations / 1000, stride, list_size * 4 / 1024);
    fprintf(stderr, "Using %d threads with local size %d\n", thread_count, local_size);
#pragma region opencl_overhead
    // Load the kernel source code into the array source_str
    FILE* fp;
    char* source_str;
    size_t source_size;

    fp = fopen("latency_kernel.cl", "r");
    if (!fp) {
        fprintf(stderr, "Failed to load kernel.\n");
        exit(1);
    }
    source_str = (char*)malloc(MAX_SOURCE_SIZE);
    source_size = fread(source_str, 1, MAX_SOURCE_SIZE, fp);
    fclose(fp);
    fprintf(stderr, "kernel loading done\n");
    
    // Create an OpenCL context
    cl_context context = get_context_from_user(platform_index, device_index);
    if (context == NULL) exit(1);

    // Create a command queue
    cl_command_queue command_queue = clCreateCommandQueue(context, selected_device_id, 0, &ret);
    fprintf(stderr, "clCreateCommandQueue returned %d\n",ret);

    // Create a program from the kernel source
    cl_program program = clCreateProgramWithSource(context, 1, (const char**)&source_str, (const size_t*)&source_size, &ret);
    //printf("clCreateProgramWithSource returned %d\n", ret);

    // Build the program
    ret = clBuildProgram(program, 1, &selected_device_id, NULL, NULL, NULL);
    fprintf(stderr, "clBuildProgram returned %d\n", ret);

    if (ret == -11)
    {
        size_t log_size;
        fprintf(stderr, "OpenCL kernel build error\n");
        clGetProgramBuildInfo(program, selected_device_id, CL_PROGRAM_BUILD_LOG, 0, NULL, &log_size);
        char* log = (char*)malloc(log_size);
        clGetProgramBuildInfo(program, selected_device_id, CL_PROGRAM_BUILD_LOG, log_size, log, NULL);
        fprintf(stderr, "%s\n", log);
        free(log);
    }

    // Create the OpenCL kernel. both simple and unrolled kernels take the same parameters
    cl_kernel kernel = clCreateKernel(program, "unrolled_latency_test", &ret);
    cl_kernel parallel_kernel = clCreateKernel(program, "parallel_latency_test", &ret);
    cl_kernel constant_kernel = clCreateKernel(program, "constant_unrolled_latency_test", &ret);
    cl_kernel int_exec_latency_test_kernel = clCreateKernel(program, "int_exec_latency_test", &ret);
    cl_kernel atomic_latency_test_kernel = clCreateKernel(program, "atomic_exec_latency_test", &ret);
    cl_kernel local_atomic_latency_test_kernel = clCreateKernel(program, "local_atomic_latency_test", &ret);
#pragma endregion opencl_overhead

    cl_ulong max_global_test_size = get_max_buffer_size();

    // this is a dumb idea
    /*printf("\nSattolo, global memory parallel pointer chasing BW(up to %lld K) unroll:\n", max_global_test_size / 1024);

    for (int size_idx = 0; size_idx < sizeof(default_test_sizes) / sizeof(int); size_idx++) {
        if (max_global_test_size < sizeof(int) * 256 * default_test_sizes[size_idx]) {
            printf("%d K would exceed device's max buffer size of %lld K, stopping here.\n", default_test_sizes[size_idx], max_global_test_size / 1024);
            break;
        }
        latency = latency_bw_test(context, command_queue, parallel_kernel, 
            256 * default_test_sizes[size_idx], // list size
            262144, // thread count
            256, // local size
            chase_iterations / default_test_sizes[size_idx]); // iterations

        printf("%d,%f\n", default_test_sizes[size_idx], latency);
        if (latency == 0) {
            printf("Something went wrong, not testing anything bigger.\n");
            break;
        }
    }*/

    

    //latency = int_exec_latency_test(context, command_queue, int_exec_latency_test_kernel, chase_iterations);
    //printf("int latency: %f\n", latency);
    latency = int_atomic_latency_test(context, command_queue, atomic_latency_test_kernel, chase_iterations, false);
    printf("global atomic latency: %f\n", latency);
    latency = int_atomic_latency_test(context, command_queue, local_atomic_latency_test_kernel, chase_iterations, true);
    printf("local atomic latency: %f\n", latency);

    printf("\nSattolo, global memory latency (up to %lld K) unroll:\n", max_global_test_size / 1024);

    for (int size_idx = 0; size_idx < sizeof(default_test_sizes) / sizeof(int); size_idx++) {
        if (max_global_test_size < sizeof(int) * 256 * default_test_sizes[size_idx]) {
            printf("%d K would exceed device's max buffer size of %lld K, stopping here.\n", default_test_sizes[size_idx], max_global_test_size / 1024);
            break;
        }
        latency = latency_test(context, command_queue, kernel, 256 * default_test_sizes[size_idx], chase_iterations, true);
        printf("%d,%f\n", default_test_sizes[size_idx], latency);
        if (latency == 0) {
            printf("Something went wrong, not testing anything bigger.\n");
            break;
        }
    }

    cl_ulong max_constant_test_size = get_max_constant_buffer_size();
    printf("\nSattolo, constant memory (up to %lld K), no-unroll:\n", max_constant_test_size / 1024);

    for (int size_idx = 0; size_idx < sizeof(default_test_sizes) / sizeof(int); size_idx++) {
        if (max_constant_test_size < sizeof(int) * 256 * default_test_sizes[size_idx]) {
            printf("%d K would exceed device's max constant buffer size of %lld K, stopping here.\n", default_test_sizes[size_idx], max_constant_test_size / 1024);
            break;
        }
        latency = latency_test(context, command_queue, constant_kernel, 256 * default_test_sizes[size_idx], chase_iterations, false);
        printf("%d,%f\n", default_test_sizes[size_idx], latency);
        if (latency == 0) {
            printf("Something went wrong, not testing anything bigger.\n");
            break;
        }
    }

    printf("If you didn't run this through cmd, now you can copy the results. And press ctrl+c to close");
    scanf("\n");

    // Clean up
    cleanup:
    ret = clFlush(command_queue);
    ret = clFinish(command_queue);
    ret = clReleaseKernel(kernel);
    ret = clReleaseProgram(program);
    ret = clReleaseCommandQueue(command_queue);
    ret = clReleaseContext(context);
    free(source_str);
    return 0;
}

float int_atomic_latency_test(cl_context context,
    cl_command_queue command_queue,
    cl_kernel kernel,
    uint32_t iterations,
    bool local)
{
    struct timeb start, end;
    cl_int ret;
    cl_int result = 0;
    size_t global_item_size = 2;
    size_t local_item_size = 1;
    float latency;
    uint32_t time_diff_ms;
    uint32_t A = 0;

    if (local)
    {
        local_item_size = 2;
    }

    cl_mem a_mem_obj = clCreateBuffer(context, CL_MEM_READ_ONLY, sizeof(uint32_t), NULL, &ret);
    cl_mem result_obj = clCreateBuffer(context, CL_MEM_READ_WRITE, sizeof(cl_int), NULL, &result);
    ret = clEnqueueWriteBuffer(command_queue, a_mem_obj, CL_TRUE, 0, sizeof(uint32_t), &A, 0, NULL, NULL);
    ret = clEnqueueWriteBuffer(command_queue, result_obj, CL_TRUE, 0, sizeof(cl_int), &result, 0, NULL, NULL);
    clFinish(command_queue);
    clSetKernelArg(kernel, 0, sizeof(cl_mem), (void*)&a_mem_obj);
    clSetKernelArg(kernel, 1, sizeof(cl_int), (void*)&iterations);
    clSetKernelArg(kernel, 2, sizeof(cl_mem), (void*)&result_obj);

    ftime(&start);
    ret = clEnqueueNDRangeKernel(command_queue, kernel, 1, NULL, &global_item_size, &local_item_size, 0, NULL, NULL);
    if (ret != CL_SUCCESS)
    {
        fprintf(stderr, "Failed to submit kernel to command queue. clEnqueueNDRangeKernel returned %d\n", ret);
        latency = 0;
        goto cleanup;
    }
    clFinish(command_queue);
    ftime(&end);
    time_diff_ms = 1000 * (end.time - start.time) + (end.millitm - start.millitm);
    latency = (1e6 * (float)time_diff_ms / (float)(iterations)) / 2;

cleanup:
    clFlush(command_queue);
    clFinish(command_queue);
    clReleaseMemObject(a_mem_obj);
    clReleaseMemObject(result_obj);
    return latency;
}

#define INT_EXEC_INPUT_SIZE 16
float int_exec_latency_test(cl_context context,
    cl_command_queue command_queue,
    cl_kernel kernel,
    uint32_t iterations)
{
    struct timeb start, end;
    cl_int ret;
    cl_int result = 0;
    size_t global_item_size = 1;
    size_t local_item_size = 1;
    float latency;
    uint32_t time_diff_ms;
    uint32_t A[INT_EXEC_INPUT_SIZE];
    
    for (int i = 0; i < INT_EXEC_INPUT_SIZE; i++) A[i] = i;

    cl_mem a_mem_obj = clCreateBuffer(context, CL_MEM_READ_ONLY, INT_EXEC_INPUT_SIZE * sizeof(uint32_t), NULL, &ret);
    cl_mem result_obj = clCreateBuffer(context, CL_MEM_READ_WRITE, sizeof(cl_int), NULL, &result);
    ret = clEnqueueWriteBuffer(command_queue, a_mem_obj, CL_TRUE, 0, INT_EXEC_INPUT_SIZE * sizeof(uint32_t), A, 0, NULL, NULL);
    ret = clEnqueueWriteBuffer(command_queue, result_obj, CL_TRUE, 0, sizeof(cl_int), &result, 0, NULL, NULL);
    clFinish(command_queue);
    clSetKernelArg(kernel, 0, sizeof(cl_mem), (void*)&a_mem_obj);
    clSetKernelArg(kernel, 1, sizeof(cl_int), (void*)&iterations);
    clSetKernelArg(kernel, 2, sizeof(cl_mem), (void*)&result_obj);

    ftime(&start);
    ret = clEnqueueNDRangeKernel(command_queue, kernel, 1, NULL, &global_item_size, &local_item_size, 0, NULL, NULL);
    if (ret != CL_SUCCESS)
    {
        fprintf(stderr, "Failed to submit kernel to command queue. clEnqueueNDRangeKernel returned %d\n", ret);
        latency = 0;
        goto cleanup;
    }
    clFinish(command_queue);
    ftime(&end);
    time_diff_ms = 1000 * (end.time - start.time) + (end.millitm - start.millitm);
    latency = 1e6 * (float)time_diff_ms / (float)(iterations * 12);

cleanup:
    clFlush(command_queue);
    clFinish(command_queue);
    clReleaseMemObject(a_mem_obj);
    clReleaseMemObject(result_obj);
    return latency;
}

float latency_bw_test(cl_context context,
    cl_command_queue command_queue,
    cl_kernel kernel,
    uint32_t list_size,
    uint32_t thread_count,
    uint32_t local_size,
    uint32_t chase_iterations)
{
    size_t global_item_size = thread_count;
    size_t local_item_size = local_size;
    float bandwidth, total_data_gb;
    struct timeb start, end;
    cl_int ret;
    int64_t time_diff_ms;

    uint32_t* A = (uint32_t*)malloc(sizeof(uint32_t) * list_size);
    cl_int* result = (cl_int*)malloc(sizeof(cl_int) * thread_count);
    for (uint32_t i = 0; i < list_size; i++)
    {
        A[i] = i;
    }

    int iter = list_size;
    while (iter > 1)
    {
        iter -= 1;
        int j = iter - 1 == 0 ? 0 : rand() % (iter - 1);
        uint32_t tmp = A[iter];
        A[iter] = A[j];
        A[j] = tmp;
    }

    // copy array to device
    cl_mem a_mem_obj = clCreateBuffer(context, CL_MEM_READ_ONLY, list_size * sizeof(uint32_t), NULL, &ret);
    ret = clEnqueueWriteBuffer(command_queue, a_mem_obj, CL_TRUE, 0, list_size * sizeof(uint32_t), A, 0, NULL, NULL);

    cl_mem result_obj = clCreateBuffer(context, CL_MEM_READ_WRITE, sizeof(cl_int) * thread_count, NULL, &ret);
    //fprintf(stderr, "create result buffer = %d\n", ret);
    ret = clEnqueueWriteBuffer(command_queue, result_obj, CL_TRUE, 0, sizeof(cl_int) * thread_count, result, 0, NULL, NULL);
    //fprintf(stderr, "copy result buffer = %d\n", ret);

    // Set kernel arguments for parallel_latency_test(__global const int* A, int count, int size, __global int* ret)
    clSetKernelArg(kernel, 0, sizeof(cl_mem), (void*)&a_mem_obj);
    clSetKernelArg(kernel, 1, sizeof(cl_int), (void*)&chase_iterations);
    clSetKernelArg(kernel, 2, sizeof(cl_int), (void*)&list_size);
    clSetKernelArg(kernel, 3, sizeof(cl_mem), (void*)&result_obj);

    ftime(&start);
    // Execute the OpenCL kernel. launch a single thread
    ret = clEnqueueNDRangeKernel(command_queue, kernel, 1, NULL, &global_item_size, &local_item_size, 0, NULL, NULL);
    if (ret != CL_SUCCESS)
    {
        fprintf(stderr, "Failed to submit kernel to command queue. clEnqueueNDRangeKernel returned %d\n", ret);
        bandwidth = 0;
        goto cleanup;
    }

    ret = clFinish(command_queue); // returns success even when TDR happens?
    if (ret != CL_SUCCESS)
    {
        printf("Failed to finish command queue. clFinish returned %d\n", ret);
        bandwidth = 0;
        goto cleanup;
    }

    ftime(&end);
    time_diff_ms = 1000 * (end.time - start.time) + (end.millitm - start.millitm);

    // each thread does iterations reads
    total_data_gb = sizeof(cl_int) * ((float)chase_iterations * thread_count + thread_count)/ 1e9;
    bandwidth = 1000 * (float)total_data_gb / (float)time_diff_ms;

    //fprintf(stderr, "%llu ms, %llu GB\n", time_diff_ms, total_data_gb);

    ret = clEnqueueReadBuffer(command_queue, result_obj, CL_TRUE, 0, sizeof(uint32_t) * thread_count, result, 0, NULL, NULL);
    clFinish(command_queue);

    //fprintf(stderr, "Finished reading result. Sum: %d\n", result[0]);

cleanup:
    clFlush(command_queue);
    clFinish(command_queue);
    clReleaseMemObject(a_mem_obj);
    clReleaseMemObject(result_obj);
    free(A);
    free(result);
    return bandwidth;
}

float latency_test(cl_context context, 
    cl_command_queue command_queue, 
    cl_kernel kernel, 
    uint32_t list_size, 
    uint32_t chase_iterations,
    bool sattolo)
{
    size_t global_item_size = 1, local_item_size = 1;
    cl_int ret;
    float latency;
    struct timeb start, end;
    int64_t time_diff_ms;
    uint32_t result;
    uint32_t stride = 1211;
    int* A = (int*)malloc(sizeof(int) * list_size);
    if (sattolo) {
        for (int i = 0; i < list_size; i++)
        {
            A[i] = i;
        }

        int iter = list_size;
        while (iter > 1)
        {
            iter -= 1;
            int j = iter - 1 == 0 ? 0 : rand() % (iter - 1);
            uint32_t tmp = A[iter];
            A[iter] = A[j];
            A[j] = tmp;
        }
    } else {
        for (int i = 0; i < list_size; i++)
        {
            A[i] = (i + stride) % list_size;
        }
    }

    // copy array to device
    cl_mem a_mem_obj = clCreateBuffer(context, CL_MEM_READ_ONLY, list_size * sizeof(uint32_t), NULL, &ret);
    clEnqueueWriteBuffer(command_queue, a_mem_obj, CL_TRUE, 0, list_size * sizeof(uint32_t), A, 0, NULL, NULL);

    cl_mem result_obj = clCreateBuffer(context, CL_MEM_READ_WRITE, sizeof(uint32_t), NULL, &ret);
    clEnqueueWriteBuffer(command_queue, result_obj, CL_TRUE, 0, sizeof(uint32_t), &result, 0, NULL, NULL);
    clFinish(command_queue);
    
    // Set kernel arguments
    ret = clSetKernelArg(kernel, 0, sizeof(cl_mem), (void*)&a_mem_obj);
    if (ret != CL_SUCCESS)
    {
        fprintf(stderr, "Failed to set list as kernel arg. clSetKernelArg returned %d\n", ret);
        latency = 0;
        goto cleanup;
    }

    ret = clSetKernelArg(kernel, 1, sizeof(cl_int), (void*)&chase_iterations);
    ret = clSetKernelArg(kernel, 2, sizeof(cl_mem), (void*)&result_obj);

    ftime(&start);
    // Execute the OpenCL kernel. launch a single thread
    ret = clEnqueueNDRangeKernel(command_queue, kernel, 1, NULL, &global_item_size, &local_item_size, 0, NULL, NULL);
    if (ret != CL_SUCCESS)
    {
        fprintf(stderr, "Failed to submit kernel to command queue. clEnqueueNDRangeKernel returned %d\n", ret);
        latency = 0;
        goto cleanup;
    }

    ret = clFinish(command_queue); // returns success even when TDR happens?
    if (ret != CL_SUCCESS)
    {
        printf("Failed to finish command queue. clFinish returned %d\n", ret);
        latency = 0;
        goto cleanup;
    }

    ftime(&end);
    time_diff_ms = 1000 * (end.time - start.time) + (end.millitm - start.millitm);
    latency = 1e6 * (float)time_diff_ms / (float)chase_iterations;

    ret = clEnqueueReadBuffer(command_queue, result_obj, CL_TRUE, 0, sizeof(uint32_t), &result, 0, NULL, NULL);
    clFinish(command_queue);

    //fprintf(stderr, "Finished reading result. Sum: %d\n", result[0]);

cleanup:
    clFlush(command_queue);
    clFinish(command_queue);
    clReleaseMemObject(a_mem_obj);
    clReleaseMemObject(result_obj);
    free(A);
    return latency;
}

/// <summary>
/// populate global variables for opencl device id and platform id
/// </summary>
/// <param name="platform_index">platform index. if -1, prompt user</param>
/// <param name="device_index">device index. if -1. prompt user</param>
/// <returns>opencl context</returns>
cl_context get_context_from_user(int platform_index, int device_index) {
    int i = 0;
    int selected_platform_index = 0, selected_device_index = 0;
    // Get platform and device information
    cl_uint ret_num_devices;
    cl_uint ret_num_platforms;

    cl_int ret = clGetPlatformIDs(0, NULL, &ret_num_platforms);
    cl_platform_id *platforms = NULL;
    cl_device_id* devices = NULL;
    cl_context context = NULL;
    platforms = (cl_platform_id*)malloc(ret_num_platforms * sizeof(cl_platform_id));

    ret = clGetPlatformIDs(ret_num_platforms, platforms, NULL);
    fprintf(stderr, "clGetPlatformIDs returned %d. %d platforms\n", ret, ret_num_platforms);

    for (i = 0; i < ret_num_platforms; i++)
    {
        size_t platform_name_len;
        char* platform_name = NULL;
        if (CL_SUCCESS != clGetPlatformInfo(platforms[i], CL_PLATFORM_NAME, 0, NULL, &platform_name_len)) {
            fprintf(stderr, "Failed to get platform info for platform %d\n", i);
            continue;
        }

        platform_name = (char*)malloc(platform_name_len + 1);
        platform_name[platform_name_len] = 0;

        if (CL_SUCCESS != clGetPlatformInfo(platforms[i], CL_PLATFORM_NAME, platform_name_len, platform_name, NULL)) {
            fprintf(stderr, "Failed to get platform name for platform %d\n", i);
            free(platform_name);
            continue;
        }

        fprintf(stderr, "Platform %d: %s\n", i, platform_name);
        free(platform_name);
    }

    selected_platform_index = platform_index;
    if (selected_platform_index == -1)
    {
        printf("Enter platform #:");
        scanf("%d", &selected_platform_index);
    }

    if (selected_platform_index > ret_num_platforms - 1)
    {
        fprintf(stderr, "platform index out of range\n");
        goto get_context_from_user_end;
    }

    selected_platform_id = platforms[selected_platform_index];

    if (CL_SUCCESS != clGetDeviceIDs(selected_platform_id, CL_DEVICE_TYPE_ALL, 0, NULL, &ret_num_devices)) {
        fprintf(stderr, "Failed to enumerate device ids for platform");
        return NULL;
    }

    devices = (cl_device_id*)malloc(ret_num_devices * sizeof(cl_device_id));
    if (CL_SUCCESS != clGetDeviceIDs(selected_platform_id, CL_DEVICE_TYPE_ALL, ret_num_devices, devices, NULL)) {
        fprintf(stderr, "Failed to get device ids for platform");
        free(devices);
        return NULL;
    }

    fprintf(stderr, "clGetDeviceIDs returned %d devices\n", ret_num_devices);

    for (i = 0; i < ret_num_devices; i++)
    {
        size_t device_name_len;
        char* device_name = NULL;
        if (CL_SUCCESS != clGetDeviceInfo(devices[i], CL_DEVICE_NAME, 0, NULL, &device_name_len)) {
            fprintf(stderr, "Failed to get name length for device %d\n", i);
            continue;
        }

        //fprintf(stderr, "debug: device name length: %d\n", device_name_len);
        device_name = (char*)malloc(device_name_len + 1);
        device_name[device_name_len] = 0;

        if (CL_SUCCESS != clGetDeviceInfo(devices[i], CL_DEVICE_NAME, device_name_len, device_name, &device_name_len)) {
            fprintf(stderr, "Failed to get name for device %d\n", i);
            free(device_name);
            continue;
        }

        fprintf(stderr, "Device %d: %s\n", i, device_name);
        free(device_name);
    }

    selected_device_index = device_index;
    if (selected_device_index == -1)
    {
        fprintf(stderr, "Enter device #:");
        scanf("%d", &selected_device_index);
    }

    
    if (selected_device_index > ret_num_devices - 1)
    {
        fprintf(stderr, "Device index out of range\n");
        goto get_context_from_user_end;
    }

    selected_device_id = devices[selected_device_index];

    // Create an OpenCL context
    context = clCreateContext(NULL, 1, &selected_device_id, NULL, NULL, &ret);
    fprintf(stderr, "clCreateContext returned %d\n", ret);

    get_context_from_user_end:
    free(platforms);
    free(devices);
    return context;
}

cl_ulong get_max_constant_buffer_size() {
    cl_ulong constant_buffer_size = 0;
    if (CL_SUCCESS != clGetDeviceInfo(selected_device_id, CL_DEVICE_MAX_CONSTANT_BUFFER_SIZE, sizeof(cl_ulong), &constant_buffer_size, NULL)) {
        fprintf(stderr, "Failed to get max constant buffer size\n");
    }

    return constant_buffer_size;
}

cl_ulong get_max_buffer_size() {
    cl_ulong buffer_size = 0;
    if (CL_SUCCESS != clGetDeviceInfo(selected_device_id, CL_DEVICE_MAX_MEM_ALLOC_SIZE, sizeof(cl_ulong), &buffer_size, NULL)) {
        fprintf(stderr, "Failed to get max constant buffer size\n");
    }

    return buffer_size;
}