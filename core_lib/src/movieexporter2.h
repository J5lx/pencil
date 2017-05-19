#ifndef MOVIEEXPORTER2_H
#define MOVIEEXPORTER2_H

#include <functional>
#include <QString>
#include <QSize>
#include <QTemporaryDir>
#include "pencilerror.h"
#include "layercamera.h"
extern "C" {
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
    Status checkInputParameters();
    void paintAvFrame( AVFrame *avFrame, int frameNumber );
    int convertPixFmt( AVFrame *dst, AVFrame *src );
    int encodeFrame( AVFrame *frame );

    bool mCanceled = false;
    Object *mObj;
    ExportMovieDesc2 mDesc;
    LayerCamera *mCameraLayer;
    FILE *mF;
    AVCodecContext *mCodecCtx;
    AVPacket *mPkt;
};

#endif // MOVIEEXPORTER2_H
