/**
   \file
   \author Rafael Cisneros
*/

#include "TactileSensor.h"

#include <iostream>  // Rafa, this is temporal

using namespace cnoid;


TactileSensor::TactileSensor()
{
  forceData_ = std::make_shared<ForceData>();
}


TactileSensor::TactileSensor(const TactileSensor& org, bool copyStateOnly)
  : Device(org, copyStateOnly)
{
  copyStateFrom(org);
}


const char* TactileSensor::typeName()
{
  return "TactileSensor";
}


void TactileSensor::copyStateFrom(const TactileSensor& other)
{
  forceData_ = other.forceData_;
}


void TactileSensor::copyStateFrom(const DeviceState& other)
{
  if (typeid(other) != typeid(TactileSensor))
    throw std::invalid_argument("Type mismatch in the Device::copyStateFrom function");
  
  copyStateFrom(static_cast<const TactileSensor&>(other));
}


// Device* TactileSensor::clone() const
//Device* TactileSensor::doClone(BodyCloneMap*) const
Referenced* TactileSensor::doClone(CloneMap*) const
{
  return new TactileSensor(*this, false);
}


DeviceState* TactileSensor::cloneState() const
{
  return new TactileSensor(*this, true);
}


void TactileSensor::forEachActualType(std::function<bool(const std::type_info& type)> func)
{
  if (!func(typeid(TactileSensor)))
    Device::forEachActualType(func);
}


void TactileSensor::clearState()
{
  forceData_->clear();
}


int TactileSensor::stateSize() const
{
  // std::cout << "Rafa, in TactileSensor::stateSize, forceData_->size() = " << forceData_->size() << std::endl;
  
  return forceData_->size() * 5;
  // return forceData_->size() * 2;  // Rafa, temporal implementation
}


const double* TactileSensor::readState(const double* buf)
{
  return buf;
}


double* TactileSensor::writeState(double* out_buf) const
{
  for (size_t i = 0; i < forceData_->size(); i++) {
    Eigen::Map<Vector2>(out_buf) << (*forceData_)[i].first;
    //Eigen::Map<Vector2>(out_buf) << (*forceData_)[i];  // Rafa, temporal implementation
    out_buf = out_buf + 2;
    Eigen::Map<Vector3>(out_buf) << (*forceData_)[i].second;
    out_buf = out_buf + 3;
  }
  
  return out_buf;
}
