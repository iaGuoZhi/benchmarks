#include "common.hh"

#include <ndzip/offload.hh>


namespace ndzip {

void compressor_requirements::include(const extent &data_size) {
    if (_dims == -1) {
        _dims = data_size.dimensions();
    } else {
        if (data_size.dimensions() != _dims) {
            throw std::runtime_error{"Cannot add a " + std::to_string(data_size.dimensions())
                    + "-dimensional extent to " + std::to_string(_dims) + "-dimensional compressor_requirements"};
        }
    }
    _max_num_hypercubes = std::max(_max_num_hypercubes, detail::num_hypercubes(data_size));
}

compressor_requirements::compressor_requirements(const extent &single_data_size) {
    include(single_data_size);
}

compressor_requirements::compressor_requirements(std::initializer_list<extent> data_sizes) {
    for (auto ds : data_sizes) {
        include(ds);
    }
}


template<typename T, ndzip::dim_type Dims>
static index_type compressed_length_bound(const detail::static_extent<Dims> &size) {
    using profile = detail::profile<T, Dims>;
    using bits_type = typename profile::bits_type;

    const auto num_hypercubes = detail::num_hypercubes(size);
    const auto header_length
            = detail::div_ceil(num_hypercubes, detail::bits_of<bits_type> / detail::bits_of<index_type>);
    const auto compressed_length_bound = num_hypercubes * profile::compressed_block_length_bound;
    const auto border_length = detail::border_element_count(size, profile::hypercube_side_length);
    return header_length + compressed_length_bound + border_length;
}

template<typename T>
index_type compressed_length_bound(const extent &size) {
    switch (size.dimensions()) {
        case 1: return compressed_length_bound<T, 1>(detail::static_extent<1>{size});
        case 2: return compressed_length_bound<T, 2>(detail::static_extent<2>{size});
        case 3: return compressed_length_bound<T, 3>(detail::static_extent<3>{size});
        default: abort();
    }
}

template index_type compressed_length_bound<float>(const extent &);
template index_type compressed_length_bound<double>(const extent &);

}  // namespace ndzip
