#include "MediaFrames.h"
#include <QCache>
#include <QMutex>
#include <QMutexLocker>
#include <QFileInfo>
#include <QDateTime>
extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
}
namespace {
struct Decoder {
    AVFormatContext *format=nullptr;AVCodecContext *codec=nullptr;AVFrame *decoded=nullptr;AVPacket *packet=nullptr;SwsContext *scale=nullptr;
    int stream=-1;qint64 timestamp=-1,requested=-1;QImage image;bool draining=false;QString error;
    explicit Decoder(const QString &file){
        int result=avformat_open_input(&format,file.toUtf8().constData(),nullptr,nullptr);
        if(result>=0)result=avformat_find_stream_info(format,nullptr);
        const AVCodec *decoder=nullptr;if(result>=0)result=av_find_best_stream(format,AVMEDIA_TYPE_VIDEO,-1,-1,&decoder,0);
        if(result>=0){stream=result;codec=avcodec_alloc_context3(decoder);if(!codec){error="Cannot allocate the video decoder.";return;}avcodec_parameters_to_context(codec,format->streams[stream]->codecpar);codec->thread_count=2;result=avcodec_open2(codec,decoder,nullptr);}
        if(result<0){char message[AV_ERROR_MAX_STRING_SIZE];av_strerror(result,message,sizeof(message));error=QString::fromUtf8(message);return;}
        decoded=av_frame_alloc();packet=av_packet_alloc();
    }
    ~Decoder(){sws_freeContext(scale);av_packet_free(&packet);av_frame_free(&decoded);avcodec_free_context(&codec);avformat_close_input(&format);}
    qint64 duration()const{return format&&format->duration>0?format->duration/1000:0;}
    QImage at(qint64 ms){
        if(!error.isEmpty()||!decoded||!packet)return {};ms=qMax<qint64>(0,ms);if(requested==ms)return image;
        const auto timebase=format->streams[stream]->time_base;const auto start=format->streams[stream]->start_time==AV_NOPTS_VALUE?0:format->streams[stream]->start_time;
        if(ms<timestamp||ms>timestamp+1500){if(av_seek_frame(format,stream,start+av_rescale_q(ms,AVRational{1,1000},timebase),AVSEEK_FLAG_BACKWARD)>=0){avcodec_flush_buffers(codec);draining=false;}}
        requested=ms;
        for(int guard=0;guard<20000;++guard){int result=avcodec_receive_frame(codec,decoded);
            if(result==0){auto pts=decoded->best_effort_timestamp;timestamp=pts==AV_NOPTS_VALUE?ms:av_rescale_q(pts-start,timebase,AVRational{1,1000});
                scale=sws_getCachedContext(scale,decoded->width,decoded->height,AVPixelFormat(decoded->format),decoded->width,decoded->height,AV_PIX_FMT_RGBA,SWS_BILINEAR,nullptr,nullptr,nullptr);
                if(!scale)return {};const auto *coefficients=sws_getCoefficients(decoded->colorspace==AVCOL_SPC_BT709?SWS_CS_ITU709:decoded->colorspace==AVCOL_SPC_BT2020_NCL?SWS_CS_BT2020:SWS_CS_DEFAULT);sws_setColorspaceDetails(scale,coefficients,decoded->color_range==AVCOL_RANGE_JPEG,coefficients,1,0,1<<16,1<<16);
                image=QImage(decoded->width,decoded->height,QImage::Format_RGBA8888);if(image.isNull())return {};uint8_t *planes[]={image.bits(),nullptr,nullptr,nullptr};int strides[]={int(image.bytesPerLine()),0,0,0};sws_scale(scale,decoded->data,decoded->linesize,0,decoded->height,planes,strides);
                av_frame_unref(decoded);if(timestamp>=ms)return image;continue;
            }
            if(result==AVERROR_EOF||draining)return image;
            if(result!=AVERROR(EAGAIN))return image;
            while((result=av_read_frame(format,packet))>=0){if(packet->stream_index==stream){result=avcodec_send_packet(codec,packet);av_packet_unref(packet);break;}av_packet_unref(packet);}
            if(result<0){avcodec_send_packet(codec,nullptr);draining=true;}
        }return image;
    }
};
QCache<QString,Decoder> decoders(12);QMutex mutex;
}
QImage MediaFrames::frame(const QString &file,qint64 milliseconds,const QString &instance){QMutexLocker lock(&mutex);const QFileInfo media(file);const auto key=instance+"|"+media.absoluteFilePath()+QString("|%1|%2|%3").arg(media.exists()).arg(media.size()).arg(media.lastModified().toMSecsSinceEpoch());auto *decoder=decoders.object(key);if(!decoder){decoder=new Decoder(file);decoders.insert(key,decoder);}return decoder->at(milliseconds);}
qint64 MediaFrames::duration(const QString &file,QString *error){Decoder decoder(file);if(error)*error=decoder.error;return decoder.error.isEmpty()?decoder.duration():0;}
