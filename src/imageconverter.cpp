#include "imageconverter.h"

#include <QPainter>
#include <QTextStream>
#include <QCoreApplication>
#include <QLabel>
#include <QDebug>

ImageConverter::ImageConverter(LameImage image)
{
    // set defaults
    setColorTable();
    setTransparentColor();
    setScaleFactor();
    loadImage(image);
}

void ImageConverter::loadImage(LameImage image)
{
    _image = image;
    setFrameSize(_image.width(), _image.height());
    setDynamicRange();
    recolor();
}

bool ImageConverter::setColorTable(QString key)
{
    if (!_colors.keys().contains(key))
        return false;

    _colorkey = key;
    _image.setColorTable(_colors[_colorkey]);
    return true;
}

bool ImageConverter::setScaleFactor(float scale)
{
    if (scale < 0.0)
        return false;

    _scale = scale;
    return true;
}

bool ImageConverter::setFrameSize(int w, int h)
{
    return _image.setFrameSize(w, h);
}

bool ImageConverter::setDynamicRange(int range)
{
    if (range < 0 || range > 100)
        return false;

    detectDynamicRange(range);
    return true;
}

void ImageConverter::setTransparentColor(QColor color)
{
    _transparent = color;
}

LameImage ImageConverter::originalImage()
{
    return _image;
}

LameImage ImageConverter::resultImage()
{
    LameImage resized = _image.scaleByFactor(_scale);

    _newframewidth  = ceilingMultiple(resized.frameWidth(), 8);
    _newframeheight = resized.frameHeight();
    newwidth  = ceilingMultiple(resized.width()  * _newframewidth  / resized.frameWidth(),8);
    newheight = ceilingMultiple(resized.height() * _newframeheight / resized.frameHeight(), _newframeheight);

    LameImage newimage(newwidth, newheight, LameImage::Format_RGB32);
    newimage.setFrameSize(_newframewidth, _newframeheight);
    newimage.fill(2);

    QPainter paint(&newimage);

    for (int fy = 0; fy < resized.frameCountY(); fy++)
    {
        for (int fx = 0; fx < resized.frameCountX(); fx++)
        {
            paint.drawImage(
                        fx*_newframewidth,          fy*_newframeheight,
                        resized,
                        fx*resized.frameWidth(),    fy*resized.frameHeight(),
                        resized.frameWidth(),       resized.frameHeight()
                    );
        }
    }

    paint.end();

    newimage = applyColorFilter(newimage);

    return newimage;
}


void ImageConverter::detectDynamicRange(int range)
{
    low = high = QColor(_image.pixel(0, 0)).lightness();

    for (int y = 0; y < _image.height(); y++)
    {
        for (int x = 0; x < _image.width(); x++)
        {
            QColor color(_image.pixel(x, y));
            if (color != _transparent)
            {
                int lightness = color.lightness();
                low = qMin(low, lightness);
                high = qMax(high, lightness);
            }
        }
    }

    mid = (low + high) / 2;
    lowbreak = mid - (mid - low) * range / 100;
    highbreak = mid + (high - mid) * range / 100;
}

void ImageConverter::recolor()
{
    _image = applyColorFilter(_image);
}


LameImage ImageConverter::applyColorFilter(LameImage image)
{
    LameImage newimage(image.size(), LameImage::Format_Indexed8);
    newimage.setFrameSize(image.frameWidth(), image.frameHeight());
    newimage.setColorTable(_colors[_colorkey]);

    for (int y = 0; y < image.height(); y++)
    {
        for (int x = 0; x < image.width(); x++)
        {
            QColor color = image.pixel(x, y);
            int lightness = color.lightness();

            if (color == _transparent)
            {
                newimage.setPixel(x, y, 2);
            }
            else
            {
                if (lightness >= highbreak)
                    newimage.setPixel(x, y, 1);
                else if (lightness <= lowbreak)
                    newimage.setPixel(x, y, 0);
                else
                    newimage.setPixel(x, y, 3);
            }
        }
    }

    return newimage;
}

void ImageConverter::preview()
{

    QLabel l;
    l.setPixmap(QPixmap::fromImage(originalImage()));
    l.show();

    qApp->exec();

    l.setPixmap(QPixmap::fromImage(resultImage()));
    l.show();

    qApp->exec();

}

QString ImageConverter::toSpin(QString filename)
{
    LameImage newimage = resultImage();

    QString output;
    QTextStream stream(&output);

    stream  << "' *********************************************************\n"
            << "' " << filename << "\n"
            << "' generated by img2dat " << qApp->applicationVersion() << "\n"
            << "' *********************************************************\n"
            << "PUB Addr\n"
            << "    return @gfx_data\n\n"
            << "DAT\n\n"
            << "gfx_data\n\n"
            << "word    " << newimage.frameboost() << "\n"
            << "word    " << newimage.frameWidth() << ", " << newimage.frameHeight() << "\n";

    // print data
    unsigned short word = 0;
    for (int fy = 0; fy < newimage.frameCountY(); fy++)
    {
        for (int fx = 0; fx < newimage.frameCountX(); fx++)
        {
            for (int y = 0; y < _newframeheight; y++)
            {
                stream << "word    ";
                word = 0;
                for (int x = 0; x < _newframewidth; x++)
                {
                    if (x % 8 == 0)
                    {
                        word = 0;
                    }
                    word += (newimage.pixelIndex(fx*_newframewidth+x, fy*_newframeheight+y) << ((x % 8)*2));

                    if (x % 8 == 7)
                    {
                        stream << QString("$%1").arg(word,4,16,QChar('0'));
                        if (x < _newframewidth-1)
                            stream << ",";
                    }
                }

                stream << " ' ";
                for (int x = 0; x < _newframewidth; x++)
                {
                    QString s;
                    switch (newimage.pixelIndex(fx*_newframewidth+x, fy*_newframeheight+y))
                    {
                        case 2: s = " ";
                                break;
                        case 1: s = "░";
                                break;
                        case 3: s = "▓";
                                break;
                        case 0: s = "█";
                                break;
                    }
                    stream << s;
                }

                stream << "\n";
            }
            stream << "\n";
        }
    }

    return output;
}


int ImageConverter::ceilingMultiple(int x, int multiple)
{
    if (x % multiple == 0)
        return x;
    else
        return ((x / multiple) + 1) * multiple;
}
