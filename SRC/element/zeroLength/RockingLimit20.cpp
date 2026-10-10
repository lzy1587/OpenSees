#include "RockingLimit20.h"

#include <Domain.h>
#include <Node.h>
#include <Information.h>
#include <ElementResponse.h>
#include <elementAPI.h>
#include <classTags.h>

#include <cmath>
#include <cstring>

void *OPS_RockingLimit20()
{
    if (OPS_GetNDM() != 3 || OPS_GetNDF() != 6 || OPS_GetNumRemainingInputArgs() != 23) {
        opserr << "RockingLimit20: use element RockingLimit20 tag n1 ... n20 Ktheta thetaLimit in 3D/6DOF\n";
        return 0;
    }
    int integers[21];
    int count = 21;
    if (OPS_GetIntInput(&count, integers) < 0) return 0;
    double values[2];
    count = 2;
    if (OPS_GetDoubleInput(&count, values) < 0 || values[0] <= 0.0 || values[1] <= 0.0) return 0;
    ID nodes(20);
    for (int i = 0; i < 20; ++i) nodes(i) = integers[i + 1];
    return new RockingLimit20(integers[0], nodes, values[0], values[1]);
}

RockingLimit20::RockingLimit20(int tag, const ID &nodes, double stiffness, double limit)
    : Element(tag, ELE_TAG_RockingLimit20), connected(nodes), tangent(120, 120),
      zero(120, 120), force(120), thetaMoment(4), kTheta(stiffness),
      thetaLimit(limit), ready(false)
{
    for (int i = 0; i < 20; ++i) {
        theNodes[i] = 0;
        reference[i] = 0.0;
        projection[0][i] = projection[1][i] = 0.0;
    }
    tangent.Zero(); zero.Zero(); force.Zero(); thetaMoment.Zero();
}

RockingLimit20::~RockingLimit20() {}

void RockingLimit20::setDomain(Domain *domain)
{
    if (domain == 0) {
        for (int i = 0; i < 20; ++i) theNodes[i] = 0;
        DomainComponent::setDomain(0);
        return;
    }
    for (int i = 0; i < 20; ++i) {
        theNodes[i] = domain->getNode(connected(i));
        if (theNodes[i] == 0 || theNodes[i]->getNumberDOF() != 6 || theNodes[i]->getCrds().Size() != 3) {
            opserr << "RockingLimit20: missing/incompatible node " << connected(i) << endln;
            ready = false;
            return;
        }
    }
    DomainComponent::setDomain(domain);
    if (ready) return;

    // Centering makes the common-Z mode exactly orthogonal to both rocking modes.
    double xc = 0.0, yc = 0.0;
    for (int i = 0; i < 20; ++i) {
        xc += theNodes[i]->getCrds()(0) / 20.0;
        yc += theNodes[i]->getCrds()(1) / 20.0;
    }
    double xx = 0.0, xy = 0.0, yy = 0.0;
    for (int i = 0; i < 20; ++i) {
        const double x = theNodes[i]->getCrds()(0) - xc;
        const double y = theNodes[i]->getCrds()(1) - yc;
        xx += x*x; xy += x*y; yy += y*y;
    }
    const double determinant = xx*yy - xy*xy;
    if (determinant <= 1.0e-12) {
        opserr << "RockingLimit20: support layout is rank deficient" << endln;
        return;
    }
    for (int i = 0; i < 20; ++i) {
        const double x = theNodes[i]->getCrds()(0) - xc;
        const double y = theNodes[i]->getCrds()(1) - yc;
        // thetaX = slope in y; thetaY = minus slope in x.
        projection[0][i] = (-xy*x + xx*y) / determinant;
        projection[1][i] = -(yy*x - xy*y) / determinant;
        reference[i] = theNodes[i]->getDisp()(2); // post-gravity committed state
    }
    ready = true;
}

void RockingLimit20::evaluate(void)
{
    tangent.Zero(); force.Zero(); thetaMoment.Zero();
    if (!ready) return;
    double theta[2] = {0.0, 0.0};
    for (int i = 0; i < 20; ++i) {
        const double dz = theNodes[i]->getTrialDisp()(2) - reference[i];
        theta[0] += projection[0][i]*dz;
        theta[1] += projection[1][i]*dz;
    }
    const double radius = std::hypot(theta[0], theta[1]);
    thetaMoment(0) = theta[0];
    thetaMoment(1) = theta[1];
    if (radius <= thetaLimit) return;

    const double unit[2] = {theta[0]/radius, theta[1]/radius};
    const double scale = kTheta*(radius - thetaLimit);
    thetaMoment(2) = scale*unit[0];
    thetaMoment(3) = scale*unit[1];
    const double transverse = kTheta*(1.0 - thetaLimit/radius);
    double local[2][2];
    for (int a = 0; a < 2; ++a)
        for (int b = 0; b < 2; ++b)
            local[a][b] = (a == b ? transverse : 0.0) +
                          (kTheta - transverse)*unit[a]*unit[b];
    for (int i = 0; i < 20; ++i) {
        const int row = 6*i + 2;
        force(row) = projection[0][i]*thetaMoment(2) +
                     projection[1][i]*thetaMoment(3);
        for (int j = 0; j < 20; ++j) {
            const int col = 6*j + 2;
            for (int a = 0; a < 2; ++a)
                for (int b = 0; b < 2; ++b)
                    tangent(row, col) += projection[a][i]*local[a][b]*projection[b][j];
        }
    }
}

const Matrix &RockingLimit20::getTangentStiff(void) { evaluate(); return tangent; }
const Matrix &RockingLimit20::getInitialStiff(void) { return zero; }
const Matrix &RockingLimit20::getMass(void) { return zero; }
const Matrix &RockingLimit20::getDamp(void) { return zero; }
const Vector &RockingLimit20::getResistingForce(void) { evaluate(); return force; }
const Vector &RockingLimit20::getResistingForceIncInertia(void) { return getResistingForce(); }
int RockingLimit20::sendSelf(int, Channel &) { return -1; }
int RockingLimit20::recvSelf(int, Channel &, FEM_ObjectBroker &) { return -1; }
void RockingLimit20::Print(OPS_Stream &stream, int) {
    stream << "RockingLimit20 " << getTag() << " Ktheta " << kTheta
           << " thetaLimit " << thetaLimit << endln;
}
Response *RockingLimit20::setResponse(const char **args, int argc, OPS_Stream &stream)
{
    if (argc > 0 && (!std::strcmp(args[0], "thetaMoment") || !std::strcmp(args[0], "rocking")))
        return new ElementResponse(this, 1, thetaMoment);
    if (argc > 0 && (!std::strcmp(args[0], "forces") || !std::strcmp(args[0], "force")))
        return new ElementResponse(this, 2, force);
    return 0;
}
int RockingLimit20::getResponse(int responseID, Information &info)
{
    evaluate();
    if (responseID == 1) return info.setVector(thetaMoment);
    if (responseID == 2) return info.setVector(force);
    return -1;
}
