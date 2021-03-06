#include "segbotcommunicator.h"
#include <QByteArray>
#include <QProcess>
#include <QFileInfo>
#include <QtGamepad/QGamepadManager>
#include <QtGamepad/QGamepad>

#ifdef Q_OS_UNIX
#include <termios.h>
#endif
static void config_tty(int fd)
{
#ifdef Q_OS_UNIX
    struct termios tty;
    memset(&tty, 0, sizeof(tty));

    tcgetattr(fd, &tty);

    tty.c_iflag &= ~IGNBRK;         // disable break processing
    tty.c_lflag = 0;                // no signaling chars, no echo,
                                    // no canonical processing

    tty.c_oflag = 0;                // no remapping, no delays

    tcsetattr(fd, TCSANOW, &tty);
#else
    Q_UNUSED(fd)
#endif
}

SegBotCommunicator::SegBotCommunicator(QObject *parent)
    : QObject(parent)
    , m_active(false)
    , m_angle(0)
    , m_speedLeft(0)
    , m_speedRight(0)
    , m_sensorDistance(0)
    , m_voltage(0)
    , m_updateTimer(nullptr)
    , m_armMovementTimer(nullptr)
    , m_updateInterval(100)
    , m_gamepad(nullptr)
    , m_rightServo(0)
    , m_leftServo(0)
{

    auto gamepads = QGamepadManager::instance()->connectedGamepads();
    if (!gamepads.isEmpty()) {
        m_gamepad = new QGamepad(*gamepads.begin(), this);

        connect(m_gamepad, SIGNAL(buttonUpChanged(bool)), this, SLOT(forward(bool)));
        connect(m_gamepad, SIGNAL(buttonDownChanged(bool)), this, SLOT(reverse(bool)));
        connect(m_gamepad, SIGNAL(buttonRightChanged(bool)), this, SLOT(turnRight(bool)));
        connect(m_gamepad, SIGNAL(buttonLeftChanged(bool)), this, SLOT(turnLeft(bool)));
    }

}

SegBotCommunicator::~SegBotCommunicator()
{
    delete m_gamepad;
    delete m_updateTimer;
    delete m_armMovementTimer;
}

void SegBotCommunicator::init()
{
    m_updateTimer = new QTimer();
    m_updateTimer->setInterval(m_updateInterval);
    connect(m_updateTimer, SIGNAL(timeout()), this, SLOT(update()));

    m_armMovementTimer = new QTimer();
    m_armMovementTimer->setInterval(100);
    connect(m_armMovementTimer, SIGNAL(timeout()), this, SLOT(updateArms()));
}

void SegBotCommunicator::setDevice(const QString &device)
{
    if (!QFileInfo::exists(device))
        return;

    //Initialize device
    QString programName("stty");
    QStringList arguments;
    arguments.append(QString("-F ") + device);
    arguments.append(QString("-isig -icanon -iexten -echo -echoe -echok -echoctl -echoke -opost -onlcr -cread"));
    QProcess::execute(programName, arguments);

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
    QByteArray angleQuery("?angle");
    m_rpMsgFile.write(angleQuery);
    QByteArray response = m_rpMsgFile.readLine(64);
    sscanf(response.constData(), "?angle:%d", &angle);
    if (angle != m_angle) {
        m_angle = angle;
        emit angleChanged(angle);
    }

    //Speed Left
    int speedLeft;
    QByteArray speedLeftQuery("?speedLeft");
    m_rpMsgFile.write(speedLeftQuery);
    response = m_rpMsgFile.readLine(64);
    sscanf(response.constData(), "?speedLeft:%d", &speedLeft);
    if (speedLeft != m_speedLeft) {
        m_speedLeft = speedLeft;
        emit speedLeftChanged(speedLeft);
    }

    //Speed Right
    int speedRight;
    QByteArray speedRightQuery("?speedRight");
    m_rpMsgFile.write(speedRightQuery);
    response = m_rpMsgFile.readLine(64);
    sscanf(response.constData(), "?speedRight:%d", &speedRight);
    if (speedRight != m_speedRight) {
        m_speedRight = speedRight;
        emit speedRightChanged(speedRight);
    }

    //Sensor Distance
    int sensorDistance;
    QByteArray distanceQuery("?distance");
    m_rpMsgFile.write(distanceQuery);
    response = m_rpMsgFile.readLine(64);
    sscanf(response.constData(), "?distance:%d", &sensorDistance);
    if (sensorDistance != m_sensorDistance) {
        m_sensorDistance = sensorDistance;
        emit sensorDistanceChanged(sensorDistance);
    }

    // Voltage
    int voltage;
    QByteArray voltageQuery("?voltage");
    m_rpMsgFile.write(voltageQuery);
    response = m_rpMsgFile.readLine(64);
    sscanf(response.constData(), "?voltage:%d", &voltage);
    if (voltage != m_voltage) {
        m_voltage = voltage;
        emit voltageChanged(voltage);
    }
}

void SegBotCommunicator::updateArms()
{
    if (m_gamepad == nullptr || !m_gamepad->isConnected())
        return;

    //Axis values are already normalized between -1 and 1, so mutiply that by a
    //constant speed we want and thats how fast the arm moves.

    static const int multiplier = 30;
    const int rightServo = 90 - m_gamepad->axisRightY() * multiplier;
    const int leftServo = 90 + m_gamepad->axisLeftY() * multiplier;

    if (m_rightServo != rightServo) {
        m_rightServo = rightServo;
        QString commandString = QString("!servo:1:") + QString::number(m_rightServo);
        QByteArray command = commandString.toLocal8Bit();
        m_rpMsgFile.write(command);
        m_rpMsgFile.flush();
        m_rpMsgFile.readLine(64);
    }

    if (m_leftServo != leftServo) {
        m_leftServo = leftServo;
        QString commandString = QString("!servo:0:") + QString::number(m_leftServo);
        QByteArray command = commandString.toLocal8Bit();
        m_rpMsgFile.write(command);
        m_rpMsgFile.flush();
        m_rpMsgFile.readLine(64);
    }
}

void SegBotCommunicator::turnLeft(bool pressed)
{
    if (!m_active || !m_rpMsgFile.isOpen())
        return;

    if (pressed == false)
        stop();
    else {
        QByteArray command("!turnLeft:50");
        m_rpMsgFile.write(command);
        m_rpMsgFile.flush();
        m_rpMsgFile.readLine(64);
    }
}

void SegBotCommunicator::turnRight(bool pressed)
{
    if (!m_active || !m_rpMsgFile.isOpen())
        return;

    if (pressed == false)
        stop();
    else {
        QByteArray command("!turnRight:50");
        m_rpMsgFile.write(command);
        m_rpMsgFile.flush();
        m_rpMsgFile.readLine(64);
    }
}

void SegBotCommunicator::forward(bool pressed)
{
    if (!m_active || !m_rpMsgFile.isOpen())
        return;

    if (pressed == false)
        stop();
    else {
        QByteArray command("!move:8");
        m_rpMsgFile.write(command);
        m_rpMsgFile.flush();
        m_rpMsgFile.readLine(64);
    }
}

void SegBotCommunicator::reverse(bool pressed)
{
    if (!m_active || !m_rpMsgFile.isOpen())
        return;

    if (pressed == false)
        stop();
    else {
        QByteArray command("!move:-8");
        m_rpMsgFile.write(command);
        m_rpMsgFile.flush();
        m_rpMsgFile.readLine(64);
    }
}

void SegBotCommunicator::stop()
{
    if (!m_active || !m_rpMsgFile.isOpen())
        return;

    QByteArray command("!stop");
    m_rpMsgFile.write(command);
    m_rpMsgFile.flush();
    m_rpMsgFile.readLine(64);
}

void SegBotCommunicator::openFile()
{
    if (m_rpMsgFile.isOpen())
        closeFile();

    m_rpMsgFile.setFileName(m_device);
    if (m_rpMsgFile.open(QIODevice::ReadWrite)) {
        m_active = true;
        config_tty(m_rpMsgFile.handle());
        m_updateTimer->start();
        m_armMovementTimer->start();
    } else {
        m_errorString = QString("File failed to open");
        emit errorStringChanged(m_errorString);
    }
}

void SegBotCommunicator::closeFile()
{
    m_updateTimer->stop();
    m_armMovementTimer->stop();
    m_active = false;

    if (m_rpMsgFile.isOpen()) {
        m_rpMsgFile.close();
    }
}
