#ifndef WPC_MOVIE_DECODER_H
#define WPC_MOVIE_DECODER_H
#include "game_avi.h"
#include <memory>
#include <string>

namespace wpc {
// Decodes one AVI video stream frame by frame into top-down RGBA. WPC hands
// these frames to the installed VfW codec (IV50 -> ir50_32); the native port
// uses libavcodec's Indeo 5 decoder for the bitstream only.
class VideoDecoder {
public:
    virtual ~VideoDecoder()=default;
    // An empty payload is a dropped frame: the previous picture is kept.
    virtual void decode(const uint8_t* data,size_t size)=0;
    const std::vector<uint8_t>& rgba() const {return rgba_;}
    int width() const {return width_;}
    int height() const {return height_;}
protected:
    std::vector<uint8_t> rgba_;
    int width_=0,height_=0;
};
// Returns null and sets reason when the stream's codec is unavailable.
std::unique_ptr<VideoDecoder> create_video_decoder(const AviStream& stream,std::string& reason);
// YUV 4:1:0 (Indeo's YVU9 layout, one chroma sample per 4x4 pixels) to RGBA
// with BT.601 studio-range coefficients.
void yuv410_to_rgba(const uint8_t* y,int y_stride,const uint8_t* u,int u_stride,const uint8_t* v,int v_stride,
                    int width,int height,uint8_t* rgba);
}
#endif
