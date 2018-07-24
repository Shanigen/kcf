
#ifndef FFT_CUDA_H
#define FFT_CUDA_H

#include "fft.h"
#include "cuda/cuda_error_check.cuh"

#if CV_MAJOR_VERSION == 2
  #include <opencv2/gpu/gpu.hpp>
  #define CUDA cv::gpu
#else
  #include "opencv2/opencv.hpp"
  #define CUDA cv::cuda
#endif

#include <cufft.h>
#include <cuda_runtime.h>

struct Scale_vars;

class cuFFT : public Fft
{
public:
    void init(unsigned width, unsigned height, unsigned num_of_feats, unsigned num_of_scales, bool big_batch_mode) override;
    void set_window(const cv::Mat & window) override;
    void forward(const cv::Mat & real_input, ComplexMat & complex_result, float *real_input_arr, cudaStream_t  stream) override;
    void forward_window(std::vector<cv::Mat> patch_feats, ComplexMat & complex_result, cv::Mat & fw_all, float *real_input_arr, cudaStream_t stream) override;
    void inverse(ComplexMat &  complex_input, cv::Mat & real_result, float *real_result_arr, cudaStream_t stream) override;
    ~cuFFT() override;
private:
    cv::Mat m_window;
    unsigned m_width, m_height, m_num_of_feats, m_num_of_scales;
    bool m_big_batch_mode;
    cufftHandle plan_f, plan_f_all_scales, plan_fw, plan_fw_all_scales, plan_i_features,
     plan_i_features_all_scales, plan_i_1ch, plan_i_1ch_all_scales;
};

#endif // FFT_CUDA_H
