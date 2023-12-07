#include "constants.hpp"
#include "input_t.hpp"
#include "tuple_t.hpp"

#include "ap_int.h"

typedef ap_uint<5> win_state_t;
typedef ap_uint<5> win_size_t;

struct predictor_t{
    win_state_t win[WIN_DIM];
    win_size_t win_elems;
};

static const float trans_prob[NUM_STATES][NUM_STATES] = {
    // values
    0x1.a07a1ep-4, 0x1.0a1522p-3, 0x1.e9bf02p-5, 0x1.13149cp-6, 0x1.a83086p-6, 0x1.6d0f62p-7, 0x1.54f55ap-3, 0x1.a267b8p-3, 0x1.5cfe08p-4, 0x1.e0bf88p-6, 0x1.145dacp-5, 0x1.d9091ep-7, 0x1.371284p-5, 0x1.74c5ccp-5, 0x1.4e35cp-6,  0x1.7233a8p-8, 0x1.2f5c1cp-7, 0x1.49117ap-9,
    0x1.9123a6p-4, 0x1.038bcap-3, 0x1.dbcafep-5, 0x1.447eaap-6, 0x1.68541cp-6, 0x1.526e1ep-7, 0x1.447eaap-3, 0x1.bd6f0cp-3, 0x1.7eb984p-4, 0x1.b7f58cp-6, 0x1.24a47cp-5, 0x1.f5ac44p-7, 0x1.22a6dap-5, 0x1.8531d4p-5, 0x1.349194p-6, 0x1.4e72d8p-8, 0x1.02ccacp-7, 0x1.8e2732p-9,
    0x1.b74de4p-4, 0x1.0c0568p-3, 0x1.b5073p-5,  0x1.4337fap-6, 0x1.a2d18ep-6, 0x1.6c30a8p-7, 0x1.33dabap-3, 0x1.a11c86p-3, 0x1.48e8bep-4, 0x1.eba816p-6, 0x1.235a2p-5,  0x1.4337fap-6, 0x1.5326e8p-5, 0x1.7e664ap-5, 0x1.99b6bcp-6, 0x1.c73cd2p-8, 0x1.2c74fp-7,  0x1.b5073p-9,
    0x1.b2883cp-4, 0x1.19609cp-3, 0x1.c7e712p-5, 0x1.e46582p-7, 0x1.55ed4ep-6, 0x1.55ed4ep-7, 0x1.2967bcp-3, 0x1.a7d8d2p-3, 0x1.6edbfp-4,  0x1.15d0cep-5, 0x1.079196p-5, 0x1.8eea3p-7,  0x1.6b4c22p-5, 0x1.9d2968p-5, 0x1.726bbep-6, 0x1.c7e712p-8, 0x1.0071fap-7, 0x1.55ed4ep-9,
    0x1.9f6a74p-4, 0x1.19191ap-3, 0x1.d14406p-5, 0x1.362d5ap-6, 0x1.8ecc98p-6, 0x1.e76bd6p-7, 0x1.4c552ap-3, 0x1.a0ccfp-3,  0x1.7e2ebcp-4, 0x1.c6301ep-6, 0x1.30a366p-5, 0x1.bb1c36p-7, 0x1.09ddbap-5, 0x1.78a4c8p-5, 0x1.14f1a2p-6, 0x1.627cf8p-8, 0x1.8ecc98p-8, 0x1.09ddbap-9,
    0x1.73cf3cp-4, 0x1.2aaaaap-3, 0x1p-4,        0x1.555556p-6, 0x1.555556p-6, 0x1.e79e7ap-7, 0x1.30c30cp-3, 0x1.79e79ep-3, 0x1.986186p-4, 0x1p-5,        0x1.861862p-6, 0x1.b6db6ep-7, 0x1.30c30cp-5, 0x1.c30c3p-5,  0x1.6db6dcp-6, 0x1.24924ap-7, 0x1.861862p-7, 0x1.e79e7ap-8,
    0x1.a74c7p-4,  0x1.0fb58ap-3, 0x1.f5debcp-5, 0x1.282f84p-6, 0x1.917efcp-6, 0x1.282f84p-7, 0x1.47dc6ap-3, 0x1.a0ec2p-3,  0x1.78d05cp-4, 0x1.aa2d9cp-6, 0x1.196056p-5, 0x1.f783f8p-7, 0x1.188db8p-5, 0x1.65e414p-5, 0x1.5cd74p-6,  0x1.e70f8ep-8, 0x1.355972p-7, 0x1.d9e59ep-9,
    0x1.a5d1dcp-4, 0x1.10a4bap-3, 0x1.ccdc08p-5, 0x1.51b5bp-6,  0x1.6ee902p-6, 0x1.5e67d4p-7, 0x1.4554cep-3, 0x1.a5809ap-3, 0x1.71730ap-4, 0x1.b9d0d4p-6, 0x1.2b9f46p-5, 0x1.160a0ap-6, 0x1.2a5a42p-5, 0x1.5e67d4p-5, 0x1.3eaa7cp-6, 0x1.d84928p-8, 0x1.19d914p-7, 0x1.0d26fp-8,
    0x1.a07e1ep-4, 0x1.1181b2p-3, 0x1.e9d1ap-5,  0x1.2b2bb8p-6, 0x1.71905cp-6, 0x1.31097p-7,  0x1.356fbcp-3, 0x1.ad5322p-3, 0x1.651934p-4, 0x1.bdd2b8p-6, 0x1.23d692p-5, 0x1.16a3b4p-6, 0x1.31097p-5,  0x1.963a1cp-5, 0x1.6bb2a4p-6, 0x1.9aa066p-8, 0x1.19929p-7,  0x1.a65bd8p-9,
    0x1.afa57ep-4, 0x1.27e2fep-3, 0x1.813b9cp-5, 0x1.1fc3aap-6, 0x1.c6da7p-6,  0x1.03ea88p-7, 0x1.4ab266p-3, 0x1.961e76p-3, 0x1.6a06acp-4, 0x1.e2b39p-6,  0x1.65627cp-5, 0x1.f5445p-7,  0x1.e2b39p-6,  0x1.7c976cp-5, 0x1.44e52ap-6, 0x1.bd920ep-9, 0x1.4e2d8cp-7, 0x1.03ea88p-8,
    0x1.8c9094p-4, 0x1.efb4bap-4, 0x1.02de62p-4, 0x1.d600bp-7,  0x1.592882p-6, 0x1.a9f0ap-7,  0x1.592882p-3, 0x1.a1ad9cp-3, 0x1.71068ap-4, 0x1.25c06ep-5, 0x1.0fb866p-5, 0x1.d600bp-7,  0x1.2d187p-5,  0x1.818c9p-5,  0x1.67d886p-6, 0x1.086064p-7, 0x1.f360bcp-8, 0x1.9b409ap-9,
    0x1.b30bccp-4, 0x1.0290acp-3, 0x1.37eb7ap-4, 0x1.cbabdep-7, 0x1.17161p-6,  0x1.48563p-7,  0x1.4a6386p-3, 0x1.b0fe76p-3, 0x1.692b9cp-4, 0x1.ec8148p-6, 0x1.1f4b6ap-5, 0x1.2780c6p-6, 0x1.37eb7ap-5, 0x1.58c0e6p-5, 0x1.8a0106p-7, 0x1.06ab5ap-8, 0x1.cbabdep-8, 0x1.8a0106p-9,
    0x1.d3daaep-4, 0x1.0280ccp-3, 0x1.85933ap-5, 0x1.2a8d8ap-6, 0x1.47ae14p-6, 0x1.5d867cp-7, 0x1.621392p-3, 0x1.925178p-3, 0x1.925178p-4, 0x1.9f0fb4p-6, 0x1.440a04p-5, 0x1.31d5acp-6, 0x1.d950c8p-6, 0x1.735ee4p-5, 0x1.2a8d8ap-6, 0x1.b4e81cp-8, 0x1.4ef638p-7, 0x1.5d867cp-9,
    0x1.a12c06p-4, 0x1.1fe092p-3, 0x1.c375c6p-5, 0x1.f6e468p-7, 0x1.84993ap-6, 0x1.900724p-7, 0x1.4e4f1cp-3, 0x1.94505cp-3, 0x1.5b2ac6p-4, 0x1.fc9b5ep-6, 0x1.1dbbf6p-5, 0x1.b250e6p-7, 0x1.2ee0d6p-5, 0x1.a12c06p-5, 0x1.8a503p-6,  0x1.56e18ep-8, 0x1.56e18ep-8, 0x1.124e0ap-8,
    0x1.aba818p-4, 0x1.2045b2p-3, 0x1.e7d866p-5, 0x1.2370a8p-6, 0x1.e1827ap-6, 0x1.16c4cep-7, 0x1.613672p-3, 0x1.7a8e28p-3, 0x1.431e4ap-4, 0x1.a22734p-6, 0x1.4fca26p-5, 0x1.562014p-6, 0x1.fada3p-6,  0x1.62cbeep-5, 0x1.6f77c8p-6, 0x1.62cbeep-8, 0x1.301c82p-7, 0x1.301c82p-8,
    0x1.4a27fap-4, 0x1.0717dcp-3, 0x1.01ef3cp-4, 0x1.ef3bf8p-7, 0x1.ef3bf8p-6, 0x1.9cb1fap-7, 0x1.20e2fcp-3, 0x1.bba5bap-3, 0x1.691bbap-4, 0x1.4a27fap-6, 0x1.20e2fcp-5, 0x1.4a27fap-6, 0x1.5eca7ap-5, 0x1.da9978p-5, 0x1.4a27fap-6, 0x1.ef3bf8p-8, 0x1.9cb1fap-7, 0x1.ef3bf8p-8,
    0x1.91459p-4,  0x1.32a1d6p-3, 0x1.c64516p-5, 0x1.a7fc36p-7, 0x1.e48df6p-6, 0x1.e48df6p-8, 0x1.1beb2ep-3, 0x1.bae9c2p-3, 0x1.beb2dep-4, 0x1.3dfd2ap-5, 0x1.c64516p-6, 0x1.108fdap-6, 0x1.e48df6p-6, 0x1.016b6ap-5, 0x1.6b6a78p-6, 0x1.e48df6p-8, 0x1.e48df6p-9, 0x1.6b6a78p-8,
    0x1.ae408ap-4, 0x1.2d2d2ep-3, 0x1.e1e1e2p-5, 0x1.9d0ac2p-5, 0x1.135c82p-5, 0x1.5833a2p-6, 0x1.f317aap-4, 0x1.7a9f32p-3, 0x1.5833a2p-4, 0x1.e1e1e2p-6, 0x1.5833a2p-6, 0x1.5833a2p-6, 0x1.e1e1e2p-6, 0x1.35c812p-5, 0x1.9d0ac2p-6, 0x1.135c82p-7, 0x1.135c82p-6, 0x1.135c82p-8
};

struct predictor {

    predictor_t states[STATE_SIZE / PREDICTOR_PAR];

    predictor()
    {
        #pragma HLS BIND_STORAGE variable=states type=RAM_S2P impl=bram
    }

    float get_local_metric(const win_state_t win[WIN_DIM]) {
    #pragma HLS INLINE

        float param[WIN_DIM - 1];

        for (int i = 0; i < WIN_DIM - 1; ++i) {
        #pragma HLS unroll
            param[i] = 0;
            win_state_t prev = win[i];
            win_state_t curr = win[i + 1];
            for (int j = 0; j < NUM_STATES; ++j) {
            #pragma HLS unroll
                param[i] += (j != curr ? trans_prob[prev][j] : 0);
            }
        }

        float sum = 0;
        for (int i = 0; i < WIN_DIM - 1; ++i) {
        #pragma HLS unroll
            sum += param[i];
        }

        return sum / (WIN_DIM - 1);
    }

    template <typename T>
    void operator() (input_t in, fx::FlatMapShipper<T> & shipper) {
    #pragma HLS INLINE
        const unsigned int idx = in.key / PREDICTOR_PAR;

        // get state
        predictor_t p = states[idx];

        // push new element into window
        for (int i = 0; i < (WIN_DIM - 1); ++i) {
        #pragma HLS unroll
            p.win[i] = p.win[i + 1];
        }
        p.win[WIN_DIM - 1] = (win_state_t)in.state_id;

        // increment window elements up to WIN_DIM
        if (p.win_elems < WIN_DIM) {
            p.win_elems++;
        }

        // update state
        states[idx] = p;

        // calculate score if window is full
        float score = 0;
        if (p.win_elems == WIN_DIM) {
            score = get_local_metric(p.win);
        }

        // output
        if (score > THRESHOLD) {
            tuple_t out;
            out.key = in.key;
            out.state_id = in.state_id;
            out.score = score;
            out.timestamp = in.timestamp;
            shipper.send(out);
        }
    }
};