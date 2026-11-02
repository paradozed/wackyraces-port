#include "movie_decoder.h"
#include <algorithm>
#include <stdexcept>

#if WPC_HAVE_LIBAVCODEC
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/frame.h>
#include <libavutil/pixfmt.h>
}
#endif

namespace wpc {
namespace {
uint8_t clamp8(int v) {return uint8_t(std::clamp(v,0,255));}
}
void yuv410_to_rgba(const uint8_t* yp,int ys,const uint8_t* up,int us,const uint8_t* vp,int vs,int width,int height,uint8_t* out) {
    // Fitted to ir50_32's own 24-bit output (VfW AVIStreamGetFrame under
    // Wine, intro.avi frames 300 and 800; mean error 0.9/1.4 against 3.1/3.6
    // for replicated chroma): each chroma sample sits at its 4x4 block's
    // centre and is interpolated bilinearly (clamped at the edges), then
    // BT.601 studio range with offsets of -2.0, -3.3 and -1.7 on R, G, B.
    // Not the codec's recovered routine.
    const int cw=(width+3)/4,ch=(height+3)/4;
    const auto at=[](int i,int n){return std::clamp(i,0,n-1);};
    for(int row=0;row<height;++row) {
        const uint8_t* y=yp+size_t(row)*ys;
        // (row - 1.5) / 4 in eighths.
        const int ty=2*row-3,ry=ty>=0?ty/8:-1,fy=ty-8*ry;
        const uint8_t* u0=up+size_t(at(ry,ch))*us;const uint8_t* u1=up+size_t(at(ry+1,ch))*us;
        const uint8_t* v0=vp+size_t(at(ry,ch))*vs;const uint8_t* v1=vp+size_t(at(ry+1,ch))*vs;
        uint8_t* p=out+size_t(row)*width*4;
        for(int x=0;x<width;++x) {
            const int tx=2*x-3,rx=tx>=0?tx/8:-1,fx=tx-8*rx;
            const int a=at(rx,cw),b=at(rx+1,cw);
            const auto mix=[&](const uint8_t* r0,const uint8_t* r1) {
                return ((r0[a]*(8-fx)+r0[b]*fx)*(8-fy)+(r1[a]*(8-fx)+r1[b]*fx)*fy+32)>>6;
            };
            // Fixed-point BT.601: 1.164, 1.596, 0.391, 0.813, 2.018 (x1024).
            const int c=(y[x]-16)*1192,d=mix(u0,u1)-128,e=mix(v0,v1)-128;
            p[0]=clamp8((c+1634*e-2048+512)>>10);
            p[1]=clamp8((c-401*d-833*e-3379+512)>>10);
            p[2]=clamp8((c+2066*d-1741+512)>>10);
            p[3]=255;p+=4;
        }
    }
}
#if WPC_HAVE_LIBAVCODEC
namespace {
class Indeo5Decoder final:public VideoDecoder {
public:
    explicit Indeo5Decoder(const AviStream& stream) {
        const AVCodec* codec=avcodec_find_decoder(AV_CODEC_ID_INDEO5);
        if(!codec) throw std::runtime_error("libavcodec has no Indeo 5 decoder");
        context_=avcodec_alloc_context3(codec);
        frame_=av_frame_alloc();packet_=av_packet_alloc();
        if(!context_ || !frame_ || !packet_) throw std::runtime_error("libavcodec allocation failed");
        context_->width=width_=stream.width;context_->height=height_=std::abs(stream.height);
        context_->codec_tag=stream.compression;
        if(avcodec_open2(context_,codec,nullptr)<0) throw std::runtime_error("cannot open the Indeo 5 decoder");
        rgba_.assign(size_t(width_)*height_*4,0);
        for(size_t i=3;i<rgba_.size();i+=4) rgba_[i]=255;
    }
    ~Indeo5Decoder() override {
        av_packet_free(&packet_);av_frame_free(&frame_);avcodec_free_context(&context_);
    }
    void decode(const uint8_t* data,size_t size) override {
        if(!size) return;
        if(size>size_t(INT32_MAX)) throw std::runtime_error("oversized movie frame");
        // libavcodec reads ahead; give it a padded private copy.
        padded_.assign(data,data+size);padded_.resize(size+AV_INPUT_BUFFER_PADDING_SIZE,0);
        packet_->data=padded_.data();packet_->size=int(size);
        if(avcodec_send_packet(context_,packet_)<0) return; // corrupt frame: keep the last picture
        while(avcodec_receive_frame(context_,frame_)==0) {
            if(frame_->format!=AV_PIX_FMT_YUV410P || frame_->width!=width_ || frame_->height!=height_)
                throw std::runtime_error("unexpected Indeo 5 picture format");
            yuv410_to_rgba(frame_->data[0],frame_->linesize[0],frame_->data[1],frame_->linesize[1],
                           frame_->data[2],frame_->linesize[2],width_,height_,rgba_.data());
            av_frame_unref(frame_);
        }
    }
private:
    AVCodecContext* context_=nullptr;
    AVFrame* frame_=nullptr;
    AVPacket* packet_=nullptr;
    std::vector<uint8_t> padded_;
};
}
#endif
std::unique_ptr<VideoDecoder> create_video_decoder(const AviStream& stream,std::string& reason) {
    const uint32_t iv50=uint32_t('I')|uint32_t('V')<<8|uint32_t('5')<<16|uint32_t('0')<<24;
    if(!stream.video() || stream.compression!=iv50) {reason="unsupported video compression";return nullptr;}
#if WPC_HAVE_LIBAVCODEC
    try {return std::make_unique<Indeo5Decoder>(stream);}
    catch(const std::exception& e) {reason=e.what();return nullptr;}
#else
    reason="built without libavcodec (Indeo 5 decoder unavailable)";
    return nullptr;
#endif
}
}
