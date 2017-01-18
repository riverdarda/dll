//=======================================================================
// Copyright (c) 2014-2016 Baptiste Wicht
// Distributed under the terms of the MIT License.
// (See accompanying file LICENSE or copy at
//  http://opensource.org/licenses/MIT)
//=======================================================================

#pragma once

#include "dll/neural_layer.hpp"

namespace dll {

/*!
 * \brief Standard dynamic convolutional layer of neural network.
 */
template <typename Desc>
struct dyn_conv_layer final : neural_layer<dyn_conv_layer<Desc>, Desc> {
    using desc      = Desc;                  ///< The descriptor type
    using weight    = typename desc::weight; ///< The weight type
    using this_type = dyn_conv_layer<desc>;  ///< This type
    using base_type = neural_layer<this_type, desc>;

    static constexpr const bool dbn_only = layer_traits<this_type>::is_dbn_only();

    static constexpr auto activation_function = desc::activation_function;
    static constexpr auto w_initializer       = desc::w_initializer;
    static constexpr auto b_initializer       = desc::b_initializer;

    using input_one_t  = etl::dyn_matrix<weight, 3>; ///< The type for one input
    using output_one_t = etl::dyn_matrix<weight, 3>; ///< The type for one output
    using input_t      = std::vector<input_one_t>;   ///< The type for many input
    using output_t     = std::vector<output_one_t>;  ///< The type for many output

    using w_type = etl::dyn_matrix<weight, 4>;
    using b_type = etl::dyn_matrix<weight, 1>;

    //Weights and biases
    w_type w; //!< Weights
    b_type b; //!< Hidden biases

    //Backup weights and biases
    std::unique_ptr<w_type> bak_w; //!< Backup Weights
    std::unique_ptr<b_type> bak_b; //!< Backup Hidden biases

    size_t nv1; ///< The first visible dimension
    size_t nv2; ///< The second visible dimension
    size_t nh1; ///< The first output dimension
    size_t nh2; ///< The second output dimension
    size_t nc;  ///< The number of input channels
    size_t k;   ///< The number of filters

    size_t nw1; ///< The first dimension of the filters
    size_t nw2; ///< The second dimension of the filters

    dyn_conv_layer(): base_type() {
        // Nothing else to init
    }

    void init_layer(size_t nc, size_t nv1, size_t nv2, size_t k, size_t nh1, size_t nh2){
        this->nv1 = nv1;
        this->nv2 = nv2;
        this->nh1 = nh1;
        this->nh2 = nh2;
        this->nc = nc;
        this->k = k;

        this->nw1 = nv1 - nh1 + 1;
        this->nw2 = nv2 - nh2 + 1;

        w = etl::dyn_matrix<weight, 4>(k, nc, nw1, nw2);

        b = etl::dyn_vector<weight>(k);

        initializer_function<w_initializer>::initialize(w, input_size(), output_size());
        initializer_function<b_initializer>::initialize(b, input_size(), output_size());
    }

    std::size_t input_size() const noexcept {
        return nc * nv1 * nv2;
    }

    std::size_t output_size() const noexcept {
        return k * nh1 * nh2;
    }

    std::size_t parameters() const noexcept {
        return k * nw1 * nw2;
    }

    std::string to_short_string() const {
        char buffer[1024];
        snprintf(buffer, 1024, "Conv(dyn): %lux%lux%lu -> (%lux%lux%lu) -> %s -> %lux%lux%lu", nc, nv1, nv2, k, nw1, nw2, to_string(activation_function).c_str(), k, nh1, nh2);
        return {buffer};
    }

    void activate_hidden(output_one_t& output, const input_one_t& v) const {
        auto b_rep = etl::force_temporary(etl::rep(b, nh1, nh2));

        etl::reshape(output, 1, k, nh1, nh2) = etl::conv_4d_valid_flipped(etl::reshape(v, 1, nc, nv1, nv2), w);

        output = f_activate<activation_function>(b_rep + output);
    }

    template <typename V>
    void activate_hidden(output_one_t& output, const V& v) const {
        decltype(auto) converted = converter_one<V, input_one_t>::convert(*this, v);
        activate_hidden(output, converted);
    }

    template <typename H1, typename V>
    void batch_activate_hidden(H1&& output, const V& v) const {
        output = etl::conv_4d_valid_flipped(v, w);

        auto b_rep = etl::force_temporary(etl::rep_l(etl::rep(b, nh1, nh2), etl::dim<0>(output)));

        output = f_activate<activation_function>(b_rep + output);
    }

    void prepare_input(input_one_t& input) const {
        input = input_one_t(nc, nv1, nv2);
    }

    template <typename Input>
    output_t prepare_output(std::size_t samples) const {
        output_t output;
        output.reserve(samples);
        for(size_t i = 0; i < samples; ++i){
            output.emplace_back(k, nh1, nh2);
        }
        return output;
    }

    template <typename Input>
    output_one_t prepare_one_output() const {
        return output_one_t(k, nh1, nh2);
    }

    template <typename DBN>
    void init_sgd_context() {
        this->sgd_context_ptr = std::make_shared<sgd_context<DBN, this_type>>(nc, nv1, nv2, k, nh1, nh2);
    }

    template<typename DRBM>
    static void dyn_init(DRBM&){
        //Nothing to change
    }

    template<typename C>
    void adapt_errors(C& context) const {
        context.errors = f_derivative<activation_function>(context.output) >> context.errors;
    }

    template<typename H, typename C>
    void backward_batch(H&& output, C& context) const {
        output = etl::conv_4d_full_flipped(context.errors, w);
    }

    template<typename C>
    void compute_gradients(C& context) const {
        context.w_grad = conv_4d_valid_filter_flipped(context.input, context.errors);
        context.b_grad = etl::mean_r(etl::sum_l(context.errors));
    }
};

} //end of dll namespace