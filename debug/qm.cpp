#include <QCoreApplication>
#include <QProcess>
#include <QTimer>
#include <QThread>
#include <QDebug>
#include <cstdio>
int main(int argc, char *argv[]) {
    QCoreApplication a(argc, argv);
    // 模拟 DTK5/DApplication 启动后的线程环境
    QThread *t = new QThread;
    QObject::connect(t, &QThread::started, []{ QThread::sleep(30); });
    t->start();
    QProcess *p = new QProcess(&a);
    QObject::connect(p, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                     [&](int code, QProcess::ExitStatus) {
        printf("FINISHED code=%d output_tail=[%s]\n", code,
               p->readAllStandardOutput().trimmed().right(60).constData());
        a.quit();
    });
    auto *deadThread = QThread::create([]{ QThread::sleep(30); });
    deadThread->start();   // 模拟 DTK 启动产生的常驻线程
    printf("thread started
");
    p->start("bash", QStringList() << "-c" << "apt list --upgradable 2>/dev/null | wc -l");
    QTimer::singleShot(20000, [&]{ printf("TIMEOUT - finished never fired\n"); a.quit(); });
    return a.exec();
}
