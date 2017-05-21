#include "movieexporter2.h"

#include <QDebug>
extern "C" {
#   include <libavformat/avformat.h>
#   include <libavcodec/avcodec.h>
#   include <libswscale/swscale.h>
#   include <libavutil/avutil.h>
#   include <libavutil/opt.h>
}
#include "object.h"
#include "layersound.h"
#include "soundclip.h"

using std::runtime_error;

#define ALLOC_CHECK(ptr, msg) do { \
    if ( !( ptr ) ) { \
        throw runtime_error( msg ); \
    } \
} while ( 0 )
#define AVERR_CHECK(ret, msg) do { \
    if ( ( ret ) < 0 ) { \
        throw runtime_error( msg ); \
    } \
} while ( 0 )

// Pixel formats of QImage rely on endianness, those of lavutil don’t
#if Q_BYTE_ORDER == Q_LITTLE_ENDIAN
#define PIX_FMT_QIMAGE_ARGB32 AV_PIX_FMT_BGRA
#else
#define PIX_FMT_QIMAGE_ARGB32 AV_PIX_FMT_ARGB
#endif // Q_BYTE_ORDER == Q_LITTLE_ENDIAN

MovieExporter2::MovieExporter2(Object* obj, ExportMovieDesc2& desc) :
    mObj(obj),
    mDesc(desc)
{
    // Proconditions, the UI should prevent invalid values
    Q_ASSERT( !mDesc.strFileName.isEmpty() );
    Q_ASSERT( mDesc.startFrame > 0 );
    Q_ASSERT( mDesc.endFrame >= mDesc.startFrame );
    Q_ASSERT( mDesc.fps > 0 );
    Q_ASSERT( !mDesc.strCameraName.isEmpty() );
    // TODO: This is not currently reflected in the UI
    Q_ASSERT( mDesc.exportSize.width() % 2 == 0 );
    Q_ASSERT( mDesc.exportSize.height() % 2 == 0 );

    mCameraLayer = static_cast< LayerCamera* >( mObj->findLayerByName( mDesc.strCameraName, Layer::CAMERA ) );
    if ( mCameraLayer == nullptr )
    {
        mCameraLayer = mObj->getLayersByType< LayerCamera >().front();
    }
    createVideoCodecContext( AV_CODEC_ID_H264 );
    createFormatContext( mDesc.strFileName.toLocal8Bit().data() );
    mVideoStream = createStream( mVideoCodecCtx );

    mArgbFrame = createFrame( PIX_FMT_QIMAGE_ARGB32 );
    mYuv420pFrame = createFrame( AV_PIX_FMT_YUV420P );

    createPacket();

    // Qt Multimedia can use GStreamer which insists on screwing up lav logging
    av_log_set_callback( &av_log_default_callback );
}

MovieExporter2::~MovieExporter2()
{
    destroyFormatContext();
    destroyCodecContext( mVideoCodecCtx );
    destroyFrame( mArgbFrame );
    destroyFrame( mYuv420pFrame );
    destroyPacket();
}

Status MovieExporter2::run()
{
    AVERR_CHECK( avformat_write_header( mFormatCtx, nullptr), "Unable to write format header" );

    emit progress( 0 );

    for ( int currentFrame = mDesc.startFrame; currentFrame <= mDesc.endFrame; currentFrame++ )
    {
        if ( mCanceled )
        {
            return Status::CANCELED;
        }

        paintAvFrame( mArgbFrame, currentFrame );
        convertPixFmt( mYuv420pFrame, mArgbFrame );

        mYuv420pFrame->pts = currentFrame - mDesc.startFrame;

        encodeFrame( mVideoStream, mVideoCodecCtx, mYuv420pFrame );

        emit progress( 100 * ( currentFrame - mDesc.startFrame ) / ( mDesc.endFrame - mDesc.startFrame ) );
    }

    encodeFrame( mVideoStream, mVideoCodecCtx, nullptr );
    AVERR_CHECK( av_write_trailer( mFormatCtx ), "Unable to write format trailer" );

    return Status::OK;
}

void MovieExporter2::createFormatContext(const char *filename)
{
    AVERR_CHECK( avformat_alloc_output_context2( &mFormatCtx, NULL, "mp4", filename ),
                 "Unable to allocate format context" );

    if ( !( mFormatCtx->oformat->flags & AVFMT_NOFILE ) )
    {
        AVERR_CHECK( avio_open( &mFormatCtx->pb, filename, AVIO_FLAG_WRITE ),
                     "Unable to open output file" );
    }
}

void MovieExporter2::destroyFormatContext()
{
    if ( !mFormatCtx )
    {
        return;
    }


    if ( !( mFormatCtx->oformat->flags & AVFMT_NOFILE ) )
    {
        avio_close( mFormatCtx->pb );
    }
    avformat_free_context( mFormatCtx );
}

AVStream *MovieExporter2::createStream(AVCodecContext *codecContext)
{
    AVStream *stream = avformat_new_stream( mFormatCtx, nullptr );
    ALLOC_CHECK( stream, "Unable to create stream" );
    stream->id = mFormatCtx->nb_streams - 1;

    if ( mFormatCtx->oformat->flags & AVFMT_GLOBALHEADER )
    {
        codecContext->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }

    AVERR_CHECK( avcodec_parameters_from_context( stream->codecpar, codecContext ),
                 "Unable to copy codec parameters to stream" );
    return stream;
}

AVCodecContext *MovieExporter2::createCodecContext(AVCodecID codecId)
{
    AVCodec *codec = avcodec_find_encoder( codecId );
    ALLOC_CHECK( codec, "Unable to find codec" );

    AVCodecContext *codecCtx = avcodec_alloc_context3( codec );
    ALLOC_CHECK( codecCtx, "Unable to allocate codec context" );

    return codecCtx;
}

void MovieExporter2::createVideoCodecContext(AVCodecID codecId)
{
    mVideoCodecCtx = createCodecContext( codecId );

    mVideoCodecCtx->bit_rate = 400000;
    mVideoCodecCtx->width = mDesc.exportSize.width();
    mVideoCodecCtx->height = mDesc.exportSize.height();
    mVideoCodecCtx->time_base = { 1, mDesc.fps };
    mVideoCodecCtx->pix_fmt = AV_PIX_FMT_YUV420P;
    av_opt_set( mVideoCodecCtx->priv_data, "preset", "slow", 0 ); // NOTE: H.264-specific

    AVERR_CHECK( avcodec_open2( mVideoCodecCtx, mVideoCodecCtx->codec, nullptr ), "Unable to open codec" );
}

void MovieExporter2::destroyCodecContext(AVCodecContext *&codecContext)
{
    avcodec_free_context( &codecContext );
}

AVFrame *MovieExporter2::createFrame(int format)
{
    AVFrame *frame = av_frame_alloc();
    ALLOC_CHECK( frame, "Unable to allocate frame" );
    frame->format = format;
    frame->width = mVideoCodecCtx->width;
    frame->height = mVideoCodecCtx->height;
    AVERR_CHECK( av_frame_get_buffer( frame, 32 ), "Unable to allocate frame data" );
    return frame;
}

void MovieExporter2::destroyFrame(AVFrame *&frame )
{
    av_frame_free( &frame );
}

void MovieExporter2::createPacket()
{
    mPkt = av_packet_alloc();
    ALLOC_CHECK( mPkt, "Unable to allocate video packet" );
}

void MovieExporter2::destroyPacket()
{
    av_packet_free( &mPkt );
}

void MovieExporter2::paintAvFrame(AVFrame *avFrame, int frame)
{
    AVERR_CHECK( av_frame_make_writable( avFrame ), "Unable to make frame writable" );

    QImage imageToExport( mDesc.exportSize, QImage::Format_ARGB32_Premultiplied );
    imageToExport.fill( Qt::white );

    QPainter painter( &imageToExport );

    QTransform view = mCameraLayer->getViewAtFrame( frame );

    QSize camSize = mCameraLayer->getViewSize();
    QTransform centralizeCamera;
    centralizeCamera.translate( camSize.width() / 2, camSize.height() / 2 );

    painter.setWorldTransform( view * centralizeCamera );
    painter.setWindow( QRect( 0, 0, camSize.width(), camSize.height() ) );

    mObj->paintImage( painter, frame, false, true );

    Q_ASSERT( avFrame->format == PIX_FMT_QIMAGE_ARGB32 );
    // ARGB pixel format stores all components on plane 0
    Q_ASSERT( avFrame->height * avFrame->linesize[0] == imageToExport.byteCount() );
    memcpy( avFrame->data[0], imageToExport.constBits(), avFrame->height * avFrame->linesize[0] );
}

void MovieExporter2::convertPixFmt(AVFrame *dst, AVFrame *src)
{
    AVERR_CHECK( av_frame_make_writable( dst ), "Unable to make frame writable" );

    SwsContext *sws = sws_getContext( src->width,
                                      src->height,
                                      static_cast< AVPixelFormat >( src->format ),
                                      dst->width,
                                      dst->height,
                                      static_cast< AVPixelFormat >( dst->format ),
                                      0,
                                      NULL,
                                      NULL,
                                      NULL );
    ALLOC_CHECK( sws, "Unable to allocate swscaler context" );

    sws_scale( sws, src->data, src->linesize, 0, src->height, dst->data, dst->linesize );

    sws_freeContext( sws );
}

void MovieExporter2::encodeFrame(AVStream *stream, AVCodecContext *codecContext, AVFrame *frame)
{
    AVERR_CHECK( avcodec_send_frame( codecContext, frame ),
                 "Unable to send frame" + std::to_string(frame->pts) + "for encoding" );

    while ( true )
    {
        int ret = avcodec_receive_packet( codecContext, mPkt );
        if ( ret == AVERROR( EAGAIN ) || ret == AVERROR_EOF )
        {
            return;
        }
        AVERR_CHECK( ret, "Error during encoding" );

        writePacket( stream, codecContext );
        av_packet_unref( mPkt );
    }
}

void MovieExporter2::writePacket(AVStream *stream, AVCodecContext *codecContext)
{
    av_packet_rescale_ts( mPkt, codecContext->time_base , stream->time_base);
    mPkt->stream_index = stream->index;

    AVERR_CHECK( av_interleaved_write_frame( mFormatCtx, mPkt ), "Unable to write packet to file" );
}
