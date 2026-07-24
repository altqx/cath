#include "cath/media/movie_player.hpp"

#include "cath/platform/log.hpp"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
}

#include <cstring>
#include <vector>

namespace cath {

struct MoviePlayer::Impl {
  AVFormatContext* fmt = nullptr;
  AVCodecContext* vctx = nullptr;
  AVCodecContext* actx = nullptr;
  SwsContext* sws = nullptr;
  SwrContext* swr = nullptr;
  AVFrame* frame = nullptr;
  AVFrame* rgba = nullptr;
  AVPacket* pkt = nullptr;
  int vstream = -1;
  int astream = -1;
  int out_rate = 48000;
  int out_ch = 2;
  std::vector<uint8_t> rgba_buf;
};

MoviePlayer::MoviePlayer() : impl_(new Impl) {}

MoviePlayer::~MoviePlayer() {
  close();
  delete impl_;
  impl_ = nullptr;
}

void MoviePlayer::close() {
  if (!impl_) {
    return;
  }
  if (impl_->swr) {
    swr_free(&impl_->swr);
  }
  if (impl_->sws) {
    sws_freeContext(impl_->sws);
    impl_->sws = nullptr;
  }
  if (impl_->rgba) {
    av_frame_free(&impl_->rgba);
  }
  if (impl_->frame) {
    av_frame_free(&impl_->frame);
  }
  if (impl_->pkt) {
    av_packet_free(&impl_->pkt);
  }
  if (impl_->vctx) {
    avcodec_free_context(&impl_->vctx);
  }
  if (impl_->actx) {
    avcodec_free_context(&impl_->actx);
  }
  if (impl_->fmt) {
    avformat_close_input(&impl_->fmt);
  }
  impl_->rgba_buf.clear();
  opened_ = false;
  width_ = height_ = 0;
  duration_ = 0;
}

bool MoviePlayer::open(const std::filesystem::path& path, std::string* error) {
  close();
  if (avformat_open_input(&impl_->fmt, path.string().c_str(), nullptr, nullptr) < 0) {
    if (error) {
      *error = "avformat_open_input failed";
    }
    return false;
  }
  if (avformat_find_stream_info(impl_->fmt, nullptr) < 0) {
    if (error) {
      *error = "avformat_find_stream_info failed";
    }
    close();
    return false;
  }
  duration_ = impl_->fmt->duration > 0 ? double(impl_->fmt->duration) / AV_TIME_BASE : 0.0;

  impl_->vstream = av_find_best_stream(impl_->fmt, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
  impl_->astream = av_find_best_stream(impl_->fmt, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
  if (impl_->vstream < 0) {
    if (error) {
      *error = "no video stream";
    }
    close();
    return false;
  }

  auto open_codec = [&](int index, AVCodecContext*& ctx) -> bool {
    const AVCodec* codec = avcodec_find_decoder(impl_->fmt->streams[index]->codecpar->codec_id);
    if (!codec) {
      return false;
    }
    ctx = avcodec_alloc_context3(codec);
    if (!ctx) {
      return false;
    }
    if (avcodec_parameters_to_context(ctx, impl_->fmt->streams[index]->codecpar) < 0) {
      return false;
    }
    return avcodec_open2(ctx, codec, nullptr) >= 0;
  };

  if (!open_codec(impl_->vstream, impl_->vctx)) {
    if (error) {
      *error = "video codec open failed";
    }
    close();
    return false;
  }
  width_ = impl_->vctx->width;
  height_ = impl_->vctx->height;

  if (impl_->astream >= 0) {
    if (!open_codec(impl_->astream, impl_->actx)) {
      CATH_LOG_WARN("movie audio codec open failed; continuing silent");
      impl_->astream = -1;
    } else {
      AVChannelLayout out_ch{};
      av_channel_layout_default(&out_ch, impl_->out_ch);
      if (swr_alloc_set_opts2(&impl_->swr, &out_ch, AV_SAMPLE_FMT_FLT, impl_->out_rate, &impl_->actx->ch_layout,
                             impl_->actx->sample_fmt, impl_->actx->sample_rate, 0, nullptr) < 0 ||
          swr_init(impl_->swr) < 0) {
        CATH_LOG_WARN("swr_init failed; movie audio disabled");
        swr_free(&impl_->swr);
        impl_->astream = -1;
      }
    }
  }

  impl_->frame = av_frame_alloc();
  impl_->rgba = av_frame_alloc();
  impl_->pkt = av_packet_alloc();
  const int buf_size = av_image_get_buffer_size(AV_PIX_FMT_RGBA, width_, height_, 1);
  impl_->rgba_buf.resize(size_t(buf_size));
  av_image_fill_arrays(impl_->rgba->data, impl_->rgba->linesize, impl_->rgba_buf.data(), AV_PIX_FMT_RGBA, width_,
                       height_, 1);
  impl_->sws = sws_getContext(width_, height_, impl_->vctx->pix_fmt, width_, height_, AV_PIX_FMT_RGBA, SWS_BILINEAR,
                              nullptr, nullptr, nullptr);
  if (!impl_->sws) {
    if (error) {
      *error = "sws_getContext failed";
    }
    close();
    return false;
  }
  opened_ = true;
  CATH_LOG_INFO("movie open %s (%dx%d)", path.string().c_str(), width_, height_);
  return true;
}

bool MoviePlayer::next_frame(MovieFrame& out, std::string* error) {
  if (!opened_) {
    return false;
  }
  while (av_read_frame(impl_->fmt, impl_->pkt) >= 0) {
    if (impl_->pkt->stream_index == impl_->vstream) {
      if (avcodec_send_packet(impl_->vctx, impl_->pkt) < 0) {
        av_packet_unref(impl_->pkt);
        continue;
      }
      av_packet_unref(impl_->pkt);
      const int rr = avcodec_receive_frame(impl_->vctx, impl_->frame);
      if (rr == AVERROR(EAGAIN) || rr == AVERROR_EOF) {
        continue;
      }
      if (rr < 0) {
        if (error) {
          *error = "receive_frame failed";
        }
        return false;
      }
      // Recreate sws if pix_fmt changes mid-stream
      if (impl_->frame->format != impl_->vctx->pix_fmt) {
        sws_freeContext(impl_->sws);
        impl_->sws = sws_getContext(width_, height_, AVPixelFormat(impl_->frame->format), width_, height_,
                                    AV_PIX_FMT_RGBA, SWS_BILINEAR, nullptr, nullptr, nullptr);
      }
      sws_scale(impl_->sws, impl_->frame->data, impl_->frame->linesize, 0, height_, impl_->rgba->data,
                impl_->rgba->linesize);
      out.image.width = uint32_t(width_);
      out.image.height = uint32_t(height_);
      out.image.pixels.resize(size_t(width_) * height_ * 4);
      const int src_stride = impl_->rgba->linesize[0];
      for (int y = 0; y < height_; ++y) {
        std::memcpy(out.image.pixels.data() + size_t(y) * width_ * 4, impl_->rgba->data[0] + y * src_stride,
                    size_t(width_) * 4);
      }
      const AVRational tb = impl_->fmt->streams[impl_->vstream]->time_base;
      out.pts = impl_->frame->best_effort_timestamp * av_q2d(tb);
      av_frame_unref(impl_->frame);
      return true;
    }
    if (impl_->pkt->stream_index == impl_->astream && impl_->swr) {
      // Drop audio packets here; game can call next_audio in a dedicated path later.
    }
    av_packet_unref(impl_->pkt);
  }
  // flush
  avcodec_send_packet(impl_->vctx, nullptr);
  if (avcodec_receive_frame(impl_->vctx, impl_->frame) == 0) {
    sws_scale(impl_->sws, impl_->frame->data, impl_->frame->linesize, 0, height_, impl_->rgba->data,
              impl_->rgba->linesize);
    out.image.width = uint32_t(width_);
    out.image.height = uint32_t(height_);
    out.image.pixels.resize(size_t(width_) * height_ * 4);
    for (int y = 0; y < height_; ++y) {
      std::memcpy(out.image.pixels.data() + size_t(y) * width_ * 4, impl_->rgba->data[0] + y * impl_->rgba->linesize[0],
                  size_t(width_) * 4);
    }
    out.pts = 0;
    av_frame_unref(impl_->frame);
    return true;
  }
  return false;
}

bool MoviePlayer::next_audio(std::vector<float>& interleaved, int& sample_rate, int& channels) {
  interleaved.clear();
  sample_rate = impl_->out_rate;
  channels = impl_->out_ch;
  return false;  // video-driven loop for Phase 3; PCM path wired for Phase 4
}

}  // namespace cath
