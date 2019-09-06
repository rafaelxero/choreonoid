/*!
  @file
  @author Shin'ichiro Nakaoka
*/

#ifndef CNOID_BODY_COMPOSITE_IK_H
#define CNOID_BODY_COMPOSITE_IK_H

#include "InverseKinematics.h"
#include <cnoid/Referenced>
#include <vector>
#include <memory>
#include "exportdecl.h"

namespace cnoid {

class Body;
class Link;
class LinkTraverse;
class JointPath;

class CNOID_EXPORT CompositeIK : public InverseKinematics
{
public:
    CompositeIK();
    CompositeIK(Body* body, Link* targetLink);

    void reset(Body* body, Link* targetLink);
    bool addBaseLink(Link* link);

    Body* body() { return body_; }
    Link* targetLink() { return targetLink_; }
    int numJointPaths() const { return paths.size(); }
    std::shared_ptr<JointPath> jointPath(int index) const { return paths[index]; }
    Link* baseLink(int index) const;

    void setMaxIKerror(double e);
    bool hasAnalyticalIK() const { return hasAnalyticalIK_; }

    virtual bool calcInverseKinematics(const Position& T) override;
    virtual bool calcRemainingPartForwardKinematicsForInverseKinematics() override;

    //! deprecated
    bool calcInverseKinematics(const Vector3& p, const Matrix3& R) {
        return InverseKinematics::calcInverseKinematics(p, R);
    }

private:
    ref_ptr<Body> body_;
    Link* targetLink_;
    std::vector<std::shared_ptr<JointPath>> paths;
    std::vector<double> q0;
    std::shared_ptr<LinkTraverse> remainingLinkTraverse;
    bool hasAnalyticalIK_;
};

}

#endif
