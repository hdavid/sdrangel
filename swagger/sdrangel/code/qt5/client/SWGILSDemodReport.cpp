/**
 * SDRangel
 * This is the web REST/JSON API of SDRangel SDR software. SDRangel is an Open Source Qt5/OpenGL 3.0+ (4.3+ in Windows) GUI and server Software Defined Radio and signal analyzer in software. It supports Airspy, BladeRF, HackRF, LimeSDR, PlutoSDR, RTL-SDR, SDRplay RSP1 and FunCube    ---   Limitations and specifcities:    * In SDRangel GUI the first Rx device set cannot be deleted. Conversely the server starts with no device sets and its number of device sets can be reduced to zero by as many calls as necessary to /sdrangel/deviceset with DELETE method.   * Preset import and export from/to file is a server only feature.   * Device set focus is a GUI only feature.   * The following channels are not implemented (status 501 is returned): ATV and DATV demodulators, Channel Analyzer NG, LoRa demodulator   * The device settings and report structures contains only the sub-structure corresponding to the device type. The DeviceSettings and DeviceReport structures documented here shows all of them but only one will be or should be present at a time   * The channel settings and report structures contains only the sub-structure corresponding to the channel type. The ChannelSettings and ChannelReport structures documented here shows all of them but only one will be or should be present at a time    --- 
 *
 * OpenAPI spec version: 7.0.0
 * Contact: f4exb06@gmail.com
 *
 * NOTE: This class is auto generated by the swagger code generator program.
 * https://github.com/swagger-api/swagger-codegen.git
 * Do not edit the class manually.
 */


#include "SWGILSDemodReport.h"

#include "SWGHelpers.h"

#include <QJsonDocument>
#include <QJsonArray>
#include <QObject>
#include <QDebug>

namespace SWGSDRangel {

SWGILSDemodReport::SWGILSDemodReport(QString* json) {
    init();
    this->fromJson(*json);
}

SWGILSDemodReport::SWGILSDemodReport() {
    channel_power_db = 0.0f;
    m_channel_power_db_isSet = false;
    channel_sample_rate = 0;
    m_channel_sample_rate_isSet = false;
    ident = nullptr;
    m_ident_isSet = false;
    deviation = 0.0f;
    m_deviation_isSet = false;
    sdm = 0.0f;
    m_sdm_isSet = false;
    ddm = 0.0f;
    m_ddm_isSet = false;
    dm90 = 0.0f;
    m_dm90_isSet = false;
    dm150 = 0.0f;
    m_dm150_isSet = false;
}

SWGILSDemodReport::~SWGILSDemodReport() {
    this->cleanup();
}

void
SWGILSDemodReport::init() {
    channel_power_db = 0.0f;
    m_channel_power_db_isSet = false;
    channel_sample_rate = 0;
    m_channel_sample_rate_isSet = false;
    ident = new QString("");
    m_ident_isSet = false;
    deviation = 0.0f;
    m_deviation_isSet = false;
    sdm = 0.0f;
    m_sdm_isSet = false;
    ddm = 0.0f;
    m_ddm_isSet = false;
    dm90 = 0.0f;
    m_dm90_isSet = false;
    dm150 = 0.0f;
    m_dm150_isSet = false;
}

void
SWGILSDemodReport::cleanup() {


    if(ident != nullptr) { 
        delete ident;
    }





}

SWGILSDemodReport*
SWGILSDemodReport::fromJson(QString &json) {
    QByteArray array (json.toStdString().c_str());
    QJsonDocument doc = QJsonDocument::fromJson(array);
    QJsonObject jsonObject = doc.object();
    this->fromJsonObject(jsonObject);
    return this;
}

void
SWGILSDemodReport::fromJsonObject(QJsonObject &pJson) {
    ::SWGSDRangel::setValue(&channel_power_db, pJson["channelPowerDB"], "float", "");
    
    ::SWGSDRangel::setValue(&channel_sample_rate, pJson["channelSampleRate"], "qint32", "");
    
    ::SWGSDRangel::setValue(&ident, pJson["ident"], "QString", "QString");
    
    ::SWGSDRangel::setValue(&deviation, pJson["deviation"], "float", "");
    
    ::SWGSDRangel::setValue(&sdm, pJson["sdm"], "float", "");
    
    ::SWGSDRangel::setValue(&ddm, pJson["ddm"], "float", "");
    
    ::SWGSDRangel::setValue(&dm90, pJson["dm90"], "float", "");
    
    ::SWGSDRangel::setValue(&dm150, pJson["dm150"], "float", "");
    
}

QString
SWGILSDemodReport::asJson ()
{
    QJsonObject* obj = this->asJsonObject();

    QJsonDocument doc(*obj);
    QByteArray bytes = doc.toJson();
    delete obj;
    return QString(bytes);
}

QJsonObject*
SWGILSDemodReport::asJsonObject() {
    QJsonObject* obj = new QJsonObject();
    if(m_channel_power_db_isSet){
        obj->insert("channelPowerDB", QJsonValue(channel_power_db));
    }
    if(m_channel_sample_rate_isSet){
        obj->insert("channelSampleRate", QJsonValue(channel_sample_rate));
    }
    if(ident != nullptr && *ident != QString("")){
        toJsonValue(QString("ident"), ident, obj, QString("QString"));
    }
    if(m_deviation_isSet){
        obj->insert("deviation", QJsonValue(deviation));
    }
    if(m_sdm_isSet){
        obj->insert("sdm", QJsonValue(sdm));
    }
    if(m_ddm_isSet){
        obj->insert("ddm", QJsonValue(ddm));
    }
    if(m_dm90_isSet){
        obj->insert("dm90", QJsonValue(dm90));
    }
    if(m_dm150_isSet){
        obj->insert("dm150", QJsonValue(dm150));
    }

    return obj;
}

float
SWGILSDemodReport::getChannelPowerDb() {
    return channel_power_db;
}
void
SWGILSDemodReport::setChannelPowerDb(float channel_power_db) {
    this->channel_power_db = channel_power_db;
    this->m_channel_power_db_isSet = true;
}

qint32
SWGILSDemodReport::getChannelSampleRate() {
    return channel_sample_rate;
}
void
SWGILSDemodReport::setChannelSampleRate(qint32 channel_sample_rate) {
    this->channel_sample_rate = channel_sample_rate;
    this->m_channel_sample_rate_isSet = true;
}

QString*
SWGILSDemodReport::getIdent() {
    return ident;
}
void
SWGILSDemodReport::setIdent(QString* ident) {
    this->ident = ident;
    this->m_ident_isSet = true;
}

float
SWGILSDemodReport::getDeviation() {
    return deviation;
}
void
SWGILSDemodReport::setDeviation(float deviation) {
    this->deviation = deviation;
    this->m_deviation_isSet = true;
}

float
SWGILSDemodReport::getSdm() {
    return sdm;
}
void
SWGILSDemodReport::setSdm(float sdm) {
    this->sdm = sdm;
    this->m_sdm_isSet = true;
}

float
SWGILSDemodReport::getDdm() {
    return ddm;
}
void
SWGILSDemodReport::setDdm(float ddm) {
    this->ddm = ddm;
    this->m_ddm_isSet = true;
}

float
SWGILSDemodReport::getDm90() {
    return dm90;
}
void
SWGILSDemodReport::setDm90(float dm90) {
    this->dm90 = dm90;
    this->m_dm90_isSet = true;
}

float
SWGILSDemodReport::getDm150() {
    return dm150;
}
void
SWGILSDemodReport::setDm150(float dm150) {
    this->dm150 = dm150;
    this->m_dm150_isSet = true;
}


bool
SWGILSDemodReport::isSet(){
    bool isObjectUpdated = false;
    do{
        if(m_channel_power_db_isSet){
            isObjectUpdated = true; break;
        }
        if(m_channel_sample_rate_isSet){
            isObjectUpdated = true; break;
        }
        if(ident && *ident != QString("")){
            isObjectUpdated = true; break;
        }
        if(m_deviation_isSet){
            isObjectUpdated = true; break;
        }
        if(m_sdm_isSet){
            isObjectUpdated = true; break;
        }
        if(m_ddm_isSet){
            isObjectUpdated = true; break;
        }
        if(m_dm90_isSet){
            isObjectUpdated = true; break;
        }
        if(m_dm150_isSet){
            isObjectUpdated = true; break;
        }
    }while(false);
    return isObjectUpdated;
}
}

