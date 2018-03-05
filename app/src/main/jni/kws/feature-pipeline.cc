/* Created on 2017-08-02
 * Author: Binbin Zhang
 */

#include "feature-pipeline.h"

FeaturePipeline::FeaturePipeline(const FeaturePipelineConfig &config):
        config_(config),
        left_context_(config.left_context), 
        right_context_(config.right_context),
        raw_feat_dim_(config.num_bins), 
        fbank_(config.num_bins, config.sample_rate, config.frame_length, config.frame_shift),
        num_frames_(0),
        done_(false) {
    ReadCmvn(config.cmvn_file);
}

void FeaturePipeline::ReadCmvn(const std::string cmvn_file) {
    std::ifstream is(cmvn_file, std::ifstream::binary);   
    if (is.fail()) {
       ERROR("read file %s error, check!!!", cmvn_file.c_str()); 
    }
    cmvn_.Read(is);
}

void FeaturePipeline::AcceptRawWav(const std::vector<float> &wav) {
    std::vector<float> feat;
    std::vector<float> waves;
    waves.insert(waves.end(), ctx_wav_.begin(), ctx_wav_.end());
    waves.insert(waves.end(), wav.begin(), wav.end());
    int num_frames = fbank_.Compute(waves, &feat);
    // do cmvn
    assert(raw_feat_dim_ == cmvn_.NumCols());
    for (int i = 0; i < num_frames; i++) {
        for (int j = 0; j < raw_feat_dim_; j++) {
            feat[i*raw_feat_dim_+j] = 
                (feat[i*raw_feat_dim_+j] - cmvn_(0, j)) * cmvn_(1, j);
            //printf("%f ", feat[i*raw_feat_dim+j]);
        }
        //printf("\n");
    }
    if (feature_buf_.size() == 0 && left_context_ > 0) { 
        for (int i = 0; i < left_context_; i++) {
            feature_buf_.insert(feature_buf_.end(), 
                                feat.begin(), feat.begin() + raw_feat_dim_);
        }
    }
    feature_buf_.insert(feature_buf_.end(), feat.begin(), feat.end());
    num_frames_ += num_frames;
    
    int left_samples = waves.size() - config_.frame_shift * num_frames;
    ctx_wav_.resize(left_samples);
    std::copy(waves.begin() + config_.frame_shift * num_frames, 
              waves.end(), ctx_wav_.begin());
}

int FeaturePipeline::NumFramesReady() const {
    if (num_frames_ < right_context_) return 0;
    if (done_) return num_frames_;
    else return num_frames_ - right_context_;
}

void FeaturePipeline::SetDone() { 
    assert(!done_);
    done_ = true; 
    if (num_frames_ == 0) return;
    // copy last frames to buffer
    std::vector<float> last_feat(feature_buf_.end() - raw_feat_dim_, feature_buf_.end());
    for (int i = 0; i < right_context_; i++) {
        feature_buf_.insert(feature_buf_.end(), last_feat.begin(), last_feat.end());
    }
}

int FeaturePipeline::ReadFeature(int t, std::vector<float> *feat) {
    assert(t < num_frames_);
    int num_frames_ready = NumFramesReady();
    if (num_frames_ready <= 0) return 0;
    int total_frame = num_frames_ready - t;
    int feat_dim = (left_context_ + 1 + right_context_) * raw_feat_dim_;
    feat->resize(total_frame * feat_dim);
    for (int i = t; i < num_frames_ready; i++) {
        memcpy(feat->data() + (i - t) * feat_dim,
               feature_buf_.data() + i * raw_feat_dim_,
               sizeof(float) * feat_dim);
    }
    return total_frame;
}

int FeaturePipeline::ReadAllFeature(std::vector<float> *feat) {
    return ReadFeature(0, feat);
}

int FeaturePipeline::NumFrames(int size) const {
    return 1 + (size - config_.frame_length) / config_.frame_shift;
}


