/**
   @file
   @author Shin'ichiro Nakaoka
*/

#include "SmokeDevice.h"
#include "SceneSmoke.h"
#include <cnoid/YAMLBodyLoader>
#include <cnoid/SceneDevice>
#include <cnoid/EigenUtil>

using namespace std;
using namespace cnoid;

namespace {

YAMLBodyLoader::NodeTypeRegistration
registerSmokeDevice(
    "SmokeDevice",
    [](YAMLBodyLoader& loader, Mapping& node){
        SmokeDevicePtr smoke = new SmokeDevice;
        smoke->particleSystem().readParameters(loader.sceneReader(), node);
        return loader.readDevice(smoke, node);
    });

SceneDevice::FactoryRegistration<SmokeDevice>
registerSceneSmokeDeviceFactory(
    [](Device* device){
        auto smokeDevice = static_cast<SmokeDevice*>(device);
        auto sceneSmoke = new SceneSmoke;
        auto sceneDevice = new SceneDevice(device, sceneSmoke);

        sceneDevice->setFunctionOnStateChanged(
            [sceneSmoke, smokeDevice](){
                sceneSmoke->particleSystem() = smokeDevice->particleSystem();
                sceneSmoke->notifyUpdate();
            });
    
        sceneDevice->setFunctionOnTimeChanged(
            [sceneSmoke](double time){
                sceneSmoke->setTime(time);
                sceneSmoke->notifyUpdate();
            });
            
        return sceneDevice;
    });

}


SmokeDevice::SmokeDevice()
{
    on_ = true;

    auto& ps = particleSystem_;
    ps.setLifeTime(5.0f);
    ps.setParticleSize(0.06f);
    ps.setNumParticles(2000);
    ps.setAcceleration(Vector3f(0.0f, 0.0f, 0.04f));
    ps.setEmissionRange(radian(120.0f));
}


SmokeDevice::SmokeDevice(const SmokeDevice& org, bool copyStateOnly)
    : Device(org, copyStateOnly),
      on_(org.on_),
      particleSystem_(org.particleSystem_)
{

}


const char* SmokeDevice::typeName()
{
    return "SmokeDevice";
}


void SmokeDevice::copyStateFrom(const SmokeDevice& other)
{
    on_ = other.on_;
    particleSystem_ = other.particleSystem_;
}


void SmokeDevice::copyStateFrom(const DeviceState& other)
{
    if(typeid(other) != typeid(SmokeDevice)){
        throw std::invalid_argument("Type mismatch in the Device::copyStateFrom function");
    }
    copyStateFrom(static_cast<const SmokeDevice&>(other));
}


DeviceState* SmokeDevice::cloneState() const
{
    return new SmokeDevice(*this, false);
}


Device* SmokeDevice::clone() const
{
    return new SmokeDevice(*this);
}


void SmokeDevice::forEachActualType(std::function<bool(const std::type_info& type)> func)
{
    if(!func(typeid(SmokeDevice))){
        Device::forEachActualType(func);
    }
}


int SmokeDevice::stateSize() const
{
    return 6;
}


const double* SmokeDevice::readState(const double* buf)
{
    int i = 0;
    auto& ps = particleSystem_;

    on_ = buf[i++];
    ps.setNumParticles(buf[i++]);
    ps.setAcceleration(Vector3f(buf[i], buf[i+1], buf[i+2]));
    i += 3;
    ps.setEmissionRange(buf[i++]);
    
    return buf + i;
}


double* SmokeDevice::writeState(double* out_buf) const
{
    int i = 0;
    auto& ps = particleSystem_;
    
    out_buf[i++] = on_ ? 1.0 : 0.0;
    out_buf[i++] = ps.numParticles();
    out_buf[i++] = ps.acceleration()[0];
    out_buf[i++] = ps.acceleration()[1];
    out_buf[i++] = ps.acceleration()[2];
    out_buf[i++] = ps.emissionRange();

    return out_buf + i;
}
