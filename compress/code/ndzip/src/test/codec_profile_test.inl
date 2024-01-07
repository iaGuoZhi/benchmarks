#include "test_utils.hh"

#include <ndzip/cpu_codec.inl>

#if NDZIP_HIPSYCL_SUPPORT
#include <ndzip/sycl_codec.inl>
#endif

#if NDZIP_CUDA_SUPPORT
#include <ndzip/cuda_codec.inl>
#endif

#include <iostream>


#define ALL_PROFILES (profile<DATA_TYPE, DIMENSIONS>)


using namespace ndzip;
using namespace ndzip::detail;


TEMPLATE_TEST_CASE("block transform is reversible", "[profile]", ALL_PROFILES) {
    using bits_type = typename TestType::bits_type;

    const auto input = make_random_vector<bits_type>(ipow(TestType::hypercube_side_length, TestType::dimensions));

    auto transformed = input;
    detail::block_transform(transformed.data(), TestType::dimensions, TestType::hypercube_side_length);

    detail::inverse_block_transform(transformed.data(), TestType::dimensions, TestType::hypercube_side_length);

    CHECK(input == transformed);
}


TEMPLATE_TEST_CASE("decode(encode(input)) reproduces the input", "[encoder][de]", ALL_PROFILES) {
    using profile = TestType;
    using value_type = typename profile::value_type;
    using bits_type = typename profile::bits_type;

    constexpr auto dims = profile::dimensions;
    constexpr auto side_length = profile::hypercube_side_length;
    const index_type n = side_length * 4 - 1;
    const auto size = extent::broadcast(dims, n);

    auto input_data = make_random_vector<value_type>(ipow(n, dims));

    // Regression test: trigger bug in decoder optimization by ensuring first chunk = 0
    std::fill(input_data.begin(), input_data.begin() + bits_of<value_type>, value_type{});

    auto test_encoder_decoder_pair = [&](auto &&encoder, auto &&decoder) {
        std::vector<bits_type> stream(ndzip::compressed_length_bound<value_type>(size));
        stream.resize(encoder.compress(input_data.data(), size, stream.data()));

        std::vector<value_type> output_data(input_data.size());
        auto stream_words_read = decoder.decompress(stream.data(), stream.size(), output_data.data(), size);

        CHECK(stream_words_read == stream.size());
        CHECK_FOR_VECTOR_EQUALITY(input_data, output_data);
    };

    SECTION("serial CPU compress => serial CPU decompress") {
        test_encoder_decoder_pair(*make_cpu_offloader<value_type>(dims, 1), *make_cpu_offloader<value_type>(dims, 1));
    }

#if NDZIP_OPENMP_SUPPORT
    SECTION("serial CPU compress => OpenMP CPU decompress", "[omp]") {
        test_encoder_decoder_pair(*make_cpu_offloader<value_type>(dims, 1), *make_cpu_offloader<value_type>(dims));
    }

    SECTION("OpenMP CPU compress => serial CPU decompress", "[omp]") {
        test_encoder_decoder_pair(*make_cpu_offloader<value_type>(dims), *make_cpu_offloader<value_type>(dims, 1));
    }
#endif

#if NDZIP_HIPSYCL_SUPPORT
    SECTION("serial CPU compress => SYCL decompress", "[sycl]") {
        test_encoder_decoder_pair(*make_cpu_offloader<value_type>(dims, 1), *make_sycl_offloader<value_type>(dims));
    }

    SECTION("SYCL compress => serial CPU decompress", "[sycl]") {
        test_encoder_decoder_pair(*make_sycl_offloader<value_type>(dims), *make_cpu_offloader<value_type>(dims, 1));
    }
#endif

#if NDZIP_CUDA_SUPPORT
    SECTION("serial CPU compress => CUDA decompress", "[cuda]") {
        test_encoder_decoder_pair(*make_cpu_offloader<value_type>(dims, 1), *make_cuda_offloader<value_type>(dims));
    }

    SECTION("CUDA compress => serial CPU decompress", "[cuda]") {
        test_encoder_decoder_pair(*make_cuda_offloader<value_type>(dims), *make_cpu_offloader<value_type>(dims, 1));
    }
#endif
}


#if NDZIP_OPENMP_SUPPORT || NDZIP_HIPSYCL_SUPPORT || NDZIP_CUDA_SUPPORT
TEMPLATE_TEST_CASE("file headers from different encoders are identical", "[header]", ALL_PROFILES) {
    using value_type = typename TestType::value_type;
    using bits_type = typename TestType::bits_type;
    using profile = detail::profile<value_type, TestType::dimensions>;

    constexpr auto dims = profile::dimensions;
    constexpr auto side_length = profile::hypercube_side_length;
    constexpr index_type n = side_length * 4 - 1;

    std::unique_ptr<offloader<value_type>> test_offloader;
#if NDZIP_OPENMP_SUPPORT
    SECTION("OpenMP vs CPU") { test_offloader = make_cpu_offloader<value_type>(dims); }
#endif
#if NDZIP_HIPSYCL_SUPPORT
    SECTION("SYCL vs CPU") { test_offloader = make_sycl_offloader<value_type>(dims); }
#endif
#if NDZIP_CUDA_SUPPORT
    SECTION("CUDA vs CPU") { test_offloader = make_cuda_offloader<value_type>(dims); }
#endif

    const auto input_data = make_random_vector<value_type>(ipow(n, dims));
    const auto size = extent::broadcast(dims, n);

    const auto num_hypercubes = detail::num_hypercubes(size);
    const auto aligned_stream_size_bound
            = compressed_length_bound<value_type>(size) * sizeof(bits_type) / sizeof(index_type) + 1;

    const auto reference_offloader = make_cpu_offloader<value_type>(dims, 1);
    std::vector<index_type> reference_stream(aligned_stream_size_bound);
    const auto reference_stream_length = reference_offloader->compress(
            input_data.data(), size, reinterpret_cast<bits_type *>(reference_stream.data()));
    reference_stream.resize(num_hypercubes);

    std::vector<index_type> test_stream(aligned_stream_size_bound);
    const auto test_stream_length
            = test_offloader->compress(input_data.data(), size, reinterpret_cast<bits_type *>(test_stream.data()));
    test_stream.resize(num_hypercubes);

    CHECK_FOR_VECTOR_EQUALITY(reference_stream, test_stream);
    CHECK(reference_stream_length == test_stream_length);
}
#endif

#if NDZIP_HIPSYCL_SUPPORT

using sam = sycl::access::mode;
using sycl::accessor, sycl::nd_range, sycl::buffer, sycl::nd_item, sycl::range, sycl::id, sycl::handler, sycl::group,
        sycl::sub_group;

template<typename Profile>
static std::vector<typename Profile::bits_type> sycl_load_and_dump_hypercube(const typename Profile::value_type *in,
        const static_extent<Profile::dimensions> &in_size, index_type hc_index, sycl::queue &q) {
    using value_type = typename Profile::value_type;
    using bits_type = typename Profile::bits_type;

    constexpr auto hc_size = ipow(Profile::hypercube_side_length, Profile::dimensions);

    buffer<value_type> load_buf{in, range<1>{num_elements(in_size)}};
    std::vector<bits_type> out(hc_size * 2);
    buffer<value_type> store_buf{out.size()};

    q.submit([&](handler &cgh) { cgh.fill(store_buf.template get_access<sam::discard_write>(cgh), value_type{0}); });
    q.submit([&](handler &cgh) {
        auto data_acc = load_buf.template get_access<sam::read>(cgh);
        auto result_acc = store_buf.template get_access<sam::discard_write>(cgh);
        sycl::local_accessor<gpu_sycl::hypercube_allocation<Profile, gpu::forward_transform_tag>> lm{1, cgh};
        cgh.parallel_for(gpu_sycl::make_nd_range(1, gpu::hypercube_group_size<Profile>),
                [=](gpu_sycl::hypercube_item<Profile> item) {
                    const value_type *data = data_acc.get_pointer();
                    gpu_sycl::hypercube_ptr<Profile, gpu::forward_transform_tag> hc{lm[0]};
                    gpu_sycl::load_hypercube(item.get_group(), hc_index, data, in_size, hc);
                    // TODO rotate should probaly happen during CPU load_hypercube as well to hide
                    //  memory access latencies
                    distribute_for(hc_size, item.get_group(), [&](index_type item) {
                        result_acc[item] = bit_cast<value_type>(rotate_right_1(hc.load(item)));
                    });
                });
    });
    q.submit([&](handler &cgh) {
        cgh.copy(store_buf.template get_access<sam::read>(cgh), reinterpret_cast<value_type *>(out.data()));
    });
    q.wait();
    return out;
}


#if 0  // gpu::hypercube_ptr assumes 4096 elements per hc
template<typename T, dim_type Dims>
struct mock_profile {
    using value_type = T;
    using bits_type = T;
    constexpr static dim_type dimensions = Dims;
    constexpr static index_type hypercube_side_length = 2;
    constexpr static index_type compressed_block_size_bound
            = sizeof(T) * (ipow(hypercube_side_length, dimensions) + 1);
};


TEMPLATE_TEST_CASE(
        "correctly load small hypercubes into local memory", "[sycl]", uint32_t, uint64_t) {
    sycl::queue q{sycl::gpu_selector{}};

    SECTION("1d") {
        using profile = mock_profile<TestType, 1>;
        std::vector<TestType> data{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
        auto result = load_and_dump_hypercube<profile>(data.data(), data.size(), 1 /* hc_index */, q);
        CHECK(result == std::vector<TestType>{3, 4, 0, 0});
    }

    SECTION("2d") {
        using profile = mock_profile<TestType, 2>;
        // clang-format off
        std::vector<TestType> data{
            10, 20, 30, 40, 50, 60, 70, 80, 90,
            11, 21, 31, 41, 51, 61, 71, 81, 91,
            12, 22, 32, 42, 52, 62, 72, 82, 92,
            13, 23, 33, 43, 53, 63, 73, 83, 93,
            14, 24, 34, 44, 54, 64, 74, 84, 94,
            15, 25, 35, 45, 55, 65, 75, 85, 95,
            16, 26, 36, 46, 56, 66, 76, 86, 96,
            17, 27, 37, 47, 57, 67, 77, 87, 97,
        };
        // clang-format on
        auto result = load_and_dump_hypercube<profile>(data.data(), static_extent{8, 9}, 6 /* hc_index */, q);
        CHECK(result == std::vector<TestType>{52, 62, 53, 63, 0, 0, 0, 0});
    }

    SECTION("3d") {
        using profile = mock_profile<TestType, 3>;
        // clang-format off
        std::vector<TestType> data{
            111, 211, 311, 411, 511,
            121, 221, 321, 421, 521,
            131, 231, 331, 431, 531,
            141, 241, 341, 441, 541,
            151, 251, 351, 451, 551,

            112, 212, 312, 412, 512,
            122, 222, 322, 422, 522,
            132, 232, 332, 432, 532,
            142, 242, 342, 442, 542,
            152, 252, 352, 452, 552,

            113, 213, 313, 413, 513,
            123, 223, 323, 423, 523,
            133, 233, 333, 433, 533,
            143, 243, 343, 443, 543,
            153, 253, 353, 453, 553,

            114, 214, 314, 414, 514,
            124, 224, 324, 424, 524,
            134, 234, 334, 434, 534,
            144, 244, 344, 444, 544,
            154, 254, 354, 454, 554,

            115, 215, 315, 415, 515,
            125, 225, 325, 425, 525,
            135, 235, 335, 435, 535,
            145, 245, 345, 445, 545,
            155, 255, 355, 455, 555,
        };
        auto result = load_and_dump_hypercube<profile>(data.data(), static_extent{5, 5, 5}}, 3 /* hc_index */, q);
        CHECK(result == std::vector<TestType>{331, 431, 341, 441, 332, 432, 342, 442,
                      0, 0, 0, 0, 0, 0, 0, 0});
        // clang-format on
    }
}
#endif


TEMPLATE_TEST_CASE("SYCL store_hypercube is the inverse of load_hypercube", "[sycl][load]", ALL_PROFILES) {
    using value_type = typename TestType::value_type;
    using bits_type = typename TestType::bits_type;

    constexpr auto dims = TestType::dimensions;
    constexpr auto side_length = TestType::hypercube_side_length;
    const index_type hc_size = ipow(side_length, dims);
    const index_type n = side_length * 3;

    auto input_data = make_random_vector<value_type>(ipow(n, dims));
    const auto size = static_extent<dims>::broadcast(n);

    buffer<value_type> input_buf{input_data.data(), range<1>{num_elements(size)}};
    // buffer needed for hypercube_ptr forward_transform_tag => inverse_transform_tag translation
    buffer<bits_type> temp_buf{input_buf.get_range()};
    buffer<value_type> output_buf{input_buf.get_range()};
    const auto num_hypercubes = detail::num_hypercubes(size);

    sycl::queue q{sycl::gpu_selector{}};
    q.submit([&](handler &cgh) { cgh.fill(output_buf.template get_access<sam::discard_write>(cgh), value_type{0}); });
    q.submit([&](handler &cgh) {
        auto input_acc = input_buf.template get_access<sam::read>(cgh);
        auto temp_acc = temp_buf.template get_access<sam::discard_write>(cgh);
        sycl::local_accessor<gpu_sycl::hypercube_allocation<TestType, gpu::forward_transform_tag>> lm{1, cgh};
        cgh.parallel_for(gpu_sycl::make_nd_range(num_hypercubes, gpu::hypercube_group_size<TestType>),
                [=](gpu_sycl::hypercube_item<TestType> item) {
                    auto hc_index = item.get_group_id(0);
                    const value_type *input = input_acc.get_pointer();
                    gpu::hypercube_ptr<TestType, gpu::forward_transform_tag> hc{lm[0]};
                    gpu_sycl::load_hypercube(item.get_group(), hc_index, input, size, hc);
                    distribute_for(hc_size, item.get_group(),
                            [&](index_type i) { temp_acc[hc_index * hc_size + i] = hc.load(i); });
                });
    });
    q.submit([&](handler &cgh) {
        auto temp_acc = temp_buf.template get_access<sam::read>(cgh);
        auto output_acc = output_buf.template get_access<sam::discard_write>(cgh);
        sycl::local_accessor<gpu_sycl::hypercube_allocation<TestType, gpu::inverse_transform_tag>> lm{1, cgh};
        cgh.parallel_for(gpu_sycl::make_nd_range(num_hypercubes, gpu::hypercube_group_size<TestType>),
                [=](gpu_sycl::hypercube_item<TestType> item) {
                    auto hc_index = item.get_group_id(0);
                    value_type *output = output_acc.get_pointer();
                    gpu::hypercube_ptr<TestType, gpu::inverse_transform_tag> hc{lm[0]};
                    distribute_for(hc_size, item.get_group(),
                            [&](index_type i) { hc.store(i, temp_acc[hc_index * hc_size + i]); });
                    gpu_sycl::store_hypercube(item.get_group(), hc_index, output, size, hc);
                });
    });
    std::vector<value_type> output_data(input_data.size());
    q.submit([&](handler &cgh) { cgh.copy(output_buf.template get_access<sam::read>(cgh), output_data.data()); });
    q.wait();

    CHECK_FOR_VECTOR_EQUALITY(input_data, output_data);
}

template<typename>
class gpu_hypercube_transpose_test_kernel;
template<typename>
class gpu_hypercube_compact_test_kernel;

template<typename, typename>
class sycl_transform_test_kernel;

#endif


#if NDZIP_CUDA_SUPPORT

template<typename T>
static __global__ void cuda_fill_kernel(T *dest, T value, index_type count) {
    const auto i = static_cast<index_type>(blockIdx.x * blockDim.x + threadIdx.x);
    if (i < count) {
        dest[i] = value;
    }
}

template<typename T>
static void cuda_fill(T *dest, T value, index_type count) {
    constexpr index_type threads_per_block = 256;
    cuda_fill_kernel<<<div_ceil(count, threads_per_block), threads_per_block>>>(dest, value, count);
}

template<typename Profile>
static __global__ void cuda_load_and_dump_kernel(const typename Profile::value_type *data,
        static_extent<Profile::dimensions> data_size, index_type hc_index, typename Profile::value_type *result) {
    using value_type = typename Profile::value_type;
    constexpr auto hc_size = ipow(Profile::hypercube_side_length, Profile::dimensions);

    __shared__ gpu_cuda::hypercube_allocation<Profile, gpu::forward_transform_tag> lm;
    gpu_cuda::hypercube_ptr<Profile, gpu::forward_transform_tag> hc{lm};

    auto block = gpu_cuda::hypercube_block<Profile>{};
    gpu_cuda::load_hypercube(block, hc_index, data, data_size, hc);
    __syncthreads();
    // TODO rotate should probaly happen during CPU load_hypercube as well to hide
    //  memory access latencies
    distribute_for(hc_size, block,
            [&](index_type item) { result[item] = bit_cast<value_type>(rotate_right_1(hc.load(item))); });
}

template<typename Profile>
static std::vector<typename Profile::bits_type> cuda_load_and_dump_hypercube(const typename Profile::value_type *in,
        const static_extent<Profile::dimensions> &in_size, index_type hc_index) {
    using value_type = typename Profile::value_type;
    using bits_type = typename Profile::bits_type;

    auto hc_size = ipow(Profile::hypercube_side_length, Profile::dimensions);
    gpu_cuda::cuda_buffer<value_type> load_buf{num_elements(in_size)};
    std::vector<bits_type> out(hc_size * 2);
    gpu_cuda::cuda_buffer<value_type> store_buf(out.size());
    const auto num_hypercubes = detail::num_hypercubes(in_size);

    CHECKED_CUDA_CALL(cudaMemcpy, load_buf.get(), in, load_buf.size() * sizeof(value_type), cudaMemcpyHostToDevice);
    cuda_fill(store_buf.get(), value_type{0}, store_buf.size());
    cuda_load_and_dump_kernel<Profile><<<num_hypercubes, (gpu::hypercube_group_size<Profile>)>>>(
            load_buf.get(), in_size, hc_index, store_buf.get());
    CHECKED_CUDA_CALL(cudaMemcpy, out.data(), store_buf.get(), out.size() * sizeof(value_type), cudaMemcpyDeviceToHost);
    return out;
}

template<typename Profile>
static __global__ void cuda_load_hypercube_kernel(const typename Profile::value_type *input,
        static_extent<Profile::dimensions> input_size, typename Profile::bits_type *temp) {
    constexpr index_type hc_size = ipow(Profile::hypercube_side_length, Profile::dimensions);
    auto hc_index = static_cast<index_type>(blockIdx.x);

    __shared__ gpu_cuda::hypercube_allocation<Profile, gpu::forward_transform_tag> lm;
    gpu::hypercube_ptr<Profile, gpu::forward_transform_tag> hc{lm};

    auto block = gpu_cuda::hypercube_block<Profile>{};
    gpu_cuda::load_hypercube(block, hc_index, input, input_size, hc);
    __syncthreads();
    distribute_for(hc_size, block, [&](index_type i) { temp[hc_index * hc_size + i] = hc.load(i); });
}

template<typename Profile>
static __global__ void cuda_store_hypercube_kernel(const typename Profile::bits_type *temp,
        typename Profile::value_type *output, static_extent<Profile::dimensions> output_size) {
    constexpr index_type hc_size = ipow(Profile::hypercube_side_length, Profile::dimensions);
    auto hc_index = static_cast<index_type>(blockIdx.x);

    __shared__ gpu_cuda::hypercube_allocation<Profile, gpu::inverse_transform_tag> lm;
    gpu::hypercube_ptr<Profile, gpu::inverse_transform_tag> hc{lm};

    auto block = gpu_cuda::hypercube_block<Profile>{};
    distribute_for(hc_size, block, [&](index_type i) { hc.store(i, temp[hc_index * hc_size + i]); });
    __syncthreads();
    gpu_cuda::store_hypercube(block, hc_index, output, output_size, hc);
}

TEMPLATE_TEST_CASE("CUDA store_hypercube is the inverse of load_hypercube", "[cuda][load]", ALL_PROFILES) {
    using value_type = typename TestType::value_type;
    using bits_type = typename TestType::bits_type;

    constexpr auto dims = TestType::dimensions;
    constexpr auto side_length = TestType::hypercube_side_length;
    const index_type n = side_length * 3;

    auto input_data = make_random_vector<value_type>(ipow(n, dims));
    const auto input = input_data.data();
    const auto size = static_extent<dims>::broadcast(n);

    gpu_cuda::cuda_buffer<value_type> input_buf{num_elements(size)};
    // buffer needed for hypercube_ptr forward_transform_tag => inverse_transform_tag translation
    gpu_cuda::cuda_buffer<bits_type> temp_buf(input_buf.size());
    gpu_cuda::cuda_buffer<value_type> output_buf(input_buf.size());
    const auto num_hypercubes = detail::num_hypercubes(size);

    CHECKED_CUDA_CALL(
            cudaMemcpy, input_buf.get(), input, input_buf.size() * sizeof(value_type), cudaMemcpyHostToDevice);

    cuda_fill(output_buf.get(), value_type{0}, output_buf.size());

    cuda_load_hypercube_kernel<TestType>
            <<<num_hypercubes, (gpu_cuda::hypercube_group_size<TestType>)>>>(input_buf.get(), size, temp_buf.get());
    cuda_store_hypercube_kernel<TestType>
            <<<num_hypercubes, (gpu_cuda::hypercube_group_size<TestType>)>>>(temp_buf.get(), output_buf.get(), size);

    std::vector<value_type> output_data(input_data.size());
    CHECKED_CUDA_CALL(cudaMemcpy, output_data.data(), output_buf.get(), output_buf.size() * sizeof(value_type),
            cudaMemcpyDeviceToHost);

    CHECK_FOR_VECTOR_EQUALITY(input_data, output_data);
}

template<typename Profile>
__global__ void encode_hypercube_kernel(
        const typename Profile::bits_type *input, typename Profile::bits_type *chunks, index_type *chunk_lengths) {
    constexpr index_type hc_size = ipow(Profile::hypercube_side_length, Profile::dimensions);

    __shared__ gpu_cuda::hypercube_allocation<Profile, gpu::forward_transform_tag> lm;
    gpu_cuda::hypercube_ptr<Profile, gpu::forward_transform_tag> hc{lm};

    auto block = gpu_cuda::hypercube_block<Profile>{};
    distribute_for(hc_size, block, [&](index_type i) { hc.store(i, input[i]); });
    __syncthreads();
    write_transposed_chunks(block, hc, chunks, chunk_lengths + 1);
    // hack
    if (blockIdx.x == 0 && threadIdx.x == 0) {
        chunk_lengths[0] = 0;
    }
}

template<typename Profile>
__global__ void test_compact_chunks(const typename Profile::bits_type *chunks, const index_type *offsets,
        index_type *length, typename Profile::bits_type *stream) {
    auto block = gpu_cuda::hypercube_block<Profile>{};
    gpu_cuda::compact_chunks<Profile>(block, chunks, offsets, length, stream);
}

template<typename Profile>
__global__ void
hypercube_decode_test_kernel(const typename Profile::bits_type *stream, typename Profile::bits_type *output) {
    constexpr index_type hc_size = ipow(Profile::hypercube_side_length, Profile::dimensions);

    __shared__ gpu_cuda::hypercube_allocation<Profile, gpu::inverse_transform_tag> lm;
    gpu::hypercube_ptr<Profile, gpu::inverse_transform_tag> hc{lm};

    auto block = gpu_cuda::hypercube_block<Profile>{};
    gpu_cuda::read_transposed_chunks<Profile>(block, hc, stream);
    __syncthreads();
    distribute_for(hc_size, block, [&](index_type i) { output[i] = hc.load(i); });
}

template<typename Profile, typename Tag, typename CudaTransform>
__global__ void test_gpu_transform(typename Profile::bits_type *buffer, CudaTransform cuda_transform) {
    constexpr index_type hc_size = ipow(Profile::hypercube_side_length, Profile::dimensions);

    __shared__ gpu_cuda::hypercube_allocation<Profile, Tag> lm;
    gpu::hypercube_ptr<Profile, Tag> hc{lm};

    auto block = gpu_cuda::hypercube_block<Profile>{};
    distribute_for(hc_size, block, [&](index_type i) { hc.store(i, buffer[i]); });
    __syncthreads();
    cuda_transform(block, hc);
    __syncthreads();
    distribute_for(hc_size, block, [&](index_type i) { buffer[i] = hc.load(i); });
}

#endif


#if NDZIP_HIPSYCL_SUPPORT || NDZIP_CUDA_SUPPORT

TEMPLATE_TEST_CASE("Flattening of hypercubes is identical between encoders", "[load]", ALL_PROFILES) {
    using value_type = typename TestType::value_type;
    using profile = detail::profile<value_type, TestType::dimensions>;
    using bits_type = typename profile::bits_type;

    constexpr auto dims = profile::dimensions;
    constexpr auto side_length = profile::hypercube_side_length;
    const index_type hc_size = ipow(side_length, dims);
    const index_type n = side_length * 4 - 1;

    const auto input_data = make_random_vector<value_type>(ipow(n, dims));
    const auto input = input_data.data();
    const auto size = static_extent<dims>::broadcast(n);

    static_extent<dims> hc_offset;
    hc_offset[dims - 1] = side_length;
    index_type hc_index = 1;

    cpu::simd_aligned_buffer<bits_type> cpu_dump(hc_size);
    cpu::load_hypercube<profile>(hc_offset, input, size, cpu_dump.data());

#if NDZIP_HIPSYCL_SUPPORT
    SECTION("SYCL vs CPU", "[sycl]") {
        sycl::queue sycl_q{sycl::gpu_selector{}};
        auto sycl_dump = sycl_load_and_dump_hypercube<profile>(input, size, hc_index, sycl_q);
        CHECK_FOR_VECTOR_EQUALITY(sycl_dump.data(), cpu_dump.data(), hc_size);
    }
#endif

#if NDZIP_CUDA_SUPPORT
    SECTION("CUDA vs CPU", "[cuda]") {
        auto cuda_dump = cuda_load_and_dump_hypercube<profile>(input, size, hc_index);
        CHECK_FOR_VECTOR_EQUALITY(cuda_dump.data(), cpu_dump.data(), hc_size);
    }
#endif
}


TEMPLATE_TEST_CASE("Residual encodings from different encoders are equivalent", "[residual]", ALL_PROFILES) {
    using bits_type = typename TestType::bits_type;
    const auto hc_size = ipow(TestType::hypercube_side_length, TestType::dimensions);

    constexpr index_type col_chunk_size = detail::bits_of<bits_type>;
    constexpr index_type header_chunk_size = hc_size / col_chunk_size;
    constexpr index_type hc_total_chunks_size = hc_size + header_chunk_size;
    constexpr index_type chunks_per_hc = 1 /* header */ + hc_size / col_chunk_size;

    auto input = make_random_vector<bits_type>(hc_size);
    for (index_type i = 0; i < hc_size; ++i) {
        for (index_type idx : {0u, 12u, 13u, 29u, bits_of<bits_type> - 2}) {
            input[i] &= ~(bits_type{1} << ((idx * (i / bits_of<bits_type>) ) % bits_of<bits_type>) );
            input[floor(i, bits_of<bits_type>) + idx] = 0;
        }
    }

    cpu::simd_aligned_buffer<bits_type> cpu_cube(input.size());
    memcpy(cpu_cube.data(), input.data(), input.size() * sizeof(bits_type));
    std::vector<bits_type> cpu_stream(hc_size * 2);
    const auto cpu_length_bytes
            = cpu::zero_bit_encode(cpu_cube.data(), reinterpret_cast<std::byte *>(cpu_stream.data()), hc_size);

    const auto num_chunks = 1 + hc_size / col_chunk_size;
    const auto chunk_lengths_buf_size = ceil(1 + num_chunks, gpu::hierarchical_inclusive_scan_granularity);

    // TODO OpenMP

#if NDZIP_HIPSYCL_SUPPORT
    std::vector<bits_type> sycl_chunks(hc_total_chunks_size);
    std::vector<index_type> sycl_chunk_lengths(chunk_lengths_buf_size);
    std::vector<index_type> sycl_chunk_offsets(chunk_lengths_buf_size);
    index_type sycl_num_words;
    std::vector<bits_type> sycl_stream(hc_size * 2);

    SECTION("SYCL vs CPU", "[sycl]") {
        sycl::queue q{sycl::gpu_selector{}};

        buffer<bits_type> input_buf{hc_size};
        q.submit([&](handler &cgh) { cgh.copy(input.data(), input_buf.template get_access<sam::discard_write>(cgh)); });

        buffer<bits_type> chunks_buf{hc_total_chunks_size};
        buffer<index_type> chunk_lengths_buf{range<1>{chunk_lengths_buf_size}};

        q.submit([&](handler &cgh) {
             cgh.fill(chunks_buf.template get_access<sam::discard_write>(cgh), bits_type{0});
         }).wait();
        q.submit([&](handler &cgh) {
             cgh.fill(chunk_lengths_buf.get_access<sam::discard_write>(cgh), index_type{0});
         }).wait();

        q.submit([&](handler &cgh) {
            auto input_acc = input_buf.template get_access<sam::read>(cgh);
            auto columns_acc = chunks_buf.template get_access<sam::write>(cgh);
            auto chunk_lengths_acc = chunk_lengths_buf.get_access<sam::write>(cgh);
            sycl::local_accessor<gpu_sycl::compressor_local_allocation<TestType>> lm{1, cgh};

            auto nd_range = gpu_sycl::make_nd_range(1, gpu::hypercube_group_size<TestType>);
            cgh.parallel_for<gpu_hypercube_transpose_test_kernel<TestType>>(
                    nd_range, [=](gpu_sycl::hypercube_item<TestType> item) {
                        gpu::hypercube_ptr<TestType, gpu::forward_transform_tag> hc{lm[0].hc};
                        distribute_for(hc_size, item.get_group(), [&](index_type i) { hc.store(i, input_acc[i]); });
                        gpu_sycl::write_transposed_chunks(
                                item, hc, &columns_acc[0], &chunk_lengths_acc[1], lm[0].writer);
                        // hack
                        if (item.get_global_linear_id() == 0) {
                            chunk_lengths_acc[0] = 0;
                        }
                    });
        });

        q.submit([&](handler &cgh) {
             cgh.copy(chunks_buf.template get_access<sam::read>(cgh), sycl_chunks.data());
         }).wait();
        q.submit([&](handler &cgh) {
             cgh.copy(chunk_lengths_buf.get_access<sam::read>(cgh), sycl_chunk_lengths.data());
         }).wait();

        {
            auto scratch = gpu_sycl::hierarchical_inclusive_scan_allocate<index_type>(chunk_lengths_buf_size);
            gpu_sycl::hierarchical_inclusive_scan(q, chunk_lengths_buf, scratch, sycl::plus<index_type>{});
        }

        q.submit([&](handler &cgh) {
             cgh.copy(chunk_lengths_buf.get_access<sam::read>(cgh), sycl_chunk_offsets.data());
         }).wait();

        buffer<bits_type> stream_buf(range<1>{hc_size * 2});
        q.submit(
                [&](handler &cgh) { cgh.fill(stream_buf.template get_access<sam::discard_write>(cgh), bits_type{0}); });

        buffer<index_type> length_buf{range<1>{1}};
        q.submit([&](sycl::handler &cgh) {
            auto chunks_acc = chunks_buf.template get_access<sam::read>(cgh);
            auto chunk_offsets_acc = chunk_lengths_buf.template get_access<sam::read>(cgh);
            auto stream_acc = stream_buf.template get_access<sam::discard_write>(cgh);
            auto length_acc = length_buf.template get_access<sam::discard_write>(cgh);
            sycl::local_accessor<gpu_sycl::compaction_local_allocation<TestType>> lm{1, cgh};

            auto nd_range = gpu_sycl::make_nd_range(1 /* num_hypercubes */, gpu::hypercube_group_size<TestType>);
            cgh.parallel_for<gpu_hypercube_compact_test_kernel<TestType>>(
                    nd_range, [=](gpu_sycl::hypercube_item<TestType> item) {
                        const auto hc_index = item.get_group_id(0);
                        gpu_sycl::compact_chunks(item.get_group(),
                                &chunks_acc.get_pointer()[hc_index * hc_total_chunks_size],
                                &chunk_offsets_acc.get_pointer()[hc_index * chunks_per_hc], &length_acc[0],
                                &stream_acc.get_pointer()[0], lm[0]);
                    });
        });

        q.submit([&](handler &cgh) {
             cgh.copy(length_buf.template get_access<sam::read>(cgh), &sycl_num_words);
         }).wait();
        const auto sycl_length_bytes = sycl_num_words * sizeof(bits_type);

        q.submit([&](handler &cgh) {
             cgh.copy(stream_buf.template get_access<sam::read>(cgh), sycl_stream.data());
         }).wait();

        CHECK(sycl_length_bytes == cpu_length_bytes);
        CHECK_FOR_VECTOR_EQUALITY(sycl_stream, cpu_stream);
    }
#endif

#if NDZIP_CUDA_SUPPORT
    std::vector<bits_type> cuda_chunks(hc_total_chunks_size);
    std::vector<index_type> cuda_chunk_lengths(chunk_lengths_buf_size);
    std::vector<index_type> cuda_chunk_offsets(chunk_lengths_buf_size);
    std::vector<bits_type> cuda_stream(hc_size * 2);
    index_type cuda_num_words;

    SECTION("CUDA vs CPU", "[cuda]") {
        (void) chunks_per_hc;  // unused when compiling only for CUDA

        gpu_cuda::cuda_buffer<bits_type> input_buf(hc_size);
        gpu_cuda::cuda_buffer<bits_type> chunks_buf(hc_total_chunks_size);
        gpu_cuda::cuda_buffer<index_type> chunk_lengths_buf(
                ceil(1 + num_chunks, gpu_cuda::hierarchical_inclusive_scan_granularity));

        CHECKED_CUDA_CALL(
                cudaMemcpy, input_buf.get(), input.data(), hc_size * sizeof(bits_type), cudaMemcpyHostToDevice);

        cuda_fill(chunks_buf.get(), bits_type{}, chunks_buf.size());
        cuda_fill(chunk_lengths_buf.get(), index_type{}, chunk_lengths_buf.size());

        encode_hypercube_kernel<TestType><<<1, (gpu::hypercube_group_size<TestType>)>>>(
                input_buf.get(), chunks_buf.get(), chunk_lengths_buf.get());

        CHECKED_CUDA_CALL(cudaMemcpy, cuda_chunks.data(), chunks_buf.get(), chunks_buf.size() * sizeof(bits_type),
                cudaMemcpyDeviceToHost);
        CHECKED_CUDA_CALL(cudaMemcpy, cuda_chunk_lengths.data(), chunk_lengths_buf.get(),
                chunk_lengths_buf.size() * sizeof(index_type), cudaMemcpyDeviceToHost);

        auto intermediate_bufs = gpu_cuda::hierarchical_inclusive_scan_allocate<index_type>(chunk_lengths_buf.size());
        gpu_cuda::hierarchical_inclusive_scan(
                chunk_lengths_buf.get(), intermediate_bufs, chunk_lengths_buf.size(), gpu_cuda::plus<index_type>{});

        CHECKED_CUDA_CALL(cudaMemcpy, cuda_chunk_offsets.data(), chunk_lengths_buf.get(),
                chunk_lengths_buf.size() * sizeof(index_type), cudaMemcpyDeviceToHost);

        gpu_cuda::cuda_buffer<index_type> stream_length_buf(1);
        gpu_cuda::cuda_buffer<bits_type> stream_buf(hc_size * 2);
        cuda_fill(stream_buf.get(), bits_type{}, stream_buf.size());

        test_compact_chunks<TestType><<<1, (gpu::hypercube_group_size<TestType>)>>>(
                chunks_buf.get(), chunk_lengths_buf.get(), stream_length_buf.get(), stream_buf.get());

        CHECKED_CUDA_CALL(cudaMemcpy, cuda_stream.data(), stream_buf.get(), stream_buf.size() * sizeof(bits_type),
                cudaMemcpyDeviceToHost);
        CHECKED_CUDA_CALL(
                cudaMemcpy, &cuda_num_words, stream_length_buf.get(), sizeof(index_type), cudaMemcpyDeviceToHost);
        auto cuda_length_bytes = cuda_num_words * sizeof(bits_type);

        CHECK(cuda_length_bytes == cpu_length_bytes);
        CHECK_FOR_VECTOR_EQUALITY(cuda_stream, cpu_stream);
    }
#endif
}


template<typename>
class gpu_hypercube_decode_test_kernel;

TEMPLATE_TEST_CASE("GPU hypercube decoding works", "[decode]", ALL_PROFILES) {
    using bits_type = typename TestType::bits_type;
    const auto hc_size = ipow(TestType::hypercube_side_length, TestType::dimensions);

    auto input = make_random_vector<bits_type>(hc_size);
    for (index_type i = 0; i < hc_size; ++i) {
        for (index_type idx : {0u, 12u, 13u, 29u, bits_of<bits_type> - 2}) {
            input[i] &= ~(bits_type{1} << ((idx * (i / bits_of<bits_type>) ) % bits_of<bits_type>) );
            input[floor(i, bits_of<bits_type>) + idx] = 0;
        }
    }

    cpu::simd_aligned_buffer<bits_type> cpu_cube(input.size());
    memcpy(cpu_cube.data(), input.data(), input.size() * sizeof(bits_type));
    std::vector<bits_type> stream(hc_size * 2);
    auto cpu_length_bytes
            = cpu::zero_bit_encode(cpu_cube.data(), reinterpret_cast<std::byte *>(stream.data()), hc_size);
    REQUIRE(cpu_length_bytes % sizeof(bits_type) == 0);

#if NDZIP_HIPSYCL_SUPPORT
    SECTION("Using SYCL", "[sycl]") {
        sycl::queue q{sycl::gpu_selector{}};

        buffer<bits_type> stream_buf{stream.data(), range<1>{cpu_length_bytes / sizeof(bits_type)}};

        buffer<bits_type> output_buf{range<1>{hc_size}};
        q.submit([&](handler &cgh) {
            auto stream_acc = stream_buf.template get_access<sam::read>(cgh);
            auto output_acc = output_buf.template get_access<sam::discard_write>(cgh);
            sycl::local_accessor<gpu_sycl::decompressor_local_allocation<TestType>> lm{1, cgh};

            auto nd_range = gpu_sycl::make_nd_range(1, gpu::hypercube_group_size<TestType>);
            cgh.parallel_for<gpu_hypercube_decode_test_kernel<TestType>>(
                    nd_range, [stream_acc, output_acc, lm](gpu_sycl::hypercube_item<TestType> item) {
                        gpu::hypercube_ptr<TestType, gpu::inverse_transform_tag> hc{lm[0].hc};
                        gpu_sycl::read_transposed_chunks(item, hc, stream_acc.get_pointer(), lm[0].reader);
                        distribute_for(hc_size, item.get_group(), [&](index_type i) { output_acc[i] = hc.load(i); });
                    });
        });

        std::vector<bits_type> output(hc_size);
        q.submit([&](handler &cgh) { cgh.copy(output_buf.template get_access<sam::read>(cgh), output.data()); });
        q.wait();

        CHECK_FOR_VECTOR_EQUALITY(output, input);
    }
#endif

#if NDZIP_CUDA_SUPPORT
    SECTION("Using CUDA", "[cuda]") {
        gpu_cuda::cuda_buffer<bits_type> stream_buf(stream.size());
        gpu_cuda::cuda_buffer<bits_type> output_buf(hc_size);

        CHECKED_CUDA_CALL(
                cudaMemcpy, stream_buf.get(), stream.data(), stream.size() * sizeof(bits_type), cudaMemcpyHostToDevice);

        hypercube_decode_test_kernel<TestType>
                <<<1, (gpu::hypercube_group_size<TestType>)>>>(stream_buf.get(), output_buf.get());

        std::vector<bits_type> output(hc_size);
        CHECKED_CUDA_CALL(
                cudaMemcpy, output.data(), output_buf.get(), hc_size * sizeof(bits_type), cudaMemcpyDeviceToHost);

        CHECK_FOR_VECTOR_EQUALITY(output, input);
    }
#endif
}


template<typename Profile, typename Tag, typename CpuTransform
#if NDZIP_HIPSYCL_SUPPORT
        ,
        typename SyclTransform
#endif
#if NDZIP_CUDA_SUPPORT
        ,
        typename CudaTransform
#endif
        >
static void test_cpu_gpu_transform_equality(const CpuTransform &cpu_transform
#if NDZIP_HIPSYCL_SUPPORT
        ,
        const SyclTransform &sycl_transform
#endif
#if NDZIP_CUDA_SUPPORT
        ,
        const CudaTransform &cuda_transform
#endif
) {
    using bits_type = typename Profile::bits_type;
    constexpr auto hc_size = static_cast<index_type>(ipow(Profile::hypercube_side_length, Profile::dimensions));

    const auto input = make_random_vector<bits_type>(hc_size);

    auto cpu_transformed = input;
    cpu_transform(cpu_transformed.data());

#if NDZIP_HIPSYCL_SUPPORT
    SECTION("with SYCL", "[sycl]") {
        sycl::queue sycl_q{sycl::gpu_selector{}};
        buffer<bits_type> sycl_io_buf{range<1>{hc_size}};

        sycl_q.submit([&](handler &cgh) {
            cgh.copy(input.data(), sycl_io_buf.template get_access<sam::discard_write>(cgh));
        });
        sycl_q.submit([&](handler &cgh) {
            auto global_acc = sycl_io_buf.template get_access<sam::read_write>(cgh);
            sycl::local_accessor<gpu_sycl::hypercube_allocation<Profile, Tag>> lm_hc{1, cgh};
            std::conditional_t<std::is_same_v<Tag, gpu::inverse_transform_tag>,
                    sycl::local_accessor<gpu_sycl::inverse_transform_local_allocation<Profile>>,
                    std::tuple<int, handler &>>  // dummy with the same initializer as -^
                    lm_transform{1, cgh};  // captured conditionally in kernel

            auto nd_range = gpu_sycl::make_nd_range(1, gpu::hypercube_group_size<Profile>);
            cgh.parallel_for<sycl_transform_test_kernel<Profile, Tag>>(
                    nd_range, [=](gpu_sycl::hypercube_item<Profile> item) {
                        gpu::hypercube_ptr<Profile, Tag> hc{lm_hc[0]};
                        distribute_for(hc_size, item.get_group(), [&](index_type i) { hc.store(i, global_acc[i]); });
                        if constexpr (std::is_same_v<Tag, gpu::inverse_transform_tag>) {
                            sycl_transform(item, hc, lm_transform[0]);
                        } else {
                            sycl_transform(item, hc);
                        }
                        distribute_for(hc_size, item.get_group(), [&](index_type i) { global_acc[i] = hc.load(i); });
                    });
        });

        std::vector<bits_type> sycl_transformed(hc_size);
        sycl_q.submit([&](handler &cgh) {
            cgh.copy(sycl_io_buf.template get_access<sam::read>(cgh), sycl_transformed.data());
        });
        sycl_q.wait();

        CHECK_FOR_VECTOR_EQUALITY(sycl_transformed, cpu_transformed);
    }
#endif

#if NDZIP_CUDA_SUPPORT
    SECTION("with CUDA", "[cuda]") {
        gpu_cuda::cuda_buffer<bits_type> cuda_io_buf(hc_size);
        CHECKED_CUDA_CALL(
                cudaMemcpy, cuda_io_buf.get(), input.data(), input.size() * sizeof(bits_type), cudaMemcpyHostToDevice);
        test_gpu_transform<Profile, Tag, CudaTransform>
                <<<1, (gpu::hypercube_group_size<Profile>)>>>(cuda_io_buf.get(), cuda_transform);

        std::vector<bits_type> cuda_transformed(hc_size);
        CHECKED_CUDA_CALL(cudaMemcpy, cuda_transformed.data(), cuda_io_buf.get(),
                cuda_transformed.size() * sizeof(bits_type), cudaMemcpyDeviceToHost);

        CHECK_FOR_VECTOR_EQUALITY(cuda_transformed, cpu_transformed);
    }
#endif
}

TEMPLATE_TEST_CASE("CPU and GPU forward block transforms are identical", "[transform]", ALL_PROFILES) {
    using bits_type = typename TestType::bits_type;
    test_cpu_gpu_transform_equality<TestType, gpu::forward_transform_tag>(
            [](bits_type *block) {
                detail::block_transform(block, TestType::dimensions, TestType::hypercube_side_length);
            }
#if NDZIP_HIPSYCL_SUPPORT
            ,
            // Use lambda instead of the function name, otherwise a host function pointer will
            // be passed into the device kernel
            [](gpu_sycl::hypercube_item<TestType> item, gpu::hypercube_ptr<TestType, gpu::forward_transform_tag> hc) {
                constexpr auto hc_size = ipow(TestType::hypercube_side_length, TestType::dimensions);
                distribute_for(
                        hc_size, item.get_group(), [&](index_type i) { hc.store(i, rotate_left_1(hc.load(i))); });
                gpu_sycl::forward_block_transform(item.get_group(), hc);
            }
#endif
#if NDZIP_CUDA_SUPPORT
            ,
            [] __device__(gpu_cuda::hypercube_block<TestType> block,
                    gpu::hypercube_ptr<TestType, gpu::forward_transform_tag> hc) {
                constexpr auto hc_size = ipow(TestType::hypercube_side_length, TestType::dimensions);
                distribute_for(hc_size, block, [&](index_type i) { hc.store(i, rotate_left_1(hc.load(i))); });
                __syncthreads();
                gpu_cuda::forward_block_transform(block, hc);
            }
#endif
    );
}

TEMPLATE_TEST_CASE("CPU and GPU inverse block transforms are identical", "[transform]", ALL_PROFILES) {
    using bits_type = typename TestType::bits_type;
    test_cpu_gpu_transform_equality<TestType, gpu::inverse_transform_tag>(
            [](bits_type *block) {
                detail::inverse_block_transform(block, TestType::dimensions, TestType::hypercube_side_length);
            }
#if NDZIP_HIPSYCL_SUPPORT
            ,
            // Use lambda instead of the function name, otherwise a host function pointer will
            // be passed into the device kernel
            [](gpu_sycl::hypercube_item<TestType> item, gpu::hypercube_ptr<TestType, gpu::inverse_transform_tag> hc,
                    gpu_sycl::inverse_transform_local_allocation<TestType> &lm) {
                constexpr auto hc_size = ipow(TestType::hypercube_side_length, TestType::dimensions);
                gpu_sycl::inverse_block_transform<TestType>(item, hc, lm);
                distribute_for(
                        hc_size, item.get_group(), [&](index_type i) { hc.store(i, rotate_right_1(hc.load(i))); });
            }
#endif
#if NDZIP_CUDA_SUPPORT
            ,
            [] __device__(gpu_cuda::hypercube_block<TestType> block,
                    gpu::hypercube_ptr<TestType, gpu::inverse_transform_tag> hc) {
                constexpr auto hc_size = ipow(TestType::hypercube_side_length, TestType::dimensions);
                gpu_cuda::inverse_block_transform<TestType>(block, hc);
                __syncthreads();
                distribute_for(hc_size, block, [&](index_type i) { hc.store(i, rotate_right_1(hc.load(i))); });
            });
#endif
}

#endif  // NDZIP_HIPSYCL_SUPPORT || NDZIP_CUDA_SUPPORT


TEMPLATE_TEST_CASE("Single block compresses identically on all encoders", "[omp][sycl][cuda][compress]", ALL_PROFILES) {
    using value_type = typename TestType::value_type;
    using bits_type = typename TestType::bits_type;
    constexpr auto dimensions = TestType::dimensions;

    const auto size = extent::broadcast(dimensions, TestType::hypercube_side_length);
    const auto input = make_random_vector<value_type>(num_elements(size));
    const auto output_length_bound = compressed_length_bound<value_type>(size);

    std::vector<bits_type> serial_output(output_length_bound);
    const auto serial_offloader = make_cpu_offloader<value_type>(dimensions, 1);
    const auto serial_output_length = serial_offloader->compress(input.data(), size, serial_output.data());
    serial_output.resize(serial_output_length);

#if NDZIP_OPENMP_SUPPORT
    SECTION("Multi-threaded vs single-threaded CPU", "[omp]") {
        std::vector<bits_type> openmp_output(output_length_bound);
        const auto openmp_offloader = make_cpu_offloader<value_type>(dimensions);
        const auto openmp_output_length = openmp_offloader->compress(input.data(), size, openmp_output.data());
        openmp_output.resize(openmp_output_length);
        CHECK_FOR_VECTOR_EQUALITY(openmp_output, serial_output);
    }
#endif

#if NDZIP_HIPSYCL_SUPPORT
    SECTION("SYCL vs CPU", "[sycl]") {
        std::vector<bits_type> sycl_output(output_length_bound);
        const auto sycl_offloader = make_sycl_offloader<value_type>(dimensions);
        const auto sycl_output_length = sycl_offloader->compress(input.data(), size, sycl_output.data());
        sycl_output.resize(sycl_output_length);
        CHECK_FOR_VECTOR_EQUALITY(sycl_output, serial_output);
    }
#endif

#if NDZIP_CUDA_SUPPORT
    SECTION("CUDA vs CPU", "[cuda]") {
        std::vector<bits_type> cuda_output(output_length_bound);
        const auto cuda_offloader = make_cuda_offloader<value_type>(dimensions);
        const auto cuda_output_length = cuda_offloader->compress(input.data(), size, cuda_output.data());
        cuda_output.resize(cuda_output_length);
        CHECK_FOR_VECTOR_EQUALITY(cuda_output, serial_output);
    }
#endif
}

TEMPLATE_TEST_CASE("Single block decompresses correctly on all encoders", "[decompress]", ALL_PROFILES) {
    using value_type = typename TestType::value_type;
    using bits_type = typename TestType::bits_type;
    constexpr auto dimensions = TestType::dimensions;

    const auto size = extent::broadcast(dimensions, TestType::hypercube_side_length);
    const auto input = make_random_vector<value_type>(num_elements(size));
    const auto max_output_words = compressed_length_bound<value_type>(size);

    std::vector<bits_type> compressed(max_output_words);
    const auto serial_offloader = make_cpu_offloader<value_type>(dimensions, 1);
    const auto compressed_length = serial_offloader->compress(input.data(), size, compressed.data());
    compressed.resize(compressed_length);

    SECTION("On single-threaded CPU") {
        std::vector<value_type> cpu_output(num_elements(size));
        serial_offloader->decompress(compressed.data(), compressed_length, cpu_output.data(), size);
        CHECK_FOR_VECTOR_EQUALITY(cpu_output, input);
    }

#if NDZIP_OPENMP_SUPPORT
    SECTION("On multi-threaded CPU", "[omp]") {
        std::vector<value_type> openmp_output(num_elements(size));
        const auto openmp_offloader = make_cpu_offloader<value_type>(dimensions);
        openmp_offloader->decompress(compressed.data(), compressed_length, openmp_output.data(), size);
        CHECK_FOR_VECTOR_EQUALITY(openmp_output, input);
    }
#endif

#if NDZIP_HIPSYCL_SUPPORT
    SECTION("On SYCL", "[sycl]") {
        std::vector<value_type> sycl_output(num_elements(size));
        const auto sycl_offloader = make_sycl_offloader<value_type>(dimensions);
        sycl_offloader->decompress(compressed.data(), compressed_length, sycl_output.data(), size);
        CHECK_FOR_VECTOR_EQUALITY(sycl_output, input);
    }
#endif

#if NDZIP_CUDA_SUPPORT
    SECTION("On CUDA", "[cuda]") {
        std::vector<value_type> cuda_output(num_elements(size));
        const auto cuda_offloader = make_cuda_offloader<value_type>(dimensions);
        cuda_offloader->decompress(compressed.data(), compressed_length, cuda_output.data(), size);
        CHECK_FOR_VECTOR_EQUALITY(cuda_output, input);
    }
#endif
}

TEMPLATE_TEST_CASE("Encoders compress and decompress files with 0 hypercubes", "[compress][decompress]", ALL_PROFILES) {
    using value_type = typename TestType::value_type;
    using bits_type = typename TestType::bits_type;
    constexpr static auto dimensions = TestType::dimensions;

    const auto size = extent::broadcast(dimensions, GENERATE(0, 1));
    CAPTURE(size);

    const size_t input_bound = 1;
    std::vector<value_type> input(input_bound, value_type{42});  // dummy input
    const auto compressed_bound = std::max(index_type{1}, compressed_length_bound<value_type>(size));

    using offloader_constructor = std::unique_ptr<offloader<value_type>> (*)();
    const auto [offloader_name, make_offloader] = GENERATE(values<std::pair<const char *, offloader_constructor>>({
        std::pair{"single-threaded CPU", [] { return make_cpu_offloader<value_type>(dimensions, 1); }},
#if NDZIP_OPENMP_SUPPORT
                std::pair{"multi-threaded CPU", [] { return make_cpu_offloader<value_type>(dimensions); }},
#endif
#if NDZIP_HIPSYCL_SUPPORT
                std::pair{"SYCL", [] { return make_sycl_offloader<value_type>(dimensions); }},
#endif
#if NDZIP_CUDA_SUPPORT
                std::pair{"CUDA", [] { return make_cuda_offloader<value_type>(dimensions); }},
#endif
    }));

    CAPTURE(offloader_name);
    const auto offloader = make_offloader();

    std::vector<bits_type> compressed(compressed_bound);
    const auto compressed_length = offloader->compress(input.data(), size, compressed.data());
    std::vector<value_type> output(1);
    offloader->decompress(compressed.data(), compressed_length, output.data(), size);

    if (num_elements(size) > 0) {
        CHECK_FOR_VECTOR_EQUALITY(input, output);
    }
}
