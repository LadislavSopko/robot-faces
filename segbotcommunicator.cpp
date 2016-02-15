#include "segbotcommunicator.h"
#include <termios.h>

static void config_tty(int fd)
{
    struct termios tty;
    memset(&tty, 0, sizeof(tty));

    tcgetattr(fd, &tty);

    tty.c_iflag &= ~IGNBRK;         // disable break processing
    tty.c_lflag = 0;                // no signaling chars, no echo,
                                    // no canonical processing

    tty.c_oflag = 0;                // no remapping, no delays

    tcsetattr(fd, TCSANOW, &tty);
}


SegBotCommunicator::SegBotCommunicator(QObject *parent)
    : QObject(parent)
    , m_active(false)
    , m_angle(0)
    , m_speedLeft(0)
    , m_speedRight(0)
    , m_sensorDistance(0)
    , m_updateTimer(nullptr)
    , m_updateInterval(100)
{

}

SegBotCommunicator::~SegBotCommunicator()
{
    delete m_updateTimer;
}

void SegBotCommunicator::init()
{
    m_updateTimer = new QTimer();
    m_updateTimer->setInterval(m_updateInterval);
    connect(m_updateTimer, SIGNAL(timeout()), this, SLOT(update()));
}

void SegBotCommunicator::setDevice(const QString &device)
{
    m_device = device;
    // Open the new device file
    if (!m_device.isEmpty())
        openFile();
    else
        closeFile();
}

void SegBotCommunicator::setUpdateInterval(int interval)
{
    m_updateInterval = interval;
    m_updateTimer->setInterval(m_updateInterval);
}

void SegBotCommunicator::update()
{
    // Get Updated status
    if (!m_active || !m_rpMsgFile.isOpen())
        return;

    //Angle
    int angle;
    m_commandStream << QLatin1String("?angle\n");
    m_commandStream >> angle;
    if (angle != m_angle) {
        m_angle = angle;
        emit angleChanged(angle);
    }

    //Speed Left
    int speedLeft;
    m_commandStream << QLatin1String("?speedLeft\n");
    m_commandStream >> speedLeft;
    if (speedLeft != m_speedLeft) {
        m_speedLeft = speedLeft;
        emit speedLeftChanged(speedLeft);
    }

    //Speed Right
    int speedRight;
    m_commandStream << QLatin1String("?speedRight\n");
    m_commandStream >> speedRight;
    if (speedRight != m_speedRight) {
        m_speedRight = speedRight;
        emit speedRightChanged(speedRight);
    }

    //Sensor Distance
    int sensorDistance;
    m_commandStream << QLatin1String("?distance\n");
    m_commandStream >> sensorDistance;
    if (sensorDistance != m_sensorDistance) {
        m_sensorDistance = sensorDistance;
        emit sensorDistanceChanged(sensorDistance);
    }
}

void SegBotCommunicator::openFile()
{
    if (m_rpMsgFile.isOpen())
        closeFile();

    m_rpMsgFile.setFileName(m_device);
    if (m_rpMsgFile.open(QIODevice::ReadWrite)) {
        m_active = true;
        config_tty(m_rpMsgFile.handle());
        m_commandStream.setDevice(&m_rpMsgFile);
        m_updateTimer->start();
    } else {
        m_errorString = QString("File failed to open");
        emit errorStringChanged(m_errorString);
    }
}

void SegBotCommunicator::closeFile()
{
    m_updateTimer->stop();
    m_commandStream.setDevice(nullptr);
    m_active = false;

    if (m_rpMsgFile.isOpen()) {
        m_rpMsgFile.close();
    }
}