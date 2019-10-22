#ifndef CNOID_UTIL_COORDINATE_FRAME_H
#define CNOID_UTIL_COORDINATE_FRAME_H

#include "GeneralId.h"
#include <cnoid/CloneMappableReferenced>
#include <cnoid/EigenTypes>
#include <string>
#include "exportdecl.h"

namespace cnoid {

class CoordinateFrameSet;
class Mapping;

class CNOID_EXPORT CoordinateFrame : public CloneMappableReferenced
{
public:
    EIGEN_MAKE_ALIGNED_OPERATOR_NEW

    CoordinateFrame();
    CoordinateFrame(const GeneralId& id);
    CoordinateFrame(const CoordinateFrame& org);

    /**
       This constructor is used in a special case where the frame is not actually
       contained in the owner, but the frame needs to set the owner formally.
    */
    CoordinateFrame(const GeneralId& id, CoordinateFrameSet* owner);

    const GeneralId& id() const { return id_; }

    static GeneralId defaultFrameId() { return GeneralId(0); }

    const Position& T() const { return T_; }
    Position& T() { return T_; }

    const Position& position() const { return T_; }
    void setPosition(const Position& T) { T_ = T; }

    enum Mode { Relative, Absolute };

    bool isRelative() const { return (mode_ == Relative); }
    bool isAbsolute() const { return (mode_ == Absolute); }
    void setMode(Mode mode){ mode_ = mode; }

    const std::string& note() const { return note_; }
    void setNote(const std::string& note) { note_ = note; }

    CoordinateFrameSet* ownerFrameSet() const;

    bool read(const Mapping& archive);
    bool write(Mapping& archive) const;

protected:
    virtual Referenced* doClone(CloneMap* cloneMap) const override;

private:
    Position T_;
    GeneralId id_;
    Mode mode_;
    std::string note_;
    weak_ref_ptr<CoordinateFrameSet> ownerFrameSet_;

    friend class CoordinateFrameSet;
};

typedef ref_ptr<CoordinateFrame> CoordinateFramePtr;

}

#endif