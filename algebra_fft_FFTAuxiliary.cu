#include<iostream>
#include<stdexcept>
#include<unistd.h>
#include<cstring>
#include <bitset>
#include <vector>
#include <cmath>
#include <chrono>
#include "algebra_fft_FFTAuxiliary.h"
#include <cuda.h>
#include <cuda_runtime.h>
#include <stdio.h>
#include <assert.h>
#include <vector>
#include <iostream>
#include "device_field.h"

using namespace std;

#define LOG_NUM_THREADS 2
#define NUM_THREADS (1 << LOG_NUM_THREADS)
#define LOG_CONSTRAINTS 14
#define CONSTRAINTS (1 << LOG_CONSTRAINTS)

#define CUDA_CALL( call )               \
{                                       \
cudaError_t result = call;              \
if ( cudaSuccess != result )            \
    std::cerr << "CUDA error " << result << " in " << __FILE__ << ":" << __LINE__ << ": " << cudaGetErrorString( result ) << " (" << #call << ")" << std::endl;  \
}

int reverseBits(int n, int range) {
    int ans = 0;
    for(int i = range - 1; i >= 0; i--){
        ans |= (n & 1) <<i;
        n>>=1;
    }
    return ans;
}


__device__ __forceinline__
size_t bitreverse(size_t n, const size_t l)
{
    return __brevll(n) >> (64ull - l); 
}


__global__ void cuda_fft_first_step(Scalar *input_field, Scalar omega, const size_t length, const size_t log_m) {
    const int idx = blockIdx.x * blockDim.x + threadIdx.x;
    //printf("blockIdx=%d, blockDim.x=%d, threadIdx.x=%d, idx=%d\n",blockIdx.x, blockDim.x, threadIdx.x, idx);
    const size_t block_length = 1ul << (log_m - LOG_NUM_THREADS) ; //TODO lianke when log_m is smaller than log_num_threads,  there is a bug.
    //printf("block length %d\n", block_length);
    const size_t startidx = idx * block_length;
    if(startidx > length)
        return;

    /* swapping in place (from Storer's book) */
    for (size_t k = 0; k < block_length; ++k)
    {
        size_t global_k = startidx + k;
        size_t rk = bitreverse(global_k, log_m);
        
        //printf("idx = %d, reverse %d and %d\n", startidx, global_k, rk);
        if (global_k < rk  && rk < length)
        {
            
            Scalar tmp = input_field[global_k];
            input_field[global_k] = input_field[rk];
            input_field[rk] = tmp;
        }
    }
    __syncthreads();
    
}


__global__ void cuda_fft_second_step(Scalar *input_field, Scalar omega, const size_t length, const size_t log_m) {
    const int idx = blockIdx.x * blockDim.x + threadIdx.x;
    //printf("blockIdx=%d, blockDim.x=%d, threadIdx.x=%d, idx=%d\n",blockIdx.x, blockDim.x, threadIdx.x, idx);
    const size_t block_length = 1ul << (log_m - LOG_NUM_THREADS) ;
    const size_t startidx = idx * block_length;
    if(startidx < length){
        //TODO Lianke this for loop is wrong.
        size_t m = 1; // invariant: m = 2^{s-1}
        for (size_t s = 1; s <= 1; ++s)
        {
            // w_m is 2^s-th root of unity now
            const Scalar w_m = omega^(length/(2*m));
            
            for (size_t k = 0; k < block_length; k += 2*m)
            {
                size_t global_k = startidx + k;
                Scalar w = Scalar::one();
                for (size_t j = 0; j < m; ++j)
                {
                    Scalar t = w;
                    t = w * input_field[global_k+j+m];
                    input_field[global_k+j+m] = input_field[global_k+j] - t;
                    input_field[global_k+j] = input_field[global_k+j] + t;
                    w = w * w_m;
                }
            }
            m = m * 2;
        }
    }
    __syncthreads();

}




void best_fft (std::vector<Scalar> &a, const Scalar &omg)
{
	int cnt;
    cudaGetDeviceCount(&cnt);
    printf("CUDA Devices: %d, input_field size: %lu, input_field count: %lu\n", cnt, sizeof(Scalar), a.size());

    size_t threads = NUM_THREADS > 128 ? 128 : NUM_THREADS;
    size_t blocks = (NUM_THREADS + threads - 1) / threads;

    printf("NUM_THREADS %u, blocks %lu, threads %lu \n",NUM_THREADS, blocks, threads);

    Scalar *in;
    CUDA_CALL( cudaMalloc((void**)&in, sizeof(Scalar) * a.size()); )
    CUDA_CALL( cudaMemcpy(in, (void**)&a[0], sizeof(Scalar) * a.size(), cudaMemcpyHostToDevice); )

    const size_t length = a.size();
    const size_t log_m = log2(length); 
    //auto start = std::chrono::steady_clock::now();
    printf("launch block = %d thread = %d\n", blocks, threads);
    cuda_fft_first_step <<<blocks,threads>>>( in, omg, length, log_m);
    CUDA_CALL(cudaDeviceSynchronize());
    //cuda_fft_second_step <<<blocks,threads>>>( in, omg, length, log_m);
    CUDA_CALL(cudaDeviceSynchronize());

    // auto end = std::chrono::steady_clock::now();
    // std::chrono::duration<double> elapsed_seconds = end-start;
    // std::cout << "CUDA FFT elapsed time: " << elapsed_seconds.count() << "s\n";

    cudaError_t error = cudaGetLastError();
    if(error != cudaSuccess)
    {
        printf("CUDA error: %s\n", cudaGetErrorString(error));
        exit(-1);
    }

    CUDA_CALL(cudaMemcpy((void**)&a[0], in, sizeof(Scalar) * a.size(), cudaMemcpyDeviceToHost); )
    CUDA_CALL(cudaDeviceSynchronize());
    CUDA_CALL(cudaFree(in));
}

/*
 * Class:     algebra_fft_FFTAuxiliary
 * Method:    serialRadix2FFTNativeHelper
 * Signature: (Ljava/util/List;[B)[B
 */
JNIEXPORT jbyteArray JNICALL Java_algebra_fft_FFTAuxiliary_serialRadix2FFTNativeHelper
  (JNIEnv * env, jclass obj, jobject inputs, jbyteArray omegaArray){
    jclass java_util_ArrayList      = static_cast<jclass>(env->NewGlobalRef(env->FindClass("java/util/ArrayList")));
    jmethodID java_util_ArrayList_size = env->GetMethodID(java_util_ArrayList, "size", "()I");
    jmethodID java_util_ArrayList_get  = env->GetMethodID(java_util_ArrayList, "get", "(I)Ljava/lang/Object;");
    jint input_len = env->CallIntMethod(inputs, java_util_ArrayList_size);


    vector<Scalar> inputArray = vector<Scalar>(input_len, Scalar());
    //TODO lianke update copy from java
    for(int i =0; i < input_len; i++){
        jbyteArray element = (jbyteArray)env->CallObjectMethod(inputs, java_util_ArrayList_get, i);
        char* bytes = (char*)env->GetByteArrayElements(element, NULL);
        int len = env->GetArrayLength(element);
        char* tmp = (char*)&inputArray[i].im_rep;

        memcpy(tmp, 
                bytes,
                len);
    }


    Scalar omega;
    char* bytes = (char*)env->GetByteArrayElements(omegaArray, NULL);
    int len = env->GetArrayLength(omegaArray);
    char* tmp = (char*)&omega.im_rep;
    memcpy(tmp , 
                bytes,
                len);
    
    best_fft(inputArray, omega);


    jbyteArray resultByteArray = env->NewByteArray((jsize)Scalar::num_of_bytes * input_len);
    for(int i=0; i < input_len;i++){
        // cout <<"cpp side output=";
        // inputArray[i].printBinaryDebug();
        env->SetByteArrayRegion(resultByteArray, i * Scalar::num_of_bytes , Scalar::num_of_bytes,   reinterpret_cast<const jbyte*>(inputArray[i].im_rep));
    }


    return resultByteArray;

}

