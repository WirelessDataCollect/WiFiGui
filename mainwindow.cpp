﻿#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    timer = new QTimer(this);
    findLocalIPs();

    if (myudp == nullptr)
    {
        myudp = new MyUDP;
    }
    if (syncudp == nullptr)
    {
        syncudp = new MyUDP;
    }
    if(dialog == nullptr)
    {
        dialog = new chartswidgt;
    }
    dialog->setWindowTitle(tr("PLOT DATA"));
    dialog->resize(800,1000);

    QGridLayout *baseLayout = new QGridLayout(); //便于显示，创建网格布局
    baseLayout->addWidget(dialog, 1, 2);
    ui->widchart->setLayout(baseLayout);
    dialog->show();
    Adc_data.clear();
    Adc_data.resize(4);
    TotalByteNum = 0;
    TotalPackNum= 0;


    //start myudp thread
    QThread *thread_txrx= new QThread(this);
    myudp->moveToThread(thread_txrx);

    //start sync thread
    QThread *thread_sync = new QThread(this);
    syncudp->moveToThread(thread_sync);

    //syncTargetAddr
    syncTargetAddr = QHostAddress("255.255.255.255");

    qDebug()<<"syncTargetAddr"<< syncTargetAddr;
    syncListenPort = 5003;
    syncTargetPort = 5003;


    udpListenPort = 5001;
    udpTargetPort = 5002;
}

MainWindow::~MainWindow()
{
    delete ui;


}

/***********************************
 *
 * Find IP of local WiFi connections
 *
 ***********************************/

void MainWindow::findLocalIPs()
{

    //获取主机名
    QString localHost = QHostInfo::localHostName();

    QHostInfo info = QHostInfo::fromName(localHost);

        info.addresses();//QHostInfo的address函数获取本机ip地址
        //如果存在多条ip地址ipv4和ipv6：
        foreach(QHostAddress address,info.addresses())
        {
            if(address.protocol()==QAbstractSocket::IPv4Protocol){//只取ipv4协议的地址
                qDebug()<<address.toString();
                localAddr= address;
                ui->label_LocalIP->setText(address.toString());
            }
        }
}

void MainWindow::on_button_UdpSend_clicked()
{
     QString text = ui->lineEdit_UdpSend->text();
     QString senderIP = ui->lineEdit_UdpTargetIP->text();
     udpTargetPort = ui->lineEdit_UdpTargetPort->text().toInt();
     qDebug()<<"udpTargetPort"<<udpTargetPort;
     if(text.isEmpty() || senderIP.isEmpty() || udpTargetPort==0 )
     {
         qDebug()<<"text.isEmpty() || senderIP.isEmpty()";
         return;
     }
     QByteArray data;
     data.append(text);
     QHostAddress IPadress(senderIP);
     emit udpsent(IPadress, udpTargetPort, data);
     onUdpAppendMessage("Me",text);
     ui->lineEdit_UdpSend->clear();
}

void MainWindow::on_button_UdpStart_clicked()
{
    qDebug()<<ui->button_UdpStart->text();
    if (ui->button_UdpStart->text()=="Start"){

        findLocalIPs();
        quint16 port = ui->lineEdit_UdpListenPort->text().toInt();
        if(port) udpListenPort = port;
        ui->lineEdit_UdpListenPort->setText(QString::number(udpListenPort,10));

        // udp_txrx bindport
        bool isSuccess = myudp->bindPort(localAddr, udpListenPort);
        if(isSuccess)
        {
             ui->statusBar->showMessage("Listen to" + udpListenPort,2);
            qDebug()<<"udp_txrx bindport ok.";
        }
        else{
            qDebug()<<"udp_txrx bindport error.";
            return;
        }

        // syncudp  bindport
        isSuccess = syncudp->bindPort(localAddr, syncListenPort);
        if(isSuccess)
        {
          ui->statusBar->showMessage("Listen to" + syncListenPort,2);
          qDebug()<<"sync bindport ok.";
        }
        else{
             qDebug()<<"syncudp bindport error.";
             return;
        }

        // signal connect to slot
        connect(this, SIGNAL(udpsent(QHostAddress,quint16, QByteArray)),syncudp, SLOT(sendMessage(QHostAddress,quint16, QByteArray)));
        connect(this, SIGNAL(myudpsent(QHostAddress,quint16, QByteArray)),myudp, SLOT(sendMessage(QHostAddress,quint16, QByteArray)));
        connect(syncudp, SIGNAL(newMessage(QString,QByteArray)),this,SLOT(syncrxmessage(QString,QByteArray)));
        connect(myudp, SIGNAL(newMessage(QString,QByteArray)),this,SLOT(AdcByteToData(QString,QByteArray)));
        connect(timer, SIGNAL(timeout()), this, SLOT(UiDataShow()));

        //test the cninnection with client and count the number of count;
        testconnect();
        sleep(500);
        ClientCount = checkreturn(GET_TEST);
        if(ClientCount ==0){
            QString  text("these is no client");
            onUdpAppendMessage("Me",text);
            return;
        }
        // time sync
         if(synctime()==false) return;
         // set the target ip adress of clients
         if(~SendIpAdress(localAddr,udpListenPort)) return;
        // Client start
         if(start()==false) return;

         // timer begin(updata the ui)
         timer->start(1000);

         ui->lineEdit_UdpListenPort->setDisabled(true);
         ui->button_UdpStart->setText("Stop");
    }
    else{
       // Client stop
        stop();
        timer->stop();
        myudp->unbindPort();
        syncudp->unbindPort();
        ui->button_UdpStart->setText("Start");
        ui->lineEdit_UdpListenPort->setDisabled(false);

      //  disconnect(this, SIGNAL(udpsent(QHostAddress,quint16, QByteArray)),syncudp, SLOT(sendMessage(QHostAddress,quint16, QByteArray)));
        disconnect(this, SIGNAL(myudpsent(QHostAddress,quint16, QByteArray)),myudp, SLOT(sendMessage(QHostAddress,quint16, QByteArray)));
       // disconnect(syncudp, SIGNAL(newMessage(QString,QByteArray)),this,SLOT(syncrxmessage(QString,QByteArray)));
        disconnect(myudp, SIGNAL(newMessage(QString,QString)),this,SLOT(AdcByteToData(QString,QString)));
    }
}


//data receive (myudp)
void MainWindow::AdcByteToData(const QString &from, const QByteArray &message)
{

  qDebug()<<"message.size()"<<message.size();
  if(message.size()<16)
  {
      qDebug()<<"message.size()"<<message.size();
      return;
  }
  TotalPackNum ++;
  TotalByteNum += message.size();
  QDateTime datatime = ByteToDatetime(message.left(8));
  quint32 count =  ByteTouint32(message.mid(8,4));
  uchar ChannelID = message.at(12);

  DigitalIO = message.mid(13,2);
  QByteArray adcbyte = message.mid(16,count);
  QVector<QVector<float> >::iterator iter=Adc_data.begin();

  for(quint32 i=0;i<16;)
   {
       iter=Adc_data.begin();
       (*iter).append(ByteToFloat(adcbyte.mid(i,2)));
       i+=2;
       iter++;
       (*iter).append(ByteToFloat(adcbyte.mid(i,2)));
       i+=2;
       iter++;
       (*iter).append(ByteToFloat(adcbyte.mid(i,2)));
       i+=2;
       iter++;
       (*iter).append(ByteToFloat(adcbyte.mid(i,2)));
       i+=2;
  }


//  iter=Adc_data.begin();
//  AdcDataShow((*iter).at(0), (*(iter+1)).at(0), (*(iter+2)).at(0), (*(iter+3)).at(0));
// DigitalDataShow(DigitalIO);
// Adc_data.clear();
// Adc_data.resize(4);

}


void MainWindow::UiDataShow()
{
    QVector<QVector<float> >::iterator iter=Adc_data.begin();
    iter=Adc_data.begin();
    AdcDataShow((*iter).at(0), (*(iter+1)).at(0), (*(iter+2)).at(0), (*(iter+3)).at(0));
    DigitalDataShow(DigitalIO);
    qDebug()<< "(*iter).size()"<<(*iter).size();
    qDebug()<<"TotalByteNum"<< TotalByteNum;
    qDebug()<<"TotalPackNum"<< TotalPackNum;
    TotalByteNum = 0;
    TotalPackNum = 0;
    Adc_data.clear();
    Adc_data.resize(4);
}

void MainWindow::AdcDataShow(float ch1, float ch2, float ch3, float ch4)
{
    ui->label_Channel1->setText(QString::number(ch1, 'g', 4));
    ui->label_Channel2->setText(QString::number(ch2, 'g', 4));
    ui->label_Channel3->setText(QString::number(ch3, 'g', 4));
    ui->label_Channel4->setText(QString::number(ch4, 'g', 4));
}

void MainWindow::DigitalDataShow(QByteArray bate0)
{
     qDebug()<< "DigitalDataShow"<<bate0.toHex();
    if(bate0.at(0) == 0)

    {
        ui->radioButton_1_0->setChecked(true);
        ui->radioButton_1_1->setChecked(false);
    }

    else
    {
        ui->radioButton_1_0->setChecked(false);
        ui->radioButton_1_1->setChecked(true);
    }
    if(bate0.at(1) == 0)

    {
        ui->radioButton_2_0->setChecked(true);
        ui->radioButton_2_1->setChecked(false);
    }

    else
    {
        ui->radioButton_2_0->setChecked(false);
        ui->radioButton_2_1->setChecked(true);
    }

}


int MainWindow::checkreturn(uchar order)
{

      return OrderReturn.count(order);
}
void MainWindow::syncrxmessage(const QString &from, const QByteArray &message)
{
   if(message.at(0) == GET_TIME_SYNC)  //时间同步
   {
   }
   else if(message.at(0) == GET_RETURN_ORDER)
   {
       OrderReturn.append(message.at(1));
   }
   onUdpAppendMessage(from,message);

}

void MainWindow::testconnect()
{
   //sync test
   QByteArray databyte;
   databyte.append((uchar)GET_TEST);
   emit udpsent(syncTargetAddr, syncTargetPort,databyte);
   emit myudpsent(syncTargetAddr,udpTargetPort,databyte);
   QString text("testconnect");
   onUdpAppendMessage("Me",text);
}
//order  (syncudp)
bool MainWindow::start()
{
    QByteArray data;
    data.resize(1);
    data[0] = (uchar)GET_WIFI_SEND_EN;
    QString text = "start";
    int i=0;
    while(checkreturn(GET_WIFI_SEND_EN) != ClientCount && i<5)
    {
       OrderReturn.clear();
       emit udpsent(syncTargetAddr,syncTargetPort,data);
       onUdpAppendMessage("Me", text);
       sleep(200);
       i++;
    }
    return checkreturn(GET_WIFI_SEND_EN) == ClientCount;
}
bool MainWindow::stop()
{
    QByteArray data;
    data.resize(1);
    data[0] = (uchar)GET_WIFI_SEND_DISABLE;
    QString text = "stop";
    int i=0;
    while(checkreturn(GET_WIFI_SEND_DISABLE) != ClientCount && i<5)
    {
       OrderReturn.clear();
       emit udpsent(syncTargetAddr,syncTargetPort,data);
       onUdpAppendMessage("Me", text);
       sleep(200);
       i++;
    }
    return checkreturn(GET_WIFI_SEND_DISABLE) == ClientCount;
}
bool MainWindow::synctime()
{
    QByteArray databyte;
    databyte.append((uchar)GET_TIME_SYNC);
    QDateTime datetime = QDateTime::currentDateTime();
    databyte.append(DatetimeToByte(datetime));
    int i=0;
    while(checkreturn(GET_TIME_SYNC) != ClientCount && i<5)
    {
       OrderReturn.clear();
       emit udpsent(syncTargetAddr,syncTargetPort,databyte);
       onUdpAppendMessage("Me",datetime.toString());
       sleep(200);
       i++;
    }
    return checkreturn(GET_TIME_SYNC) == ClientCount;

}
bool MainWindow::SendAdcModle()
{
    QByteArray databyte;
    databyte.append((uchar)GET_CHANNEL_MODEL);
    if(ui->checkBox->isChecked())   databyte.append(0x01);  //V
    else databyte.append((char)0x0);                             //I
    if(ui->checkBox_2->isChecked())   databyte.append(0x01);  //V
    else databyte.append((char)0x0);                             //I
    if(ui->checkBox_3->isChecked())   databyte.append(0x01);  //V
    else databyte.append((char)0);                             //I
    if(ui->checkBox_4->isChecked())   databyte.append(0x01);  //V
    else databyte.append((char)0x0);                             //I

    int i = 0;
    while(checkreturn(GET_CHANNEL_MODEL) != ClientCount && i<5)
    {
       OrderReturn.clear();
       emit udpsent(syncTargetAddr,syncTargetPort,databyte);
        onUdpAppendMessage("Me"," SendAdcModle: "+databyte.toHex());
       sleep(200);
       i++;
    }
    return checkreturn(GET_TIME_SYNC) == ClientCount;
}

bool MainWindow::SendIpAdress(QHostAddress ip, quint16 port)
{
   QByteArray databyte;
   databyte.append((uchar)GET_REMOTE_IP_PORT);

   quint32    ipv4sdress = ip.toIPv4Address();
   QByteArray ipv4adressByte =IntToHighByte(ipv4sdress);
   databyte.append(ipv4adressByte);
   QByteArray portByte = uint16ToByte(port);
   databyte.append(portByte);
//   qDebug()<<"syncTargetAddr"<<ip;
//   qDebug()<< "databyte.size()"<<databyte.size();
//   qDebug()<<"databyte.toHex()"<< databyte.toHex();

   int i = 0;
   while(checkreturn(GET_REMOTE_IP_PORT) != ClientCount && i<5)
   {
      OrderReturn.clear();
      emit udpsent(syncTargetAddr,syncTargetPort,databyte);
      onUdpAppendMessage("Me"," SendIpAdress: "+databyte.toHex());
      sleep(200);
      i++;
   }
   return checkreturn(GET_TIME_SYNC) == ClientCount;
}

void MainWindow::onUdpAppendMessage(const QString &from, const QString &message)
{
   qDebug()<<"onUdpAppendMessage:  "<< message;
    QTextCursor cursor(ui->textBrowser_UdpMessage->textCursor());
    cursor.movePosition(QTextCursor::End);
    if(from != "Me")
    {

        QTextTable *table = cursor.insertTable(1, 2, tableFormat);
        table->cellAt(0, 0).firstCursorPosition().insertText('<' + from + "> ");
        table->cellAt(0, 1).firstCursorPosition().insertText(message);
        }
    else
        {
        QTextTable *table = cursor.insertTable(1, 2, tableFormat);
        table->cellAt(0, 0).firstCursorPosition().insertText('<' + from + "> ");
        table->cellAt(0, 1).firstCursorPosition().insertText(message);
    }
    QScrollBar *bar = ui->textBrowser_UdpMessage->verticalScrollBar();
    bar->setValue(bar->maximum());
}

void MainWindow::onUdpAppendMessage(const QString &from, const QByteArray &message)
{
   qDebug()<<"onUdpAppendMessage:  "<< message;
    QTextCursor cursor(ui->textBrowser_UdpMessage->textCursor());
    cursor.movePosition(QTextCursor::End);
    if(from != "Me")
    {

        QTextTable *table = cursor.insertTable(1, 2, tableFormat);
        table->cellAt(0, 0).firstCursorPosition().insertText('<' + from + "> ");
        table->cellAt(0, 1).firstCursorPosition().insertText(message.toHex());
        }
    else
        {
        QTextTable *table = cursor.insertTable(1, 2, tableFormat);
        table->cellAt(0, 0).firstCursorPosition().insertText('<' + from + "> ");
        table->cellAt(0, 1).firstCursorPosition().insertText(message.toHex());
    }
    QScrollBar *bar = ui->textBrowser_UdpMessage->verticalScrollBar();
    bar->setValue(bar->maximum());
}



QByteArray MainWindow::DatetimeToByte(QDateTime datetime)
{
    QTime time = datetime.time();
    QDate date = datetime.date();
    QByteArray datebyte;

    datebyte.append((uchar)(date.year()&0x000000ff));
    datebyte.append((uchar)((date.year()&0x0000ff00)>>8));
    datebyte.append((uchar)(date.month()&0x000000ff));
    datebyte.append((uchar)(date.day()&0x000000ff));

    quint32 msec= (time.hour()*3600+time.minute()*60+time.second())*1000+time.msec();
    QByteArray timebyte = IntToByte(msec);
    datebyte.append(timebyte.data());
    qDebug()<<"datetime"<<date.year()<<date.month()<<date.day();
    qDebug()<<"datebyte.toHex()"<<datebyte.toHex();
    return datebyte;

}

QDateTime MainWindow:: ByteToDatetime(QByteArray datebyte)
{
    QDate date(datebyte[0] |(((quint16)datebyte[1])<<8),datebyte[2],datebyte[3]);
    QTime time(0,0,0);
    QByteArray timebyte = datebyte.right(4);
    time.addMSecs(ByteTouint32(timebyte));
    QDateTime datetime(date,time,QTimeZone::systemTimeZone());
    return datetime;
}

QByteArray MainWindow:: IntToByte(quint32 i)
{
    QByteArray abyte0;
    abyte0.resize(4);
    abyte0[0] = (uchar)  (0x000000ff & i);
    abyte0[1] = (uchar) ((0x0000ff00 & i) >> 8);
    abyte0[2] = (uchar) ((0x00ff0000 & i) >> 16);
    abyte0[3] = (uchar) ((0xff000000 & i) >> 24);
    return abyte0;
}

QByteArray MainWindow:: IntToHighByte(quint32 i)
{
    QByteArray abyte0;
    abyte0.resize(4);
    abyte0[3] = (uchar)  (0x000000ff & i);
    abyte0[2] = (uchar) ((0x0000ff00 & i) >> 8);
    abyte0[1] = (uchar) ((0x00ff0000 & i) >> 16);
    abyte0[0] = (uchar) ((0xff000000 & i) >> 24);
    return abyte0;
}

QByteArray MainWindow:: uint16ToByte(quint16 i)
{
    QByteArray abyte0;
    abyte0.resize(2);
    abyte0[0] = (uchar)  (0x00ff & i);
    abyte0[1] = (uchar) ((0xff00 & i) >> 8);
    return abyte0;
}


quint32 MainWindow:: ByteTouint32(QByteArray abyte0)
{
    quint32 data =0;
    for(int i=3;i>=0;i--)
    {
        data <<=8;
        data |= abyte0.at(i) ;

    }
    return data;

}
quint16 MainWindow:: ByteTouint16(QByteArray abyte0)
{
    quint16 data =0;
    for(int i=1;i>=0;i--)
    {
        data <<=8;
        data |= abyte0.at(i);

    }
    return data;
}
float MainWindow:: ByteToFloat(QByteArray abyte0)
{

   return (((uchar)abyte0.at(0))*16 + ((uchar)abyte0.at(1))/16) *5.0/4096.0;
}

void MainWindow::on_button_IVSetting_clicked()
{
    SendAdcModle();
}

void MainWindow::sleep(unsigned int msec)
{
    QTime dieTime = QTime::currentTime().addMSecs(msec);
    while( QTime::currentTime() < dieTime )
        QCoreApplication::processEvents(QEventLoop::AllEvents, 100);
}
