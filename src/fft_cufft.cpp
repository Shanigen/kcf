#include "fft_cufft.h"

void cuFFT::init(unsigned width, unsigned height, unsigned num_of_feats, unsigned num_of_scales, bool big_batch_mode)
{
    m_width = width;
    m_height = height;
    m_num_of_feats = num_of_feats;
    m_num_of_scales = num_of_scales;
    m_big_batch_mode = big_batch_mode;

    std::cout << "FFT: cuFFT" << std::endl;

    //FFT forward one scale
    {
       CufftErrorCheck(cufftPlan2d(&plan_f, int(m_height), int(m_width), CUFFT_R2C));
    }
#ifdef BIG_BATCH
    //FFT forward all scales
    if(m_num_of_scales > 1 && m_big_batch_mode)
    {
        int rank = 2;
        int n[] = {(int)m_height, (int)m_width};
        int howmany = m_num_of_scales;
        int idist = m_height*m_width, odist = m_height*(m_width/2+1);
        int istride = 1, ostride = 1;
        int *inembed = n, onembed[] = {(int)m_height, (int)m_width/2+1};

        CufftErrorCheck(cufftPlanMany(&plan_f_all_scales, rank, n,
		  inembed, istride, idist,
		  onembed, ostride, odist,
		  CUFFT_R2C, howmany));
    }
#endif
    //FFT forward window one scale
    {
        int rank = 2;
        int n[] = {int(m_height), int(m_width)};
        int howmany = int(m_num_of_feats);
        int idist = int(m_height*m_width), odist = int(m_height*(m_width/2+1));
        int istride = 1, ostride = 1;
        int *inembed = n, onembed[] = {int(m_height), int(m_width/2+1)};

        CufftErrorCheck(cufftPlanMany(&plan_fw, rank, n,
		  inembed, istride, idist,
		  onembed, ostride, odist,
		  CUFFT_R2C, howmany));
    }
#ifdef BIG_BATCH
    //FFT forward window all scales all feats
    if(m_num_of_scales > 1 && m_big_batch_mode)
    {
        int rank = 2;
        int n[] = {(int)m_height, (int)m_width};
        int howmany = m_num_of_scales*m_num_of_feats;
        int idist = m_height*m_width, odist = m_height*(m_width/2+1);
        int istride = 1, ostride = 1;
        int *inembed = n, onembed[] = {(int)m_height, (int)m_width/2+1};

        CufftErrorCheck(cufftPlanMany(&plan_fw_all_scales, rank, n,
		  inembed, istride, idist,
		  onembed, ostride, odist,
		  CUFFT_R2C, howmany));
    }
#endif
    //FFT inverse one scale
    {
        int rank = 2;
        int n[] = {int(m_height), int(m_width)};
        int howmany = int(m_num_of_feats);
        int idist = int(m_height*(m_width/2+1)), odist = 1;
        int istride = 1, ostride = int(m_num_of_feats);
        int inembed[] = {int(m_height), int(m_width/2+1)}, *onembed = n;

        CufftErrorCheck(cufftPlanMany(&plan_i_features, rank, n,
		  inembed, istride, idist,
		  onembed, ostride, odist,
		  CUFFT_C2R, howmany));
    }
    //FFT inverse all scales
#ifdef BIG_BATCH
    if(m_num_of_scales > 1 && m_big_batch_mode)
    {
        int rank = 2;
        int n[] = {(int)m_height, (int)m_width};
        int howmany = m_num_of_feats*m_num_of_scales;
        int idist = m_height*(m_width/2+1), odist = 1;
        int istride = 1, ostride = m_num_of_feats*m_num_of_scales;
        int inembed[] = {(int)m_height, (int)m_width/2+1}, *onembed = n;

        CufftErrorCheck(cufftPlanMany(&plan_i_features_all_scales, rank, n,
		  inembed, istride, idist,
		  onembed, ostride, odist,
		  CUFFT_C2R, howmany));
    }
#endif
    //FFT inverse one channel one scale
    {
        int rank = 2;
        int n[] = {int(m_height), int(m_width)};
        int howmany = 1;
        int idist = int(m_height*(m_width/2+1)), odist = 1;
        int istride = 1, ostride = 1;
        int inembed[] = {int(m_height), int(m_width/2+1)}, *onembed = n;

        CufftErrorCheck(cufftPlanMany(&plan_i_1ch, rank, n,
		  inembed, istride, idist,
		  onembed, ostride, odist,
		  CUFFT_C2R, howmany));
    }
#ifdef BIG_BATCH
    //FFT inverse one channel all scales
    if(m_num_of_scales > 1 && m_big_batch_mode)
    {
        int rank = 2;
        int n[] = {(int)m_height, (int)m_width};
        int howmany = m_num_of_scales;
        int idist = m_height*(m_width/2+1), odist = 1;
        int istride = 1, ostride = m_num_of_scales;
        int inembed[] = {(int)m_height, (int)m_width/2+1}, *onembed = n;

        CufftErrorCheck(cufftPlanMany(&plan_i_1ch_all_scales, rank, n,
		  inembed, istride, idist,
		  onembed, ostride, odist,
		  CUFFT_C2R, howmany));
    }
#endif
}

void cuFFT::set_window(const cv::Mat & window)
{
     m_window = window;
}

void cuFFT::forward(const cv::Mat & real_input, ComplexMat & complex_result, float *real_input_arr, cudaStream_t stream)
{
    (void) real_input;

    if(m_big_batch_mode && real_input.rows == int(m_height*m_num_of_scales)){
        CufftErrorCheck(cufftExecR2C(plan_f_all_scales, reinterpret_cast<cufftReal*>(real_input_arr),
                                complex_result.get_p_data()));
    } else {
#pragma omp critical
        {
        CufftErrorCheck(cufftSetStream(plan_f, stream));
        CufftErrorCheck(cufftExecR2C(plan_f, reinterpret_cast<cufftReal*>(real_input_arr), complex_result.get_p_data()));
        cudaStreamSynchronize(stream);
        }
    }
    return;
}

void cuFFT::forward_window(std::vector<cv::Mat> patch_feats, ComplexMat & complex_result, cv::Mat & fw_all, float *real_input_arr, cudaStream_t stream)
{
    int n_channels = int(patch_feats.size());

    if(n_channels > int(m_num_of_feats)){
        for (uint i = 0; i < uint(n_channels); ++i) {
            cv::Mat in_roi(fw_all, cv::Rect(0, int(i*m_height), int(m_width), int(m_height)));
            in_roi = patch_feats[i].mul(m_window);
        }
        CufftErrorCheck(cufftExecR2C(plan_fw_all_scales, reinterpret_cast<cufftReal*>(real_input_arr), complex_result.get_p_data()));
    } else {
        for (uint i = 0; i < uint(n_channels); ++i) {
            cv::Mat in_roi(fw_all, cv::Rect(0, int(i*m_height), int(m_width), int(m_height)));
            in_roi = patch_feats[i].mul(m_window);
        }
#pragma omp critical
        {
        CufftErrorCheck(cufftSetStream(plan_fw, stream));
        CufftErrorCheck(cufftExecR2C(plan_fw, reinterpret_cast<cufftReal*>(real_input_arr), complex_result.get_p_data()));
        cudaStreamSynchronize(stream);
        }
    }
    return;
}

void cuFFT::inverse(ComplexMat &  complex_input, cv::Mat & real_result, float *real_result_arr, cudaStream_t stream)
{
    int n_channels = complex_input.n_channels;
    cufftComplex *in = reinterpret_cast<cufftComplex*>(complex_input.get_p_data());

    if(n_channels == 1){
#pragma omp critical
        {
        CufftErrorCheck(cufftSetStream(plan_i_1ch, stream));
        CufftErrorCheck(cufftExecC2R(plan_i_1ch, in, reinterpret_cast<cufftReal*>(real_result_arr)));
        cudaStreamSynchronize(stream);
        }
        real_result = real_result/(m_width*m_height);
        return;
    } else if(n_channels == int(m_num_of_scales)){
        CufftErrorCheck(cufftExecC2R(plan_i_1ch_all_scales, in, reinterpret_cast<cufftReal*>(real_result_arr)));
        cudaStreamSynchronize(stream);

        real_result = real_result/(m_width*m_height);
        return;
    } else if(n_channels == int(m_num_of_feats) * int(m_num_of_scales)){
        CufftErrorCheck(cufftExecC2R(plan_i_features_all_scales, in, reinterpret_cast<cufftReal*>(real_result_arr)));
        return;
    }
#pragma omp critical
    {
    CufftErrorCheck(cufftSetStream(plan_i_features, stream));
    CufftErrorCheck(cufftExecC2R(plan_i_features, in, reinterpret_cast<cufftReal*>(real_result_arr)));
#if defined(OPENMP) && !defined(BIG_BATCH)
    cudaStreamSynchronize(stream);
#endif
    }
    return;
}

cuFFT::~cuFFT()
{
  CufftErrorCheck(cufftDestroy(plan_f));
  CufftErrorCheck(cufftDestroy(plan_fw));
  CufftErrorCheck(cufftDestroy(plan_i_1ch));
  CufftErrorCheck(cufftDestroy(plan_i_features));
  
  if (m_big_batch_mode) {
      CufftErrorCheck(cufftDestroy(plan_f_all_scales));
      CufftErrorCheck(cufftDestroy(plan_fw_all_scales));
      CufftErrorCheck(cufftDestroy(plan_i_1ch_all_scales));
      CufftErrorCheck(cufftDestroy(plan_i_features_all_scales));
  }
}
