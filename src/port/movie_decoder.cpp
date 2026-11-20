#include "movie_decoder.h"
#include <algorithm>
#include <stdexcept>
#include <vector>

#if WPC_HAVE_LIBAVCODEC
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/frame.h>
#include <libavutil/pixfmt.h>
}
#endif

namespace wpc {
// Ir50_32's MMX 24-bit converter (0x100641a0; the game asks AVIStreamGetFrameOpen
// for no format, 0x4135ad, and the codec answers 24-bit RGB). The chroma is
// interpolated horizontally and then vertically to one sample per 2x2 pixels
// (0x10064210: c0, (c0 + c1) >> 1, c1, ...; the last sample repeated). Then,
// in saturating 16-bit lanes: y = (Y - 16) x 149 (psubusb, pmullw), R = (y +
// 204 V - dR) x 2 >> 8, G = (y + dG - min(50 U + 104 V, 32767)) x 2 >> 8, B =
// (min(258 U, 65535) + y - dB) x 2 >> 8, with 4x4 dither offsets by (row % 4,
// column % 4) (read from the converter's frame under Wine). Bit-exact against
// the native codec (2026-11-18: intro.avi 300/800, outro.avi 300/900,
// legal.avi 50).
void yuv410_to_rgba(const uint8_t* yp,int ys,const uint8_t* up,int us,const uint8_t* vp,int vs,int width,int height,uint8_t* out) {
    static const uint16_t dr[4][4]={{0x668c,0x6428,0x65c0,0x64f4},{0x64f4,0x65c0,0x6428,0x668c},{0x6428,0x668c,0x64f4,0x65c0},{0x65c0,0x64f4,0x668c,0x6428}};
    static const uint16_t dg[4][4]={{0x4da8,0x4c3e,0x4d72,0x4c74},{0x4d0a,0x4cdc,0x4c70,0x4d76},{0x4c3e,0x4da8,0x4c74,0x4d72},{0x4cdc,0x4d0a,0x4d76,0x4c70}};
    static const uint16_t db[4][4]={{0x80c0,0x7fbe,0x81c2,0x7ebc},{0x81c2,0x7ebc,0x80c0,0x7fbe},{0x7fbe,0x80c0,0x7ebc,0x81c2},{0x7ebc,0x81c2,0x7fbe,0x80c0}};
    const int cw=(width+3)/4,ch=(height+3)/4;
    // A chroma plane at 2x2: horizontal midpoints, then vertical midpoints.
    const auto half=[&](const uint8_t* plane,int stride) {
        std::vector<int> h(size_t(2*cw)*size_t(ch)),v(size_t(2*cw)*size_t(2*ch));
        for(int r=0;r<ch;++r) for(int j=0;j<cw;++j) {
            const uint8_t* row=plane+size_t(r)*stride;const int a=row[j],b=row[j+1<cw?j+1:j];
            h[size_t(r)*2*cw+2*j]=a;h[size_t(r)*2*cw+2*j+1]=(a+b)>>1;
        }
        for(int r=0;r<ch;++r) for(int j=0;j<2*cw;++j) {
            const int a=h[size_t(r)*2*cw+j],b=h[size_t(r+1<ch?r+1:r)*2*cw+j];
            v[size_t(2*r)*2*cw+j]=a;v[size_t(2*r+1)*2*cw+j]=(a+b)>>1;
        }
        return v;
    };
    const std::vector<int> u2=half(up,us),v2=half(vp,vs);
    const auto sat=[](int x){return std::clamp(x,0,65535);};
    for(int row=0;row<height;++row) {
        const uint8_t* y=yp+size_t(row)*ys;uint8_t* p=out+size_t(row)*width*4;
        const int* uc=&u2[size_t(row/2)*2*cw];const int* vc=&v2[size_t(row/2)*2*cw];
        for(int x=0;x<width;++x) {
            const int l=std::max(y[x]-16,0)*149,u=uc[x/2],v=vc[x/2],r=row&3,k=x&3;
            p[0]=uint8_t(sat(2*sat(sat(l+204*v)-dr[r][k]))>>8);
            p[1]=uint8_t(sat(2*sat(sat(l+dg[r][k])-std::min(50*u+104*v,32767)))>>8);
            p[2]=uint8_t(sat(2*sat(sat(std::min(258*u,65535)+l)-db[r][k]))>>8);
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
