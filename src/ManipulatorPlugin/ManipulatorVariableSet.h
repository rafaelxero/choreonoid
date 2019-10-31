#ifndef CNOID_MANIPULATOR_PLUGIN_MANIPULATOR_VARIABLE_SET_H
#define CNOID_MANIPULATOR_PLUGIN_MANIPULATOR_VARIABLE_SET_H

#include "ManipulatorVariable.h"
#include <cnoid/CloneMappableReferenced>
#include <vector>
#include <string>
#include "exportdecl.h"

namespace cnoid {

class CNOID_EXPORT ManipulatorVariableSet : public CloneMappableReferenced
{
public:
    ManipulatorVariableSet* clone(){
        return static_cast<ManipulatorVariableSet*>(doClone(nullptr));
    }
    ManipulatorVariableSet* clone(CloneMap& cloneMap){
        return static_cast<ManipulatorVariableSet*>(doClone(&cloneMap));
    }

    const std::string& name() const { return name_; }

    virtual void setName(const std::string& name);

    virtual int getNumVariables() const = 0;

    virtual ManipulatorVariable* getVariableAt(int index) const = 0;
    
    virtual ManipulatorVariable* findVariable(const GeneralId& id) const = 0;

    virtual ManipulatorVariable* findOrCreateVariable(
        const GeneralId& id, const ManipulatorVariable::Value& defaultValue) = 0;

    virtual std::vector<ManipulatorVariablePtr> getFindableVariableLists() const = 0;

    virtual bool contains(const ManipulatorVariableSet* variableSet) const = 0;

protected:
    ManipulatorVariableSet();
    ManipulatorVariableSet(const ManipulatorVariableSet& org);
    void setManipulatorVariableId(ManipulatorVariable* variable, const GeneralId& id);
    void setManipulatorVariableOwner(ManipulatorVariable* variable, ManipulatorVariableSet* owner);

private:
    std::string name_;
};

typedef ref_ptr<ManipulatorVariableSet> ManipulatorVariableSetPtr;

}

#endif
