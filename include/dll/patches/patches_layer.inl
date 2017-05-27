//=======================================================================
// Copyright (c) 2014-2017 Baptiste Wicht
// Distributed under the terms of the MIT License.
// (See accompanying file LICENSE or copy at
//  http://opensource.org/licenses/MIT)
//=======================================================================

#pragma once

#include "dll/base_traits.hpp"
#include "dll/layer.hpp"

namespace dll {

/*!
 * \brief Layer to cut images into patches.
 */
template <typename Desc>
struct patches_layer : layer<patches_layer<Desc>> {
    using desc = Desc;
    using base_type = layer<patches_layer<Desc>>; ///< The base type

    static constexpr size_t width    = desc::width;
    static constexpr size_t height   = desc::height;
    static constexpr size_t v_stride = desc::v_stride;
    static constexpr size_t h_stride = desc::h_stride;

    using weight = typename desc::weight;

    using input_one_t = etl::dyn_matrix<weight, 3>;
    using input_t     = std::vector<input_one_t>;

    using output_one_t = std::vector<etl::fast_dyn_matrix<weight, 1UL, height, width>>;
    using output_t     = std::vector<output_one_t>;

    patches_layer() = default;

    /*!
     * \brief Returns a short description of the layer
     * \return an std::string containing a short description of the layer
     */
    static std::string to_short_string() {
        char buffer[1024];
        snprintf(buffer, 1024, "Patches -> (%lu:%lux%lu:%lu)", height, v_stride, width, h_stride);
        return {buffer};
    }

    /*!
     * \brief Return the size of the output of this layer
     * \return The size of the output of this layer
     */
    static constexpr size_t output_size() noexcept {
        return width * height;
    }

    using base_type::activate_hidden;

    template<typename Input>
    static void activate_hidden(output_one_t& h_a, const Input& input) {
        cpp_assert(etl::dim<0>(input) == 1, "Only one channel is supported for now");

        h_a.clear();

        for (size_t y = 0; y + height <= etl::dim<1>(input); y += v_stride) {
            for (size_t x = 0; x + width <= etl::dim<2>(input); x += h_stride) {
                h_a.emplace_back();

                auto& patch = h_a.back();

                for (size_t yy = 0; yy < height; ++yy) {
                    for (size_t xx = 0; xx < width; ++xx) {
                        patch(0, yy, xx) = input(0, y + yy, x + xx);
                    }
                }
            }
        }
    }

    static void activate_many(output_t& h_a, const input_t& input) {
        for (size_t i = 0; i < input.size(); ++i) {
            activate_one(input[i], h_a[i]);
        }
    }

    /*!
     * \brief Prepare a set of empty outputs for this layer
     * \param samples The number of samples to prepare the output for
     * \return a container containing empty ETL matrices suitable to store samples output of this layer
     * \tparam Input The type of one input
     */
    template <typename Input>
    static output_t prepare_output(size_t samples) {
        return output_t(samples);
    }

    /*!
     * \brief Prepare one empty output for this layer
     * \return an empty ETL matrix suitable to store one output of this layer
     *
     * \tparam Input The type of one Input
     */
    template <typename Input>
    static output_one_t prepare_one_output() {
        return output_one_t();
    }

    /*!
     * \brief Initialize the dynamic version of the layer from the
     * fast version of the layer
     * \param dyn Reference to the dynamic version of the layer that
     * needs to be initialized
     */
    template<typename DRBM>
    static void dyn_init(DRBM& dyn){
        dyn.init_layer(width, height, v_stride, h_stride);
    }
};

//Allow odr-use of the constexpr static members

template <typename Desc>
const size_t patches_layer<Desc>::width;

template <typename Desc>
const size_t patches_layer<Desc>::height;

template <typename Desc>
const size_t patches_layer<Desc>::v_stride;

template <typename Desc>
const size_t patches_layer<Desc>::h_stride;

// Declare the traits for the layer

template<typename Desc>
struct layer_base_traits<patches_layer<Desc>> {
    static constexpr bool is_neural     = false; ///< Indicates if the layer is a neural layer
    static constexpr bool is_dense      = false; ///< Indicates if the layer is dense
    static constexpr bool is_conv       = false; ///< Indicates if the layer is convolutional
    static constexpr bool is_deconv     = false; ///< Indicates if the layer is deconvolutional
    static constexpr bool is_standard   = false; ///< Indicates if the layer is standard
    static constexpr bool is_rbm        = false; ///< Indicates if the layer is RBM
    static constexpr bool is_pooling    = false; ///< Indicates if the layer is a pooling layer
    static constexpr bool is_unpooling  = false; ///< Indicates if the layer is an unpooling laye
    static constexpr bool is_transform  = false;  ///< Indicates if the layer is a transform layer
    static constexpr bool is_patches    = true; ///< Indicates if the layer is a patches layer
    static constexpr bool is_dynamic    = false; ///< Indicates if the layer is dynamic
    static constexpr bool pretrain_last = false; ///< Indicates if the layer is dynamic
    static constexpr bool sgd_supported = true;  ///< Indicates if the layer is supported by SGD
};

} //end of dll namespace
