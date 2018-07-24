#ifndef SCALE_VARS_HPP
#define SCALE_VARS_HPP

#ifdef CUFFT
  #include "complexmat.cuh"
#else
  #include "complexmat.hpp"
#ifndef CUFFTW
//For compatibility reasons between CuFFT and FFTW, OpenCVfft versions.
  typedef int* cudaStream_t;
#else
    #include "cuda_runtime.h"
#endif
#endif

#if defined(BIG_BATCH) && defined(OPENMP)
    #define BIG_BATCH_OMP_PARALLEL_FOR _Pragma("omp parallel for ordered")
    #define BIG_BATCH_OMP_ORDERED _Pragma("omp ordered")
    #define NORMAL_OMP_PARALLEL_FOR
    #define NORMAL_OMP_CRITICAL
#elif defined(OPENMP)
    #define BIG_BATCH_OMP_PARALLEL_FOR
    #define BIG_BATCH_OMP_ORDERED
    #define NORMAL_OMP_PARALLEL_FOR _Pragma("omp parallel for schedule(dynamic)")
    #define NORMAL_OMP_CRITICAL _Pragma("omp critical")
#else
    #define BIG_BATCH_OMP_PARALLEL_FOR
    #define BIG_BATCH_OMP_ORDERED
    #define NORMAL_OMP_PARALLEL_FOR
    #define NORMAL_OMP_CRITICAL
#endif

struct Scale_vars
{
public:
    Scale_vars(int windows_size[2], int cell_size, int num_of_feats, int num_of_scales = 1,ComplexMat *model_xf = nullptr,
                           ComplexMat *yf = nullptr,bool zero_index = false)
    {
        uint alloc_size;
#ifdef CUFFT
        if (zero_index) {
            cudaSetDeviceFlags(cudaDeviceMapHost);
            this->zero_index = true;
        }

#if defined(ASYNC) || defined(OPENMP)
        CudaSafeCall(cudaStreamCreate(&this->stream));
#endif

        this->patch_feats.reserve(uint(num_of_feats));

        alloc_size = uint(windows_size[0]/cell_size*windows_size[1]/cell_size*num_of_scales)*sizeof(cufftReal);
        CudaSafeCall(cudaHostAlloc(reinterpret_cast<void**>(&this->data_i_1ch), alloc_size, cudaHostAllocMapped));
        CudaSafeCall(cudaHostGetDevicePointer(reinterpret_cast<void**>(&this->data_i_1ch_d), reinterpret_cast<void*>(this->data_i_1ch), 0));

        alloc_size = uint(windows_size[0]/cell_size*windows_size[1]/cell_size*num_of_feats)*sizeof(cufftReal);
        CudaSafeCall(cudaHostAlloc(reinterpret_cast<void**>(&this->data_i_features), alloc_size, cudaHostAllocMapped));
        CudaSafeCall(cudaHostGetDevicePointer(reinterpret_cast<void**>(&this->data_i_features_d), reinterpret_cast<void*>(this->data_i_features), 0));

        this->ifft2_res = cv::Mat(windows_size[1]/cell_size, windows_size[0]/cell_size, CV_32FC(num_of_feats), this->data_i_features);
        this->response = cv::Mat(windows_size[1]/cell_size, windows_size[0]/cell_size, CV_32FC(num_of_scales), this->data_i_1ch);

        this->zf.create(windows_size[1]/cell_size, (windows_size[0]/cell_size)/2+1, num_of_feats, num_of_scales, this->stream);
        this->kzf.create(windows_size[1]/cell_size, (windows_size[0]/cell_size)/2+1, num_of_scales, this->stream);
        this->kf.create(windows_size[1]/cell_size, (windows_size[0]/cell_size)/2+1, num_of_scales, this->stream);

        alloc_size = uint(num_of_scales);

        CudaSafeCall(cudaHostAlloc(reinterpret_cast<void**>(&this->xf_sqr_norm), alloc_size*sizeof(float), cudaHostAllocMapped));
        CudaSafeCall(cudaHostGetDevicePointer(reinterpret_cast<void**>(&this->xf_sqr_norm_d), reinterpret_cast<void*>(this->xf_sqr_norm), 0));

        CudaSafeCall(cudaHostAlloc(reinterpret_cast<void**>(&this->yf_sqr_norm), sizeof(float), cudaHostAllocMapped));
        CudaSafeCall(cudaHostGetDevicePointer(reinterpret_cast<void**>(&this->yf_sqr_norm_d), reinterpret_cast<void*>(this->yf_sqr_norm), 0));

        alloc_size =uint((windows_size[0]/cell_size)*(windows_size[1]/cell_size))*alloc_size*sizeof(float);
        CudaSafeCall(cudaHostAlloc(reinterpret_cast<void**>(&this->gauss_corr_res), alloc_size, cudaHostAllocMapped));
        CudaSafeCall(cudaHostGetDevicePointer(reinterpret_cast<void**>(&this->gauss_corr_res_d), reinterpret_cast<void*>(this->gauss_corr_res), 0));
        this->in_all = cv::Mat(windows_size[1]/cell_size*num_of_scales, windows_size[0]/cell_size, CV_32F, this->gauss_corr_res);

        if (zero_index) {
            alloc_size = uint((windows_size[0]/cell_size)*(windows_size[1]/cell_size))*sizeof(float);
            CudaSafeCall(cudaHostAlloc(reinterpret_cast<void**>(&this->rot_labels_data), alloc_size, cudaHostAllocMapped));
            CudaSafeCall(cudaHostGetDevicePointer(reinterpret_cast<void**>(&this->rot_labels_data_d), reinterpret_cast<void*>(this->rot_labels_data), 0));
            this->rot_labels = cv::Mat(windows_size[1]/cell_size, windows_size[0]/cell_size, CV_32FC1, this->rot_labels_data);
        }

        alloc_size = uint((windows_size[0]/cell_size)*((windows_size[1]/cell_size)*num_of_feats))*sizeof(cufftReal);
        CudaSafeCall(cudaHostAlloc(reinterpret_cast<void**>(&this->data_features), alloc_size, cudaHostAllocMapped));
        CudaSafeCall(cudaHostGetDevicePointer(reinterpret_cast<void**>(&this->data_features_d), reinterpret_cast<void*>(this->data_features), 0));
        this->fw_all = cv::Mat((windows_size[1]/cell_size)*num_of_feats, windows_size[0]/cell_size, CV_32F, this->data_features);
#else
        alloc_size = uint(num_of_scales);

        this->xf_sqr_norm = reinterpret_cast<float*>(malloc(alloc_size*sizeof(float)));
        this->yf_sqr_norm = reinterpret_cast<float*>(malloc(sizeof(float)));

        this->patch_feats.reserve(uint(num_of_feats));

        int height = windows_size[1]/cell_size;
#ifdef FFTW
        int width = (windows_size[0]/cell_size)/2+1;
#else
        int width = windows_size[0]/cell_size;
#endif

        this->ifft2_res = cv::Mat(height, windows_size[0]/cell_size, CV_32FC(num_of_feats));
        this->response = cv::Mat(height, windows_size[0]/cell_size, CV_32FC(num_of_scales));

        this->zf = ComplexMat(height, width, num_of_feats, num_of_scales);
        this->kzf = ComplexMat(height, width, num_of_scales);
        this->kf = ComplexMat(height, width, num_of_scales);
#ifdef FFTW
        this->in_all = cv::Mat((windows_size[1]/cell_size)*num_of_scales, windows_size[0]/cell_size, CV_32F);
        this->fw_all = cv::Mat((windows_size[1]/cell_size)*num_of_feats, windows_size[0]/cell_size, CV_32F);
#else
        this->in_all = cv::Mat((windows_size[1]/cell_size), windows_size[0]/cell_size, CV_32F);
#endif
#endif
#if defined(FFTW) || defined(CUFFT)
        if (zero_index) {
            model_xf->create(windows_size[1]/cell_size, (windows_size[0]/cell_size)/2+1, num_of_feats);
            yf->create(windows_size[1]/cell_size, (windows_size[0]/cell_size)/2+1, 1);
            //We use scale_vars[0] for updating the tracker, so we only allocate memory for  its xf only.
#ifdef CUFFT
            this->xf.create(windows_size[1]/cell_size, (windows_size[0]/cell_size)/2+1, num_of_feats, this->stream);
#else
            this->xf.create(windows_size[1]/cell_size, (windows_size[0]/cell_size)/2+1, num_of_feats);
#endif
        } else if (num_of_scales > 1) {
            this->max_responses.reserve(uint(num_of_scales));
            this->max_locs.reserve(uint(num_of_scales));
            this->response_maps.reserve(uint(num_of_scales));
        }
#else
        if (zero_index) {
            model_xf->create(windows_size[1]/cell_size, windows_size[0]/cell_size, num_of_feats);
            yf->create(windows_size[1]/cell_size, windows_size[0]/cell_size, 1);
            this->xf.create(windows_size[1]/cell_size, windows_size[0]/cell_size, num_of_feats);
        }
#endif
    }

    ~Scale_vars() {
#ifdef CUFFT
        CudaSafeCall(cudaFreeHost(this->xf_sqr_norm));
        CudaSafeCall(cudaFreeHost(this->yf_sqr_norm));
        CudaSafeCall(cudaFreeHost(this->data_i_1ch));
        CudaSafeCall(cudaFreeHost(this->data_i_features));
        CudaSafeCall(cudaFreeHost(this->gauss_corr_res));
        if (zero_index)
            CudaSafeCall(cudaFreeHost(this->rot_labels_data));
        CudaSafeCall(cudaFreeHost(this->data_features));
#if defined(ASYNC) || defined(OPENMP)
        CudaSafeCall(cudaStreamDestroy(this->stream));
#endif
#else
        free(this->xf_sqr_norm);
        free(this->yf_sqr_norm);
#endif
    }

    float *xf_sqr_norm = nullptr, *yf_sqr_norm = nullptr;
    std::vector<cv::Mat> patch_feats;

    cv::Mat in_all, fw_all, ifft2_res, response;
    ComplexMat zf, kzf, kf, xyf, xf;

    //CuFFT variables
    cv::Mat rot_labels;
    float *xf_sqr_norm_d = nullptr, *yf_sqr_norm_d = nullptr, *gauss_corr_res = nullptr, *gauss_corr_res_d = nullptr, *rot_labels_data = nullptr,
              *rot_labels_data_d = nullptr, *data_features = nullptr, *data_features_d = nullptr;
    float *data_f = nullptr, *data_i_features = nullptr, *data_i_features_d = nullptr, *data_i_1ch = nullptr, *data_i_1ch_d = nullptr;

    cudaStream_t stream = nullptr;
    ComplexMat model_alphaf, model_xf;

    //Big batch variables
    cv::Point2i max_loc;
    double max_val, max_response;

    std::vector<double> max_responses;
    std::vector<cv::Point2i> max_locs;
    std::vector<cv::Mat> response_maps;
    bool zero_index = false;
};

#endif // SCALE_VARS_HPP
