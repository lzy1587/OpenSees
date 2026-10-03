#ifndef VSFSIB_BREAKAWAY_H
#define VSFSIB_BREAKAWAY_H

#include <UniaxialMaterial.h>

class Channel;
class FEM_ObjectBroker;
class Information;
class OPS_Stream;
class Response;

class VSFSIBBreakaway : public UniaxialMaterial {
public:
    VSFSIBBreakaway(int tag, double kr, double qs, double qk);
    VSFSIBBreakaway();

    int setTrialStrain(double strain, double strainRate = 0.0) override;
    double getStrain() override;
    double getStress() override;
    double getTangent() override;
    double getInitialTangent() override;
    int commitState() override;
    int revertToLastCommit() override;
    int revertToStart() override;
    UniaxialMaterial *getCopy() override;
    int sendSelf(int commitTag, Channel &theChannel) override;
    int recvSelf(int commitTag, Channel &theChannel, FEM_ObjectBroker &theBroker) override;
    void Print(OPS_Stream &s, int flag = 0) override;
    Response *setResponse(const char **argv, int argc, OPS_Stream &theOutput) override;
    int getResponse(int responseID, Information &info) override;
    const char *getClassType() const override { return "VSFSIBBreakaway"; }

    // Debug/test access. Trial state is never promoted until commitState().
    double getSlip() const { return trial_.slip; }
    double getRubberDef() const { return trial_.rubberDef; }
    double getCumulativeSlip() const { return trial_.cumulativeSlip; }
    int getState() const { return trial_.mode; }
    int getSlideDirection() const { return trial_.direction; }

private:
    enum Mode { STICK = 0, SLIDE = 1 };
    struct State {
        double strain = 0.0;
        double stress = 0.0;
        double tangent = 0.0;
        double slip = 0.0;
        double rubberDef = 0.0;
        double slideDistance = 0.0;
        double cumulativeSlip = 0.0;
        int mode = STICK;
        int direction = 0;
    };

    double slidingMagnitude(double slideDistance) const;
    double kr_;
    double qs_;
    double qk_;
    State committed_;
    State trial_;
};

#endif
