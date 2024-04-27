#include "widget.h"
#include "test.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QString url = "../Forrest_Gump_IMAX.mp4";
    testPlayer(url);
    return 0;
    //    QApplication a(argc, argv);
    //    Widget w;
    //    w.show();
    //    return a.exec();
}
