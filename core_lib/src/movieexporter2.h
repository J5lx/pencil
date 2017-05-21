#ifndef MOVIEEXPORTER2_H
#define MOVIEEXPORTER2_H

#include <functional>
#include <QString>
#include <QSize>
#include <QTemporaryDir>
#include "pencilerror.h"
#include "layercamera.h"
extern "C" {
#   include <libavformat/avformat.h>
#   include <libavcodec/avcodec.h>
}

class Object;

struct ExportMovieDesc2
{
    QString strFileName;
    int     startFrame = 0;
    int     endFrame   = 0;
    int     fps        = 12;
    QSize   exportSize{ 0, 0 };
    QString strCameraName;
    bool loop = false;
};

class MovieExporter2 : public QObject
{
    Q_OBJECT

public:
    MovieExporter2( Object* obj,
                    ExportMovieDesc2& desc );
    ~MovieExporter2();

    Status run();

signals:
    void progress( int progress );

public slots:
    void cancel() { mCanceled = true; }

private:
    void createFormatContext( const char *filename );
    void destroyFormatContext();
    AVStream *createStream( AVCodecContext *codecContext );
    AVCodecContext *createCodecContext( AVCodecID codecId );
    void createVideoCodecContext( AVCodecID codecId );
    void destroyCodecContext( AVCodecContext *&codecContext );
    void openCodec( AVCodecContext *codecContext, AVCodec *codec );
    AVFrame *createFrame( int format );
    void destroyFrame( AVFrame *&frame );
    void createPacket();
    void destroyPacket();
    void paintAvFrame( AVFrame *avFrame, int frameNumber );
    void convertPixFmt( AVFrame *dst, AVFrame *src );
    void encodeFrame( AVStream *stream, AVCodecContext *codecContext, AVFrame *frame );
    void writePacket( AVStream *stream, AVCodecContext *codecContext );

    bool mCanceled = false;
    Object *mObj;
    ExportMovieDesc2 mDesc;
    LayerCamera *mCameraLayer;
    AVFormatContext *mFormatCtx;
    AVStream *mVideoStream;
    AVFrame *mArgbFrame, *mYuv420pFrame;
    AVCodecContext *mVideoCodecCtx;
    AVPacket *mPkt;
};

#endif // MOVIEEXPORTER2_H
