/*!
  @file
  @author Shin'ichiro Nakaoka
*/

#ifndef CNOID_BODY_PLUGIN_SENSOR_VISUALIZER_ITEM_H
#define CNOID_BODY_PLUGIN_SENSOR_VISUALIZER_ITEM_H

#include <cnoid/Item>
#include "exportdecl.h"

namespace cnoid {

class SensorVisualizerItemImpl;

class CNOID_EXPORT SensorVisualizerItem : public Item
{
public:
    static void initializeClass(ExtensionManager* ext);

    SensorVisualizerItem();
    SensorVisualizerItem(const SensorVisualizerItem& org);
    virtual ~SensorVisualizerItem();

protected:
    virtual Item* doDuplicate() const;
    virtual void onPositionChanged();
    virtual void onDisconnectedFromRoot();
    virtual bool store(Archive& archive);
    virtual bool restore(const Archive& archive);


private:
    SensorVisualizerItemImpl* impl;
};
        
typedef ref_ptr<SensorVisualizerItem> SensorVisualizerItemPtr;

}

#endif
