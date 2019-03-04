#include "RTSystemExtItem.h"
#include "RTSCommonUtil.h"
#include "RTSTypeUtilExt.h"
#include "ProfileHandlerExt.h"
#include <cnoid/MessageView>
#include <cnoid/ItemManager>
#include <cnoid/Archive>
#include <cnoid/EigenArchive>
#include <cnoid/AppConfig>
#include <cnoid/Timer>
#include <fmt/format.h>
#include "LoggerUtil.h"

#include "gettext.h"

using namespace cnoid;
using namespace std;
using namespace std::placeholders;
using namespace RTC;
using fmt::format;

namespace cnoid {

struct RTSPortComparator
{
    std::string name_;

    RTSPortComparator(std::string value)
    {
        name_ = value;
    }
    bool operator()(const RTSPortExtPtr elem) const
    {
        return (name_ == elem->name);
    }
};
//////////
class RTSystemExtItemImpl
{
public:
    map<string, RTSCompExtPtr> rtsComps;
    RTSystemExtItem::RTSConnectionMap rtsConnections;
    bool autoConnection;

    std::string vendorName;
    std::string version;
    int pollingCycle;
    bool checkAtLoading;

#if defined(OPENRTM_VERSION12)
    int heartBeatPeriod;
#endif

    enum State_Detection
    {
        POLLING_CHECK = 0,
        MANUAL_CHECK,
#if defined(OPENRTM_VERSION12)
        OBSERVER_CHECK,
#endif
        N_STATE_DETECTION
    };
    Selection stateCheck;

    RTSystemExtItemImpl(RTSystemExtItem* self);
    RTSystemExtItemImpl(RTSystemExtItem* self, const RTSystemExtItemImpl& org);
    ~RTSystemExtItemImpl();

    void initialize();
    void onLocationChanged(std::string host, int port);
    RTSCompExt* addRTSComp(const string& name, const QPointF& pos);
    RTSCompExt* addRTSComp(const NamingContextHelper::ObjectInfo& info, const QPointF& pos);
    void deleteRTSComp(const string& name);
    RTSCompExt* nameToRTSComp(const string& name);
    bool compIsAlive(RTSCompExt* rtsComp);
    RTSConnectionExt* addRTSConnection(
        const string& id, const string& name,
        RTSPortExt* sourcePort, RTSPortExt* targetPort, const std::vector<NamedValueExtPtr>& propList,
        const bool setPos, const Vector2 pos[]);
    RTSConnectionExt* addRTSConnectionName(
        const string& id, const string& name,
        const string& sourceComp, const string& sourcePort,
        const string& targetComp, const string& targetPort,
        const string& dataflow, const string& subscription,
        const bool setPos, const Vector2 pos[]);
    bool connectionCheck();
    void RTSCompToConnectionList(const RTSCompExt* rtsComp, list<RTSConnectionExt*>& rtsConnectionList, int mode);
    void removeConnection(RTSConnectionExt* connection);
    void doPutProperties(PutPropertyFunction& putProperty);
    bool saveRtsProfile(const string& filename);
    void restoreRTSystem(const Archive& archive);
    void restoreRTSComp(const string& name, const Vector2& pos,
            const vector<pair<string, bool>>& inPorts, const vector<pair<string, bool>>& outPorts);
    string getConnectionNumber();
    void setStateCheckMethodByString(const string& value);
    void checkStatus();

    Signal<void(bool)> sigStatusUpdate;

    void onActivated();
    void changePollingPeriod(int value);
    void changeStateCheck();

private:
    RTSystemExtItem* self;
    Connection locationChangedConnection;
    int connectionNo;

    Timer timer;
    ScopedConnection timeOutConnection;

    void setStateCheckMethod(int value);
};

}
//////////
RTSPortExt::RTSPortExt(const string& name_, PortService_var port_, RTSCompExt* parent)
    : rtsComp(parent), isConnected_(false)
{
    isInPort = true;
    name = name_;
    port = port_;
    //
    if (port) {
        RTC::PortProfile_var profile = port->get_port_profile();
        RTC::PortInterfaceProfileList interfaceList = profile->interfaces;
        for (CORBA::ULong index = 0; index < interfaceList.length(); ++index) {
            RTC::PortInterfaceProfile ifProfile = interfaceList[index];
            PortInterfaceExtPtr portIf(new PortInterfaceExt());
            if (parent) {
                portIf->rtc_name = parent->name;
            }
            const char* port_name;
            portIf->port_name = string((const char*)profile->name);
            if (ifProfile.polarity == PortInterfacePolarity::REQUIRED) {
                portIf->if_polarity = "required";
            } else {
                portIf->if_polarity = "provided";
            }
            const char* if_iname;
            const char* if_tname;
            if_iname = (const char*)ifProfile.instance_name;
            if_tname = (const char*)ifProfile.type_name;
            DDEBUG_V("name: %s, IF name: %s, instance_name: %s, type_name: %s", name_.c_str(), portIf->port_name.c_str(), if_iname, if_tname);
            portIf->if_tname = if_tname;
            portIf->if_iname = if_iname;
            interList.push_back(portIf);

            isConnected_ = isConnected();
        }
    }
}

bool RTSPortExt::isConnected()
{
    if (!port || !isObjectAlive(port)) {
        return false;
    }
    ConnectorProfileList_var connectorProfiles = port->get_connector_profiles();
    return connectorProfiles->length() != 0;
}


bool RTSPortExt::isConnectedWith(RTSPortExt* target)
{
    if (!port || !target->port ||
       CORBA::is_nil(port) || port->_non_existent() ||
       CORBA::is_nil(target->port) || target->port->_non_existent()) {
        return false;
    }

    ConnectorProfileList_var connectorProfiles = port->get_connector_profiles();

    for (CORBA::ULong i = 0; i < connectorProfiles->length(); ++i) {
        ConnectorProfile& connectorProfile = connectorProfiles[i];
        PortServiceList& connectedPorts = connectorProfile.ports;
        for (CORBA::ULong j = 0; j < connectedPorts.length(); ++j) {
            PortService_ptr connectedPortRef = connectedPorts[j];
            if (connectedPortRef->_is_equivalent(target->port)) {
                return true;
            }
        }
    }

    return false;
}


bool RTSPortExt::checkConnectablePort(RTSPortExt* target)
{
    DDEBUG("RTSPort::checkConnectablePort");
    if (rtsComp == target->rtsComp) {
        return false;
    }
    if (!port || !target->port) {
        return false;
    }

    if ((isInPort && target->isInPort) ||
        (isInPort && target->isServicePort) ||
        (!isInPort && !isServicePort && !target->isInPort) ||
        (isServicePort && !target->isServicePort)) {
        return false;
    }

    //In case of connection between data ports
    if (!isServicePort && !target->isServicePort) {
        vector<string> dataTypes = RTSTypeUtilExt::getAllowDataTypes(this, target);
        vector<string> ifTypes = RTSTypeUtilExt::getAllowInterfaceTypes(this, target);
        vector<string> subTypes = RTSTypeUtilExt::getAllowSubscriptionTypes(this, target);
        if (dataTypes.size() == 0 || ifTypes.size() == 0 || subTypes.size() == 0) {
            return false;
        }
    }

    return true;
}


vector<string> RTSPortExt::getDataTypes()
{
    return getProperty("dataport.data_type");
}


vector<string> RTSPortExt::getInterfaceTypes()
{
    return getProperty("dataport.interface_type");
}


vector<string> RTSPortExt::getDataflowTypes()
{
    return getProperty("dataport.dataflow_type");
}


vector<string> RTSPortExt::getSubscriptionTypes()
{
    return getProperty("dataport.subscription_type");
}


vector<string> RTSPortExt::getProperty(const string& key)
{
    vector<string> result;

    NVList properties = port->get_port_profile()->properties;
    for (int index = 0; index < properties.length(); ++index) {
        string strName(properties[index].name._ptr);
        if (strName == key) {
            const char* nvValue;
            properties[index].value >>= nvValue;
            string strValue(nvValue);
            result = RTCCommonUtil::split(strValue, ',');
            break;
        }
    }

    return result;
}

RTSPortExt* RTSCompExt::nameToRTSPort(const string& name)
{
    //DDEBUG_V("RTSComp::nameToRTSPort %s", name.c_str());

    auto it = find_if(inPorts.begin(), inPorts.end(), RTSPortComparator(name));
    if (it != inPorts.end()) {
        return *it;
    }
    it = find_if(outPorts.begin(), outPorts.end(), RTSPortComparator(name));
    if (it != outPorts.end()) {
        return *it;
    }
    return nullptr;
}
//////////
RTSConnectionExt::RTSConnectionExt(const string& id, const string& name,
        const string& sourceRtcName, const string& sourcePortName,
        const string& targetRtcName, const string& targetPortName)
    : id(id), name(name), sourceRtcName(sourceRtcName), sourcePortName(sourcePortName),
    targetRtcName(targetRtcName), targetPortName(targetPortName), setPos(false)
{
    isAlive_ = false;
}


bool RTSConnectionExt::connect()
{
    DDEBUG("RTSConnection::connect");

    if (!sourcePort || !targetPort) {
        return false;
    }

    if (!sourcePort->port || !targetPort->port ||
       CORBA::is_nil(sourcePort->port) || sourcePort->port->_non_existent() ||
       CORBA::is_nil(targetPort->port) || targetPort->port->_non_existent()) {
        DDEBUG("RTSConnection::connect False");
        return false;
    }

    if (sourcePort->isConnectedWith(targetPort)) {
        return false;
    }

    ConnectorProfile cprof;
    cprof.connector_id = CORBA::string_dup(id.c_str());
    cprof.name = CORBA::string_dup(name.c_str());
    cprof.ports.length(2);
    cprof.ports[0] = PortService::_duplicate(sourcePort->port);
    cprof.ports[1] = PortService::_duplicate(targetPort->port);

    for (int index = 0; index < propList.size(); index++) {
        NamedValueExtPtr param = propList[index];
        CORBA_SeqUtil::push_back(
            cprof.properties, NVUtil::newNV(param->name_.c_str(), param->value_.c_str()));
    }

    RTC::ReturnCode_t result = sourcePort->port->connect(cprof);
    if (result == RTC::RTC_OK) {
        PortProfile_var portprofile = sourcePort->port->get_port_profile();
        ConnectorProfileList connections = portprofile->connector_profiles;
        for (CORBA::ULong i = 0; i < connections.length(); i++) {
            ConnectorProfile& connector = connections[i];
            PortServiceList& connectedPorts = connector.ports;
            for (CORBA::ULong j = 0; j < connectedPorts.length(); ++j) {
                PortService_ptr connectedPortRef = connectedPorts[j];
                if (connectedPortRef->_is_equivalent(targetPort->port)) {
                    id = string(connector.connector_id);
                    isAlive_ = true;
                    DDEBUG("RTSConnection::connect End");
                    return true;
                }
            }
        }
        return false;
    }

    DDEBUG("connect Error");
    return false;
}


bool RTSConnectionExt::disconnect()
{
    isAlive_ = false;

    if (CORBA::is_nil(sourcePort->port) || sourcePort->port->_non_existent()) {
        return false;
    }

    return (sourcePort->port->disconnect(id.c_str()) == RTC_OK);
}


void RTSConnectionExt::setPosition(const Vector2 pos[])
{
    for (int i = 0; i < 6; ++i) {
        position[i] = pos[i];
    }
    setPos = true;
    srcRTC->rts()->suggestFileUpdate();
}
//////////
RTSCompExt::RTSCompExt(const string& name, const std::string& fullPath, RTC::RTObject_ptr rtc, RTSystemExtItem* rts, const QPointF& pos, const string& host, int port, bool isDefault)
    : rts_(rts), pos_(pos), name(name), fullPath(fullPath), hostAddress(host), portNo(port), isDefaultNS(isDefault)
{
    setRtc(rtc);
}


void RTSCompExt::setRtc(RTObject_ptr rtc)
{
    DDEBUG("RTSComp::setRtc");
    rtc_ = 0;

    rts_->suggestFileUpdate();

    setRTObject(rtc);

    if (!isObjectAlive(rtc)) {
        participatingExeContList = 0;
        for (auto it = inPorts.begin(); it != inPorts.end(); ++it) {
            RTSPortExt* port = *it;
            port->port = 0;
        }
        for (auto it = outPorts.begin(); it != outPorts.end(); ++it) {
            RTSPortExt* port = *it;
            port->port = 0;
        }
        DDEBUG("RTSComp::setRtc Failed");
        return;
    }

    ComponentProfile_var cprofile = rtc_->get_component_profile();
    participatingExeContList = rtc_->get_participating_contexts();
    rtc_status_ = getRTCState();

    inPorts.clear();
    outPorts.clear();

    PortServiceList_var portlist = rtc_->get_ports();
    for (CORBA::ULong i = 0; i < portlist->length(); ++i) {
        PortProfile_var portprofile = portlist[i]->get_port_profile();
        coil::Properties pproperties = NVUtil::toProperties(portprofile->properties);
        string portType = pproperties["port.port_type"];
        RTSPortExtPtr rtsPort = new RTSPortExt(string(portprofile->name), portlist[i], this);
        if (RTCCommonUtil::compareIgnoreCase(portType, "CorbaPort")) {
            rtsPort->isServicePort = true;
            rtsPort->isInPort = false;
            outPorts.push_back(rtsPort);
        } else {
            rtsPort->isServicePort = false;
            if (RTCCommonUtil::compareIgnoreCase(portType, "DataInPort")) {
                inPorts.push_back(rtsPort);
            } else {
                rtsPort->isInPort = false;
                outPorts.push_back(rtsPort);
            }
        }
    }

    list<RTSConnectionExt*> rtsConnectionList;
    rts_->impl->RTSCompToConnectionList(this, rtsConnectionList, 0);
    for (auto it = rtsConnectionList.begin(); it != rtsConnectionList.end(); ++it) {
        RTSConnectionExtPtr connection = (*it);
        rts_->impl->removeConnection(*it);
        RTSPortExt* sourcePort = connection->srcRTC->nameToRTSPort(connection->sourcePortName);
        RTSPortExt* targetPort = connection->targetRTC->nameToRTSPort(connection->targetPortName);
        if (sourcePort && targetPort) {
            connection->sourcePort = sourcePort;
            connection->targetPort = targetPort;
            rts_->impl->rtsConnections[RTSystemExtItem::RTSPortPair(sourcePort, targetPort)] = connection;
        }
    }

    connectionCheck();
    DDEBUG("RTSComp::setRtc End");
}

bool RTSCompExt::connectionCheck()
{
    //DDEBUG("RTSComp::connectionCheck");

    bool updated = false;

    for (auto it = inPorts.begin(); it != inPorts.end(); ++it) {
        if (connectionCheckSub(*it)) {
            updated = true;
        }
        bool isCon = (*it)->isConnected();
        if( (*it)->isConnected_ != isCon) {
            updated = true;
            (*it)->isConnected_ = isCon;
        }
    }
    for (auto it = outPorts.begin(); it != outPorts.end(); it++) {
        if (connectionCheckSub(*it)) {
            updated = true;
        }
        bool isCon = (*it)->isConnected();
        if( (*it)->isConnected_ != isCon) {
            updated = true;
            (*it)->isConnected_ = isCon;
        }
    }

    return updated;
}


bool RTSCompExt::connectionCheckSub(RTSPortExt* rtsPort)
{
    bool updated = false;

    if (isObjectAlive(rtsPort->port) == false) return updated;

    /**
       The get_port_profile() function should not be used here because
       it takes much time when the port has large data and its owner RTC is in a remote host.
       The get_connector_profiles() function does not seem to cause such a problem.
    */
    ConnectorProfileList_var connectorProfiles = rtsPort->port->get_connector_profiles();

    for (CORBA::ULong i = 0; i < connectorProfiles->length(); ++i) {
        ConnectorProfile& connectorProfile = connectorProfiles[i];
        PortServiceList& connectedPorts = connectorProfile.ports;
        for (CORBA::ULong j = 0; j < connectedPorts.length(); ++j) {
            PortService_var connectedPortRef = connectedPorts[j];
            if(CORBA::is_nil(connectedPortRef)){
                continue;
            }
            PortProfile_var connectedPortProfile;
            try {
                connectedPortProfile = connectedPortRef->get_port_profile();
            }
            catch (CORBA::SystemException& ex) {
                MessageView::instance()->putln(
                    format(_("CORBA {0} ({1}), {2} in RTSComp::connectionCheckSub()"),
                           ex._name(), ex._rep_id(), ex.NP_minorString()),
                    MessageView::WARNING);
                continue;
            }
            string portName = string(connectedPortProfile->name);
            vector<string> target;
            RTCCommonUtil::splitPortName(portName, target);
            if (target[0] == name) continue;

            string rtcPath;
            if(!getComponentPath(connectedPortRef, rtcPath)){
                continue;
            }
            RTSCompExt* targetRTC = rts_->impl->nameToRTSComp("/" + rtcPath);
            if(!targetRTC){
                continue;
            }

            //DDEBUG("targetRTC Found");
            RTSPortExt* targetPort = targetRTC->nameToRTSPort(portName);
            if (targetPort) {
                auto itr = rts_->impl->rtsConnections.find(RTSystemExtItem::RTSPortPair(rtsPort, targetPort));
                if (itr != rts_->impl->rtsConnections.end()) {
                    continue;
                }
                RTSConnectionExtPtr rtsConnection = new RTSConnectionExt(
                    string(connectorProfile.connector_id), string(connectorProfile.name),
                    name, rtsPort->name,
                    target[0], portName);
                coil::Properties properties = NVUtil::toProperties(connectorProfile.properties);
                vector<NamedValueExtPtr> propList;
                NamedValueExtPtr dataType(new NamedValueExt("dataport.dataflow_type", properties["dataport.dataflow_type"]));
                propList.push_back(dataType);
                NamedValueExtPtr subscription(new NamedValueExt("dataport.subscription_type", properties["dataport.subscription_type"]));
                rtsConnection->propList = propList;
                
                rtsConnection->srcRTC = this;
                rtsConnection->sourcePort = nameToRTSPort(rtsConnection->sourcePortName);
                rtsConnection->targetRTC = targetRTC;
                rtsConnection->targetPort = targetRTC->nameToRTSPort(rtsConnection->targetPortName);
                rtsConnection->isAlive_ = true;
                rts_->impl->rtsConnections[RTSystemExtItem::RTSPortPair(rtsPort, targetPort)] = rtsConnection;
                
                rts_->suggestFileUpdate();
                
                updated = true;
            }
        }
    }

    return updated;
}


bool RTSCompExt::getComponentPath(RTC::PortService_ptr source, std::string& out_path)
{
    PortProfile_var portprofile = source->get_port_profile();
    if(!CORBA::is_nil(portprofile->owner)){
        ComponentProfile_var cprofile;
        try {
            cprofile = portprofile->owner->get_component_profile();
        }
        catch (CORBA::SystemException& ex) {
            MessageView::instance()->putln(
                format(_("CORBA {0} ({1}), {2} in RTSComp::getComponentPath()"),
                       ex._name(), ex._rep_id(), ex.NP_minorString()),
                MessageView::WARNING);
            return false;
        }
        NVList properties = cprofile->properties;
        for(int index = 0; index < properties.length(); ++index){
            string strName(properties[index].name._ptr);
            if(strName == "naming.names"){
                const char* nvValue;
                properties[index].value >>= nvValue;
                out_path = string(nvValue);
                return true;
            }
        }
    }
    return false;
}

void RTSCompExt::moveToRelative(const QPointF& p)
{
    QPointF newPos = pos_ + p;
    if (newPos != pos_) {
        pos_ = newPos;
        rts_->suggestFileUpdate();
    }
}
//////////
void RTSystemExtItem::initializeClass(ExtensionManager* ext)
{
    DDEBUG("RTSystemItem::initializeClass");
    ItemManager& im = ext->itemManager();
    im.registerClass<RTSystemExtItem>(N_("RTSystemItem"));
    im.addCreationPanel<RTSystemExtItem>();
    im.addLoaderAndSaver<RTSystemExtItem>(
        _("RT-System"), "RTS-PROFILE-XML", "xml",
        [](RTSystemExtItem* item, const std::string& filename, std::ostream&, Item*) {
        return item->loadRtsProfile(filename);
    },
        [](RTSystemExtItem* item, const std::string& filename, std::ostream&, Item*) {
        return item->saveRtsProfile(filename);
    });
}


RTSystemExtItem::RTSystemExtItem()
{
    DDEBUG("RTSystemItem::RTSystemItem");
    impl = new RTSystemExtItemImpl(this);
}


RTSystemExtItemImpl::RTSystemExtItemImpl(RTSystemExtItem* self)
    : self(self), pollingCycle(1000)
{
    initialize();
    autoConnection = true;
}


RTSystemExtItem::RTSystemExtItem(const RTSystemExtItem& org)
    : Item(org)
{
    impl = new RTSystemExtItemImpl(this, *org.impl);
}


RTSystemExtItemImpl::RTSystemExtItemImpl(RTSystemExtItem* self, const RTSystemExtItemImpl& org)
    : self(self), pollingCycle(1000)
{
    initialize();
    autoConnection = org.autoConnection;
}

void RTSystemExtItemImpl::initialize()
{
    DDEBUG_V("RTSystemItemImpl::initialize cycle:%d", pollingCycle);
    connectionNo = 0;

    Mapping* config = AppConfig::archive()->openMapping("OpenRTM");
    vendorName = config->get("defaultVendor", "AIST");
    version = config->get("defaultVersion", "1.0.0");
    stateCheck.setSymbol(POLLING_CHECK, "Polling");
    stateCheck.setSymbol(MANUAL_CHECK, "Manual");
    checkAtLoading = true;
#if defined(OPENRTM_VERSION12)
    stateCheck.setSymbol(OBSERVER_CHECK, "Observer");
    heartBeatPeriod = config->get("heartBeatPeriod", 500);
#endif

    timer.setInterval(pollingCycle);
    timer.setSingleShot(false);
    timeOutConnection.reset(
        timer.sigTimeout().connect(
            std::bind(&RTSystemExtItemImpl::checkStatus, this)));

}

RTSystemExtItem::~RTSystemExtItem()
{
    delete impl;
}

RTSystemExtItemImpl::~RTSystemExtItemImpl()
{
    locationChangedConnection.disconnect();
}


Item* RTSystemExtItem::doDuplicate() const
{
    return new RTSystemExtItem(*this);
}

void RTSystemExtItemImpl::onLocationChanged(string host, int port)
{
    NameServerManager::instance()->getNCHelper()->setLocation(host, port);
}

void RTSystemExtItem::onActivated()
{
    impl->onActivated();
}

void RTSystemExtItemImpl::onActivated()
{
    DDEBUG("RTSystemExtItemImpl::onActivated");
    if( sigStatusUpdate.empty() == false && stateCheck.selectedIndex()==POLLING_CHECK ) {
        timer.start();
    }
}

RTSCompExt* RTSystemExtItem::nameToRTSComp(const string& name)
{
    return impl->nameToRTSComp(name);
}

RTSCompExt* RTSystemExtItemImpl::nameToRTSComp(const string& name)
{
    //DDEBUG_V("RTSystemItemImpl::nameToRTSComp:%s", name.c_str());
    map<string, RTSCompExtPtr>::iterator it = rtsComps.find(name);
    if (it == rtsComps.end())
        return 0;
    else
        return it->second.get();
}


RTSCompExt* RTSystemExtItem::addRTSComp(const string& name, const QPointF& pos)
{
    return impl->addRTSComp(name, pos);
}


RTSCompExt* RTSystemExtItemImpl::addRTSComp(const string& name, const QPointF& pos)
{
    DDEBUG_V("RTSystemItemImpl::addRTSComp:%s", name.c_str());

    if (!nameToRTSComp("/" + name + ".rtc")) {
        std::vector<NamingContextHelper::ObjectPath> pathList;
        NamingContextHelper::ObjectPath path(name, "rtc");
        pathList.push_back(path);
        NamingContextHelper* ncHelper = NameServerManager::instance()->getNCHelper();
        RTC::RTObject_ptr rtc = ncHelper->findObject<RTC::RTObject>(pathList);
        DDEBUG_V("ncHelper host:%s, port:%d", ncHelper->host().c_str(), ncHelper->port());
        if (rtc == RTC::RTObject::_nil()) {
            DDEBUG("RTSystemItemImpl::addRTSComp Failed");
            return nullptr;
        }

        string fullPath = "/" + name + ".rtc";
        RTSCompExtPtr rtsComp = new RTSCompExt(name, fullPath, rtc, self, pos, ncHelper->host().c_str(), ncHelper->port(), false);
        rtsComps[fullPath] = rtsComp;

        self->suggestFileUpdate();

        return rtsComp;
    }

    return nullptr;
}


RTSCompExt* RTSystemExtItem::addRTSComp(const  NamingContextHelper::ObjectInfo& info, const QPointF& pos)
{
    return impl->addRTSComp(info, pos);
}


RTSCompExt* RTSystemExtItemImpl::addRTSComp(const NamingContextHelper::ObjectInfo& info, const QPointF& pos)
{
    DDEBUG("RTSystemItemImpl::addRTSComp");
    timeOutConnection.block();

    string fullPath = info.getFullPath();

    if (!nameToRTSComp(fullPath)) {
        std::vector<NamingContextHelper::ObjectPath> target = info.fullPath_;
        auto ncHelper = NameServerManager::instance()->getNCHelper();
        if(info.isRegisteredInRtmDefaultNameServer_) {
            NameServerInfo ns = RTCCommonUtil::getManagerAddress();
            ncHelper->setLocation(ns.hostAddress, ns.portNo);
        } else {
            ncHelper->setLocation(info.hostAddress_, info.portNo_);
        }
        RTC::RTObject_ptr rtc = ncHelper->findObject<RTC::RTObject>(target);
        if (!isObjectAlive(rtc)) {
            CORBA::release(rtc);
            rtc = nullptr;
        }

        RTSCompExtPtr rtsComp = new RTSCompExt(info.id_, fullPath, rtc, self, pos, info.hostAddress_, info.portNo_, info.isRegisteredInRtmDefaultNameServer_);
        rtsComps[fullPath] = rtsComp;

        self->suggestFileUpdate();

        return rtsComp.get();
    }
    timeOutConnection.unblock();

    return nullptr;
}

void RTSystemExtItem::deleteRTSComp(const string& name)
{
    impl->deleteRTSComp(name);
}


void RTSystemExtItemImpl::deleteRTSComp(const string& name)
{
    timeOutConnection.block();

    if (rtsComps.erase(name) > 0) {
        self->suggestFileUpdate();
    }

    timeOutConnection.unblock();
}


bool RTSystemExtItem::compIsAlive(RTSCompExt* rtsComp)
{
    return impl->compIsAlive(rtsComp);
}


bool RTSystemExtItemImpl::compIsAlive(RTSCompExt* rtsComp)
{
    //DDEBUG("RTSystemItemImpl::compIsAlive");
    if (rtsComp->isAlive_ && rtsComp->rtc_ && rtsComp->rtc_ != nullptr) {
        if (isObjectAlive(rtsComp->rtc_)) {
            return true;
        } else {
            rtsComp->setRtc(nullptr);
            return false;
        }
    } else {
        //DDEBUG_V("Full Path = %s", rtsComp->fullPath.c_str());
        QStringList nameList = QString::fromStdString(rtsComp->fullPath).split("/");
        std::vector<NamingContextHelper::ObjectPath> pathList;
        for (int index = 0; index < nameList.count(); index++) {
            QString elem = nameList[index];
            if (elem.length() == 0) continue;
            QStringList elemList = elem.split(".");
            if (elemList.size() != 2) return false;
            NamingContextHelper::ObjectPath path(elemList[0].toStdString(), elemList[1].toStdString());
            pathList.push_back(path);
        }

        std::string host;
        int port;
        if(rtsComp->isDefaultNS) {
            NameServerInfo ns = RTCCommonUtil::getManagerAddress();
            host = ns.hostAddress;
            port = ns.portNo;
        } else {
            host = rtsComp->hostAddress;
            port = rtsComp->portNo;
        }
        DDEBUG_V("host:%s, port:%d, default:%d", host.c_str(), port, rtsComp->isDefaultNS);
        NameServerManager::instance()->getNCHelper()->setLocation(host, port);
        RTC::RTObject_ptr rtc = NameServerManager::instance()->getNCHelper()->findObject<RTC::RTObject>(pathList);
        if (!isObjectAlive(rtc)) {
            //DDEBUG("RTSystemItemImpl::compIsAlive NOT Alive");
            return false;
        } else {
            DDEBUG("RTSystemItemImpl::compIsAlive Alive");
            rtsComp->setRtc(rtc);
            if (autoConnection) {
                list<RTSConnectionExt*> rtsConnectionList;
                RTSCompToConnectionList(rtsComp, rtsConnectionList, 0);
                for (auto it = rtsConnectionList.begin(); it != rtsConnectionList.end(); it++) {
                    auto connection = *it;
                    connection->connect();
                }
                DDEBUG("autoConnection End");
            }
            return true;
        }
    }
}


string RTSystemExtItemImpl::getConnectionNumber()
{
    stringstream ss;
    ss << connectionNo;
    connectionNo++;
    return ss.str();
}


RTSConnectionExt* RTSystemExtItem::addRTSConnection
(const std::string& id, const std::string& name, RTSPortExt* sourcePort, RTSPortExt* targetPort, const std::vector<NamedValueExtPtr>& propList, const Vector2 pos[])
{
    bool setPos = true;
    if (!pos) setPos = false;
    return impl->addRTSConnection(id, name, sourcePort, targetPort, propList, setPos, pos);
}


RTSConnectionExt* RTSystemExtItemImpl::addRTSConnection
(const string& id, const string& name,
 RTSPortExt* sourcePort, RTSPortExt* targetPort, const std::vector<NamedValueExtPtr>& propList,
 const bool setPos, const Vector2 pos[])
{
    DDEBUG("RTSystemItemImpl::addRTSConnection");
    timeOutConnection.block();

    bool updated = false;

    RTSConnectionExt* rtsConnection_;
    auto it = rtsConnections.find(RTSystemExtItem::RTSPortPair(sourcePort, targetPort));

    if (it != rtsConnections.end()) {
        rtsConnection_ = it->second;;

    } else {
        RTSConnectionExtPtr rtsConnection =
            new RTSConnectionExt(
                id, name,
                sourcePort->rtsComp->name, sourcePort->name,
                targetPort->rtsComp->name, targetPort->name);

        rtsConnection->srcRTC = sourcePort->rtsComp;
        rtsConnection->sourcePort = sourcePort;
        rtsConnection->targetRTC = targetPort->rtsComp;
        rtsConnection->targetPort = targetPort;
        rtsConnection->propList = propList;

        if (setPos) {
            rtsConnection->setPosition(pos);
        }

        rtsConnections[RTSystemExtItem::RTSPortPair(sourcePort, targetPort)] = rtsConnection;
        rtsConnection_ = rtsConnection;

        updated = true;
    }

    if (!CORBA::is_nil(sourcePort->port) && !sourcePort->port->_non_existent() &&
       !CORBA::is_nil(targetPort->port) && !targetPort->port->_non_existent()) {
        if (rtsConnection_->connect()) {
            updated = true;
        }
    }

    if (rtsConnection_->id.empty()) {
        rtsConnection_->id = "NoConnection_" + getConnectionNumber();
        updated = true;
    }

    if (updated) {
        self->suggestFileUpdate();
    }

    timeOutConnection.unblock();
    return rtsConnection_;
}


RTSConnectionExt* RTSystemExtItemImpl::addRTSConnectionName
(const string& id, const string& name,
 const string& sourceCompName, const string& sourcePortName,
 const string& targetCompName, const string& targetPortName,
 const string& dataflow, const string& subscription,
 const bool setPos, const Vector2 pos[])
{
    string sourceId = "/" + sourceCompName + ".rtc";
    RTSPortExt* sourcePort = 0;
    RTSCompExt* sourceRtc = nameToRTSComp(sourceId);
    if (sourceRtc) {
        sourcePort = sourceRtc->nameToRTSPort(sourcePortName);
    }

    string targetId = "/" + targetCompName + ".rtc";
    RTSPortExt* targetPort = 0;
    RTSCompExt* targetRtc = nameToRTSComp(targetId);
    if (targetRtc) {
        targetPort = targetRtc->nameToRTSPort(targetPortName);
    }
    if (sourcePort && targetPort) {
        vector<NamedValueExtPtr> propList;
        NamedValueExtPtr paramDataFlow(new NamedValueExt("dataport.dataflow_type", dataflow));
        propList.push_back(paramDataFlow);
        NamedValueExtPtr paramSubscription(new NamedValueExt("dataport.subscription_type", subscription));
        propList.push_back(paramSubscription);
        NamedValueExtPtr sinterfaceProp(new NamedValueExt("dataport.interface_type", "corba_cdr"));
        propList.push_back(sinterfaceProp);

        return addRTSConnection(id, name, sourcePort, targetPort, propList, setPos, pos);
    }

    return nullptr;
}


bool RTSystemExtItem::connectionCheck()
{
    return impl->connectionCheck();
}


bool RTSystemExtItemImpl::connectionCheck()
{
    bool updated = false;

    for (auto it = rtsConnections.begin(); it != rtsConnections.end(); it++) {
        const RTSystemExtItem::RTSPortPair& ports = it->first;
        if (ports(0)->isConnectedWith(ports(1))) {
            if (!it->second->isAlive_) {
                it->second->isAlive_ = true;
                updated = true;
            }
        } else {
            if (it->second->isAlive_) {
                it->second->isAlive_ = false;
                updated = true;
            }
        }
    }

    for (auto it = rtsComps.begin(); it != rtsComps.end(); it++) {
        if (it->second->connectionCheck()) {
            updated = true;
        }
    }

    return updated;
}


void RTSystemExtItem::RTSCompToConnectionList
(const RTSCompExt* rtsComp, list<RTSConnectionExt*>& rtsConnectionList, int mode)
{
    impl->RTSCompToConnectionList(rtsComp, rtsConnectionList, mode);
}


void RTSystemExtItemImpl::RTSCompToConnectionList
(const RTSCompExt* rtsComp, list<RTSConnectionExt*>& rtsConnectionList, int mode)
{
    for (RTSystemExtItem::RTSConnectionMap::iterator it = rtsConnections.begin();
            it != rtsConnections.end(); it++) {
        switch (mode) {
            case 0:
            default:
                if (it->second->sourceRtcName == rtsComp->name || it->second->targetRtcName == rtsComp->name)
                    rtsConnectionList.push_back(it->second);
                break;
            case 1:
                if (it->second->sourceRtcName == rtsComp->name)
                    rtsConnectionList.push_back(it->second);
                break;
            case 2:
                if (it->second->targetRtcName == rtsComp->name)
                    rtsConnectionList.push_back(it->second);
                break;
        }
    }
}


map<string, RTSCompExtPtr>& RTSystemExtItem::rtsComps()
{
    return impl->rtsComps;
}


RTSystemExtItem::RTSConnectionMap& RTSystemExtItem::rtsConnections()
{
    return impl->rtsConnections;
}


void RTSystemExtItem::disconnectAndRemoveConnection(RTSConnectionExt* connection)
{
    connection->disconnect();
    impl->removeConnection(connection);
}


void RTSystemExtItemImpl::removeConnection(RTSConnectionExt* connection)
{
    RTSystemExtItem::RTSPortPair pair(connection->sourcePort, connection->targetPort);
    if (rtsConnections.erase(pair) > 0) {
        self->suggestFileUpdate();
    }
}


void RTSystemExtItem::doPutProperties(PutPropertyFunction& putProperty)
{
    impl->doPutProperties(putProperty);
}


void RTSystemExtItemImpl::doPutProperties(PutPropertyFunction& putProperty)
{
    DDEBUG("RTSystemItemImpl::doPutProperties");
    putProperty(_("Auto Connection"), autoConnection, changeProperty(autoConnection));
    putProperty(_("Vendor Name"), vendorName, changeProperty(vendorName));
    putProperty(_("Version"), version, changeProperty(version));
    putProperty(_("State Check"), stateCheck,
                [&](int value) { setStateCheckMethod(value); return true; });
    putProperty(_("Polling Cycle"), pollingCycle,
                [&](int value) { changePollingPeriod(value); return true; });
    putProperty(_("CheckAtLoading"), checkAtLoading, changeProperty(checkAtLoading));

#if defined(OPENRTM_VERSION12)
    putProperty(_("HeartBeat Period"), heartBeatPeriod, changeProperty(heartBeatPeriod));
#endif
}

void RTSystemExtItemImpl::changePollingPeriod(int value)
{
    DDEBUG_V("RTSystemItemImpl::changePollingPeriod=%d", value);
    if (pollingCycle != value) {
        pollingCycle = value;

        bool isStarted = timer.isActive();
        if( isStarted ) timer.stop();
        timer.setInterval(value);
        if( isStarted ) timer.start();
    }
}

void RTSystemExtItemImpl::setStateCheckMethod(int value)
{
    DDEBUG_V("RTSystemItemImpl::setStateCheckMethod=%d", value);
    stateCheck.selectIndex(value);
    changeStateCheck();
}

void RTSystemExtItemImpl::setStateCheckMethodByString(const string& value)
{
    DDEBUG_V("RTSystemItemImpl::setStateCheckMethodByString=%s", value.c_str());
    stateCheck.select(value);
    DDEBUG_V("RTSystemItemImpl::setStateCheckMethodByString=%d", stateCheck.selectedIndex());
    changeStateCheck();
}

void RTSystemExtItemImpl::changeStateCheck()
{
    int state = stateCheck.selectedIndex();
    DDEBUG_V("RTSystemItemImpl::changeStateCheck=%d", state);
    switch (state) {
        case MANUAL_CHECK:
            timer.stop();
            break;
#if defined(OPENRTM_VERSION12)
        case OBSERVER_CHECK:
            break;
#endif
        default:
            timer.start();
            break;
    }
}

bool RTSystemExtItem::loadRtsProfile(const string& filename)
{
    DDEBUG_V("RTSystemItem::loadRtsProfile=%s", filename.c_str());
    ProfileHandlerExt::getRtsProfileInfo(filename, impl->vendorName, impl->version);
    if (ProfileHandlerExt::restoreRtsProfile(filename, this)) {
        notifyUpdate();
        return true;
    }
    return false;
}


bool RTSystemExtItem::saveRtsProfile(const string& filename)
{
    return impl->saveRtsProfile(filename);
}


bool RTSystemExtItemImpl::saveRtsProfile(const string& filename)
{
    if (vendorName.empty()) {
        vendorName = "Choreonoid";
    }
    if (version.empty()) {
        version = "1.0.0";
    }
    string systemId = "RTSystem:" + vendorName + ":" + self->name() + ":" + version;
    ProfileHandlerExt::saveRtsProfile(filename, systemId, rtsComps, rtsConnections, MessageView::mainInstance()->cout());

    return true;
}

void RTSystemExtItem::setVendorName(const std::string& name)
{
    impl->vendorName = name;
}


void RTSystemExtItem::setVersion(const std::string& version)
{
    impl->version = version;
}

int RTSystemExtItem::stateCheck() const
{
    return impl->stateCheck.selectedIndex();
}

void RTSystemExtItem::checkStatus()
{
    impl->checkStatus();
}

void RTSystemExtItemImpl::checkStatus()
{
    DDEBUG("RTSystemExtItemImpl::checkStatus");
    if( sigStatusUpdate.empty() ) {
        timer.stop();
    }

    bool modified = false;

    for (auto it = rtsComps.begin(); it != rtsComps.end(); it++) {
        if (compIsAlive(it->second)) {
            if (!it->second->isAlive_) {
                modified = true;
            }
            it->second->isAlive_ = true;
            RTC_STATUS status = it->second->getRTCState();
            DDEBUG_V("RTC State : %d, %d", it->second->rtc_status_, status);
            if( status != it->second->rtc_status_ ) {
                modified = true;
                it->second->rtc_status_ = status;
            }
        } else {
            if (it->second->isAlive_) {
                modified = true;
            }
            it->second->isAlive_ = false;
        }
    }
    //
    if (connectionCheck()) {
        modified = true;
    }
    DDEBUG_V("RTSystemExtItemImpl::checkStatus End : %d", modified);
    sigStatusUpdate(modified);
}

bool RTSystemExtItem::isCheckAtLoading()
{
    return impl->checkAtLoading;
}
///////////
bool RTSystemExtItem::store(Archive& archive)
{
    if (overwrite()) {
        archive.writeRelocatablePath("filename", filePath());
        archive.write("format", fileFormat());

        archive.write("autoConnection", impl->autoConnection);
        archive.write("pollingCycle", impl->pollingCycle);
        archive.write("stateCheck", impl->stateCheck.selectedSymbol());
        archive.write("checkAtLoading", impl->checkAtLoading);

#if defined(OPENRTM_VERSION12)
        archive.write("HeartBeatPeriod", impl->heartBeatPeriod);
#endif
        return true;
    }

    return true;
}


bool RTSystemExtItem::restore(const Archive& archive)
{
    DDEBUG("RTSystemExtItem::restore");

    if (archive.read("autoConnection", impl->autoConnection) == false) {
        archive.read("AutoConnection", impl->autoConnection);
    }

    int pollingCycle = 1000;
    if( archive.read("pollingCycle", pollingCycle)==false) {
        archive.read("PollingCycle", pollingCycle);
    }
    if( archive.read("checkAtLoading", impl->checkAtLoading)==false) {
        archive.read("CheckAtLoading", impl->checkAtLoading);
    }

    impl->changePollingPeriod(pollingCycle);

#if defined(OPENRTM_VERSION12)
    if(archive.read("HeartBeatPeriod", impl->heartBeatPeriod) == false) {
        archive.read("heartBeatPeriod", impl->heartBeatPeriod);
    }
#endif

    /**
       The contents of RTSystemItem must be loaded after all the items are restored
       so that the states of the RTCs created by other items can be loaded.
    */
    std::string filename, formatId;
    if (archive.readRelocatablePath("filename", filename)) {
        if (archive.read("format", formatId)) {
            archive.addPostProcess(
                [this, filename, formatId]() { load(filename, formatId); });
        }
    } else {
        // old format data contained in a project file
        archive.addPostProcess([&]() { impl->restoreRTSystem(archive); });
    }

    string stateCheck;
    if (archive.read("stateCheck", stateCheck) == false) {
        archive.read("StateCheck", stateCheck);
    }
    if(stateCheck.empty()==false) {
        DDEBUG_V("StateCheck:%s", stateCheck.c_str());
        impl->setStateCheckMethodByString(stateCheck);
        archive.addPostProcess([&]() { impl->changeStateCheck(); });
    }

    return true;
}


void RTSystemExtItemImpl::restoreRTSystem(const Archive& archive)
{
    DDEBUG("RTSystemItemImpl::restoreRTSystem");

    const Listing& compListing = *archive.findListing("RTSComps");
    if (compListing.isValid()) {
        for (int i = 0; i < compListing.size(); i++) {
            const Mapping& compMap = *compListing[i].toMapping();
            string name;
            Vector2 pos;
            compMap.read("name", name);
            read(compMap, "pos", pos);

            vector<pair<string, bool>> inPorts;
            vector<pair<string, bool>> outPorts;
            const Listing& inportListing = *compMap.findListing("InPorts");
            if (inportListing.isValid()) {
                for (int i = 0; i < inportListing.size(); i++) {
                    const Mapping& inMap = *inportListing[i].toMapping();
                    string portName;
                    bool isServicePort;
                    inMap.read("name", portName);
                    inMap.read("isServicePort", isServicePort);
                    inPorts.push_back(make_pair(portName, isServicePort));
                }
            }
            const Listing& outportListing = *compMap.findListing("OutPorts");
            if (outportListing.isValid()) {
                for (int i = 0; i < outportListing.size(); i++) {
                    const Mapping& outMap = *outportListing[i].toMapping();
                    string portName;
                    bool isServicePort;
                    outMap.read("name", portName);
                    outMap.read("isServicePort", isServicePort);
                    outPorts.push_back(make_pair(portName, isServicePort));
                }
            }
            restoreRTSComp(name, pos, inPorts, outPorts);
        }
    }

    if (autoConnection) {
        const Listing& connectionListing = *archive.findListing("RTSConnections");
        if (connectionListing.isValid()) {
            for (int i = 0; i < connectionListing.size(); i++) {
                const Mapping& connectMap = *connectionListing[i].toMapping();
                string name, sR, sP, tR, tP, dataflow, subscription;
                connectMap.read("name", name);
                connectMap.read("sourceRtcName", sR);
                connectMap.read("sourcePortName", sP);
                connectMap.read("targetRtcName", tR);
                connectMap.read("targetPortName", tP);
                connectMap.read("dataflow", dataflow);
                connectMap.read("subscription", subscription);
                VectorXd p(12);
                bool readPos = false;
                Vector2 pos[6];
                if (read(connectMap, "position", p)) {
                    readPos = true;
                    for (int i = 0; i < 6; i++) {
                        pos[i] << p(2 * i), p(2 * i + 1);
                    }
                }

                addRTSConnectionName("", name, sR, sP, tR, tP, dataflow, subscription, readPos, pos);
            }
        }
    }

    if (checkAtLoading) {
        checkStatus();
    }

    self->notifyUpdate();
    DDEBUG("RTSystemItemImpl::restoreRTSystem End");
}


void RTSystemExtItemImpl::restoreRTSComp(const string& name, const Vector2& pos,
        const vector<pair<string, bool>>& inPorts, const vector<pair<string, bool>>& outPorts)
{
    DDEBUG("RTSystemItemImpl::restoreRTSComp");

    RTSCompExt* comp = addRTSComp(name, QPointF(pos(0), pos(1)));
    if (comp == 0) return;
    if (!comp->rtc_) {
        comp->inPorts.clear();
        comp->outPorts.clear();
        for (int i = 0; i < inPorts.size(); i++) {
            RTSPortExtPtr rtsPort = new RTSPortExt(inPorts[i].first, 0, comp);
            rtsPort->isInPort = true;
            rtsPort->isServicePort = inPorts[i].second;
            comp->inPorts.push_back(rtsPort);
        }
        for (int i = 0; i < outPorts.size(); i++) {
            RTSPortExtPtr rtsPort = new RTSPortExt(outPorts[i].first, 0, comp);
            rtsPort->isInPort = false;
            rtsPort->isServicePort = outPorts[i].second;
            comp->outPorts.push_back(rtsPort);
        }
    }
    DDEBUG("RTSystemItemImpl::restoreRTSComp End");
}

SignalProxy<void(bool)> RTSystemExtItem::sigStatusUpdate()
{
    return impl->sigStatusUpdate;
}
