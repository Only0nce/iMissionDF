QT += quick websockets
QT += sql
CONFIG += c++11
CONFIG += c++17
QT += widgets
QT += multimedia
static {
    QT += svg
    QTPLUGIN += qtvirtualkeyboardplugin
}
INCLUDEPATH += $$PWD
INCLUDEPATH += $$PWD/iScreenDF $$PWD/iRecordManage $$PWD/DoaViewer

# The following define makes your compiler emit warnings if you use
# any Qt feature that has been marked deprecated (the exact warnings
# depend on your compiler). Refer to the documentation for the
# deprecated API to know how to port your code away from it.
# DEFINES += QT_DEPRECATED_WARNINGS
# DEFINES += QT_NO_DEBUG_OUTPUT

# You can also make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
        Databases.cpp \
        FileUpdateWatcher.cpp \
        I2CReadWrite.cpp \
        ImaAdpcmCodec.cpp \
        InputEventReader.cpp \
        Mainwindows.cpp \
        NetworkController.cpp \
        OpenWebRxConfig.cpp \
        PCM3168A.cpp \
        ReceiverConfigManager.cpp \
        ReceiverRecorderConfigManager.cpp \
        RecorderSocketClient.cpp \
        SPI.cpp \
        SigmaStudioFW.cpp \
        SocketClient.cpp \
        alsaaudioplayer.cpp \
        alsarecconfigmanager.cpp \
        iScreenDF/TcpClientDF.cpp \
        iScreenDF/TcpServerDF.cpp \
        iScreenDF/functionTcpClient.cpp \
        iScreenDF/functionTcpServer.cpp \
        linux_spi.cpp \
        logwatcher.cpp \
        main.cpp \
        newGPIOClass.cpp \
        pcmImaadpcmcodec.cpp \
        rfdc_nco_client.cpp \
        screencapture.cpp \
        websocketclient.cpp\
        ChatServer.cpp \
        iScreenDF/ChatServerDF.cpp \
        iScreenDF/ChatClientDF.cpp \
        iScreenDF/DatabaseDF.cpp \
        iScreenDF/ImageProviderDF.cpp \
        iScreenDF/newGPIOClassDF.cpp \
        iScreenDF/iScreenDF.cpp \
        iScreenDF/CompassClient.cpp \
        iScreenDF/DataloggerDB.cpp \
        iScreenDF/GpsdReader.cpp \
        iScreenDF/NetworkMng.cpp \
        iScreenDF/function.cpp \
        iScreenDF/functionLoggerDB.cpp \
        iScreenDF/functionMonitor.cpp \
        iScreenDF/functionServer.cpp \
        iScreenDF/functionWebsocketMng.cpp \
        iScreenDF/functionconectCompassServer.cpp \
        iRecordManage/ChatClientiGate.cpp \
        iRecordManage/ChatServerWebRec.cpp \
        iRecordManage/ChatiGateServer.cpp \
        iRecordManage/mainwindowsiRec.cpp \
        iRecordManage/ChatServeriRec.cpp \
        iRecordManage/GPIOClass.cpp \
        iRecordManage/GetInputEvent.cpp \
        iRecordManage/MAX31760.cpp \
        iRecordManage/Unixsocketlistener.cpp \
        iRecordManage/databaseiRec.cpp \
        iRecordManage/max9850.cpp \
        iRecordManage/storagemanagement.cpp \
        DoaViewer/DoaClient.cpp


RESOURCES += qml.qrc

# Additional import path used to resolve QML modules in Qt Creator's code model
QML_IMPORT_PATH =

# Additional import path used to resolve QML modules just for Qt Quick Designer
QML_DESIGNER_IMPORT_PATH =

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

linux-jetson-orin-g++:{
    INCLUDEPATH += /home/ubuntu/BackupData/BackupData/OrinNx/Jetson_Linux_R35.3.1_aarch64/QtSource/sysroot/usr/local/include

    LIBS += -L/usr/local/lib \
    -lpjsua2-aarch64-unknown-linux-gnu \
    -lgpiod \
    -lstdc++ \
    -lpjsua-aarch64-unknown-linux-gnu \
    -lpjsip-ua-aarch64-unknown-linux-gnu \
    -lpjsip-simple-aarch64-unknown-linux-gnu  \
    -lpjsip-aarch64-unknown-linux-gnu \
    -lpjmedia-codec-aarch64-unknown-linux-gnu \
    -lpjmedia-aarch64-unknown-linux-gnu \
    -lpjmedia-videodev-aarch64-unknown-linux-gnu \
    -lpjmedia-audiodev-aarch64-unknown-linux-gnu \
    -lpjnath-aarch64-unknown-linux-gnu \
    -lpjlib-util-aarch64-unknown-linux-gnu \
    -lsrtp-aarch64-unknown-linux-gnu \
    -lresample-aarch64-unknown-linux-gnu \
    -lgsmcodec-aarch64-unknown-linux-gnu \
    -lspeex-aarch64-unknown-linux-gnu \
    -lilbccodec-aarch64-unknown-linux-gnu \
    -lg7221codec-aarch64-unknown-linux-gnu \
    -lyuv-aarch64-unknown-linux-gnu \
    -lpj-aarch64-unknown-linux-gnu \
    -lssl \
    -lcrypto \
    -luuid \
    -lm \
    -lrt \
    -lpthread \
    -lGeographic \
    -lasound \
    -lgps
}

linux-g++:{
    LIBS += `pkg-config --libs libpjproject`
}

HEADERS += \
    ChatServer.h \
    Databases.h \
    DesignDSP_REC_V1/DesignDSP_REC_V1_IC_1.h \
    DesignDSP_REC_V1/DesignDSP_REC_V1_IC_1_PARAM.h \
    DesignDSP_REC_V1/DesignDSP_REC_V1_IC_1_REG.h \
    FileUpdateWatcher.h \
    HMC253Controller.h \
    I2CReadWrite.h \
    ImaAdpcmCodec.h \
    InputEventReader.h \
    Mainwindows.h \
    NetworkController.h \
    OpenWebRxConfig.h \
    OpenWebRxConfig.h \
    PCM3168A.h \
    ReceiverConfigManager.h \
    ReceiverRecorderConfigManager.h \
    RecorderSocketClient.h \
    SPI.h \
    SetFreqWorker.h \
    SigmaStudioFW.h \
    SocketClient.h \
    alsaaudioplayer.h \
    alsarecconfigmanager.h \
    iScreenDF/DatabaseDF.h \
    iScreenDF/ImageProviderDF.h \
    iScreenDF/TcpClientDF.h \
    iScreenDF/TcpServerDF.h \
    iScreenDF/newGPIOClassDF.h \
    linux_spi.h \
    logwatcher.h \
    newGPIOClass.h \
    pcmImaadpcmcodec.h \
    rfdc_nco_client.h \
    screencapture.h \
    websocketclient.h\
    iScreenDF/ChatClientDF.h \
    iScreenDF/ChatServerDF.h \
    iScreenDF/CompassClient.h \
    iScreenDF/DataloggerDB.h \
    iScreenDF/GpsdReader.h \
    iScreenDF/NetworkMng.h \
    iScreenDF/iClockOrin_types.h \
    iScreenDF/iScreenDF.h \
    iScreenDF/WorkerScan.h \
    iRecordManage/ChatClientiGate.h \
    iRecordManage/ChatServerWebRec.h \
    iRecordManage/ChatiGateServer.h \
    iRecordManage/alsarecconfigmanager.h \
    iRecordManage/mainwindowsiRec.h \
    iRecordManage/ChatServeriRec.h \
    iRecordManage/FileReader.h \
    iRecordManage/GPIOClass.h \
    iRecordManage/GetInputEvent.h \
    iRecordManage/MAX31760.h \
    iRecordManage/Unixsocketlistener.h \
    iRecordManage/databaseiRec.h \
    iRecordManage/max9850.h \
    iRecordManage/storagemanagement.h \
    DoaViewer/DoaClient.h

DISTFILES +=
