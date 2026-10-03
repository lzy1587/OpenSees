#include "VSFSIBBreakaway.h"

#include <Channel.h>
#include <FEM_ObjectBroker.h>
#include <Information.h>
#include <MaterialResponse.h>
#include <OPS_Globals.h>
#include <OPS_Stream.h>
#include <Vector.h>
#include <classTags.h>
#include <elementAPI.h>

#include <algorithm>
#include <cmath>
#include <cstring>

// Assign this tag in the target OpenSees source tree before building.
#ifndef MAT_TAG_VSFSIBBreakaway
#error "Register a unique MAT_TAG_VSFSIBBreakaway in OpenSees classTags.h"
#endif

VSFSIBBreakaway::VSFSIBBreakaway(int tag, double kr, double qs, double qk)
    : UniaxialMaterial(tag, MAT_TAG_VSFSIBBreakaway), kr_(kr), qs_(qs), qk_(qk) {
    revertToStart();
}

VSFSIBBreakaway::VSFSIBBreakaway()
    : UniaxialMaterial(0, MAT_TAG_VSFSIBBreakaway), kr_(0.0), qs_(0.0), qk_(0.0) {
    revertToStart();
}

double VSFSIBBreakaway::slidingMagnitude(double distance) const {
    // InterfaceState uses max(transition_disp, 1e-12), with transition_disp=0.
    const double ratio = std::min(std::max(distance / 1.0e-12, 0.0), 1.0);
    return qs_ - (qs_ - qk_) * ratio;
}

int VSFSIBBreakaway::setTrialStrain(double strain, double) {
    if (kr_ <= 0.0 || qs_ <= 0.0 || qk_ < 0.0 || qk_ > qs_ || !std::isfinite(strain))
        return -1;

    trial_ = committed_;
    const double du = strain - committed_.strain;
    if (std::abs(du) < 1.0e-14)
        return 0;  // Matches InterfaceState.update's no-increment early return.
    trial_.strain = strain;

    if (committed_.mode == SLIDE && du * committed_.direction > 0.0) {
        trial_.slideDistance += std::abs(du);
        trial_.stress = committed_.direction * slidingMagnitude(trial_.slideDistance);
        trial_.slip = strain - trial_.stress / kr_;
        trial_.rubberDef = trial_.stress / kr_;
        trial_.tangent = 0.0;
    } else {
        if (committed_.mode == SLIDE) {
            // Reversal re-sticks at the committed spring extension, not at the
            // global zero-deformation envelope.
            trial_.mode = STICK;
            trial_.slip = committed_.strain - committed_.stress / kr_;
            trial_.direction = 0;
            trial_.slideDistance = 0.0;
        }
        const double stickForce = kr_ * (strain - trial_.slip);
        if (std::abs(stickForce) <= qs_) {
            trial_.stress = stickForce;
            trial_.rubberDef = strain - trial_.slip;
            trial_.tangent = kr_;
        } else {
            trial_.mode = SLIDE;
            trial_.direction = stickForce > 0.0 ? 1 : -1;
            const double overshoot = std::max(std::abs(stickForce) - qs_, 0.0);
            trial_.slideDistance = overshoot / kr_;
            trial_.stress = trial_.direction * slidingMagnitude(trial_.slideDistance);
            trial_.rubberDef = trial_.stress / kr_;
            trial_.slip = strain - trial_.rubberDef;
            trial_.tangent = 0.0;
        }
    }
    trial_.cumulativeSlip = committed_.cumulativeSlip +
                            std::abs(trial_.slip - committed_.slip);
    return 0;
}

double VSFSIBBreakaway::getStrain() { return trial_.strain; }
double VSFSIBBreakaway::getStress() { return trial_.stress; }
double VSFSIBBreakaway::getTangent() { return trial_.tangent; }
double VSFSIBBreakaway::getInitialTangent() { return kr_; }

int VSFSIBBreakaway::commitState() {
    committed_ = trial_;
    return 0;
}

int VSFSIBBreakaway::revertToLastCommit() {
    trial_ = committed_;
    return 0;
}

int VSFSIBBreakaway::revertToStart() {
    committed_ = State{};
    committed_.tangent = kr_;
    trial_ = committed_;
    return 0;
}

UniaxialMaterial *VSFSIBBreakaway::getCopy() {
    auto *copy = new VSFSIBBreakaway(getTag(), kr_, qs_, qk_);
    copy->committed_ = committed_;
    copy->trial_ = trial_;
    return copy;
}

int VSFSIBBreakaway::sendSelf(int commitTag, Channel &channel) {
    Vector data(13);
    data(0) = getTag(); data(1) = kr_; data(2) = qs_; data(3) = qk_;
    data(4) = committed_.strain; data(5) = committed_.stress;
    data(6) = committed_.tangent; data(7) = committed_.slip;
    data(8) = committed_.rubberDef; data(9) = committed_.slideDistance;
    data(10) = committed_.cumulativeSlip;
    data(11) = committed_.mode; data(12) = committed_.direction;
    int dbTag = getDbTag();
    if (dbTag == 0) { dbTag = channel.getDbTag(); setDbTag(dbTag); }
    return channel.sendVector(dbTag, commitTag, data);
}

int VSFSIBBreakaway::recvSelf(int commitTag, Channel &channel, FEM_ObjectBroker &) {
    Vector data(13);
    const int result = channel.recvVector(getDbTag(), commitTag, data);
    if (result < 0) return result;
    setTag(static_cast<int>(data(0)));
    kr_ = data(1); qs_ = data(2); qk_ = data(3);
    committed_.strain = data(4); committed_.stress = data(5);
    committed_.tangent = data(6); committed_.slip = data(7);
    committed_.rubberDef = data(8); committed_.slideDistance = data(9);
    committed_.cumulativeSlip = data(10);
    committed_.mode = static_cast<int>(data(11));
    committed_.direction = static_cast<int>(data(12));
    trial_ = committed_;
    return (kr_ > 0.0 && qs_ > 0.0 && qk_ >= 0.0 && qk_ <= qs_) ? 0 : -1;
}

void VSFSIBBreakaway::Print(OPS_Stream &out, int) {
    out << "VSFSIBBreakaway tag=" << getTag() << " Kr=" << kr_
        << " Qs=" << qs_ << " Qk=" << qk_
        << " strain=" << trial_.strain << " force=" << trial_.stress
        << " slip=" << trial_.slip << " rubberDef=" << trial_.rubberDef
        << " state=" << trial_.mode << " direction=" << trial_.direction;
}

Response *VSFSIBBreakaway::setResponse(const char **argv, int argc, OPS_Stream &out) {
    if (argc < 1) return nullptr;
    int id = 0;
    if (std::strcmp(argv[0], "state") == 0) id = 101;
    else if (std::strcmp(argv[0], "slip") == 0) id = 102;
    else if (std::strcmp(argv[0], "rubberDef") == 0) id = 103;
    else if (std::strcmp(argv[0], "frictionForce") == 0) id = 104;
    else if (std::strcmp(argv[0], "slideDirection") == 0) id = 105;
    else if (std::strcmp(argv[0], "cumulativeSlip") == 0) id = 106;
    if (id == 0) return UniaxialMaterial::setResponse(argv, argc, out);
    out.tag("UniaxialMaterialOutput");
    out.attr("matTag", getTag());
    out.tag("ResponseType", argv[0]);
    out.endTag();
    return new MaterialResponse(this, id, 0.0);
}

int VSFSIBBreakaway::getResponse(int responseID, Information &info) {
    switch (responseID) {
    case 101: return info.setDouble(trial_.mode);
    case 102: return info.setDouble(trial_.slip);
    case 103: return info.setDouble(trial_.rubberDef);
    case 104: return info.setDouble(trial_.mode == SLIDE ? trial_.stress : 0.0);
    case 105: return info.setDouble(trial_.direction);
    case 106: return info.setDouble(trial_.cumulativeSlip);
    default: return UniaxialMaterial::getResponse(responseID, info);
    }
}

extern "C" void *OPS_VSFSIBBreakaway() {
    if (OPS_GetNumRemainingInputArgs() != 4) {
        opserr << "VSFSIBBreakaway: expected matTag Kr Qs Qk\n";
        return nullptr;
    }
    int tag = 0, one = 1, three = 3;
    double values[3] = {0.0, 0.0, 0.0};
    if (OPS_GetIntInput(&one, &tag) < 0 || OPS_GetDoubleInput(&three, values) < 0 ||
        values[0] <= 0.0 || values[1] <= 0.0 || values[2] < 0.0 ||
        values[2] > values[1] || !std::isfinite(values[0]) ||
        !std::isfinite(values[1]) || !std::isfinite(values[2])) {
        opserr << "VSFSIBBreakaway: invalid tag/Kr/Qs/Qk\n";
        return nullptr;
    }
    return new VSFSIBBreakaway(tag, values[0], values[1], values[2]);
}
