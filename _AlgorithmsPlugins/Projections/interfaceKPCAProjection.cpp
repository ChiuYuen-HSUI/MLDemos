#include "interfaceKPCAProjection.h"
#include "projectorKPCA.h"
#include <QDebug>
#include <QImage>
#include <QClipboard>
#include "qcontour.h"

using namespace std;

KPCAProjection::KPCAProjection()
    : widget(new QWidget()), contourLabel(0), pcaPointer(0), contourWidget(new QWidget()),
      xIndex(0), yIndex(1)
{
    params = new Ui::paramsKPCA();
    params->setupUi(widget);
    contours = new Ui::ContourWidget();
    contours->setupUi(contourWidget);
    contourWidget->layout()->setSizeConstraint( QLayout::SetFixedSize );
    contourWidget->setWindowTitle("Kernel Eigenvector Projections");

    connect(params->kernelTypeCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(ChangeOptions()));
    connect(params->contourButton, SIGNAL(clicked()), this, SLOT(ShowContours()));
    connect(contours->dimSpin, SIGNAL(valueChanged(int)), this, SLOT(DrawContours(int)));
    connect(contours->displayCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(ShowContours()));
    connect(contours->clipboardButton, SIGNAL(clicked()), this, SLOT(SaveScreenshot()));
    connect(contours->spinX1, SIGNAL(valueChanged(int)), this, SLOT(ContoursChanged()));
    connect(contours->spinX2, SIGNAL(valueChanged(int)), this, SLOT(ContoursChanged()));
}

void KPCAProjection::SaveScreenshot()
{
    const QPixmap *screenshot = contours->plotLabel->pixmap();
    if(screenshot->isNull()) return;

    QClipboard *clipboard = QApplication::clipboard();
    clipboard->setImage(screenshot->toImage());
    clipboard->setPixmap(*screenshot);
}

void KPCAProjection::ContoursChanged()
{
    contourPixmaps.clear();
    ShowContours();
}

void KPCAProjection::ShowContours()
{
    PCA *pca = dynamic_cast<PCA*> (pcaPointer);
    if(!pca) return;
    DrawContours(contours->dimSpin->value());
    contourWidget->show();
}

void KPCAProjection::ChangeOptions()
{
    switch(params->kernelTypeCombo->currentIndex())
    {
    case 0: // poly
        params->kernelDegSpin->setEnabled(true);
        params->kernelDegSpin->setVisible(true);
        params->kernelWidthSpin->setEnabled(false);
        params->kernelWidthSpin->setVisible(false);
        break;
    case 1: // RBF
        params->kernelDegSpin->setEnabled(false);
        params->kernelDegSpin->setVisible(false);
        params->kernelWidthSpin->setEnabled(true);
        params->kernelWidthSpin->setVisible(true);
        break;
    }
}

// virtual functions to manage the algorithm creation
Projector *KPCAProjection::GetProjector()
{
    return new ProjectorKPCA(params->dimCountSpin->value());
}

void KPCAProjection::DrawInfo(Canvas *canvas, QPainter &painter, Projector *projector)
{
    if(!canvas || !projector) return;
}

void KPCAProjection::GetContoursPixmap(int index)
{
    PCA *pca = dynamic_cast<PCA*>(pcaPointer);
    if(!pca) return;
    if(contourPixmaps.count(index)) return; // nothing to be done here, moving on!

    // we compute the density map
    int w = 65;
    int h = 65;
    int hmo = h-1; // we will drop one line at the edges to avoid weird border conditions
    int wmo = w-1;
    QImage image(wmo,hmo,QImage::Format_RGB32);
    float *values = new float[w*h];
    float vmin = FLT_MAX, vmax = -FLT_MAX;
    int dim = pca->sourcePoints.rows();

    int xIndex = contours->spinX1->value()-1;
    int yIndex = contours->spinX2->value()-1;

    VectorXd point(dim);
    FOR(d,dim) point(d) = 0;
    FOR(i, w)
    {
        FOR(j, h)
        {
            if(xIndex<dim) point(xIndex) = i/(float)w*(xmax-xmin) + xmin;
            if(yIndex<dim) point(yIndex) = j/(float)h*(ymax-ymin) + ymin;
            float value = pcaPointer->test(point, index-1); // indices start from 1 in params.dimCountSpin
            vmin = min(value, vmin);
            vmax = max(value, vmax);
            values[j*w + i] = value;
        }
    }
    float vdiff=vmax-vmin;
    if(vdiff == 0) vdiff = 1.f;
    FOR(i, wmo)
    {
        FOR(j, hmo)
        {
            int value = (int)((values[j*w + i]-vmin)/vdiff*255.f);
            image.setPixel(i,j, qRgb((int)value,value,value));
        }
    }
    QPixmap contourPixmap = QPixmap::fromImage(image).scaled(512,512, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

    int W = contourPixmap.width();
    int H = contourPixmap.height();
    QPainter painter(&contourPixmap);
    painter.setRenderHint(QPainter::Antialiasing);

    // we plot the samples
    painter.setPen(Qt::black);
    painter.setBrush(Qt::white);
    painter.setOpacity(1);
    FOR(i, contourSamples.size())
    {
        fvec &sample = contourSamples[i];
        int x = (sample[xIndex]-xmin)/(xmax-xmin)*w;
        int y = (sample[yIndex]-ymin)/(ymax-ymin)*h;
        x = (x+1)*W/w;
        y = (y+1)*H/h;
        Canvas::drawSample(painter, QPointF(x,y), 10, contourSampleLabels[i]);
    }

    // we plot the contour lines
    if(contourSamples.size())
    {
        QContour contour(values, w, h);
        contour.Paint(painter, 10);
    }

    contourPixmaps[index] = contourPixmap;
    delete [] values;
}

void KPCAProjection::DrawContours(int index)
{
    PCA *pca = dynamic_cast<PCA*>(pcaPointer);
    if(!pca) return;
    int displayType = contours->displayCombo->currentIndex();

    switch(displayType)
    {
    case 0: // single
    {
        // ensure that we have the right pixmap
        GetContoursPixmap(index);
        contours->plotLabel->setPixmap(contourPixmaps[index]);
    }
        break;
    case 1: // take all the values and draw them
    {
        int maximum = contours->dimSpin->maximum();
        for(int i=1; i<=contours->dimSpin->maximum(); i++)
        {
            GetContoursPixmap(i);
        }
        int gridX = std::ceil(sqrtf(maximum));
        //int gridY = std::ceil(maximum / (float)gridX);
        int gridY = gridX;

        int w = contourPixmaps[1].width();
        int h = contourPixmaps[1].height();
        QPixmap bigPixmap(gridX*w, gridX*h);
        QBitmap bitmap(bigPixmap.width(), bigPixmap.height());
        bitmap.clear();
        bigPixmap.setMask(bitmap);
        bigPixmap.fill(Qt::transparent);
        QPainter painter(&bigPixmap);
        for(int i=1; i<=contours->dimSpin->maximum(); i++)
        {
            int x = ((i-1)%gridX)*w;
            int y = ((i-1)/gridX)*h;
            QRect rect(x,y,w,h);
            painter.drawPixmap(rect, contourPixmaps[i], QRect(0,0,w,h));
        }
        contours->plotLabel->setPixmap(bigPixmap.scaled(QSize(w,h), Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
    }
        break;
    }
    contours->plotLabel->repaint();
}

void KPCAProjection::DrawModel(Canvas *canvas, QPainter &painter, Projector *projector)
{
    contourPixmaps.clear();
    if(!canvas || !projector) return;
    ProjectorKPCA *kpca = dynamic_cast<ProjectorKPCA*>(projector);
    if(!kpca) return;
    pcaPointer = kpca->pca;
    vector<fvec> samples = projector->source;
    contourSamples = samples;
    contourSampleLabels = canvas->data->GetLabels();
    if(contourSampleLabels.size() != contourSamples.size()) contourSampleLabels = ivec(contourSamples.size(), 0);

    xIndex = canvas->xIndex;
    yIndex = canvas->yIndex;
    xmin=ymin=FLT_MAX;
    xmax=ymax=-FLT_MAX;
    int dim = samples.size() ? samples[0].size() : 2;
    contours->spinX1->setRange(1, dim);
    contours->spinX2->setRange(1, dim);
    if(canvas->xIndex < dim) contours->spinX1->setValue(xIndex+1);
    if(canvas->yIndex < dim) contours->spinX2->setValue(yIndex+1);

    fvec mean;
    FOR(i, samples.size())
    {
        contourSamples[i] -= kpca->mean;
        xmin=min(samples[i][xIndex]-kpca->mean[xIndex], xmin);
        xmax=max(samples[i][xIndex]-kpca->mean[xIndex], xmax);
        ymin=min(samples[i][yIndex]-kpca->mean[yIndex], ymin);
        ymax=max(samples[i][yIndex]-kpca->mean[yIndex], ymax);
        if(!i) mean = samples[0];
        else mean += samples[i];
    }
    mean /= samples.size();
    float xdiff = (xmax - xmin);
    float ydiff = (ymax - ymin);
    if(xdiff < ydiff)
    {
        xdiff = ydiff;
        float xmid = (xmax - xmin)/2 + xmin;
        xmin = xmid - xdiff/2;
        xmax = xmid + xdiff/2;
    }
    else if(ydiff < xdiff)
    {
        ydiff = xdiff;
        float ymid = (ymax - ymin)/2 + ymin;
        ymin = ymid - ydiff/2;
        ymax = ymid + ydiff/2;
    }
    if(xdiff == 0) xdiff = 5.f;
    if(ydiff == 0) ydiff = 5.f;
    xmin -= xdiff;
    xmax += xdiff;
    ymin -= ydiff;
    ymax += ydiff;
    if(samples.size() < 3)
    {
        xmin -= 3*xdiff;
        xmax += 3*xdiff;
        ymin -= 3*ydiff;
        ymax += 3*ydiff;
        cout << pcaPointer->eigenVectors;
    }

    contours->dimSpin->setRange(1, kpca->targetDims);
    DrawContours(contours->dimSpin->value());
}

// virtual functions to manage the GUI and I/O
QString KPCAProjection::GetAlgoString()
{
    return QString("KPCA");
}

void KPCAProjection::SetParams(Projector *projector)
{
    if(!projector) return;
    ProjectorKPCA *kpca = dynamic_cast<ProjectorKPCA*>(projector);
    if(!kpca) return;
    // we add 1 to the kernel type because we have taken out the linear kernel
    kpca->SetParams(params->kernelTypeCombo->currentIndex()+1, params->kernelDegSpin->value(), params->kernelWidthSpin->value());
}

void KPCAProjection::SaveOptions(QSettings &settings)
{
    settings.setValue("kernelTypeCombo", params->kernelTypeCombo->currentIndex());
    settings.setValue("kernelDegSpin", params->kernelDegSpin->value());
    settings.setValue("kernelWidthSpin", params->kernelWidthSpin->value());
    settings.setValue("dimCountSpin", params->dimCountSpin->value());
}

bool KPCAProjection::LoadOptions(QSettings &settings)
{
    if(settings.contains("kernelTypeCombo")) params->kernelTypeCombo->setCurrentIndex(settings.value("kernelTypeCombo").toInt());
    if(settings.contains("kernelDegSpin")) params->kernelDegSpin->setValue(settings.value("kernelDegSpin").toInt());
    if(settings.contains("kernelWidthSpin")) params->kernelWidthSpin->setValue(settings.value("kernelWidthSpin").toFloat());
    if(settings.contains("dimCountSpin")) params->dimCountSpin->setValue(settings.value("dimCountSpin").toInt());
    ChangeOptions();
    return true;
}

void KPCAProjection::SaveParams(QTextStream &file)
{
    file << "clusterOptions" << ":" << "kernelTypeCombo" << " " << params->kernelTypeCombo->currentIndex() << "\n";
    file << "clusterOptions" << ":" << "kernelDegSpin" << " " << params->kernelDegSpin->value() << "\n";
    file << "clusterOptions" << ":" << "kernelWidthSpin" << " " << params->kernelWidthSpin->value() << "\n";
    file << "clusterOptions" << ":" << "dimCountSpin" << " " << params->dimCountSpin->value() << "\n";
}

bool KPCAProjection::LoadParams(QString name, float value)
{
    if(name.endsWith("kernelTypeCombo")) params->kernelTypeCombo->setCurrentIndex((int)value);
    if(name.endsWith("kernelDegSpin")) params->kernelDegSpin->setValue(value);
    if(name.endsWith("kernelWidthSpin")) params->kernelWidthSpin->setValue(value);
    if(name.endsWith("dimCountSpin")) params->dimCountSpin->setValue((int)value);
    ChangeOptions();
    return true;
}
