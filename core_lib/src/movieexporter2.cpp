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

// TODO: Right now we are using gotos to ensure proper cleanup which is the C way (as libav* have C APIs).
//       Eventually we should probably use something more idiomatic instead.
#define ENDMSG(s, m) do { \
    status = ( s ); \
    if ( m ) qDebug() << ( m ); \
    goto end; \
} while ( 0 )
#define END(s) ENDMSG( ( s ), 0 )
#define ALLOC_CHECK(p, m) do { \
    if ( !( p ) ) { \
        ENDMSG( Status::FAIL, ( m ) ); \
    } \
} while ( 0 )
#define FFRET_CHECK(r, m) do { \
    if ( ( r ) < 0 ) { \
        ENDMSG( Status::FAIL, ( m ) ); \
    } \
} while ( 0 )

// Pixel formats of QImage depend on endianness, those of lavutil don’t
#if Q_BYTE_ORDER == Q_LITTLE_ENDIAN
#define QIMAGE_ARGB32_PIX_FMT AV_PIX_FMT_BGRA
#else
#define QIMAGE_ARGB32_PIX_FMT AV_PIX_FMT_ARGB
#endif // Q_BYTE_ORDER == Q_LITTLE_ENDIAN

MovieExporter2::MovieExporter2(Object* obj, ExportMovieDesc2& desc) :
    mObj(obj),
    mDesc(desc)
{
    mCameraLayer = static_cast< LayerCamera* >( mObj->findLayerByName( mDesc.strCameraName, Layer::CAMERA ) );
    if ( mCameraLayer == nullptr )
    {
        mCameraLayer = mObj->getLayersByType< LayerCamera >().front();
    }

    // Qt Multimedia can use GStreamer which insists on screwing up lav logging
    av_log_set_callback( &av_log_default_callback );
}

MovieExporter2::~MovieExporter2()
{
}

Status MovieExporter2::run()
{
    Status status = Status::OK;
    AVCodec *codec;
    AVFrame *argbFrame, *yuv420pFrame;
    uint8_t endcode[] = { 0, 0, 1, 0xb7 };

    STATUS_CHECK( checkInputParameters() );

    qDebug() << "Exporting as movie to " << mDesc.strFileName;

    emit progress( 0 );

    // TODO: strdup <> free?
    mF = fopen( strdup( mDesc.strFileName.toLocal8Bit().data() ), "wb" );
    ALLOC_CHECK( mF, "Unable to open output file" );

    // TODO: audio

    ALLOC_CHECK( codec = avcodec_find_encoder( AV_CODEC_ID_H264 ), "Unable to find codec" );

    mCodecCtx = avcodec_alloc_context3( codec );
    ALLOC_CHECK( mCodecCtx, "Unable to allocate video codec context" );

    mCodecCtx->bit_rate = 400000;
    Q_ASSERT( mDesc.exportSize.width() % 2 == 0 );
    Q_ASSERT( mDesc.exportSize.height() % 2 == 0 );
    mCodecCtx->width = mDesc.exportSize.width();
    mCodecCtx->height = mDesc.exportSize.height();
    mCodecCtx->time_base = { 1, mDesc.fps };
    mCodecCtx->framerate = { mDesc.fps, 1 };
    mCodecCtx->pix_fmt = AV_PIX_FMT_YUV420P;
    av_opt_set( mCodecCtx->priv_data, "preset", "slow", 0 ); // NOTE: H.264-specific

    FFRET_CHECK( avcodec_open2( mCodecCtx, codec, nullptr ), "Unable to open codec" );

    argbFrame = av_frame_alloc();
    ALLOC_CHECK( argbFrame, "Unable to allocate ARGB frame" );
    argbFrame->format = QIMAGE_ARGB32_PIX_FMT;
    argbFrame->width = mCodecCtx->width;
    argbFrame->height = mCodecCtx->height;
    FFRET_CHECK( av_frame_get_buffer( argbFrame, 32 ), "Unable to allocate ARGB frame data" );

    yuv420pFrame = av_frame_alloc();
    ALLOC_CHECK( yuv420pFrame, "Unable to allocate YUV420p frame" );
    yuv420pFrame->format = AV_PIX_FMT_YUV420P;
    yuv420pFrame->width = mCodecCtx->width;
    yuv420pFrame->height = mCodecCtx->height;
    FFRET_CHECK( av_frame_get_buffer( yuv420pFrame, 32 ), "Unable to allocate YUV420p frame data" );

    mPkt = av_packet_alloc();
    ALLOC_CHECK( mPkt, "Unable to allocate video packet" );

    for ( int currentFrame = mDesc.startFrame; currentFrame <= mDesc.endFrame; currentFrame++ )
    {
        if ( mCanceled )
        {
            END( Status::CANCELED );
        }

        FFRET_CHECK( av_frame_make_writable( argbFrame ), "Unable to make ARGB frame writable" );
        FFRET_CHECK( av_frame_make_writable( yuv420pFrame ), "Unable to make YUV420p frame writable" );

        paintAvFrame( argbFrame, currentFrame );

        FFRET_CHECK( convertPixFmt( yuv420pFrame, argbFrame ), "Unable to convert frame to YUV420p" );

        yuv420pFrame->pts = currentFrame - mDesc.startFrame;

        FFRET_CHECK( encodeFrame( yuv420pFrame ), "Unable to encode frame" );

        emit progress( 100 * ( currentFrame - mDesc.startFrame ) / ( mDesc.endFrame - mDesc.startFrame ) );
    }

    // TODO: format

    encodeFrame( nullptr );

    fwrite( endcode, 1, sizeof( endcode ), mF );

    emit progress( 100 );

    end:
    fclose( mF );
    avcodec_free_context( &mCodecCtx );
    av_frame_free( &argbFrame );
    av_frame_free( &yuv420pFrame );
    av_packet_free( &mPkt );
    return status;
}

Status MovieExporter2::checkInputParameters()
{
    bool b = true;
    b &= ( !mDesc.strFileName.isEmpty() );
    b &= ( mDesc.startFrame > 0 );
    b &= ( mDesc.endFrame >= mDesc.startFrame );
    b &= ( mDesc.fps > 0 );
    b &= ( !mDesc.strCameraName.isEmpty() );

    return b ? Status::OK : Status::INVALID_ARGUMENT;
}

void MovieExporter2::paintAvFrame(AVFrame *avFrame, int frame)
{
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

    Q_ASSERT( avFrame->format == QIMAGE_ARGB32_PIX_FMT );
    // ARGB pixel format stores the entire data of all components on plane 0 (packed)
    Q_ASSERT( avFrame->height * avFrame->linesize[0] == imageToExport.byteCount() );
    memcpy( avFrame->data[0], imageToExport.constBits(), avFrame->height * avFrame->linesize[0] );
}

int MovieExporter2::convertPixFmt(AVFrame *dst, AVFrame *src)
{
    SwsContext *sws;

    sws = sws_getContext( src->width,
                          src->height,
                          static_cast< AVPixelFormat >( src->format ),
                          dst->width,
                          dst->height,
                          static_cast< AVPixelFormat >( dst->format ),
                          0,
                          NULL,
                          NULL,
                          NULL );
    if ( !sws )
    {
        qDebug() << "Unable to allocate swscaler context";
        return -1;
    }

    sws_scale( sws, src->data, src->linesize, 0, src->height, dst->data, dst->linesize );

    sws_freeContext( sws );

    return 0;
}

int MovieExporter2::encodeFrame(AVFrame *frame)
{
    if ( avcodec_send_frame( mCodecCtx, frame ) < 0 )
    {
        qDebug() << "Unable to send frame" << frame->pts << "for encoding";
        return -1;
    }

    int ret = 0;
    while ( true )
    {
        ret = avcodec_receive_packet( mCodecCtx, mPkt );
        if ( ret == AVERROR( EAGAIN ) || ret == AVERROR_EOF )
        {
            return 0;
        }
        else if ( ret < 0 )
        {
            return -1;
        }

        fwrite( mPkt->data, 1, mPkt->size, mF );
        if ( ferror( mF ) )
        {
            qDebug() << "Error while writing output";
            return -1;
        }
        av_packet_unref( mPkt );
    }
    Q_ASSERT( false );
    return -1;
}
