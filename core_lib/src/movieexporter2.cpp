#include "movieexporter2.h"

#include <QDebug>
extern "C" {
#   include <libavformat/avformat.h>
#   include <libavcodec/avcodec.h>
#   include <libswscale/swscale.h>
#   include <libavutil/avutil.h>
#   include <libavutil/opt.h>
#   include <libswresample/swresample.h>
}
#include "object.h"
#include "layersound.h"

using std::runtime_error;

#define ALLOC_CHECK(ptr, msg) do { \
    if ( !( ptr ) ) { \
        throw runtime_error( msg ); \
    } \
} while ( 0 )
#define AVERR_CHECK(ret, msg) do { \
    int _ret = ( ret ); \
    if ( _ret < 0 ) { \
        /* TODO: this works, but it doesn’t feel quite right */ \
        throw runtime_error( QString( "%1: %2" ).arg( QString::fromUtf8( msg ) ).arg( av_err2str( _ret ) ).toStdString() ); \
    } \
} while ( 0 )

// Pixel formats of QImage rely on endianness, those of lavutil don’t
#if Q_BYTE_ORDER == Q_LITTLE_ENDIAN
#define PIX_FMT_QIMAGE_ARGB32 AV_PIX_FMT_BGRA
#else
#define PIX_FMT_QIMAGE_ARGB32 AV_PIX_FMT_ARGB
#endif // Q_BYTE_ORDER == Q_LITTLE_ENDIAN

struct InputAudioStream {
    SoundClip       *soundClip;
    AVFormatContext *formatContext;
    int              streamIndex;
    AVCodecContext  *codecContext;
    AVFrame         *recvFrame;
    SwrContext      *swrContext;
};

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
    // FIXME: This is not currently reflected in the UI
    Q_ASSERT( mDesc.exportSize.width() % 2 == 0 );
    Q_ASSERT( mDesc.exportSize.height() % 2 == 0 );

    mCameraLayer = static_cast< LayerCamera* >( mObj->findLayerByName( mDesc.strCameraName, Layer::CAMERA ) );
    if ( mCameraLayer == nullptr )
    {
        mCameraLayer = mObj->getLayersByType< LayerCamera >().front();
    }
    std::vector< SoundClip* > soundClips;
    for ( LayerSound* layer : mObj->getLayersByType< LayerSound >() )
    {
        layer->foreachKeyFrame( [&soundClips]( KeyFrame* key )
        {
            soundClips.push_back( static_cast< SoundClip* >( key ) );
        } );
    }

    // Qt Multimedia can use GStreamer which insists on screwing up lav logging
    av_log_set_callback( &av_log_default_callback );

    createFormatContext( mDesc.strFileName.toLocal8Bit().data() );

    createVideoCodecContext( AV_CODEC_ID_H264 );
    mVideoStream = createStream( mVideoCodecCtx );
    mArgbFrame = createVideoFrame( PIX_FMT_QIMAGE_ARGB32 );
    mYuv420pFrame = createVideoFrame( AV_PIX_FMT_YUV420P );

    if ( !soundClips.empty() )
    {
        createAudioCodecContext( AV_CODEC_ID_AAC );
        mAudioStream = createStream( mAudioCodecCtx );
        mAudioFrame = createAudioFrame();

        for ( SoundClip *soundClip : soundClips )
        {
            addInputAudioStream( soundClip );
        }
    }

    av_dump_format( mFormatCtx, 0, mDesc.strFileName.toLocal8Bit().data(), 1);

    createPacket();
}

MovieExporter2::~MovieExporter2()
{
    destroyFormatContext();
    destroyCodecContext( mVideoCodecCtx );
    destroyCodecContext( mAudioCodecCtx );
    clearInputAudioStreams();
    destroyFrame( mArgbFrame );
    destroyFrame( mYuv420pFrame );
    destroyFrame( mAudioFrame );
    destroyPacket();
}

Status MovieExporter2::run()
{
    emit progress( 0 );

    AVERR_CHECK( avformat_write_header( mFormatCtx, nullptr), "Unable to write format header" );

    emit progress( 100 / ( mDesc.endFrame - mDesc.startFrame + 3 ) );

    for ( int currentFrame = mDesc.startFrame; currentFrame <= mDesc.endFrame; currentFrame++ )
    {
        if ( mCanceled )
        {
            return Status::CANCELED;
        }

        writeVideoFrame( mArgbFrame, currentFrame );
        convertPixFmt( mYuv420pFrame, mArgbFrame );

        mYuv420pFrame->pts = currentFrame - mDesc.startFrame;

        encodeFrame( mVideoStream, mVideoCodecCtx, mYuv420pFrame );

        if ( !mInputAudioStreams.empty() )
        {
            writeAudioFrame( mAudioFrame, currentFrame );

            mAudioFrame->pts = currentFrame - mDesc.startFrame;

            encodeFrame( mAudioStream, mAudioCodecCtx, mAudioFrame );
        }

        emit progress( 100 * ( 2 + currentFrame   - mDesc.startFrame ) /
                             ( 3 + mDesc.endFrame - mDesc.startFrame ) );
    }

    encodeFrame( mVideoStream, mVideoCodecCtx, nullptr );
    if ( !mInputAudioStreams.empty() )
    {
        encodeFrame( mAudioStream, mAudioCodecCtx, nullptr );
    }
    AVERR_CHECK( av_write_trailer( mFormatCtx ), "Unable to write format trailer" );

    emit progress( 100 );

    return Status::OK;
}

void MovieExporter2::addInputAudioStream(SoundClip *soundClip)
{
    qDebug() << "Opening sound clip" << soundClip->fileName();

    AVFormatContext *fmtCtx = nullptr;
    AVERR_CHECK( avformat_open_input( &fmtCtx, soundClip->fileName().toLocal8Bit().data(), nullptr, nullptr ),
                 "Unable to open sound clip" );
    AVERR_CHECK( avformat_find_stream_info( fmtCtx, nullptr ),
                 "Unable to find stream info of sound clip" );
    av_dump_format( fmtCtx, mInputAudioStreams.size(), soundClip->fileName().toLocal8Bit().data(), 0 );

    AVCodec *codec = nullptr;
    int streamIndex = av_find_best_stream( fmtCtx, AVMEDIA_TYPE_AUDIO, -1, -1, &codec, 0 );
    AVERR_CHECK( streamIndex, "Unable to find best audio stream in sound clip" );

    AVCodecContext *codecCtx;
    ALLOC_CHECK( codecCtx = avcodec_alloc_context3( codec ), "Unable to allocate codec context for sound clip" );
    AVERR_CHECK( avcodec_parameters_to_context( codecCtx, fmtCtx->streams[streamIndex]->codecpar ),
                 "Unable to copy codec parameters of sound clip to codec context" );
    AVERR_CHECK( avcodec_open2( codecCtx, codec, nullptr), "Unable to open decoder for sound clip" );

    AVFrame *recvFrame;
    ALLOC_CHECK( recvFrame = av_frame_alloc(), "Unable to allocate receive frame for sound clip" );

    SwrContext *swrCtx = swr_alloc_set_opts( nullptr,
                                          mAudioCodecCtx->channel_layout,
                                          mAudioCodecCtx->sample_fmt,
                                          mAudioCodecCtx->sample_rate,
                                          codecCtx->channel_layout,
                                          codecCtx->sample_fmt,
                                          codecCtx->sample_rate,
                                          0,
                                          nullptr );
    ALLOC_CHECK( swrCtx, "Unable to allocate resampler context for sound clip");
    AVERR_CHECK( swr_init( swrCtx ), "Unable to initialize resampler context for sound clip" );

    mInputAudioStreams.push_back( {
                                      soundClip,
                                      fmtCtx,
                                      streamIndex,
                                      codecCtx,
                                      recvFrame,
                                      swrCtx
                                  } );
}

void MovieExporter2::clearInputAudioStreams()
{
    for ( InputAudioStream stream : mInputAudioStreams )
    {
        avformat_close_input( &stream.formatContext );
        avcodec_free_context( &stream.codecContext );
        av_frame_free( &stream.recvFrame );
        swr_free( &stream.swrContext );
    }
    mInputAudioStreams.clear();
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
    // We have to use a temporary QImage to obtain the pixel aspect ratio used by Qt
    // The only alternative is using private, unsupported functions
    QImage tmp( 1, 1, QImage::Format_ARGB32_Premultiplied );
    mVideoCodecCtx->sample_aspect_ratio = { tmp.dotsPerMeterX(), tmp.dotsPerMeterY() };
    mVideoCodecCtx->time_base = { 1, mDesc.fps };
    mVideoCodecCtx->pix_fmt = AV_PIX_FMT_YUV420P;
    av_opt_set( mVideoCodecCtx->priv_data, "preset", "slow", 0 ); // NOTE: H.264-specific

    AVERR_CHECK( avcodec_open2( mVideoCodecCtx, mVideoCodecCtx->codec, nullptr ), "Unable to open codec" );
}

void MovieExporter2::createAudioCodecContext(AVCodecID codecId)
{
    mAudioCodecCtx = createCodecContext( codecId );

    mAudioCodecCtx->bit_rate = 64000;
    mAudioCodecCtx->sample_fmt = AV_SAMPLE_FMT_FLTP; // TODO: proper selection / check support in codec
    mAudioCodecCtx->sample_rate = 44100; // TODO: proper selection / check support in codec
    mAudioCodecCtx->channel_layout = AV_CH_LAYOUT_STEREO; // TODO: proper selection / check support in codec
    mAudioCodecCtx->channels = av_get_channel_layout_nb_channels( mAudioCodecCtx->channel_layout );

    AVERR_CHECK( avcodec_open2( mAudioCodecCtx, mAudioCodecCtx->codec, nullptr ), "Unable to open codec" );
}

void MovieExporter2::destroyCodecContext(AVCodecContext *&codecContext)
{
    // FIXME: can produce SIGSEGV
    avcodec_free_context( &codecContext );
}

AVFrame *MovieExporter2::createVideoFrame(int format)
{
    AVFrame *frame = av_frame_alloc();
    ALLOC_CHECK( frame, "Unable to allocate frame" );
    frame->format = format;
    frame->width = mVideoCodecCtx->width;
    frame->height = mVideoCodecCtx->height;
    AVERR_CHECK( av_frame_get_buffer( frame, 32 ), "Unable to allocate frame data" );
    return frame;
}

AVFrame *MovieExporter2::createAudioFrame()
{
    AVFrame *frame = av_frame_alloc();
    ALLOC_CHECK( frame, "Unable to allocate audio frame" );
    frame->nb_samples = mAudioCodecCtx->frame_size;
    frame->format = mAudioCodecCtx->sample_fmt;
    frame->channel_layout = mAudioCodecCtx->channel_layout;
    frame->sample_rate = mAudioCodecCtx->sample_rate;
    AVERR_CHECK( av_frame_get_buffer( frame, 0 ), "Unable to allocate audio frame data" );

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

void MovieExporter2::readAudioFrame(InputAudioStream &stream, AVPacket *&pkt)
{
    while ( true )
    {
        AVERR_CHECK( av_read_frame(stream.formatContext, pkt), "Unable to read frame from sound clip" );
        if ( pkt->stream_index == stream.streamIndex )
        {
            break;
        }
        av_packet_unref( pkt );
    }
}

void MovieExporter2::writeVideoFrame(AVFrame *avFrame, int frameNumber)
{
    AVERR_CHECK( av_frame_make_writable( avFrame ), "Unable to make video frame writable" );

    QImage imageToExport( mDesc.exportSize, QImage::Format_ARGB32_Premultiplied );
    imageToExport.fill( Qt::white );

    QPainter painter( &imageToExport );

    QTransform view = mCameraLayer->getViewAtFrame( frameNumber );

    QSize camSize = mCameraLayer->getViewSize();
    QTransform centralizeCamera;
    centralizeCamera.translate( camSize.width() / 2, camSize.height() / 2 );

    painter.setWorldTransform( view * centralizeCamera );
    painter.setWindow( QRect( 0, 0, camSize.width(), camSize.height() ) );

    mObj->paintImage( painter, frameNumber, false, true );

    Q_ASSERT( avFrame->format == PIX_FMT_QIMAGE_ARGB32 );
    // ARGB / BGRA pixel formats store all components on plane 0
    Q_ASSERT( avFrame->height * avFrame->linesize[0] == imageToExport.byteCount() );
    memcpy( avFrame->data[0], imageToExport.constBits(), avFrame->height * avFrame->linesize[0] );
}

// TODO: probably split this method up cause it’s too big
// FIXME: We really need to support sample rate conversions (i.e. flush libswr internal buffer when necessary)
void MovieExporter2::writeAudioFrame(AVFrame *avFrame, int frameNumber)
{
    // TODO: support for more than one sound clip
    InputAudioStream stream = mInputAudioStreams.front();

    // Just return if there is no source audio available for the current frame
    // FIXME: Using the length from Pencil’s sound clip could cause a number of problems. Eventually we should obtain
    //        only the position from Pencil and rely on lavf/lavc for everything else
    // FIXME: We should at least write some silence to the AVFrame we got
    if ( stream.soundClip->pos() > frameNumber || stream.soundClip->pos() + stream.soundClip->length() < frameNumber )
    {
        return;
    }

    readAudioFrame( stream, mPkt );

    AVERR_CHECK( avcodec_send_packet( stream.codecContext, mPkt ), "Unable to send packet from sound clip to decoder" );
    av_packet_unref( mPkt );

    while ( true )
    {
        // FIXME: currently processing only some frames
        int ret = avcodec_receive_frame( stream.codecContext, stream.recvFrame );
        if ( ret == AVERROR( EAGAIN ) || ret == AVERROR_EOF )
        {
            break;
        }
        AVERR_CHECK( ret, "Unable to receive frame from sound clip decoder" );

        AVERR_CHECK( av_frame_make_writable( avFrame ), "Unable to make audio frame writable" );
        AVERR_CHECK( swr_convert_frame( stream.swrContext, avFrame, stream.recvFrame ), "Unable to resample audio frame" );
    }
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
                 "Unable to send frame " /*+ std::to_string(frame->pts) +*/ " for encoding" );
                 // FIXME: We are not using Java

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
