#include "ubench.hh"

#include <ndzip/sycl_codec.inl>
#include <test/test_utils.hh>

using namespace ndzip;
using namespace ndzip::detail;
using namespace ndzip::detail::gpu_sycl;
using sam = sycl::access::mode;


#define ALL_PROFILES (profile<DATA_TYPE, DIMENSIONS>)


// Kernel names (for profiler)
template<typename>
class load_hypercube_kernel;
template<typename>
class block_transform_reference_kernel;
template<typename>
class block_forward_transform_kernel;
template<typename>
class block_inverse_transform_kernel;
template<typename>
class encode_reference_kernel;
template<typename>
class chunk_transpose_write_kernel;
template<typename>
class chunk_transpose_read_kernel;
template<typename>
class chunk_compact_kernel;


TEMPLATE_TEST_CASE("Loading", "[load]", ALL_PROFILES) {
    using value_type = typename TestType::value_type;
    using bits_type = typename TestType::bits_type;

    constexpr dim_type dimensions = TestType::dimensions;
    constexpr index_type n_blocks = 16384;

    const auto grid_extent = [] {
        static_extent<TestType::dimensions> grid_extent;
        const auto n_blocks_regular = static_cast<index_type>(pow(n_blocks, 1.f / dimensions));
        auto n_blocks_to_distribute = n_blocks;
        for (dim_type d = 0; d < dimensions; ++d) {
            auto n_blocks_this_dim = std::min(n_blocks_regular, n_blocks_to_distribute);
            grid_extent[d] = n_blocks_this_dim * TestType::hypercube_side_length + 3 /* border */;
            n_blocks_to_distribute /= n_blocks_this_dim;
        }
        assert(n_blocks_to_distribute == 0);
        return grid_extent;
    }();

    const auto data = make_random_vector<value_type>(num_elements(grid_extent));
    sycl::buffer<value_type> data_buffer(data.data(), data.size());

    SYCL_BENCHMARK("Load hypercube")(sycl::queue & q) {
        constexpr auto hc_size = ipow(TestType::hypercube_side_length, TestType::dimensions);

        sycl::buffer<bits_type> out(n_blocks * hc_size);
        return q.submit([&](sycl::handler &cgh) {
            auto data_acc = data_buffer.template get_access<sam::read>(cgh);
            sycl::local_accessor<hypercube_allocation<TestType, gpu::forward_transform_tag>> lm{1, cgh};
            cgh.parallel_for<load_hypercube_kernel<TestType>>(
                    make_nd_range(n_blocks, hypercube_group_size<TestType>), [=](hypercube_item<TestType> item) {
                        hypercube_ptr<TestType, gpu::forward_transform_tag> hc{lm[0]};
                        index_type hc_index = item.get_group_id(0);
                        load_hypercube(item.get_group(), hc_index, data_acc.get_pointer(), grid_extent, hc);
                        black_hole(hc.memory());
                    });
        });
    };
}


TEMPLATE_TEST_CASE("Block transform", "[transform]", ALL_PROFILES) {
    constexpr index_type n_blocks = 16384;

    SYCL_BENCHMARK("Reference: rotate only")(sycl::queue & q) {
        constexpr auto hc_size = ipow(TestType::hypercube_side_length, TestType::dimensions);

        return q.submit([&](sycl::handler &cgh) {
            sycl::local_accessor<hypercube_allocation<TestType, forward_transform_tag>> lm{1, cgh};
            auto nd_range = make_nd_range(n_blocks, hypercube_group_size<TestType>);
            cgh.parallel_for<block_transform_reference_kernel<TestType>>(nd_range, [=](hypercube_item<TestType> item) {
                gpu::hypercube_ptr<TestType, forward_transform_tag> hc{lm[0]};
                distribute_for(hc_size, item.get_group(), [&](index_type i) { hc.store(i, i); });
                black_hole(hc.memory());
            });
        });
    };

    SYCL_BENCHMARK("Forward transform")(sycl::queue & q) {
        constexpr auto hc_size = ipow(TestType::hypercube_side_length, TestType::dimensions);

        return q.submit([&](sycl::handler &cgh) {
            sycl::local_accessor<hypercube_allocation<TestType, forward_transform_tag>> lm{1, cgh};
            auto nd_range = make_nd_range(n_blocks, hypercube_group_size<TestType>);
            cgh.parallel_for<block_forward_transform_kernel<TestType>>(nd_range, [=](hypercube_item<TestType> item) {
                gpu::hypercube_ptr<TestType, forward_transform_tag> hc{lm[0]};
                distribute_for(hc_size, item.get_group(), [&](index_type i) { hc.store(i, i); });
                forward_block_transform(item.get_group(), hc);
                black_hole(hc.memory());
            });
        });
    };

    SYCL_BENCHMARK("Inverse transform")(sycl::queue & q) {
        constexpr auto hc_size = ipow(TestType::hypercube_side_length, TestType::dimensions);

        return q.submit([&](sycl::handler &cgh) {
            sycl::local_accessor<decompressor_local_allocation<TestType>> lm{1, cgh};
            auto nd_range = make_nd_range(n_blocks, hypercube_group_size<TestType>);
            cgh.parallel_for<block_inverse_transform_kernel<TestType>>(nd_range, [=](hypercube_item<TestType> item) {
                gpu::hypercube_ptr<TestType, inverse_transform_tag> hc{lm[0].hc};
                distribute_for(hc_size, item.get_group(), [&](index_type i) { hc.store(i, i); });
                inverse_block_transform(item, hc, lm[0].transform);
                black_hole(hc.memory());
            });
        });
    };
}


// Impact of dimensionality should not be that large, but the hc padding could hold surprises
TEMPLATE_TEST_CASE("Chunk encoding", "[encode]", ALL_PROFILES) {
    constexpr index_type n_blocks = 16384;
    using bits_type = typename TestType::bits_type;
    constexpr auto hc_size = ipow(TestType::hypercube_side_length, TestType::dimensions);
    constexpr index_type col_chunk_size = bits_of<bits_type>;
    constexpr index_type header_chunk_size = hc_size / col_chunk_size;
    constexpr index_type hc_total_chunks_size = hc_size + header_chunk_size;
    constexpr index_type chunks_per_hc = 1 /* header */ + hc_size / col_chunk_size;

    SYCL_BENCHMARK("Reference: serialize")(sycl::queue & q) {
        return q.submit([&](sycl::handler &cgh) {
            sycl::local_accessor<hypercube_allocation<TestType, forward_transform_tag>> lm{1, cgh};
            auto nd_range = make_nd_range(n_blocks, hypercube_group_size<TestType>);
            cgh.parallel_for<encode_reference_kernel<TestType>>(nd_range, [=](hypercube_item<TestType> item) {
                gpu::hypercube_ptr<TestType, forward_transform_tag> hc{lm[0]};
                distribute_for(hc_size, item.get_group(), [&](index_type i) { hc.store(i, i); });
                black_hole(hc.memory());
            });
        });
    };

    sycl::buffer<bits_type> chunks(n_blocks * hc_total_chunks_size);
    sycl::buffer<index_type> lengths(ceil(1 + n_blocks * chunks_per_hc, hierarchical_inclusive_scan_granularity));

    SYCL_BENCHMARK("Transpose chunks")(sycl::queue & q) {
        return q.submit([&](sycl::handler &cgh) {
            auto c = chunks.template get_access<sam::discard_write>(cgh);
            auto l = lengths.template get_access<sam::discard_write>(cgh);
            sycl::local_accessor<compressor_local_allocation<TestType>> lm{1, cgh};

            auto nd_range = make_nd_range(n_blocks, hypercube_group_size<TestType>);
            cgh.parallel_for<chunk_transpose_write_kernel<TestType>>(nd_range, [=](hypercube_item<TestType> item) {
                gpu::hypercube_ptr<TestType, forward_transform_tag> hc{lm[0].hc};
                // Set some to zero - test zero-head shortcut optimization
                distribute_for(hc_size, item.get_group(), [&](index_type i) { hc.store(i, (i > 512 ? i * 199 : 0)); });
                const auto hc_index = item.get_group_id(0);
                write_transposed_chunks(
                        item, hc, &c[hc_index * hc_total_chunks_size], &l[1 + hc_index * chunks_per_hc], lm[0].writer);
                // hack
                if (item.get_global_linear_id() == 0) { l[0] = 0; }
            });
        });
    };

    {
        sycl::queue q;
        auto scratch = hierarchical_inclusive_scan_allocate<index_type>(lengths.get_count());
        hierarchical_inclusive_scan(q, lengths, scratch, sycl::plus<index_type>{});
    }

    sycl::buffer<bits_type> stream(n_blocks * hc_total_chunks_size);

    SYCL_BENCHMARK("Compact transposed")(sycl::queue & q) {
        return q.submit([&](sycl::handler &cgh) {
            auto c = chunks.template get_access<sam::read>(cgh);
            auto l = lengths.template get_access<sam::read>(cgh);
            auto s = stream.template get_access<sam::discard_write>(cgh);
            sycl::local_accessor<compaction_local_allocation<TestType>> lm{1, cgh};

            auto nd_range = make_nd_range(n_blocks, hypercube_group_size<TestType>);
            cgh.parallel_for<chunk_compact_kernel<TestType>>(nd_range, [=](hypercube_item<TestType> item) {
                auto hc_index = static_cast<index_type>(item.get_group_id(0));
                index_type offset_after;
                compact_chunks<TestType>(item.get_group(), &c.get_pointer()[hc_index * hc_total_chunks_size],
                        &l.get_pointer()[hc_index * chunks_per_hc], &offset_after, &s.get_pointer()[0], lm[0]);
            });
        });
    };
}


// Impact of dimensionality should not be that large, but the hc padding could hold surprises
TEMPLATE_TEST_CASE("Chunk decoding", "[decode]", ALL_PROFILES) {
    constexpr index_type n_blocks = 16384;
    using bits_type = typename TestType::bits_type;
    constexpr auto hc_size = ipow(TestType::hypercube_side_length, TestType::dimensions);

    sycl::buffer<bits_type> columns(n_blocks * hc_size);
    sycl::queue{}.submit([&](sycl::handler &cgh) {
        cgh.fill(columns.template get_access<sam::discard_write>(cgh),
                static_cast<bits_type>(7948741984121192831 /* whatever */));
    });

    SYCL_BENCHMARK("Read and transpose")(sycl::queue & q) {
        return q.submit([&](sycl::handler &cgh) {
            auto c = columns.template get_access<sam::read>(cgh);
            sycl::local_accessor<decompressor_local_allocation<TestType>> lm{1, cgh};

            auto nd_range = make_nd_range(n_blocks, hypercube_group_size<TestType>);
            cgh.parallel_for<chunk_transpose_read_kernel<TestType>>(nd_range, [=](hypercube_item<TestType> item) {
                gpu::hypercube_ptr<TestType, inverse_transform_tag> hc{lm[0].hc};
                const auto hc_index = item.get_group_id(0);
                const bits_type *column = c.get_pointer();
                read_transposed_chunks(item, hc, &column[hc_index * hc_size], lm[0].reader);
            });
        });
    };
}
