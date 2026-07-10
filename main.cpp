#include "OctivOutData.h"
#include <QtWidgets/QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    OctivOutData window;
    window.show();
    return app.exec();
}
